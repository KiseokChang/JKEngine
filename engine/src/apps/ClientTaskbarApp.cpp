// Desktop shell / taskbar client (docs/28). Registers the shell role right
// after connecting, renders one button per window from the server's
// WindowList snapshots, and sends WindowActivate on click.
#include <apps/ClientTaskbarApp.h>

#include "theme/JKTheme.h"

#include <JKHangulUtil.h>
#include <net/JKNet.h>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <thread>

namespace jk {

namespace {
// Bar/button geometry (logical points).
constexpr int kButtonHeight = 44;   // 셸 2.0 S2 — 바 56px(SendShellRegister 갱신).
constexpr int kClockW = 76;         // 시계 셀(우측 고정 — S3).
constexpr int kButtonGap = 4;
constexpr int kBarMargin = 4;
constexpr int kButtonMaxWidth = 180;   // Windows-style cap
constexpr int kButtonMinWidth = 40;    // overflow floor (docs/28): 축소 하한
constexpr int kChipSize = 10;
constexpr int kChipPad = 6;

// [S3 — 시계 셀 텍스트] HH:MM 24시(로컬).
std::string CurrentClockText() {
    time_t now = time(nullptr);
    tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &now);
#else
    tm* pp = std::localtime(&now);
    if (pp) lt = *pp;
#endif
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", lt.tm_hour, lt.tm_min);
    return buf;
}

// FNV-1a title hash -> stable per-window chip color.
void TitleChipColor(const std::string& title, uint8_t& r, uint8_t& g, uint8_t& b) {
    uint32_t h = 2166136261u;
    for (char c : title) {
        h ^= static_cast<uint8_t>(c);
        h *= 16777619u;
    }
    r = static_cast<uint8_t>(96 + ((h >> 16) & 63));
    g = static_cast<uint8_t>(96 + ((h >> 8) & 63));
    b = static_cast<uint8_t>(96 + (h & 63));
}

// 네비 셀 지시 채널(스펙 §3 — 수단 통일 계약): :8091/talk POST 단발 —
// 라우터 exact 행("브라우저 뒤로"·"바탕화면으로"·"창 목록")이 소비한다.
// 워커 스레드 단발(detach) — UI 스레드 블록 금지. 실패 = 조용한 포기
// (콘솔 행 없음 — 컨트롤 앱/구두 폴백이 남는다).
void PostTalkAsync(const std::string& text) {
    std::thread([text] {
        if (!jk::net::Startup()) return;
        jk::net::Socket s = jk::net::ConnectTcp("127.0.0.1", 8091);
        if (s == jk::net::kInvalidSocket) return;
        jk::net::SetTimeouts(s, 5000);
        const std::string body = "{\"text\":\"" + text + "\"}";
        std::string req =
            "POST /talk HTTP/1.1\r\n"
            "Host: 127.0.0.1:8091\r\n"
            "Content-Type: application/json\r\n"
            "Connection: close\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
        if (jk::net::Send(s, req.data(), static_cast<int>(req.size())) <= 0) {
            jk::net::Close(s);
            return;
        }
        char buf[256];
        (void)jk::net::Recv(s, buf, sizeof(buf));  // 1회 회신 폐기(단발 소화)
        jk::net::Close(s);
    }).detach();
}
} // namespace

ClientTaskbarApp::~ClientTaskbarApp() = default;

