#ifndef CLIENTTASKBARAPP_H
#define CLIENTTASKBARAPP_H

// Desktop shell client (docs/28): registers as the server's shell via
// ShellRegister and renders the taskbar. Stage 1 is the protocol skeleton —
// register on startup and log received WindowList snapshots; the button UI
// arrives in stage 3.

#include <client/JKClientApplication.h>
#include <string>
#include <vector>

namespace jk {

class ClientTaskbarApp : public JKClientApplication {
public:
    ClientTaskbarApp() = default;
    ~ClientTaskbarApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    bool PreProcessMessage(const JKEvent& ev) override;

private:
    void RefreshWindowList();

    // Latest snapshot from GetWindowList (copy of the surface's pending list).
    std::vector<jk::client::ShellWindowInfo> windows_;
};

} // namespace jk

#endif // CLIENTTASKBARAPP_H