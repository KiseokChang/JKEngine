// 시계 앱 (2026-11-16 사용자 판정: "트레이 시계를 클릭하면 뜨는 앱").
// 데스크톱 우상단 대형 시계 위젯을 없애고 시간 표시를 앱 창 하나로 모은다.
// 서버 도구 접촉 0 — 시간·날짜만 그리는 순수 클라이언트.
#include <apps/ClientClockApp.h>
#include <text/JKImGuiFont.h>  // ImGui 한글 폰트 정본

#include <client/JKClientSurface.h>
#include <imgui_impl_jkwindow.h>
#include "theme/JKThemeImGui.h"
#include <JKWindow.h>
#include <SDL.h>

#include <cstdio>
#include <ctime>

namespace jk {
namespace {
// 루트 = 테마 클리어색(뒤가 흰색으로 번쩍이지 않게 — shot/notify 동형).
class ClockRoot : public JKWindow {
public:
    explicit ClockRoot(const std::string& title) : JKWindow(title) {}
    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetClientRect();
        const auto& t = jk::theme::current();
        dc.SetColor(t.appClearBg.r, t.appClearBg.g, t.appClearBg.b, 255);
        dc.FillRect(client);
    }
};
} // namespace

ClientClockApp::~ClientClockApp() = default;

void ClientClockApp::OnInit() {
    auto main = std::make_unique<ClockRoot>("시계");
    main->SetWindowRect(JKRect{ 0, 0, 420, 260 });
    main->SetAttrFlags(WA_CHROMELESS);
    SetMainWindow(std::move(main));

    SetTimerInterval(200);   // 초 경계 감지용 — 1초 미만이면 충분

    ImGui::CreateContext();
    jk::theme::ApplyImGuiTheme();
    ImGui::GetIO().IniFilename = nullptr;
    ImGuiIO& io = ImGui::GetIO();
    (void)jk::text::AddImGuiKoreanFont(io.Fonts);
}

void ClientClockApp::OnClose() {
    if (imguiReady_) {
        ImGui_ImplJKWindow_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
}

void ClientClockApp::OnThemeChanged() {
    if (imguiReady_) jk::theme::ApplyImGuiTheme();
    frameDirty_ = true;
}

bool ClientClockApp::PreProcessMessage(const JKEvent& ev) {
    ImGui_ImplJKWindow_ProcessJKEvent(ev);
    return true;
}

void ClientClockApp::OnFrameCommitted() { frameDirty_ = false; }

void ClientClockApp::BuildUi(int w, int h) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(w), static_cast<float>(h)));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar;
    if (!ImGui::Begin("##clock", nullptr, flags)) {
        ImGui::End();
        return;
    }

    time_t now = time(nullptr);
    tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &now);
#else
    tm* pp = std::localtime(&now);
    if (pp) lt = *pp;
#endif

    // 큰 시간 — 창 폭에 맞춰 스케일(폰/TV 어디서도 읽히게).
    const float big = h * 0.42f;
    ImGui::PushFont(nullptr);
    const std::string hh = [&] {
        char b[8];
        std::snprintf(b, sizeof(b), "%02d:%02d", lt.tm_hour, lt.tm_min);
        return std::string(b);
    }();
    const ImVec2 sz = ImGui::CalcTextSize(hh.c_str(), nullptr, false, big);
    ImGui::SetCursorPosX((w - sz.x * 2.4f) * 0.5f);
    ImGui::SetWindowFontScale(2.4f * (big / 48.0f) + 0.6f);
    ImGui::TextUnformatted(hh.c_str());
    ImGui::SetWindowFontScale(1.0f);

    static const char* kWeek[] = {"일", "월", "화", "수", "목", "금", "토"};
    char date[64];
    std::snprintf(date, sizeof(date), "%d년 %d월 %d일 (%s)",
                  lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                  kWeek[(lt.tm_wday + 7) % 7]);
    const ImVec2 ds = ImGui::CalcTextSize(date);
    ImGui::SetCursorPosX((w - ds.x) * 0.5f);
    ImGui::TextUnformatted(date);

    char sec[16];
    std::snprintf(sec, sizeof(sec), "%02d초", lt.tm_sec);
    const ImVec2 ss = ImGui::CalcTextSize(sec);
    ImGui::SetCursorPosX((w - ss.x) * 0.5f);
    ImGui::TextDisabled("%s", sec);

    ImGui::PopFont();
    ImGui::End();
}

void ClientClockApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
    if (!imguiReady_) {
        if (!ImGui_ImplJKWindow_Init(renderer)) return;
        imguiReady_ = true;
    }

    // 1초 경계에서만 다시 그린다 — 정지 화면 프레임 낭비 없음(활동 게이트 계약).
    const long long sec = static_cast<long long>(time(nullptr));
    if (sec != lastSecond_) {
        lastSecond_ = sec;
        frameDirty_ = true;
    }

    ImGui_ImplJKWindow_NewFrame(1.0f / 60.0f, w, h);
    ImGui::NewFrame();
    BuildUi(w, h);
    ImGui::Render();
    ImGui_ImplJKWindow_RenderDrawData(ImGui::GetDrawData(), renderer);
}

} // namespace jk
