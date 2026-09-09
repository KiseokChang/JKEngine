#include <apps/ClientTerminalApp.h>
#include <apps/JKTerminalConfig.h>
#include <apps/TerminalView.h>
#include <JKWindow.h>

#include <chrono>
#include <cstdio>

namespace jk {

namespace {
// Coalescing windows for terminal.output publishes (M2b): batch fast shell
// output instead of one event per VT chunk.
constexpr uint64_t kPubMinIntervalMs = 250;
constexpr size_t kPubMaxBytes = 4096;
}  // namespace

ClientTerminalApp::~ClientTerminalApp() = default;

void ClientTerminalApp::OnInit() {
    // terminal.json (docs/26 단계 5 via docs/27 단계 4): every key falls back
    // to its default — a missing/malformed file never fails the startup.
    JKTerminalConfig cfg;
    if (!cfg.Load(JKTerminalConfig::DefaultPath())) {
        std::printf("[terminal] no terminal.json next to the exe — defaults\n");
        std::fflush(stdout);
    }

    grid_ = std::make_unique<JKTerminalGrid>();
    grid_->SetScrollbackMax(static_cast<size_t>(cfg.scrollback));
    parser_ = std::make_unique<JKVtParser>();
    parser_->Attach(grid_.get());
    atlas_ = std::make_unique<JKGlyphAtlas>();
    // Consolas ships with Windows; if missing the view draws placeholders.
    atlas_->Init(cfg.font.c_str(), kTermCellW, kTermCellH);
    // Malgun Gothic covers the Hangul/CJK blocks Consolas lacks (docs/26
    // 단계 1); failure only degrades wide glyphs to placeholders.
    atlas_->InitFallback(cfg.fontFallback.c_str());
    pty_ = std::make_unique<JKConPtyBridge>();
    shell_ = cfg.shell;

    auto main = std::make_unique<JKWindow>("Terminal");
    main->SetWindowRect(JKRect{ 0, 0, 800, 500 });
    const JKRect clientArea = main->GetClientRect();

    auto view = std::make_unique<TerminalView>(
        parser_.get(), grid_.get(), atlas_.get(), GetResourceCache());
    view->SetOnInput([this](const char* data, size_t len) { WriteToPty(data, len); });
    view->SetOnResize([this](int cols, int rows) { OnViewResized(cols, rows); });
    view->SetTheme(cfg.themeBg, cfg.themeFg);
    view_ = view.get();
    // Chrome-less dock-fill child (the window server owns the frame chrome);
    // same arrangement as the tetris client game window.
    view->SetAttrFlags(WA_CHROMELESS);
    view->SetWindowRect(JKRect{ 0, 0, clientArea.w, clientArea.h });
    view->SetDock(DOCK_FILL);
    main->AddControl(std::move(view));
    view_->SetFocus();

    SetMainWindow(std::move(main));
    SetTimerInterval(530);   // cursor blink
}

void ClientTerminalApp::OnClose() {
    if (pty_) {
        pty_->Stop();
    }
}

bool ClientTerminalApp::PreProcessMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::Timer && view_) {
        view_->TickBlink();
    }
    // Wheel events carry dx/dy (SDL wheel axes) and have no x/y hit target —
    // intercept them before routing so they always reach the view.
    if (ev.type == JKEventType::MouseWheel && view_) {
        view_->HandleWheel(ev.dy);
    }
    return JKClientApplication::PreProcessMessage(ev);
}

void ClientTerminalApp::OnIdle() {
    if (!ptyStarted_) {
        // Start the shell once the view reached its final layout so the
        // pty cell size matches the grid (Init relayout happens first).
        if (view_ && view_->Cols() > 0 && view_->Rows() > 0) {
            ptyCols_ = view_->Cols();
            ptyRows_ = view_->Rows();
            ptyStarted_ = pty_->Start(shell_, ptyCols_, ptyRows_);
            if (!ptyStarted_) return;
        } else {
            return;
        }
    }
    PumpPty();
}

