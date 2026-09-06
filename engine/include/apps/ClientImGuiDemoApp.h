#ifndef APPS_CLIENTIMGUIDEMOAPP_H
#define APPS_CLIENTIMGUIDEMOAPP_H

#include <client/JKClientApplication.h>
#include <chrono>

namespace jk {

// ImGui Phase 1 showcase (docs/23 §9): feeds every JKEvent into the fused
// jkwindow backend and renders the UI in RenderOverlay — after the scene
// replay, while the off-screen target texture is bound, before the shm
// readback. The JKWindow tree stays an empty dark chromeless root; the ImGui
// layer IS the UI (docs/23 §11 two-track principle: ImGui = utility apps).
class ClientImGuiDemoApp : public JKClientApplication {
public:
    ~ClientImGuiDemoApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    bool PreProcessMessage(const JKEvent& ev) override;
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;

private:
    void BuildUi(int w, int h);

    bool frameDirty_ = true;
    bool imguiReady_ = false;
    std::chrono::steady_clock::time_point lastFrame_;

    // Panel toggles + demo plot state (docs/23 §9 panel set).
    bool showDemo_ = true;
    bool showStyle_ = false;
    bool showIo_ = false;
    float plotValues_[90] = {};
    int plotOffset_ = 0;
};

} // namespace jk

#endif // APPS_CLIENTIMGUIDEMOAPP_H