#include <client/JKClientApplication.h>

#include <JKSDLRenderBackend.h>
#include <JKRenderCommandList.h>
#include <JKOffscreenSurface.h>
#include <JKTimerThread.h>
#include <JKAudioThread.h>
#include <JKSDLAudioBackend.h>
#include <JKSoundManager.h>
#include <JKPlatform.h>
#include <cstdio>
#include <cstring>

namespace jk {

JKClientApplication::JKClientApplication() : dc_(nullptr) {
    windowManager_ = std::make_unique<JKWindowManager>();
    messageBus_ = std::make_unique<JKMessageBus>();
}

JKClientApplication::~JKClientApplication() {
    Close();
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
    if (!CreateHiddenRenderer(title, width, height)) {
        return false;
    }

    dc_ = JKDC(renderBackend_.get());
    resourceCache_ = std::make_unique<JKResourceCache>(renderBackend_.get());

    audioThread_ = std::make_unique<JKAudioThread>();
    audioThread_->Start(messageBus_.get(), std::make_unique<SDLAudioBackend>());

    {
        AudioCommand initCmd;
        initCmd.type = AudioCommand::Type::Init;
        PostAudioCommand(initCmd);

        auto& soundManager = JKSoundManager::GetInstance();
        soundManager.SetCommandMode(true);
        soundManager.Init();
    }

    surface_ = std::make_unique<jk::client::JKClientSurface>(pipeName, width, height, title);
    if (!surface_->Connect()) {
        std::fprintf(stderr, "JKClientApplication::Init: failed to connect to server\n");
        Close();
        return false;
    }

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

    if (audioThread_) {
        AudioCommand cmd;
        cmd.type = AudioCommand::Type::Quit;
        PostAudioCommand(cmd);
        audioThread_->Stop();
        audioThread_.reset();
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
    resourceCache_.reset();

    DestroyHiddenRenderer();
    SDL_Quit();
}

int JKClientApplication::Run() {
    if (!running_ || !surface_ || !surface_->IsValid() || !mainWindow_) {
        return 1;
    }

    while (running_) {
        DrainTimerChannel();
        if (!running_) break;

        DrainInputChannel();
        if (!running_) break;

        if (mainWindow_) {
            mainWindow_->RemoveClosedChildren();
        }

        RenderAndCommit();

        SDL_Delay(1);
    }

    return 0;
}

void JKClientApplication::DrainTimerChannel() {
    JKMessageBus::Payload timerPayload;
    while (messageBus_->Pop(JKMessageBus::Channel::Timer, timerPayload)) {
        if (!ProcessOneEvent(timerPayload.event)) {
            running_ = false;
            break;
        }
    }
}

void JKClientApplication::DrainInputChannel() {
    // Input events arrive from the server via JKClientSurface, not the message bus.
    JKEvent ev;
    while (surface_ && surface_->PollInputEvent(ev)) {
        if (ev.type == JKEventType::Quit) {
            running_ = false;
            break;
        }
        if (!ProcessOneEvent(ev)) {
            running_ = false;
            break;
        }
    }
}

bool JKClientApplication::ProcessOneEvent(const JKEvent& ev) {
    if (ev.type == JKEventType::Quit) {
        return false;
    }

    if (ev.type == JKEventType::SizeChanged) {
        if (ev.x <= 0 || ev.y <= 0) {
            return true;
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
    if (!messageBus_) return;
    std::vector<uint8_t> data(sizeof(AudioCommand));
    std::memcpy(data.data(), &cmd, sizeof(AudioCommand));
    messageBus_->Push(JKMessageBus::Channel::Audio,
        JKMessageBus::Payload(static_cast<uint32_t>(cmd.type), std::move(data)));
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

    dc.SetColor(192, 192, 192, 255);
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

    // Replay the serialized scene into the target texture.
    if (!pendingScene_.empty()) {
        auto scene = JKRenderCommandList::Deserialize(pendingScene_);
        if (scene) {
            scene->Replay(renderBackend_.get());
        }
    }

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

    renderBackend_->SetRenderTarget(nullptr);

    // Copy to shared memory and commit.
    uint8_t* dest = surface_->Pixels();
    if (dest) {
        std::memcpy(dest, pixelBuffer_.data(), needed);
        surface_->CommitFull();
    }
}

} // namespace jk
