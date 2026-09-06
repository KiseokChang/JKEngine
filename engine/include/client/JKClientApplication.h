#ifndef JKCLIENTAPPLICATION_H
#define JKCLIENTAPPLICATION_H

#include <JKWindow.h>
#include <JKMessageBus.h>
#include <JKAudioCommand.h>
#include <JKApplicationHost.h>
#include <JKHangulManager.h>
#include <JKDC.h>
#include <JKRenderBackend.h>
#include <JKSDLRenderBackend.h>
#include <JKResourceCache.h>
#include <JKWindowManager.h>
#include <client/JKClientSurface.h>
#include <SDL.h>
#include <memory>
#include <string>
#include <mutex>
#include <chrono>

namespace jk {

class JKTimerThread;
class JKAudioThread;

// Client-side application that renders into a server-managed surface instead of
// owning a visible SDL window. A hidden SDL window + renderer is used locally
// so that JKDC and the resource cache can upload textures and render to an
// off-screen target, whose pixels are then copied to shared memory and
// committed to the window server.
//
// It is a JKApplicationHost (NOT a JKApplication — a client must not own the
// SDL video/audio threads the single-process host owns), so controls and
// shared dialogs reach its services through g_jkAppHost exactly like they
// reach JKApplication in single-process mode.
class JKClientApplication : public JKApplicationHost {
public:
    JKClientApplication();
    virtual ~JKClientApplication();

    // Connect to the server at `pipeName` and request a surface of the given
    // logical size. Returns false if any step fails.
    bool Init(const std::string& title, int width, int height,
              const std::string& pipeName);

    void Close();
    int Run();

    void SetMainWindow(std::unique_ptr<JKWindow> window);
    JKWindow* GetMainWindow() const;

    void SetModalWindow(JKWindow* window);
    JKWindow* GetModalWindow() const;

    // JKApplicationHost: IME composition targets the hidden renderer window.
    SDL_Window* GetSdlWindow() const { return hiddenWindow_; }

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

    void SetTimerInterval(uint32_t ms);
    uint64_t AddTimer(uint32_t winId, uint32_t intervalMs, bool repeat);
    void RemoveTimer(uint64_t handle);
    void RemoveTimersForWindow(uint32_t winId);

    // Stop the run loop cleanly (Exit buttons in client apps): Run() returns,
    // Close() disconnects the surface and the server removes the layer.
    void RequestQuit() { running_ = false; }

    void PostAudioCommand(const AudioCommand& cmd);

    void SetLogicalSize(int w, int h);
    void GetLogicalSize(int& w, int& h) const;
    void GetScale(float& sx, float& sy) const;
    void GetLetterbox(int& x, int& y) const;

protected:
    virtual void OnInit() {}
    virtual void OnClose() {}
    virtual bool PreProcessMessage(const JKEvent& ev);
    virtual void RouteMessage(const JKEvent& ev);
    virtual void ComposeScene();

    // Run-loop hooks (docs/22 §5.3): OnIdle runs every loop iteration before
    // the frame gate — apps pump background sources there (terminal ConPTY).
    // IsFrameDirty gates RenderAndCommit so idle apps do not burn CPU; the
    // default keeps the legacy always-repaint behavior. OnFrameCommitted runs
    // after a render+commit so apps can clear dirty state.
    virtual void OnIdle() {}
    virtual bool IsFrameDirty() const { return true; }
    virtual void OnFrameCommitted() {}
    // TAB drives the child focus cycle in most apps; the terminal consumes it
    // as data instead.
    virtual bool WantsTabFocusCycle() const { return true; }

private:
    SDL_Window* hiddenWindow_ = nullptr;
    SDL_Renderer* hiddenRenderer_ = nullptr;
    std::unique_ptr<JKSDLRenderBackend> renderBackend_;

    std::unique_ptr<jk::client::JKClientSurface> surface_;

    std::unique_ptr<JKWindow> mainWindow_;
    std::unique_ptr<JKWindowManager> windowManager_;
    std::unique_ptr<JKMessageBus> messageBus_;
    JKDC dc_;
    std::unique_ptr<HangulManager> hangulManager_;
    std::unique_ptr<JKResourceCache> resourceCache_;
    bool running_ = false;

    std::unique_ptr<JKTimerThread> timerThread_;

    mutable std::mutex stateMutex_;
    int logicalWidth_ = 0;
    int logicalHeight_ = 0;
    float scaleX_ = 1.0f;
    float scaleY_ = 1.0f;
    int letterboxX_ = 0;
    int letterboxY_ = 0;

    uint32_t legacyTimerWinId_ = 0;
    uint32_t legacyTimerInterval_ = 0;

    std::vector<uint8_t> pixelBuffer_;
    std::vector<uint8_t> pendingScene_;

    JKRenderBackend::TextureHandle targetTexture_ = JKRenderBackend::InvalidTexture;
    int targetW_ = 0;
    int targetH_ = 0;

    bool CreateHiddenRenderer(const std::string& title, int width, int height);
    void DestroyHiddenRenderer();
    bool ProcessOneEvent(const JKEvent& ev);
    void ApplyInputRouting(JKEvent& ev);
    void DrainTimerChannel();
    void DrainInputChannel();
    void RenderAndCommit();
};

} // namespace jk

#endif // JKCLIENTAPPLICATION_H
