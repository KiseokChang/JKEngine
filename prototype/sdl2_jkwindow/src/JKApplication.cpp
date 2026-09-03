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
#include <chrono>
#include <typeinfo>

namespace {

const char* SdlWindowEventName(uint8_t e) {
    switch (e) {
        case SDL_WINDOWEVENT_NONE: return "NONE";
        case SDL_WINDOWEVENT_SHOWN: return "SHOWN";
        case SDL_WINDOWEVENT_HIDDEN: return "HIDDEN";
        case SDL_WINDOWEVENT_EXPOSED: return "EXPOSED";
        case SDL_WINDOWEVENT_MOVED: return "MOVED";
        case SDL_WINDOWEVENT_RESIZED: return "RESIZED";
        case SDL_WINDOWEVENT_SIZE_CHANGED: return "SIZE_CHANGED";
        case SDL_WINDOWEVENT_MINIMIZED: return "MINIMIZED";
        case SDL_WINDOWEVENT_MAXIMIZED: return "MAXIMIZED";
        case SDL_WINDOWEVENT_RESTORED: return "RESTORED";
        case SDL_WINDOWEVENT_ENTER: return "ENTER";
        case SDL_WINDOWEVENT_LEAVE: return "LEAVE";
        case SDL_WINDOWEVENT_FOCUS_GAINED: return "FOCUS_GAINED";
        case SDL_WINDOWEVENT_FOCUS_LOST: return "FOCUS_LOST";
        case SDL_WINDOWEVENT_CLOSE: return "CLOSE";
#if SDL_VERSION_ATLEAST(2, 0, 5)
        case SDL_WINDOWEVENT_TAKE_FOCUS: return "TAKE_FOCUS";
#endif
#if SDL_VERSION_ATLEAST(2, 0, 16)
        case SDL_WINDOWEVENT_HIT_TEST: return "HIT_TEST";
        case SDL_WINDOWEVENT_DISPLAY_CHANGED: return "DISPLAY_CHANGED";
#endif
        default: return "UNKNOWN";
    }
}

} // anonymous namespace

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
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
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

    sdlWindow_ = renderThread_->GetWindow();

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

    // Initial logical size comes from the render thread's created window.
    renderThread_->GetLogicalSize(logicalWidth_, logicalHeight_);

    // Compute the initial DPI scale before the render thread starts so the
    // first mouse events are translated with the correct scale factor.
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
    EnsureSdlFocus();

    mainWindow_->Init();
    mainWindow_->Setup();
    mainWindow_->Open();
    mainWindow_->FocusFirstChild();

    renderThread_->Start(messageBus_.get());

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

void JKApplication::PumpInputEvents() {
    SDL_Event sdlEvent;
    while (SDL_PollEvent(&sdlEvent)) {
        if (mouseLog_) {
            std::fprintf(mouseLog_, "[SDL-EV] type=%d", sdlEvent.type);
            if (sdlEvent.type == SDL_WINDOWEVENT && sdlWindow_) {
                const Uint32 flags = SDL_GetWindowFlags(sdlWindow_);
                std::fprintf(mouseLog_,
                    " win=%s data1=%d data2=%d flags=0x%x mouseFocus=%d inputFocus=%d",
                    SdlWindowEventName(sdlEvent.window.event),
                    sdlEvent.window.data1, sdlEvent.window.data2,
                    flags,
                    (flags & SDL_WINDOW_MOUSE_FOCUS) ? 1 : 0,
                    (flags & SDL_WINDOW_INPUT_FOCUS) ? 1 : 0);
            }
            std::fprintf(mouseLog_, "\n");
            std::fflush(mouseLog_);
        }

        // Force a full redraw when the OS exposes the window after a move,
        // resize, or monitor change. SDL does not always re-issue mouse/keyboard
        // events in these cases, and the renderer backbuffer may have been
        // recreated. Also respond to TAKE_FOCUS so Windows actually gives us
        // input focus after cross-monitor moves.
        if (sdlEvent.type == SDL_WINDOWEVENT &&
            (sdlEvent.window.event == SDL_WINDOWEVENT_EXPOSED ||
             sdlEvent.window.event == SDL_WINDOWEVENT_SHOWN ||
             sdlEvent.window.event == SDL_WINDOWEVENT_RESTORED ||
             sdlEvent.window.event == SDL_WINDOWEVENT_FOCUS_GAINED
#if SDL_VERSION_ATLEAST(2, 0, 5)
             || sdlEvent.window.event == SDL_WINDOWEVENT_TAKE_FOCUS
#endif
             )) {
            if (mainWindow_) {
                mainWindow_->Invalidate();
                // Ensure the window manager routes input to the main window when
                // the SDL window regains focus after a move or activation.
                windowManager_->SetInputWindow(mainWindow_.get());
                mainWindow_->FocusFirstChild();
            }
#if SDL_VERSION_ATLEAST(2, 0, 5)
            if (sdlEvent.window.event == SDL_WINDOWEVENT_TAKE_FOCUS) {
                EnsureSdlFocus();
            }
#endif
        }

        JKEvent ev = TranslateSDLEvent(sdlEvent);
        if (ev.type != JKEventType::None) {
            if (ev.type == JKEventType::Quit) {
                running_ = false;
                break;
            }
            messageBus_->Push(JKMessageBus::Channel::Input, ev);
        }
    }
}

