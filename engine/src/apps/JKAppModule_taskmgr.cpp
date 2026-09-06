// Task manager app module (ImGui Phase 2 showcase, docs/23 §11.2).
// All C++ — app object construction, Init and Run — happens inside this DLL;
// the host only sees the C ABI from JKAppModule.h.
#include <apps/JKAppModule.h>
#include <apps/ClientTaskmgrApp.h>

JKAPP_EXPORT const jk::JKAppMeta* jk_app_meta() {
    static const jk::JKAppMeta meta{ "taskmgr", "Task Manager", 900, 620 };
    return &meta;
}

JKAPP_EXPORT int jk_app_run_client(const char* pipeName) {
    const jk::JKAppMeta* meta = jk_app_meta();
    jk::ClientTaskmgrApp app;
    if (!app.Init(meta->title, meta->width, meta->height, pipeName)) {
        return 1;
    }
    return app.Run();
}