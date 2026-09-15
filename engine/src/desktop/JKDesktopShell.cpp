#include <desktop/JKDesktopShell.h>

#include "theme/JKTheme.h"

#include <JKImageLoader.h>
#include <JKJkxFile.h>

#include <quickjs.h>

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
#endif // _WIN32

namespace jk {
namespace desktop {

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
        launcherIcons_.push_back(icon);
    }
    if (!hasJkx("tetris")) {
        LauncherIcon icon;
        icon.appName = "tetris";
        launcherIcons_.push_back(icon);
    }
    // Phase A TUI 흡수 (docs/44): 콘솔 앱은 appName "terminal:<cmdline>" 관례로
    // 터미널 위에 띄운다 (SpawnClient가 접두사를 해석). 상대경로는 jkdesktop
    // cwd(engine/build) 기준. 전용 아이콘 아트는 이후 폴리싱 — fallback 아트.
    if (!hasJkx("terminal:apps-bin/lf/lf.exe")) {
        LauncherIcon icon;
        icon.appName = "terminal:apps-bin/lf/lf.exe";
        launcherIcons_.push_back(icon);
    }
    if (!hasJkx("terminal:apps-bin/helix/hx.exe")) {
        LauncherIcon icon;
        icon.appName = "terminal:apps-bin/helix/hx.exe";
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

        // Built-in apps: assets/icons/launcher_<pfx>; legacy cell layout kept
        // for them so the flat-placeholder fallback still matches by name.
        // terminal: 셀은 각 TUI 앱의 전용 아이콘 (docs/44).
        const char* base = "assets/icons/launcher_tetris";
        if (icon.appName == "minesweeper") {
            base = "assets/icons/launcher_mine";
        } else if (icon.appName == "terminal:apps-bin/lf/lf.exe") {
            base = "assets/icons/launcher_lf";
        } else if (icon.appName == "terminal:apps-bin/helix/hx.exe") {
            base = "assets/icons/launcher_helix";
        }
        icon.texture = LoadTextureScaled(base);
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
        icon.consoleDir = std::string("apps\\") + dirName;
        icon.consoleCmd = cmd;

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

bool JKDesktopShell::LaunchAt(int x, int y) {
    const int icon = HitTest(x, y);
    if (icon < 0) return false;
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

} // namespace desktop
} // namespace jk
