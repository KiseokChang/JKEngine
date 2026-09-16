#ifndef JKDESKTOPSHELL_H
#define JKDESKTOPSHELL_H

#include <JKTypes.h>
#include <SDL.h>

#include <functional>
#include <string>
#include <utility>
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
        // 콘솔 앱 스폰 (P4 SDK, specs/2026-09-16-p4-sdk-contract §3): 터미널
        // 위에 cmd를 띄운다 (cwd = 앱 폴더, 상대경로).
        std::function<void(const std::string& cmd, const std::string& cwd,
                           const std::string& name)> spawnConsole;
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

    // 콘솔 앱 조회 (P4 SDK §5): 설치된 매니페스트 앱의 스폰 cmd/디렉토리/
    // cmd 지문을 돌려준다. 매니페스트 앱이 아니면 false. 에이전트
    // run_console_app 도구가 재사용한다.
    bool ConsoleAppInfo(const std::string& name, std::string& cmd,
                        std::string& dir, std::string& fingerprint) const;

    // 에이전트 관리자 (specs/2026-09-16-agent-manager §2.4): 설치 앱의
    // 이름/kind 열람. kind = "console"(consoleCmd 비지 않음) | "jkx" |
    // "builtin". cmd/지문은 run_console_app 승인 경로의 관심사 — 최소 노출.
    void ListInstalled(
        std::vector<std::pair<std::string, const char*>>& out) const;

private:
    struct LauncherIcon {
        JKRect rect;
        std::string appName;   // spawn key / display name
        std::string jkxPath;   // non-empty → spawn "--jkx <path>"
        // 콘솔 앱 kind (P4 SDK §3): 비었으면 .jkx/내장 앱 셀. cmd는 서버
        // cwd(engine/build) 기준 상대경로 — 인용 겹침 방지(453a327).
        std::string consoleCmd;
        std::string consoleDir;
        SDL_Texture* texture = nullptr;
    };

    void ScanJkxApps();
    void ScanConsoleApps();
    void RelayoutLauncherIcons();
    SDL_Texture* LoadTextureScaled(const char* assetBase);

    ShellHost host_;
    std::vector<LauncherIcon> launcherIcons_;
    SDL_Texture* backgroundTexture_ = nullptr;
};

} // namespace desktop
} // namespace jk

#endif // JKDESKTOPSHELL_H