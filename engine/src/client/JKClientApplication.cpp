#include <client/JKClientApplication.h>

#include <JKApplication.h>
#include <JKSDLRenderBackend.h>
#include <JKRenderCommandList.h>
#include <JKOffscreenSurface.h>
#include <JKTimerThread.h>
#include <JKSoundManager.h>
#include <JKPlatform.h>
#include <JKTextAtlas.h>
#include <agent/JKAgentJson.h>
#include <theme/JKTheme.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <string>

#include <fs/JKFs.h>

namespace {

// [uistall] 워치독(roadmap 후보 3 실측)의 영구 착지점: stderr는 런처가
// 가리키는 곳(콘솔/프로브 파이프)으로만 가서 실사용 세션에서 휘발된다.
// 같은 줄을 <exeDir>\uistall.log에도 어펜드한다 — 스톨 빈도는 낮아서
// fopen/fclose per-report가 허용된다. 첫 open 시점에 4MiB 초과면 잘라낸다.
void UiStallReport(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    va_end(ap);
    std::fflush(stderr);
#ifdef _WIN32
    static std::string logPath;
    if (logPath.empty()) {
        // jk::fs::GetExecutablePath 흡수 (docs/68 W5) — 원문 규약(후행 '\' 유지,
        // 실패/구분자 없음 시 조용히 중단), '\'만 자른다. exe 옆 착지: 클라/
        // 서버가 같은 exe다 — 상태 파일 관례(bookmarks.json의 ExeDirSlash
        // 패턴)와 동일.
        std::string dir = jk::fs::GetExecutablePath();
        if (dir.empty()) return;
        const size_t slash = dir.find_last_of('\\');
        if (slash == std::string::npos) return;
        dir.resize(slash + 1);
        // exe 옆: 클라/서버가 같은 exe다 — 상태 파일 관례(browser
        // bookmarks.json의 ExeDirSlash 패턴)와 동일한 착지.
        logPath = dir + "uistall.log";
        FILE* f = std::fopen(logPath.c_str(), "rb");
        long size = 0;
        if (f) {
            std::fseek(f, 0, SEEK_END);
            size = std::ftell(f);
            std::fclose(f);
        }
        if (size > 4 * 1024 * 1024)
            std::fopen(logPath.c_str(), "wb"); // truncate — 본문은 아래 "a"가 다시 연다
    }
    FILE* f = std::fopen(logPath.c_str(), "a");
    if (!f) return;
    va_start(ap, fmt);
    std::vfprintf(f, fmt, ap);
    va_end(ap);
    std::fclose(f);
#else
    (void)fmt;
#endif
}

} // namespace

