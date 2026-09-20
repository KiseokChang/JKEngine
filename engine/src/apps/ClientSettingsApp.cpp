// Settings hub client (specs/2026-09-18-settings-hub). ClientAgentMgrApp
// pattern: request/response polling, no event subscription (settings screens
// are static data). Single scroll, 5 sections — theme / triggers / data /
// permissions (summary + route to agentmgr) / devices.
#include <apps/ClientSettingsApp.h>

#include <agent/JKAgentJson.h>
#include <imgui_impl_jkwindow.h>
#include "theme/JKThemeImGui.h"
#include <JKWindow.h>
#include <SDL.h>

#include <cfloat>
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
} // namespace

ClientSettingsApp::~ClientSettingsApp() = default;

void ClientSettingsApp::OnInit() {
    auto main = std::make_unique<SetRoot>("Settings");
    main->SetWindowRect(JKRect{ 0, 0, 900, 620 });
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
    SendQuery("settings_read", "{}", Query::Read);
    SendQuery("agent_permissions", "{}", Query::Perms);
}

void ClientSettingsApp::OnClose() {
    if (imguiReady_) {
        ImGui_ImplJKWindow_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
}

void ClientSettingsApp::OnThemeChanged() {
    if (imguiReady_) jk::theme::ApplyImGuiTheme();   // docs/52 hot-swap
}

bool ClientSettingsApp::PreProcessMessage(const JKEvent& ev) {
    ImGui_ImplJKWindow_ProcessJKEvent(ev);
    if (ev.type == JKEventType::Timer) frameDirty_ = true;
    return true;
}

void ClientSettingsApp::OnFrameCommitted() { frameDirty_ = false; }

void ClientSettingsApp::SendQuery(const char* tool, const std::string& args,
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

void ClientSettingsApp::PollReplies() {
    jk::client::JKClientSurface* surface = Surface();
    if (!surface) return;
    jk::client::AgentReply reply;
    while (surface->PollAgentReply(reply)) {
        Query kind = Query::Read;
        std::string arg;
        bool found = false;
        for (auto it = pending_.begin(); it != pending_.end(); ++it) {
            if (it->first == reply.queryId) {
                kind = it->second.kind;
                arg = it->second.arg;
                pending_.erase(it);
                found = true;
                break;
            }
        }
        if (found) ApplyReply(kind, reply.json, arg);
    }
}

void ClientSettingsApp::ApplyRead(const std::string& json) {
    kv_.clear();
    triggers_.clear();
    layouts_.clear();
    receiptRows_ = 0;
    receiptLastTs_ = 0;
    jk::agent::AgentJson r(json);
    if (!r.ok()) { status_ = "[!] " + json; return; }
    int cnt = 0;
    if (r.GetArraySize("settings", cnt)) {
        for (int i = 0; i < cnt; ++i) {
            std::string key, value;
            r.GetArrStr("settings", i, "key", key);
            r.GetArrStr("settings", i, "value", value);
            if (key.empty()) continue;
            kv_[key] = value;
            if (key.rfind("trigger.", 0) == 0) {
                SetTriggerRow row;
                row.name = key.substr(8);
                row.enabled = value == "1" ? 1 : 0;
                triggers_.push_back(row);
            } else if (key.rfind("layout.", 0) == 0) {
                layouts_.push_back(key.substr(7));
            }
        }
    }
    long long rows = 0, lastTs = 0;
    if (r.GetObjInt("receipts", "rows", cnt)) rows = cnt;
    if (r.GetObjInt("receipts", "last_ts", cnt)) lastTs = cnt;
    receiptRows_ = rows;
    receiptLastTs_ = lastTs;
    // 볼륨/보존 UI 임시값 재동기 — 슬라이더 드래그 중이 아닐 때만.
    if (!volumeDragging_) {
        const auto it = kv_.find("audio_master_volume");
        volumeTemp_ = it != kv_.end() ? std::atoi(it->second.c_str()) : 80;
    }
    retentionSel_ = -1;   // 콤보는 kv_로부터 재계산
    // 보조 폰트 버퍼 1회 시드 — **첫 settings_read 도착 시점**(리뷰 fix-2).
    // kv_가 채워진 지금이 curFb의 진실원 시점이다(첫 프레임 래치는 답신 전
    // 공백을 근거로 래치했다). 래치 후 재시드 없음 — 사용자 삭제 원본 유지.
    if (!fallbackSeeded_) {
        fallbackSeeded_ = true;
        const auto fb = kv_.find("text.font_fallback");
        if (fb != kv_.end() && !fb->second.empty()) {
            std::snprintf(fallbackFontBuf_, sizeof(fallbackFontBuf_), "%s",
                          fb->second.c_str());
        }
    }
    // 셀 확대 배율 1회 시드 — fallbackFontBuf_와 동일 래치(첫 settings_read
    // 도착). 설정되면 항상 비지 않는 숫자 문자열, 키 부재 = 기본 "1.0" 표시.
    if (!scaleSeeded_) {
        scaleSeeded_ = true;
        const auto sc = kv_.find("text.font_scale");
        if (sc != kv_.end() && !sc->second.empty()) {
            std::snprintf(scaleBuf_, sizeof(scaleBuf_), "%s",
                          sc->second.c_str());
        } else {
            std::snprintf(scaleBuf_, sizeof(scaleBuf_), "1.0");
        }
    }
}

void ClientSettingsApp::ApplyReply(Query kind, const std::string& json,
                                   const std::string& arg) {
    jk::agent::AgentJson r(json);
    switch (kind) {
        case Query::Read:
            if (!r.ok()) { status_ = "[!] " + json; break; }
            status_.clear();
            ApplyRead(json);
            break;
        case Query::Perms: {
            perms_.clear();
            if (!r.ok()) { status_ = "[!] " + json; break; }
            int cnt = 0;
            if (r.GetArraySize("perms", cnt)) {
                for (int i = 0; i < cnt; ++i) {
                    SetPermRow row;
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
        case Query::ThemeSet:
            if (r.ok()) {
                std::string preset;
                r.GetStr("preset", preset);
                if (!preset.empty()) kv_["theme.current"] = preset;
                status_ = koreanFont_ ? "테마 적용됨" : "theme applied";
            } else {
                status_ = "[!] " + json;
            }
            break;
        case Query::TriggerToggle:
            if (r.ok()) {
                status_.clear();
                SendQuery("settings_read", "{}", Query::Read);
            } else {
                status_ = "[!] " + json;
            }
            break;
        case Query::Set:
            pendingCapture_.clear();
            if (r.ok()) {
                // applied의 키는 동적(파서 계약 — 2레벨 리더 못 읽음) —
                // pending_.arg로 전달한 key를 쓴다.
                status_ = koreanFont_ ? "적용됨: " + arg : "applied: " + arg;
                if (json.find("prune_failed") != std::string::npos) {
                    status_ += koreanFont_ ? " (정리 실패 — 다음 set에서 재정리)"
                                           : " (prune failed)";
                }
                SendQuery("settings_read", "{}", Query::Read);
                if (arg == "capture_allow")
                    SendQuery("agent_permissions", "{}", Query::Perms);
            } else if (json.find("approval_unavailable") != std::string::npos) {
                status_ = koreanFont_
                    ? "[!] 캡처 승인 대기 불가 — jkchat(에이전트 구독) 필요"
                    : "[!] approval_unavailable — no subscriber";
            } else if (json.find("denied_by_user") != std::string::npos) {
                status_ = koreanFont_ ? "승인 거부됨" : "denied";
            } else {
                status_ = "[!] " + json;
            }
            break;
        case Query::LayoutSave:
        case Query::LayoutRestore:
            if (r.ok()) {
                status_ = koreanFont_ ? "레이아웃 완료" : "layout done";
                SendQuery("settings_read", "{}", Query::Read);
            } else {
                status_ = "[!] " + json;
            }
            break;
        case Query::Launch:
            if (!r.ok()) status_ = "[!] " + json;
            break;
    }
}

void ClientSettingsApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
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

namespace {
void SectionHeader(const char* title) {
    ImGui::SeparatorText(title);
    ImGui::Spacing();
}
} // namespace

void ClientSettingsApp::BuildUi(int w, int h) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
    // Begin-false여도 End()는 반드시 호출(imgui.h 계약 — docs/53 §9 잔여).
    const bool open = ImGui::Begin("settings", nullptr,
                                   ImGuiWindowFlags_NoDecoration |
                                       ImGuiWindowFlags_NoMove);
    ImGui::End();
    if (!open) return;

    // 서버 크롬이 상단 24pt를 먹는다(레슨 8) — 본문은 y>=30부터.
    ImGui::SetNextWindowPos(ImVec2(0, 30));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h - 30));
    if (!ImGui::Begin("##settingsbody", nullptr,
                      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        ImGui::End();
        return;
    }

    const char* L = koreanFont_ ? "테마" : "Theme";
    const auto KvStr = [&](const char* key, const char* def) -> std::string {
        const auto it = kv_.find(key);
        return it != kv_.end() ? it->second : std::string(def);
    };

    // ---- 1. 테마 ----
    SectionHeader(L);
    ImGui::TextDisabled("%s: %s", koreanFont_ ? "현재" : "current",
                        KvStr("theme.current", "dark").c_str());
    {
        static const char* kPresets[] = { "dark", "light", "classic" };
        for (const char* p : kPresets) {
            const bool cur = KvStr("theme.current", "dark") == p;
            ImGui::PushID(p);
            if (cur) {
                ImGui::BeginDisabled();
                ImGui::Button(p);
                ImGui::EndDisabled();
            } else if (ImGui::Button(p)) {
                SendQuery("theme_set",
                          std::string("{\"preset\":\"") + p + "\"}",
                          Query::ThemeSet, p);
            }
            ImGui::PopID();
            ImGui::SameLine();
        }
        ImGui::Dummy(ImVec2(0, 0));
    }

    // ---- 2. 트리거 ----
    SectionHeader(koreanFont_ ? "트리거" : "Triggers");
    for (const SetTriggerRow& row : triggers_) {
        bool on = row.enabled != 0;
        ImGui::PushID(row.name.c_str());
        if (ImGui::Checkbox(row.name.c_str(), &on)) {
            SendQuery("trigger_toggle",
                      std::string("{\"name\":\"") + EscapeJson(row.name) +
                          "\",\"on\":" + (on ? "1" : "0") + "}",
                      Query::TriggerToggle, row.name);
        }
        ImGui::PopID();
    }
    if (triggers_.empty())
        ImGui::TextDisabled(koreanFont_ ? "(트리거 없음)" : "(no triggers)");
    {
        const int idle =
            std::atoi(KvStr("idle_minutes", "30").c_str());
        int idleTemp = idle;
        ImGui::SetNextItemWidth(120);
        if (ImGui::InputInt(koreanFont_ ? "유휴 임계(분)" : "idle minutes",
                            &idleTemp) && idleTemp >= 0 && idleTemp != idle) {
            SendQuery("settings_set",
                      "{\"key\":\"idle_minutes\",\"value\":" +
                          std::to_string(idleTemp) + "}",
                      Query::Set, "idle_minutes");
        }
    }

    // ---- 텍스트 폰트 (docs/63 §4) ----
    SectionHeader(koreanFont_ ? "텍스트" : "Text");
    {
        // 오버라이드 없음(빈 값)이면 플랫폼 기본값을 표시한다 — 서버의
        // textFontPath_는 빈 문자열(기본값 합성은 ResolveDesktopFontPath).
        const std::string curPath = KvStr("text.font_path", "");
        ImGui::TextDisabled(
            "%s: %s",
            koreanFont_ ? "벡터 폰트" : "vector font",
            (curPath.empty() ? std::string("C:\\Windows\\Fonts\\malgun.ttf")
                             : curPath)
                .c_str());
        if (textFontBuf_[0] == '\0' && !curPath.empty()) {
            std::snprintf(textFontBuf_, sizeof(textFontBuf_), "%s",
                          curPath.c_str());
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText(koreanFont_ ? "폰트 경로(재시작 적용)"
                                         : "font path (applies on restart)",
                             textFontBuf_, sizeof(textFontBuf_),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            const std::string v(textFontBuf_);
            if (!v.empty()) {
                SendQuery("settings_set",
                          std::string("{\"key\":\"text_font_path\",\"value\":"
                                      "\"") +
                              EscapeJson(v) + "\"}",
                          Query::Set, "text_font_path");
            }
        }

        // 보조 폰트 체인 (docs/63 §6 2단계): 미커버 cp의 승계 폰트 — 빈 값 =
        // 해제 허용(text_font_path와 반대). 현재값과 다를 때만 전송(빈 Enter
        // 스팸 방지). 버퍼 시드는 ApplyRead(첫 settings_read 도착)에서 1회 —
        // 프레임 시점 시드는 답신 전 kv_ 공백을 근거로 삼는 사각(리뷰 fix-2).
        const std::string curFb = KvStr("text.font_fallback", "");
        ImGui::TextDisabled(
            "%s: %s",
            koreanFont_ ? "보조 폰트" : "fallback font",
            (curFb.empty() ? std::string("(none)") : curFb).c_str());
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText(koreanFont_ ? "보조 폰트(재시작 적용, 빈 값=해제)"
                                         : "fallback font (restart, empty=off)",
                             fallbackFontBuf_, sizeof(fallbackFontBuf_),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            const std::string v(fallbackFontBuf_);
            if (v != curFb) {
                SendQuery("settings_set",
                          std::string("{\"key\":\"text_font_fallback\","
                                      "\"value\":\"") +
                              EscapeJson(v) + "\"}",
                          Query::Set, "text_font_fallback");
            }
        }

        // 셀 확대 배율 (docs/63 §6 Task 3): 숫자 문자열(1.0–3.0) — 옵트인,
        // 재시작 적용. 시드는 ApplyRead 1회 래치(위). 현재값과 다를 때만 전송
        // (빈 Enter 스팸 방지 — 보조 폰트 행 선례).
        const std::string curScale = KvStr("text.font_scale", "1.0");
        ImGui::TextDisabled(
            "%s: %s",
            koreanFont_ ? "셀 배율" : "cell scale",
            (curScale.empty() ? std::string("1.0") : curScale).c_str());
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText(koreanFont_ ? "셀 배율(1.0-3.0, 재시작 적용)"
                                         : "cell scale (1.0-3.0, restart)",
                             scaleBuf_, sizeof(scaleBuf_),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            const std::string v(scaleBuf_);
            if (v != curScale) {
                SendQuery("settings_set",
                          std::string("{\"key\":\"text_font_scale\","
                                      "\"value\":\"") +
                              EscapeJson(v) + "\"}",
                          Query::Set, "text_font_scale");
            }
        }
    }

    // ---- 3. 데이터 ----
    SectionHeader(koreanFont_ ? "데이터" : "Data");
    {
        // ts는 초 단위(파서 계약) — 로컬 시간으로 표시.
        char tsBuf[32] = "-";
        if (receiptLastTs_ > 0) {
            const std::time_t tt = static_cast<std::time_t>(receiptLastTs_);
            std::tm lt{};
#ifdef _WIN32
            localtime_s(&lt, &tt);
#else
            localtime_r(&tt, &lt);
#endif
            std::strftime(tsBuf, sizeof(tsBuf), "%m-%d %H:%M:%S", &lt);
        }
        // opus 리뷰 MINOR-3: settings_read의 rows는 256KiB 꼬리 행수다 —
        // 전체 행수로 읽지 않게 라벨에 명시.
        ImGui::Text(koreanFont_ ? "receipt 행수(최근 256KiB): %lld, 최근: %s"
                                : "receipts rows (last 256KiB): %lld, last: %s",
                    receiptRows_, tsBuf);
    }
    {
        // 보존기간 콤보: 현재값과 일치하는 항목 선택(없으면 "무기한").
        const auto it = kv_.find("receipt_retention_days");
        const int cur = it != kv_.end() ? std::atoi(it->second.c_str()) : 0;
        static const int kDays[] = { 7, 30, 90, 365 };
        if (retentionSel_ < 0) {
            retentionSel_ = 0;   // 0 = 무기한 라벨
            for (int i = 0; i < 4; ++i) {
                if (kDays[i] == cur) { retentionSel_ = i + 1; break; }
            }
        }
        // opus 리뷰 MINOR-4: 현재값이 프리셋 외(수동 편집 등)면 "무기한
        // (현재)" 라벨이 거짓말한다 — 현재값이 무기한일 때만 "(현재)"를 붙이고
        // 프리셋 외 값은 별도 힌트로 표시.
        char zeroLabel[40];
        if (cur == 0) {
            std::snprintf(zeroLabel, sizeof(zeroLabel), "%s",
                          koreanFont_ ? "무기한 (현재)" : "unlimited (current)");
        } else {
            std::snprintf(zeroLabel, sizeof(zeroLabel), "%s",
                          koreanFont_ ? "무기한" : "unlimited");
        }
        const char* labels[] = { zeroLabel, "7", "30", "90", "365" };
        const bool unmatched = cur > 0 && retentionSel_ == 0;
        ImGui::SetNextItemWidth(160);
        if (ImGui::Combo(koreanFont_ ? "receipt 보존" : "receipt retention",
                         &retentionSel_, labels, 5)) {
            // 무기한(0)은 set 불가(§2.2) — 현재값이 무기한이면 콤보는 표시만.
            if (retentionSel_ > 0) {
                SendQuery("settings_set",
                          "{\"key\":\"receipt_retention_days\",\"value\":" +
                              std::to_string(kDays[retentionSel_ - 1]) + "}",
                          Query::Set, "receipt_retention_days");
            } else {
                status_ = koreanFont_
                    ? "무기한은 현재값 — 임계는 7/30/90/365로 설정"
                    : "unlimited is the current value";
            }
        }
        if (unmatched) {
            ImGui::SameLine();
            ImGui::TextDisabled(koreanFont_ ? "(현재 %d일)" : "(current %d d)",
                                cur);
        }
    }
    ImGui::Separator();
    {
        static char layoutName[64] = "";
        ImGui::SetNextItemWidth(180);
        ImGui::InputText(koreanFont_ ? "레이아웃 이름" : "layout name",
                         layoutName, sizeof(layoutName));
        if (ImGui::Button(koreanFont_ ? "저장" : "save") && layoutName[0]) {
            SendQuery("save_layout",
                      std::string("{\"name\":\"") + EscapeJson(layoutName) +
                          "\"}", Query::LayoutSave, layoutName);
        }
        ImGui::SameLine();
        if (ImGui::Button(koreanFont_ ? "복원" : "restore") && layoutName[0]) {
            SendQuery("restore_layout",
                      std::string("{\"name\":\"") + EscapeJson(layoutName) +
                          "\"}", Query::LayoutRestore, layoutName);
        }
        if (!layouts_.empty()) {
            ImGui::TextDisabled(
                koreanFont_ ? "저장됨: %s" : "saved: %s",
                [&] {
                    std::string joined;
                    for (const std::string& n : layouts_) {
                        if (!joined.empty()) joined += ", ";
                        joined += n;
                    }
                    return joined;
                }().c_str());
        }
    }

    // ---- 4. 권한 (요약 + 라우팅 — 본체는 agentmgr) ----
    SectionHeader(koreanFont_ ? "권한" : "Permissions");
    {
        int overrides = 0;
        for (const SetPermRow& row : perms_) {
            if (!row.file.empty() && row.file != row.deflt) ++overrides;
        }
        ImGui::Text(
            koreanFont_ ? "매트릭스 %d행, 파일 오버라이드 %d건"
                        : "matrix %d rows, file overrides %d",
            static_cast<int>(perms_.size()), overrides);
        if (!perms_.empty() &&
            ImGui::BeginTable("setperms", 3,
                              ImGuiTableFlags_Borders |
                                  ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn(
                koreanFont_ ? "도구(오버라이드)" : "Tool (override)");
            ImGui::TableSetupColumn("gate");
            ImGui::TableSetupColumn(
                koreanFont_ ? "파일/기본값" : "file/default");
            ImGui::TableHeadersRow();
            for (const SetPermRow& row : perms_) {
                if (row.file.empty() || row.file == row.deflt) continue;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(row.tool.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(row.gate.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s / %s", row.file.c_str(), row.deflt.c_str());
            }
            ImGui::EndTable();
        }
        if (ImGui::Button(koreanFont_ ? "에이전트 관리자 열기"
                                      : "Open Agent Manager")) {
            SendQuery("launch_app", "{\"app\":\"agentmgr\"}", Query::Launch);
        }
    }

    // ---- 5. 장치 ----
    SectionHeader(koreanFont_ ? "장치" : "Devices");
    {
        bool mute = KvStr("audio_master_mute", "0") == "1";
        if (ImGui::Checkbox(koreanFont_ ? "전역 오디오 음소거"
                                        : "global audio mute", &mute)) {
            SendQuery("settings_set",
                      std::string("{\"key\":\"audio_master_mute\",\"value\":") +
                          (mute ? "1" : "0") + "}",
                      Query::Set, "audio_master_mute");
        }
        ImGui::SetNextItemWidth(220);
        if (ImGui::SliderInt(koreanFont_ ? "볼륨" : "volume", &volumeTemp_, 0,
                             100)) {
            volumeDragging_ = true;   // 커밋-on-release: 드래그 중엔 set 안 함
        }
        if (volumeDragging_ && !ImGui::IsItemActive()) {
            volumeDragging_ = false;
            SendQuery("settings_set",
                      "{\"key\":\"audio_master_volume\",\"value\":" +
                          std::to_string(volumeTemp_) + "}",
                      Query::Set, "audio_master_volume");
        }
        if (!mute && volumeDragging_) {
            ImGui::TextDisabled(koreanFont_ ? "해제 시 적용" : "applies on release");
        }
        // opus 리뷰 MINOR-2: JKSoundManager 소비자는 게임/런처 — vplayer는
        // 자체 SDL 오디오라 미적용. 과대광고 방지 힌트.
        ImGui::TextDisabled(
            koreanFont_ ? "※ vplayer(자체 오디오) 미적용"
                        : "(vplayer uses its own audio — not covered)");
        // 캡처 스위치 — 키별 Ask(§2.2): 승인은 jkchat 스트립. 현재 상태의
        // 원천은 agent_permissions의 capture_window 오버라이드(설정 화면은
        // 정적 데이터 — 조작 후 Perms 재수집).
        bool cap = true;
        for (const SetPermRow& row : perms_) {
            if (row.tool == "capture_window") {
                cap = (row.file.empty() ? row.effective : row.file) == "allow";
                break;
            }
        }
        if (ImGui::Checkbox(koreanFont_ ? "캡처 허용" : "allow capture", &cap)) {
            pendingCapture_ = koreanFont_ ? "캡처 승인 대기 (jkchat)"
                                          : "awaiting approval (jkchat)";
            SendQuery("settings_set",
                      std::string("{\"key\":\"capture_allow\",\"value\":") +
                          (cap ? "1" : "0") + "}",
                      Query::Set, "capture_allow");
        }
        if (!pendingCapture_.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.5f, 1.0f), "%s",
                               pendingCapture_.c_str());
        }
        // N7 예정 행 — 물리 활성은 4단계(스펙 §3 비목표).
        {
            static bool reserved = false;
            ImGui::BeginDisabled();
            ImGui::Checkbox(koreanFont_ ? "마이크/카메라 — 4단계 예정"
                                        : "microphone/camera — phase 4",
                            &reserved);
            ImGui::EndDisabled();
        }
    }

    ImGui::Separator();
    if (!status_.empty()) ImGui::TextWrapped("%s", status_.c_str());
    ImGui::End();
}
} // namespace jk
