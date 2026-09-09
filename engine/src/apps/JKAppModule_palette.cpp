// Command palette app module (agent platform M2a, spec §6.2). All C++ —
// app object construction, Init and Run — happens inside this DLL; the host
// only sees the C ABI from JKAppModule.h.
#include <apps/JKAppModule.h>
#include <apps/ClientPaletteApp.h>

JKAPP_EXPORT const jk::JKAppMeta* jk_app_meta() {
    static const jk::JKAppMeta meta{ "palette", "Command Palette", 520, 360 };
    return &meta;
}

JKAPP_EXPORT int jk_app_run_client(const char* pipeName) {
    const jk::JKAppMeta* meta = jk_app_meta();
    jk::ClientPaletteApp app;
    if (!app.Init(meta->title, meta->width, meta->height, pipeName)) {
        return 1;
    }
    return app.Run();
}