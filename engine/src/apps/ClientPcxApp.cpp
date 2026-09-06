#include <apps/ClientPcxApp.h>

#include <apps/PcxViews.h>
#include <JKWindow.h>

namespace jk {

void ClientPcxApp::OnInit() {
    // App modules receive no CLI arguments, so run with an empty path. The
    // shared builder then shows the same "Failed to load PCX file."
    // placeholder as the single-process `pcx` invocation without a file.
    auto main = CreatePcxViewerWindow(std::string());
    // The root window paints its own frame chrome; the server overlays the
    // close button. Its rect is the designed 1280x680 layout (1:1 on the
    // 1280x680 work area — no fit-scale shrink).
    SetMainWindow(std::move(main));
}

} // namespace jk