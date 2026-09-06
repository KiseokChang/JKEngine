#ifndef CLIENTOCCAPP_H
#define CLIENTOCCAPP_H

#include <client/JKClientApplication.h>
#include <memory>

namespace jk {

class OccUI;

// Server-mode client module wrapper for the OCC (2CAOCC C2) app. Renders into
// a server-managed surface and reuses the single-process OccUI.
class ClientOccApp : public JKClientApplication {
public:
    ClientOccApp();
    ~ClientOccApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    bool PreProcessMessage(const JKEvent& ev) override;

private:
    std::unique_ptr<OccUI> ui_;
};

} // namespace jk

#endif // CLIENTOCCAPP_H