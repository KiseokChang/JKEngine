#include <JKApplication.h>
#include <JKEvent.h>
#include <JKSDLRenderBackend.h>
#include <JKPlatform.h>
#include <JKSoundManager.h>
#include <cstdio>

namespace jk {

JKApplication* g_currentJKApp = nullptr;

JKApplication::JKApplication() : dc_(nullptr) {
    g_currentJKApp = this;
    windowManager_ = std::make_unique<JKWindowManager>();
}

JKApplication::~JKApplication() {
    Close();
    if (g_currentJKApp == this) g_currentJKApp = nullptr;
}

bool JKApplication::Init(const std::string& title, int width, int height) {
#ifdef _WIN32
    // SDL2 on Windows: declare per-monitor V2 DPI awareness and use logical point
    // coordinates. This disables OS bitmap scaling, makes SDL_GetWindowSize() report
    // logical points, and makes SDL_GetRendererOutputSize() report physical pixels.
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");

    // 디버깅/검증용 마우스 이벤트 로그.
    mouseLog_ = std::fopen("C:\\temp_jkwin_verify\\mouse.log", "w");

    // Process-level DPI awareness is handled by JKPlatform::InitializeProcessDpiAwareness()
    // at process start (see main.cpp). SDL hints here reinforce the behavior.
#endif

    // 비디오와 오디오 초기화를 분리한다. 오디오가 실패해도 GUI는 계속 실행.
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "JKENGINE Error",
            (std::string("SDL video init failed: ") + SDL_GetError()).c_str(), nullptr);
        return false;
    }

    bool audioOk = true;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        audioOk = false;
    } else if (!JKSoundManager::GetInstance().Init()) {
        audioOk = false;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    // 오디오 실패는 조용히 넘어간다. GUI는 계속 실행.

    // 진단 출력: SDL 디스플레이 인덱스와 물리 배치/DPI 대응을 기록한다.
    // 모니터 간 이동 시 DISPLAY_CHANGED data1로 어떤 SDL 인덱스가 오고
    // SDL_GetDisplayDPI가 어떤 배율을 보고하는지 추적하기 위함이다.
    {
        const int numDisplays = SDL_GetNumVideoDisplays();
        for (int di = 0; di < numDisplays; ++di) {
            float ddpi = 0.0f, hdpi = 0.0f, vdpi = 0.0f;
            SDL_GetDisplayDPI(di, &ddpi, &hdpi, &vdpi);
            SDL_Rect bounds{};
            SDL_GetDisplayBounds(di, &bounds);
            std::fprintf(stderr,
                "[DPISYNC] display %d bounds=(%d,%d %dx%d) ddpi=%.1f hdpi=%.1f vdpi=%.1f\n",
                di, bounds.x, bounds.y, bounds.w, bounds.h, ddpi, hdpi, vdpi);
        }
    }

    // 앱 논리 좌표계는 Init에 요청한 크기로 시작한다. 실제 창이 생성된 뒤에는
    // SDL_GetWindowSize()가 보고하는 논리 포인트 크기로 덮어쓴다. 그래야
    // HiDPI 환경에서도 내용이 창에 꽉 차고, 레터박스(회색 여백) 없이 1:1로
    // 렌더링된다. 요청 크기는 여전히 창 생성 시의 힌트로 사용된다.
    logicalWidth_ = width;
    logicalHeight_ = height;

    // 임시로 요청한 크기의 창을 띄워 본 뒤, SDL이 보고하는 실제 논리 포인트
    // 크기를 앱 좌표계로 삼는다. 이 값은 CreateWindow 직후 얻을 수 있다.
    int actualW = width;
    int actualH = height;

    // 창(타이틀 바/테두리 포함)이 화면 작업 영역(작업표시줄 제외)보다 크면
    // 생성 크기를 줄여서 전체 창이 화면 안에 들어오게 한다.
    // 주의: SDL2의 SDL_SetWindowSize()는 Windows DPI 스케일링 힌트가 켜져 있을 때
    // 포인트/물리 픽셀 해석이 일관되지 않으므로, 생성 시점 크기로만 맞춘다.
    int createW = width;
    int createH = height;
    SDL_Rect usable{};
    const bool hasUsable =
        SDL_GetDisplayUsableBounds(0, &usable) == 0 && usable.w > 0 && usable.h > 0;
    if (hasUsable) {
        // 생성 전에는 창 장식 크기를 알 수 없으므로 일반적인 값으로 추정한다.
        constexpr int kEstSide = 4;    // 좌/우 테두리 각각
        constexpr int kEstTop = 31;    // 타이틀 바 + 상단 테두리
        constexpr int kEstBottom = 4;  // 하단 테두리
        const int maxClientW = usable.w - kEstSide * 2;
        const int maxClientH = usable.h - kEstTop - kEstBottom;
        if (createW > maxClientW && maxClientW > 320) createW = maxClientW;
        if (createH > maxClientH && maxClientH > 240) createH = maxClientH;
    }

    window_ = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        createW,
        createH,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!window_) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    // SDL이 실제로 생성한 창의 논리 포인트 크기를 앱 좌표계로 채택한다.
    // (HiDPI + Windows 작업 표시줄/장식 보정 후의 값)
    SDL_GetWindowSize(window_, &actualW, &actualH);
    logicalWidth_ = actualW;
    logicalHeight_ = actualH;

    std::fprintf(stderr,
        "[DPISYNC] window created at pt=%dx%d (requested %dx%d, logical %dx%d)\n",
        createW, createH, width, height, actualW, actualH);

    sdlRenderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdlRenderer_) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SDL_Quit();
        return false;
    }

    renderBackend_ = std::make_unique<jk::JKSDLRenderBackend>(sdlRenderer_);

    // 장식(타이틀 바/테두리) 포함 창 전체가 화면 작업 영역 중앙에 놓이도록 위치를
    // 보정한다. 렌더러 생성 이후로 미뤄야 물리 픽셀 크기를 알 수 있다.
    // - SDL_GetWindowBordersSize()는 물리 픽셀 단위를 반환하지만
    //   SDL_GetDisplayUsableBounds()/SDL_GetWindowSize()는 논리 포인트 단위이므로
    //   렌더러 출력(물리 픽셀) / 창 크기(논리 포인트) 비율로 DPI 배율을 구해 환산한다.
    // - SDL_SetWindowPosition()은 클라이언트 영역의 원점 기준으로 적용되므로,
    //   프레임 좌상단 목표 위치에 좌상단 장식 두께(논리 포인트)를 더해 지정해야
    //   타이틀 바가 화면 위로 밀려나 보이지 않는 일이 없다.
    // 장식(타이틀 바/테두리) 포함 창 전체를 현재 모니터 작업 영역 중앙에 배치한다.
    // 배치 계산은 모니터 이동(DPI 전환) 시에도 필요하므로 헬퍼로 추출했다.
    ReapplyPlacement();

    dc_ = JKDC(renderBackend_.get());
    resourceCache_ = std::make_unique<jk::JKResourceCache>(renderBackend_.get());

    // 텍스트 입력 이벤트(SDL_TEXTINPUT, SDL_TEXTEDITING)를 활성화한다.
    SDL_StartTextInput();

    hangulManager_ = std::make_unique<HangulManager>();
    if (hangulManager_->CreationError) {
        std::fprintf(stderr,
            "Warning: HangulManager font files missing; using built-in ASCII font.\n");
    }
    dc_.SetHangulManager(hangulManager_.get());
    HanMan = hangulManager_.get();
    resourceCache_->RegisterFont("default", hangulManager_.get());

    if (!mainWindow_) {
        mainWindow_ = std::make_unique<JKWindow>(title);
    }

    OnInit();

    // HiDPI 환경: SDL 논리 좌표(window size)와 물리 픽셀(renderer output size) 사이의
    // 배율을 저장해 두고, SDL_RenderSetScale()로 렌더링만 스케일한다.
    // 내부 좌표계(hit-test, layout, 드래그/리사이즈)는 모두 SDL 논리 좌표를 그대로 사용.
    // OnInit에서 교체된 mainWindow_라도 SDL 논리 크기(window size)로 맞춘다.
    UpdateScale();
    CreateOrResizeBackBuffer();

    // 안정 pt 시드: 모니터 전이 후 SDL이 제안 rect를 잘못 pt 환산해 창이
    // 축소되는 것을 복구할 때의 기준이 된다(SynchronizeWindowOnDisplayChanged).
    SDL_GetWindowSize(window_, &stablePtW_, &stablePtH_);

    windowManager_->SetMainWindow(mainWindow_.get());

    mainWindow_->Init();
    mainWindow_->Setup();
    mainWindow_->Open();
    mainWindow_->FocusFirstChild();

    running_ = true;
    return true;
}

