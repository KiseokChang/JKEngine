// Settings hub app module (specs/2026-09-18-settings-hub). All C++ stays in
// the DLL; the host only sees the C ABI from JKAppModule.h.
#include <apps/JKAppModule.h>
#include <apps/ClientSettingsApp.h>

JKAPP_EXPORT const jk::JKAppMeta* jk_app_meta() {
    static const jk::JKAppMeta meta{ "settings", "Settings", 900, 620 };
    return &meta;
}

JKAPP_EXPORT int jk_app_run_client(const char* pipeName) {
    const jk::JKAppMeta* meta = jk_app_meta();
    jk::ClientSettingsApp app;
    if (!app.Init(meta->title, meta->width, meta->height, pipeName)) {
        return 1;
    }
    return app.Run();
}
