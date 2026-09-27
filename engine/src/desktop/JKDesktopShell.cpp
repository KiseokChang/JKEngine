#include <desktop/JKDesktopShell.h>

#include "theme/JKTheme.h"

#include <JKDC.h>
#include <JKHangulManager.h>
#include <JKHangulUtil.h>
#include <JKImageLoader.h>
#include <JKJkxFile.h>
#include <JKResourceCache.h>
#include <JKSDLRenderBackend.h>
#include <JKTextAtlas.h>

#include <quickjs.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
// Minimal Windows API declarations for the .jkx app scan — the same local
// declaration style the server TU uses (full Windows headers conflict with
// legacy JKENGINE typedefs in other translation units).
extern "C" __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(
    void* hModule, char* lpFilename, unsigned long nSize);

// .jkx app discovery (ScanJkxApps).
struct JkxFindData {
    unsigned long dwFileAttributes = 0;
    unsigned long ftCreationTime[2] = {};
    unsigned long ftLastAccessTime[2] = {};
    unsigned long ftLastWriteTime[2] = {};
    unsigned long nFileSizeHigh = 0;
    unsigned long nFileSizeLow = 0;
    unsigned long dwReserved0 = 0;
    unsigned long dwReserved1 = 0;
    char cFileName[260] = {};
    char cAlternateFileName[14] = {};
};

extern "C" __declspec(dllimport) void* __stdcall FindFirstFileA(
    const char* lpFileName, JkxFindData* lpFindFileData);
extern "C" __declspec(dllimport) int __stdcall FindNextFileA(
    void* hFindFile, JkxFindData* lpFindFileData);
extern "C" __declspec(dllimport) int __stdcall FindClose(void* hFindFile);

// bcrypt (cmd 지문, P4 SDK §3.4) — jktriggers Sha256Hex와 동일 CNG 호출.
// long = NTSTATUS, wchar_t* = LPCWSTR (알고리즘 ID는 유니코드).
extern "C" __declspec(dllimport) long __stdcall BCryptOpenAlgorithmProvider(
    void** phAlgorithm, const wchar_t* pszAlgId, const wchar_t* pszImplementation,
    unsigned long dwFlags);
extern "C" __declspec(dllimport) long __stdcall BCryptCreateHash(
    void* hAlgorithm, void** phHash, unsigned char* pbHashObject,
    unsigned long cbHashObject, unsigned char* pbSecret, unsigned long cbSecret,
    unsigned long dwFlags);
extern "C" __declspec(dllimport) long __stdcall BCryptHashData(
    void* hHash, unsigned char* pbInput, unsigned long cbInput, unsigned long dwFlags);
extern "C" __declspec(dllimport) long __stdcall BCryptFinishHash(
    void* hHash, unsigned char* pbOutput, unsigned long cbOutput, unsigned long dwFlags);
extern "C" __declspec(dllimport) long __stdcall BCryptDestroyHash(void* hHash);
extern "C" __declspec(dllimport) long __stdcall BCryptCloseAlgorithmProvider(
    void* hAlgorithm, unsigned long dwFlags);
extern "C" __declspec(dllimport) int __stdcall CreateDirectoryA(
    const char* lpPathName, void* lpSecurityAttributes);
#endif // _WIN32

namespace jk {
namespace desktop {

JKDesktopShell::JKDesktopShell() = default;

// 툴팁 지연 부품(unique_ptr 멤버)은 전방선언 타입이라 소멸자를 cpp에서 정의.
JKDesktopShell::~JKDesktopShell() = default;

namespace {

// 콘솔 앱 매니페스트 읽기 상한 (JKTerminalConfig와 동일 — 1MiB는 매니페스트
// 로 넘치는 크기).
constexpr size_t kMaxManifestBytes = 1u << 20;

bool ReadFileBytes(const std::string& path, std::vector<uint8_t>& out) {
#ifdef _WIN32
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0) return false;
#else
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
#endif
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0 || static_cast<size_t>(size) > kMaxManifestBytes) {
        std::fclose(f);
        return false;
    }
    out.resize(static_cast<size_t>(size) + 1);  // + NUL: JS_ParseJSON requires
    const size_t read = std::fread(out.data(), 1, static_cast<size_t>(size), f);
    std::fclose(f);
    if (read != static_cast<size_t>(size)) return false;
    out[static_cast<size_t>(size)] = '\0';
    return true;
}

// 콘솔 앱 스폰 cmd 지문 (P4 SDK §3.4): docs/37 스킴 재사용 — jktriggers
// Sha256Hex와 동일 형식("sha256:"+64hex)/동일 CNG 구현. 빈 문자열은 실패.
std::string ConsoleAppFingerprint(const std::string& cmd) {
#ifdef _WIN32
    void* alg = nullptr;
    // BCRYPT_SHA256_ALGORITHM == L"SHA256"
    if (BCryptOpenAlgorithmProvider(&alg, L"SHA256", nullptr, 0) != 0) return "";
    void* h = nullptr;
    uint8_t digest[32] = {};
    bool ok = BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0) == 0;
    if (ok && !cmd.empty())
        ok = BCryptHashData(h, (unsigned char*)cmd.data(),
                            (unsigned long)cmd.size(), 0) == 0;
    if (ok) ok = BCryptFinishHash(h, digest, sizeof(digest), 0) == 0;
    if (h) BCryptDestroyHash(h);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (!ok) return "";
    static const char* kHex = "0123456789abcdef";
    std::string out = "sha256:";
    for (uint8_t b : digest) {
        out += kHex[b >> 4];
        out += kHex[b & 0xf];
    }
    return out;
#else
    (void)cmd;
    return "";
#endif
}

