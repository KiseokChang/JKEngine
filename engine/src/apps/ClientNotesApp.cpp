// Notes hub client (specs/2026-09-18-notes-hub). ClientAgentMgrApp pattern:
// request/response polling, no event subscription (agent notes reach the
// user through the notification center — the server broadcasts agent.notify).
// Two tabs: 코멘트 (margin comments, newest-first, optional window link) and
// 백로그 (대기/진행/완료 board).
#include <apps/ClientNotesApp.h>

#include <agent/JKAgentJson.h>
#include <imgui_impl_jkwindow.h>
#include "theme/JKThemeImGui.h"
#include <JKWindow.h>
#include <SDL.h>

#include <cstdio>
#include <ctime>

namespace jk {
namespace {
// Root window paints the dark clear color (palette/notify/agentmgr idiom).
class SetRoot : public JKWindow {
public:
    explicit SetRoot(const std::string& title) : JKWindow(title) {}
    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetClientRect();
        const auto& t = jk::theme::current();
        dc.SetColor(t.appClearBg.r, t.appClearBg.g, t.appClearBg.b, 255);
        dc.FillRect(client);
    }
};

std::string EscapeJson(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (const char ch : in) {
        if (ch == '"' || ch == '\\') { out += '\\'; out += ch; }
        else if (static_cast<unsigned char>(ch) < 0x20) {
            char num[8];
            std::snprintf(num, sizeof(num), "\\u%04x", ch);
            out += num;
        } else {
            out += ch;
        }
    }
    return out;
}

// ts(epoch 초) → "mm-dd hh:mm". notes_read 응답은 초 단위(서버 절단).
std::string FmtStamp(long long ts) {
    if (ts <= 0) return "";
    const std::time_t t = static_cast<std::time_t>(ts);
    std::tm lt{};
    if (!localtime_s(&lt, &t)) return "";
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%02d-%02d %02d:%02d",
                  lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min);
    return buf;
}
} // namespace

ClientNotesApp::~ClientNotesApp() = default;

void ClientNotesApp::OnInit() {
    auto main = std::make_unique<SetRoot>("Notes");
    main->SetWindowRect(JKRect{ 0, 0, 760, 520 });
    main->SetAttrFlags(WA_CHROMELESS);
    SetMainWindow(std::move(main));

    SetTimerInterval(16);   // ~60 Hz frame cadence (agentmgr 선례)

    ImGui::CreateContext();
    jk::theme::ApplyImGuiTheme();
    ImGui::GetIO().IniFilename = nullptr;
    ImGuiIO& io = ImGui::GetIO();
    if (io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f,
                                     nullptr,
                                     io.Fonts->GetGlyphRangesKorean())) {
        koreanFont_ = true;
    }
    SendQuery("notes_read", "{}", Query::Read);
    SendQuery("list_windows", "{}", Query::Windows);
}