void ClientTaskbarApp::OnInit() {
    auto main = std::make_unique<TaskbarWindow>("Taskbar");
    main->SetWindowRect(JKRect{ 0, 0, 1280, 40 });
    main->SetAttrFlags(WA_CHROMELESS);

    // 네비 셀 3종(스펙 §4 — DeX 하단바 관례 순서: 뒤·홈·최근 — 좌측 고정).
    // 창 버튼 pool과 별개 — Relayout에서 스트립 폭만큼 창 버튼을 미끼친다.
    struct NavCell { const char* label; const char* say; TaskbarButton** dst; };
    const NavCell nav[] = {
        { "뒤",  "브라우저 뒤로",   &navBack_ },
        { "홈",  "바탕화면으로",    &navHome_ },
        { "최근", "창 목록",        &navRecents_ },
    };
    for (const NavCell& n : nav) {
        auto btn = std::make_unique<TaskbarButton>();
        btn->SetNavMode(true);
        btn->SetText(n.label);
        TaskbarButton* raw = btn.get();
        const char* say = n.say;
        raw->SetOnClickWindow([say](uint32_t, bool) {
            PostTalkAsync(say);
        });
        main->AddControl(std::move(btn));
        *n.dst = raw;
    }
    {
        // [S5 정리 셀] 창 배치 저장 서버 도구 위임(save_layout 라우터 재용).
        auto btn = std::make_unique<TaskbarButton>();
        btn->SetNavMode(true);
        btn->SetText("정리");
        cleanupCell_ = btn.get();
        cleanupCell_->SetOnClickWindow([](uint32_t, bool) {
            PostTalkAsync("창 정리해줘");
        });
        main->AddControl(std::move(btn));
    }
    // [S3 시계 셀 = 제거 — 2026-11-09 실측 원존] 자식 프로세스 KSSM 텍스트
    // 미렌더(빈 셀 원문) — 바탕화면 시계 위젯(S4)이 대체.

    // Button pool (WindowListPayload caps at 32 windows).
    for (size_t i = 0; i < 32; ++i) {
        auto btn = std::make_unique<TaskbarButton>();
        btn->SetOnClickWindow([this](uint32_t surfaceId, bool wasActive) {
            jk::client::JKClientSurface* surface = Surface();
            if (!surface) return;
            if (wasActive) {
                surface->SendWindowMinimizeToggle(surfaceId);
            } else {
                surface->SendWindowActivate(surfaceId);
            }
        });
        btn->Hide();
        TaskbarButton* raw = btn.get();
        main->AddControl(std::move(btn));
        buttons_.push_back(raw);
    }

    SetMainWindow(std::move(main));

    // Claim the shell role: bottom edge, 40pt requested thickness. The
    // server acks (accepted/denied) and replies with an initial WindowList
    // snapshot; the shell layer goes topmost from here on.
    jk::client::JKClientSurface* surface = Surface();
    if (!surface || !surface->SendShellRegister(0 /*bottom*/, 56)) {
        std::fprintf(stderr, "[taskbar] SendShellRegister failed\n");
        std::fflush(stderr);
    }
}

void ClientTaskbarApp::OnClose() {
}

bool ClientTaskbarApp::PreProcessMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::WindowListChanged) {
        RefreshWindowList();
        return true;
    }

    // Docking resize: the server reshapes the bar to the desktop width —
    // re-flow the buttons into the new client rect.
    if (ev.type == JKEventType::SizeChanged) {
        Relayout();
        return true;
    }
    return true;
}

void ClientTaskbarApp::RefreshWindowList() {
    jk::client::JKClientSurface* surface = Surface();
    if (!surface || !surface->GetWindowList(windows_)) {
        return;
    }

    const size_t count = std::min(windows_.size(), buttons_.size());
    for (size_t i = 0; i < buttons_.size(); ++i) {
        TaskbarButton* btn = buttons_[i];
        if (i < count) {
            const auto& w = windows_[i];
            btn->Bind(w.surfaceId, w.title,
                      (w.flags & 0x1u) != 0,   // ipc::kShellWindowActive
                      (w.flags & 0x2u) != 0);  // ipc::kShellWindowMinimized
            btn->Show();
        } else {
            btn->Hide();
        }
    }
    Relayout();
}

