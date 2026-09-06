#include <apps/ClientPcxApp.h>

#include <apps/PcxViews.h>

namespace jk {

void ClientPcxApp::OnInit() {
    // App modules receive no CLI arguments, so run with an empty path. The
    // first OnIdle tick then opens the file dialog (after the window manager
    // registered the root window).
    auto main = CreatePcxViewerWindow(std::string());
    // The root window paints its own frame chrome; the server overlays the
    // close button. Its rect is the designed 1280x680 layout (1:1 on the
    // 1280x680 work area — no fit-scale shrink).
    SetMainWindow(std::move(main));
}

void ClientPcxApp::OnIdle() {
    if (!startupDone_) {
        startupDone_ = true;
        if (auto* viewer = static_cast<ImageViewerWindow*>(GetMainWindow())) {
            viewer->MaybeShowStartupDialog();
        }
    }
}

bool ClientPcxApp::PreProcessMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::MouseWheel) {
        if (auto* viewer = static_cast<ImageViewerWindow*>(GetMainWindow())) {
            viewer->ForwardWheel(ev.dy);
        }
    }
    return JKClientApplication::PreProcessMessage(ev);
}

} // namespace jk