// trust ledger 보증 (P4 SDK §3.4): 스폰 cmd 지문을 state\trust.json에 upsert
// (docs/37 형식 — jktriggers SaveTrustRecords와 동일 레이아웃). 서버는 시작
// 때 한 번만 쓴다. 이미 같은 지문이 있으면 파일을 건드리지 않는다(읽기-수정
// -쓰기 경쟁 최소화). 파손된 스토어는 덮어쓰지 않는다 — 기록 보존이 우선.
void EnsureTrustRecord(const std::string& fingerprint, const std::string& name) {
#ifdef _WIN32
    char exePath[1024] = {};
    if (!GetModuleFileNameA(nullptr, exePath, sizeof(exePath))) return;
    std::string exeDir = exePath;
    const size_t slash = exeDir.find_last_of("\\/");
    if (slash == std::string::npos) return;
    exeDir.resize(slash);
    CreateDirectoryA((exeDir + "\\state").c_str(), nullptr);
    const std::string path = exeDir + "\\state\\trust.json";

    std::vector<uint8_t> bytes;
    std::vector<std::string> recs;  // 재조립용 레코드 JSON 문자열
    bool corrupt = false;
    if (ReadFileBytes(path, bytes)) {
        JSRuntime* rt = JS_NewRuntime();
        if (!rt) return;
        JSContext* ctx = JS_NewContext(rt);
        if (!ctx) {
            JS_FreeRuntime(rt);
            return;
        }
        JSValue root = JS_ParseJSON(ctx, reinterpret_cast<const char*>(bytes.data()),
                                    bytes.size() - 1, "trust.json");
        if (JS_IsException(root)) {
            std::fprintf(stderr, "JKWindowServer: trust store corrupt — console app '%s' not recorded\n",
                         name.c_str());
            JS_FreeValue(ctx, root);
            JS_FreeContext(ctx);
            JS_FreeRuntime(rt);
            return;
        }
        if (JS_IsObject(root)) {
            JSValue arr = JS_GetPropertyStr(ctx, root, "records");
            if (JS_IsArray(arr)) {
                JSValue lenVal = JS_GetPropertyStr(ctx, arr, "length");
                int32_t len = 0;
                JS_ToInt32(ctx, &len, lenVal);
                JS_FreeValue(ctx, lenVal);
                for (int32_t i = 0; i < len; ++i) {
                    JSValue item = JS_GetPropertyUint32(ctx, arr, (uint32_t)i);
                    auto str = [&](const char* key) {
                        JSValue v = JS_GetPropertyStr(ctx, item, key);
                        std::string s;
                        if (JS_IsString(v)) {
                            size_t l = 0;
                            const char* c = JS_ToCStringLen(ctx, &l, v);
                            if (c) s.assign(c, l);
                            JS_FreeCString(ctx, c);
                        }
                        JS_FreeValue(ctx, v);
                        return s;
                    };
                    const std::string fp = str("fingerprint");
                    const std::string nm = str("name");
                    const std::string src = str("source");
                    JSValue tsVal = JS_GetPropertyStr(ctx, item, "ts");
                    int64_t ts = 0;
                    JS_ToInt64(ctx, &ts, tsVal);
                    JS_FreeValue(ctx, tsVal);
                    if (fp == fingerprint) {
                        // 이미 기록된 지문 — 스토어 변경 없이 끝낸다.
                        JS_FreeValue(ctx, item);
                        JS_FreeValue(ctx, arr);
                        JS_FreeValue(ctx, root);
                        JS_FreeContext(ctx);
                        JS_FreeRuntime(rt);
                        return;
                    }
                    if (!fp.empty()) {
                        recs.push_back("{\"fingerprint\":\"" + fp + "\",\"name\":\"" +
                                       nm + "\",\"source\":\"" + src +
                                       "\",\"ts\":" + std::to_string(ts) + "}");
                    }
                    JS_FreeValue(ctx, item);
                }
            }
            JS_FreeValue(ctx, arr);
        }
        JS_FreeValue(ctx, root);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }

    // 새 레코드 append (source "user" — jktriggers 로더와 동일 역할 표기).
    const uint64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now().time_since_epoch())
                               .count();
    recs.push_back("{\"fingerprint\":\"" + fingerprint + "\",\"name\":\"" + name +
                   "\",\"source\":\"user\",\"ts\":" + std::to_string(nowMs) + "}");
    std::string out = "{\"records\":[";
    for (size_t i = 0; i < recs.size(); ++i) {
        if (i) out += ",";
        out += recs[i];
    }
    out += "]}";

    FILE* wf = nullptr;
    if (fopen_s(&wf, path.c_str(), "wb") == 0 && wf) {
        std::fwrite(out.data(), 1, out.size(), wf);
        std::fclose(wf);
        std::fprintf(stderr, "JKWindowServer: console app cmd fingerprint recorded (%s, '%s')\n",
                     fingerprint.substr(0, 15).c_str(), name.c_str());
    }
