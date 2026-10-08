// Screenshot viewer (agent platform, docs/35 Task 4). Structure mirrors
// ClientNotifyApp (16 ms timer, ImGui over a dark root window). File list +
// decoded-image display; the 영역 캡처 button spawns the rubber-band overlay
// (launch_app snap) — one API, many faces.
#include <apps/ClientShotApp.h>

#include <client/JKClientSurface.h>
#include <imgui_impl_jkwindow.h>
#include "theme/JKThemeImGui.h"
#include <JKTextAtlas.h>
#include <JKWindow.h>
#include <SDL.h>

#include <algorithm>
#include <cstdio>
// windows.h는 소각됐다(stage-3 task 7) — 상태 폴더 열거(FindFirstFileA)는
// std::filesystem::directory_iterator(ec 중립형), .png 대소문자 무시 비교는
// jk::crt::Stricmp 셈(T2)이 소유한다.
#include <filesystem>
#include <port/JKCrtShim.h>

namespace jk {
namespace {
// Root window paints the dark clear color so the surface never flashes white
// behind ImGui's rounded windows (same idiom as the palette/notify).
class ShotRoot : public JKWindow {
public:
    explicit ShotRoot(const std::string& title) : JKWindow(title) {}
    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetClientRect();
        const auto& t = jk::theme::current();
        dc.SetColor(t.appClearBg.r, t.appClearBg.g, t.appClearBg.b, 255);
        dc.FillRect(client);
    }
};
} // namespace

ClientShotApp::~ClientShotApp() = default;

std::string ClientShotApp::ExeDir() {
    char base[1024] = {};
    char* p = SDL_GetBasePath();
    if (p) {
        std::snprintf(base, sizeof(base), "%s", p);
        SDL_free(p);
    }
    return base;
}

void ClientShotApp::OnInit() {
    auto main = std::make_unique<ShotRoot>("Screenshots");
    main->SetWindowRect(JKRect{ 0, 0, 480, 420 });
    main->SetAttrFlags(WA_CHROMELESS);
    SetMainWindow(std::move(main));

    SetTimerInterval(16); // ~60 Hz frame cadence (notify idiom)

    ImGui::CreateContext();
    jk::theme::ApplyImGuiTheme(); // JKTheme 팔레트 봉합 (P2 단계 3)
    ImGui::GetIO().IniFilename = nullptr;
    // Korean UI (새로고침/영역 캡처/empty-state text) — desktop font resolver
    // (docs/63 §4.1, notify 동일): 빈 해석은 커스텀 폰트 스킵(내장 기본
    // 글리프 — AddFont 시점 assert 사망 대신 열화).
    ImGuiIO& io = ImGui::GetIO();
    const std::string fontPath = jk::text::ResolveDesktopFontPath();
    if (!fontPath.empty())
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f,
                                     nullptr,
                                     io.Fonts->GetGlyphRangesKorean());

    RefreshList();
}

void ClientShotApp::OnClose() {
    DropTexture();
    if (imguiReady_) {
        ImGui_ImplJKWindow_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
}

bool ClientShotApp::PreProcessMessage(const JKEvent& ev) {
    ImGui_ImplJKWindow_ProcessJKEvent(ev);
    if (ev.type == JKEventType::Timer) {
        frameDirty_ = true;
    }
    return true;
}

void ClientShotApp::OnFrameCommitted() {
    frameDirty_ = false;
}

void ClientShotApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
    if (!imguiReady_) {
        if (!ImGui_ImplJKWindow_Init(renderer))
            return;
        imguiReady_ = true;
    }

    ImGui_ImplJKWindow_NewFrame(1.0f / 60.0f, w, h);
    ImGui::NewFrame();
    renderer_ = renderer;
    BuildUi(w, h);
    ImGui::Render();
    ImGui_ImplJKWindow_RenderDrawData(ImGui::GetDrawData(), renderer);
    frameDirty_ = true;
}

