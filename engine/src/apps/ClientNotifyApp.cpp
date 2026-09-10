// Notification center client (agent platform, docs/33). Structure mirrors
// ClientPaletteApp (docs/23 §11.2 taskmgr clock): 16 ms timer re-arms the
// frame gate, every event feeds the ImGui backend before NewFrame, the root
// paints the dark clear color.
#include <apps/ClientNotifyApp.h>

#include <agent/JKAgentJson.h>
#include <imgui_impl_jkwindow.h>
#include <JKWindow.h>
#include <SDL.h>

#include <chrono>
#include <cstdio>
#include <windows.h>

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

    LoadConfig();
    LoadHistory();
    UpdateBadge();   // restored unread count survives restart (docs/33)

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

namespace {
// "[HH:MM] 제목 — 본문"; non-agent.notify entries carry their topic prefix so
// mixed subscriptions stay readable. localtime from the stored epoch ms.
std::string FormatEntry(const NotifyEntry& e) {
    time_t t = static_cast<time_t>(e.ts / 1000);
    struct tm lt;
    localtime_s(&lt, &t);
    char hhmm[8];
    std::snprintf(hhmm, sizeof(hhmm), "%02d:%02d", lt.tm_hour, lt.tm_min);
    std::string line = std::string("[") + hhmm + "] ";
    if (e.topic != "agent.notify") line += e.topic + " · ";
    line += e.title.empty() ? std::string("(무제)") : e.title;
    if (!e.body.empty()) line += " — " + e.body;
    return line;
}
} // namespace

void ClientNotifyApp::BuildUi(int w, int h) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
    if (ImGui::Begin("notify", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
        // Header: unread count + actions
        ImGui::Text(koreanFont_ ? "안읽음 %d" : "Unread %d", unread_);
        ImGui::SameLine(ImGui::GetWindowWidth() - 160);
        if (ImGui::Button(koreanFont_ ? "모두 읽음" : "Mark read") && unread_ > 0)
            MarkAllRead();
        ImGui::SameLine();
        if (ImGui::Button(koreanFont_ ? "지우기" : "Clear") && !history_.empty())
            ClearHistory();
        ImGui::Separator();
        // History, newest first; unread entries highlighted amber
        if (ImGui::BeginChild("history", ImVec2(0, -32),
                              ImGuiChildFlags_Borders)) {
            for (int i = static_cast<int>(history_.size()) - 1; i >= 0; --i) {
                const NotifyEntry& e = history_[i];
                if (!e.read)
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          ImVec4(1.0f, 0.85f, 0.5f, 1.0f));
                ImGui::TextWrapped("%s", FormatEntry(e).c_str());
                if (!e.read) ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();
        // Toast strip (MVP, docs/33 §제한): newest entry, 5 s on, 2 s fade
        const uint64_t now = NowMs();
        if (now < toastUntilMs_) {
            const uint64_t remain = toastUntilMs_ - now;
            const float alpha =
                remain >= 2000 ? 1.0f : static_cast<float>(remain) / 2000.0f;
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
            ImGui::Separator();
            ImGui::TextWrapped("%s", FormatEntry(toastEntry_).c_str());
            ImGui::PopStyleVar();
        }
    }
    ImGui::End();
}

void ClientNotifyApp::DrainEvents() {
    jk::client::JKClientSurface* surface = Surface();
    std::vector<std::string> events;
    if (!surface || surface->DrainAgentEvents(events) == 0) return;
    bool changed = false;
    for (const std::string& e : events) {
        agent::AgentJson json(e);
        std::string topic;
        json.GetStr("topic", topic);
        if (topic.empty()) continue;
        // Subscription filter (docs/33): only the configured topics enter
        // the history. Default install is agent.notify only.
        bool wanted = false;
        for (const std::string& t : topics_) wanted |= (t == topic);
        if (!wanted) continue;

        NotifyEntry entry;
        entry.topic = topic;
        // Envelope shapes differ by publisher (docs/32 §1): publish_event
        // puts fields under data.*; server-internal pushes (app.crashed,
        // window.*) put title/pid at the top level.
        json.GetObjStr("data", "title", entry.title);
        if (entry.title.empty()) json.GetStr("title", entry.title);
        json.GetObjStr("data", "body", entry.body);
        entry.ts = static_cast<long long>(NowMs());
        entry.read = false;
        history_.push_back(entry);
        ++unread_;
        while (history_.size() > 200) history_.erase(history_.begin());
        SaveHistory();

        toastEntry_ = entry;
        toastUntilMs_ = NowMs() + 5000;
        changed = true;
    }
    if (changed) UpdateBadge();
    frameDirty_ = true;
}

void ClientNotifyApp::UpdateBadge() {
    JKWindow* main = GetMainWindow();
    if (!main) return;
    const std::string title = unread_ > 0
        ? "Notifications (" + std::to_string(unread_) + ")"
        : std::string("Notifications");
    if (title == badge_) return;   // no-op guard — SetTitle spams the pipe
    badge_ = title;
    main->SetTitle(title);
    if (jk::client::JKClientSurface* surface = Surface()) {
        surface->SendWindowTitle(title);
    }
}

void ClientNotifyApp::MarkAllRead() {
    for (NotifyEntry& e : history_) e.read = true;
    unread_ = 0;
    SaveHistory();
    UpdateBadge();
    frameDirty_ = true;
}

void ClientNotifyApp::ClearHistory() {
    history_.clear();
    unread_ = 0;
    toastUntilMs_ = 0;
    SaveHistory();
    UpdateBadge();
    frameDirty_ = true;
}

// ---------------------------------------------------------------------------
// Config + persistence (docs/33)
// ---------------------------------------------------------------------------

std::string ClientNotifyApp::StatePath(const char* name) {
    std::string path;
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
        path = exePath;
        const size_t slash = path.find_last_of("\\/");
        if (slash != std::string::npos) path.resize(slash + 1);
    }
    path += "state";
    CreateDirectoryA(path.c_str(), nullptr);   // harmless if it exists
    path += "\\";
    path += name;
    return path;
}

