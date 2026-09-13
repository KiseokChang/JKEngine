// File open dialog app module (file dialog design §1b, Task 2). The host only
// sees the C ABI from JKAppModule.h; the dialog resolves the server's parked
// file_open query over its own window connection.
#include <apps/JKAppModule.h>
#include <apps/ClientFileDialogApp.h>

JKAPP_EXPORT const jk::JKAppMeta* jk_app_meta() {
    static const jk::JKAppMeta meta{ "filedlg", "파일 열기", 560, 400 };
    return &meta;
}

JKAPP_EXPORT int jk_app_run_client(const char* pipeName) {
    const jk::JKAppMeta* meta = jk_app_meta();
    jk::ClientFileDialogApp app;
    if (!app.Init(meta->title, meta->width, meta->height, pipeName)) {
        return 1;
    }
    const int rc = app.Run();
    // Server chrome Quit path: the read thread's Quit stops the run loop
    // before any UI hook can fire — attempt the file_open_result send here,
    // while the transport is still open (no-op on the open/cancel paths).
    // Failure is harmless: the parked query expires and reclaims the slot.
    app.FlushPendingResult();
    return rc;
}