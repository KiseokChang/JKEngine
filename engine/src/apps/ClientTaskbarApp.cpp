// Desktop shell / taskbar client (docs/28). Stage 1 skeleton: registers the
// shell role right after connecting and logs WindowList snapshots so the
// protocol round-trip is verifiable from both ends' logs.
#include <apps/ClientTaskbarApp.h>

#include <cstdio>

namespace jk {

ClientTaskbarApp::~ClientTaskbarApp() = default;

void ClientTaskbarApp::OnInit() {
    auto main = std::make_unique<JKWindow>("Taskbar");
    main->SetWindowRect(JKRect{ 0, 0, 1280, 40 });
    main->SetAttrFlags(WA_CHROMELESS);
    SetMainWindow(std::move(main));

    // Claim the shell role: bottom edge, 40pt requested thickness. The
    // server acks (accepted/denied) and replies with an initial WindowList
    // snapshot; the shell layer goes topmost from here on (stage 2).
    jk::client::JKClientSurface* surface = Surface();
    if (!surface || !surface->SendShellRegister(0 /*bottom*/, 40)) {
        std::fprintf(stderr, "[taskbar] SendShellRegister failed\n");
        std::fflush(stderr);
    }
}

void ClientTaskbarApp::OnClose() {
}

bool ClientTaskbarApp::PreProcessMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::WindowListChanged) {
        RefreshWindowList();
        return true;
    }
    return true;
}

void ClientTaskbarApp::RefreshWindowList() {
    jk::client::JKClientSurface* surface = Surface();
    if (!surface || !surface->GetWindowList(windows_)) {
        return;
    }
    std::fprintf(stderr, "[taskbar] window list (%u):\n",
                 static_cast<unsigned>(windows_.size()));
    for (const jk::client::ShellWindowInfo& w : windows_) {
        std::fprintf(stderr, "[taskbar]   id=%u flags=0x%x title='%s'\n",
                     w.surfaceId, w.flags, w.title.c_str());
    }
    std::fflush(stderr);
}

} // namespace jk