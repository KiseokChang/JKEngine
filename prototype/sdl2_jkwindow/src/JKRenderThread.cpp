#include <JKRenderThread.h>
#include <JKSDLRenderBackend.h>
#include <JKPlatform.h>
#include <cstdio>

namespace jk {

JKRenderThread::JKRenderThread() = default;

JKRenderThread::~JKRenderThread() {
    Stop();
}

bool JKRenderThread::Init(const std::string& title, int width, int height) {
#ifdef _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
#endif

    int createW = width;
    int createH = height;
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
        title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        createW,
        createH,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!window_) {
        std::fprintf(stderr, "JKRenderThread::Init: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    sdlRenderer_ = SDL_CreateRenderer(window_, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdlRenderer_) {
        std::fprintf(stderr, "JKRenderThread::Init: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        return false;
    }

    renderBackend_ = std::make_unique<JKSDLRenderBackend>(sdlRenderer_);

    int actualW = width;
    int actualH = height;
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
    // For now these values are written only from the render thread during Init.
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

void JKRenderThread::Run() {
    while (running_) {
        JKMessageBus::Payload payload;
        bool hasPayload = bus_ && bus_->PopWait(JKMessageBus::Channel::Render, payload, 16);

        // Handle window resize events on the render thread.
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            // Discard here; input events are handled by the input thread.
            // Window resize/DPI events are handled separately if needed.
            if (ev.type == SDL_WINDOWEVENT) {
                if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    logicalWidth_ = ev.window.data1;
                    logicalHeight_ = ev.window.data2;
                }
            }
        }

        if (hasPayload) {
            // Scene rendering will be implemented by JKApplication posting
            // serialized draw commands. For now we just clear the screen to
            // show the thread is alive.
            if (renderBackend_) {
                renderBackend_->SetRenderTarget(nullptr);
                renderBackend_->SetScale(1.0f, 1.0f);
                renderBackend_->SetDrawColor(192, 192, 192, 255);
                renderBackend_->Clear();
                renderBackend_->Present();
            }
        }
    }
}

} // namespace jk
