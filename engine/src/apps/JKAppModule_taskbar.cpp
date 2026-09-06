// Taskbar (shell) app module (docs/28). The shell is NOT an app in the
// launcher sense — it is never packaged as .jkx; the server auto-spawns it
// from jkapp_taskbar.dll in stage 2. Dynamic-loading entry points follow the
// JKAppModule pattern.
#include <apps/JKAppModule.h>
#include <apps/ClientTaskbarApp.h>

JKAPP_EXPORT const jk::JKAppMeta* jk_app_meta() {
    static const jk::JKAppMeta meta{ "taskbar", "Taskbar", 1280, 40 };
    return &meta;
}

JKAPP_EXPORT int jk_app_run_client(const char* pipeName) {
    const jk::JKAppMeta* meta = jk_app_meta();
    jk::ClientTaskbarApp app;
    if (!app.Init(meta->title, meta->width, meta->height, pipeName)) {
        return 1;
    }
    return app.Run();
}