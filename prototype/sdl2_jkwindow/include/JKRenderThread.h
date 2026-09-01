#ifndef JKRENDERTHREAD_H
#define JKRENDERTHREAD_H

#include <JKMessageBus.h>
#include <JKRenderBackend.h>
#include <SDL.h>
#include <string>
#include <atomic>
#include <thread>
#include <memory>
#include <cstdint>

namespace jk {
class JKResourceCache;
}

namespace jk {

// Render thread owns the SDL window, SDL renderer, and JKRenderBackend.
// It receives render scene payloads from the application thread via the
// message bus Render channel and performs all SDL rendering on this thread.
class JKRenderThread {
public:
    JKRenderThread();
    ~JKRenderThread();

    // Create the SDL window and renderer. Must be called from the render thread.
    bool Init(const std::string& title, int width, int height);

    // Start the render loop. The application thread posts scene payloads via
    // the message bus.
    void Start(JKMessageBus* bus);
    void Stop();

    SDL_Window* GetWindow() const { return window_; }
    SDL_Renderer* GetRenderer() const { return sdlRenderer_; }
    JKRenderBackend* GetBackend() const { return renderBackend_.get(); }

    // The resource cache uploads textures on this thread.
    void SetResourceCache(JKResourceCache* cache) { resourceCache_ = cache; }

    // Thread-safe query of the current logical window size.
    void GetLogicalSize(int& w, int& h) const;

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdlRenderer_ = nullptr;
    std::unique_ptr<JKRenderBackend> renderBackend_;

    JKMessageBus* bus_ = nullptr;
    std::atomic<bool> running_{false};
    std::thread thread_;

    int logicalWidth_ = 0;
    int logicalHeight_ = 0;

    JKResourceCache* resourceCache_ = nullptr;

    void Run();
};

} // namespace jk

#endif // JKRENDERTHREAD_H
