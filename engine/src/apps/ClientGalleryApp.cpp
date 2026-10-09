// Photo gallery hub (spec 2026-10-09-gallery-design) — photo 모아보기 허브.
// Structure mirrors ClientShotApp (dark root window, 16 ms timer, ImGui over
// the surface, Korean font via the desktop resolver, AppContentTopOffset band
// math, theme hot-swap re-apply). T1: dir tabs + fixed-cell grid (160x120
// placeholder box + one label row) + the selection path. T2 (this file's
// full-view branch): grid click -> full view swap in the same window —
// LoadImageFile decode into an SDL streaming texture (shot SelectFile 수형),
// fit display via gallery::FitFull, prev/next with wrap-around via
// gallery::WrapStep (buttons + left/right keys), filename+pixel-size meta row,
// Esc/button back to grid. T3 (this file's thumbnail pipeline): the grid cells
// request thumb textures from an LRU pool (cap gallery::kThumbPoolMax=96) —
// cache hit = state/gallery/thumbs/<key>.png via LoadImageFile (full decode
// skipped), miss = LoadImageFile full decode + gallery::MakeThumb nearest
// downscale + jk::SaveImageFile best-effort cache write. The cache key is
// path+size+mtime (gallery::ThumbKey — 2g-d Fnv1a 소비). Pure parts live in
// apps/GalleryModel.h (jk::gallery) so the selftest twins and the app share
// them.
#include <apps/ClientGalleryApp.h>

#include <client/JKClientSurface.h>
#include <JKImageLoader.h>
#include <imgui_impl_jkwindow.h>
#include "theme/JKThemeImGui.h"
#include <JKTextAtlas.h>
#include <JKWindow.h>
#include <SDL.h>

#include <algorithm>
#include <cstdio>
// 폴더 열거는 std::filesystem::directory_iterator — ec 중립형(throwing
// 오버로드 금지)은 jk::gallery::ListImageFiles 헤더 원문 계약이 소유한다.
#include <filesystem>
#include <port/JKCrtShim.h>

namespace jk {
namespace {
// Root window paints the dark clear color so the surface never flashes white
// behind ImGui's rounded windows (same idiom as the palette/notify/shot).
class GalleryRoot : public JKWindow {
public:
    explicit GalleryRoot(const std::string& title) : JKWindow(title) {}
    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetClientRect();
        const auto& t = jk::theme::current();
        dc.SetColor(t.appClearBg.r, t.appClearBg.g, t.appClearBg.b, 255);
        dc.FillRect(client);
    }
};
} // namespace

ClientGalleryApp::~ClientGalleryApp() = default;

std::string ClientGalleryApp::ExeDir() {
    char base[1024] = {};
    char* p = SDL_GetBasePath();
    if (p) {
        std::snprintf(base, sizeof(base), "%s", p);
        SDL_free(p);
    }
    return base;
}

void ClientGalleryApp::OnInit() {
    auto main = std::make_unique<GalleryRoot>("Gallery");
    main->SetWindowRect(JKRect{ 0, 0, 560, 520 });
    main->SetAttrFlags(WA_CHROMELESS);
    SetMainWindow(std::move(main));

    SetTimerInterval(16); // ~60 Hz frame cadence (notify/shot idiom)

    ImGui::CreateContext();
    jk::theme::ApplyImGuiTheme(); // JKTheme 팔레트 봉합 (P2 단계 3)
    ImGui::GetIO().IniFilename = nullptr;
    // Korean UI (새로고침/빈 폴더 상태 문구) — desktop font resolver
    // (docs/63 §4.1, shot 동일): 빈 해석은 커스텀 폰트 스킵(내장 기본 글리프
    // — AddFont 시점 assert 사망 대신 열화).
    ImGuiIO& io = ImGui::GetIO();
    const std::string fontPath = jk::text::ResolveDesktopFontPath();
    if (!fontPath.empty())
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f,
                                     nullptr,
                                     io.Fonts->GetGlyphRangesKorean());

    ResolveDirs();
    RefreshFiles();
}

