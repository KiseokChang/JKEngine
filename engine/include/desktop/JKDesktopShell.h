#ifndef JKDESKTOPSHELL_H
#define JKDESKTOPSHELL_H

#include <JKTypes.h>
#include <SDL.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace jk {
struct LoadedImage;
class JKTextAtlas;
class JKResourceCache;
class JKSDLRenderBackend;
class HangulManager;
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
    JKDesktopShell();
    ~JKDesktopShell();
    void Init(const ShellHost& host);

    // Frame background painter — the server's Composite() calls this before
    // compositing layers. No-op with no icons (legacy empty-desktop behavior).
    void Draw(SDL_Renderer* renderer);

    void Destroy();

    // Physical-pixel hit test → launcher icon index, or -1.
    int HitTest(int x, int y) const;

    // 마우스 호버 툴팁 (docs/67 후속): 서버 SDL 마우스 경로가 런처 영역의
    // 물리 픽셀 히트 인덱스를 중계한다. 인덱스가 바뀌면 지연 타이머를 리셋하고,
    // 300ms 머무르면 Draw()가 아이콘 아래에 툴팁을 렌더한다. 아이콘 밖(-1)이나
    // 클라이언트 표면 위에서는 ClearHover로 즉시 숨긴다.
    void UpdateHover(int hitIndex);
    void ClearHover();

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
        // 툴팁 표시명 (docs/67 후속): .jkx는 매니페스트 title, 콘솔 앱은
        // manifest.json desc. 빈 값이면 그리기 때 appName로 폴백.
        std::string title;
        std::string jkxPath;   // non-empty → spawn "--jkx <path>"
        // 콘솔 앱 kind (P4 SDK §3): 비었으면 .jkx/내장 앱 셀. cmd는 서버
        // cwd(engine/build) 기준 상대경로 — 인용 겹침 방지(453a327).
        std::string consoleCmd;
        std::string consoleDir;
        SDL_Texture* texture = nullptr;
    };

    // 툴팁 텍스처 (표시명별 1회 렌더 후 캐시 — 승인 배너 approvalBannerTexs_
    // 선례). 표시명별 가로·세로는 물리 픽셀 크기.
    struct TooltipTex {
        SDL_Texture* tex = nullptr;
        int w = 0;
        int h = 0;
    };

    // 표시명 UTF-8 → 지연 부품(JKTextAtlas/JKResourceCache/백엔드/한글
    // 매니저)으로 배경·테두리·글자를 실은 텍스처 1장 렌더. 실패는 빈 항목을
    // 캐시해 매 프레임 재시도하지 않는다(배너 선례).
    SDL_Texture* TooltipTexture(const std::string& utf8, int* w, int* h);
    // 호버 중인 아이콘 셀 아래(바닥에 닿으면 위) 툴팁을 물리 픽셀로 렌더.
    void DrawTooltip(SDL_Renderer* renderer, const LauncherIcon& icon);

    void ScanJkxApps();
    void ScanConsoleApps();
    void RelayoutLauncherIcons();
    SDL_Texture* LoadTextureScaled(const char* assetBase);

    ShellHost host_;
    std::vector<LauncherIcon> launcherIcons_;
    SDL_Texture* backgroundTexture_ = nullptr;

    // 호버 상태: hoverIndex_는 최근 모션의 런처 히트 인덱스(-1 = 런처 밖).
    // 같은 아이콘에 kTooltipHoverDelayMs 이상 머무르면 hoverActive_가 서고
    // Draw()가 툴팁을 렌더한다. 텍스처 캐시와 렌더 지연 부품은 툴팁 전용 —
    // 서버의 승인 배너 부품(bannerAtlas_ 등)과 키·수명이 분리된다.
    static constexpr Uint32 kTooltipHoverDelayMs = 300;
    int hoverIndex_ = -1;
    Uint32 hoverStartMs_ = 0;
    bool hoverActive_ = false;
    std::map<std::string, TooltipTex> tooltipTexs_;
    std::unique_ptr<JKTextAtlas> tooltipAtlas_;
    std::unique_ptr<JKResourceCache> tooltipCache_;
    std::unique_ptr<JKSDLRenderBackend> tooltipBackend_;
    std::unique_ptr<HangulManager> tooltipFont_;
};

} // namespace desktop
} // namespace jk

#endif // JKDESKTOPSHELL_H