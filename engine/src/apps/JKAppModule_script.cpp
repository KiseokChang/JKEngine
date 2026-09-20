// Common script app module (docs/27 단계 1) — every script .jkx shares this
// DLL; the script (app.js) is data extracted by the client host next to it as
// <dllPath>.app.js together with a manifest copy <dllPath>.manifest.txt
// (docs/27 §8.1: no ABI change, module locates its data via its own path).
//
// Workshop mode (docs/60 §2.1-2.2): MANI `scriptfile=` points at an
// exeDir-relative editable script (the workshop's single source of truth —
// human notepad edits and agent tool writes are the same file) and `watch=1`
// forces hot reload on. The module resolves and (when missing) seeds the file
// with a template, then runs the WorkshopScriptApp variant that registers
// get_script/set_script agent tools. Built-in SCRI apps are unchanged.
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

std::string ExeDir() {
    char exe[MAX_PATH] = {};
    if (!GetModuleFileNameA(nullptr, exe, MAX_PATH)) return {};
    const std::string p(exe);
    const size_t slash = p.find_last_of("\\/");
    return slash == std::string::npos ? std::string(".") : p.substr(0, slash);
}

bool IsAbsolutePath(const std::string& p) {
    return (p.size() >= 2 && p[1] == ':') ||
           (!p.empty() && (p[0] == '\\' || p[0] == '/'));
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

bool WriteTextFile(const std::string& path, const std::string& data) {
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "wb");
    if (!f) return false;
    const size_t w = std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    return w == data.size();
}

void EnsureParentDirs(const std::string& path) {
    size_t pos = 0;
    while ((pos = path.find_first_of("\\/", pos + 1)) != std::string::npos) {
        CreateDirectoryA(path.substr(0, pos).c_str(), nullptr);
    }
}

// docs/60 §2.1 — the workshop truth source. Seeded with a hello button when
// missing so a fresh install boots to something editable (by hand or by
// talking to the agent: "할일 판 만들어줘").
const char kTemplateScript[] =
    "// myapp.js — 워크숍 진실원 (docs/60 §2.1)\n"
    "// 이 파일이 곧 앱입니다. 메모장에서 고치거나, 에이전트에게 말로\n"
    "// 고치게 하세요 (폰 채팅: \"할일 판 만들어줘\"). 저장하면 즉시 리로드됩니다.\n"
    "\n"
    "var hello = createLabel({ x: 20, y: 20, w: 220, h: 26 }, \"안녕하세요!\");\n"
    "var helloBtn = createButton({ x: 20, y: 56, w: 140, h: 34 }, \"눌러 보세요\");\n"
    "\n"
    "function onClick(id) {\n"
    "    if (id === helloBtn) {\n"
    "        setText(hello, \"반가워요!\");\n"
    "    }\n"
    "}\n";

// Resolved workshop script path + MANI watch flag, filled by jk_app_meta's
// one-time manifest read (single-threaded: the client process reads meta on
// its main thread before anything else touches these).
std::string g_scriptfile;
bool g_watch = false;

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
                g_scriptfile = mani.scriptfile;
                g_watch = (mani.watch != 0);
            }
        }
    }
    return &meta;
}

JKAPP_EXPORT int jk_app_run_client(const char* pipeName) {
    const jk::JKAppMeta* meta = jk_app_meta();

    if (!g_scriptfile.empty()) {
        // Workshop mode (docs/60 §2.1): the editable external script is the
        // truth source — resolve exeDir-relative (absolute passes through)
        // and seed the template when the file does not exist yet.
        std::string path = g_scriptfile;
        if (!IsAbsolutePath(path)) {
            std::string base = ExeDir();
            if (!base.empty() && base.back() != '\\' && base.back() != '/') {
                base += '\\';
            }
            path = base + g_scriptfile;
        }
        std::string existing;
        if (!ReadTextFile(path, existing)) {
            EnsureParentDirs(path);
            if (!WriteTextFile(path, kTemplateScript)) {
                std::fprintf(stderr, "[workshop] cannot seed '%s'\n", path.c_str());
                return 1;
            }
        }

        jk::WorkshopScriptApp app;
        app.SetScriptInfo(meta->title, path);
        app.SetHotWatch(g_watch);
        app.SetAgentAppName(meta->name);
        if (!app.Init(meta->title, meta->width, meta->height, pipeName)) {
            return 1;
        }
        return app.Run();
    }

    jk::ClientScriptApp app;
    app.SetScriptInfo(meta->title, SideFilePath(".app.js"));
    if (!app.Init(meta->title, meta->width, meta->height, pipeName)) {
        return 1;
    }
    return app.Run();
}