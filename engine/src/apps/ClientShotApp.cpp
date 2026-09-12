// Screenshot viewer (agent platform, docs/35 Task 4). Structure mirrors
// ClientNotifyApp (16 ms timer, ImGui over a dark root window). File list +
// decoded-image display; the 영역 캡처 button spawns the rubber-band overlay
// (launch_app snap) — one API, many faces.
#include <apps/ClientShotApp.h>

#include <client/JKClientSurface.h>
#include <imgui_impl_jkwindow.h>
#include <JKWindow.h>
#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <windows.h>

namespace jk {
namespace {
// Root window paints the dark clear color so the surface never flashes white
// behind ImGui's rounded windows (same idiom as the palette/notify).
class ShotRoot : public JKWindow {
public:
    explicit ShotRoot(const std::string& title) : JKWindow(title) {}
    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetClientRect();
        dc.SetColor(24, 24, 30, 255);
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
    ImGui::GetIO().IniFilename = nullptr;
    // Korean UI (새로고침/영역 캡처/empty-state text) — Malgun Gothic like
    // the notify center; failure degrades to the default font.
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f,
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
    const std::string dir = ExeDir() + "state\\screenshots";
    const std::string pattern = dir + "\\*.png";
    WIN32_FIND_DATAA fd;
    HANDLE find = FindFirstFileA(pattern.c_str(), &fd);
    if (find == INVALID_HANDLE_VALUE) {
        selectedPath_.clear();
        DropTexture();
        return;
    }
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        files_.push_back(fd.cFileName);
    } while (FindNextFileA(find, &fd));
    FindClose(find);
    // shot_<epoch-ms>_... names sort lexically by time — reverse for
    // newest-first.
    std::sort(files_.begin(), files_.end(), std::greater<std::string>());
    // The shown file may have been deleted out from under the viewer.
    if (!selectedPath_.empty() &&
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
        // The server reserves the top 24 px of every surface as title-bar
        // chrome — mouse-downs there never reach the client (vplayer lesson
        // 8). First row starts below the strip or its buttons are dead.
        ImGui::SetCursorPosY(30.0f);
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
                const std::string full = ExeDir() + "state\\screenshots\\" + name;
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

} // namespace jk