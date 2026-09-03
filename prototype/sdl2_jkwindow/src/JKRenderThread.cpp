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

void JKRenderThread::Run() {
    int lastPhysW = 0, lastPhysH = 0;
    int lastLogW = 0, lastLogH = 0;

    while (running_) {
        JKMessageBus::Payload payload;
        bool hasPayload = bus_ && bus_->PopWait(JKMessageBus::Channel::Render, payload, 16);

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
