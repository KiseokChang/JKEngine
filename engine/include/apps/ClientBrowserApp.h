#ifndef APPS_CLIENTBROWSERAPP_H
#define APPS_CLIENTBROWSERAPP_H

#include <client/JKClientApplication.h>
#include <chrono>
#include <string>

namespace jk {

// ImGui Phase 4 long-term track (docs/23 §11.9): a Chromium browser client
// built on the CEF C API. The app links libcef.dll directly (no
// libcef_dll_wrapper — GNU ld links the DLL as an import) and runs CEF's
// integrated message loop on the app's main thread: cef_do_message_loop_work()
// is pumped from RenderOverlay, so on_paint BGRA frames arrive on that same
// thread and are uploaded straight into an SDL texture drawn with ImGui —
// the page is just another texture, exactly like vplayer video frames.
// CEF subprocesses (renderer/GPU) re-enter cefosr.exe next to the host exe;
// the CEF runtime set must sit next to jkdesktop.exe (see
// third_party/cef/README.md).
class ClientBrowserApp : public JKClientApplication {
public:
    ~ClientBrowserApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    bool PreProcessMessage(const JKEvent& ev) override;
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;

private:
    void BuildUi(int w, int h);
    void InitCef();
    void Navigate(const char* url);

    bool frameDirty_ = true;
    bool imguiReady_ = false;
    bool cefReady_ = false;
    std::string initError_;
    char urlBuf_[2048] = {0};
    std::chrono::steady_clock::time_point lastFrame_{};

    // SDL_Texture*/SDL_Renderer* kept as void* so this header stays SDL-free.
    void* renderer_ = nullptr;

    // Page-area geometry (everything below the URL bar row). CEF's OSR view
    // IS the page area — get_view_rect reports this size and mouse coords are
    // translated by pageY before being handed to CEF.
    int pageY_ = 80;
    int pageW_ = 960, pageH_ = 576;
    bool viewSizeDirty_ = false;

    int lastMouseX_ = 0, lastMouseY_ = 0;

    // True between a page MouseDown and its MouseUp. UPs are paired by this
    // flag instead of the ImGui gate: WantCaptureMouse is true while any
    // button is down (imgui.cpp mouse_any_down), which would otherwise eat
    // the UP and leave CEF stuck pressed.
    bool cefMouseDown_ = false;
};

} // namespace jk

#endif // APPS_CLIENTBROWSERAPP_H