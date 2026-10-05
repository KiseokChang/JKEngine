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
// windows.h가 ClientScriptApp.h보다 먼저 와야 했다 — ClientScriptApp.h는
// windows.h 미포함 TU용 수기 선언(GetFileAttributesExA)을 가졌었다. 그 수기
// 선언은 JKPlatform::FileMtime100ns 흡수로 소각됐고(doc 60 §7 이력), stage-3
// task 7에서 win32-leg만 windows.h를 유지한다(모듈-자기 DLL 경로가 아직 원문
// API — SideFilePath 주석 참조). 리눅스-leg는 windows.h 없이 컴파일.
#ifdef _WIN32
#include <windows.h>
#endif
#include <apps/ClientScriptApp.h>
#include <fs/JKFs.h>
#include <JKJkxFile.h>
#include <apps/JKWorkshopSeed.h>

#include <cstdio>
#include <filesystem>
#include <string>

namespace {

// <path of this DLL><suffix> — the client host drops the manifest copy and
// app.js beside the extracted DLL.
std::string SideFilePath(const char* suffix) {
#ifdef _WIN32
    // GetModuleHandleExA+GetModuleFileNameA 원문 — 모듈(추출된 DLL) 측 경로가
    // 계약(extract-next-to-DLL)이라 exe 경로 어댑터로의 치환이 관측을 바꾼다.
    // 리눅스-leg만 jk::fs::GetExecutablePath 승계(brief; 리눅스는 아직 모듈
    // 로딩 경로가 미배선 — build 디렉터리 안 .so가 exe와 동거하므로 동치).
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&SideFilePath), &self);
    char dllPath[MAX_PATH] = {};
    if (!self || !GetModuleFileNameA(self, dllPath, MAX_PATH)) return {};
    return std::string(dllPath) + suffix;
#else
    return jk::fs::GetExecutablePath() + suffix;
#endif
}

std::string ExeDir() {
    // jk::fs::GetExecutablePath 흡수 (docs/68 W5). 원문 규약: 추출 실패 시
    // 빈값({}), 구분자 없을 시 ".", 뒤 "\\" 없음 — 그대로.
    const std::string exe = jk::fs::GetExecutablePath();
    if (exe.empty()) return {};
    const size_t slash = exe.find_last_of("\\/");
    return slash == std::string::npos ? std::string(".") : exe.substr(0, slash);
}

bool IsAbsolutePath(const std::string& p) {
    return (p.size() >= 2 && p[1] == ':') ||
           (!p.empty() && (p[0] == '\\' || p[0] == '/'));
}

bool ReadTextFile(const std::string& path, std::string& out) {
    FILE* f = nullptr;
    jk::crt::FopenS(&f, path.c_str(), "rb");
    if (!f) return false;
    char buf[4096];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

bool WriteTextFile(const std::string& path, const std::string& data) {
    FILE* f = nullptr;
    jk::crt::FopenS(&f, path.c_str(), "wb");
    if (!f) return false;
    const size_t w = std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    return w == data.size();
}

void EnsureParentDirs(const std::string& path) {
    size_t pos = 0;
    // CreateDirectoryA → std::filesystem::create_directory(stage-3 task 7).
    // 마지막 성분만 만드는 원문(이미 있으면 무해 no-op)과 동일 — ec 중립형,
    // 실패는 원문 bool 무시 규약대로 흘려보낸다(throwing 금지).
    while ((pos = path.find_first_of("\\/", pos + 1)) != std::string::npos) {
        std::error_code dirEc;
        std::filesystem::create_directory(
            std::filesystem::path(path.substr(0, pos)), dirEc);
    }
}

// docs/60 §2.1 — the workshop truth source. Seeded with a hello button when
// missing so a fresh install boots to something editable (by hand or by
// talking to the agent: "할일 판 만들어줘").
const char kTemplateScript[] =
    "// myapp.js — 워크숍 진실원 (docs/60 §2.1)\n"
    "// 이 파일이 곧 앱입니다. 메모장에서 고치거나, 에이전트에게 말로\n"
    "// 고치게 하세요 (폰 채팅: \"할일 판 만들어줘\"). 저장하면 즉시 리로드됩니다.\n"
    "// 능력 선언: 이 슬롯이 createButton/setText 등을 쓰면 슬롯 .jkx 출하 시\n"
    "// 사용량이 자동 분석돼 MANI capabilities= 로 선언된다(docs/74 —\n"
    "// \"capability '<tok>' not declared in MANI\"가 나오면 미선언 API 호출).\n"
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
// 능력 선언 원문 (docs/74 능력 게이트 — 배지 문구와 게이트 주입의 유일 원천).
std::string g_capabilities;

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
                g_capabilities = mani.capabilities;  // docs/74 능력 게이트
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
                // stage-3 task 7: 구분자 붙이는 문(fs::path 합성) — 윈은 '\',
                // 리눅스는 '/'로 플랫폼 몫(수기 '\\' 붙이기는 리눅스 경로
                // 성분 안에 백슬래시를 박는다).
                base = (std::filesystem::path(base) / g_scriptfile).string();
            }
            path = base;
        }
        // 시딩 출처 전환 (스펙 2026-10-05-slot-ship-tool §4 — 외부 진실원이
        // 이미 있으면 아무것도 안 한다(수신 기기 존중, 현행 불변), 부재 시
        // 파묻힌 SCRI(.app.js) 원문으로 시딩, 그것도 없으면 템플릿 회귀).
        std::string seedError;
        const int seeded = jk::WorkshopSeedScript(   // 스펙 slot-ship §4
            path, SideFilePath(".app.js"), kTemplateScript, seedError);
        if (seeded < 0) {
            std::fprintf(stderr, "[workshop] cannot seed '%s': %s\n",
                         path.c_str(), seedError.c_str());
            return 1;
        }

        jk::WorkshopScriptApp app;
        app.SetScriptInfo(meta->title, path);
        app.SetHotWatch(g_watch);
        app.SetAgentAppName(meta->name);
        // 능력 게이트 (docs/74 — 워크숍만): 빈값도 주입한다(선언 없음 =
        // 능력 없음, 배지가 그대로 보여 준다). ClientScriptApp 분기는
        // 주입하지 않는다 — 게이트 비활성 (결정 3).
        app.SetEnabledCapabilities(g_capabilities);
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