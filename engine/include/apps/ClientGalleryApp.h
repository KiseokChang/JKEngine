#ifndef CLIENTGALLERYAPP_H
#define CLIENTGALLERYAPP_H

// Photo gallery hub (spec 2026-10-09-gallery-design, T1 skeleton). Structure
// mirrors ClientShotApp (dark root window, 16 ms timer, ImGui over the
// surface, Korean desktop font, AppContentTopOffset band math, theme
// hot-swap). T1 scope: dir tabs over the resolved source dirs
// (state/screenshots default + settings.json gallery.dirs) + a fixed-cell
// thumbnail grid (160x120 box + one label row) with the selection path. The
// full view entry is T2 and the thumbnail decode/disk cache is T3 — both
// consume the shared pure parts from apps/GalleryModel.h.

#include <client/JKClientApplication.h>
#include <apps/GalleryModel.h>
#include <string>
#include <vector>

namespace jk {

class ClientGalleryApp : public JKClientApplication {
public:
    ClientGalleryApp() = default;
    ~ClientGalleryApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    void OnThemeChanged() override;  // ImGui palette re-apply (docs/52)
    bool PreProcessMessage(const JKEvent& ev) override;
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;

private:
    void BuildUi(int w, int h);
    // exe-dir state/settings.json 직독 → GalleryDirList(기본 폴더+유저 dirs).
    // settings 부재/파손 = 기본 1건(fail-safe) — 앱이 죽지 않는다.
    void ResolveDirs();
    // 선택 중인 dir의 이미지 파일 열거(ListImageFiles — 최신순, ec 중립형).
    // dirIndex_가 범위 밖이면 목록만 비운다.
    void RefreshFiles();
    static std::string ExeDir();

    bool frameDirty_ = true;
    bool imguiReady_ = false;

    std::vector<std::string> dirs_;    // resolved source dirs (defaults first)
    int dirIndex_ = 0;                 // active tab
    bool dirOk_ = false;               // active dir enumerated OK (empty-folder
                                       //   vs missing-folder 문구 구분 — ec 중립)
    std::vector<std::string> files_;   // active dir's file names, newest first
    std::string selectedPath_;         // T2 full-view entry path (kept warm)
};

} // namespace jk

#endif // CLIENTGALLERYAPP_H