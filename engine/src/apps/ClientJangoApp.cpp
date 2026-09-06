#include <apps/ClientJangoApp.h>

#include <apps/JangoUI.h>

namespace jk {

ClientJangoApp::ClientJangoApp() = default;

ClientJangoApp::~ClientJangoApp() = default;

void ClientJangoApp::OnInit() {
    ui_ = std::make_unique<JangoUI>([this]() {
        // Exit button: stop the run loop; Close() disconnects the surface and
        // the window server removes the layer.
        RequestQuit();
    });
    ui_->BuildMainWindow();
    SetMainWindow(ui_->TakeMainWindow());
    ui_->CreateDialogs();
}

} // namespace jk