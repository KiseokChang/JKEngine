#ifndef CLIENTGALLERYAPP_H
#define CLIENTGALLERYAPP_H

// Photo gallery hub (spec 2026-10-09-gallery-design). Structure mirrors
// ClientShotApp (dark root window, 16 ms timer, ImGui over the surface,
// Korean desktop font, AppContentTopOffset band math, theme hot-swap). T1
// scope: dir tabs over the resolved source dirs (state/screenshots default +
// settings.json gallery.dirs) + a fixed-cell thumbnail grid (160x120 box +
// one label row). T2 adds the full view: a 2-state view swap inside the same
// root window (grid click -> full view), fit display (gallery::FitFull),
// prev/next with wrap-around (gallery::WrapStep — buttons + left/right keys),
// filename+pixel-size meta row, Esc/button back to grid. T3 adds the thumbnail
// pipeline: an LRU texture pool (cap gallery::kThumbPoolMax) over the
// state/gallery/thumbs disk cache (key = path+size+mtime via gallery::ThumbKey)
// and gallery::MakeThumb nearest downscale. All three tasks consume the shared
// pure parts from apps/GalleryModel.h.

#include <client/JKClientApplication.h>
#include <apps/GalleryModel.h>
#include <JKImageLoader.h>
#include <string>
#include <vector>

struct SDL_Texture;

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
    // T3 썸네일 슬롯 — LRU 텍스처 풀 1칸(gallery::kThumbPoolMax 상한).
    // 슬롯 키 = 전체 경로(탭 바꿔 같은 이름 파일이 서로 다른 원전 — 파일명
    // 슬롯은 오합). 텍스처 지연 업로드는 전체 보기와 같은 수형(renderer_).
    struct ThumbSlot {
        std::string path;          // owning slot key (full path)
        std::string key;           // disk-cache key ("" = 스탬프 실패)
        LoadedImage img;           // reduce 완료 픽셀(RGBA8, 최대 160x120)
        SDL_Texture* tex = nullptr;
        long long useFrame = 0;    // 마지막 접촉 프레임 세대(fix r1 — 퇴출 산치
                                   //   GalleryModel.h PickLruVictim 소비)
        bool failed = false;       // 디코드/스탬프 실패 — 60Hz 재시도 방지
        unsigned long long failedGen = 0;  // 실패한 스캔 세대(재스캔에 재시도)
    };
    struct ThumbView {
        SDL_Texture* tex = nullptr;
        int w = 0;
        int h = 0;
    };
    // 격자 셀 1장의 썸네일 텍스처(요청 단위 LRU 접촉). 실패면 nullptr —
    // placeholder 박스가 남는다(T1 수형). 실패 슬롯은 스캔 세대가 바뀌어야 재시도.
    ThumbView ThumbTexture(const std::string& fullPath);
    // 풀에서 자리 확보 — 빈/실패 슬롯 재용 → 상한 미만이면 신설 → LRU 퇴출
    // (PickLruVictim — 이번 프레임 접촉분 전부 제외(fix r1 — 동일 프레임에
    // 기록된 드로우리스트 텍스처의 파괴 방지); 후보 없으면 nullptr — 그 셀은
    // placeholder 유지).
    ThumbSlot* AcquireThumbSlot(const std::string& fullPath);
    ThumbSlot* FindThumbSlot(const std::string& fullPath);
    // 슬롯 채우기 — 캐시 PNG 적중 = full 디코드 생략(비용 축 계약), 미적중 =
    // LoadImageFile full 디코드 → MakeThumb nearest 축소 → 디스크 캐시 기록
    // (best-effort — 기록 실패는 재부팅 재조명 비용일 뿐, 표시 결함 아님).
    void FillThumbSlot(ThumbSlot& slot, const std::string& fullPath);
    void EnsureThumbTexture(ThumbSlot& slot);
    // 빈 슬롯 파견 준비(텍스처·픽셀 소각) — 퇴출/재용의 공통 전처리.
    void ResetThumbSlot(ThumbSlot& slot);
    void ReleaseThumbTexture(ThumbSlot& slot);
    void ReleaseThumbs();
    void BuildUi(int w, int h);
    // 전체 보기 분기 UI(BuildUi가 fullView_일 때 위임). 메타 행 1줄+
    // 뷰포트 핏 이미지(2g-f FitFull 소비)+지연 텍스처 업로드(shot 수형).
    void BuildFullViewUi();
    // 전체 보기(T2) — 격자 셀 클릭 진입. fullIndex_+selectedPath_를 세우고
    // 디코드한다(실패 = 텍스처만 없음 — "이미지를 열 수 없습니다" 문구).
    void OpenFull(int index);
    // 격자 복귀(Esc 키/버튼): 텍스처 소각+모드 해제. selectedPath_는 격자
    // 선택 하이라이트로 남긴다.
    void CloseFull();
    // 이전/다음(delta ±1) — gallery::WrapStep 끝 지점 순환+재디코드.
    void StepFull(int delta);
    // fullIndex_의 파일을 디코드(shot SelectFile 수형 — 텍스처 업로드는
    // RenderOverlay에서 지연).
    void LoadFull();
    void DropTexture();
    // exe-dir state/settings.json 직독 → GalleryDirList(기본 폴더+유저 dirs).
    // settings 부재/파손 = 기본 1건(fail-safe) — 앱이 죽지 않는다.
    void ResolveDirs();
    // 선택 중인 dir의 이미지 파일 열거(ListImageFiles — 최신순, ec 중립형).
    // dirIndex_가 범위 밖이면 목록만 비운다. 전체 보기 중이면 열려 있던 파일이
    // 목록에 남아 있는지 재정렬(사라졌으면 격자 복귀).
    void RefreshFiles();
    static std::string ExeDir();

    bool frameDirty_ = true;
    bool imguiReady_ = false;

    std::vector<std::string> dirs_;    // resolved source dirs (defaults first)
    int dirIndex_ = 0;                 // active tab
    bool dirOk_ = false;               // active dir enumerated OK (empty-folder
                                       //   vs missing-folder 문구 구분 — ec 중립)
    std::vector<std::string> files_;   // active dir's file names, newest first
    std::string selectedPath_;         // full-view path (kept warm — 격자 하이라이트)

    // T3 thumbnail pool state.
    std::vector<ThumbSlot> thumbs_;    // LRU pool (cap gallery::kThumbPoolMax)
    long long thumbFrame_ = 0;         // 프레임 세대(RenderOverlay마다 증가 —
                                       //   퇴출 후보의 동일 프레임 접촉 제외)
    unsigned long long scanGen_ = 0;   // RefreshFiles마다 증가(실패 재시도 세대)

    // T2 full view state.
    bool fullView_ = false;            // 2-state view mode (grid / full view)
    int fullIndex_ = -1;               // files_ index of the viewed file
    LoadedImage current_;              // decoded pixels of selectedPath_
    SDL_Texture* texture_ = nullptr;   // GPU copy of current_ (RGBA32)
    int texW_ = 0;
    int texH_ = 0;
    SDL_Renderer* renderer_ = nullptr; // RenderOverlay's renderer, stashed
                                       //   for the deferred texture upload
};

} // namespace jk

#endif // CLIENTGALLERYAPP_H