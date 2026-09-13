#include <desktop/JKDesktopShell.h>

#include "theme/JKTheme.h"

#include <JKImageLoader.h>
#include <JKJkxFile.h>

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
#endif // _WIN32

namespace jk {
namespace desktop {

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
        const char* base = (icon.appName == "minesweeper") ? "assets/icons/launcher_mine"
                                                           : "assets/icons/launcher_tetris";
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
    if (item.jkxPath.empty()) {
        if (host_.launch) host_.launch(item.appName.c_str(), false);
    } else {
        if (host_.launch) host_.launch(item.jkxPath.c_str(), /*fromJkx=*/true);
    }
    return true;
}

} // namespace desktop
} // namespace jk
