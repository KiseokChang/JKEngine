#include <JKApplication.h>
#include <JKAudioThread.h>
#include <JKEvent.h>
#include <JKSDLAudioBackend.h>
#include <JKRenderThread.h>
#include <JKSDLRenderBackend.h>
#include <JKRenderCommandList.h>
#include <JKPlatform.h>
#include <JKSoundManager.h>
#include <JKTimerThread.h>
#include <cstdio>
#include <cstring>

namespace jk {

JKApplication* g_currentJKApp = nullptr;

JKApplication::JKApplication() : dc_(nullptr) {
    g_currentJKApp = this;
    windowManager_ = std::make_unique<JKWindowManager>();
    messageBus_ = std::make_unique<JKMessageBus>();
}

JKApplication::~JKApplication() {
    Close();
    if (g_currentJKApp == this) g_currentJKApp = nullptr;
}

bool JKApplication::Init(const std::string& title, int width, int height) {
#ifdef _WIN32
    mouseLog_ = std::fopen("C:\\temp_jkwin_verify\\mouse.log", "w");
#endif

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "JKENGINE Error",
            (std::string("SDL video init failed: ") + SDL_GetError()).c_str(), nullptr);
        return false;
    }

    // Render thread owns the SDL window and renderer.
    renderThread_ = std::make_unique<JKRenderThread>();
    if (!renderThread_->Init(title, width, height)) {
        SDL_Quit();
        return false;
    }

    // Audio thread owns SDL_mixer and initializes its own SDL audio subsystem.
    audioThread_ = std::make_unique<JKAudioThread>();
    audioThread_->Start(messageBus_.get(), std::make_unique<SDLAudioBackend>());

    // Route JKSoundManager calls through the audio thread.
    {
        AudioCommand initCmd;
        initCmd.type = AudioCommand::Type::Init;
        PostAudioCommand(initCmd);

        auto& soundManager = JKSoundManager::GetInstance();
        soundManager.SetCommandMode(true);
        soundManager.Init();
    }

    // Launch the render thread. It will create the SDL window and renderer on
    // its own thread so that all SDL_Window/SDL_Renderer API calls stay on one
    // thread. WaitInit() blocks until creation is complete.
    renderThread_->Start(messageBus_.get());
    if (!renderThread_->WaitInit()) {
        std::fprintf(stderr, "JKApplication::Init: render thread failed to create window.\n");
        SDL_Quit();
        return false;
    }
    sdlWindow_ = renderThread_->GetWindow();

    // Initial logical size comes from the render thread's created window.
    renderThread_->GetLogicalSize(logicalWidth_, logicalHeight_);

    // Compute the initial DPI scale so the first mouse events are translated
    // with the correct scale factor.
    {
        int physW = 0, physH = 0;
        if (renderThread_->GetBackend()) {
            renderThread_->GetBackend()->GetOutputSize(physW, physH);
        }
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (logicalWidth_ > 0 && logicalHeight_ > 0 && physW > 0 && physH > 0) {
            scaleX_ = physW / static_cast<float>(logicalWidth_);
            scaleY_ = physH / static_cast<float>(logicalHeight_);
        }
    }

    // Resource cache needs a render backend; use the render thread's backend.
    dc_ = JKDC(renderThread_->GetBackend());
    resourceCache_ = std::make_unique<jk::JKResourceCache>(renderThread_->GetBackend());
    renderThread_->SetResourceCache(resourceCache_.get());

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

    // Start the timer thread before OnInit() so apps can configure timers
    // during initialization. Events posted before the main loop starts are
    // simply drained once Run() begins.
    timerThread_ = std::make_unique<JKTimerThread>();
    timerThread_->Start(messageBus_.get());

    OnInit();

    // In single-window mode mainWindow fills the logical window size.
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

    running_ = true;
    return true;
}

void JKApplication::Close() {
    running_ = false;

    if (timerThread_) {
        timerThread_->Stop();
        timerThread_.reset();
    }

    if (audioThread_) {
        AudioCommand cmd;
        cmd.type = AudioCommand::Type::Quit;
        PostAudioCommand(cmd);
        audioThread_->Stop();
        audioThread_.reset();
    }

    if (renderThread_) {
        renderThread_->Stop();
        renderThread_.reset();
    }

    OnClose();
    if (mainWindow_) {
        mainWindow_->Close();
    }

    hangulManager_.reset();
    HanMan = nullptr;
    resourceCache_.reset();

    SDL_Quit();
    running_ = false;
}

