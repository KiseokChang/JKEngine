#ifndef CLIENTSNAPAPP_H
#define CLIENTSNAPAPP_H

// Rubber-band region-capture overlay (agent platform, docs/35 Task 3).
// A chromeless, transparent, desktop-sized layer: the server resizes the
// surface to the output size on spawn (the DockShellClient ResizeSurface
// round-trip — the meta size is only a placeholder) and keeps it at (0,0)
// with scale 1, so this app's client coords are desktop logical coords.
// The user drags a rectangle over the dimmed desktop; mouse-up sends it to
// the capture_region tool via the window connection and the app closes
// itself when the reply (or a 3 s timeout) arrives. ESC cancels.

#include <client/JKClientApplication.h>
#include <cstdint>

namespace jk {

class ClientSnapApp : public JKClientApplication {
public:
    ClientSnapApp() = default;
    ~ClientSnapApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    bool PreProcessMessage(const JKEvent& ev) override;
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;
    bool WantsTransparentSurface() const override { return true; }

private:
    void BuildUi(int w, int h);
    // Mouse-up: send the drag rect to capture_region (docs/35 Task 2 hides
    // this app's layer during the readback, so the dim never lands in the
    // shot). Space-free JSON — the palette's native-argv lesson.
    void SendCapture(int x, int y, int w, int h);
    void CloseSelf();

    bool frameDirty_ = true;
    bool imguiReady_ = false;

    // Drag state (surface coords = desktop logical coords, see above).
    bool dragging_ = false;
    float dragX0_ = 0.0f;
    float dragY0_ = 0.0f;
    float dragX1_ = 0.0f;
    float dragY1_ = 0.0f;

    // 1-deep agent query state (palette pattern): 0 = idle; while nonzero a
    // capture_region reply is awaited, with a deadline so a wedged server
    // still dismisses the overlay.
    uint32_t nextQueryId_ = 1;
    uint32_t pendingQueryId_ = 0;
    uint64_t queryDeadlineMs_ = 0;
};

} // namespace jk

#endif // CLIENTSNAPAPP_H