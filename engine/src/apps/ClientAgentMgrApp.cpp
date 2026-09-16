// Agent manager client (specs/2026-09-16-agent-manager). ClientNotifyApp
// template (docs/33): 16ms timer frame gate, dark root, malgun font, theme
// re-apply hook. No event subscription — request/response only (palette
// idiom: PollAgentReply drains the window connection's reply queue).
#include <apps/ClientAgentMgrApp.h>

#include <agent/JKAgentJson.h>
#include <imgui_impl_jkwindow.h>
#include "theme/JKThemeImGui.h"
#include <JKWindow.h>
#include <SDL.h>

#include <cstdio>

namespace jk {
namespace {
// Root window paints the dark clear color (palette/notify idiom).
class MgrRoot : public JKWindow {
public:
    explicit MgrRoot(const std::string& title) : JKWindow(title) {}
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
} // namespace

ClientAgentMgrApp::~ClientAgentMgrApp() = default;

void ClientAgentMgrApp::OnInit() {
    auto main = std::make_unique<MgrRoot>("Agent Manager");
    main->SetWindowRect(JKRect{ 0, 0, 560, 640 });
    main->SetAttrFlags(WA_CHROMELESS);
    SetMainWindow(std::move(main));

    SetTimerInterval(16);   // ~60 Hz frame cadence (taskmgr clock)

    ImGui::CreateContext();
    jk::theme::ApplyImGuiTheme();
    ImGui::GetIO().IniFilename = nullptr;
    ImGuiIO& io = ImGui::GetIO();
    if (io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f,
                                     nullptr,
                                     io.Fonts->GetGlyphRangesKorean())) {
        koreanFont_ = true;
    }
    Refresh(0);   // 첫 탭 데이터
}

void ClientAgentMgrApp::OnClose() {
    if (imguiReady_) {
        ImGui_ImplJKWindow_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
}

void ClientAgentMgrApp::OnThemeChanged() {
    if (imguiReady_) jk::theme::ApplyImGuiTheme();   // docs/52 hot-swap
}

bool ClientAgentMgrApp::PreProcessMessage(const JKEvent& ev) {
    ImGui_ImplJKWindow_ProcessJKEvent(ev);
    if (ev.type == JKEventType::Timer) frameDirty_ = true;
    return true;
}

void ClientAgentMgrApp::OnFrameCommitted() { frameDirty_ = false; }

void ClientAgentMgrApp::SendQuery(const char* tool, const std::string& args,
                                  Query kind, const std::string& arg) {
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
    pending_.emplace_back(id, PendingQuery{ kind, arg });
}

void ClientAgentMgrApp::Refresh(int tab) {
    switch (tab) {
        case 0: SendQuery("agent_permissions", "{}", Query::Perms); break;
        case 1: SendQuery("trigger_list", "{}", Query::Triggers); break;
        case 2: SendQuery("trust_list", "{}", Query::Trust); break;
        case 3:
            SendQuery("installed_list", "{}", Query::InstalledApps);
            SendQuery("list_windows", "{}", Query::RunningWindows);
            SendQuery("read_receipts", "{\"limit\":50}", Query::Receipts);
            break;
        default: break;
    }
}

void ClientAgentMgrApp::MergeRunning() {
    // 완화 매칭 (스펙 델타): 타이틀에 앱 이름이 포함되면 실행 중으로 표기.
    for (MgrInstalledRow& row : installed_) {
        row.running = false;
        for (const auto& w : windows_) {
            if (!w.second.empty() &&
                w.second.find(row.name) != std::string::npos) {
                row.running = true;
                break;
            }
        }
    }
}

void ClientAgentMgrApp::PollReplies() {
    jk::client::JKClientSurface* surface = Surface();
    if (!surface) return;
    jk::client::AgentReply reply;
    while (surface->PollAgentReply(reply)) {
        Query kind = Query::Perms;
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

void ClientAgentMgrApp::ApplyReply(Query kind, const std::string& json) {
    jk::agent::AgentJson r(json);
    switch (kind) {
        case Query::Perms: {
            perms_.clear();
            if (!r.ok()) { status_ = "[!] " + json; break; }
            int cnt = 0;
            if (r.GetArraySize("perms", cnt)) {
                for (int i = 0; i < cnt; ++i) {
                    MgrPermRow row;
                    r.GetArrStr("perms", i, "tool", row.tool);
                    r.GetArrStr("perms", i, "gate", row.gate);
                    r.GetArrStr("perms", i, "file", row.file);
                    r.GetArrStr("perms", i, "effective", row.effective);
                    r.GetArrStr("perms", i, "default", row.deflt);
                    perms_.push_back(row);
                }
            }
            break;
        }
        case Query::Triggers: {
            // topics 배열은 2레벨 리더의 관심사 밖 — name/enabled만 (파서 계약).
            triggers_.clear();
            if (!r.ok()) { status_ = "[!] " + json; break; }
            int cnt = 0;
            if (r.GetArraySize("triggers", cnt)) {
                for (int i = 0; i < cnt; ++i) {
                    MgrTriggerRow row;
                    r.GetArrStr("triggers", i, "name", row.name);
                    r.GetArrInt("triggers", i, "enabled", row.enabled);
                    triggers_.push_back(row);
                }
            }
            break;
        }
        case Query::Trust: {
            trust_.clear();
            if (!r.ok()) {
                // trust_store_unreadable은 상태줄이 아니라 탭에 에러 박스
                status_ = "";
                trust_.clear();
                trustStoreError_ = json;
                break;
            }
            trustStoreError_.clear();
            int cnt = 0;
            if (r.GetArraySize("records", cnt)) {
                for (int i = 0; i < cnt; ++i) {
                    MgrTrustRow row;
                    r.GetArrStr("records", i, "name", row.name);
                    r.GetArrStr("records", i, "source", row.source);
                    r.GetArrStr("records", i, "fingerprint", row.shortFp);
                    r.GetArrStr("records", i, "fp", row.fp);
                    trust_.push_back(row);
                }
            }
            break;
        }
        case Query::InstalledApps: {
            installed_.clear();
            if (!r.ok()) { status_ = "[!] " + json; break; }
            int cnt = 0;
            if (r.GetArraySize("installed", cnt)) {
                for (int i = 0; i < cnt; ++i) {
                    MgrInstalledRow row;
                    r.GetArrStr("installed", i, "name", row.name);
                    r.GetArrStr("installed", i, "kind", row.kind);
                    installed_.push_back(row);
                }
            }
            MergeRunning();
            break;
        }
        case Query::RunningWindows: {
            windows_.clear();
            if (!r.ok()) { status_ = "[!] " + json; break; }
            int cnt = 0;
            if (r.GetArraySize("windows", cnt)) {
                for (int i = 0; i < cnt; ++i) {
                    int idInt = 0;   // GetArrInt는 int — uint32로 안전 승격
                    std::string title;
                    r.GetArrInt("windows", i, "id", idInt);
                    r.GetArrStr("windows", i, "title", title);
                    windows_.emplace_back(static_cast<uint32_t>(idInt),
                                          title);
                }
            }
            MergeRunning();
            break;
        }
        case Query::Receipts: {
            receipts_.clear();
            if (!r.ok()) { status_ = "[!] " + json; break; }
            int cnt = 0;
            if (r.GetArraySize("rows", cnt)) {
                for (int i = 0; i < cnt; ++i) {
                    MgrReceiptRow row;
                    row.ts = 0;
                    int ts32 = 0;
                    if (r.GetArrInt("rows", i, "ts", ts32)) {
                        row.ts = ts32;   // 표시용 초 단위 절단 허용
                    }
                    r.GetArrStr("rows", i, "tool", row.tool);
                    std::string okRaw;
                    row.ok = r.GetArrStr("rows", i, "ok", okRaw) &&
                             okRaw == "1";
                    receipts_.push_back(row);
                }
            }
            break;
        }
        case Query::Toggle:
            if (r.ok()) Refresh(1);
            else status_ = "[!] " + json;
            break;
        case Query::PermissionSet:
        case Query::TrustRevoke:
            pendingPerm_.clear();
            if (r.ok()) {
                status_ = json.find("restart_needed") != std::string::npos
                    ? (koreanFont_ ? "완료 — jktriggers 재시작 후 적용"
                                   : "done (jktriggers restart needed)")
                    : (koreanFont_ ? "완료" : "done");
                Refresh(tab_);
            } else {
                status_ = "[!] " + json;
            }
            break;
    }
}

void ClientAgentMgrApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
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

void ClientAgentMgrApp::BuildUi(int w, int h) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
    // Begin-false여도 End()는 반드시 호출(imgui.h 계약 — docs/53 §9 잔여).
    const bool open = ImGui::Begin("agentmgr", nullptr,
                                   ImGuiWindowFlags_NoDecoration |
                                       ImGuiWindowFlags_NoMove);
    ImGui::End();
    if (!open) return;

    // 서버 크롬이 상단 24pt를 먹는다(레슨 8) — 탭바는 y>=30부터.
    ImGui::SetNextWindowPos(ImVec2(0, 30));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h - 30));
    if (ImGui::Begin("##mgrtabs", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
        const int prev = tab_;
        if (ImGui::BeginTabBar("mgrtabs")) {
            if (ImGui::BeginTabItem(koreanFont_ ? "권한" : "Perms")) {
                tab_ = 0;
                BuildPermissionsTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(koreanFont_ ? "트리거" : "Triggers")) {
                tab_ = 1;
                BuildTriggersTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(koreanFont_ ? "신뢰" : "Trust")) {
                tab_ = 2;
                BuildTrustTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(koreanFont_ ? "설치/로그" : "Apps")) {
                tab_ = 3;
                BuildInstalledTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        if (tab_ != prev) {   // 탭 진입 시 데이터 리프레시
            status_.clear();
            Refresh(tab_);
        }
        ImGui::Separator();
        if (!status_.empty()) ImGui::TextWrapped("%s", status_.c_str());
    }
    ImGui::End();
}

void ClientAgentMgrApp::BuildPermissionsTab() {
    if (ImGui::Button(koreanFont_ ? "새로고침" : "Refresh")) Refresh(0);
    if (!pendingPerm_.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.5f, 1.0f), "%s",
                           pendingPerm_.c_str());
    }
    if (ImGui::BeginTable(
            "perms", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(koreanFont_ ? "도구" : "Tool");
        ImGui::TableSetupColumn("gate");
        ImGui::TableSetupColumn(koreanFont_ ? "파일" : "File");
        ImGui::TableSetupColumn(koreanFont_ ? "기본값" : "Default");
        ImGui::TableSetupColumn(koreanFont_ ? "변경" : "Set");
        ImGui::TableHeadersRow();
        for (const MgrPermRow& row : perms_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.tool.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(row.gate.c_str());
            ImGui::TableSetColumnIndex(2);
            if (row.file.empty()) ImGui::TextDisabled("-");
            else ImGui::TextUnformatted(row.file.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextDisabled("%s", row.deflt.c_str());
            ImGui::TableSetColumnIndex(4);
            if (row.gate == std::string("server(fixed)")) {
                ImGui::TextDisabled(koreanFont_ ? "ask 고정" : "fixed ask");
            } else {
                static const char* kDec[3] = { "allow", "ask", "deny" };
                for (int d = 0; d < 3; ++d) {
                    if (row.effective == kDec[d]) continue;
                    ImGui::PushID((row.tool + kDec[d]).c_str());
                    if (ImGui::SmallButton(kDec[d])) {
                        pendingPerm_ = row.tool + " -> " + kDec[d] + " " +
                                      (koreanFont_ ? "(승인 대기)" : "(await)");
                        SendQuery("permission_set",
                                  std::string("{\"tool\":\"") +
                                      EscapeJson(row.tool) +
                                      "\",\"decision\":\"" + kDec[d] + "\"}",
                                  Query::PermissionSet, row.tool);
                    }
                    ImGui::PopID();
                    ImGui::SameLine();
                }
                ImGui::Dummy(ImVec2(0, 0));
            }
        }
        ImGui::EndTable();
    }
}

void ClientAgentMgrApp::BuildTriggersTab() {
    if (ImGui::Button(koreanFont_ ? "새로고침" : "Refresh")) Refresh(1);
    if (triggers_.empty())
        ImGui::TextDisabled(koreanFont_ ? "(트리거 없음)" : "(no triggers)");
    if (ImGui::BeginTable("trigs", 2, ImGuiTableFlags_Borders |
                                         ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn(koreanFont_ ? "이름" : "Name");
        ImGui::TableSetupColumn(koreanFont_ ? "상태" : "State");
        ImGui::TableHeadersRow();
        for (const MgrTriggerRow& row : triggers_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::PushID(("tg" + row.name).c_str());
            if (ImGui::SmallButton(row.enabled
                                       ? (koreanFont_ ? "끄기" : "off")
                                       : (koreanFont_ ? "켜기" : "on"))) {
                SendQuery("trigger_toggle",
                          std::string("{\"name\":\"") +
                              EscapeJson(row.name) + "\",\"on\":" +
                              (row.enabled ? "0" : "1") + "}",
                          Query::Toggle);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void ClientAgentMgrApp::BuildTrustTab() {
    if (ImGui::Button(koreanFont_ ? "새로고침" : "Refresh")) Refresh(2);
    if (!trustStoreError_.empty()) {
        // state 파일 corrupt/unreadable — 앱이 죽지 않고 에러 박스 (스펙 §5)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.4f, 1.0f));
        ImGui::TextWrapped("[!] %s", trustStoreError_.c_str());
        ImGui::PopStyleColor();
        return;
    }
    ImGui::TextDisabled(koreanFont_
        ? "해지는 jktriggers 재시작 후 적용된다 (로드 1회 규약)"
        : "revokes apply after jktriggers restart (load-once)");
    if (ImGui::BeginTable("trust", 4, ImGuiTableFlags_Borders |
                                          ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_ScrollY,
                          ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(koreanFont_ ? "이름" : "Name");
        ImGui::TableSetupColumn("source");
        ImGui::TableSetupColumn(koreanFont_ ? "지문" : "Fingerprint");
        ImGui::TableSetupColumn("");
        ImGui::TableHeadersRow();
        for (const MgrTrustRow& row : trust_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(row.source.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(row.shortFp.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::PushID(("rv" + row.shortFp).c_str());
            if (ImGui::SmallButton(koreanFont_ ? "해지" : "revoke") &&
                !row.fp.empty()) {
                SendQuery("trust_revoke",
                          std::string("{\"fingerprint\":\"") + row.fp +
                              "\"}",
                          Query::TrustRevoke);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void ClientAgentMgrApp::BuildInstalledTab() {
    if (ImGui::Button(koreanFont_ ? "새로고침" : "Refresh")) Refresh(3);
    if (ImGui::BeginTable("inst", 3, ImGuiTableFlags_Borders |
                                         ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn(koreanFont_ ? "이름" : "Name");
        ImGui::TableSetupColumn("kind");
        ImGui::TableSetupColumn(koreanFont_ ? "실행 중" : "Running");
        ImGui::TableHeadersRow();
        for (const MgrInstalledRow& row : installed_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(row.kind.c_str());
            ImGui::TableSetColumnIndex(2);
            if (row.running) {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
                ImGui::TextUnformatted(koreanFont_ ? "예" : "yes");
                ImGui::PopStyleColor();
            } else {
                ImGui::TextDisabled("-");
            }
        }
        ImGui::EndTable();
    }
    ImGui::Separator();
    ImGui::TextUnformatted(koreanFont_ ? "브로커 수행 기록 (receipts)"
                                       : "Broker receipts");
    if (receipts_.empty()) {
        ImGui::TextDisabled(koreanFont_ ? "(기록 없음)" : "(no rows)");
    }
    if (ImGui::BeginTable("rcpts", 3, ImGuiTableFlags_Borders |
                                          ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_ScrollY,
                          ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("ts");
        ImGui::TableSetupColumn("tool");
        ImGui::TableSetupColumn("ok");
        ImGui::TableHeadersRow();
        for (const MgrReceiptRow& row : receipts_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%lld", row.ts);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(row.tool.c_str());
            ImGui::TableSetColumnIndex(2);
            if (row.ok) ImGui::TextUnformatted("true");
            else {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(1.0f, 0.5f, 0.4f, 1.0f));
                ImGui::TextUnformatted("false");
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndTable();
    }
}

} // namespace jk