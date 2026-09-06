#include <apps/ClientRecogApp.h>

#include <apps/RecogViews.h>
#include <JKWindow.h>

namespace jk {

ClientRecogApp::ClientRecogApp() = default;
ClientRecogApp::~ClientRecogApp() = default;

void ClientRecogApp::OnInit() {
    auto main = std::make_unique<JKWindow>("Stroke Recognition - SDL2 Port");
    main->SetWindowRect(JKRect{ 0, 0, 1280, 680 });

    // Same layout as RecogApp::OnInit (1280x680 with the input board at
    // {20,320,1880,1060}). All controls are added straight to the root
    // window — no floating child window, so no chrome stripping is needed;
    // the server overlays the close button.
    ui_ = std::make_unique<RecogUI>();
    ui_->Build(main.get());

    SetMainWindow(std::move(main));
}

} // namespace jk