// Overflow rule (docs/28, 최소 너비 보장형 동적 축소): split the available
// width across buttons, cap at kButtonMaxWidth, never below kButtonMinWidth.
// Titles wider than the button are simply clipped by TextOutX.
// 창 개수 무관 항상 보이는 네비 스트립 폭(뒤·홈·최근 — DeX 하단바 관례).
void ClientTaskbarApp::Relayout() {
    JKWindow* win = GetMainWindow();
    if (!win) return;
    const JKRect client = win->GetClientRect();

    constexpr int kNavW = 44;
    constexpr int kNavCount = 3;
    const int navStrip = kNavW * kNavCount + kButtonGap * (kNavCount - 1);

    // 네비 셀 — 좌측 뒤·홈·최근, 그 옆 정리(S5), 우측 끝 시계(S3).
    int x = kBarMargin;
    const int y = std::max(0, (client.h - kButtonHeight) / 2);
    TaskbarButton* navs[] = { navBack_, navHome_, navRecents_ };
    for (TaskbarButton* nav : navs) {
        if (nav) {
            nav->SetRect(JKRect{ x, y, kNavW, kButtonHeight });
            nav->Show();
        }
        x += kNavW + kButtonGap;
    }
    if (cleanupCell_) {
        cleanupCell_->SetRect(JKRect{ x, y, kNavW, kButtonHeight });
        cleanupCell_->Show();
        x += kNavW + kButtonGap;
    }
    // 창 버튼 — 시계를 피해 배치(avail에서 kClockW 차감 — 위).

    size_t count = 0;
    for (TaskbarButton* btn : buttons_) {
        if (btn->IsVisible()) ++count;
    }
    if (count == 0) { win->Invalidate(); return; }

    const int avail = client.w - kBarMargin * 2 - navStrip
                      - kButtonGap * static_cast<int>(count - 1);
    int btnW = avail / static_cast<int>(count);
    btnW = std::max(kButtonMinWidth, std::min(kButtonMaxWidth, btnW));

    for (TaskbarButton* btn : buttons_) {
        if (!btn->IsVisible()) continue;
        btn->SetRect(JKRect{ x, y, btnW, kButtonHeight });
        x += btnW + kButtonGap;
    }
    win->Invalidate();
}

// --- TaskbarButton -------------------------------------------------------

void TaskbarButton::Bind(uint32_t surfaceId, const std::string& title,
                         bool active, bool minimized) {
    surfaceId_ = surfaceId;
    active_ = active;
    minimized_ = minimized;
    SetText(title);
}

void TaskbarButton::OnClick() {
    if (onClickWindow_) {
        onClickWindow_(surfaceId_, active_);
    }
}