void ClientNotesApp::OnClose() {
    if (imguiReady_) {
        ImGui_ImplJKWindow_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
}

void ClientNotesApp::OnThemeChanged() {
    if (imguiReady_) jk::theme::ApplyImGuiTheme();   // docs/52 hot-swap
}

bool ClientNotesApp::PreProcessMessage(const JKEvent& ev) {
    ImGui_ImplJKWindow_ProcessJKEvent(ev);
    if (ev.type == JKEventType::Timer) frameDirty_ = true;
    return true;
}

void ClientNotesApp::OnFrameCommitted() { frameDirty_ = false; }

void ClientNotesApp::SendQuery(const char* tool, const std::string& args,
                               Query kind) {
    jk::client::JKClientSurface* surface = Surface();
    if (!surface || !surface->IsConnected()) {
        status_ = koreanFont_ ? "[!] 서버에 연결되어 있지 않습니다"
                              : "[!] not connected";
        return;
    }
    const std::string json =
        "{\"tool\":\"" + std::string(tool) + "\",\"args\":" + args + "}";
    const uint32_t id = nextQueryId_++;
    if (!surface->SendAgentQuery(id, json)) {
        status_ = "[!] send failed";
        return;
    }
    pending_.emplace_back(id, PendingQuery{ kind });
}

void ClientNotesApp::PollReplies() {
    jk::client::JKClientSurface* surface = Surface();
    if (!surface) return;
    jk::client::AgentReply reply;
    while (surface->PollAgentReply(reply)) {
        Query kind = Query::Read;
        bool found = false;
        for (auto it = pending_.begin(); it != pending_.end(); ++it) {
            if (it->first == reply.queryId) {
                kind = it->second.kind;
                pending_.erase(it);
                found = true;
                break;
            }
        }
        if (found) ApplyReply(kind, reply.json);
    }
}

void ClientNotesApp::ApplyReply(Query kind, const std::string& json) {
    jk::agent::AgentJson r(json);
    if (kind == Query::Write) {
        if (r.ok()) {
            status_ = koreanFont_ ? "완료" : "done";
            SendQuery("notes_read", "{}", Query::Read);   // 자기 쓰기 반영
        } else {
            status_ = "[!] " + json;   // bad_op/bad_text/bad_state/...
        }
        return;
    }
    if (kind == Query::Windows) {
        windows_.clear();
        if (!r.ok()) { status_ = "[!] " + json; return; }
        int cnt = 0;
        if (r.GetArraySize("windows", cnt)) {
            for (int i = 0; i < cnt; ++i) {
                int idInt = 0;
                std::string title;
                r.GetArrInt("windows", i, "id", idInt);
                r.GetArrStr("windows", i, "title", title);
                windows_.emplace_back(static_cast<uint32_t>(idInt), title);
            }
        }
        if (winSel_ > static_cast<int>(windows_.size())) winSel_ = 0;
        return;
    }
    // Query::Read — notes_read 봉투 {ok,notes:[...],backlog:[...]}.
    notes_.clear();
    items_.clear();
    if (!r.ok()) { status_ = "[!] " + json; return; }
    int cnt = 0;
    if (r.GetArraySize("notes", cnt)) {
        for (int i = 0; i < cnt; ++i) {
            NoteRowUi row;
            int idInt = 0, winInt = 0, tsInt = 0;
            r.GetArrInt("notes", i, "id", idInt);
            r.GetArrStr("notes", i, "text", row.text);
            r.GetArrInt("notes", i, "win", winInt);
            r.GetArrInt("notes", i, "ts", tsInt);
            r.GetArrStr("notes", i, "src", row.src);
            row.id = idInt;
            row.win = winInt;
            row.ts = tsInt;
            notes_.push_back(row);
        }
    }
    if (r.GetArraySize("backlog", cnt)) {
        for (int i = 0; i < cnt; ++i) {
            NoteItemUi row;
            int idInt = 0, stInt = 0, tsInt = 0;
            r.GetArrInt("backlog", i, "id", idInt);
            r.GetArrStr("backlog", i, "title", row.title);
            r.GetArrInt("backlog", i, "state", stInt);
            r.GetArrInt("backlog", i, "ts", tsInt);
            r.GetArrStr("backlog", i, "src", row.src);
            row.id = idInt;
            row.state = stInt;
            row.ts = tsInt;
            items_.push_back(row);
        }
    }
}

void ClientNotesApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
    if (!imguiReady_) {
        if (!ImGui_ImplJKWindow_Init(renderer)) return;
        imguiReady_ = true;
    }
    PollReplies();

    ImGui_ImplJKWindow_NewFrame(1.0f / 60.0f, w, h);
    ImGui::NewFrame();
    BuildUi(w, h);
    ImGui::Render();
    ImGui_ImplJKWindow_RenderDrawData(ImGui::GetDrawData(), renderer);
}

