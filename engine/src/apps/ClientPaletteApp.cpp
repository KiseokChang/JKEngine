// Command palette client (agent platform M2a, spec §6.2). See
// ClientPaletteApp.h. Structure mirrors ClientTaskmgrApp (docs/23 §11.2):
// 16 ms timer re-arms the frame gate, every event is fed to the ImGui
// backend before NewFrame, the root paints the dark clear color.
#include <apps/ClientPaletteApp.h>

#include <agent/JKAgentJson.h>
#include <imgui_impl_jkwindow.h>
#include "theme/JKThemeImGui.h"
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
        const auto& t = jk::theme::current();
        dc.SetColor(t.appClearBg.r, t.appClearBg.g, t.appClearBg.b, 255);
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
    jk::theme::ApplyImGuiTheme(); // JKTheme 팔레트 봉합 (P2 단계 3)
    ImGui::GetIO().IniFilename = nullptr;
    lastFrame_ = std::chrono::steady_clock::now();
    // Opt into desktop events on the window connection (M2a relaxed push —
    // the palette is a regular client that subscribed).
    if (jk::client::JKClientSurface* surface = Surface()) {
        surface->SendAgentEventSubscribe(true);
    }
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

    PumpReplies();

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
                Submit(text);
            }
            ImGui::SetKeyboardFocusHere();  // stay in the box after Enter
            focusInput_ = true;
        }
        ImGui::BeginChild("log", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 7));
        for (const auto& line : log_) ImGui::TextWrapped("%s", line.c_str());
        if (scrollDirty_) {
            ImGui::SetScrollHereY(1.0f);
            scrollDirty_ = false;
        }
        ImGui::EndChild();

        // Event feed — the minimal notification center (spec §7 MVP).
        ImGui::Separator();
        ImGui::Text("events");
        ImGui::BeginChild("feed", ImVec2(0, 0), ImGuiChildFlags_Borders);
        for (const auto& line : feed_) ImGui::TextWrapped("%s", line.c_str());
        ImGui::EndChild();
    }
    ImGui::End();
}

void ClientPaletteApp::AppendLog(const std::string& line) {
    log_.push_back(line);
    scrollDirty_ = true;
}

// 코어 펌프 이관 (docs/54 §4, opus 리뷰 M1): 코어가 유일 드레이너 — 훅으로
// 건별 수령(자체 DrainAgentEvents는 빈 큐만 본다).
void ClientPaletteApp::OnAgentEvent(const std::string& eventJson) {
    agent::AgentJson json(eventJson);
    std::string topic, title;
    int id = 0;
    json.GetStr("topic", topic);
    json.GetStr("title", title);
    json.GetInt("id", id);
    if (topic.empty()) return;
    feed_.push_back(topic + ": " +
                    (title.empty() ? "?" : title) +
                    " (#" + std::to_string(id) + ")");
    while (feed_.size() > 12) feed_.erase(feed_.begin());
}

std::string ClientPaletteApp::EscapeJson(const std::string& in) {
    std::string out;
    for (char c : in) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (static_cast<unsigned char>(c) < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", c);
            out += buf;
        } else {
            out += c;
        }
    }
    return out;
}

// One deterministic tool call ("one API, many faces": the same tools the
// MCP broker drives, sent over the palette's own window connection).
uint32_t ClientPaletteApp::SendTool(const std::string& tool,
                                    const std::string& argsJson) {
    jk::client::JKClientSurface* surface = Surface();
    if (!surface || !surface->IsConnected()) {
        AppendLog("! not connected to the desktop");
        return 0;
    }
    if (pendingQueryId_ != 0) {
        AppendLog("! busy (previous command still running)");
        return 0;
    }
    const std::string json =
        "{\"tool\":\"" + tool + "\",\"args\":" + argsJson + "}";
    const uint32_t id = nextQueryId_++;
    if (!surface->SendAgentQuery(id, json)) {
        AppendLog("! send failed");
        return 0;
    }
    pendingQueryId_ = id;
    return id;
}