#else
    (void)fingerprint;
    (void)name;
#endif
}

} // namespace

// Load a PNG asset pair ("<base>@1x.png" / "@2x.png") — @2x when the
// output scale is >= 1.5 — into a blended SDL texture. Returns nullptr
// when the asset is missing (callers fall back to flat drawing).
SDL_Texture* JKDesktopShell::LoadTextureScaled(const char* assetBase) {
    if (!host_.renderer) return nullptr;

    // Pick the @2x asset when the display scale is high enough for the extra
    // pixels to pay off (mixed-DPI rule: renderer ratio drives the choice).
    const float s = host_.outputScale ? host_.outputScale() : 1.0f;
    char path[512];
    std::snprintf(path, sizeof(path), "%s@%s.png", assetBase, s >= 1.5f ? "2x" : "1x");

    jk::LoadedImage img;
    if (!jk::LoadImageFile(jk::ResolveAssetPath(path), img)) {
        return nullptr;
    }
    return host_.makeTexture(img, path);
}

void JKDesktopShell::Init(const ShellHost& host) {
    host_ = host;
    if (!host_.renderer) return;

    launcherIcons_.clear();

    // Installed .jkx containers first (Phase C): one launcher cell per
    // apps/<name>.jkx, icon decoded from the container itself.
    ScanJkxApps();
    ScanConsoleApps();

    // Built-in process-mode fallback for apps that have no .jkx installed.
    auto hasJkx = [this](const char* name) {
        for (const auto& icon : launcherIcons_) {
            if (icon.appName == name) return true;
        }
        return false;
    };
    if (!hasJkx("minesweeper")) {
        LauncherIcon icon;
        icon.appName = "minesweeper";
        icon.title = "Minesweeper";
        launcherIcons_.push_back(icon);
    }
    if (!hasJkx("tetris")) {
        LauncherIcon icon;
        icon.appName = "tetris";
        icon.title = "Tetris";
        launcherIcons_.push_back(icon);
    }
    // Phase A TUI 흡수 (docs/44): 콘솔 앱은 appName "terminal:<cmdline>" 관례로
    // 터미널 위에 띄운다 (SpawnClient가 접두사를 해석). 상대경로는 jkdesktop
    // cwd(engine/build) 기준. 전용 아이콘 아트는 이후 폴리싱 — fallback 아트.
    // 툴팁 표시명은 exe 이름(접두어·경로 제거).
    if (!hasJkx("terminal:apps-bin/lf/lf.exe")) {
        LauncherIcon icon;
        icon.appName = "terminal:apps-bin/lf/lf.exe";
        icon.title = "lf";
        launcherIcons_.push_back(icon);
    }
    if (!hasJkx("terminal:apps-bin/helix/hx.exe")) {
        LauncherIcon icon;
        icon.appName = "terminal:apps-bin/helix/hx.exe";
        icon.title = "hx";
        launcherIcons_.push_back(icon);
    }

    // Desktop background photo + launcher icon art (PNG assets, see
    // ARCHITECTURE_DOCS/20). Missing assets fall back to the flat placeholder.
    if (backgroundTexture_) {
        SDL_DestroyTexture(backgroundTexture_);
        backgroundTexture_ = nullptr;
    }
    backgroundTexture_ = LoadTextureScaled("assets/backgrounds/desktop");

    for (auto& icon : launcherIcons_) {
        if (icon.texture) continue;   // .jkx apps carry their own icon texture

        // Per-app icon art: assets/icons/launcher_<appName>. .jkx 앱이 컨테이너
        // ICON 엔트리를 빠뜨렸거나 콘솔 앱에 icon@{2x,1x}.png가 없어도 전용
        // 아이콘 PNG만 놓아두면 테트리스 공용 플레이스홀더에 겹치지 않는다.
        // terminal: 셀은 각 TUI 앱의 전용 아이콘 (docs/44). 아트가 없으면
        // 테트리스 플레이스홀더로 폴백.
        char nameBase[160];
        const char* base = "assets/icons/launcher_tetris";
        bool hasDedicated = false;
        if (icon.appName == "minesweeper") {
            base = "assets/icons/launcher_mine";
            hasDedicated = true;
        } else if (icon.appName == "terminal:apps-bin/lf/lf.exe") {
            base = "assets/icons/launcher_lf";
            hasDedicated = true;
        } else if (icon.appName == "terminal:apps-bin/helix/hx.exe") {
            base = "assets/icons/launcher_helix";
            hasDedicated = true;
        } else {
            // appName은 .jkx 매니페스트 문자열이라 경로 특수문자 방지 — 안전한
            // 이름만 자산 경로로 쓴다.
            bool safe = !icon.appName.empty() && icon.appName.size() < 100;
            for (char c : icon.appName) {
                const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                (c >= '0' && c <= '9') || c == '_';
                if (!ok) { safe = false; break; }
            }
            if (safe) {
                std::snprintf(nameBase, sizeof(nameBase), "assets/icons/launcher_%s",
                              icon.appName.c_str());
                base = nameBase;
                hasDedicated = true;
            }
        }
        icon.texture = LoadTextureScaled(base);
        if (!icon.texture && hasDedicated) {
            icon.texture = LoadTextureScaled("assets/icons/launcher_tetris");
        }
        if (icon.texture) {
            std::fprintf(stderr, "JKWindowServer: launcher icon '%s' loaded\n", base);
        }
    }

    // Grid layout: one source of truth for cell rects, wrapping to the window
    // width (the single row overflowed the 1280px desktop at 13 .jkx apps).
    RelayoutLauncherIcons();
    Draw(host_.renderer);
}

