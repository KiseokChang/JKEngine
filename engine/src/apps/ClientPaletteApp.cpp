// Command palette client (agent platform M2a, spec §6.2). See
// ClientPaletteApp.h. Structure mirrors ClientTaskmgrApp (docs/23 §11.2):
// 16 ms timer re-arms the frame gate, every event is fed to the ImGui
// backend before NewFrame, the root paints the dark clear color.
#include <apps/ClientPaletteApp.h>

#include <imgui_impl_jkwindow.h>
#include <JKWindow.h>
#include <SDL.h>
#include <cctype>
#include <cstdio>

namespace jk {
namespace {
// Root window paints the dark clear color so the surface never flashes white
// behind ImGui's rounded windows (same idiom as the Phase 1 demo).
class PaletteRoot : public JKWindow {
public:
    explicit PaletteRoot(const std::string& title) : JKWindow(title) {}
    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetClientRect();
        dc.SetColor(24, 24, 30, 255);
        dc.FillRect(client);
    }
};

std::string Trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}
} // namespace

ClientPaletteApp::~ClientPaletteApp() = default;

void ClientPaletteApp::OnInit() {
    auto main = std::make_unique<PaletteRoot>("Command Palette");
    main->SetWindowRect(JKRect{ 0, 0, 520, 360 });
    main->SetAttrFlags(WA_CHROMELESS);
    SetMainWindow(std::move(main));

    SetTimerInterval(16); // ~60 Hz frame cadence (taskmgr clock)

    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    lastFrame_ = std::chrono::steady_clock::now();
    AppendLog("command palette ready - type /help");
}

void ClientPaletteApp::OnClose() {
    if (imguiReady_) {
        ImGui_ImplJKWindow_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
}

bool ClientPaletteApp::PreProcessMessage(const JKEvent& ev) {
    // Every event goes to the backend before the next NewFrame consumes the
    // input queue (docs/23 §5.2-3). The 16 ms timer is the frame clock —
    // re-arm the frame gate here (docs/23 §11.5 lesson 5).
    ImGui_ImplJKWindow_ProcessJKEvent(ev);
    if (ev.type == JKEventType::Timer) {
        frameDirty_ = true;
    }
    return true;
}

void ClientPaletteApp::OnFrameCommitted() {
    frameDirty_ = false;
}

void ClientPaletteApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
    if (!imguiReady_) {
        if (!ImGui_ImplJKWindow_Init(renderer))
            return;
        imguiReady_ = true;
    }

    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - lastFrame_).count();
    lastFrame_ = now;

    ImGui_ImplJKWindow_NewFrame(dt, w, h);
    ImGui::NewFrame();

    BuildUi(w, h);

    ImGui::Render();
    ImGui_ImplJKWindow_RenderDrawData(ImGui::GetDrawData(), renderer);
}

void ClientPaletteApp::BuildUi(int w, int h) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
    if (ImGui::Begin("palette", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
        if (focusInput_) {
            ImGui::SetKeyboardFocusHere();
            focusInput_ = false;
        }
        if (ImGui::InputText("##cmd", input_, sizeof(input_),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            const std::string text = Trim(input_);
            input_[0] = '\0';
            if (!text.empty()) {
                AppendLog("> " + text);
                if (text == "/help") {
                    AppendLog("  /list  /launch <app>  /close <id>");
                    AppendLog("  /save <name>  /restore <name>  /undo");
                } else {
                    AppendLog("  (agent queries arrive in the next task)");
                }
            }
            ImGui::SetKeyboardFocusHere();  // stay in the box after Enter
            focusInput_ = true;
        }
        ImGui::BeginChild("log", ImVec2(0, 0));
        for (const auto& line : log_) ImGui::TextWrapped("%s", line.c_str());
        if (scrollDirty_) {
            ImGui::SetScrollHereY(1.0f);
            scrollDirty_ = false;
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

void ClientPaletteApp::AppendLog(const std::string& line) {
    log_.push_back(line);
    scrollDirty_ = true;
}

} // namespace jk