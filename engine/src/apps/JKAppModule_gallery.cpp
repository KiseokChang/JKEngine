// Gallery app module (spec 2026-10-09-gallery-design): 사진 모아보기 허브.
// All C++ — app object construction, Init and Run — happens inside this DLL;
// the host only sees the C ABI from JKAppModule.h.
#include <apps/JKAppModule.h>
#include <apps/ClientGalleryApp.h>

JKAPP_EXPORT const jk::JKAppMeta* jk_app_meta() {
    static const jk::JKAppMeta meta{ "gallery", "Gallery", 560, 520 };
    return &meta;
}

JKAPP_EXPORT int jk_app_run_client(const char* pipeName) {
    const jk::JKAppMeta* meta = jk_app_meta();
    jk::ClientGalleryApp app;
    if (!app.Init(meta->title, meta->width, meta->height, pipeName)) {
        return 1;
    }
    return app.Run();
}