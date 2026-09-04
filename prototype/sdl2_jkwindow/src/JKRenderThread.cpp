#include <JKRenderThread.h>
#include <JKSDLRenderBackend.h>
#include <JKRenderCommandList.h>
#include <JKResourceCache.h>
#include <JKEvent.h>
#include <JKPlatform.h>
#include <cstdio>

namespace jk {

JKRenderThread::JKRenderThread() = default;

JKRenderThread::~JKRenderThread() {
    Stop();
}

bool JKRenderThread::Init(const std::string& title, int width, int height) {
    initTitle_ = title;
    initW_ = width;
    initH_ = height;
    return true;
}

bool JKRenderThread::WaitInit() {
    std::unique_lock<std::mutex> lock(initMutex_);
    initCond_.wait(lock, [this] { return initDone_; });
    return initSuccess_;
}

bool JKRenderThread::CreateSdlWindowAndRenderer() {
#ifdef _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
#endif

    int createW = initW_;
    int createH = initH_;
    SDL_Rect usable{};
    const bool hasUsable =
        SDL_GetDisplayUsableBounds(0, &usable) == 0 && usable.w > 0 && usable.h > 0;
    if (hasUsable) {
        constexpr int kEstSide = 4;
        constexpr int kEstTop = 31;
        constexpr int kEstBottom = 4;
        const int maxClientW = usable.w - kEstSide * 2;
        const int maxClientH = usable.h - kEstTop - kEstBottom;
        if (createW > maxClientW && maxClientW > 320) createW = maxClientW;
        if (createH > maxClientH && maxClientH > 240) createH = maxClientH;
    }

    window_ = SDL_CreateWindow(
        initTitle_.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        createW,
        createH,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!window_) {
        std::fprintf(stderr, "JKRenderThread::CreateSdlWindowAndRenderer: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    sdlRenderer_ = SDL_CreateRenderer(window_, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdlRenderer_) {
        std::fprintf(stderr, "JKRenderThread::CreateSdlWindowAndRenderer: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        return false;
    }

    renderBackend_ = std::make_unique<JKSDLRenderBackend>(sdlRenderer_);

    int actualW = initW_;
    int actualH = initH_;
    SDL_GetWindowSize(window_, &actualW, &actualH);
    logicalWidth_ = actualW;
    logicalHeight_ = actualH;

    // Center window using platform helper.
    JKPlatform::Placement placement{};
    if (JKPlatform::ComputeCenteredPlacement(window_, logicalWidth_, logicalHeight_, placement)) {
        SDL_SetWindowPosition(window_, placement.clientPtX, placement.clientPtY);
    }

    return true;
}

void JKRenderThread::GetLogicalSize(int& w, int& h) const {
    w = logicalWidth_;
    h = logicalHeight_;
}

void JKRenderThread::Start(JKMessageBus* bus) {
    if (running_ || thread_.joinable()) return;
    bus_ = bus;
    running_ = true;
    thread_ = std::thread(&JKRenderThread::Run, this);
}

void JKRenderThread::Stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    if (resourceCache_ && renderBackend_) {
        resourceCache_->UnloadAllImages();
        resourceCache_->FlushUploads(renderBackend_.get());
    }
    renderBackend_.reset();
    if (sdlRenderer_) {
        SDL_DestroyRenderer(sdlRenderer_);
        sdlRenderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    bus_ = nullptr;
}

void JKRenderThread::PollSdlEvents() {
    SDL_Event sdl;
    while (SDL_PollEvent(&sdl)) {
        // Keep input focus after moves, monitor changes, or exposure events
        // where Windows may not automatically restore focus.
        if (sdl.type == SDL_WINDOWEVENT) {
            const uint8_t e = sdl.window.event;
            if (e == SDL_WINDOWEVENT_EXPOSED ||
                e == SDL_WINDOWEVENT_SHOWN ||
                e == SDL_WINDOWEVENT_RESTORED ||
                e == SDL_WINDOWEVENT_FOCUS_GAINED ||
                e == SDL_WINDOWEVENT_MOVED
#if SDL_VERSION_ATLEAST(2, 0, 5)
                || e == SDL_WINDOWEVENT_TAKE_FOCUS
#endif
#if SDL_VERSION_ATLEAST(2, 0, 16)
                || e == SDL_WINDOWEVENT_DISPLAY_CHANGED
#endif
                ) {
                EnsureWindowFocus();
            }
        }

        JKEvent ev = jk::TranslateSDLEvent(sdl);
        if (ev.type == JKEventType::None) {
            continue;
        }

        // On Windows, replace SDL's logical mouse coordinates with the value
        // derived from the actual monitor DPI. SDL's cached scale can drift
        // during cross-monitor moves.
        if ((ev.type == JKEventType::MouseMove ||
             ev.type == JKEventType::MouseDown ||
             ev.type == JKEventType::MouseUp) && window_) {
#ifdef _WIN32
            int logicalX = ev.x;
            int logicalY = ev.y;
            if (JKPlatform::GetLogicalMousePos(window_, logicalX, logicalY)) {
                ev.x = logicalX;
                ev.y = logicalY;
            }
#endif
        }

        if (bus_) {
            bus_->Push(JKMessageBus::Channel::Input, ev);
        }
    }
}

void JKRenderThread::EnsureWindowFocus() {
    if (!window_) {
        return;
    }
    // Use only Win32 activate + SDL_RaiseWindow. SDL_SetWindowInputFocus
    // can report "operation not supported" on some Windows/SDL builds.
    JKPlatform::ActivateWindow(window_);
    SDL_RaiseWindow(window_);
}

void JKRenderThread::Run() {
    // Create the SDL window and renderer on the render thread so that every
    // SDL_Window and SDL_Renderer call (including SDL_PollEvent) happens on
    // the same thread. This satisfies SDL's thread-affinity requirements.
    const bool created = CreateSdlWindowAndRenderer();
    {
        std::lock_guard<std::mutex> lock(initMutex_);
        initSuccess_ = created;
        initDone_ = true;
    }
    initCond_.notify_one();

    if (!created) {
        running_ = false;
        return;
    }

    // Make sure the newly created window has focus.
    EnsureWindowFocus();

    int lastPhysW = 0, lastPhysH = 0;
    int lastLogW = 0, lastLogH = 0;

    while (running_) {
        // The render thread both pumps SDL events and performs rendering.
        // SDL events must be pumped on the thread that owns the SDL window.
        PollSdlEvents();

        JKMessageBus::Payload payload;
        bool hasPayload = bus_ && bus_->PopWait(JKMessageBus::Channel::Render, payload, 5);

        // The application thread may generate frames faster than the monitor
        // refresh rate. Always render the most recent scene and drop stale ones
        // to avoid an ever-growing queue.
        if (hasPayload && bus_) {
            JKMessageBus::Payload newer;
            while (bus_->Pop(JKMessageBus::Channel::Render, newer)) {
                payload = std::move(newer);
            }
        }

        if (hasPayload && renderBackend_) {
            // Upload any textures queued from the application thread before
            // replaying this frame. All SDL renderer access stays on this thread.
            if (resourceCache_) {
                resourceCache_->FlushUploads(renderBackend_.get());
            }

            // Read the current logical point size from SDL (render-thread only).
            int logW = 0, logH = 0;
            SDL_GetWindowSize(window_, &logW, &logH);
            logicalWidth_ = logW;
            logicalHeight_ = logH;

            int physW = 0, physH = 0;
            renderBackend_->GetOutputSize(physW, physH);

            // Notify the application thread whenever the logical or physical
            // size changes. The application thread should not call SDL window
            // functions directly, so we ship both values from the render thread.
            // Skip bogus zero-size values that can appear during creation or
            // monitor transitions.
            if (bus_ && logW > 0 && logH > 0 && physW > 0 && physH > 0 &&
                (physW != lastPhysW || physH != lastPhysH ||
                 logW != lastLogW || logH != lastLogH)) {
                lastPhysW = physW;
                lastPhysH = physH;
                lastLogW = logW;
                lastLogH = logH;

                JKEvent sizeEv;
                sizeEv.type = JKEventType::SizeChanged;
                sizeEv.x = logW;
                sizeEv.y = logH;
                bus_->Push(JKMessageBus::Channel::Input, sizeEv);

                JKEvent dpiEv;
                dpiEv.type = JKEventType::DpiChanged;
                dpiEv.x = physW;
                dpiEv.y = physH;
                bus_->Push(JKMessageBus::Channel::Input, dpiEv);
            }

            // Scale logical-point drawing commands to physical pixels.
            float sx = 1.0f, sy = 1.0f;
            if (logW > 0 && logH > 0 && physW > 0 && physH > 0) {
                sx = physW / static_cast<float>(logW);
                sy = physH / static_cast<float>(logH);
            }

            renderBackend_->SetRenderTarget(nullptr);
            renderBackend_->SetScale(sx, sy);

            auto scene = JKRenderCommandList::Deserialize(payload.data);
            if (scene) {
                scene->Replay(renderBackend_.get());
            }

            renderBackend_->Present();

            // After Present, check whether SDL logged a renderer/D3D error.
            // This helps diagnose "black screen / no updates" after cross-monitor
            // moves where the D3D device can be lost.
            const char* err = SDL_GetError();
            if (err && err[0] != '\0') {
                std::fprintf(stderr, "[RenderThread] SDL error after Present: %s\n", err);
                SDL_ClearError();
            }
        }
    }
}

} // namespace jk
