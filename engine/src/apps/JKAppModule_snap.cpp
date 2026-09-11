// Region-capture overlay app module (agent platform, docs/35 Task 3). All
// C++ — app object construction, Init and Run — happens inside this DLL; the
// host only sees the C ABI from JKAppModule.h. The meta size is a placeholder:
// the server resizes the surface to the full desktop on spawn.
#include <apps/JKAppModule.h>
#include <apps/ClientSnapApp.h>

JKAPP_EXPORT const jk::JKAppMeta* jk_app_meta() {
    // Title must stay in sync with kCaptureOverlayTitle (JKCompositor.h) —
    // the server keys its fullscreen placement + chromeless treatment on it.
    static const jk::JKAppMeta meta{ "snap", "Region Capture", 800, 600 };
    return &meta;
}

JKAPP_EXPORT int jk_app_run_client(const char* pipeName) {
    const jk::JKAppMeta* meta = jk_app_meta();
    jk::ClientSnapApp app;
    if (!app.Init(meta->title, meta->width, meta->height, pipeName)) {
        return 1;
    }
    return app.Run();
}