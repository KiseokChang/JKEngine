#include <apps/ClientTetrisApp.h>

#include <apps/TetrisApp.h>
#include <JKWindow.h>

namespace jk {

ClientTetrisApp::ClientTetrisApp() = default;
ClientTetrisApp::~ClientTetrisApp() = default;

void ClientTetrisApp::OnInit() {
    auto main = std::make_unique<JKWindow>("Tetris");
    main->SetWindowRect(JKRect{ 0, 0, 320, 520 });

    // Fill the main window's client area exactly (the root paints the frame
    // chrome that the server overlays) and dock-fill so server-initiated
    // resizes propagate into the game layout.
    const JKRect clientArea = main->GetClientRect();
    gameWindow_ = std::make_unique<TetrisGameWindow>();
    gameWindow_->Build(main.get(), JKRect{ 0, 0, clientArea.w, clientArea.h });
    // TetrisWindow is built as a floating window (own title bar + move/resize
    // attrs) for the single-process path. In server mode the window server
    // owns all chrome, so strip it: chrome-less and fixed in place.
    auto* gameWin = gameWindow_->GetWindow();
    gameWin->SetAttrFlags(WA_CHROMELESS);
    gameWin->SetWindowRect(JKRect{ 0, 0, clientArea.w, clientArea.h });
    gameWin->SetDock(DOCK_FILL);
    // The grid's SetFocus() (inside Build) stops at the FIRST window ancestor
    // — the game window, a 2nd-level window here — so the root's focusChild_
    // stays null and JKWindow::RespondMessage drops all KeyDown events.
    // Focus the game window itself so the root forwards keys into it
    // (root → gameWin → grid), matching single-process where the game window
    // IS the main window. FocusFirstChild later focuses the grid under it.
    gameWin->SetFocus();

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