void ClientGalleryApp::OnClose() {
    DropTexture();
    ReleaseThumbs();
    if (imguiReady_) {
        ImGui_ImplJKWindow_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
}

bool ClientGalleryApp::PreProcessMessage(const JKEvent& ev) {
    ImGui_ImplJKWindow_ProcessJKEvent(ev);
    if (ev.type == JKEventType::Timer) {
        frameDirty_ = true;
    }
    return true;
}

void ClientGalleryApp::OnFrameCommitted() {
    frameDirty_ = false;
}

void ClientGalleryApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
    ++thumbFrame_;  // T3 fix r1 — 이번 프레임의 요청 세대(퇴출 산치의 원자선;
                    //   BuildUi 셀 요청 전에 반드시 성립)
    if (!imguiReady_) {
        if (!ImGui_ImplJKWindow_Init(renderer))
            return;
        imguiReady_ = true;
    }

    // 텍스처 업로드는 BuildUi에서 — SDL 렌더러가 존재하는 곳이 여기뿐이라
    // 여기서 스태시(shot 동일 수형).
    renderer_ = renderer;
    ImGui_ImplJKWindow_NewFrame(1.0f / 60.0f, w, h);
    ImGui::NewFrame();
    BuildUi(w, h);
    ImGui::Render();
    ImGui_ImplJKWindow_RenderDrawData(ImGui::GetDrawData(), renderer);
    frameDirty_ = true;
}

void ClientGalleryApp::ResolveDirs() {
    // exe-dir settings.json 직독(LoadDesktopSettingsJson 선례 — JKTextAtlas)
    // →GalleryDirList에 **원문 텍스트**로 넘긴다: 리졸버는 순수 함수(디스크
    // I/O 없음)라 셀프테스트 쌍둥이가 같은 경로를 직단정한다.
    std::string text;
    {
        const std::string kvPath =
            (std::filesystem::path(ExeDir()) / "state" / "settings.json")
                .string();
        std::FILE* f = std::fopen(kvPath.c_str(), "rb");
        if (f) {
            char chunk[2048];
            size_t n;
            while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0)
                text.append(chunk, n);
            std::fclose(f);
        }
        // 부재/읽기 실패 = 빈 텍스트 → 리졸버가 기본 1건(fail-safe)으로 떨어진다.
    }
    dirs_ = gallery::GalleryDirList(ExeDir(), text);
    if (dirIndex_ < 0 || dirIndex_ >= static_cast<int>(dirs_.size()))
        dirIndex_ = 0;
}

void ClientGalleryApp::RefreshFiles() {
    files_.clear();
    dirOk_ = false;
    // 실패 슬롯의 재시도 세대 — 재스캔(새로고침·탭 전환·부팅)마다 실패한
    // 셀이 한 번 다시 디코드를 시도한다(60Hz 재시도 방지 계약의 리셋점).
    ++scanGen_;
    if (dirIndex_ < 0 || dirIndex_ >= static_cast<int>(dirs_.size())) return;
    files_ = gallery::ListImageFiles(dirs_[dirIndex_], &dirOk_);
    // 없는 폴더 = 목록 비움+dirOk_ false(ec 중립형 계약 — ListImageFiles).
    // 표시 문구는 BuildUi가 dirOk_로 갈린다(빈 폴더 vs 폴더 없음).
    // 선택 파일이 열외되었으면 진입 경로를 비운다(전체보기 선보관 계약).
    if (!selectedPath_.empty() &&
        std::find(files_.begin(), files_.end(),
                  std::filesystem::path(selectedPath_).filename().string()) ==
            files_.end()) {
        selectedPath_.clear();
    }
    // 전체 보기 중 재스캔이면 인덱스를 재정렬한다 — 열려 있던 파일이 목록에
    // 남아 있으면 mtime 정렬 변화만 반영(텍스처 보존), 사라졌으면 격자 복귀.
    if (fullView_) {
        if (selectedPath_.empty()) {
            CloseFull();
        } else {
            const std::string name =
                std::filesystem::path(selectedPath_).filename().string();
            const auto it = std::find(files_.begin(), files_.end(), name);
            fullIndex_ = (it == files_.end())
                             ? -1
                             : static_cast<int>(it - files_.begin());
            if (fullIndex_ < 0) CloseFull();
        }
    }
}