// Launcher cell grid (docs/21 §2): wraps cells into multiple rows so a
// growing app list stays on screen. Cells sit 100x100 apart starting at
// (50, 50); the column count derives from the window's logical width.
void JKDesktopShell::RelayoutLauncherIcons() {
    if (!host_.renderer) return;
    const float s = host_.outputScale ? host_.outputScale() : 1.0f;
    int pw = 1280;
    int ph = 720;
    SDL_GetRendererOutputSize(host_.renderer, &pw, &ph);
    const int logicalW = static_cast<int>(pw / s);
    int cols = (logicalW - 50) / 100;  // (left margin .. right edge) / pitch
    if (cols < 1) cols = 1;
    for (size_t i = 0; i < launcherIcons_.size(); ++i) {
        const int col = static_cast<int>(i) % cols;
        const int row = static_cast<int>(i) / cols;
        launcherIcons_[i].rect = JKRect{ 50 + col * 100, 50 + row * 100, 64, 80 };
    }
}

void JKDesktopShell::ScanJkxApps() {
#ifdef _WIN32
    // Enumerate <exe-dir>/apps/*.jkx. Icon textures are decoded from the
    // container's ICON entries (no temp files); spawning uses --jkx <path>.
    char basePath[1024] = {};
    if (!GetModuleFileNameA(nullptr, basePath, sizeof(basePath))) return;
    char* lastSlash = basePath;
    for (char* p = basePath; *p; ++p) {
        if (*p == '\\' || *p == '/') lastSlash = p;
    }
    *lastSlash = '\0';

    char pattern[1024];
    std::snprintf(pattern, sizeof(pattern), "%s\\apps\\*.jkx", basePath);
    JkxFindData fd{};
    void* find = FindFirstFileA(pattern, &fd);
    if (!find) return;

    const float s = host_.outputScale ? host_.outputScale() : 1.0f;

    do {
        char path[1024];
        std::snprintf(path, sizeof(path), "%s\\apps\\%s", basePath, fd.cFileName);

        jk::JKJkxFile jkx;
        if (!jkx.Open(path)) continue;
        const jk::JkxManifest& mani = jkx.Manifest();
        if (mani.name.empty()) continue;

        LauncherIcon icon;
        icon.appName = mani.name;
        // 툴팁 표시명: 매니페스트 title 우선, 없으면 스폰 키.
        icon.title = !mani.title.empty() ? mani.title : mani.name;
        icon.jkxPath = path;

        // Icon entry: prefer @2x on high-scale displays.
        std::string wanted = (s >= 1.5f && !mani.icon2x.empty()) ? mani.icon2x : mani.icon;
        if (wanted.empty()) wanted = !mani.icon2x.empty() ? mani.icon2x : mani.icon;
        const int entry = wanted.empty() ? -1 : jkx.FindEntry("ICON", wanted);
        std::vector<uint8_t> png;
        jk::LoadedImage img;
        if (entry >= 0 && jkx.ReadEntry(entry, png) &&
            jk::LoadImageMemory(png.data(), png.size(), img)) {
            icon.texture = host_.makeTexture(img, mani.name.c_str());
        }

        launcherIcons_.push_back(std::move(icon));
        std::fprintf(stderr, "JKWindowServer: installed app '%s' from %s (icon %s)\n",
                     mani.name.c_str(), fd.cFileName,
                     launcherIcons_.back().texture ? "decoded" : "missing");
    } while (FindNextFileA(find, &fd));
    FindClose(find);
#endif // _WIN32
}

