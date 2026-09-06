#include <apps/ClientImGuiDemoApp.h>

#include <imgui_impl_jkwindow.h>
#include <imgui_memory_editor.h>
#include <JKWindow.h>
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <cstring>
#include <stdlib.h>
#include <string.h>

namespace jk {

namespace {
// Root window paints the dark clear color so the surface never flashes white
// behind ImGui's rounded windows.
class ImGuiDemoRoot : public JKWindow {
public:
    explicit ImGuiDemoRoot(const std::string& title) : JKWindow(title) {}
    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetClientRect();
        dc.SetColor(36, 36, 43, 255);
        dc.FillRect(client);
    }
};

// Phase 3 ecosystem fixture (docs/23 §11.7): a 256-byte scratch buffer the
// in-app hex editor can read AND write — edits persist for the session so
// the panel doubles as proof that keyboard focus flows into ImGui widgets.
uint8_t g_memEditData[256];
MemoryEditor g_memEdit;

void SeedMemEditData() {
    for (int i = 0; i < 256; ++i)
        g_memEditData[i] = static_cast<uint8_t>(i);
    std::memcpy(g_memEditData, "jkwindow memory editor", 22);
}
} // namespace

ClientImGuiDemoApp::~ClientImGuiDemoApp() = default;

void ClientImGuiDemoApp::OnInit() {
    auto main = std::make_unique<ImGuiDemoRoot>("Dear ImGui Demo");
    main->SetWindowRect(JKRect{ 0, 0, 1024, 720 });
    main->SetAttrFlags(WA_CHROMELESS); // server close button only (docs/23 §9)
    SetMainWindow(std::move(main));

    SetTimerInterval(16); // ~60 Hz frame cadence

    ImGui::CreateContext();
    // No window-position persistence: a client has no writable cwd guarantee,
    // and the demo resets its layout every launch anyway (docs/23 §11.4).
    ImGui::GetIO().IniFilename = nullptr;
    lastFrame_ = std::chrono::steady_clock::now();

    // Plot ring buffer seed so PlotLines has a waveform on frame 1.
    for (int i = 0; i < 90; ++i)
        plotValues_[i] = 0.5f + 0.4f * sinf(i * 0.23f);

    SeedMemEditData();
}

void ClientImGuiDemoApp::OnClose() {
    if (imguiReady_) {
        ImGui_ImplJKWindow_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
}

bool ClientImGuiDemoApp::PreProcessMessage(const JKEvent& ev) {
    // Every event the client receives goes to the backend before the next
    // NewFrame consumes the input queue (docs/23 §5.2-3). Unhandled kinds are
    // ignored inside the backend, so blind feeding is safe.
    ImGui_ImplJKWindow_ProcessJKEvent(ev);
    // The 16ms timer is the frame clock: without re-arming here the first
    // frame would be the last (OnFrameCommitted clears the flag and nothing
    // else sets it — the run loop then idles forever).
    if (ev.type == JKEventType::Timer) {
        frameDirty_ = true;
    }
    return true;
}

void ClientImGuiDemoApp::OnFrameCommitted() {
    frameDirty_ = false;
}

void ClientImGuiDemoApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
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

void ClientImGuiDemoApp::BuildUi(int w, int h) {
    ImGuiIO& io = ImGui::GetIO();

    // Control panel (docs/23 §9): toggles + live counters + plot + IO dump.
    ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 420), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("imguidemo panel")) {
        ImGui::Text("jkwindow client + Dear ImGui %s", ImGui::GetVersion());
        ImGui::Text("surface %d x %d  |  %.1f FPS", w, h, io.Framerate);
        ImGui::Separator();
        ImGui::Checkbox("ShowDemoWindow", &showDemo_);
        ImGui::Checkbox("StyleEditor", &showStyle_);
        ImGui::Checkbox("IO dump", &showIo_);
        // imgui_club hex editor — editing writes straight into the fixture
        // buffer, so a keystroke here is visible in the same window.
        ImGui::Checkbox("Memory editor", &g_memEdit.Open);

        // Rolling waveform: proves vertex output beyond flat rects.
        plotValues_[plotOffset_] = 0.5f + 0.4f * sinf((float)ImGui::GetTime() * 2.2f)
                                 + 0.05f * ((rand() % 100) / 100.0f - 0.5f);
        plotOffset_ = (plotOffset_ + 1) % 90;
        ImGui::PlotLines("wave", plotValues_, 90, plotOffset_, nullptr, 0.0f, 1.0f, ImVec2(-1, 60));

        if (showIo_) {
            ImGui::Separator();
            ImGui::BeginChild("io", ImVec2(0, 0), ImGuiChildFlags_Borders);
            ImGui::Text("MousePos: %.1f, %.1f", io.MousePos.x, io.MousePos.y);
            ImGui::Text("MouseDown: %d%d%d%d%d", io.MouseDown[0], io.MouseDown[1],
                        io.MouseDown[2], io.MouseDown[3], io.MouseDown[4]);
            ImGui::Text("MouseWheel: %.2f %.2f", io.MouseWheelH, io.MouseWheel);
            ImGui::Text("WantCaptureMouse: %d", io.WantCaptureMouse);
            ImGui::Text("WantCaptureKeyboard: %d", io.WantCaptureKeyboard);
            ImGui::Text("WantTextInput: %d", io.WantTextInput);
            ImGui::Text("Modifiers: %s%s%s%s",
                        io.KeyCtrl ? "Ctrl " : "", io.KeyShift ? "Shift " : "",
                        io.KeyAlt ? "Alt " : "", io.KeySuper ? "Super " : "");
            ImGui::EndChild();
        }
    }
    ImGui::End();

    if (showStyle_)
        ImGui::ShowStyleEditor();

    if (showDemo_)
        ImGui::ShowDemoWindow(&showDemo_);

    // DrawWindow owns its own window (Begin inside) and syncs g_memEdit.Open
    // with its close button, so the panel checkbox above stays consistent.
    if (g_memEdit.Open)
        g_memEdit.DrawWindow("memory editor", g_memEditData,
                             sizeof(g_memEditData));
}

} // namespace jk