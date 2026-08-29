#include <JKApplication.h>
#include <JKEvent.h>
#include <JKSDLRenderBackend.h>
#include <SDL_syswm.h>
#include <cstdio>

namespace jk {

JKApplication* g_currentJKApp = nullptr;

JKApplication::JKApplication() : dc_(nullptr) {
    g_currentJKApp = this;
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
#endif

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

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

    // 앱 논리 좌표계는 Init에 요청한 크기로 고정한다. 창이 화면(작업 영역)보다
    // 작아지더라도 앱 레이아웃은 이 좌표계를 그대로 사용하고, UpdateScale()이
    // 렌더링/입력을 등비(레터박스)로 스케일한다.
    logicalWidth_ = width;
    logicalHeight_ = height;

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

    std::fprintf(stderr,
        "[DPISYNC] window created at pt=%dx%d (requested %dx%d)\n",
        createW, createH, width, height);

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

    mainWindow_->Init();
    mainWindow_->Setup();
    mainWindow_->Open();

    running_ = true;
    return true;
}

void JKApplication::Close() {
    running_ = false;
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

    lastTimerTick_ = SDL_GetTicks();
    SDL_Event sdlEvent;
    while (running_) {
        // 1. 주기적 타이머 이벤트 생성
        uint32_t now = SDL_GetTicks();
        if (now - lastTimerTick_ >= timerInterval_) {
            lastTimerTick_ = now;
            JKEvent timerEv;
            timerEv.type = JKEventType::Timer;
            timerEv.targetId = mainWindow_->GetWinId();
            msgQue_.Push(timerEv);
        }

        // 2. SDL 이벤트 수집 → JKEvent 변환 → MessageQue
        while (SDL_PollEvent(&sdlEvent)) {
            if (mouseLog_) {
                std::fprintf(mouseLog_, "[SDL-EV] type=%d\n", sdlEvent.type);
                std::fflush(mouseLog_);
            }
            if (sdlEvent.type == SDL_WINDOWEVENT) {
                const Uint8 winEv = sdlEvent.window.event;
                if (winEv == SDL_WINDOWEVENT_SIZE_CHANGED ||
                    winEv == SDL_WINDOWEVENT_DISPLAY_CHANGED) {
                    std::fprintf(stderr,
                        "[DPISYNC] Run(): winEv=%s data1=%d data2=%d\n",
                        (winEv == SDL_WINDOWEVENT_SIZE_CHANGED)
                            ? "SIZE_CHANGED" : "DISPLAY_CHANGED",
                        sdlEvent.window.data1, sdlEvent.window.data2);
                }
                if (winEv == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    UpdateScale();
                } else if (winEv == SDL_WINDOWEVENT_DISPLAY_CHANGED) {
                    // 모니터 이동(WM_DPICHANGED): 사이즈 이벤트만으로는 배율·창 pt
                    // 크기·장식 배치가 새 모니터 기준과 어긋난 채 남을 수 있다.
                    SynchronizeWindowOnDisplayChanged(sdlEvent.window.data1);
                }
            }

            JKEvent ev = TranslateSDLEvent(sdlEvent);
            if (ev.type != JKEventType::None) {
                msgQue_.Push(ev);
            }
        }

        // 3. 메시지 처리
        JKEvent ev;
        while (msgQue_.Pop(ev)) {
            if (!PreProcessMessage(ev)) {
                running_ = false;
                break;
            }

            // Tab / Shift+Tab은 활성 윈도우 내에서 포커스를 이동시킨다.
            if (ev.type == JKEventType::KeyDown && ev.keyCode == SDLK_TAB) {
                JKWindow* active = modalWindow_ ? modalWindow_
                                                : (inputWindow_ ? inputWindow_ : mainWindow_.get());
                bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
                if (shift) active->FocusPrevChild();
                else       active->FocusNextChild();
                continue;
            }

            RouteMessage(ev);
        }

        if (!running_) break;

        // 4. 닫기 요청된 자식 윈도우를 정리한다.
        if (mainWindow_) {
            mainWindow_->RemoveClosedChildren();
        }

        // 5. 그리기
        Render();

        // 6. ~60 FPS
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
    if (modalWindow_ == window) return;
    ReleaseCapture();
    modalWindow_ = window;
    if (modalWindow_) {
        inputWindow_ = modalWindow_;
        modalWindow_->FocusFirstChild();
    } else {
        inputWindow_ = mainWindow_.get();
    }
}

void JKApplication::SetCapture(JKControl* control) {
    captureControl_ = control;
}

void JKApplication::ReleaseCapture() {
    captureControl_ = nullptr;
}

void JKApplication::SetInputWindow(JKWindow* window) {
    inputWindow_ = window;
}

JKControl* JKApplication::FindControlById(uint32_t winId) {
    if (!mainWindow_) return nullptr;
    return mainWindow_->FindControlById(winId);
}

JKControl* JKApplication::FindControlByControlId(uint16_t controlId) {
    if (!mainWindow_) return nullptr;
    return mainWindow_->FindControlByControlId(controlId);
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
    if (modalWindow_) {
        JKEvent modalEv = ev;
        modalEv.targetId = modalWindow_->GetWinId();
        modalWindow_->RespondMessage(modalEv);
        return;
    }

    JKControl* target = FindControlById(ev.targetId);
    if (!target) target = mainWindow_.get();
    target->RespondMessage(ev);
}

void JKApplication::Render() {
    if (!renderBackend_) return;

    // Render everything into the backbuffer at scaled logical coordinates.
    renderBackend_->SetScale(scaleX_, scaleY_);
    renderBackend_->SetRenderTarget(backBuffer_);

    // desktop background
    dc_.SetColor(192, 192, 192, 255);
    dc_.Clear();

    if (mainWindow_) {
        mainWindow_->PaintWindow(dc_);
        mainWindow_->PaintClient(dc_);
    }
    if (modalWindow_) {
        modalWindow_->PaintWindow(dc_);
        modalWindow_->PaintClient(dc_);
    }

    // Blit the backbuffer to the default render target in unscaled pixel coords.
    // 화면 전체를 데스크톱 배경색으로 먼저 지운다(레터박스 여백).
    renderBackend_->SetRenderTarget(nullptr);
    renderBackend_->SetScale(1.0f, 1.0f);
    renderBackend_->SetDrawColor(192, 192, 192, 255);
    renderBackend_->Clear();
    // 앱 좌표계 내용 영역(appW*scale x appH*scale)만 레터박스 여백 중앙에 배치한다.
    const int appW = (logicalWidth_ > 0) ? logicalWidth_ : backBufferW_;
    const int appH = (logicalHeight_ > 0) ? logicalHeight_ : backBufferH_;
    const int contentW = static_cast<int>(appW * scaleX_ + 0.5f);
    const int contentH = static_cast<int>(appH * scaleY_ + 0.5f);
    const jk::JKRect srcRect{ 0, 0, contentW, contentH };
    renderBackend_->BlitTexture(
        backBuffer_,
        &srcRect,
        jk::JKRect{ letterboxX_, letterboxY_, contentW, contentH });
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
        bool usedWin32Phys = false;
#ifdef _WIN32
        if (window_) {
            SDL_SysWMinfo wmInfo;
            SDL_VERSION(&wmInfo.version);
            if (SDL_GetWindowWMInfo(window_, &wmInfo) &&
                wmInfo.subsystem == SDL_SYSWM_WINDOWS &&
                wmInfo.info.win.window) {
                HWND hwnd = wmInfo.info.win.window;
                POINT pt;
                if (GetCursorPos(&pt) && ScreenToClient(hwnd, &pt)) {
                    physX = static_cast<int>(pt.x);
                    physY = static_cast<int>(pt.y);
                    usedWin32Phys = true;
                }
            }
        }
#endif

        // SDL 폴백: 논리 포인트 좌표를 물리 픽셀로 변환한다.
        // (Win32 경로는 GetCursorPos+ScreenToClient로 이미 물리 픽셀을 얻는다.)
        if (!usedWin32Phys) {
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
                    "[TRSDL] type=%d phys=(%d,%d) usedWin32=%d scale=(%.3f,%.3f) lb=(%d,%d) -> app=(%d,%d)\n",
                    static_cast<int>(ev.type), physX, physY, usedWin32Phys ? 1 : 0,
                    scaleX_, scaleY_, letterboxX_, letterboxY_, ev.x, ev.y);
                std::fflush(mouseLog_);
            }

            if (ev.type == JKEventType::MouseMove) {
                // 상대 이동량: Win32 경로는 이전 물리 좌표와의 차이를,
                // 폴백 경로는 SDL relative motion을 물리 픽셀로 환산 후 앱 좌표로 변환.
                int physDx = ev.dx;
                int physDy = ev.dy;
                if (usedWin32Phys) {
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
        if (usedWin32Phys) {
            lastMousePhysX_ = physX;
            lastMousePhysY_ = physY;
            hasLastMousePhys_ = true;
        }
#endif

        // 캡처 중이면 캡처한 컨트롤로 이동/뗌 이벤트를 계속 전달한다.
        if (captureControl_ &&
            (ev.type == JKEventType::MouseMove || ev.type == JKEventType::MouseUp)) {
            ev.targetId = captureControl_->GetWinId();
            return ev;
        }

        JKWindow* active = modalWindow_ ? modalWindow_ : mainWindow_.get();
        if (ev.type == JKEventType::MouseDown && captureControl_) {
            ReleaseCapture();
        }
        JKControl* target = active->HitTest(ev.x, ev.y);
        ev.targetId = target ? target->GetWinId() : active->GetWinId();
    } else if (ev.type == JKEventType::KeyDown ||
               ev.type == JKEventType::KeyUp ||
               ev.type == JKEventType::Char) {
        JKWindow* active = modalWindow_ ? modalWindow_ : (inputWindow_ ? inputWindow_ : mainWindow_.get());
        ev.targetId = active->GetWinId();
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

    // 앱 좌표계는 Init에 요청한 논리 크기로 고정한다.
    const int appW = (logicalWidth_ > 0) ? logicalWidth_ : windowW;
    const int appH = (logicalHeight_ > 0) ? logicalHeight_ : windowH;

    // 등비(레터박스) 스케일: 창 비율이 앱 좌표계 비율과 달라도 왜곡 없이
    // 앱 내용 전체가 보이도록 x/y 배율을 통일하고 남는 영역은 여백으로 둔다.
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
            "[DPISYNC] UpdateScale: pt=%dx%d render=%dx%d ptToPhys=(%.3f,%.3f) fit=%.3f\n",
            windowW, windowH, renderW, renderH, ptToPhysX_, ptToPhysY_, scaleX_);
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
    if (!window_ || !renderBackend_) {
        return;
    }

    int ptW = 0, ptH = 0;
    SDL_GetWindowSize(window_, &ptW, &ptH);
    int renderW = 0, renderH = 0;
    renderBackend_->GetOutputSize(renderW, renderH);
    if (ptW <= 0 || ptH <= 0 || renderW <= 0 || renderH <= 0) {
        return;
    }
    const float dpiScale = renderW / static_cast<float>(ptW);

#ifdef _WIN32
    // Win32 경로: 현재 모니터 작업 영역을 물리 픽셀로 직접 조회한다.
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(window_, &wmInfo) &&
        wmInfo.subsystem == SDL_SYSWM_WINDOWS && wmInfo.info.win.window) {
        HWND hwnd = wmInfo.info.win.window;
        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        RECT frame{};
        if (monitor && GetMonitorInfoW(monitor, &mi) && GetWindowRect(hwnd, &frame)) {
            const int frameW = frame.right - frame.left;
            const int frameH = frame.bottom - frame.top;
            // GetMonitorInfo/GetWindowRect는 PMv1/PMv2 모두 물리 px를 반환함을
            // PMv2 드라이버 덤프와 교차 검증했다(2026-08: 보조 모니터 1600x900@100%,
            // rcWork 높이 852 = 900-48 태스크바). 초기 세션의 "모니터 rect ×0.8
            // 가상화" 가설은 Screen.AllScreens의 ×1.25 가상화 값을 참으로 오신 것 — 폐기.
            // pt 환산은 GetDpiForWindow(실측 창 DPI)로만 한다.
            const UINT winDpiMon = GetDpiForWindow(hwnd);
            // 스레드 컨텍스트는 SDL 내부 초기화가 조작할 수 있어 실측상 신뢰가
            // 없다(PMv2 매니페스트 앱에서도 -4가 아닌 값을 반환 — 2026-08 실측).
            // PMv2 판정은 외부 관찰과 교차 검증된 아래 API 조합을 쓴다.
            const bool pmv2 = AreDpiAwarenessContextsEqual(
                GetWindowDpiAwarenessContext(hwnd),
                (DPI_AWARENESS_CONTEXT)(void*)(intptr_t)-4) != FALSE;
            const int workLeft = mi.rcWork.left;
            const int workTop = mi.rcWork.top;
            const int workW = mi.rcWork.right - mi.rcWork.left;
            const int workH = mi.rcWork.bottom - mi.rcWork.top;
            if (workW > 0 && workH > 0 && frameW > 0 && frameH > 0) {
                // 프레임이 작업 영역보다 크면 중앙 정렬 시 좌표가 음수가 되어
                // 타이틀 바가 화면 위로 잘리므로, 그 경우 작업 영역 원점에 맞춘다.
                const int outerPxX = workLeft + ((workW > frameW) ? (workW - frameW) / 2 : 0);
                const int outerPxY = workTop + ((workH > frameH) ? (workH - frameH) / 2 : 0);

                // 프레임 좌상단(outer)에서 클라이언트 원점까지의 장식 두께(물리 px).
                // MapWindowPoints의 반환값은 클라이언트 원점의 절대 화면 좌표이므로
                // 현재 프레임 원점을 빼야 장식 두께가 된다(절대 좌표를 그대로
                // 더하면 배치가 매번 틀어진다 — 2026-08 회귀 원인).
                RECT clientRect{};
                POINT clientOrigin{0, 0};
                GetClientRect(hwnd, &clientRect);
                MapWindowPoints(hwnd, nullptr, &clientOrigin, 1);
                const int borderPxX = clientOrigin.x - frame.left;
                const int borderPxY = clientOrigin.y - frame.top;
                const int clientPxX = outerPxX + borderPxX;
                const int clientPxY = outerPxY + borderPxY;

                // SDL_SetWindowPosition은 창 pt(창 DPI 환산) 좌표를 받는다.
                const float ptScale = (winDpiMon > 0) ? (winDpiMon / 96.0f) : dpiScale;
                const int posPtX = static_cast<int>(clientPxX / ptScale + 0.5f);
                const int posPtY = static_cast<int>(clientPxY / ptScale + 0.5f);

                std::fprintf(stderr,
                    "[DPISYNC] ReapplyPlacement(win32): frame=(%d,%d %dx%d) work=(%d,%d %dx%d) "
                    "pmv2=%d border=(%d,%d) ptScale=%.3f -> pt=(%d,%d)\n",
                    frame.left, frame.top, frameW, frameH, workLeft, workTop,
                    workW, workH, pmv2 ? 1 : 0, borderPxX, borderPxY,
                    ptScale, posPtX, posPtY);
                SDL_SetWindowPosition(window_, posPtX, posPtY);
                return;
            }
        }
        // Win32 조회 실패 시 아래 SDL 폴백으로 진행.
    }
#endif

    // 폴백: SDL 표시 경계 기반 계산(비-Windows / Win32 조회 실패).
    int displayIndex = SDL_GetWindowDisplayIndex(window_);
    if (displayIndex < 0) {
        displayIndex = 0;
    }
    SDL_Rect usable{};
    if (SDL_GetDisplayUsableBounds(displayIndex, &usable) != 0 ||
        usable.w <= 0 || usable.h <= 0) {
        return;
    }

    int top = 0, left = 0, bottom = 0, right = 0;
    if (SDL_GetWindowBordersSize(window_, &top, &left, &bottom, &right) != 0 ||
        top < 0 || left < 0 || bottom < 0 || right < 0) {
        top = 31; left = 4; bottom = 4; right = 4;
    }

    // 장식을 포함한 프레임 전체 크기(논리 포인트)를 구해 작업 영역 중앙에
    // 놓이는 프레임 좌상단 위치를 계산한다.
    const int frameW = ptW + static_cast<int>((left + right) / dpiScale + 0.5f);
    const int frameH = ptH + static_cast<int>((top + bottom) / dpiScale + 0.5f);
    const int outerX = usable.x + ((usable.w <= frameW) ? 0 : (usable.w - frameW) / 2);
    const int outerY = usable.y + ((usable.h <= frameH) ? 0 : (usable.h - frameH) / 2);

    // 클라이언트 원점 기준 보정: 좌상단 장식 두께만큼 더한다.
    const int posX = outerX + static_cast<int>(left / dpiScale + 0.5f);
    const int posY = outerY + static_cast<int>(top / dpiScale + 0.5f);
    // 진단 출력(Init/모니터 이동 시에만 호출되어 로그가 드물다).
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
} // namespace jk
