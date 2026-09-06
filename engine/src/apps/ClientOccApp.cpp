#include <apps/ClientOccApp.h>

#include <apps/OccUI.h>
#include <JKEvent.h>

namespace jk {

ClientOccApp::ClientOccApp() = default;

ClientOccApp::~ClientOccApp() {
    if (ui_) {
        ui_->SaveAll();
    }
}

void ClientOccApp::OnInit() {
    ui_ = std::make_unique<OccUI>([this]() {
        // Exit menu: stop the run loop; Close() disconnects the surface and
        // the window server removes the layer.
        RequestQuit();
    });
    ui_->LoadAll();
    ui_->BuildMainWindow();
    SetMainWindow(ui_->TakeMainWindow());

    // 원본 SetTimer(30000)에 대응하는 1초 틱(상태줄 T+초 카운터).
    SetTimerInterval(1000);
}

void ClientOccApp::OnClose() {
    if (ui_) {
        ui_->SaveAll();
    }
}

bool ClientOccApp::PreProcessMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::Timer && ui_) {
        ui_->OnTimerTick();
    }
    return JKClientApplication::PreProcessMessage(ev);
}

} // namespace jk