namespace jk {

JKClientApplication::JKClientApplication() : dc_(nullptr) {
    windowManager_ = std::make_unique<JKWindowManager>();
    messageBus_ = std::make_unique<JKMessageBus>();
    // Controls and shared dialogs (JKMessageBox/JKMenu/JKDialog/...) reach
    // host services through g_jkAppHost — without a host registered here they
    // silently no-op in client processes (the game-over message box was never
    // registered as modal and never painted).
    g_jkAppHost = this;
}

JKClientApplication::~JKClientApplication() {
    Close();
    if (g_jkAppHost == this) g_jkAppHost = nullptr;
}

bool JKClientApplication::CreateHiddenRenderer(const std::string& title, int width, int height) {
#ifdef _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
#endif

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::fprintf(stderr, "JKClientApplication::Init: SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    hiddenWindow_ = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        width,
        height,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!hiddenWindow_) {
        std::fprintf(stderr, "JKClientApplication::Init: SDL_CreateWindow failed: %s\n",
                     SDL_GetError());
        SDL_Quit();
        return false;
    }

    hiddenRenderer_ = SDL_CreateRenderer(hiddenWindow_, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    if (!hiddenRenderer_) {
        std::fprintf(stderr, "JKClientApplication::Init: SDL_CreateRenderer failed: %s\n",
                     SDL_GetError());
        DestroyHiddenRenderer();
        SDL_Quit();
        return false;
    }

    renderBackend_ = std::make_unique<JKSDLRenderBackend>(hiddenRenderer_);
    return true;
}

void JKClientApplication::DestroyHiddenRenderer() {
    renderBackend_.reset();
    if (hiddenRenderer_) {
        SDL_DestroyRenderer(hiddenRenderer_);
        hiddenRenderer_ = nullptr;
    }
    if (hiddenWindow_) {
        SDL_DestroyWindow(hiddenWindow_);
        hiddenWindow_ = nullptr;
    }
}

bool JKClientApplication::Init(const std::string& title, int width, int height,
                               const std::string& pipeName) {
    // theme.json 프리셋 로딩 (P2 단계 2) — 파일 없으면 다크 기본값 유지
    jk::theme::loadPresetFromFile(jk::theme::DefaultThemePath());

    if (!CreateHiddenRenderer(title, width, height)) {
        return false;
    }

    dc_ = JKDC(renderBackend_.get());
    resourceCache_ = std::make_unique<JKResourceCache>(renderBackend_.get());

    {
        auto& soundManager = JKSoundManager::GetInstance();
        soundManager.SetCommandMode(true);
        soundManager.SetCommandPoster(
            [this](const AudioCommand& cmd) {
                if (surface_) {
                    surface_->PostAudioCommand(cmd);
                }
            });
        soundManager.Init();
    }

    surface_ = std::make_unique<jk::client::JKClientSurface>(pipeName, width, height, title);
    if (!surface_->Connect()) {
        std::fprintf(stderr, "JKClientApplication::Init: failed to connect to server\n");
        Close();
        return false;
    }

    // 코어 레벨 에이전트 이벤트 구독(스펙 2026-09-18-settings-hub §2.3):
    // audio.master 등 서버 상태 변경을 모든 클라가 받는다. 자체 구독 앱과
    // 멱등 공존(서버 구독은 플래그 세트).
    surface_->SendAgentEventSubscribe(true);

    logicalWidth_ = width;
    logicalHeight_ = height;
    scaleX_ = 1.0f;
    scaleY_ = 1.0f;

    SDL_StartTextInput();

    hangulManager_ = std::make_unique<HangulManager>();
    if (hangulManager_->CreationError) {
        std::fprintf(stderr,
            "Warning: HangulManager font files missing; using built-in ASCII font.\n");
    }
    dc_.SetHangulManager(hangulManager_.get());
    HanMan = hangulManager_.get();
    resourceCache_->RegisterFont("default", hangulManager_.get());

    // 데스크탑 벡터 폰트 (docs/63 §2): 관문 교체 — 실패 시 비트맵 경로 그대로.
    // ResolveDesktopFontPath가 state\settings.json의 text.font_path 오버라이드를
    // 직접 읽으므로 서버 KV(textFontPath_)를 여기로 파이프할 필요가 없다.
    textAtlas_ = std::make_unique<JKTextAtlas>();
    // 셀 메트릭 진실원 (docs/63 §6 text.font_scale) — 기본 1.0 = {8,16,16}.
    const jk::text::CellMetrics m = jk::text::GetCellMetrics();
    const std::string fontPath = jk::text::ResolveDesktopFontPath();
    if (!fontPath.empty() && textAtlas_->Init(fontPath, m.engW, m.cellH, m.hanW)) {
        dc_.SetTextAtlas(textAtlas_.get(), resourceCache_.get());
        // 보조 폰트 체인 (docs/63 §6 2단계): 미설정(빈)이면 체인 없음 — 1차만.
        // Init 실패는 경고 1줄, 체인 없이 계속(치명 아님).
        const std::string fbPath = jk::text::ResolveDesktopFallbackPath();
        if (!fbPath.empty() && !textAtlas_->InitFallback(fbPath)) {
            std::fprintf(stderr,
                "Warning: fallback font init failed (%s); glyph chain "
                "disabled.\n",
                fbPath.c_str());
        }
    } else if (fontPath.empty()) {
        std::fprintf(stderr,
            "Warning: no vector font configured (text.font_path empty); "
            "staying on bitmap glyphs.\n");
    } else {
        std::fprintf(stderr,
            "Warning: vector font init failed (%s); staying on bitmap glyphs.\n",
            fontPath.c_str());
    }

    timerThread_ = std::make_unique<JKTimerThread>();
    timerThread_->Start(messageBus_.get());

    OnInit();

    if (!mainWindow_) {
        mainWindow_ = std::make_unique<JKWindow>(title);
    }
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        mainWindow_->SetWindowRect(JKRect{ 0, 0, logicalWidth_, logicalHeight_ });
    }
    windowManager_->SetMainWindow(mainWindow_.get());
    windowManager_->SetInputWindow(mainWindow_.get());

    mainWindow_->Init();
    mainWindow_->Setup();
    mainWindow_->Open();
    mainWindow_->FocusFirstChild();

    pixelBuffer_.resize(static_cast<size_t>(width) * height * 4, 0);
    running_ = true;
    return true;
}

void JKClientApplication::Close() {
    running_ = false;

    if (timerThread_) {
        timerThread_->Stop();
        timerThread_.reset();
    }

    // Stop the surface read thread explicitly before destroying the surface so
    // the background thread does not access shared memory after it is closed.
    if (surface_) {
        surface_->Close();
    }
    surface_.reset();

    OnClose();
    if (mainWindow_) {
        mainWindow_->Close();
    }

    hangulManager_.reset();
    HanMan = nullptr;
    textAtlas_.reset();
    resourceCache_.reset();

    DestroyHiddenRenderer();
    SDL_Quit();
}

int JKClientApplication::Run() {
    if (!running_ || !surface_ || !surface_->IsValid() || !mainWindow_) {
        return 1;
    }
    // 프레임 페이싱 진실원 (docs/78 폰 실측 CPU 소등 — 서버 Run()과 동일
    // 처방): always-dirty 앱(IsFrameDirty 기본 true — ImGui 계열과 태스크바
    //·스크립트 앱)이 SW 렌더러(폰 X11)에서 vsync 블록 없이 돌면 SDL_Delay(1)
    // 만으로 ~1000fps, idle 프로세스가 코어를 통째로 점유했다. vsync 플랫폼은
    // present가 이미 스트라이드를 잠그므로 아래 절전은 no-op (관측 무변경).
    Uint32 frameStart = SDL_GetTicks();
    // -1000 기점(activity 게이트): 첫 이터레이션에서 폴백 조건이 곧 참 —
    // 부팅 첫 프레임을 이벤트 없이도 즉시 그리게(Uint32 랩 산술로 안전).
    Uint32 lastRenderMs = frameStart - 1000;
    // 폴백 스킵(아래): 첫 렌더는 더티와 무관하게 강제 — 부팅 즉시 1프레임.
    bool renderedOnce = false;
    int traceTimer = 0, traceInput = 0, traceAgent = 0, traceTool = 0,
        traceTheme = 0, traceFrames = 0;  // [tmp] docs/78 CPU 소등 계측
    const bool trace = std::getenv("JK_CPU_TRACE") != nullptr;
    Uint32 traceSince = frameStart;

    while (running_) {
        // 활동 게이트 (docs/78 폰 실측 CPU 소등 2차 — always-dirty 앱): 이벤트가
        // 하나도 없는 idle에서도 IsFrameDirty 기본(true)이 게이트 OR 첫 항으로
        // 무력화돼 태스크바가 63fps를 계속 그렸다 — SW 렌더러(폰 X11)에서
        // readback 동기 비용(~14ms) × 60fps ≈ 풀코어 + 서버 합성·llvmpipe도
        // 따라 풀점유. 계약 변경(폰 실측 결정): IsFrameDirty 기본은 false —
        // 게이트 = 이번 이터레이션에 메시지 버스 활동(타이머/입력/에이전트/
        // 툴콜)이나 테마 변경이 있었거나 IsFrameDirty(오버라이드 앱 —
        // vplayer·터미널·ImGui 계열)이거나 마지막 렌더에서 1s 폴백이 지났을
        // 때만 렌더. 1s 폴백은 이벤트로 승계되지 않는 느린 변화(비동기
        // Invalidate 등)의 최악 지연 상한을 묶는 안전망 — 폴백 1fps의 idle
        // 비용은 무시 수준. 스크립트 앱의 애니메이션은 setInterval 타이머로
        // 활동을 만든다(api 캐탈로그 계약 — 문서화 동기화 완료).
        bool activity = false;
        const auto t0 = std::chrono::steady_clock::now();
        { const int n = DrainTimerChannel(); if (n > 0) { activity = true; traceTimer += n; } }
        const auto t1 = std::chrono::steady_clock::now();
        if (!running_) break;

        { const int n = DrainInputChannel(); if (n > 0) { activity = true; traceInput += n; } }

        // 코어 에이전트 이벤트 펌프(스펙 2026-09-18-settings-hub §2.3): 유일
        // 소비자. audio.master는 코어가 직접 JKSoundManager 마스터 게인에
        // 적용(모든 ImGui 앱 일괄 수용 — 앱별 코드 0), 나머지는 앱 훅으로.
        {
            std::vector<std::string> events;
            if (surface_->DrainAgentEvents(events) > 0) {
                activity = true;
                traceAgent += static_cast<int>(events.size());
                for (const std::string& js : events) {
                    if (js.find("\"topic\":\"audio.master\"") != std::string::npos) {
                        jk::agent::AgentJson body(js);
                        int mute = 0, vol = 80;
                        body.GetObjInt("data", "mute", mute);
                        body.GetObjInt("data", "volume", vol);
                        JKSoundManager::GetInstance().SetMasterVolume(
                            mute ? 0.f : std::min(1.f, vol / 100.f));
                    }
                    OnAgentEvent(js);
                }
            }
        }

        // 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §8.2): 프레임 펌프 결합
        // 폴링 — DrainAgentEvents와 동일 스위프/관용구. 결과 전송까지 코어가
        // 처리(앱은 OnAgentToolCall 훅만 오버라이드; reqId 비노출).
        {
            jk::client::JKClientSurface::AgentToolCallMsg tc;
            while (surface_ && surface_->PollToolCall(tc)) {
                activity = true;
                ++traceTool;
                std::string resultJson;
                const bool ok = OnAgentToolCall(tc.tool, tc.args, resultJson);
                surface_->SendAgentToolResult(tc.reqId, ok, resultJson);
            }
        }

        const auto t2 = std::chrono::steady_clock::now();
        if (!running_) break;

        if (mainWindow_) {
            mainWindow_->RemoveClosedChildren();
        }

        // P3 theme hot-swap (docs/52): poll theme.json every 500ms. The
        // theme_set tool swaps the server in-process and clients catch up
        // here — mtime polling covers every client process, including
        // non-agent native apps a wire event would miss. PollPresetFile
        // re-runs the loader on change and ApplyTheme re-captures the
        // ctor-captured tokens down the widget tree.
        {
            static auto s_themeLast = std::chrono::steady_clock::now();
            const auto now = std::chrono::steady_clock::now();
            if (now - s_themeLast >= std::chrono::milliseconds(500)) {
                s_themeLast = now;
                if (jk::theme::PollPresetFile()) {
                    activity = true;
                    ++traceTheme;
                    if (mainWindow_) mainWindow_->ApplyTheme();
                    OnThemeChanged();
                }
            }
        }

        OnIdle();
        const auto t3 = std::chrono::steady_clock::now();
        const Uint32 nowMs = SDL_GetTicks();
        const bool fallback = nowMs - lastRenderMs >= 1000;
        // 폴백 스킵 (docs/78 CPU 소등 잔여): 폴백만이 유발한 렌더는 장면에
        // 더티가 남아 있을 때만 이어간다. 더티가 비면 그리기+리드백+풀 커밋은
        // 전부 무의미한 원천 — 커밋이 서버 full 합성(폰 1회 ~200ms)을 유발해
        // idle 1fps 커밋 2클라만으로 서버 40%대가 성립했다. 첫 렌더만 더티와
        // 무관하게 강제한다(부팅 시점 더티 상태를 보증하지 않는다). 스킵은
        // 더티를 소각하지 않는다 — 유입된 더티는 다음 폴백에 그려진다.
        bool wantRender = IsFrameDirty() || activity || !renderedOnce;
        if (!wantRender && fallback) {
            JKWindow* modal = windowManager_ ? windowManager_->GetModalWindow()
                                             : nullptr;
            const bool dirty = (mainWindow_ && mainWindow_->HasDirtyWindows())
                               || (modal && modal->HasDirtyWindows());
            if (dirty) {
                wantRender = true;
            } else {
                lastRenderMs = nowMs;  // 스킵도 폴백 기점 리셋 — 다음 초 재검
            }
        }
        if (wantRender) {
            RenderAndCommit();
            OnFrameCommitted();
            lastRenderMs = nowMs;
            ++traceFrames;
            renderedOnce = true;
        }
        const auto t4 = std::chrono::steady_clock::now();

        // Permanent low-cost watchdog (diag-probe-ui-stall §7-1): any
        // UI-thread stall >= kUiStallGapMs is attributed to a phase.
        // Log-only — on stall, ph= values route the triage (render ->
        // GPU/DWM path, idle/timer -> app event path). Cost when quiet: a
        // few clock reads per iteration.
        constexpr double kUiStallGapMs = 500.0;
        const double totalMs =
            std::chrono::duration<double, std::milli>(t4 - t0).count();
        if (totalMs >= kUiStallGapMs) {
            const double timerMs =
                std::chrono::duration<double, std::milli>(t1 - t0).count();
            const double inputMs =
                std::chrono::duration<double, std::milli>(t2 - t1).count();
            const double idleMs =
                std::chrono::duration<double, std::milli>(t3 - t2).count();
            const double renderMs =
                std::chrono::duration<double, std::milli>(t4 - t3).count();
            UiStallReport(
                "[uistall] total=%.0fms timer=%.0f input=%.0f "
                "idle=%.0f render=%.0f gap=%.0f\n",
                totalMs, timerMs, inputMs, idleMs, renderMs,
                totalMs - timerMs - inputMs - idleMs - renderMs);
        }

        // 프레임 페이싱 (서버 Run()과 동일 처방 — 60fps 하한 스트라이드,
        // 늦은 프레임은 흘려보낸다): always-dirty 앱 렌더 빈도 상한.
        const Uint32 frameWorked = SDL_GetTicks() - frameStart;
        SDL_Delay(frameWorked < 16 ? 16 - frameWorked : 1);
        frameStart = SDL_GetTicks();
        if (trace) {
            const Uint32 n = SDL_GetTicks();
            if (n - traceSince >= 1000) {
                traceSince = n;
                std::fprintf(stderr,
                             "[cpustat] timer=%d input=%d agent=%d tool=%d "
                             "theme=%d frames=%d\n",
                             traceTimer, traceInput, traceAgent, traceTool,
                             traceTheme, traceFrames);
                std::fflush(stderr);
                traceTimer = traceInput = traceAgent = traceTool =
                    traceTheme = traceFrames = 0;
            }
        }
    }

    return 0;
}

int JKClientApplication::DrainTimerChannel() {
    int consumed = 0;  // 활동 게이트 (docs/78 CPU 소등) — 소비 수 반환
    JKMessageBus::Payload timerPayload;
    while (messageBus_->Pop(JKMessageBus::Channel::Timer, timerPayload)) {
        ++consumed;
        if (!ProcessOneEvent(timerPayload.event)) {
            running_ = false;
            break;
        }
    }
    return consumed;
}

int JKClientApplication::DrainInputChannel() {
    // Input events arrive from the server via JKClientSurface, not the message bus.
    int consumed = 0;  // 활동 게이트 (docs/78 CPU 소등) — 소비 수 반환
    JKEvent ev;
    while (surface_ && surface_->PollInputEvent(ev)) {
        ++consumed;
        if (ev.type == JKEventType::Quit) {
            running_ = false;
            break;
        }
        if (!ProcessOneEvent(ev)) {
            running_ = false;
            break;
        }
    }
    return consumed;
}

bool JKClientApplication::ProcessOneEvent(const JKEvent& ev) {
    if (ev.type == JKEventType::Quit) {
        return false;
    }

    if (ev.type == JKEventType::SizeChanged) {
        if (ev.x <= 0 || ev.y <= 0) {
            return true;
        }
        // A server-initiated resize (MsgType::ResizeSurface) arrives as a
        // SizeChanged event: remap shared memory first so subsequent commits
        // target the new mapping, then let the relayout below run.
        if (surface_) {
            surface_->ApplyPendingResize();
        }
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            logicalWidth_ = ev.x;
            logicalHeight_ = ev.y;
        }
        if (mainWindow_) {
            mainWindow_->SetWindowRect(JKRect{ 0, 0, ev.x, ev.y });
            mainWindow_->Invalidate();
        }
    }

