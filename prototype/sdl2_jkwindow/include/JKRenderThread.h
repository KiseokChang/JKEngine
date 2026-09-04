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
#include <mutex>
#include <condition_variable>

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

    // Store the desired window title and size. Actual SDL window/renderer
    // creation happens on the render thread inside Start()/Run().
    bool Init(const std::string& title, int width, int height);

    // Start the render loop. This launches the render thread, which creates
    // the SDL window and renderer and then begins pumping SDL events and
    // rendering incoming scenes. WaitInit() must be called before accessing
    // GetWindow()/GetRenderer()/GetBackend().
    void Start(JKMessageBus* bus);
    void Stop();

    // Block until the render thread has finished creating the SDL window and
    // renderer. Returns true on success. Safe to call from the application thread.
    bool WaitInit();

    SDL_Window* GetWindow() const { return window_; }
    SDL_Renderer* GetRenderer() const { return sdlRenderer_; }
    JKRenderBackend* GetBackend() const { return renderBackend_.get(); }

    // The resource cache uploads textures on this thread.
    void SetResourceCache(JKResourceCache* cache) { resourceCache_ = cache; }

    // Thread-safe query of the current logical window size.
    void GetLogicalSize(int& w, int& h) const;

private:
    std::string initTitle_;
    int initW_ = 0;
    int initH_ = 0;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdlRenderer_ = nullptr;
    std::unique_ptr<JKRenderBackend> renderBackend_;

    JKMessageBus* bus_ = nullptr;
    std::atomic<bool> running_{false};
    std::thread thread_;

    int logicalWidth_ = 0;
    int logicalHeight_ = 0;

    JKResourceCache* resourceCache_ = nullptr;

    // Synchronization for creation on the render thread.
    std::mutex initMutex_;
    std::condition_variable initCond_;
    bool initDone_ = false;
    bool initSuccess_ = false;

    void Run();
    bool CreateSdlWindowAndRenderer();
    void PollSdlEvents();
    void EnsureWindowFocus();
};

} // namespace jk

#endif // JKRENDERTHREAD_H