// 콘솔 앱 스캔 (P4 SDK, specs/2026-09-16-p4-sdk-contract §3): apps/<name>/
// manifest.json 디렉터리를 .jkx와 같은 스캔 대상으로 받는다. JSON 파싱은
// quickjs throwaway 런타임(JKTerminalConfig::Load 패턴). .jkx와 이름이
// 겹치면 .jkx가 이긴다(스캔 순서 + 명시적 스킵).
void JKDesktopShell::ScanConsoleApps() {
#ifdef _WIN32
    char basePath[1024] = {};
    if (!GetModuleFileNameA(nullptr, basePath, sizeof(basePath))) return;
    char* lastSlash = basePath;
    for (char* p = basePath; *p; ++p) {
        if (*p == '\\' || *p == '/') lastSlash = p;
    }
    *lastSlash = '\0';

    char pattern[1024];
    std::snprintf(pattern, sizeof(pattern), "%s\\apps\\*", basePath);
    JkxFindData fd{};
    void* find = FindFirstFileA(pattern, &fd);
    if (!find) return;

    constexpr unsigned long kDir = 0x10;  // FILE_ATTRIBUTE_DIRECTORY
    do {
        if (!(fd.dwFileAttributes & kDir) || fd.cFileName[0] == '.') continue;

        const std::string dirName = fd.cFileName;
        const std::string manifestPath =
            std::string(basePath) + "\\apps\\" + dirName + "\\manifest.json";
        std::vector<uint8_t> bytes;
        if (!ReadFileBytes(manifestPath, bytes)) continue;  // no manifest → not a console app

        // Throwaway runtime — parse + field extraction, then it's gone.
        JSRuntime* rt = JS_NewRuntime();
        if (!rt) continue;
        JSContext* ctx = JS_NewContext(rt);
        if (!ctx) {
            JS_FreeRuntime(rt);
            continue;
        }
        JSValue root = JS_ParseJSON(ctx, reinterpret_cast<const char*>(bytes.data()),
                                    bytes.size() - 1, "manifest.json");
        std::string name, cmd, desc;
        if (!JS_IsException(root) && JS_IsObject(root)) {
            auto getString = [&](const char* key, std::string* field) {
                JSValue v = JS_GetPropertyStr(ctx, root, key);
                if (JS_IsString(v)) {
                    size_t len = 0;
                    const char* s = JS_ToCStringLen(ctx, &len, v);
                    if (s) {
                        *field = std::string(s, len);
                        JS_FreeCString(ctx, s);
                    }
                }
                JS_FreeValue(ctx, v);
            };
            getString("name", &name);
            getString("cmd", &cmd);
            getString("desc", &desc);
        }
        JS_FreeValue(ctx, root);
        if (ctx) JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        if (name.empty() || cmd.empty()) continue;

        // .jkx 우선: 같은 이름의 컨테이너가 이미 있으면 매니페스트 앱은 스킵.
        bool taken = false;
        for (const auto& icon : launcherIcons_) taken |= (icon.appName == name);
        if (taken) {
            std::fprintf(stderr, "JKWindowServer: console app '%s' skipped (.jkx wins)\n",
                         name.c_str());
            continue;
        }

        LauncherIcon icon;
        icon.appName = name;
        // 툴팁 표시명: manifest.json desc가 있으면 그걸 쓴다(이름보다 정보량).
        icon.title = desc;
        icon.consoleDir = std::string("apps\\") + dirName;
        icon.consoleCmd = cmd;

        // 스폰 cmd 지문 보증 (P4 SDK §3.4, docs/37 형식). 이미 있으면
        // EnsureTrustRecord가 파일을 건드리지 않는다.
        const std::string fp = ConsoleAppFingerprint(cmd);
        if (!fp.empty()) EnsureTrustRecord(fp, name);

        // 아이콘(선택): apps/<name>/icon@{2x,1x}.png — 없으면 placeholder 사각형.
        const float s = host_.outputScale ? host_.outputScale() : 1.0f;
        const char* pick = s >= 1.5f ? "icon@2x.png" : "icon@1x.png";
        char iconPath[1024];
        std::snprintf(iconPath, sizeof(iconPath), "%s\\apps\\%s\\%s", basePath,
                      dirName.c_str(), pick);
        jk::LoadedImage img;
        if (jk::LoadImageFile(iconPath, img)) {
            icon.texture = host_.makeTexture(img, name.c_str());
        }

        launcherIcons_.push_back(std::move(icon));
        std::fprintf(stderr,
                     "JKWindowServer: console app '%s' (cmd='%s', dir='%s', icon %s)\n",
                     name.c_str(), cmd.c_str(),
                     launcherIcons_.back().consoleDir.c_str(),
                     launcherIcons_.back().texture ? "decoded" : "missing");
    } while (FindNextFileA(find, &fd));
    FindClose(find);
#endif // _WIN32
}