    if (ev.type == JKEventType::DpiChanged) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (logicalWidth_ > 0 && logicalHeight_ > 0) {
            scaleX_ = ev.x / static_cast<float>(logicalWidth_);
            scaleY_ = ev.y / static_cast<float>(logicalHeight_);
        }
        letterboxX_ = 0;
        letterboxY_ = 0;
    }

    if (!PreProcessMessage(ev)) {
        return false;
    }

    JKEvent routedEv = ev;
    ApplyInputRouting(routedEv);

    if (routedEv.type == JKEventType::KeyDown && routedEv.keyCode == SDLK_TAB &&
        WantsTabFocusCycle()) {
        JKWindow* active = windowManager_->GetKeyboardTargetWindow();
        bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        if (active) {
            if (shift) active->FocusPrevChild();
            else       active->FocusNextChild();
        }
        return true;
    }

    RouteMessage(routedEv);
    return true;
}

void JKClientApplication::ApplyInputRouting(JKEvent& ev) {
    if (!mainWindow_ || !windowManager_) {
        return;
    }

    if (ev.type == JKEventType::MouseMove ||
        ev.type == JKEventType::MouseDown ||
        ev.type == JKEventType::MouseUp) {
        JKControl* capture = windowManager_->GetCapture();
        if (capture &&
            (ev.type == JKEventType::MouseMove || ev.type == JKEventType::MouseUp)) {
            ev.targetId = capture->GetWinId();
            ev.winId = ev.targetId;
            ev.controlId = capture->GetControlId();
            return;
        }

        if (ev.type == JKEventType::MouseDown && capture) {
            windowManager_->ReleaseCapture();
        }

        JKWindow* active = windowManager_->GetMouseTargetWindow();
        if (active) {
            JKControl* target = active->HitTest(ev.x, ev.y);
            ev.targetId = target ? target->GetWinId() : active->GetWinId();
            ev.winId = ev.targetId;
            ev.controlId = target ? target->GetControlId() : 0;
        }
    } else if (ev.type == JKEventType::KeyDown ||
               ev.type == JKEventType::KeyUp ||
               ev.type == JKEventType::Char ||
               ev.type == JKEventType::TextEditing) {
        JKWindow* active = windowManager_->GetKeyboardTargetWindow();
        if (active) {
            ev.targetId = active->GetWinId();
            ev.winId = ev.targetId;
        }
    }
}

