#ifndef APPS_CLIENTTETRISAPP_H
#define APPS_CLIENTTETRISAPP_H

#include <client/JKClientApplication.h>
#include <JKEvent.h>
#include <memory>

namespace jk {

class TetrisGameWindow;

// Separate-process Tetris client. Renders into a server-managed surface via
// JKClientApplication instead of owning a visible SDL window.
class ClientTetrisApp : public JKClientApplication {
public:
    ClientTetrisApp();
    ~ClientTetrisApp() override;

protected:
    void OnInit() override;
    bool PreProcessMessage(const JKEvent& ev) override;

private:
    std::unique_ptr<TetrisGameWindow> gameWindow_;
};

} // namespace jk

#endif // APPS_CLIENTTETRISAPP_H