void JKApplication::Close() {
    running_ = false;

    JKSoundManager::GetInstance().Quit();

    if (!window_) return;

    OnClose();
    if (mainWindow_) {
        mainWindow_->Close();
    }

    hangulManager_.reset();
    HanMan = nullptr;

    DestroyBackBuffer();
    renderBackend_.reset();
    if (sdlRenderer_) {
        SDL_DestroyRenderer(sdlRenderer_);
        sdlRenderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
    running_ = false;
}

int JKApplication::Run() {
    if (!running_ || !window_ || !mainWindow_) {
        return 1;
    }

    lastLegacyTimerTick_ = SDL_GetTicks();
    SDL_Event sdlEvent;
    while (running_) {
        // 1. 주기적 타이머 이벤트 생성
        const uint32_t now = SDL_GetTicks();

        // Legacy global timer (backward compatibility).
        if (legacyTimerInterval_ > 0 &&
            now - lastLegacyTimerTick_ >= legacyTimerInterval_) {
            lastLegacyTimerTick_ = now;
            JKEvent timerEv;
            timerEv.type = JKEventType::Timer;
            timerEv.targetId = mainWindow_ ? mainWindow_->GetWinId() : 0;
            timerEv.winId = timerEv.targetId;
            msgQue_.Push(timerEv);
        }

        // Deadline-based timers.
        while (!timers_.empty() && timers_.begin()->deadline <= now) {
            DeadlineTimer t = *timers_.begin();
            timers_.erase(timers_.begin());
            JKEvent timerEv;
            timerEv.type = JKEventType::Timer;
            timerEv.targetId = t.winId;
            timerEv.winId = t.winId;
            msgQue_.Push(timerEv);
            if (t.repeat) {
                t.deadline = now + t.intervalMs;
                timers_.insert(t);
            }
        }

        // 2. SDL 이벤트 수집 → JKEvent 변환 → MessageQue
        while (SDL_PollEvent(&sdlEvent)) {
            if (mouseLog_) {
                std::fprintf(mouseLog_, "[SDL-EV] type=%d\n", sdlEvent.type);
                std::fflush(mouseLog_);
            }
            if (sdlEvent.type == SDL_WINDOWEVENT) {
                OnSDLWindowEvent(sdlEvent.window);
                // Window events are either consumed by the debouncer or translated below.
            }

            JKEvent ev = TranslateSDLEvent(sdlEvent);
            if (ev.type != JKEventType::None) {
                msgQue_.Push(ev);
            }
        }

        // 3. 디바운스된 resize/DPI 변경 처리
        ProcessPendingResize(now);

        // 4. 메시지 처리
        JKEvent ev;
        while (msgQue_.Pop(ev)) {
            if (!PreProcessMessage(ev)) {
                running_ = false;
                break;
            }

            // Tab / Shift+Tab은 활성 윈도우 내에서 포커스를 이동시킨다.
            if (ev.type == JKEventType::KeyDown && ev.keyCode == SDLK_TAB) {
                JKWindow* active = windowManager_->GetKeyboardTargetWindow();
                bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
                if (active) {
                    if (shift) active->FocusPrevChild();
                    else       active->FocusNextChild();
                }
                continue;
            }

            RouteMessage(ev);
        }

        if (!running_) break;

        // 5. 닫기 요청된 자식 윈도우를 정리한다.
        if (mainWindow_) {
            mainWindow_->RemoveClosedChildren();
        }

        // 6. 그리기
        Render();

        // 7. ~60 FPS
        SDL_Delay(16);
    }

    return 0;
}

void JKApplication::SetMainWindow(std::unique_ptr<JKWindow> window) {
    mainWindow_ = std::move(window);
}

JKWindow* JKApplication::GetMainWindow() const {
    return mainWindow_.get();
}

void JKApplication::SetModalWindow(JKWindow* window) {
    windowManager_->SetModalWindow(window);
}

void JKApplication::SetCapture(JKControl* control) {
    windowManager_->SetCapture(control);
}

void JKApplication::ReleaseCapture() {
    windowManager_->ReleaseCapture();
}

void JKApplication::SetInputWindow(JKWindow* window) {
    windowManager_->SetInputWindow(window);
}

void JKApplication::SetTimerInterval(uint32_t ms) {
    legacyTimerInterval_ = ms;
}

uint64_t JKApplication::AddTimer(uint32_t winId, uint32_t intervalMs, bool repeat) {
    if (intervalMs == 0) return 0;
    DeadlineTimer t;
    t.handle = nextTimerHandle_++;
    t.winId = winId;
    t.intervalMs = intervalMs;
    t.deadline = SDL_GetTicks() + intervalMs;
    t.repeat = repeat;
    timers_.insert(t);
    return t.handle;
}

void JKApplication::RemoveTimer(uint64_t handle) {
    for (auto it = timers_.begin(); it != timers_.end(); ++it) {
        if (it->handle == handle) {
            timers_.erase(it);
            return;
        }
    }
}

void JKApplication::RemoveTimersForWindow(uint32_t winId) {
    for (auto it = timers_.begin(); it != timers_.end(); ) {
        if (it->winId == winId) {
            it = timers_.erase(it);
        } else {
            ++it;
        }
    }
}

JKControl* JKApplication::FindControlById(uint32_t winId) {
    return windowManager_->FindControlById(winId);
}

JKControl* JKApplication::FindControlByControlId(uint16_t controlId) {
    return windowManager_->FindControlByControlId(controlId);
}

JKWindow* JKApplication::FindWindowById(uint32_t winId) {
    return windowManager_->FindWindowById(winId);
}

void JKApplication::OnInit() {
}

void JKApplication::OnClose() {
}

bool JKApplication::PreProcessMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::Quit) {
        return false; // 루프 종료
    }
    return true;
}

