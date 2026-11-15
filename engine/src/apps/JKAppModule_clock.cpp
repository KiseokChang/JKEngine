// 시계 앱 모듈 (2026-11-16 — 트레이 시계 클릭 leg). 순수 클라이언트.
#include <apps/JKAppModule.h>
#include <apps/ClientClockApp.h>

JKAPP_EXPORT const jk::JKAppMeta* jk_app_meta() {
    static const jk::JKAppMeta meta{ "clock", "시계", 420, 260 };
    return &meta;
}

JKAPP_EXPORT int jk_app_run_client(const char* pipeName) {
    const jk::JKAppMeta* meta = jk_app_meta();
    jk::ClientClockApp app;
    if (!app.Init(meta->title, meta->width, meta->height, pipeName)) return 1;
    return app.Run();
}