// T2 full view — 격자 셀 클릭 진입. 범위 밖 인덱스는 격자로 되돌려 보낸다
// (RefreshFiles 열거 계약과 같은 방어선).
void ClientGalleryApp::OpenFull(int index) {
    if (index < 0 || index >= static_cast<int>(files_.size())) {
        CloseFull();
        return;
    }
    fullView_ = true;
    fullIndex_ = index;
    LoadFull();
}

void ClientGalleryApp::CloseFull() {
    fullView_ = false;
    fullIndex_ = -1;
    DropTexture();
    // selectedPath_는 격자 선택 하이라이트로 남긴다 — RefreshFiles의 열외
    // 제거 계약이 파일 사라짐만 정리한다.
}

void ClientGalleryApp::StepFull(int delta) {
    if (!fullView_ || files_.empty()) return;
    fullIndex_ = gallery::WrapStep(fullIndex_, delta,
                                   static_cast<int>(files_.size()));
    LoadFull();
}

void ClientGalleryApp::LoadFull() {
    DropTexture();
    if (fullIndex_ < 0 || fullIndex_ >= static_cast<int>(files_.size()) ||
        dirIndex_ < 0 || dirIndex_ >= static_cast<int>(dirs_.size()))
        return;
    // fs::path 합성 — RefreshFiles 열거와 같은 플랫폼 몫 구분자 (shot 동일
    // 계약: selectedPath_ 비교와 정확히 일치하는 수형).
    selectedPath_ =
        (std::filesystem::path(dirs_[dirIndex_]) / files_[fullIndex_])
            .string();
    // 실패(decode 불가) = 텍스처만 없음 — 앱은 사지 않는다(shot SelectFile
    // 수형; DropTexture가 current_를 먼저 비운다 — LoadImageFile 실패 시
    // out 무변경 계약이므로 찌꺼기 방지).
    LoadImageFile(selectedPath_, current_);
}

void ClientGalleryApp::DropTexture() {
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    texW_ = 0;
    texH_ = 0;
    current_.rgba.clear();
    current_.w = 0;
    current_.h = 0;
}

// ---- T3 thumbnail pipeline (디스크 캐시 + nearest 축소 + LRU 텍스처 풀) ----

ClientGalleryApp::ThumbSlot* ClientGalleryApp::FindThumbSlot(
    const std::string& fullPath) {
    for (ThumbSlot& s : thumbs_)
        if (s.path == fullPath) return &s;
    return nullptr;
}

ClientGalleryApp::ThumbSlot* ClientGalleryApp::AcquireThumbSlot(
    const std::string& fullPath) {
    // 남는 슬롯(텍스처·픽셀 없음) 재용 — 상한 접촉 없이 소진.
    for (ThumbSlot& s : thumbs_) {
        if (!s.tex && s.img.rgba.empty()) {
            ReleaseThumbTexture(s);
            s.path.clear();
            s.key.clear();
            s.failed = false;
            return &s;
        }
    }
    if (static_cast<int>(thumbs_.size()) < gallery::kThumbPoolMax) {
        thumbs_.emplace_back();
        ThumbSlot* fresh = &thumbs_.back();
        fresh->path = fullPath;
        return fresh;
    }
    // LRU 퇴출(끝, fix r1) — **이번 프레임 접촉분은 후보에서 전부 제외한다**:
    // 같은 프레임 안의 요청도 tick이 달라 예전 가드(`lastUse == cur` 1건)는
    // 앞선 셀의 슬롯을 후보로 놓쳤고, victim의 텍스처는 이미 이번 프레임
    // 드로우리스트(ImGui::Image가 캔 ImTextureID)에 기록된 상태 — 파괴하면
    // `RenderDrawData`가 해방된 SDL 텍스처로 돈다(C1 — N>96 격자에서 프레임마다
    // 발동). 프레임 세대 스탬프(useFrame)로 전체 제외하고, 후보가 0이면 그
    // 요청은 텍스처를 못 받고 placeholder로 뺀다(산치는 순수 부품 PickLruVictim
    // — 2g-i 소비, 퇴출 파괴는 다음 프레임 접촉 시점까지 미뤄진다).
    std::vector<long long> useFrames;
    useFrames.reserve(thumbs_.size());
    for (const ThumbSlot& s : thumbs_) useFrames.push_back(s.useFrame);
    const int victimIx = gallery::PickLruVictim(useFrames, thumbFrame_);
    if (victimIx < 0) return nullptr;
    ThumbSlot& victim = thumbs_[static_cast<size_t>(victimIx)];
    ReleaseThumbTexture(victim);
    victim = ThumbSlot();
    victim.path = fullPath;
    return &victim;
}