int JKApplication::Run() {
    if (!running_ || !sdlWindow_ || !mainWindow_) {
        return 1;
    }

    // SDL events must be pumped on the thread that created the SDL window.
    // The main application thread is that thread, so input polling lives here.
    while (running_) {
        PumpInputEvents();

        // Drain timer channel.
        JKMessageBus::Payload timerPayload;
        while (messageBus_->Pop(JKMessageBus::Channel::Timer, timerPayload)) {
            if (!ProcessOneEvent(timerPayload.event)) {
                running_ = false;
                break;
            }
        }
        if (!running_) break;

        // Drain input channel. DpiChanged events from the render thread are
        // folded into the pending resize debouncer so rapid monitor moves do
        // not trigger repeated re-scaling.
        JKMessageBus::Payload inputPayload;
        while (messageBus_->Pop(JKMessageBus::Channel::Input, inputPayload)) {
            if (inputPayload.event.type == JKEventType::SizeChanged) {
                MergePendingResizeFromSizeEvent(inputPayload.event);
                continue;
            }
            if (inputPayload.event.type == JKEventType::DpiChanged) {
                MergePendingResizeFromDpiEvent(inputPayload.event);
                continue;
            }
            if (!ProcessOneEvent(inputPayload.event)) {
                running_ = false;
                break;
            }
        }
        if (!running_) break;

        // Once the window has stayed the same size/DPI for the debounce
        // window, emit a single synchronized SizeChanged + DpiChanged pair.
        FlushPendingResizeIfStable();

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

    if (ev.type == JKEventType::KeyDown && ev.keyCode == SDLK_TAB) {
        JKWindow* active = windowManager_->GetKeyboardTargetWindow();
        bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        if (active) {
            if (shift) active->FocusPrevChild();
            else       active->FocusNextChild();
        }
        return true;
    }

    RouteMessage(ev);
    return true;
}

void JKApplication::MergePendingResizeFromSizeEvent(const JKEvent& ev) {
    pendingResize_.dirty = true;
    pendingResize_.logicalW = ev.x;
    pendingResize_.logicalH = ev.y;
    pendingResize_.deadline =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(kResizeDebounceMs);
}

void JKApplication::MergePendingResizeFromDpiEvent(const JKEvent& ev) {
    pendingResize_.dirty = true;
    pendingResize_.physicalW = ev.x;
    pendingResize_.physicalH = ev.y;
    pendingResize_.deadline =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(kResizeDebounceMs);
}

void JKApplication::FlushPendingResizeIfStable() {
    if (!pendingResize_.dirty) return;
    if (std::chrono::steady_clock::now() < pendingResize_.deadline) return;

    JKEvent sizeEv;
    sizeEv.type = JKEventType::SizeChanged;
    sizeEv.x = pendingResize_.logicalW;
    sizeEv.y = pendingResize_.logicalH;
    messageBus_->Push(JKMessageBus::Channel::Input, sizeEv);

    if (pendingResize_.physicalW > 0 && pendingResize_.physicalH > 0) {
        JKEvent dpiEv;
        dpiEv.type = JKEventType::DpiChanged;
        dpiEv.x = pendingResize_.physicalW;
        dpiEv.y = pendingResize_.physicalH;
        messageBus_->Push(JKMessageBus::Channel::Input, dpiEv);
    }

    pendingResize_.dirty = false;
    pendingResize_.physicalW = 0;
    pendingResize_.physicalH = 0;
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

JKEvent JKApplication::TranslateSDLEvent(const SDL_Event& sdl) {
    JKEvent ev = jk::TranslateSDLEvent(sdl);

    if (!mainWindow_) {
        return ev;
    }

    if (ev.type == JKEventType::MouseMove ||
        ev.type == JKEventType::MouseDown ||
        ev.type == JKEventType::MouseUp) {
        // SDL reports mouse coordinates in the window's logical-point space when
        // SDL_HINT_WINDOWS_DPI_SCALING is enabled. We use the raw SDL event
        // coordinates for hit-testing. Log the independently computed global-state
        // coordinate as a diagnostic so we can detect divergence.
        int globalX = 0, globalY = 0;
        int winPosX = 0, winPosY = 0;
        SDL_GetGlobalMouseState(&globalX, &globalY);
        SDL_GetWindowPosition(sdlWindow_, &winPosX, &winPosY);
        const int globalRelX = globalX - winPosX;
        const int globalRelY = globalY - winPosY;

        if (mouseLog_) {
            SDL_Window* mouseFocus = SDL_GetMouseFocus();
            SDL_Window* keyFocus = SDL_GetKeyboardFocus();
            std::fprintf(mouseLog_,
                "[TRSDL] type=%d raw=(%d,%d) globalRel=(%d,%d) d=(%d,%d) "
                "mouseFocus=%d keyFocus=%d\n",
                static_cast<int>(ev.type), ev.x, ev.y,
                globalRelX, globalRelY, ev.dx, ev.dy,
                mouseFocus == sdlWindow_ ? 1 : 0,
                keyFocus == sdlWindow_ ? 1 : 0);
            std::fflush(mouseLog_);
        }

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
            if (mouseLog_) {
                std::fprintf(mouseLog_,
                    "[TRSDL] target=%s id=%u ctrl=%u active='%s'\n",
                    target ? typeid(*target).name() : "(window)",
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

    return ev;
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

void JKApplication::EnsureSdlFocus() {
    if (!sdlWindow_) return;
    // Ask Windows to activate the SDL window, then tell SDL to take input focus.
    // This is especially important on startup and after cross-monitor moves where
    // Windows sometimes leaves the SDL window without focus even though it is
    // visible.
    JKPlatform::ActivateWindow(sdlWindow_);
    SDL_RaiseWindow(sdlWindow_);
    SDL_SetWindowInputFocus(sdlWindow_);
}

} // namespace jk
