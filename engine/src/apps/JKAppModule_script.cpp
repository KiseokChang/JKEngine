// Common script app module (docs/27 단계 1) — every script .jkx shares this
// DLL; the script (app.js) is data extracted by the client host next to it as
// <dllPath>.app.js together with a manifest copy <dllPath>.manifest.txt
// (docs/27 §8.1: no ABI change, module locates its data via its own path).
#include <apps/JKAppModule.h>
#include <apps/ClientScriptApp.h>
#include <JKJkxFile.h>

#include <windows.h>

#include <cstdio>
#include <string>

namespace {

// <path of this DLL><suffix> — the client host drops the manifest copy and
// app.js beside the extracted DLL.
std::string SideFilePath(const char* suffix) {
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&SideFilePath), &self);
    char dllPath[MAX_PATH] = {};
    if (!self || !GetModuleFileNameA(self, dllPath, MAX_PATH)) return {};
    return std::string(dllPath) + suffix;
}

bool ReadTextFile(const std::string& path, std::string& out) {
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "rb");
    if (!f) return false;
    char buf[4096];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

} // namespace

JKAPP_EXPORT const jk::JKAppMeta* jk_app_meta() {
    static jk::JKAppMeta meta{ "script", "Script App", 320, 240 };
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        // The manifest copy is informational-only at runtime everywhere else
        // (the native module's jk_app_meta is authoritative); here the module
        // IS the manifest reader since scripts have no native meta of their
        // own. Defaults keep --client mode (no .jkx) usable for debugging.
        std::string text;
        if (ReadTextFile(SideFilePath(".manifest.txt"), text)) {
            jk::JkxManifest mani;
            if (mani.Parse(text)) {
                static const std::string name =
                    mani.name.empty() ? std::string("script") : mani.name;
                static const std::string title =
                    mani.title.empty() ? name : mani.title;
                meta.name = name.c_str();
                meta.title = title.c_str();
                if (mani.width > 0) meta.width = mani.width;
                if (mani.height > 0) meta.height = mani.height;
            }
        }
    }
    return &meta;
}

JKAPP_EXPORT int jk_app_run_client(const char* pipeName) {
    const jk::JKAppMeta* meta = jk_app_meta();

    jk::ClientScriptApp app;
    app.SetScriptInfo(meta->title, SideFilePath(".app.js"));
    if (!app.Init(meta->title, meta->width, meta->height, pipeName)) {
        return 1;
    }
    return app.Run();
}