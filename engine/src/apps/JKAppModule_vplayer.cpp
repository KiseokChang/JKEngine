// Video player app module (ImGui Phase 4 long-term track, docs/23 §11.8).
// FFmpeg demux/decode + SDL audio live inside this DLL; the host only sees
// the C ABI from JKAppModule.h and the server stays a dumb compositor.
#include <apps/JKAppModule.h>
#include <apps/ClientVPlayerApp.h>

JKAPP_EXPORT const jk::JKAppMeta* jk_app_meta() {
    static const jk::JKAppMeta meta{ "vplayer", "Video Player", 960, 640 };
    return &meta;
}

JKAPP_EXPORT int jk_app_run_client(const char* pipeName) {
    const jk::JKAppMeta* meta = jk_app_meta();
    jk::ClientVPlayerApp app;
    if (!app.Init(meta->title, meta->width, meta->height, pipeName)) {
        return 1;
    }
    return app.Run();
}