// Frame background painter (formerly DrawLauncherBackground): this only
// draws — the compositor calls SDL_RenderPresent once per frame.
void JKDesktopShell::Draw(SDL_Renderer* renderer) {
    if (!renderer || launcherIcons_.empty()) return;

    // All server drawing is in physical pixels. icon.rect is stored in SDL
    // logical points, so multiply by the compositor output scale to get the
    // physical-pixel rect. The mouse hit-test uses the same physical rect.
    const float s = host_.outputScale ? host_.outputScale() : 1.0f;

    const auto& t = jk::theme::current();
    SDL_SetRenderDrawColor(renderer, t.desktopBgFallback.r, t.desktopBgFallback.g, t.desktopBgFallback.b, 255);
    SDL_RenderClear(renderer);

    // Desktop background photo stretched to the full window.
    if (backgroundTexture_) {
        int pw = 0;
        int ph = 0;
        SDL_GetRendererOutputSize(renderer, &pw, &ph);
        SDL_Rect dst{ 0, 0, pw, ph };
        SDL_RenderCopy(renderer, backgroundTexture_, nullptr, &dst);
    }

    for (const auto& icon : launcherIcons_) {
        SDL_Rect rc{
            static_cast<int>(icon.rect.x * s),
            static_cast<int>(icon.rect.y * s),
            static_cast<int>(icon.rect.w * s),
            static_cast<int>(icon.rect.h * s),
        };
        if (icon.texture) {
            // Square icon art in the top part of the 64x80 cell; the rest of
            // the cell is label space (the server has no text renderer).
            SDL_Rect art{
                rc.x,
                rc.y,
                static_cast<int>(icon.rect.w * s),
                static_cast<int>(icon.rect.w * s),
            };
            SDL_RenderCopy(renderer, icon.texture, nullptr, &art);
        } else {
            if (icon.appName == "minesweeper") {
                SDL_SetRenderDrawColor(renderer, t.launcherCellPlaceholder.r, t.launcherCellPlaceholder.g, t.launcherCellPlaceholder.b, 255);
            } else if (icon.appName == "tetris") {
                // 의도적 리터럴 잔존 — 앱 식별색 (P2 테마 스왑 제외).
                SDL_SetRenderDrawColor(renderer, 128, 0, 128, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, t.launcherCellFace.r, t.launcherCellFace.g, t.launcherCellFace.b, 255);
            }
            SDL_RenderFillRect(renderer, &rc);
            SDL_SetRenderDrawColor(renderer, t.launcherCellOutline.r, t.launcherCellOutline.g, t.launcherCellOutline.b, 255);
            SDL_RenderDrawRect(renderer, &rc);
        }
    }

    // 호버 툴팁 (docs/67 후속): 300ms 머문 아이콘 셀 위에 마지막으로 얹는다 —
    // 아이콘·배경 위에 항상 깔리도록 그리기 순서상 맨 뒤. 활성화 판정은 여기서
    // (정지 커서 승격 — UpdateHover 주석 참조).
    if (hoverIndex_ >= 0 && !hoverActive_ &&
        SDL_GetTicks() - hoverStartMs_ >= kTooltipHoverDelayMs) {
        hoverActive_ = true;
    }
    if (hoverActive_ && hoverIndex_ >= 0 &&
        hoverIndex_ < static_cast<int>(launcherIcons_.size())) {
        DrawTooltip(renderer, launcherIcons_[static_cast<size_t>(hoverIndex_)]);
    }
}

void JKDesktopShell::Destroy() {
    for (auto& icon : launcherIcons_) {
        if (icon.texture) {
            SDL_DestroyTexture(icon.texture);
            icon.texture = nullptr;
        }
    }
    launcherIcons_.clear();
    if (backgroundTexture_) {
        SDL_DestroyTexture(backgroundTexture_);
        backgroundTexture_ = nullptr;
    }
    // 툴팁 텍스처 캐시 + 렌더 지연 부품 (캐시가 소유 백엔드에 의존하므로
    // 텍스처 파괴 후 플러시 순 — 승인 배너 정리 경로 선례).
    for (auto& kv : tooltipTexs_) {
        if (kv.second.tex) {
            SDL_DestroyTexture(kv.second.tex);
            kv.second.tex = nullptr;
        }
    }
    tooltipTexs_.clear();
    ClearHover();
    if (tooltipCache_ && tooltipBackend_) {
        tooltipCache_->UnloadAllImages();
        tooltipCache_->FlushUploads(tooltipBackend_.get());
    }
    tooltipAtlas_.reset();
    tooltipCache_.reset();
    tooltipBackend_.reset();
    tooltipFont_.reset();
}

