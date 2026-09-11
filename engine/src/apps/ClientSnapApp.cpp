// Rubber-band region-capture overlay (agent platform, docs/35 Task 3).
// Structure mirrors ClientNotifyApp (16 ms timer, ImGui over a root window)
// with two snap-specific twists: the root paints NOTHING — ComposeScene
// clears the surface to transparent (WantsTransparentSurface) and ImGui's
// dim rectangle is the only thing the desktop sees through — and the app's
// lifetime is one drag: mouse-up sends capture_region, the reply closes it.
#include <apps/ClientSnapApp.h>

#include <client/JKClientSurface.h>
#include <imgui_impl_jkwindow.h>
#include <JKWindow.h>
#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace jk {
namespace {
// Transparent root: without an override JKWindow paints chrome/background
// into the surface, which would turn the overlay into an opaque sheet.
class SnapRoot : public JKWindow {
public:
    explicit SnapRoot(const std::string& title) : JKWindow(title) {}
    void OnPaintClient(JKDC&) override {}
};

// Epoch ms (notify center idiom) — deadline math for the capture reply.
uint64_t NowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}
} // namespace

ClientSnapApp::~ClientSnapApp() = default;

void ClientSnapApp::OnInit() {
    auto main = std::make_unique<SnapRoot>("Region Capture");
    // Placeholder rect — the server resizes the surface to the output size
    // on spawn (JKWindowServer spawn placement, kCaptureOverlayTitle hook).
    main->SetWindowRect(JKRect{ 0, 0, 800, 600 });
    main->SetAttrFlags(WA_CHROMELESS);
    SetMainWindow(std::move(main));

    SetTimerInterval(16); // ~60 Hz frame cadence (notify/palette idiom)

    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    // The hint line is Korean — load Malgun Gothic like the notify center;
    // failure degrades to the default font (English-only UI).
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f,
                                 nullptr,
                                 io.Fonts->GetGlyphRangesKorean());
}

void ClientSnapApp::OnClose() {
    if (imguiReady_) {
        ImGui_ImplJKWindow_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
}

bool ClientSnapApp::PreProcessMessage(const JKEvent& ev) {
    ImGui_ImplJKWindow_ProcessJKEvent(ev);
    if (ev.type == JKEventType::Timer) {
        frameDirty_ = true;
    }
    return true;
}

void ClientSnapApp::OnFrameCommitted() {
    frameDirty_ = false;
}

void ClientSnapApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
    if (!imguiReady_) {
        if (!ImGui_ImplJKWindow_Init(renderer))
            return;
        imguiReady_ = true;
    }

    // Reply poll (palette's 1-deep pattern): the capture_region reply or the
    // 3 s deadline ends the overlay. Closing mid-frame is precedented (the
    // palette closes itself from BuildUi) — the frame just finishes first.
    bool done = false;
    if (pendingQueryId_ != 0) {
        jk::client::JKClientSurface* surface = Surface();
        jk::client::AgentReply reply;
        if (surface && surface->PollAgentReply(reply) &&
            reply.queryId == pendingQueryId_) {
            done = true;
        } else if (NowMs() >= queryDeadlineMs_) {
            done = true;
        }
    }

    if (!done) {
        ImGui_ImplJKWindow_NewFrame(1.0f / 60.0f, w, h);
        ImGui::NewFrame();
        BuildUi(w, h);
        ImGui::Render();
        ImGui_ImplJKWindow_RenderDrawData(ImGui::GetDrawData(), renderer);
        frameDirty_ = true; // keep animating until the overlay is gone
    }

    if (done) {
        CloseSelf();
    }
}

void ClientSnapApp::BuildUi(int w, int h) {
    ImGuiIO& io = ImGui::GetIO();

    // ESC cancels without capturing.
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        CloseSelf();
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    if (ImGui::Begin("snap", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoBackground)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 full((float)w, (float)h);
        // Desktop dim — the only non-transparent pixels on the surface.
        dl->AddRectFilled(ImVec2(0.0f, 0.0f), full, IM_COL32(0, 0, 0, 70));

        if (io.MouseDown[0]) {
            if (!dragging_) {
                dragging_ = true;
                dragX0_ = io.MousePos.x;
                dragY0_ = io.MousePos.y;
            }
            dragX1_ = io.MousePos.x;
            dragY1_ = io.MousePos.y;
        } else if (dragging_) {
            dragging_ = false;
            const int x = static_cast<int>(std::min(dragX0_, dragX1_));
            const int y = static_cast<int>(std::min(dragY0_, dragY1_));
            const int rw = static_cast<int>(std::abs(dragX1_ - dragX0_));
            const int rh = static_cast<int>(std::abs(dragY1_ - dragY0_));
            // Sub-4 px drags are clicks, not selections — just dismiss.
            if (rw >= 4 && rh >= 4) {
                SendCapture(x, y, rw, rh);
            } else {
                CloseSelf();
            }
        }

        if (dragging_) {
            const float x0 = std::min(dragX0_, dragX1_);
            const float y0 = std::min(dragY0_, dragY1_);
            const float rw = std::abs(dragX1_ - dragX0_);
            const float rh = std::abs(dragY1_ - dragY0_);
            // Amber selection: faint fill + 2 px border + live size readout.
            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + rw, y0 + rh),
                              IM_COL32(255, 176, 64, 30));
            dl->AddRect(ImVec2(x0, y0), ImVec2(x0 + rw, y0 + rh),
                        IM_COL32(255, 176, 64, 255), 0.0f, 0, 2.0f);
            char label[32];
            std::snprintf(label, sizeof(label), "%d × %d",
                          static_cast<int>(rw), static_cast<int>(rh));
            dl->AddText(ImVec2(x0 + 6.0f, y0 + 6.0f),
                        IM_COL32(255, 176, 64, 255), label);
        } else {
            dl->AddText(ImVec2(12.0f, 10.0f), IM_COL32(255, 255, 255, 200),
                        "영역을 드래그하세요 · ESC 취소");
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void ClientSnapApp::SendCapture(int x, int y, int w, int h) {
    jk::client::JKClientSurface* surface = Surface();
    if (!surface) {
        CloseSelf();
        return;
    }
    // Client coords == desktop logical coords: the layer sits at (0,0) with
    // scale 1 after the server's spawn resize (header comment).
    char json[160];
    std::snprintf(json, sizeof(json),
                  "{\"tool\":\"capture_region\",\"args\":{\"x\":%d,\"y\":%d,"
                  "\"w\":%d,\"h\":%d}}",
                  x, y, w, h);
    pendingQueryId_ = nextQueryId_++;
    if (!surface->SendAgentQuery(pendingQueryId_, json)) {
        CloseSelf();
        return;
    }
    queryDeadlineMs_ = NowMs() + 3000;
}

void ClientSnapApp::CloseSelf() {
    // Called from at most one place per frame (ESC in BuildUi, or the
    // reply/deadline branch in RenderOverlay) — the run loop exits before
    // the next frame, so a second Close() cannot happen.
    Close();
}

} // namespace jk