void JKClientApplication::SetMainWindow(std::unique_ptr<JKWindow> window) {
    mainWindow_ = std::move(window);
}

JKWindow* JKClientApplication::GetMainWindow() const {
    return mainWindow_.get();
}

void JKClientApplication::SetModalWindow(JKWindow* window) {
    windowManager_->SetModalWindow(window);
}

JKWindow* JKClientApplication::GetModalWindow() const {
    return windowManager_ ? windowManager_->GetModalWindow() : nullptr;
}

void JKClientApplication::SetCapture(JKControl* control) {
    windowManager_->SetCapture(control);
}

void JKClientApplication::ReleaseCapture() {
    windowManager_->ReleaseCapture();
}

JKControl* JKClientApplication::GetCapture() const {
    return windowManager_ ? windowManager_->GetCapture() : nullptr;
}

void JKClientApplication::SetInputWindow(JKWindow* window) {
    windowManager_->SetInputWindow(window);
}

JKWindow* JKClientApplication::GetInputWindow() const {
    return windowManager_ ? windowManager_->GetInputWindow() : nullptr;
}

JKControl* JKClientApplication::FindControlById(uint32_t winId) {
    return windowManager_->FindControlById(winId);
}

JKControl* JKClientApplication::FindControlByControlId(uint16_t controlId) {
    return windowManager_->FindControlByControlId(controlId);
}

