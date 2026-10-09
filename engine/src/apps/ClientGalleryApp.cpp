// Photo gallery hub (spec 2026-10-09-gallery-design, T1 skeleton) — photo
// 모아보기 허브의 격자 뷰 골격. Structure mirrors ClientShotApp (dark root
// window, 16 ms timer, ImGui over the surface, Korean font via the desktop
// resolver, AppContentTopOffset band math, theme hot-swap re-apply).
// T1 scope: dir tabs + fixed-cell grid (160x120 placeholder box + one label
// row) + the selection path. Full-view entry is T2; thumbnail decode and the
// state/gallery/thumbs disk cache are T3. Pure parts live in apps/
// GalleryModel.h (jk::gallery) so the selftest twins and T3 share them.
#include <apps/ClientGalleryApp.h>

#include <client/JKClientSurface.h>
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
    if (!imguiReady_) {
        if (!ImGui_ImplJKWindow_Init(renderer))
            return;
        imguiReady_ = true;
    }

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
    if (dirIndex_ < 0 || dirIndex_ >= static_cast<int>(dirs_.size())) return;
    files_ = gallery::ListImageFiles(dirs_[dirIndex_], &dirOk_);
    // 없는 폴더 = 목록 비움+dirOk_ false(ec 중립형 계약 — ListImageFiles).
    // 표시 문구는 BuildUi가 dirOk_로 갈린다(빈 폴더 vs 폴더 없음).
    // 선택 파일이 열외되었으면 진입 경로를 비운다(T2 전체보기 선보관 계약).
    if (!selectedPath_.empty() &&
        std::find(files_.begin(), files_.end(),
                  std::filesystem::path(selectedPath_).filename().string()) ==
            files_.end()) {
        selectedPath_.clear();
    }
}

void ClientGalleryApp::BuildUi(int w, int h) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
    if (ImGui::Begin("gallery", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
        // The server reserves the top title band of every surface as
        // title-bar chrome — mouse-downs there never reach the client
        // (vplayer lesson 8, shot 동일). First row starts below the strip
        // or its buttons are dead (밴드 산식 진실원 — s=1.0 등호 30).
        ImGui::SetCursorPosY(static_cast<float>(jk::text::AppContentTopOffset()));

        if (ImGui::Button("새로고침")) {
            ResolveDirs();
            RefreshFiles();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%d개 폴더", static_cast<int>(dirs_.size()));

        // Dir tabs — resolved dirs in contract order (default capture dir
        // first, user dirs after; NormalizeDirs 중복·빈 성분 제거済).
        for (int i = 0; i < static_cast<int>(dirs_.size()); ++i) {
            if (i > 0) ImGui::SameLine();
            ImGui::PushID(i);
            const bool active = (i == dirIndex_);
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      ImGui::GetStyleColorVec4(
                                          ImGuiCol_ButtonHovered));
            if (ImGui::Button(dirs_[i].c_str())) {
                if (dirIndex_ != i) {
                    dirIndex_ = i;
                    selectedPath_.clear();
                    RefreshFiles();
                }
            }
            if (active) ImGui::PopStyleColor();
            ImGui::PopID();
        }

        // Grid: fixed cells — 160x120 thumbnail box + one clipped label row
        // (T1 brief). The cell click target holds the selection path warm;
        // T2 swaps the handler for the full-view enter, T3 fills the box
        // with the decoded thumb (placeholder + filename text for now).
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
                    selectedPath_ =
                        (std::filesystem::path(dirs_[dirIndex_]) / name)
                            .string();

                // Placeholder box uses the launcher cell tokens (docs/54
                // 허브) — face/outline; selection highlight = selectionBg 면.
                const ImU32 face =
                    selected
                        ? IM_COL32(t.selectionBg.r, t.selectionBg.g,
                                   t.selectionBg.b, 255)
                        : IM_COL32(t.launcherCellFace.r, t.launcherCellFace.g,
                                   t.launcherCellFace.b, 255);
                dl->AddRectFilled(p, q, face, 6.0f);
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

// P3 theme hot-swap (docs/52): the palette was snapshotted into ImGuiStyle
// at OnInit - re-apply it after a preset swap.
void ClientGalleryApp::OnThemeChanged() { jk::theme::ApplyImGuiTheme(); }

} // namespace jk