// Library hub app module (스펙 2026-10-06-app-library — settings/notes/files
// 4호 멤버). C++ 전부 DLL 안부.
#include <apps/JKAppModule.h>
#include <apps/ClientLibraryApp.h>

JKAPP_EXPORT const jk::JKAppMeta* jk_app_meta() {
    static const jk::JKAppMeta meta{ "library", "Library", 920, 640 };
    return &meta;
}

JKAPP_EXPORT int jk_app_run_client(const char* pipeName) {
    const jk::JKAppMeta* meta = jk_app_meta();
    jk::ClientLibraryApp app;
    if (!app.Init(meta->title, meta->width, meta->height, pipeName)) {
        return 1;
    }
    return app.Run();
}