JKWindow* JKClientApplication::FindWindowById(uint32_t winId) {
    return windowManager_->FindWindowById(winId);
}

void JKClientApplication::SetTimerInterval(uint32_t ms) {
    legacyTimerInterval_ = ms;
    if (timerThread_) {
        if (ms == 0) {
            timerThread_->ClearLegacyTimer();
        } else {
            timerThread_->SetLegacyTimer(
                mainWindow_ ? mainWindow_->GetWinId() : 0, ms);
        }
    }
}

uint64_t JKClientApplication::AddTimer(uint32_t winId, uint32_t intervalMs, bool repeat) {
    if (timerThread_) {
        return timerThread_->AddTimer(winId, intervalMs, repeat);
    }
    return 0;
}

void JKClientApplication::RemoveTimer(uint64_t handle) {
    if (timerThread_) timerThread_->RemoveTimer(handle);
}

void JKClientApplication::RemoveTimersForWindow(uint32_t winId) {
    if (timerThread_) timerThread_->RemoveTimersForWindow(winId);
}

void JKClientApplication::PostAudioCommand(const AudioCommand& cmd) {
    // As a window-server client, audio commands are forwarded to the server
    // over IPC instead of being played locally.
    if (surface_) {
        surface_->PostAudioCommand(cmd);
    }
}

