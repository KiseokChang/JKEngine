#ifndef APPS_CLIENTMINESWEEPERAPP_H
#define APPS_CLIENTMINESWEEPERAPP_H

#include <client/JKClientApplication.h>
#include <JKEvent.h>
#include <memory>

namespace jk {

class MineGameWindow;

// Separate-process Minesweeper client. Renders into a server-managed surface via
// JKClientApplication instead of owning a visible SDL window.
class ClientMineSweeperApp : public JKClientApplication {
public:
    ClientMineSweeperApp();
    ~ClientMineSweeperApp() override;

protected:
    void OnInit() override;
    bool PreProcessMessage(const JKEvent& ev) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace jk

#endif // APPS_CLIENTMINESWEEPERAPP_H
