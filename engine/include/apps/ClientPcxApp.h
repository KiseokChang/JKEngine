#ifndef APPS_CLIENTPCXAPP_H
#define APPS_CLIENTPCXAPP_H

#include <client/JKClientApplication.h>

namespace jk {

// Separate-process image viewer client (pcx module). Renders into a
// server-managed surface via JKClientApplication instead of owning a visible
// SDL window. Reuses the single-process viewer construction through
// CreatePcxViewerWindow; OnIdle opens the startup file dialog and
// PreProcessMessage forwards wheel events (no hit target) to the canvas.
class ClientPcxApp : public JKClientApplication {
public:
    ClientPcxApp() = default;
    ~ClientPcxApp() override = default;

protected:
    void OnInit() override;
    void OnIdle() override;
    bool PreProcessMessage(const JKEvent& ev) override;

private:
    bool startupDone_ = false;
};

} // namespace jk

#endif // APPS_CLIENTPCXAPP_H