int JKDesktopShell::HitTest(int x, int y) const {
    // (x, y) are physical client px. icon.rect is in logical points.
    const float s = host_.outputScale ? host_.outputScale() : 1.0f;
    for (size_t i = 0; i < launcherIcons_.size(); ++i) {
        const auto& r = launcherIcons_[i].rect;
        const int rx = static_cast<int>(r.x * s);
        const int ry = static_cast<int>(r.y * s);
        const int rw = static_cast<int>(r.w * s);
        const int rh = static_cast<int>(r.h * s);
        if (x >= rx && x < rx + rw && y >= ry && y < ry + rh) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// 마우스 호버 툴팁 (docs/67 후속): 서버 마우스 경로가 모션마다 히트 인덱스를
// 중계한다. 인덱스 변화(이동·벗어남)는 지연 타이머를 리셋한다. 활성화 판정은
// Draw()가 매 프레임 한다 — SDL 모션은 커서가 움직일 때만 도착하므로 정지
// 커서는 추가 모션 없이 머무름만으로 300ms 경과 후 툴팁이 떠야 한다.
void JKDesktopShell::UpdateHover(int hitIndex) {
    if (hitIndex != hoverIndex_) {
        hoverIndex_ = hitIndex;
        hoverStartMs_ = SDL_GetTicks();
        hoverActive_ = false;
    }
}

void JKDesktopShell::ClearHover() {
    hoverIndex_ = -1;
    hoverActive_ = false;
}

SDL_Texture* JKDesktopShell::TooltipTexture(const std::string& utf8, int* w, int* h) {
    *w = 0;
    *h = 0;
    auto it = tooltipTexs_.find(utf8);
    if (it != tooltipTexs_.end()) {
        *w = it->second.w;
        *h = it->second.h;
        return it->second.tex;
    }
    if (!host_.renderer || !SDL_RenderTargetSupported(host_.renderer)) {
        // 렌더 타깃 미지원 백엔드 — 툴팁 없이 계속(사실상 안 쓰는 가지, 배너 선례).
        tooltipTexs_[utf8] = TooltipTex{};
        return nullptr;
    }
    // 렌더 지연 부품 (승인 배너 ApprovalBannerTexture 선례): 한글 매니저는
    // 생성 실패해도 JKDC 내장 ASCII 폴백으로 계속. 캐시는 등록 경로와 소멸자
    // 플러시가 소유 백엔드에 의존하므로 수명까지 소유한다.
    if (!tooltipFont_) {
        tooltipFont_ = std::make_unique<HangulManager>();
    }
    if (!tooltipBackend_) {
        tooltipBackend_ = std::make_unique<JKSDLRenderBackend>(host_.renderer);
    }
    if (!tooltipCache_) {
        tooltipCache_ = std::make_unique<JKResourceCache>(tooltipBackend_.get());
    }
    if (!tooltipAtlas_) {
        tooltipAtlas_ = std::make_unique<jk::JKTextAtlas>();
        // 셀 메트릭 진실원 (docs/63 §6 text.font_scale).
        const jk::text::CellMetrics m = jk::text::GetCellMetrics();
        const std::string fp = jk::text::ResolveDesktopFontPath();
        if (fp.empty()) {
            std::fprintf(stderr,
                         "[shell] tooltip: no vector font configured, staying "
                         "on bitmap glyphs\n");
        } else if (!tooltipAtlas_->Init(fp, m.engW, m.cellH, m.hanW)) {
            std::fprintf(stderr,
                         "[shell] tooltip: vector font init failed (%s), "
                         "staying on bitmap glyphs\n",
                         fp.c_str());
        } else {
            const std::string fb = jk::text::ResolveDesktopFallbackPath();
            if (!fb.empty() && !tooltipAtlas_->InitFallback(fb)) {
                std::fprintf(stderr,
                             "[shell] tooltip: fallback font init failed (%s), "
                             "glyph chain disabled\n",
                             fb.c_str());
            }
        }
    }
    // 크롬 타이틀 LegacyFontTitle 선례: KSSM 변환이 빈 결과면 원문을 쓴다.
    std::string kssm = Utf8ToKssm(utf8.c_str());
    if (kssm.empty()) {
        kssm = utf8;
    }
    const JKPoint m = JKDC::MeasureText(kssm.c_str());
    constexpr int kPad = 5;
    const int texW = m.x + kPad * 2;
    const int texH = (m.y > 0 ? m.y : 16) + kPad * 2;
    SDL_Texture* tex = SDL_CreateTexture(host_.renderer, SDL_PIXELFORMAT_RGBA8888,
                                         SDL_TEXTUREACCESS_TARGET, texW, texH);
    if (!tex) {
        tooltipTexs_[utf8] = TooltipTex{};
        return nullptr;
    }
    SDL_Texture* prev = SDL_GetRenderTarget(host_.renderer);
    SDL_SetRenderTarget(host_.renderer, tex);
    // 어두운 툴팁 박스 + 밝은 테두리 — 데스크톱 사진 위에서도 읽히는 고정 대비색.
    SDL_SetRenderDrawColor(host_.renderer, 26, 26, 30, 255);
    SDL_RenderClear(host_.renderer);
    SDL_SetRenderDrawColor(host_.renderer, 130, 130, 140, 255);
    SDL_Rect border{ 0, 0, texW, texH };
    SDL_RenderDrawRect(host_.renderer, &border);
    JKSDLRenderBackend backend(host_.renderer);
    JKDC dc(&backend);
    if (tooltipFont_) {
        dc.SetHangulManager(tooltipFont_.get());
    }
    // 벡터 글리프 장착 — Init 실패(IsLoaded()==false)면 비트맵 폴백이 그대로.
    if (tooltipAtlas_ && tooltipAtlas_->IsLoaded() && tooltipCache_) {
        dc.SetTextAtlas(tooltipAtlas_.get(), tooltipCache_.get());
    }
    dc.SetTextColor(235, 235, 240);
    dc.TextOut(JKPoint{kPad, kPad}, kssm.c_str());
    SDL_SetRenderTarget(host_.renderer, prev);
    tooltipTexs_[utf8] = TooltipTex{tex, texW, texH};
    // 프로브 단정 지점: 표시명별 텍스처는 1회만 렌더되므로 로그에도 1회만
    // 찍힌다(캐시 적중 재호버는 무로그).
    std::fprintf(stderr, "[shell] tooltip: texture '%s' rendered (%dx%d)\n",
                 utf8.c_str(), texW, texH);
    *w = texW;
    *h = texH;
    return tex;
}

void JKDesktopShell::DrawTooltip(SDL_Renderer* renderer, const LauncherIcon& icon) {
    const float s = host_.outputScale ? host_.outputScale() : 1.0f;
    const std::string& label = !icon.title.empty() ? icon.title : icon.appName;
    if (label.empty()) return;
    int tw = 0;
    int th = 0;
    SDL_Texture* tex = TooltipTexture(label, &tw, &th);
    if (!tex || tw <= 0 || th <= 0) return;

    // 셀 아래 4pt 간격(논리 좌표). 데스크톱 바닥에 닿으면 셀 위로 뒤집고,
    // 오른쪽/왼쪽 클램프로 화면 밖을 막는다. 최종 좌표·크기는 물리 픽셀 —
    // Draw()의 나머지(rect × outputScale)와 같은 산식. 텍스처는 셀 메트릭
    // 크기라 스케일 >1에선 늘어나 그려진다(승인 배너의 raw 드로잉과 달리
    // 주변 UI와 크기를 맞춘다).
    int pw = 0;
    int ph = 0;
    SDL_GetRendererOutputSize(renderer, &pw, &ph);
    const int dw = static_cast<int>(tw * s);
    const int dh = static_cast<int>(th * s);
    int px = static_cast<int>(icon.rect.x * s);
    int py = static_cast<int>((icon.rect.y + icon.rect.h + 4) * s);
    if (px + dw > pw) px = pw - dw;
    if (px < 0) px = 0;
    if (py + dh > ph) {
        py = static_cast<int>(icon.rect.y * s) - dh - 4;
    }
    if (py < 0) py = 0;
    SDL_Rect dst{ px, py, dw, dh };
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
}

bool JKDesktopShell::LaunchAt(int x, int y) {
    const int icon = HitTest(x, y);
    if (icon < 0) return false;
    ClearHover();  // 실행 직후 툴팁 즉시 숨김
    const LauncherIcon& item = launcherIcons_[static_cast<size_t>(icon)];
    if (!item.consoleCmd.empty()) {
        // 콘솔 앱 kind (P4 SDK §3): 터미널 위에 스폰 — cwd는 앱 폴더.
        if (host_.spawnConsole) {
            host_.spawnConsole(item.consoleCmd, item.consoleDir, item.appName);
        }
        return true;
    }
    if (item.jkxPath.empty()) {
        if (host_.launch) host_.launch(item.appName.c_str(), false);
    } else {
        if (host_.launch) host_.launch(item.jkxPath.c_str(), /*fromJkx=*/true);
    }
    return true;
}

bool JKDesktopShell::ConsoleAppInfo(const std::string& name, std::string& cmd,
                                    std::string& dir, std::string& fingerprint) const {
    for (const auto& icon : launcherIcons_) {
        if (icon.consoleCmd.empty() || icon.appName != name) continue;
        cmd = icon.consoleCmd;
        dir = icon.consoleDir;
        fingerprint = ConsoleAppFingerprint(cmd);
        return true;
    }
    return false;
}

// 에이전트 관리자 (스펙 §2.4): 런처 스캔 결과의 이름/kind만 돌려준다.
void JKDesktopShell::ListInstalled(
        std::vector<std::pair<std::string, const char*>>& out) const {
    for (const LauncherIcon& icon : launcherIcons_) {
        const char* kind = !icon.consoleCmd.empty()
            ? "console"
            : (!icon.jkxPath.empty() ? "jkx" : "builtin");
        out.emplace_back(icon.appName, kind);
    }
}

} // namespace desktop
} // namespace jk
