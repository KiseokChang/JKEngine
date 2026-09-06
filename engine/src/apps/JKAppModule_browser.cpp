// Browser app module (docs/23 §11.9, ImGui Phase 4 long-term track): CEF C
// API lives inside this DLL (libcef.dll is an import, no wrapper); the host
// only sees the C ABI from JKAppModule.h and the server stays a dumb
// compositor. CEF runtime + cefosr.exe (subprocess worker) sit next to the
// host exe — see third_party/cef/README.md.
#include <apps/JKAppModule.h>
#include <apps/ClientBrowserApp.h>

JKAPP_EXPORT const jk::JKAppMeta* jk_app_meta() {
    static const jk::JKAppMeta meta{ "browser", "Browser", 960, 640 };
    return &meta;
}

JKAPP_EXPORT int jk_app_run_client(const char* pipeName) {
    const jk::JKAppMeta* meta = jk_app_meta();
    jk::ClientBrowserApp app;
    if (!app.Init(meta->title, meta->width, meta->height, pipeName)) {
        return 1;
    }
    return app.Run();
}