void ClientGalleryApp::ReleaseThumbTexture(ThumbSlot& slot) {
    if (slot.tex) {
        SDL_DestroyTexture(slot.tex);
        slot.tex = nullptr;
    }
}

void ClientGalleryApp::ResetThumbSlot(ThumbSlot& slot) {
    ReleaseThumbTexture(slot);
    slot.img.rgba.clear();
    slot.img.w = 0;
    slot.img.h = 0;
    slot.key.clear();
    slot.failed = false;
    slot.failedGen = 0;
}

void ClientGalleryApp::FillThumbSlot(ThumbSlot& slot,
                                     const std::string& fullPath) {
    // 스탬프 실패(파일 부재·mtime 부정) = 실패 슬롯 — 열거 직후 소멸 성분의
    // 방어선(2g-e의 열외 성분 스킵과 같은 성질).
    const gallery::ThumbStamp st = gallery::ThumbStampOf(fullPath);
    if (!st.ok) {
        ReleaseThumbTexture(slot);
        slot.path = fullPath;
        slot.key.clear();
        slot.img = LoadedImage();
        slot.failed = true;
        slot.failedGen = scanGen_;
        return;
    }
    slot.path = fullPath;
    slot.key = gallery::ThumbKey(fullPath, st.size, st.mtime);
    // 캐시 적중 = full 디코드 생략(비용 축 계약 — 재부팅 재조명 방지). 캐시
    // PNG도 LoadImageFile로 디코드한다(작은 그림이라 저렴 — 폴리 계약).
    const std::string cachePath = gallery::GalleryThumbPath(ExeDir(), slot.key);
    if (!cachePath.empty()) {
        LoadedImage cached;
        if (LoadImageFile(cachePath, cached) && cached.w > 0 && cached.h > 0) {
            ReleaseThumbTexture(slot);
            slot.img = std::move(cached);
            slot.failed = false;
            return;
        }
    }
    // 미적중 — full 디코드 1회 + nearest 축소 + 디스크 기록(best-effort).
    LoadedImage full;
    if (!LoadImageFile(fullPath, full) || full.w <= 0 || full.h <= 0) {
        ReleaseThumbTexture(slot);
        slot.img = LoadedImage();
        slot.failed = true;
        slot.failedGen = scanGen_;
        return;
    }
    LoadedImage thumb = gallery::MakeThumb(full, gallery::kCellW,
                                          gallery::kCellH);
    if (thumb.w > 0 && thumb.h > 0 && !slot.key.empty()) {
        // 캐시 dir 부재 = 최초 쓰기 시 생성(ec 중립형 — 지장 없는 방어선).
        std::error_code cd;
        std::filesystem::create_directories(
            std::filesystem::path(
                gallery::GalleryThumbPath(ExeDir(), slot.key))
                .parent_path(),
            cd);
        if (!cd) jk::SaveImageFile(cachePath, thumb);  // 실패 무시(비용 축 몫)
    }
    ReleaseThumbTexture(slot);
    slot.img = std::move(thumb);
    slot.failed = false;
}

ClientGalleryApp::ThumbView ClientGalleryApp::ThumbTexture(
    const std::string& fullPath) {
    ThumbSlot* slot = FindThumbSlot(fullPath);
    if (!slot) {
        slot = AcquireThumbSlot(fullPath);
        if (!slot) return ThumbView();  // 상한+동프레임 — placeholder 유지
        FillThumbSlot(*slot, fullPath);
    } else if (slot->failed && slot->failedGen != scanGen_) {
        // 실패 슬롯 재시도 — 스캔 세대가 바뀐 뒤 처음 조명에서 1회만.
        ResetThumbSlot(*slot);
        FillThumbSlot(*slot, fullPath);
    }
    slot->useFrame = thumbFrame_;  // LRU 스탬프 — 프레임 세대(fix r1)
    if (slot->failed) return ThumbView();
    EnsureThumbTexture(*slot);  // 지연 업로드(renderer_ 스태시 수형 — 전체 보기)
    return ThumbView{ slot->tex, slot->img.w, slot->img.h };
}

