#ifndef JKAPPLICATION_H
#define JKAPPLICATION_H

#include <JKWindow.h>
#include <JKMessageBus.h>
#include <JKAudioCommand.h>
#include <JKHangulManager.h>
#include <JKDC.h>
#include <JKRenderBackend.h>
#include <JKResourceCache.h>
#include <JKWindowManager.h>
#include <SDL.h>
#include <memory>
#include <string>
#include <mutex>
#include <chrono>

namespace jk {

class JKRenderThread;
class JKTimerThread;
class JKAudioThread;

class JKApplication {
public:
    JKApplication();
    virtual ~JKApplication();

    bool Init(const std::string& title, int width, int height);
    void Close();
    int Run();

    void SetMainWindow(std::unique_ptr<JKWindow> window);
    JKWindow* GetMainWindow() const;
    SDL_Window* GetSdlWindow() const { return sdlWindow_; }

    void SetModalWindow(JKWindow* window);
    JKWindow* GetModalWindow() const;

    void SetCapture(JKControl* control);
    void ReleaseCapture();
    JKControl* GetCapture() const;

    void SetInputWindow(JKWindow* window);
    JKWindow* GetInputWindow() const;

    JKControl* FindControlById(uint32_t winId);
    JKControl* FindControlByControlId(uint16_t controlId);
    JKWindow*  FindWindowById(uint32_t winId);

    JKResourceCache* GetResourceCache() const { return resourceCache_.get(); }
    JKWindowManager* GetWindowManager() const { return windowManager_.get(); }

    // Legacy single timer interval. Prefer AddTimer/RemoveTimer for per-window
    // or per-control timers. SetTimerInterval configures the implicit global
    // timer (winId 0) for backward compatibility.
    void SetTimerInterval(uint32_t ms);

    // Register a repeating or one-shot timer. Returns a handle that can be passed
    // to RemoveTimer. When fired, a JKEventType::Timer event is posted with the
    // given winId as the target.
    uint64_t AddTimer(uint32_t winId, uint32_t intervalMs, bool repeat);
    void RemoveTimer(uint64_t handle);
    void RemoveTimersForWindow(uint32_t winId);

    // Post an audio command to the audio thread. Used by JKSoundManager.
    void PostAudioCommand(const AudioCommand& cmd);

    // Shared state accessors used by input thread / render thread.
    void SetLogicalSize(int w, int h);
    void GetLogicalSize(int& w, int& h) const;
    void GetScale(float& sx, float& sy) const;
    void GetLetterbox(int& x, int& y) const;

#ifdef _WIN32
    FILE* GetMouseLog() const { return mouseLog_; }
#endif

protected:
    virtual void OnInit();
    virtual void OnClose();
    virtual bool PreProcessMessage(const JKEvent& ev);
    virtual void RouteMessage(const JKEvent& ev);

    // Called on the application thread when a scene should be produced and sent
    // to the render thread. In Phase 1 this is a placeholder; Phase 1.5 adds
    // serialized render commands.
    virtual void ComposeScene();

    JKEvent TranslateSDLEvent(const SDL_Event& sdl);

private:
    SDL_Window* sdlWindow_ = nullptr;

    std::unique_ptr<JKWindow> mainWindow_;
    std::unique_ptr<JKWindowManager> windowManager_;
    std::unique_ptr<JKMessageBus> messageBus_;
    JKDC dc_;
    std::unique_ptr<HangulManager> hangulManager_;
    std::unique_ptr<JKResourceCache> resourceCache_;
    bool running_ = false;

    std::unique_ptr<JKRenderThread> renderThread_;
    std::unique_ptr<JKTimerThread> timerThread_;
    std::unique_ptr<JKAudioThread> audioThread_;

    // Shared mutable state protected by a single mutex.
    mutable std::mutex stateMutex_;
    int logicalWidth_ = 0;
    int logicalHeight_ = 0;
    float scaleX_ = 1.0f;
    float scaleY_ = 1.0f;
    int letterboxX_ = 0;
    int letterboxY_ = 0;

    // Legacy timer state passed to the timer thread.
    uint32_t legacyTimerWinId_ = 0;
    uint32_t legacyTimerInterval_ = 0;

    // Debounced resize/DPI handling. SDL fires many SIZE_CHANGED/MOVED/
    // DISPLAY_CHANGED events when a window crosses monitor boundaries, and the
    // render thread sends DpiChanged at the same time. We collect the latest
    // values and emit a single SizeChanged/DpiChanged pair after a short quiet
    // period to avoid re-layout storms.
    struct PendingResize {
        bool dirty = false;
        int logicalW = 0;
        int logicalH = 0;
        int physicalW = 0;
        int physicalH = 0;
        std::chrono::steady_clock::time_point deadline;
    } pendingResize_;
    static constexpr int kResizeDebounceMs = 200;

#ifdef _WIN32
    int lastMousePhysX_ = 0;
    int lastMousePhysY_ = 0;
    bool hasLastMousePhys_ = false;
    FILE* mouseLog_ = nullptr;
#endif

    void PumpInputEvents();
    bool ProcessOneEvent(const JKEvent& ev);
    void MergePendingResizeFromSDL();
    void MergePendingResizeFromDpiEvent(const JKEvent& ev);
    void FlushPendingResizeIfStable();
};

extern JKApplication* g_currentJKApp;

} // namespace jk

#endif // JKAPPLICATION_H