void ClientNotifyApp::LoadConfig() {
    // {"topics":[{"topic":"agent.notify"}, ...]} — array of objects so the
    // existing AgentJson array accessors work unchanged.
    topics_.clear();
    FILE* f = std::fopen(StatePath("notify.json").c_str(), "rb");
    if (!f) {
        topics_.push_back("agent.notify");   // default subscription
        return;
    }
    std::string buf;
    char chunk[4096];
    size_t got;
    while ((got = std::fread(chunk, 1, sizeof(chunk), f)) > 0)
        buf.append(chunk, got);
    std::fclose(f);
    // JS_ParseJSON requires a NUL-terminated buffer — std::string::c_str()
    // guarantees it (docs/27 lesson 3).
    agent::AgentJson json(buf);
    int count = 0;
    if (json.ok() && json.GetArraySize("topics", count) && count > 0) {
        for (int i = 0; i < count && i < 16; ++i) {
            std::string topic;
            if (json.GetArrStr("topics", i, "topic", topic) && !topic.empty())
                topics_.push_back(topic);
        }
    }
    if (topics_.empty()) topics_.push_back("agent.notify");
}

void ClientNotifyApp::LoadHistory() {
    history_.clear();
    unread_ = 0;
    FILE* f = std::fopen(StatePath("notify_history.json").c_str(), "rb");
    if (!f) return;
    std::string buf;
    char chunk[4096];
    size_t got;
    while ((got = std::fread(chunk, 1, sizeof(chunk), f)) > 0)
        buf.append(chunk, got);
    std::fclose(f);
    agent::AgentJson json(buf);
    int count = 0;
    if (!json.ok() || !json.GetArraySize("entries", count)) return;
    for (int i = 0; i < count; ++i) {
        NotifyEntry entry;
        int read = 0;
        if (!json.GetArrStr("entries", i, "topic", entry.topic)) continue;
        json.GetArrStr("entries", i, "title", entry.title);
        json.GetArrStr("entries", i, "body", entry.body);
        int ts = 0;
        json.GetArrInt("entries", i, "ts", ts);
        entry.ts = static_cast<long long>(ts) * 1000;   // stored epoch sec
        json.GetArrInt("entries", i, "read", read);
        entry.read = (read != 0);
        if (!entry.read) ++unread_;
        history_.push_back(std::move(entry));
    }
}

void ClientNotifyApp::SaveHistory() {
    std::string out = "{\"entries\":[";
    for (size_t i = 0; i < history_.size(); ++i) {
        const NotifyEntry& e = history_[i];
        if (i) out += ",";
        out += "{\"topic\":\"" + EscapeJson(e.topic) + "\"";
        out += ",\"title\":\"" + EscapeJson(e.title) + "\"";
        out += ",\"body\":\"" + EscapeJson(e.body) + "\"";
        // ts is epoch seconds (fits int for the next ~60 years); AgentJson
        // array accessors are int-based.
        out += ",\"ts\":" + std::to_string(e.ts / 1000);
        out += ",\"read\":" + std::string(e.read ? "1" : "0");
        out += "}";
    }
    out += "]}";
    FILE* f = std::fopen(StatePath("notify_history.json").c_str(), "wb");
    if (!f) return;
    std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
}

std::string ClientNotifyApp::EscapeJson(const std::string& in) {
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

uint64_t ClientNotifyApp::NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

} // namespace jk