int JKApplication::Run() {
    if (!running_ || !sdlWindow_ || !mainWindow_) {
        return 1;
    }

    // The render thread owns SDL and pumps events; this thread only drains
    // the translated input/timer events from the bus and composes frames.
    while (running_) {
        // Drain timer channel.
        JKMessageBus::Payload timerPayload;
        while (messageBus_->Pop(JKMessageBus::Channel::Timer, timerPayload)) {
            if (!ProcessOneEvent(timerPayload.event)) {
                running_ = false;
                break;
            }
        }
        if (!running_) break;

        // Drain input channel. SizeChanged/DpiChanged come from the render
        // thread and must be applied immediately: during a cross-monitor move
        // the logical size and physical scale can change and hit-testing / the
        // main-window rectangle must stay in sync with SDL. A 200ms debounce
        // here left a stale rectangle that broke mouse input and redrawing.
        JKMessageBus::Payload inputPayload;
        while (messageBus_->Pop(JKMessageBus::Channel::Input, inputPayload)) {
            const auto evType = inputPayload.event.type;
            if (evType == JKEventType::SizeChanged || evType == JKEventType::DpiChanged) {
                // Apply immediately; no debounce for the state used by input.
                if (!ProcessOneEvent(inputPayload.event)) {
                    running_ = false;
                    break;
                }
                continue;
            }
            if (!ProcessOneEvent(inputPayload.event)) {
                running_ = false;
                break;
            }
        }
        if (!running_) break;

        // Cleanup closed child windows.
        if (mainWindow_) {
            mainWindow_->RemoveClosedChildren();
        }

        // Compose scene and send to render thread.
        ComposeScene();

        SDL_Delay(1);
    }

    return 0;
}