void TaskbarButton::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    if (client.IsEmpty()) return;

    // 네비 셀(스펙 §4) — 의미 그림(관례 글리프)+소형 라벨: 뒤=◀(좌향 삼각형
    // 세로 절단 스윕), 홈=◯(Android HOME 관례 동형 원), 최근=겹쳐린 두 창
    // (Recents 관례). 라벨 = 셀 하단 한글(그림 만으로 의미가 끊기지 않게 —
    // 사용자 원존 "그림도 없고 설명도 없고 → 의미 있게 그려주세요").
    if (navMode_) {
        const auto& t = jk::theme::current();
        dc.SetColor(t.taskbarFaceActive.r, t.taskbarFaceActive.g,
                    t.taskbarFaceActive.b, 255);
        dc.FillRect(client);
        dc.SetColor(t.taskbarBorderActive.r, t.taskbarBorderActive.g,
                    t.taskbarBorderActive.b, 255);
        dc.DrawRect(client);
        dc.SetColor(t.taskbarTextActive.r, t.taskbarTextActive.g,
                    t.taskbarTextActive.b, 255);
        dc.SetTextColor(t.taskbarTextActive.r, t.taskbarTextActive.g,
                        t.taskbarTextActive.b);
        const int cx = client.x + client.w / 2;
        const int cy = client.y + (client.h - 14) / 2 + 2;
        const std::string& label = GetText();
        if (label == "뒤") {
            // ◀ — 세로 1px 절단 스윕(왼쪽 끝=끝점·오른쪽 끝=만폭).
            for (int j = 0; j < 10; ++j) {
                const int half = 6 * j / 10;
                dc.FillRect(JKRect{ cx - 6 + j, cy - half, 1, half * 2 + 1 });
            }
        } else if (label == "홈") {
            // ◯ — Android HOME 관례 동형(수평 절단 스윕 원).
            for (int dy = -5; dy <= 5; ++dy) {
                const int w2 = static_cast<int>(
                    std::lround(std::sqrt(25.0 - dy * dy)));
                dc.FillRect(JKRect{ cx - w2, cy + dy, w2 * 2, 1 });
            }
        } else if (label == "최근") {
            // ▤ — Recents 관례 동형(겹쳐진 두 창 외곽선).
            dc.DrawRect(JKRect{ cx - 7, cy - 7, 10, 10 });
            dc.DrawRect(JKRect{ cx - 3, cy - 3, 10, 10 });
        } else if (label == "정리") {
            // ‖ — 정렬(창 배치 save_layout) 기호(두 수직 바).
            dc.FillRect(JKRect{ cx - 5, cy - 6, 3, 12 });
            dc.FillRect(JKRect{ cx + 2, cy - 6, 3, 12 });
        } else if (label.size() == 5 && label[2] == ':') {
            // [S3 시계] HH:MM — 글리프 없이 숫자 중앙 대형 낭독.
            dc.TextOutX(client, Utf8ToKssm(label.c_str()).c_str(),
                        ADJ_YCENTER, false);
            return;
        } else {
            // ▤ — Recents 관례 동형(겹쳐진 두 창 외곽선).
            dc.DrawRect(JKRect{ cx - 7, cy - 7, 10, 10 });
            dc.DrawRect(JKRect{ cx - 3, cy - 3, 10, 10 });
        }
        // 소형 라벨(셀 하단 한글 — KSSM 비트맵 최소 가시 높이 여유 14px).
        const JKRect textRect{ client.x, client.y + client.h - 14,
                               client.w, 14 };
        dc.TextOutX(textRect, Utf8ToKssm(label.c_str()).c_str(),
                    ADJ_YCENTER, false);
        return;
    }

    // Variant faces (docs/28): active = light + white border,
    // minimized = dark + dim text, normal = mid grey.
    // colors: jk::theme::current() (P2) — 값 불변 토큰화.
    const auto& t = jk::theme::current();
    uint8_t fr, fg, fb, tr, tg, tb;
    if (active_) {
        fr = t.taskbarFaceActive.r; fg = t.taskbarFaceActive.g; fb = t.taskbarFaceActive.b;
        tr = t.taskbarTextActive.r; tg = t.taskbarTextActive.g; tb = t.taskbarTextActive.b;
    } else if (minimized_) {
        fr = t.taskbarFaceMinimized.r; fg = t.taskbarFaceMinimized.g; fb = t.taskbarFaceMinimized.b;
        tr = t.taskbarTextMinimized.r; tg = t.taskbarTextMinimized.g; tb = t.taskbarTextMinimized.b;
    } else {
        fr = t.taskbarFaceNormal.r; fg = t.taskbarFaceNormal.g; fb = t.taskbarFaceNormal.b;
        tr = t.taskbarTextNormal.r; tg = t.taskbarTextNormal.g; tb = t.taskbarTextNormal.b;
    }
    dc.SetColor(fr, fg, fb, 255);
    dc.FillRect(client);
    const SDL_Color& border = active_ ? t.taskbarBorderActive : t.taskbarBorderInactive;
    dc.SetColor(border.r, border.g, border.b, 255);
    dc.DrawRect(client);

    // Title hash-color chip.
    const std::string& title = GetText();
    uint8_t cr = 0, cg = 0, cb = 0;
    TitleChipColor(title, cr, cg, cb);
    dc.SetColor(cr, cg, cb, 255);
    const JKRect chip{ client.x + kChipPad,
                       client.y + (client.h - kChipSize) / 2,
                       kChipSize, kChipSize };
    dc.FillRect(chip);

    // Title text (UTF-8 -> KSSM for the bitmap font; clipped to the button).
    if (!title.empty()) {
        dc.SetTextColor(tr, tg, tb);
        const JKRect textRect{ client.x + kChipPad * 2 + kChipSize, client.y,
                               client.w - (kChipPad * 3 + kChipSize), client.h };
        if (textRect.w > 0) {
            dc.TextOutX(textRect, Utf8ToKssm(title.c_str()).c_str(),
                        ADJ_YCENTER, false);
        }
    }
}

// --- TaskbarWindow -------------------------------------------------------

void ClientTaskbarApp::TaskbarWindow::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    const auto& t = jk::theme::current();
    dc.SetColor(t.taskbarBg.r, t.taskbarBg.g, t.taskbarBg.b, 255);
    dc.FillRect(client);
    for (const auto& child : GetChildren()) {
        if (child->IsVisible()) {
            child->PaintClient(dc);
        }
    }
}

} // namespace jk
