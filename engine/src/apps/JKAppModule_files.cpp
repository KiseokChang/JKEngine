// File hub app module (specs/2026-09-18-file-hub). All C++ stays in the
// DLL; the host only sees the C ABI from JKAppModule.h.
#include <apps/JKAppModule.h>
#include <apps/ClientFilesApp.h>

JKAPP_EXPORT const jk::JKAppMeta* jk_app_meta() {
    static const jk::JKAppMeta meta{ "files", "Files", 860, 560 };
    return &meta;
}

JKAPP_EXPORT int jk_app_run_client(const char* pipeName) {
    const jk::JKAppMeta* meta = jk_app_meta();
    jk::ClientFilesApp app;
    if (!app.Init(meta->title, meta->width, meta->height, pipeName)) {
        return 1;
    }
    return app.Run();
}