bool ClientTerminalApp::IsFrameDirty() const {
    return firstFrame_ || (grid_ && grid_->IsDirty());
}

void ClientTerminalApp::OnFrameCommitted() {
    firstFrame_ = false;
    if (grid_) {
        grid_->ClearDirty();
    }
}

void ClientTerminalApp::PumpPty() {
    if (!pty_ || !pty_->IsValid()) return;

    std::string out;
    pty_->DrainOutput(out);
    if (!out.empty()) {
        parser_->Feed(reinterpret_cast<const uint8_t*>(out.data()), out.size());
        // M2b trigger feed: buffer the sanitized text for terminal.output.
        std::string cleaned = StripVtEscapes(out);
        if (!cleaned.empty()) pendingPub_ += cleaned;
    }
    const std::string replies = parser_->TakeReplies();
    if (!replies.empty()) {
        pty_->WriteInput(replies.data(), replies.size());
    }
    PublishTerminalOutput();
    if (pty_->ShellExited()) {
        RequestQuit();   // shell gone → close the surface, server drops the layer
    }
}

// Remove VT escape sequences (CSI, OSC, other ESC forms) and control chars,
// keeping the human-readable text for trigger regexes (e.g. /error C\d+/).
std::string ClientTerminalApp::StripVtEscapes(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(raw[i]);
        if (c != 0x1B) {
            if (c == '\r' || c == '\n' || c == '\t' || c >= 0x20) out += c;
            continue;
        }
        if (i + 1 >= raw.size()) break;
        const char next = raw[i + 1];
        if (next == '[') {  // CSI: ESC [ params final(0x40-0x7E)
            i += 2;
            while (i < raw.size() &&
                   static_cast<unsigned char>(raw[i]) < 0x40)
                ++i;
            // raw[i] is now the final byte (or end) — consumed below.
        } else if (next == ']') {  // OSC: terminated by BEL or ESC \
            i += 2;
            while (i < raw.size()) {
                if (raw[i] == 0x07) break;
                if (raw[i] == 0x1B && i + 1 < raw.size() &&
                    raw[i + 1] == '\\') {
                    ++i;
                    break;
                }
                ++i;
            }
        } else {
            ++i;  // other two-byte ESC forms
        }
    }
    return out;
}

std::string ClientTerminalApp::JsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

void ClientTerminalApp::PublishTerminalOutput() {
    if (pendingPub_.empty()) return;
    jk::client::JKClientSurface* surface = Surface();
    if (!surface || !surface->IsConnected()) {
        pendingPub_.clear();  // drop rather than queue against a dead pipe
        return;
    }
    const uint64_t now =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
    const bool intervalMet = now - lastPubTickMs_ >= kPubMinIntervalMs;
    const bool full = pendingPub_.size() >= kPubMaxBytes;
    if (!intervalMet && !full) return;

    const std::string json =
        "{\"tool\":\"publish_event\",\"args\":{\"topic\":\"terminal.output\","
        "\"data\":{\"text\":\"" + JsonEscape(pendingPub_) + "\"}}}";
    if (surface->SendAgentQuery(nextPubQueryId_++, json)) {
        lastPubTickMs_ = now;
    }
    pendingPub_.clear();

    // Fire-and-forget: keep the reply ring drained so it never fills.
    jk::client::AgentReply r;
    while (surface->PollAgentReply(r)) {
    }
}

void ClientTerminalApp::WriteToPty(const char* data, size_t len) {
    if (ptyStarted_ && pty_) {
        pty_->WriteInput(data, len);
    }
}

void ClientTerminalApp::OnViewResized(int cols, int rows) {
    if (!ptyStarted_) return;   // the lazy start below uses the view size
    if (cols == ptyCols_ && rows == ptyRows_) return;
    ptyCols_ = cols;
    ptyRows_ = rows;
    pty_->Resize(cols, rows);
}

} // namespace jk