void ClientGalleryApp::EnsureThumbTexture(ThumbSlot& slot) {
    if (slot.tex || slot.img.rgba.empty() || slot.img.w <= 0 ||
        slot.img.h <= 0 || !renderer_)
        return;
    SDL_Texture* t = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                       SDL_TEXTUREACCESS_STREAMING,
                                       slot.img.w, slot.img.h);
    if (!t) return;  // 희귀(텍스처 생성 실패) — 다음 프레임 재시도
    SDL_UpdateTexture(t, nullptr, slot.img.rgba.data(), slot.img.w * 4);
    slot.tex = t;
}

void ClientGalleryApp::ReleaseThumbs() {
    for (ThumbSlot& s : thumbs_) ReleaseThumbTexture(s);
    thumbs_.clear();
}

void ClientGalleryApp::BuildUi(int w, int h) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::Begin("gallery", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
        // The server reserves the top title band of every surface as
        // title-bar chrome — mouse-downs there never reach the client
        // (vplayer lesson 8, shot 동일). First row starts below the strip
        // or its buttons are dead (밴드 산식 진실원 — s=1.0 등호 30).
        ImGui::SetCursorPosY(static_cast<float>(jk::text::AppContentTopOffset()));

        // 키보드 처리(snap 원문 — IsKeyPressed는 NewFrame 안에서만 성립):
        // 전체 보기에서 Esc=격자 복귀, ←/→=이전/다음. 텍스트 입력 없는 앱이나
        // WantTextInput 방어선은 유지(vplayer 원문 계약).
        if (fullView_ && !io.WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                CloseFull();
            else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false))
                StepFull(-1);
            else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false))
                StepFull(+1);
        }

        if (fullView_) {
            BuildFullViewUi();
            ImGui::End();
            return;
        }

        if (ImGui::Button("새로고침")) {
            ResolveDirs();
            RefreshFiles();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%d개 폴더", static_cast<int>(dirs_.size()));

        // Dir tabs — resolved dirs in contract order (default capture dir
        // first, user dirs after; NormalizeDirs 중복·빈 성분 제거済).
        // (T1 리뷰 C1 수리) 탭 라벨 = 폴더 말단 이름 — 전문 경로 라벨은 긴
        // 경로에서 탭 행 오버플로. 전문 경로는 툴힌트로 본다. 말단 이름이
        // 빈 수형(루트 "/")은 전문으로 남긴다(빈 라벨 방지).
        for (int i = 0; i < static_cast<int>(dirs_.size()); ++i) {
            if (i > 0) ImGui::SameLine();
            ImGui::PushID(i);
            std::string tabLabel =
                std::filesystem::path(dirs_[i]).filename().string();
            if (tabLabel.empty()) tabLabel = dirs_[i];
            const bool active = (i == dirIndex_);
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      ImGui::GetStyleColorVec4(
                                          ImGuiCol_ButtonHovered));
            if (ImGui::Button(tabLabel.c_str())) {
                if (dirIndex_ != i) {
                    dirIndex_ = i;
                    CloseFull();  // 탭 전환 = 모드 격자 복귀(텍스처 소각)
                    selectedPath_.clear();
                    RefreshFiles();
                }
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", dirs_[i].c_str());
            if (active) ImGui::PopStyleColor();
            ImGui::PopID();
        }

        // Grid: fixed cells — 160x120 thumbnail box + one clipped label row
        // (T1 brief). The cell click enters the full view (T2 — 같은 창 내
        // 모드 스왑; OpenFull이 진입 경로를 합성+디코드한다). T3 fills the
        // box with the decoded thumb (placeholder + filename text for now).
        ImGui::Separator();
        if (ImGui::BeginChild("grid", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const auto& t = jk::theme::current();
            const float cellW = static_cast<float>(gallery::kCellW);
            const float cellH = static_cast<float>(gallery::kCellH);
            const float labelH = ImGui::GetTextLineHeight();  // 라벨 1행
            const float stepX = cellW + 10.0f;

            // 한 행에 몇 장 — 창폭에서 도출(스케일 무관 플로우).
            const float avail = ImGui::GetContentRegionAvail().x;
            int perRow = static_cast<int>(avail / stepX);
            if (perRow < 1) perRow = 1;

            if (files_.empty()) {
                // 빈 폴더 vs 폴더 없음(없는 dir = dirOk_ false) — ec 중립형의
                // 소비 문구.
                ImGui::TextUnformatted(dirOk_ ? "사진이 없습니다"
                                              : "폴더를 열 수 없습니다");
            }
            for (int i = 0; i < static_cast<int>(files_.size()); ++i) {
                const std::string& name = files_[i];
                if (i % perRow != 0) ImGui::SameLine();

                const ImVec2 p = ImGui::GetCursorScreenPos();
                const ImVec2 q{ p.x + cellW, p.y + cellH };
                const bool selected =
                    selectedPath_ ==
                    (std::filesystem::path(dirs_[dirIndex_]) / name).string();

                ImGui::PushID(i);
                const bool clicked = ImGui::InvisibleButton(
                    "cell", ImVec2(cellW, cellH + labelH));
                ImGui::PopID();
                // (fs::path 합성 — RefreshFiles 열거와 같은 플랫폼 몫 구분자
                // 라 selected 비교가 정확히 일치한다: shot 앱 동일 계약.)
                if (clicked)
                    OpenFull(i);  // T2 — 진입 경로는 LoadFull이 합성한다

                // T3 — 셀 박스 안 썸네일 요청. 슬롯 키는 전체 경로(fs::path
                // 합성 — selected 비교와 같은 수형): 탭이 바뀌어 같은 이름
                // 파일이 다른 폴더에 있으면 서로 다른 원전이다. 텍스처 없는
                // 셀(디코드 실패·상한 초과·지연 중)은 placeholder 면이 그대로
                // 남는다(T1 수형).
                const std::string cellPath =
                    (std::filesystem::path(dirs_[dirIndex_]) / name).string();
                const ThumbView view = ThumbTexture(cellPath);
                // Placeholder box uses the launcher cell tokens (docs/54
                // 허브) — face/outline; selection highlight = selectionBg 면.
                const ImU32 face =
                    selected
                        ? IM_COL32(t.selectionBg.r, t.selectionBg.g,
                                   t.selectionBg.b, 255)
                        : IM_COL32(t.launcherCellFace.r, t.launcherCellFace.g,
                                   t.launcherCellFace.b, 255);
                dl->AddRectFilled(p, q, face, 6.0f);
                // 썸네일 — FitThumb 산치로 박스에 맞춰 중앙 그린다(비율 유지 —
                // 박스 밖 출혈 없음), 면 위에 아웃라인/라벨 텍스처가 아래 깔린다.
                if (view.tex && view.w > 0 && view.h > 0) {
                    const gallery::FitSize tf = gallery::FitThumb(
                        view.w, view.h, gallery::kCellW, gallery::kCellH);
                    if (tf.w > 0.f && tf.h > 0.f) {
                        const float tx = p.x + (cellW - tf.w) * 0.5f;
                        const float ty = p.y + (cellH - tf.h) * 0.5f;
                        dl->AddImage((ImTextureID)view.tex, ImVec2(tx, ty),
                                     ImVec2(tx + tf.w, ty + tf.h));
                    }
                }
                const ImU32 textColor =
                    IM_COL32(t.launcherCellOutline.r, t.launcherCellOutline.g,
                             t.launcherCellOutline.b, 255);
                dl->AddRect(p, q, textColor, 6.0f);
                // 파일명 — 박스 안 1행 클립(hover 톤 없이 텍스트로 식별).
                const ImVec4 clipBoxTop(p.x + 6.0f, p.y + 4.0f, q.x - 6.0f,
                                        p.y + 4.0f + labelH);
                dl->AddText(nullptr, 0.0f, ImVec2(clipBoxTop.x, clipBoxTop.y),
                            textColor, name.c_str(), nullptr, 0.0f,
                            &clipBoxTop);
                // 라벨 1행 — 박스 아래(클립).
                const ImVec4 clipLabel(p.x, q.y, q.x, q.y + labelH);
                dl->AddText(nullptr, 0.0f, ImVec2(clipLabel.x, clipLabel.y),
                            textColor, name.c_str(), nullptr, 0.0f,
                            &clipLabel);
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

// T2 full view (같은 root 창 내 모드 스왑 — 격자 대신 한 장을 뷰포트 핏으로):
// 메타 행(파일명+원본 픽셀 크기) 1줄 + 핏 이미지(2g-f 소비). Esc/좌우 키는
// BuildUi 상단에서 처리한다.
void ClientGalleryApp::BuildFullViewUi() {
    // 상단 행: 격자 복귀 버튼 + 이전/다음(brief 계약 — `<`=이전/`>`=다음).
    if (ImGui::Button("격자로")) CloseFull();
    ImGui::SameLine();
    if (ImGui::Button("<")) StepFull(-1);
    ImGui::SameLine();
    if (ImGui::Button(">")) StepFull(+1);
    ImGui::SameLine();
    // 메타 행 1줄 — 파일명+원본 픽셀 크기(스펙 결정 3 "파일명·픽셀 크기
    // 표시"). 창 우측 잘림은 ImGui 창 클립이 맡는다(격자 라벨과 동일 성질).
    if (fullIndex_ >= 0 && fullIndex_ < static_cast<int>(files_.size())) {
        if (current_.w > 0 && current_.h > 0)
            ImGui::TextDisabled("%s  %dx%d",
                                files_[fullIndex_].c_str(), current_.w,
                                current_.h);
        else
            ImGui::TextDisabled("%s  (열 수 없음)",
                                files_[fullIndex_].c_str());
    } else {
        ImGui::TextDisabled("사진이 없습니다");
    }

    // 지연 텍스처 업로드 — SDL 렌더러가 존재하는 RenderOverlay 이후에만 가능
    // (shot 동일 수형: 선택 1장당 스트리밍 텍스처 1개).
    if (!texture_ && !current_.rgba.empty() && current_.w > 0 &&
        current_.h > 0 && renderer_) {
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     current_.w, current_.h);
        if (texture_) {
            SDL_UpdateTexture(texture_, nullptr, current_.rgba.data(),
                              current_.w * 4);
            texW_ = current_.w;
            texH_ = current_.h;
        }
    }

    // 핏 이미지 — FitFull(원본 w,h; 뷰포트 w,h)의 순수 비율 산치를 ImGui
    // Image 스케일 그대로 쓴다(2g-f 소비 계약 — 화면 배율 상태 무접촉).
    const float availW = ImGui::GetContentRegionAvail().x;
    const float availH = ImGui::GetContentRegionAvail().y;
    const gallery::FitSize fit =
        gallery::FitFull(current_.w, current_.h, availW, availH);
    if (texture_ && fit.w > 0 && fit.h > 0) {
        // 뷰포트 중앙 정렬(남은 폭/높이의 절반 오프셋).
        const float offX = (availW - fit.w) * 0.5f;
        if (offX > 0.f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offX);
        const float offY = (availH - fit.h) * 0.5f;
        if (offY > 0.f) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offY);
        ImGui::Image((ImTextureID)texture_, ImVec2(fit.w, fit.h));
    } else if (!current_.rgba.empty()) {
        // (T2 리뷰 M2 수리) 문구 성립 조건 분리 — 픽셀은 있는데 텍스처만 못
        // 만든 희귀 경로(GPU 생성 실패)는 "디코드 실패" 문구로 오인하지 않는다.
        // 다음 프레임 재시도(EnsureThumbTexture와 같은 지연 계약)이므로 문구 없이
        // 지연 중을 유지한다.
    } else if (!selectedPath_.empty()) {
        // 디코드 실패 = 앱 상존(shot 수형), 문구만.
        ImGui::TextUnformatted("이미지를 열 수 없습니다");
    } else {
        ImGui::TextUnformatted("사진이 없습니다");
    }
}

// P3 theme hot-swap (docs/52): the palette was snapshotted into ImGuiStyle
// at OnInit - re-apply it after a preset swap.
void ClientGalleryApp::OnThemeChanged() { jk::theme::ApplyImGuiTheme(); }

} // namespace jk