void ClientPaletteApp::Submit(const std::string& text) {
    if (text.empty()) return;
    if (text[0] != '/') {
        AppendLog("  natural-language delegation arrives in a later stage");
        return;
    }
    const size_t sp = text.find(' ');
    const std::string cmd = text.substr(1, sp == std::string::npos
                                             ? std::string::npos : sp - 1);
    const std::string arg = (sp == std::string::npos)
                                ? std::string() : Trim(text.substr(sp + 1));
    if (cmd == "help") {
        AppendLog("  /list  /launch <app>  /close <id>  /chat  /notify");
        AppendLog("  /shot  /triggers  /trigger <name> on|off  /events  /trust");
        AppendLog("  /theme dark|light|classic  /save <name>  /restore <name>  /undo");
        AppendLog("  /agentmgr  /settings  /notes  /files");
    } else if (cmd == "agentmgr") {
        // 에이전트 관리자 앱 (specs/2026-09-16-agent-manager) — launch_app
        // 재사용. MVP는 토글 없음(재스폰 = 사용자 책임).
        SendTool("launch_app", "{\"app\":\"agentmgr\"}");
    } else if (cmd == "settings") {
        // 설정 허브 앱 (specs/2026-09-18-settings-hub) — launch_app 재사용.
        SendTool("launch_app", "{\"app\":\"settings\"}");
    } else if (cmd == "notes") {
        // 노트 허브 앱 (specs/2026-09-18-notes-hub) — launch_app 재사용.
        SendTool("launch_app", "{\"app\":\"notes\"}");
    } else if (cmd == "files") {
        // 파일 허브 앱 (specs/2026-09-18-file-hub) — launch_app 재사용.
        SendTool("launch_app", "{\"app\":\"files\"}");
    } else if (cmd == "events") {
        // Structured event catalog (docs/32): topic/source/payload shape +
        // live fired/last_ts/subscribers.
        SendTool("events_list", "{}");
    } else if (cmd == "notify") {
        // Open/toggle the notification center (docs/33).
        SendTool("open_notify", "{}");
    } else if (cmd == "shot") {
        // Open the screenshot viewer (docs/35); its 영역 캡처 button spawns
        // the rubber-band overlay.
        SendTool("launch_app", "{\"app\":\"shot\"}");
    } else if (cmd == "triggers") {
        SendTool("trigger_list", "{}");
    } else if (cmd == "trust") {
        // Script trust store (docs/37).
        SendTool("trust_list", "{}");
    } else if (cmd == "trigger") {
        // /trigger <name> on|off — toggle one trigger bundle (docs/34).
        const size_t sp2 = arg.find(' ');
        if (sp2 == std::string::npos || arg.empty()) {
            AppendLog("  usage: /trigger <name> on|off");
        } else {
            const std::string name = Trim(arg.substr(0, sp2));
            const std::string mode = Trim(arg.substr(sp2 + 1));
            if (mode != "on" && mode != "off") {
                AppendLog("  usage: /trigger <name> on|off");
            } else {
                SendTool("trigger_toggle",
                         "{\"name\":\"" + EscapeJson(name) + "\",\"on\":" +
                             (mode == "on" ? "1" : "0") + "}");
            }
        }
    } else if (cmd == "chat") {
        // Open the Win32 agent chat window (the inline approval surface).
        SendTool("launch_chat", "{}");
    } else if (cmd == "list") {
        SendTool("list_windows", "{}");
    } else if (cmd == "launch") {
        if (arg.empty()) {
            AppendLog("  usage: /launch <app>");
        } else {
            SendTool("launch_app", "{\"app\":\"" + EscapeJson(arg) + "\"}");
        }
    } else if (cmd == "theme") {
        // P3 hot-swap (docs/52): /theme dark|light|classic — the server swaps
        // in-process and writes theme.json; every client follows via the
        // 500ms mtime poll.
        if (arg != "dark" && arg != "light" && arg != "classic") {
            AppendLog("  usage: /theme dark|light|classic");
        } else {
            SendTool("theme_set", "{\"preset\":\"" + arg + "\"}");
        }
    } else if (cmd == "save") {
        if (arg.empty()) {
            AppendLog("  usage: /save <name>");
        } else {
            SendTool("save_layout", "{\"name\":\"" + EscapeJson(arg) + "\"}");
        }
    } else if (cmd == "restore") {
        if (arg.empty()) {
            AppendLog("  usage: /restore <name>");
        } else if (const uint32_t save = SendTool(
                       "save_layout", "{\"name\":\"pre_undo\"}")) {
            undoSaveQueryId_ = save;
            afterUndoSave_ = "restore_layout\x1f" + arg;
        }
    } else if (cmd == "close") {
        const int id = arg.empty() ? 0 : std::atoi(arg.c_str());
        if (id <= 0) {
            AppendLog("  usage: /close <id>  (see /list)");
        } else if (const uint32_t save = SendTool(
                       "save_layout", "{\"name\":\"pre_undo\"}")) {
            undoSaveQueryId_ = save;
            afterUndoSave_ = "close_window\x1f" + arg;
        }
    } else if (cmd == "undo") {
        if (!undoSaved_) {
            AppendLog("  nothing to undo");
        } else {
            SendTool("restore_layout", "{\"name\":\"pre_undo\"}");
        }
    } else {
        AppendLog("  unknown command - /help");
    }
}

void ClientPaletteApp::PumpReplies() {
    jk::client::JKClientSurface* surface = Surface();
    jk::client::AgentReply reply;
    while (surface && surface->PollAgentReply(reply)) {
        if (reply.queryId == pendingQueryId_) pendingQueryId_ = 0;
        if (reply.queryId == undoSaveQueryId_ && !afterUndoSave_.empty()) {
            // The pre_undo save landed — now run the destructive command it
            // was guarding (sequential because the palette is one query
            // deep). If the save failed, the chain stops: never run a
            // destructive command without its undo snapshot.
            if (reply.ok) {
                undoSaved_ = true;
                const size_t sep = afterUndoSave_.find('\x1f');
                const std::string tool = afterUndoSave_.substr(0, sep);
                const std::string arg = afterUndoSave_.substr(sep + 1);
                if (tool == "close_window") {
                    SendTool(tool, "{\"id\":" + arg + "}");
                } else {
                    SendTool(tool, "{\"name\":\"" + EscapeJson(arg) + "\"}");
                }
            } else {
                AppendLog("  ! snapshot failed - destructive command skipped");
            }
            afterUndoSave_.clear();
            undoSaveQueryId_ = 0;
            continue;
        }
        AppendLog("  " + reply.json);
    }
}


// P3 theme hot-swap (docs/52): the palette was snapshotted into ImGuiStyle
// at OnInit - re-apply it after a preset swap.
void ClientPaletteApp::OnThemeChanged() { jk::theme::ApplyImGuiTheme(); }

} // namespace jk