void ClientShotApp::RefreshList() {
    files_.clear();
    // FindFirstFileA("...\\*.png") → std::filesystem::directory_iterator
    // (stage-3 task 7). 원문 계약: 없는 폴더 = INVALID_HANDLE_VALUE 경로(목록
    // 비움+선택 해제), 디렉터리 성분 스킵, *.png 확장자(윈 패턴은 대소문자
    // 무시 — Stricmp로 등가), 열거 실패 = 종료. ec 중립형 필수 — throwing
    // 오버로드 금지(이 TU 무 try/catch).
    const std::filesystem::path dirPath =
        std::filesystem::path(ExeDir()) / "state" / "screenshots";
    const std::string dir = dirPath.string();
    std::error_code ec;
    std::filesystem::directory_iterator it(dirPath, ec);
    if (ec) {
        selectedPath_.clear();
        DropTexture();
        return;
    }
    for (const std::filesystem::directory_entry& entry : it) {
        std::error_code entryEc;
        const bool isDir = entry.is_directory(entryEc);
        if (entryEc || isDir) continue;   // 열거 스캔 중 소멸 성분은 스킵
        const std::string name = entry.path().filename().string();
        if (jk::crt::Stricmp(entry.path().extension().string().c_str(),
                             ".png") != 0)
            continue;
        files_.push_back(name);
    }
    // shot_<epoch-ms>_... names sort lexically by time — reverse for
    // newest-first.
    std::sort(files_.begin(), files_.end(), std::greater<std::string>());
    // The shown file may have been deleted out from under the viewer.
    if (!selectedPath_.empty() && selectedPath_.size() > dir.size() &&
        std::find(files_.begin(), files_.end(),
                  selectedPath_.substr(dir.size() + 1)) == files_.end()) {
        selectedPath_.clear();
        DropTexture();
    }
}

void ClientShotApp::DropTexture() {
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

void ClientShotApp::SelectFile(const std::string& fullPath) {
    selectedPath_ = fullPath;
    DropTexture();
    if (!LoadImageFile(fullPath, current_)) {
        return;
    }
}

void ClientShotApp::BuildUi(int w, int h) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
    if (ImGui::Begin("shot", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
        // The server reserves the top title band of every surface as
        // title-bar chrome — mouse-downs there never reach the client
        // (vplayer lesson 8). First row starts below the strip or its
        // buttons are dead. (T3 fix r1: 고정 30 → 밴드 산식 진실원,
        // s=1.0 등호 30.)
        ImGui::SetCursorPosY(static_cast<float>(jk::text::AppContentTopOffset()));
        // Header: refresh + spawn the rubber-band overlay.
        if (ImGui::Button("새로고침")) {
            RefreshList();
        }
        ImGui::SameLine();
        if (ImGui::Button("영역 캡처")) {
            if (jk::client::JKClientSurface* surface = Surface()) {
                // Fire-and-forget: the reply sits in the queue (64 deep) —
                // the overlay closes itself when it lands.
                surface->SendAgentQuery(1, "{\"tool\":\"launch_app\","
                                          "\"args\":{\"app\":\"snap\"}}");
            }
        }

        // Defer the texture upload to here — the renderer only exists inside
        // RenderOverlay (stashed in renderer_). One streaming texture per
        // selection.
        if (!texture_ && !current_.rgba.empty() && current_.w > 0 && renderer_) {
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

        // Image area: decode-fit to the 460 px column.
        const float availW = ImGui::GetContentRegionAvail().x;
        const float availH = ImGui::GetContentRegionAvail().y - 96.0f;
        if (texture_) {
            float scale = availW / static_cast<float>(texW_);
            if (texH_ * scale > availH) {
                scale = availH / static_cast<float>(texH_);
            }
            ImGui::Image((ImTextureID)texture_,
                         ImVec2(texW_ * scale, texH_ * scale));
        } else {
            ImGui::TextUnformatted(selectedPath_.empty()
                                       ? "저장된 스크린샷이 없습니다"
                                       : "이미지를 열 수 없습니다");
        }

        // File list, newest first.
        ImGui::Separator();
        if (ImGui::BeginChild("files", ImVec2(0, 0),
                              ImGuiChildFlags_Borders)) {
            for (const std::string& name : files_) {
                // RefreshList와 같은 fs::path 합성 — 구분자가 플랫폼 몫이라
                // selectedPath_ 비교(SelectFile)가 열거 경로와 정확히 일치한다.
                const std::string full =
                    (std::filesystem::path(ExeDir()) / "state" / "screenshots" /
                     name)
                        .string();
                const bool selected = (full == selectedPath_);
                if (ImGui::Selectable(name.c_str(), selected)) {
                    SelectFile(full);
                }
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();
}


// P3 theme hot-swap (docs/52): the palette was snapshotted into ImGuiStyle
// at OnInit - re-apply it after a preset swap.
void ClientShotApp::OnThemeChanged() { jk::theme::ApplyImGuiTheme(); }

} // namespace jk