void JKApplication::RouteMessage(const JKEvent& ev) {
    JKWindow* targetWindow = mainWindow_.get();
    JKControl* targetControl = nullptr;

    JKWindow* modal = windowManager_->GetModalWindow();
    if (modal) {
        targetWindow = modal;
    } else if (ev.winId != 0) {
        targetWindow = windowManager_->FindWindowById(ev.winId);
        if (!targetWindow) targetWindow = mainWindow_.get();
    } else if (ev.targetId != 0) {
        // Legacy path: targetId may encode either a window or control id.
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

void JKApplication::Render() {
    if (!renderBackend_ || backBuffer_ == JKRenderBackend::InvalidTexture) {
        // Nothing to draw until the backbuffer is ready.
        return;
    }

    // Decide whether we can use partial redraw.
    // If there are no dirty regions, fall back to the full-screen path so that
    // the very first frame and resize events are handled correctly.
    JKWindow* modal = windowManager_->GetModalWindow();
    JKWindow* dirtyWindow = nullptr;
    if (mainWindow_ && mainWindow_->HasDirtyRects()) {
        dirtyWindow = mainWindow_.get();
    } else if (modal && modal->HasDirtyRects()) {
        dirtyWindow = modal;
    }

    if (dirtyWindow) {
        RenderDirtyRegions(dirtyWindow);
        return;
    }

    // Full redraw path.
    // 백버퍼는 물리 픽셀 크기이지만, 앱 좌표계(논리 포인트)로 그리기 위해
    // SDL 렌더 스케일을 fit으로 설정한다. 그러면 24pt 타이틀 바가 125% DPI에서
    // 30px로 렌더링되는 등 Windows GUI와 동일한 HiDPI 동작을 얻는다.
    renderBackend_->SetRenderTarget(backBuffer_);
    renderBackend_->SetScale(scaleX_, scaleY_);

    // desktop background (논리 좌표, 스케일에 의해 물리 픽셀로 확대)
    dc_.SetColor(192, 192, 192, 255);
    dc_.Clear();

    if (mainWindow_) {
        mainWindow_->PaintWindow(dc_);
        mainWindow_->PaintClient(dc_);
    }
    if (modal) {
        modal->PaintWindow(dc_);
        modal->PaintClient(dc_);
    }

    // Blit the backbuffer to the default render target in unscaled pixel coords.
    // backbuffer는 이미 물리 픽셀 크기이므로 1:1로 화면에 복사한다.
    renderBackend_->SetRenderTarget(nullptr);
    renderBackend_->SetScale(1.0f, 1.0f);
    renderBackend_->SetDrawColor(192, 192, 192, 255);
    renderBackend_->Clear();
    const jk::JKRect srcRect{ 0, 0, backBufferW_, backBufferH_ };
    renderBackend_->BlitTexture(
        backBuffer_,
        &srcRect,
        jk::JKRect{ 0, 0, backBufferW_, backBufferH_ });
    renderBackend_->Present();
}

void JKApplication::RenderDirtyRegions(JKWindow* dirtyWindow) {
    const auto& dirtyRects = dirtyWindow->GetDirtyRects();
    if (dirtyRects.empty()) return;

    JKWindow* modal = windowManager_->GetModalWindow();

    // Render into the backbuffer in app logical coordinates, scaled to physical
    // pixels by SDL_RenderSetScale(fit). The dirty regions are registered in
    // app logical coordinates by JKControl::InvalidateRect, so the clip rect
    // and FillRect calls are consistent with the window hierarchy painting.
    renderBackend_->SetRenderTarget(backBuffer_);
    renderBackend_->SetScale(scaleX_, scaleY_);

    for (const auto& dirty : dirtyRects) {
        if (dirty.w <= 0 || dirty.h <= 0) continue;

        // Clip to the dirty region (app logical coords).
        dc_.PushClipRect(dirty);

        // Clear the dirty area to the desktop background first, then redraw
        // the window hierarchy over it. Because controls overlap and child
        // windows are drawn on top, we repaint the relevant window branch.
        dc_.SetColor(192, 192, 192, 255);
        dc_.FillRect(dirty);

        if (mainWindow_) {
            mainWindow_->PaintWindow(dc_);
            mainWindow_->PaintClient(dc_);
        }
        if (modal) {
            modal->PaintWindow(dc_);
            modal->PaintClient(dc_);
        }

        dc_.PopClipRect();
    }

    dirtyWindow->ClearDirtyRects();

    // Backbuffer already contains the full scene updated for dirty regions;
    // blit it 1:1 to the default render target (both are physical pixels).
    renderBackend_->SetRenderTarget(nullptr);
    renderBackend_->SetScale(1.0f, 1.0f);
    renderBackend_->SetDrawColor(192, 192, 192, 255);
    renderBackend_->Clear();
    const jk::JKRect srcRect{ 0, 0, backBufferW_, backBufferH_ };
    renderBackend_->BlitTexture(
        backBuffer_,
        &srcRect,
        jk::JKRect{ 0, 0, backBufferW_, backBufferH_ });
    renderBackend_->Present();
}

JKEvent JKApplication::TranslateSDLEvent(const SDL_Event& sdl) {
    JKEvent ev = jk::TranslateSDLEvent(sdl);

    if (!mainWindow_) {
        return ev;
    }

    // 마우스 캡처 라우팅은 좌표 변환(아래 공통 변환 블록) 이후에 수행한다.

    if (ev.type == JKEventType::MouseMove ||
        ev.type == JKEventType::MouseDown ||
        ev.type == JKEventType::MouseUp) {
        // Windows에서는 SDL 마우스 이벤트의 x/y가 논리 포인트인지 물리 픽셀인지가
        // SDL 버전/드라이버/입력 장치에 따라 달라질 수 있다(사용자 보고: 실제
        // 마우스가 ×DPI 만큼 밀려 보임). 실제 마우스 커서 위치를 Win32 API로 직접
        // 읽어 물리 픽셀 기준으로 통일하면, synthetic 클릭과 실제 마우스가 동일한
        // 좌표계를 사용한다. Non-Windows 폴백은 SDL 논리 포인트를 ptToPhys로
        // 물리 픽셀로 환산한 뒤 같은 공식을 적용한다.
        int physX = ev.x;
        int physY = ev.y;
        bool usedPlatformPhys = JKPlatform::GetPhysicalMousePos(window_, physX, physY);

        // SDL 폴백: 논리 포인트 좌표를 물리 픽셀로 변환한다.
        // (PAL 경로는 GetCursorPos+ScreenToClient로 이미 물리 픽셀을 얻는다.)
        if (!usedPlatformPhys) {
            if (ptToPhysX_ > 0.0f && ptToPhysY_ > 0.0f) {
                const float tmpPhysX = ev.x * ptToPhysX_;
                const float tmpPhysY = ev.y * ptToPhysY_;
                physX = static_cast<int>(tmpPhysX + (tmpPhysX >= 0.0f ? 0.5f : -0.5f));
                physY = static_cast<int>(tmpPhysY + (tmpPhysY >= 0.0f ? 0.5f : -0.5f));
            }
        }

        // 공통 변환: 물리 픽셀 -> 앱 논리 좌표 (레터박스 여백 제거 후 등비 배율로 나눔).
        if (scaleX_ > 0.0f && scaleY_ > 0.0f) {
            const float appX = (static_cast<float>(physX) - letterboxX_) / scaleX_;
            const float appY = (static_cast<float>(physY) - letterboxY_) / scaleY_;
            ev.x = static_cast<int32_t>(appX + (appX >= 0.0f ? 0.5f : -0.5f));
            ev.y = static_cast<int32_t>(appY + (appY >= 0.0f ? 0.5f : -0.5f));

            if (mouseLog_) {
                std::fprintf(mouseLog_,
                    "[TRSDL] type=%d phys=(%d,%d) usedPlatform=%d scale=(%.3f,%.3f) lb=(%d,%d) -> app=(%d,%d)\n",
                    static_cast<int>(ev.type), physX, physY, usedPlatformPhys ? 1 : 0,
                    scaleX_, scaleY_, letterboxX_, letterboxY_, ev.x, ev.y);
                std::fflush(mouseLog_);
            }

            if (ev.type == JKEventType::MouseMove) {
                // 상대 이동량: PAL 경로는 이전 물리 좌표와의 차이를,
                // 폴백 경로는 SDL relative motion을 물리 픽셀로 환산 후 앱 좌표로 변환.
                int physDx = ev.dx;
                int physDy = ev.dy;
                if (usedPlatformPhys) {
#ifdef _WIN32
                    if (hasLastMousePhys_) {
                        physDx = physX - lastMousePhysX_;
                        physDy = physY - lastMousePhysY_;
                    } else {
                        physDx = 0;
                        physDy = 0;
                    }
#endif
                } else {
                    if (ptToPhysX_ > 0.0f && ptToPhysY_ > 0.0f) {
                        physDx = static_cast<int>(ev.dx * ptToPhysX_);
                        physDy = static_cast<int>(ev.dy * ptToPhysY_);
                    }
                }
                ev.dx = static_cast<int32_t>(
                    physDx / scaleX_ + (physDx >= 0 ? 0.5f : -0.5f));
                ev.dy = static_cast<int32_t>(
                    physDy / scaleY_ + (physDy >= 0 ? 0.5f : -0.5f));
            }
        }

#ifdef _WIN32
        if (usedPlatformPhys) {
            lastMousePhysX_ = physX;
            lastMousePhysY_ = physY;
            hasLastMousePhys_ = true;
        }
#endif

        // 캡처 중이면 캡처한 컨트롤로 이동/뗌 이벤트를 계속 전달한다.
        JKControl* capture = windowManager_->GetCapture();
        if (capture &&
            (ev.type == JKEventType::MouseMove || ev.type == JKEventType::MouseUp)) {
            ev.targetId = capture->GetWinId();
            ev.winId = ev.targetId;
            ev.controlId = capture->GetControlId();
            return ev;
        }

        JKWindow* active = windowManager_->GetMouseTargetWindow();
        if (ev.type == JKEventType::MouseDown && capture) {
            windowManager_->ReleaseCapture();
        }
        if (active) {
            JKControl* target = active->HitTest(ev.x, ev.y);
            ev.targetId = target ? target->GetWinId() : active->GetWinId();
            ev.winId = ev.targetId;
            if (target) {
                ev.controlId = target->GetControlId();
            }
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

    return ev;
}

void JKApplication::UpdateScale() {
    if (!window_ || !sdlRenderer_ || !renderBackend_ || !mainWindow_) {
        return;
    }

    int windowW = 0, windowH = 0;
    SDL_GetWindowSize(window_, &windowW, &windowH);
    int renderW = 0, renderH = 0;
    renderBackend_->GetOutputSize(renderW, renderH);

    // 앱 논리 좌표계는 SDL이 보고하는 창 논리 포인트 크기와 동일하게 유지한다.
    // HiDPI 환경에서도 UI 요소(버튼, 타이틀 바, 폰트)가 Windows GUI처럼 일관된
    // 크기로 보이려면, 렌더링만 물리 픽셀에 맞춰 스케일하고 내부 좌표계는
    // 논리 포인트를 그대로 사용해야 한다. 요청 크기에 강제 고정하지 않으며,
    // 창이 화면 작업 영역에 맞춰 줄어들면 좌표계도 같이 줄어든다.
    logicalWidth_ = windowW;
    logicalHeight_ = windowH;
    const int appW = logicalWidth_;
    const int appH = logicalHeight_;

    // 등비(레터박스) 스케일: 창 논리 크기를 물리 출력 크기에 맞춘다.
    // 창과 렌더러는 동일한 창의 다른 단위이므로 비율이 같고, 레터박스는 0에
    // 가깝다. 추가 축소 없이 전체 내용을 1:1(물리 픽셀 기준)로 채운다.
    if (appW > 0 && appH > 0 && renderW > 0 && renderH > 0) {
        const float fitX = renderW / static_cast<float>(appW);
        const float fitY = renderH / static_cast<float>(appH);
        const float fit = (fitX < fitY) ? fitX : fitY;
        scaleX_ = fit;
        scaleY_ = fit;
        letterboxX_ = static_cast<int>((renderW - appW * fit) * 0.5f + 0.5f);
        letterboxY_ = static_cast<int>((renderH - appH * fit) * 0.5f + 0.5f);
    } else {
        scaleX_ = 1.0f;
        scaleY_ = 1.0f;
        letterboxX_ = 0;
        letterboxY_ = 0;
    }

    // 창 좌표(SDL 논리 포인트) -> 물리 픽셀 배율(DPI). 마우스 좌표 변환에 사용.
    ptToPhysX_ = (windowW > 0) ? renderW / static_cast<float>(windowW) : 1.0f;
    ptToPhysY_ = (windowH > 0) ? renderH / static_cast<float>(windowH) : 1.0f;

    // 진단 출력: 창/렌더 크기 변화 시에만 기록한다(반복 호출 노이즈 방지).
    static int sLoggedW = -1, sLoggedH = -1, sLoggedRW = -1, sLoggedRH = -1;
    if (windowW != sLoggedW || windowH != sLoggedH ||
        renderW != sLoggedRW || renderH != sLoggedRH) {
        sLoggedW = windowW; sLoggedH = windowH;
        sLoggedRW = renderW; sLoggedRH = renderH;
        std::fprintf(stderr,
            "[DPISYNC] UpdateScale: pt=%dx%d render=%dx%d ptToPhys=(%.3f,%.3f) fit=%.3f lb=(%d,%d)\n",
            windowW, windowH, renderW, renderH, ptToPhysX_, ptToPhysY_, scaleX_, letterboxX_, letterboxY_);
    }

    mainWindow_->SetWindowRect(JKRect{ 0, 0, appW, appH });
    CreateOrResizeBackBuffer();

#ifdef DEBUG
    SDL_version compiled, linked;
    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);
    std::printf("[DPI] windowSize=%dx%d renderSize=%dx%d scale=(%.3f,%.3f) "
                "SDL compiled=%d.%d.%d linked=%d.%d.%d\n",
                windowW, windowH, renderW, renderH, scaleX_, scaleY_,

                compiled.major, compiled.minor, compiled.patch,
                linked.major, linked.minor, linked.patch);
#endif
}


void JKApplication::CreateOrResizeBackBuffer() {
    if (!renderBackend_) return;
    int w = 0, h = 0;
    (*renderBackend_).GetOutputSize(w, h);
    if (w <= 0 || h <= 0) {
        // A zero-sized render target is invalid and will crash later draws.
        // Keep the previous buffer if we have one; otherwise leave it invalid.
        return;
    }
    if (backBuffer_ != JKRenderBackend::InvalidTexture &&
        w == backBufferW_ && h == backBufferH_) {
        return;
    }
    DestroyBackBuffer();
    backBuffer_ = (*renderBackend_).CreateTargetTexture(w, h);
    backBufferW_ = w;
    backBufferH_ = h;
}

// 장식(타이틀 바/테두리) 포함 창 전체를 현재 모니터 작업 영역 중앙에 배치한다.
// 주의(2026-08 다중 스케일 환경 검증): 이 머신(주 모니터 1920x1080@125% + 보조
// 1600x900@100% + 좌측 1920x1080@100%)에서 SDL_GetDisplayUsableBounds()는
// 디스플레이별 ddpi로 환산된 좌표를 섞어 반환하므로 신뢰할 수 없다. Windows
// 경로는 Win32 GetMonitorInfo(작업 영역, 물리 px — PMv1/PMv2 모두 실측 확인)와
// GetWindowRect/MapWindowPoints(장식 두께 산출)로 직접 계산하고, SDL
// SetWindowPosition용 pt 좌표는 GetDpiForWindow(실측)로 환산한다.
// Init()의 1회성 배치와 달리, 모니터 이동 후에는 장식 크기와 배율이 바뀌므로
// 매번 새로 조회해서 계산해야 한다(클라이언트 원점 보정 유지).
void JKApplication::ReapplyPlacement() {
    if (!window_) return;

    int ptW = 0, ptH = 0;
    SDL_GetWindowSize(window_, &ptW, &ptH);

    JKPlatform::Placement placement{};
    if (JKPlatform::ComputeCenteredPlacement(window_, ptW, ptH, placement)) {
        // Windows PAL path already accounts for frame decorations, monitor work
        // area, and per-window DPI. Diagnostic details are logged inside the PAL.
        std::fprintf(stderr,
            "[DPISYNC] ReapplyPlacement(pal): targetClientPx=(%d,%d) -> pt=(%d,%d)\n",
            placement.targetClientPxX, placement.targetClientPxY,
            placement.clientPtX, placement.clientPtY);
        SDL_SetWindowPosition(window_, placement.clientPtX, placement.clientPtY);
        return;
    }

    // Non-Windows / PAL failure fallback: use SDL display bounds and estimated
    // border sizes. This keeps the window on-screen but is less precise than the
    // Win32 path.
    int displayIndex = SDL_GetWindowDisplayIndex(window_);
    if (displayIndex < 0) displayIndex = 0;
    SDL_Rect usable{};
    if (SDL_GetDisplayUsableBounds(displayIndex, &usable) != 0 ||
        usable.w <= 0 || usable.h <= 0) {
        return;
    }

    int renderW = 0, renderH = 0;
    if (renderBackend_) renderBackend_->GetOutputSize(renderW, renderH);
    const float dpiScale = (renderW > 0 && ptW > 0)
                         ? renderW / static_cast<float>(ptW) : 1.0f;

    int top = 0, left = 0, bottom = 0, right = 0;
    if (SDL_GetWindowBordersSize(window_, &top, &left, &bottom, &right) != 0 ||
        top < 0 || left < 0 || bottom < 0 || right < 0) {
        top = 31; left = 4; bottom = 4; right = 4;
    }

    const int frameW = ptW + static_cast<int>((left + right) / dpiScale + 0.5f);
    const int frameH = ptH + static_cast<int>((top + bottom) / dpiScale + 0.5f);
    const int outerX = usable.x + ((usable.w <= frameW) ? 0 : (usable.w - frameW) / 2);
    const int outerY = usable.y + ((usable.h <= frameH) ? 0 : (usable.h - frameH) / 2);
    const int posX = outerX + static_cast<int>(left / dpiScale + 0.5f);
    const int posY = outerY + static_cast<int>(top / dpiScale + 0.5f);

    std::fprintf(stderr,
        "[DPISYNC] ReapplyPlacement(sdl-fallback): display=%d usable=(%d,%d %dx%d) pt=%dx%d "
        "render=%dx%d dpiScale=%.3f framePt=%dx%d -> pos=(%d,%d)\n",
        displayIndex, usable.x, usable.y, usable.w, usable.h,
        ptW, ptH, renderW, renderH, dpiScale, frameW, frameH, posX, posY);
    SDL_SetWindowPosition(window_, posX, posY);
}

// 모니터 이동(WM_DPICHANGED → SDL_WINDOWEVENT_DISPLAY_CHANGED) 후 처리.
// 주의: SDL_GetDisplayDPI() 기반으로 창 pt 크기를 재계산하는 로직은 제거했다.
// 검증(2026-08, 다중 모니터 스케일 혼합 환경) 결과:
//  - SDL이 WM_DPICHANGED 제안 크기를 이미 반영한 뒤 남는 DISPLAY_CHANGED 잔향
//    이벤트마다 expPt = renderPx ÷ dpiScale 을 다시 적용하면 배율이 이중 적용되어
//    창이 ×0.8 로 연쇄 축소된다(1910→1528→1222→978→782 재현 확인).
//  - 창 pt 크기는 SDL의 DPI 자동 스케일링(제안 rect)에 맡기면 원 모니터 복귀 시
//    원래 크기(1910x976px)로 정확히 복원된다.
// 따라서 이 핸들러는 파생 상태 재계산만 수행한다:
//  - UpdateScale(): ptToPhys_(마우스 pt→px 배율), 등비 스케일/레터박스, 백버퍼
//  - ReapplyPlacement(): 현재 모니터 작업 영역 중앙 배치(타이틀바 보존)
void JKApplication::SynchronizeWindowOnDisplayChanged(int displayIndex) {
    if (!window_ || !renderBackend_) {
        return;
    }
    if (displayIndex < 0) {
        displayIndex = SDL_GetWindowDisplayIndex(window_);
        if (displayIndex < 0) {
            displayIndex = 0;
        }
    }

    int renderW = 0, renderH = 0;
    renderBackend_->GetOutputSize(renderW, renderH);
    int ptW = 0, ptH = 0;
    SDL_GetWindowSize(window_, &ptW, &ptH);

    std::fprintf(stderr,
        "[DPISYNC] Synchronize: display=%d render=%dx%d pt=%dx%d (no size resync)\n",
        displayIndex, renderW, renderH, ptW, ptH);

    UpdateScale();

    // SDL의 WM_DPICHANGED 처리는 제안 rect를 stale dpiScale로 pt 환산하는 탓에
    // 창 pt를 x0.8(연쇄 시 x0.64)로 잘못 줄인다(PMv1/PMv2 매니페스트 모두 실측
    // 재현: 1916x1011 -> 1228x654). 안정 pt(Init 직후 창 크기)로 되돌린다.
    // 목표가 불변값이므로 반복 DISPLAY_CHANGED 잔향 이벤트에도 수렴하며,
    // 이전 세션처럼 expPt를 누적 재적용하는 연쇄 축소 사고는 재현되지 않는다.
    int curPtW = 0, curPtH = 0;
    SDL_GetWindowSize(window_, &curPtW, &curPtH);
    if (stablePtW_ > 0 && stablePtH_ > 0 &&
        (curPtW != stablePtW_ || curPtH != stablePtH_)) {
        std::fprintf(stderr,
            "[DPISYNC] Resync pt: cur=%dx%d -> stable=%dx%d\n",
            curPtW, curPtH, stablePtW_, stablePtH_);
        SDL_SetWindowSize(window_, stablePtW_, stablePtH_);
    }

    ReapplyPlacement();
}

void JKApplication::DestroyBackBuffer() {
    if (backBuffer_ != JKRenderBackend::InvalidTexture && renderBackend_) {
        (*renderBackend_).DestroyTexture(backBuffer_);
        backBuffer_ = JKRenderBackend::InvalidTexture;
        backBufferW_ = 0;
        backBufferH_ = 0;
    }
}

void JKApplication::OnSDLWindowEvent(const SDL_WindowEvent& winEv) {
    const Uint8 ev = winEv.event;
    if (ev == SDL_WINDOWEVENT_SIZE_CHANGED ||
        ev == SDL_WINDOWEVENT_DISPLAY_CHANGED) {
        std::fprintf(stderr,
            "[DPISYNC] Run(): winEv=%s data1=%d data2=%d\n",
            (ev == SDL_WINDOWEVENT_SIZE_CHANGED) ? "SIZE_CHANGED" : "DISPLAY_CHANGED",
            winEv.data1, winEv.data2);
    }

    if (ev == SDL_WINDOWEVENT_SIZE_CHANGED) {
        // 디바운서에 기록. 실제 처리는 ProcessPendingResize에서 마감 시점에 한 번.
        pendingResizeW_ = winEv.data1;
        pendingResizeH_ = winEv.data2;
        pendingResizeDeadline_ = SDL_GetTicks() + kResizeDebounceMs;
        resizePending_ = true;
    } else if (ev == SDL_WINDOWEVENT_DISPLAY_CHANGED) {
        pendingDpiDisplay_ = winEv.data1;
        pendingResizeDeadline_ = SDL_GetTicks() + kResizeDebounceMs;
        resizePending_ = true;
    }
}

void JKApplication::ProcessPendingResize(uint32_t now) {
    if (!resizePending_) return;
    if (now < pendingResizeDeadline_) return;

    const bool hadDpiChange = pendingDpiDisplay_ >= 0;

    // Display 변경이 있으면 SynchronizeWindowOnDisplayChanged 내부에서 UpdateScale()도
    // 호출하므로, 별도의 UpdateScale() 호출은 불필요.
    if (hadDpiChange) {
        SynchronizeWindowOnDisplayChanged(pendingDpiDisplay_);
    } else if (pendingResizeW_ > 0 && pendingResizeH_ > 0) {
        UpdateScale();
    }

    // 앱에 하나의 SizeChanged 이벤트를 발행.
    if (mainWindow_) {
        const uint32_t mainId = mainWindow_->GetWinId();
        JKEvent sizeEv;
        sizeEv.type = JKEventType::SizeChanged;
        sizeEv.targetId = mainId;
        sizeEv.winId = mainId;
        sizeEv.x = logicalWidth_;
        sizeEv.y = logicalHeight_;
        msgQue_.Push(sizeEv);

        if (hadDpiChange) {
            int displayIndex = SDL_GetWindowDisplayIndex(window_);
            if (displayIndex < 0) displayIndex = 0;
            JKEvent dpiEv;
            dpiEv.type = JKEventType::DpiChanged;
            dpiEv.targetId = mainId;
            dpiEv.winId = mainId;
            dpiEv.detail = static_cast<uint32_t>(displayIndex);
            dpiEv.option = static_cast<uint32_t>(scaleX_ * 1000.0f + 0.5f);
            msgQue_.Push(dpiEv);
        }
    }

    pendingResizeW_ = 0;
    pendingResizeH_ = 0;
    pendingDpiDisplay_ = -1;
    resizePending_ = false;
}

} // namespace jk