void JKClientApplication::SetLogicalSize(int w, int h) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    logicalWidth_ = w;
    logicalHeight_ = h;
}

void JKClientApplication::GetLogicalSize(int& w, int& h) const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    w = logicalWidth_;
    h = logicalHeight_;
}

void JKClientApplication::GetScale(float& sx, float& sy) const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    sx = scaleX_;
    sy = scaleY_;
}

void JKClientApplication::GetLetterbox(int& x, int& y) const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    x = letterboxX_;
    y = letterboxY_;
}

bool JKClientApplication::PreProcessMessage(const JKEvent& ev) {
    (void)ev;
    return true;
}

// 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §8.2) — 기본 구현: 등록하지 않은
// 앱은 도구가 오지 않지만(서버가 레지스트리로 중계 대상을 고름), 방어적으로
// unsupported 에러로 응답한다(코어가 SendAgentToolResult로 에코).
bool JKClientApplication::OnAgentToolCall(const std::string& tool,
                                          const std::string& argsJson,
                                          std::string& resultJson) {
    (void)argsJson;
    resultJson = "{\"error\":\"unsupported_tool\",\"tool\":\"" + tool + "\"}";
    return false;
}

void JKClientApplication::RouteMessage(const JKEvent& ev) {
    JKWindow* targetWindow = mainWindow_.get();
    JKControl* targetControl = nullptr;

    JKWindow* modal = windowManager_->GetModalWindow();
    if (modal) {
        targetWindow = modal;
    } else if (ev.winId != 0) {
        targetWindow = windowManager_->FindWindowById(ev.winId);
        if (!targetWindow) targetWindow = mainWindow_.get();
    } else if (ev.targetId != 0) {
        targetControl = windowManager_->FindControlById(ev.targetId);
        if (!targetControl) {
            targetWindow = windowManager_->FindWindowById(ev.targetId);
            if (!targetWindow) targetWindow = mainWindow_.get();
        }
    }

    if (targetControl) {
        targetControl->RespondMessage(ev);
        return;
    }

    if (ev.controlId != 0 && targetWindow) {
        targetControl = targetWindow->FindControlByControlId(ev.controlId);
        if (targetControl) {
            targetControl->RespondMessage(ev);
            return;
        }
    }

    if (targetWindow) {
        targetWindow->RespondMessage(ev);
    }
}

