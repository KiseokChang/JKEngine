// Chat app module (스펙 2026-10-07-desktop-chat-app — T1 stub 라우터 소비자).
// C++ 전부 DLL 안부.
#include <apps/JKAppModule.h>
#include <apps/ClientChatApp.h>

JKAPP_EXPORT const jk::JKAppMeta* jk_app_meta() {
    static const jk::JKAppMeta meta{ "chat", "Chat", 720, 540 };
    return &meta;
}

JKAPP_EXPORT int jk_app_run_client(const char* pipeName) {
    const jk::JKAppMeta* meta = jk_app_meta();
    jk::ClientChatApp app;
    if (!app.Init(meta->title, meta->width, meta->height, pipeName)) {
        return 1;
    }
    return app.Run();
}
