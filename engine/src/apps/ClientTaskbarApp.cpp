// Desktop shell / taskbar client (docs/28). Registers the shell role right
// after connecting, renders one button per window from the server's
// WindowList snapshots, and sends WindowActivate on click.
#include <apps/ClientTaskbarApp.h>

#include <JKHangulUtil.h>
#include <cstdio>
#include <algorithm>

namespace jk {

namespace {
// Bar/button geometry (logical points).
constexpr int kButtonHeight = 30;
constexpr int kButtonGap = 4;
constexpr int kBarMargin = 4;
constexpr int kButtonMaxWidth = 180;   // Windows-style cap
constexpr int kButtonMinWidth = 40;    // overflow floor (docs/28): 축소 하한
constexpr int kChipSize = 10;
constexpr int kChipPad = 6;

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
} // namespace

ClientTaskbarApp::~ClientTaskbarApp() = default;

void ClientTaskbarApp::OnInit() {
    auto main = std::make_unique<TaskbarWindow>("Taskbar");
    main->SetWindowRect(JKRect{ 0, 0, 1280, 40 });
    main->SetAttrFlags(WA_CHROMELESS);

    // Button pool (WindowListPayload caps at 32 windows).
    for (size_t i = 0; i < 32; ++i) {
        auto btn = std::make_unique<TaskbarButton>();
        btn->SetOnActivate([this](uint32_t surfaceId) {
            if (jk::client::JKClientSurface* surface = Surface()) {
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
    if (!surface || !surface->SendShellRegister(0 /*bottom*/, 40)) {
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
void ClientTaskbarApp::Relayout() {
    JKWindow* win = GetMainWindow();
    if (!win) return;
    const JKRect client = win->GetClientRect();

    size_t count = 0;
    for (TaskbarButton* btn : buttons_) {
        if (btn->IsVisible()) ++count;
    }
    if (count == 0) return;

    const int avail = client.w - kBarMargin * 2 - kButtonGap * static_cast<int>(count - 1);
    int btnW = avail / static_cast<int>(count);
    btnW = std::max(kButtonMinWidth, std::min(kButtonMaxWidth, btnW));

    int x = kBarMargin;
    const int y = std::max(0, (client.h - kButtonHeight) / 2);
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
    if (onActivate_) {
        onActivate_(surfaceId_);
    }
}

void TaskbarButton::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    if (client.IsEmpty()) return;

    // Variant faces (docs/28): active = light + white border,
    // minimized = dark + dim text, normal = mid grey.
    uint8_t fr, fg, fb, tr, tg, tb;
    if (active_) {
        fr = 92; fg = 98; fb = 122;
        tr = 255; tg = 255; tb = 255;
    } else if (minimized_) {
        fr = 38; fg = 40; fb = 46;
        tr = 120; tg = 120; tb = 120;
    } else {
        fr = 56; fg = 58; fb = 68;
        tr = 224; tg = 224; tb = 224;
    }
    dc.SetColor(fr, fg, fb, 255);
    dc.FillRect(client);
    dc.SetColor(active_ ? 255 : 70, active_ ? 255 : 72, active_ ? 255 : 84, 255);
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
    dc.SetColor(24, 26, 32, 255);
    dc.FillRect(client);
    for (const auto& child : GetChildren()) {
        if (child->IsVisible()) {
            child->PaintClient(dc);
        }
    }
}

} // namespace jk