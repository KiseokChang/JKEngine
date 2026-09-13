#ifndef JKDESKTOPSHELL_H
#define JKDESKTOPSHELL_H

#include <JKTypes.h>
#include <SDL.h>

#include <functional>
#include <string>
#include <vector>

namespace jk {
struct LoadedImage;
}

namespace jk {
namespace desktop {

// In-process privileged shell (P1 ③, spec D7): the launcher desktop. Owns
// the icon grid, the background photo and the .jkx scan. Every service it
// needs from the server arrives through the injected ShellHost callbacks —
// this unit never includes jkserver headers, so a future out-of-process
// shell can reuse it as-is.
class JKDesktopShell {
public:
    struct ShellHost {
        SDL_Renderer* renderer = nullptr;
        std::function<float()> outputScale;
        std::function<SDL_Texture*(const jk::LoadedImage&, const char*)> makeTexture;
        std::function<void(const char*, bool)> launch;  // (appName, fromJkx)
    };

    // Scan apps/*.jkx, add built-in fallbacks, load the background photo,
    // lay the grid out and draw once (the per-frame draw happens via Draw
    // from Composite).
    void Init(const ShellHost& host);

    // Frame background painter — the server's Composite() calls this before
    // compositing layers. No-op with no icons (legacy empty-desktop behavior).
    void Draw(SDL_Renderer* renderer);

    void Destroy();

    // Physical-pixel hit test → launcher icon index, or -1.
    int HitTest(int x, int y) const;

    // Launcher click dispatch (server SDL mouse path): hit-tests (x, y) and,
    // on a hit, invokes the ShellHost launch callback with the icon's spawn
    // key ("--client" app name, or the "--jkx" container path). Returns true
    // when an icon was hit — the icon table is shell-private, so the server
    // never sees the index.
    bool LaunchAt(int x, int y);

private:
    struct LauncherIcon {
        JKRect rect;
        std::string appName;   // spawn key / display name
        std::string jkxPath;   // non-empty → spawn "--jkx <path>"
        SDL_Texture* texture = nullptr;
    };

    void ScanJkxApps();
    void RelayoutLauncherIcons();
    SDL_Texture* LoadTextureScaled(const char* assetBase);

    ShellHost host_;
    std::vector<LauncherIcon> launcherIcons_;
    SDL_Texture* backgroundTexture_ = nullptr;
};

} // namespace desktop
} // namespace jk

#endif // JKDESKTOPSHELL_H