void JKClientApplication::ComposeScene() {
    if (!mainWindow_) {
        return;
    }

    auto cmdList = std::make_unique<JKRenderCommandList>();
    JKDC dc(cmdList.get());
    dc.SetHangulManager(hangulManager_.get());

    const auto& t = jk::theme::current();
    if (WantsTransparentSurface()) {
        // SDL_RenderClear ignores the blend mode: this is a raw write, so
        // (0,0,0,0) really leaves the surface transparent for the compositor.
        dc.SetColor(0, 0, 0, 0);
    } else {
        dc.SetColor(t.appClearBg.r, t.appClearBg.g, t.appClearBg.b, 255);
    }
    dc.Clear();

    mainWindow_->PaintWindow(dc);
    mainWindow_->PaintClient(dc);

    JKWindow* modal = GetModalWindow();
    if (modal) {
        modal->PaintWindow(dc);
        modal->PaintClient(dc);
    }

    mainWindow_->ClearDirtyRects();
    if (modal) {
        modal->ClearDirtyRects();
    }

    pendingScene_ = cmdList->Serialize();
}

void JKClientApplication::RenderAndCommit() {
    if (!surface_ || !surface_->IsValid() || !renderBackend_ || !mainWindow_) {
        return;
    }

    const int w = surface_->Width();
    const int h = surface_->Height();
    if (w <= 0 || h <= 0) {
        return;
    }

    // Upload queued resource-cache images (flag/mine/question icons etc.)
    // before painting: the single-process path flushes these in
    // JKRenderThread, but the client has no render thread. ComposeScene bakes
    // texture handles into the serialized scene, so any image loaded during
    // paint must already be a real texture or the blit is a silent no-op.
    resourceCache_->FlushUploads(renderBackend_.get());

    // Build the scene description once.
    ComposeScene();

    // Create/resize off-screen target texture if needed.
    if (targetTexture_ == JKRenderBackend::InvalidTexture ||
        targetW_ != w || targetH_ != h) {
        if (targetTexture_ != JKRenderBackend::InvalidTexture) {
            renderBackend_->DestroyTexture(targetTexture_);
        }
        targetTexture_ = renderBackend_->CreateTargetTexture(w, h);
        targetW_ = w;
        targetH_ = h;
        if (targetTexture_ == JKRenderBackend::InvalidTexture) {
            std::fprintf(stderr, "JKClientApplication: failed to create target texture\n");
            return;
        }
    }

    renderBackend_->SetRenderTarget(targetTexture_);
    renderBackend_->SetScale(1.0f, 1.0f);

    // Commit-stage watchdog marks (diag-probe-ui-stall §7-2): boundaries
    // between compose/replay, overlay, readback and the shm commit. Logged
    // only when the stage total crosses kCommitStallMs.
    constexpr double kCommitStallMs = 100.0;
    const auto c0 = std::chrono::steady_clock::now();

    // Replay the serialized scene into the target texture.
    if (!pendingScene_.empty()) {
        auto scene = JKRenderCommandList::Deserialize(pendingScene_);
        if (scene) {
            scene->Replay(renderBackend_.get());
        }
    }
    const auto cComp = std::chrono::steady_clock::now();

    // Overlay hook (docs/23 §5.2-4): immediate-mode layers (ImGui) draw straight
    // onto the target texture after the scene replay, before the readback turns
    // the result into a shm commit. The target is bound at 1:1 scale here.
    RenderOverlay(hiddenRenderer_, w, h);
    const auto c1 = std::chrono::steady_clock::now();

    // Read pixels from the current render target (the off-screen texture).
    const size_t needed = static_cast<size_t>(w) * h * 4;
    if (pixelBuffer_.size() != needed) {
        pixelBuffer_.resize(needed, 0);
    }

    int pitch = 0;
    if (SDL_RenderReadPixels(hiddenRenderer_, nullptr, SDL_PIXELFORMAT_RGBA32,
                             pixelBuffer_.data(), w * 4) != 0) {
        std::fprintf(stderr, "JKClientApplication: SDL_RenderReadPixels failed: %s\n",
                     SDL_GetError());
        renderBackend_->SetRenderTarget(nullptr);
        return;
    }
    (void)pitch;
    const auto c2 = std::chrono::steady_clock::now();

    renderBackend_->SetRenderTarget(nullptr);

    // Copy to shared memory and commit.
    uint8_t* dest = surface_->Pixels();
    if (dest) {
        std::memcpy(dest, pixelBuffer_.data(), needed);
        surface_->CommitFull();
    }
    const auto c3 = std::chrono::steady_clock::now();

    // On stall, attribute: readpix+commit -> GPU/DWM path, comp/overlay ->
    // app paint path (same triage rule as the Run-loop watchdog).
    const double stageMs =
        std::chrono::duration<double, std::milli>(c3 - c0).count();
    if (stageMs >= kCommitStallMs) {
        const double compMs =
            std::chrono::duration<double, std::milli>(cComp - c0).count();
        const double overlayMs =
            std::chrono::duration<double, std::milli>(c1 - cComp).count();
        const double readpixMs =
            std::chrono::duration<double, std::milli>(c2 - c1).count();
        const double commitMs =
            std::chrono::duration<double, std::milli>(c3 - c2).count();
        UiStallReport(
            "[uistall] commit comp=%.0f overlay=%.0f readpix=%.0f "
            "commit=%.0f\n",
            compMs, overlayMs, readpixMs, commitMs);
    }
}

} // namespace jk
