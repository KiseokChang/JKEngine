#ifndef APPS_CLIENTPCXAPP_H
#define APPS_CLIENTPCXAPP_H

#include <client/JKClientApplication.h>

namespace jk {

// Separate-process PCX viewer client. Renders into a server-managed surface
// via JKClientApplication instead of owning a visible SDL window. Reuses the
// single-process viewer construction through CreatePcxViewerWindow.
class ClientPcxApp : public JKClientApplication {
public:
    ClientPcxApp() = default;
    ~ClientPcxApp() override = default;

protected:
    void OnInit() override;
};

} // namespace jk

#endif // APPS_CLIENTPCXAPP_H