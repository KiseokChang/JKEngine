#include <apps/ClientIconEditApp.h>

#include <apps/IconEditViews.h>
#include <JKWindow.h>

namespace jk {

ClientIconEditApp::ClientIconEditApp() = default;
ClientIconEditApp::~ClientIconEditApp() = default;

void ClientIconEditApp::OnInit() {
    auto main = std::make_unique<JKWindow>("Icon Editor - SDL2 Port");
    main->SetWindowRect(JKRect{ 0, 0, 1920, 1080 });

    // Same layout as IconEditApp::OnInit (1920x1080 with the pixel board at
    // {20,120,1900,1060}). All controls are added straight to the root
    // window — no floating child window, so no chrome stripping is needed;
    // the server overlays the close button.
    ui_ = std::make_unique<IconEditUI>();
    ui_->Build(main.get());

    SetMainWindow(std::move(main));
}

} // namespace jk