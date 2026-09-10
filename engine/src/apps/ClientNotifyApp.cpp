// Notification center client (agent platform, docs/33). Structure mirrors
// ClientPaletteApp (docs/23 §11.2 taskmgr clock): 16 ms timer re-arms the
// frame gate, every event feeds the ImGui backend before NewFrame, the root
// paints the dark clear color.
#include <apps/ClientNotifyApp.h>

#include <imgui_impl_jkwindow.h>
#include <JKWindow.h>
#include <SDL.h>

namespace jk {
namespace {
// Root window paints the dark clear color so the surface never flashes white
// behind ImGui's rounded windows (same idiom as the palette).
class NotifyRoot : public JKWindow {
public:
    explicit NotifyRoot(const std::string& title) : JKWindow(title) {}
    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetClientRect();
        dc.SetColor(24, 24, 30, 255);
        dc.FillRect(client);
    }
};
} // namespace

ClientNotifyApp::~ClientNotifyApp() = default;

void ClientNotifyApp::OnInit() {
    auto main = std::make_unique<NotifyRoot>("Notifications");
    main->SetWindowRect(JKRect{ 0, 0, 420, 560 });
    main->SetAttrFlags(WA_CHROMELESS);
    SetMainWindow(std::move(main));

    SetTimerInterval(16); // ~60 Hz frame cadence (taskmgr clock)

    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    // Notifications carry Korean (trigger titles, [알림] bodies) — the ImGui
    // default font is ASCII-only, so load Malgun Gothic with the Korean
    // ranges. Failure degrades to the default font (English-only UI).
    ImGuiIO& io = ImGui::GetIO();
    if (io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f,
                                     nullptr,
                                     io.Fonts->GetGlyphRangesKorean())) {
        koreanFont_ = true;
    }
    // Opt into desktop events on the window connection (M2a relaxed push —
    // the palette set the precedent for a regular client subscriber).
    if (jk::client::JKClientSurface* surface = Surface()) {
        surface->SendAgentEventSubscribe(true);
    }
}

void ClientNotifyApp::OnClose() {
    if (imguiReady_) {
        ImGui_ImplJKWindow_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
}

bool ClientNotifyApp::PreProcessMessage(const JKEvent& ev) {
    ImGui_ImplJKWindow_ProcessJKEvent(ev);
    if (ev.type == JKEventType::Timer) {
        frameDirty_ = true;
    }
    return true;
}

void ClientNotifyApp::OnFrameCommitted() {
    frameDirty_ = false;
}

void ClientNotifyApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
    if (!imguiReady_) {
        if (!ImGui_ImplJKWindow_Init(renderer))
            return;
        imguiReady_ = true;
    }
    DrainEvents();

    ImGui_ImplJKWindow_NewFrame(1.0f / 60.0f, w, h);
    ImGui::NewFrame();

    BuildUi(w, h);

    ImGui::Render();
    ImGui_ImplJKWindow_RenderDrawData(ImGui::GetDrawData(), renderer);
}

void ClientNotifyApp::BuildUi(int w, int h) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
    if (ImGui::Begin("notify", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
        ImGui::Text("notifications%s", koreanFont_ ? "" : " (ascii font)");
        if (ImGui::BeginChild("preview", ImVec2(0, 0),
                              ImGuiChildFlags_Borders)) {
            for (const auto& line : preview_)
                ImGui::TextWrapped("%s", line.c_str());
            if (scrollDirty_) {
                ImGui::SetScrollHereY(1.0f);
                scrollDirty_ = false;
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

void ClientNotifyApp::DrainEvents() {
    jk::client::JKClientSurface* surface = Surface();
    std::vector<std::string> events;
    if (!surface || surface->DrainAgentEvents(events) == 0) return;
    for (const std::string& e : events) preview_.push_back(e);
    while (preview_.size() > 12) preview_.erase(preview_.begin());
    frameDirty_ = true;
}

} // namespace jk