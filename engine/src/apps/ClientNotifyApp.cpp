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
        entry.ts = static_cast<long long>(NowMs());
        entry.read = false;
        history_.push_back(std::move(entry));
        ++unread_;
        while (history_.size() > 200) history_.erase(history_.begin());
        SaveHistory();
    }
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