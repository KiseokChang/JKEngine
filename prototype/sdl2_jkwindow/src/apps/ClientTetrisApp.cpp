#include <apps/ClientTetrisApp.h>

#include <apps/TetrisApp.h>
#include <JKWindow.h>

namespace jk {

ClientTetrisApp::ClientTetrisApp() = default;
ClientTetrisApp::~ClientTetrisApp() = default;

void ClientTetrisApp::OnInit() {
    auto main = std::make_unique<JKWindow>("Tetris");
    main->SetWindowRect(JKRect{ 0, 0, 320, 520 });

    gameWindow_ = std::make_unique<TetrisGameWindow>();
    gameWindow_->Build(main.get(), JKRect{ 0, 0, 320, 520 });

    SetMainWindow(std::move(main));

    uint32_t interval = gameWindow_->GetTimerInterval();
    SetTimerInterval(interval);
    gameWindow_->NewGame();
}

bool ClientTetrisApp::PreProcessMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::Timer && gameWindow_) {
        uint32_t interval = gameWindow_->GetTimerInterval();
        gameWindow_->OnTimer(interval);
        SetTimerInterval(interval);
    }
    return JKClientApplication::PreProcessMessage(ev);
}

} // namespace jk
