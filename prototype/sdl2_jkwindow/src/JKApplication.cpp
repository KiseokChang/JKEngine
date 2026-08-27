#include <JKApplication.h>
#include <JKEvent.h>
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
#endif

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    window_ = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!window_) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SDL_Quit();
        return false;
    }

    dc_ = JKDC(renderer_);

    // 텍스트 입력 이벤트(SDL_TEXTINPUT, SDL_TEXTEDITING)를 활성화한다.
    SDL_StartTextInput();

    hangulManager_ = std::make_unique<HangulManager>();
    if (hangulManager_->CreationError) {
        std::fprintf(stderr,
            "Warning: HangulManager font files missing; using built-in ASCII font.\n");
    }
    dc_.SetHangulManager(hangulManager_.get());
    HanMan = hangulManager_.get();

    if (!mainWindow_) {
        mainWindow_ = std::make_unique<JKWindow>(title);
    }

    OnInit();

    // HiDPI 환경: SDL 논리 좌표(window size)와 물리 픽셀(renderer output size) 사이의
    // 배율을 저장해 두고, SDL_RenderSetScale()로 렌더링만 스케일한다.
    // 내부 좌표계(hit-test, layout, 드래그/리사이즈)는 모두 SDL 논리 좌표를 그대로 사용.
    // OnInit에서 교체된 mainWindow_라도 SDL 논리 크기(window size)로 맞춘다.
    UpdateScale();

    mainWindow_->Init();
    mainWindow_->Setup();
    mainWindow_->Open();

    running_ = true;
    return true;
}

void JKApplication::Close() {
    if (!running_ && !window_) return;

    OnClose();
    if (mainWindow_) {
        mainWindow_->Close();
    }

    hangulManager_.reset();
    HanMan = nullptr;

    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
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
            if (sdlEvent.type == SDL_WINDOWEVENT &&
                sdlEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                UpdateScale();
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
    // SDL 논리 좌표 -> 물리 픽셀 변환. 이후 모든 그리기 명령은 논리 좌표로
    // 전달되며 SDL_Renderer가 자동으로 HiDPI 스케일을 적용한다.
    if (renderer_) {
        SDL_RenderSetScale(renderer_, scaleX_, scaleY_);
    }

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

    dc_.Present();
}

JKEvent JKApplication::TranslateSDLEvent(const SDL_Event& sdl) {
    JKEvent ev = jk::TranslateSDLEvent(sdl);

    if (!mainWindow_) {
        return ev;
    }

    // 마우스 캡처 중이면 캡처한 컨트롤로 이동/뗌 이벤트를 계속 전달한다.
    if (captureControl_ &&
        (ev.type == JKEventType::MouseMove || ev.type == JKEventType::MouseUp)) {
        ev.targetId = captureControl_->GetWinId();
        return ev;
    }

    if (ev.type == JKEventType::MouseMove ||
        ev.type == JKEventType::MouseDown ||
        ev.type == JKEventType::MouseUp) {
        // SDL_HINT_WINDOWS_DPI_SCALING=1 상태에서는 SDL 마우스 좌표가 이미
        // 논리 좌표이며, 내부 좌표계도 논리 좌표를 그대로 사용하므로
        // 별도 변환은 필요 없다. SDL_RenderSetScale()이 그리기만 물리 픽셀로 변환.

        // 내부 좌표계는 SDL 논리 좌표를 그대로 사용한다.
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
    if (!window_ || !renderer_ || !mainWindow_) {
        return;
    }

    int windowW = 0, windowH = 0;
    SDL_GetWindowSize(window_, &windowW, &windowH);
    int renderW = 0, renderH = 0;
    SDL_GetRendererOutputSize(renderer_, &renderW, &renderH);

    if (windowW > 0 && windowH > 0) {
        scaleX_ = static_cast<float>(renderW) / windowW;
        scaleY_ = static_cast<float>(renderH) / windowH;
    } else {
        scaleX_ = 1.0f;
        scaleY_ = 1.0f;
    }

    mainWindow_->SetWindowRect(JKRect{ 0, 0, windowW, windowH });

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

} // namespace jk
