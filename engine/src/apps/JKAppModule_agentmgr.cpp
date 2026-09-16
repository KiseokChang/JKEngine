// Agent manager app module (specs/2026-09-16-agent-manager). All C++ —
// app object construction, Init and Run — happens inside this DLL; the host
// only sees the C ABI from JKAppModule.h.
#include <apps/JKAppModule.h>
#include <apps/ClientAgentMgrApp.h>

JKAPP_EXPORT const jk::JKAppMeta* jk_app_meta() {
    static const jk::JKAppMeta meta{ "agentmgr", "Agent Manager", 560, 640 };
    return &meta;
}

JKAPP_EXPORT int jk_app_run_client(const char* pipeName) {
    const jk::JKAppMeta* meta = jk_app_meta();
    jk::ClientAgentMgrApp app;
    if (!app.Init(meta->title, meta->width, meta->height, pipeName)) {
        return 1;
    }
    return app.Run();
}