bool JKApplication::ProcessOneEvent(const JKEvent& ev) {
    if (ev.type == JKEventType::Quit) {
        return false;
    }

    if (ev.type == JKEventType::SizeChanged) {
        // Ignore zero-size events; they occasionally appear during window
        // creation or rapid monitor changes and would collapse hit-testing.
        if (ev.x <= 0 || ev.y <= 0) {
            if (mouseLog_) {
                std::fprintf(mouseLog_,
                    "[SizeChanged] ignored zero/bogus size %dx%d\n", ev.x, ev.y);
                std::fflush(mouseLog_);
            }
            return true;
        }
        // Update shared logical size used by input translation.
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            logicalWidth_ = ev.x;
            logicalHeight_ = ev.y;
        }
        // Keep the main window aligned with the SDL window's logical size so
        // the desktop background and child windows fill the output correctly
        // after a cross-monitor move or resize.
        if (mainWindow_) {
            mainWindow_->SetWindowRect(JKRect{ 0, 0, ev.x, ev.y });
            mainWindow_->Invalidate();
        }
        if (mouseLog_) {
            std::fprintf(mouseLog_,
                "[SizeChanged] logical=%dx%d mainRect=(%d,%d %dx%d)\n",
                ev.x, ev.y,
                mainWindow_ ? mainWindow_->GetRect().x : 0,
                mainWindow_ ? mainWindow_->GetRect().y : 0,
                mainWindow_ ? mainWindow_->GetRect().w : 0,
                mainWindow_ ? mainWindow_->GetRect().h : 0);
            std::fflush(mouseLog_);
        }
    }

    if (ev.type == JKEventType::DpiChanged) {
        // The render thread reports the physical output size. Recompute the
        // logical-to-physical scale used by mouse coordinate translation.
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (logicalWidth_ > 0 && logicalHeight_ > 0) {
            scaleX_ = ev.x / static_cast<float>(logicalWidth_);
            scaleY_ = ev.y / static_cast<float>(logicalHeight_);
        }
        letterboxX_ = 0;
        letterboxY_ = 0;
        if (mouseLog_) {
            std::fprintf(mouseLog_,
                "[DpiChanged] physical=%dx%d logical=%dx%d scale=(%.3f,%.3f)\n",
                ev.x, ev.y, logicalWidth_, logicalHeight_, scaleX_, scaleY_);
            std::fflush(mouseLog_);
        }
    }

    if (!PreProcessMessage(ev)) {
        return false;
    }

    JKEvent routedEv = ev;
    ApplyInputRouting(routedEv);

    if (routedEv.type == JKEventType::KeyDown && routedEv.keyCode == SDLK_TAB) {
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

void JKApplication::SetMainWindow(std::unique_ptr<JKWindow> window) {
    mainWindow_ = std::move(window);
}

JKWindow* JKApplication::GetMainWindow() const {
    return mainWindow_.get();
}

void JKApplication::SetModalWindow(JKWindow* window) {
    windowManager_->SetModalWindow(window);
}

JKWindow* JKApplication::GetModalWindow() const {
    return windowManager_ ? windowManager_->GetModalWindow() : nullptr;
}

void JKApplication::SetCapture(JKControl* control) {
    windowManager_->SetCapture(control);
}

void JKApplication::ReleaseCapture() {
    windowManager_->ReleaseCapture();
}

JKControl* JKApplication::GetCapture() const {
    return windowManager_ ? windowManager_->GetCapture() : nullptr;
}

void JKApplication::SetInputWindow(JKWindow* window) {
    windowManager_->SetInputWindow(window);
}

JKWindow* JKApplication::GetInputWindow() const {
    return windowManager_ ? windowManager_->GetInputWindow() : nullptr;
}

void JKApplication::SetTimerInterval(uint32_t ms) {
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

uint64_t JKApplication::AddTimer(uint32_t winId, uint32_t intervalMs, bool repeat) {
    if (timerThread_) {
        return timerThread_->AddTimer(winId, intervalMs, repeat);
    }
    return 0;
}

void JKApplication::RemoveTimer(uint64_t handle) {
    if (timerThread_) timerThread_->RemoveTimer(handle);
}

void JKApplication::RemoveTimersForWindow(uint32_t winId) {
    if (timerThread_) timerThread_->RemoveTimersForWindow(winId);
}

void JKApplication::PostAudioCommand(const AudioCommand& cmd) {
    if (!messageBus_) return;
    std::vector<uint8_t> data(sizeof(AudioCommand));
    std::memcpy(data.data(), &cmd, sizeof(AudioCommand));
    messageBus_->Push(JKMessageBus::Channel::Audio,
        JKMessageBus::Payload(static_cast<uint32_t>(cmd.type), std::move(data)));
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

void JKApplication::ComposeScene() {
    if (!mainWindow_ || !renderThread_ || !messageBus_) {
        return;
    }

    // Capture the window hierarchy into a serialized render command list on the
    // application thread. The render thread will replay these commands on the
    // real SDL backend.
    auto cmdList = std::make_unique<JKRenderCommandList>();
    JKDC dc(cmdList.get());
    dc.SetHangulManager(hangulManager_.get());

    // Desktop background.
    dc.SetColor(192, 192, 192, 255);
    dc.Clear();

    // Paint the main window tree and any modal window on top.
    mainWindow_->PaintWindow(dc);
    mainWindow_->PaintClient(dc);

    JKWindow* modal = GetModalWindow();
    if (modal) {
        modal->PaintWindow(dc);
        modal->PaintClient(dc);
    }

    // We are doing a full-frame capture, so discard accumulated dirty regions
    // to keep the vector from growing unbounded.
    mainWindow_->ClearDirtyRects();
    if (modal) {
        modal->ClearDirtyRects();
    }

    auto sceneData = cmdList->Serialize();
    messageBus_->Push(JKMessageBus::Channel::Render,
        JKMessageBus::Payload(1, std::move(sceneData)));
}

bool JKApplication::PreProcessMessage(const JKEvent& ev) {
    (void)ev;
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

void JKApplication::ApplyInputRouting(JKEvent& ev) {
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

            if (mouseLog_) {
                std::fprintf(mouseLog_,
                    "[Route] type=%d logical=(%d,%d) target=%u ctrl=%u active='%s'\n",
                    static_cast<int>(ev.type), ev.x, ev.y,
                    ev.targetId, ev.controlId,
                    active->GetTitle().c_str());
                std::fflush(mouseLog_);
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
}

void JKApplication::SetLogicalSize(int w, int h) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    logicalWidth_ = w;
    logicalHeight_ = h;
}

void JKApplication::GetLogicalSize(int& w, int& h) const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    w = logicalWidth_;
    h = logicalHeight_;
}

void JKApplication::GetScale(float& sx, float& sy) const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    sx = scaleX_;
    sy = scaleY_;
}

void JKApplication::GetLetterbox(int& x, int& y) const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    x = letterboxX_;
    y = letterboxY_;
}

} // namespace jk