void ClientNotesApp::BuildUi(int w, int h) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
    // Begin-false여도 End()는 반드시 호출(imgui.h 계약 — docs/53 §9 잔여).
    const bool open = ImGui::Begin("notes", nullptr,
                                   ImGuiWindowFlags_NoDecoration |
                                       ImGuiWindowFlags_NoMove);
    ImGui::End();
    if (!open) return;

    // 서버 크롬이 상단 24pt를 먹는다(레슨 8) — 탭바는 y>=30부터.
    ImGui::SetNextWindowPos(ImVec2(0, 30));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h - 30));
    // Begin-false여도 End()는 반드시 호출(imgui.h 계약 — docs/53 §9 잔여).
    ImGui::Begin("##notetabs", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    if (ImGui::BeginTabBar("notetabs")) {
        if (ImGui::BeginTabItem(koreanFont_ ? "코멘트" : "Comments")) {
            tab_ = 0;
            BuildCommentsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(koreanFont_ ? "백로그" : "Backlog")) {
            tab_ = 1;
            BuildBacklogTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

    // 하단 상태줄.
    if (!status_.empty()) {
        ImGui::SetNextWindowPos(ImVec2(0, (float)h - 24));
        ImGui::SetNextWindowSize(ImVec2((float)w, 24));
        if (ImGui::Begin("##notestatus", nullptr,
                         ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove)) {
            ImGui::TextUnformatted(status_.c_str());
        }
        ImGui::End();
    }
}

void ClientNotesApp::BuildCommentsTab() {
    // 목록(최신순 — 파일 순서의 역순 표시) + 행별 삭제.
    if (ImGui::BeginChild("notelist", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2),
                          ImGuiChildFlags_Borders)) {
        for (size_t i = notes_.size(); i > 0; --i) {
            const NoteRowUi& r = notes_[i - 1];
            ImGui::PushID(static_cast<int>(r.id));
            const std::string srcBadge =
                r.src == "agent" ? (koreanFont_ ? "[에이전트]" : "[agent]")
                                 : (koreanFont_ ? "[사용자]" : "[user]");
            std::string link;
            if (r.win > 0) {
                for (const auto& wd : windows_) {
                    if (wd.first == static_cast<uint32_t>(r.win)) {
                        link = " @" + wd.second;
                        break;
                    }
                }
                if (link.empty()) {
                    // 창 소멸 — 링크는 남긴다(스펙 §2.3: 역참조는 읽기 시점).
                    link = koreanFont_ ? " @닫힌창#" : " @closed#";
                    link += std::to_string(r.win);
                }
            }
            const std::string header = srcBadge + link + " · " +
                                       FmtStamp(r.ts);
            ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.95f, 1.0f), "%s",
                               header.c_str());
            ImGui::TextWrapped("%s", r.text.c_str());
            ImGui::SameLine();
            const float right = ImGui::GetWindowWidth() - 30.0f;
            if (ImGui::GetCursorPosX() < right) ImGui::SetCursorPosX(right);
            if (ImGui::SmallButton(koreanFont_ ? "삭제" : "del")) {
                const std::string args =
                    "{\"op\":\"del\",\"id\":" + std::to_string(r.id) + "}";
                SendQuery("notes_write", args, Query::Write);
            }
            ImGui::Separator();
            ImGui::PopID();
        }
        if (notes_.empty()) {
            ImGui::TextDisabled("%s", koreanFont_
                ? "(비어 있음 — 아래에 코멘트를 남겨보세요)"
                : "(empty — add a comment below)");
        }
    }
    ImGui::EndChild();

    // 하단: 창 피커 + 입력 + 추가. Enter로 확정.
    const char* preview = winSel_ == 0
        ? (koreanFont_ ? "범용" : "general")
        : (winSel_ <= static_cast<int>(windows_.size())
               ? windows_[winSel_ - 1].second.c_str() : "??");
    if (ImGui::BeginCombo("##winpicker", preview)) {
        if (ImGui::Selectable(koreanFont_ ? "범용" : "general",
                              winSel_ == 0)) {
            winSel_ = 0;
        }
        for (size_t i = 0; i < windows_.size(); ++i) {
            const bool sel = winSel_ == static_cast<int>(i) + 1;
            if (ImGui::Selectable(windows_[i].second.c_str(), sel)) {
                winSel_ = static_cast<int>(i) + 1;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    const bool commentEnter = ImGui::InputText("##comment", commentBuf_,
                                               sizeof(commentBuf_)) &&
                               ImGui::IsItemFocused() &&
                               ImGui::IsKeyPressed(ImGuiKey_Enter);
    ImGui::SameLine();
    const bool addClicked = ImGui::Button(koreanFont_ ? "추가" : "add");
    if ((commentEnter || addClicked) && commentBuf_[0] != '\0') {
        const std::string text = commentBuf_;
        const int win = winSel_ > 0 && winSel_ <= static_cast<int>(windows_.size())
            ? static_cast<int>(windows_[winSel_ - 1].first) : 0;
        const std::string args =
            "{\"op\":\"add_note\",\"text\":\"" + EscapeJson(text) +
            "\",\"win\":" + std::to_string(win) + "}";
        SendQuery("notes_write", args, Query::Write);
        commentBuf_[0] = '\0';
        winSel_ = 0;
    }
}

void ClientNotesApp::BuildBacklogTab() {
    // 3열 보드(대기0/진행1/완료2) — 카드 + 좌우 이동 + 삭제.
    const char* cols[3] = {
        koreanFont_ ? "대기" : "todo",
        koreanFont_ ? "진행" : "doing",
        koreanFont_ ? "완료" : "done" };
    const float colW = ImGui::GetWindowContentRegionMax().x / 3.0f;
    for (int c = 0; c < 3; ++c) {
        if (c) ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted(cols[c]);
        ImGui::BeginChild(std::string("bcol" + std::to_string(c)).c_str(),
                          ImVec2(colW - 8, -ImGui::GetFrameHeightWithSpacing() * 2),
                          ImGuiChildFlags_Borders);
        for (const NoteItemUi& it : items_) {
            if (it.state != c) continue;
            ImGui::PushID(static_cast<int>(it.id));
            const std::string srcBadge =
                it.src == "agent" ? (koreanFont_ ? "[에이전트]" : "[agent]")
                                  : (koreanFont_ ? "[사용자]" : "[user]");
            ImGui::TextWrapped("%s", it.title.c_str());
            ImGui::TextDisabled("%s", srcBadge.c_str());
            if (c > 0) {
                if (ImGui::SmallButton("<")) {
                    const std::string args =
                        "{\"op\":\"move_item\",\"id\":" +
                        std::to_string(it.id) +
                        ",\"state\":" + std::to_string(c - 1) + "}";
                    SendQuery("notes_write", args, Query::Write);
                }
                ImGui::SameLine();
            }
            if (c < 2) {
                if (ImGui::SmallButton(">")) {
                    const std::string args =
                        "{\"op\":\"move_item\",\"id\":" +
                        std::to_string(it.id) +
                        ",\"state\":" + std::to_string(c + 1) + "}";
                    SendQuery("notes_write", args, Query::Write);
                }
                ImGui::SameLine();
            }
            if (ImGui::SmallButton(koreanFont_ ? "삭제" : "del")) {
                const std::string args =
                    "{\"op\":\"del\",\"id\":" + std::to_string(it.id) + "}";
                SendQuery("notes_write", args, Query::Write);
            }
            ImGui::PopID();
            ImGui::Separator();
        }
        ImGui::EndChild();
        ImGui::EndGroup();
    }
    // 하단 추가 입력 — 대기(0)로 들어간다.
    const bool backlogEnter = ImGui::InputText("##backlog", backlogBuf_,
                                               sizeof(backlogBuf_)) &&
                               ImGui::IsItemFocused() &&
                               ImGui::IsKeyPressed(ImGuiKey_Enter);
    ImGui::SameLine();
    const bool addClicked = ImGui::Button(koreanFont_ ? "추가" : "add");
    if ((backlogEnter || addClicked) && backlogBuf_[0] != '\0') {
        const std::string args =
            "{\"op\":\"add_item\",\"text\":\"" +
            EscapeJson(std::string(backlogBuf_)) + "\"}";
        SendQuery("notes_write", args, Query::Write);
        backlogBuf_[0] = '\0';
    }
    // 수동 새로고침(스펙 §3 — 폴링 없음).
    ImGui::SameLine();
    if (ImGui::Button(koreanFont_ ? "새로고침" : "refresh")) {
        SendQuery("notes_read", "{}", Query::Read);
    }
}

} // namespace jk