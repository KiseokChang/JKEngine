// File hub client (specs/2026-09-18-file-hub). ClientNotesApp pattern:
// request/response polling, no event subscription (files_audit rows come in
// on manual refresh — receipts are the single audit source). The browser and
// preview run on this app's own window connection, which the server's
// files gate treats as the user source (no approval — the user's own files).
#include <apps/ClientFilesApp.h>

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

// ts(epoch 초) → "mm-dd hh:mm" (notes 선례).
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

std::string FmtSize(long long bytes) {
    char buf[32];
    if (bytes >= 1024 * 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f MB",
                      static_cast<double>(bytes) / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f KB",
                      static_cast<double>(bytes) / 1024.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%lld B", bytes);
    }
    return buf;
}
} // namespace

ClientFilesApp::~ClientFilesApp() = default;

void ClientFilesApp::OnInit() {
    auto main = std::make_unique<SetRoot>("Files");
    main->SetWindowRect(JKRect{ 0, 0, 860, 560 });
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
    // 초기 경로: C 드라이브 루트(스펙 §2.3 — 절대 경로 필수).
    std::snprintf(pathBuf_, sizeof(pathBuf_), "C:\\");
    SendQuery("files_list", "{\"path\":\"C:\\\\\"}", Query::List);
    SendQuery("files_audit", "{}", Query::Audit);
}

void ClientFilesApp::OnClose() {
    if (imguiReady_) {
        ImGui_ImplJKWindow_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
}

void ClientFilesApp::OnThemeChanged() {
    if (imguiReady_) jk::theme::ApplyImGuiTheme();   // docs/52 hot-swap
}

bool ClientFilesApp::PreProcessMessage(const JKEvent& ev) {
    ImGui_ImplJKWindow_ProcessJKEvent(ev);
    if (ev.type == JKEventType::Timer) frameDirty_ = true;
    return true;
}

void ClientFilesApp::OnFrameCommitted() { frameDirty_ = false; }

void ClientFilesApp::SendQuery(const char* tool, const std::string& args,
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

void ClientFilesApp::PollReplies() {
    jk::client::JKClientSurface* surface = Surface();
    if (!surface) return;
    jk::client::AgentReply reply;
    while (surface->PollAgentReply(reply)) {
        Query kind = Query::List;
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

std::string ClientFilesApp::JoinPath(const std::string& name) const {
    std::string p = pathBuf_;
    // 구분자 정규화: 끝이 구분자가 아니면 붙인다(서버는 두 형식 다 수용).
    if (!p.empty() && p.back() != '\\' && p.back() != '/') p += '\\';
    return p + name;
}

void ClientFilesApp::ApplyReply(Query kind, const std::string& json) {
    jk::agent::AgentJson r(json);
    if (kind == Query::List) {
        entries_.clear();
        listCapped_ = false;
        if (!r.ok()) {
            // bad_path/not_found — status줄로 표면화(목록은 유지).
            status_ = "[!] " + json;
            return;
        }
        int cnt = 0;
        if (r.GetArraySize("entries", cnt)) {
            for (int i = 0; i < cnt; ++i) {
                FileEntryUi e;
                int szInt = 0, mtInt = 0;
                r.GetArrStr("entries", i, "name", e.name);
                std::string kindStr;
                r.GetArrStr("entries", i, "kind", kindStr);
                e.dir = (kindStr == "dir");
                r.GetArrInt("entries", i, "size", szInt);
                r.GetArrInt("entries", i, "mtime", mtInt);
                e.size = szInt;
                e.mtime = mtInt;
                entries_.push_back(e);
            }
        }
        int cappedInt = 0;
        if (r.GetInt("capped", cappedInt)) listCapped_ = (cappedInt == 1);
        return;
    }
    if (kind == Query::Read) {
        prevText_.clear();
        prevBinary_ = false;
        prevTrunc_ = false;
        prevSize_ = 0;
        if (!r.ok()) {
            status_ = "[!] " + json;
            return;
        }
        // 응답 계약: binary/truncated/size는 int 직렬화 — GetInt로 읽는다.
        int binInt = 0, truncInt = 0, sizeInt = 0;
        r.GetStr("text", prevText_);
        r.GetInt("binary", binInt);
        r.GetInt("truncated", truncInt);
        r.GetInt("size", sizeInt);
        prevBinary_ = (binInt == 1);
        prevTrunc_ = (truncInt == 1);
        prevSize_ = sizeInt;
        return;
    }
    // Query::Audit — files_audit 봉투 {ok,rows:[...]}.
    audit_.clear();
    if (!r.ok()) { status_ = "[!] " + json; return; }
    int cnt = 0;
    if (r.GetArraySize("rows", cnt)) {
        for (int i = 0; i < cnt; ++i) {
            FileAuditRowUi row;
            int tsInt = 0, okInt = 0;
            r.GetArrStr("rows", i, "tool", row.tool);
            r.GetArrStr("rows", i, "path", row.path);
            r.GetArrInt("rows", i, "ts", tsInt);
            r.GetArrInt("rows", i, "ok", okInt);
            row.ts = tsInt;
            row.ok = (okInt == 1);
            audit_.push_back(row);
        }
    }
}

void ClientFilesApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
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

void ClientFilesApp::BuildUi(int w, int h) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
    // Begin-false여도 End()는 반드시 호출(imgui.h 계약 — docs/53 §9 잔여).
    const bool open = ImGui::Begin("files", nullptr,
                                   ImGuiWindowFlags_NoDecoration |
                                       ImGuiWindowFlags_NoMove);
    ImGui::End();
    if (!open) return;

    // 서버 크롬이 상단 24pt를 먹는다(레슨 8) — 패널은 y>=30부터, 감사 패널은
    // 하단 24px 상태줄 위(140px) — 브라우저/미리보기 영역은 그 사이.
    const float top = 30.0f;
    const float bottomPanel = 130.0f;
    const float bodyH = (float)h - top - bottomPanel - 24.0f;
    const int leftW = 360;

    ImGui::SetNextWindowPos(ImVec2(0, top));
    ImGui::SetNextWindowSize(ImVec2((float)w, bodyH));
    ImGui::Begin("##filesbody", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    BuildBrowser(leftW);
    ImGui::SameLine();
    BuildPreview(leftW);
    ImGui::End();

    BuildAuditPanel(w, h);

    // 하단 상태줄.
    if (!status_.empty()) {
        ImGui::SetNextWindowPos(ImVec2(0, (float)h - 24));
        ImGui::SetNextWindowSize(ImVec2((float)w, 24));
        if (ImGui::Begin("##filestatus", nullptr,
                         ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove)) {
            ImGui::TextUnformatted(status_.c_str());
        }
        ImGui::End();
    }
}

void ClientFilesApp::BuildBrowser(int leftW) {
    ImGui::BeginGroup();
    const bool goEnter = ImGui::InputText("##path", pathBuf_,
                                          sizeof(pathBuf_)) &&
                          ImGui::IsItemFocused() &&
                          ImGui::IsKeyPressed(ImGuiKey_Enter);
    ImGui::SameLine();
    const bool upClicked = ImGui::Button(koreanFont_ ? "상위" : "up");
    ImGui::SameLine();
    const bool goClicked = ImGui::Button(koreanFont_ ? "이동" : "go");
    if (goEnter || goClicked) {
        SendQuery("files_list",
                  "{\"path\":\"" + EscapeJson(std::string(pathBuf_)) + "\"}",
                  Query::List);
    }
    if (upClicked) {
        // 뒤에서 첫 구분자 제거 — "C:\" 루트면 유지.
        std::string p = pathBuf_;
        while (!p.empty() && (p.back() == '\\' || p.back() == '/')) {
            p.pop_back();   // 끝 구분자 1회 제거
            break;
        }
        const size_t slash = p.find_last_of("\\/");
        if (slash == std::string::npos || slash < 2) {
            std::snprintf(pathBuf_, sizeof(pathBuf_), "%c:\\", p.empty()
                                                               ? 'C'
                                                               : p[0]);
        } else {
            std::snprintf(pathBuf_, sizeof(pathBuf_), "%s\\",
                          p.substr(0, slash).c_str());
        }
        SendQuery("files_list",
                  "{\"path\":\"" + EscapeJson(std::string(pathBuf_)) + "\"}",
                  Query::List);
    }
    if (ImGui::BeginChild("filelist",
                          ImVec2((float)leftW, -ImGui::GetFrameHeightWithSpacing() * 2),
                          ImGuiChildFlags_Borders)) {
        for (size_t i = 0; i < entries_.size(); ++i) {
            const FileEntryUi& e = entries_[i];
            ImGui::PushID(static_cast<int>(i));
            const std::string label =
                (e.dir ? "[DIR] " : "      ") + e.name;
            if (ImGui::Selectable(label.c_str(), false,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                if (e.dir) {
                    const std::string child = JoinPath(e.name);
                    std::snprintf(pathBuf_, sizeof(pathBuf_), "%s",
                                  child.c_str());
                    SendQuery("files_list", "{\"path\":\"" +
                                                EscapeJson(child) + "\"}",
                              Query::List);
                } else {
                    const std::string child = JoinPath(e.name);
                    SendQuery("files_read", "{\"path\":\"" +
                                                EscapeJson(child) + "\"}",
                              Query::Read);
                    prevName_ = e.name;
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s%s", e.dir ? "dir" : "file",
                                  e.dir ? "" : (std::string(" ") +
                                                FmtSize(e.size)).c_str());
            }
            ImGui::PopID();
        }
        if (entries_.empty()) {
            ImGui::TextDisabled("%s", koreanFont_
                ? "(비어 있음)" : "(empty)");
        }
        if (listCapped_ && !entries_.empty()) {
            ImGui::TextDisabled("%s", koreanFont_
                ? "…512행 상한 (상위 디렉터리로 탐색)"
                : "…512-row cap");
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    if (ImGui::Button(koreanFont_ ? "새로고침" : "refresh")) {
        SendQuery("files_list",
                  "{\"path\":\"" + EscapeJson(std::string(pathBuf_)) + "\"}",
                  Query::List);
    }
    ImGui::EndGroup();
}

void ClientFilesApp::BuildPreview(int leftW) {
    // 헤더: 이름 + size(+이진/잘림 배지). 이진/잘림은 응답 계약(0/1 문자열).
    std::string header;
    if (prevName_.empty()) {
        header = koreanFont_ ? "(파일 미리보기 없음)" : "(no preview)";
    } else {
        header = prevName_;
        if (prevBinary_) {
            header += koreanFont_ ? " · 이진 파일" : " · binary";
        } else {
            header += " · " + FmtSize(prevSize_);
            if (prevTrunc_) {
                header += koreanFont_ ? " (잘림)" : " (truncated)";
            }
        }
    }
    ImGui::BeginGroup();
    ImGui::TextUnformatted(header.c_str());
    if (ImGui::BeginChild("preview", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2),
                          ImGuiChildFlags_Borders)) {
        if (!prevName_.empty() && !prevBinary_) {
            ImGui::TextWrapped("%s", prevText_.c_str());
        } else if (!prevName_.empty()) {
            ImGui::TextDisabled("%s", koreanFont_
                ? "(이진 파일 — 미리보기 없음)"
                : "(binary — no preview)");
        }
    }
    ImGui::EndChild();
    ImGui::EndGroup();
}

void ClientFilesApp::BuildAuditPanel(int w, int h) {
    // 하단 패널: 에이전트 파일 접근 최근 행(files_audit) — 수동 새로고침
    // (스펙 §2.3 — 폴링 없음; receipts가 단일 감사원).
    ImGui::SetNextWindowPos(ImVec2(0, (float)h - 178.0f));
    ImGui::SetNextWindowSize(ImVec2((float)w, 154.0f));
    ImGui::Begin("##fileaudit", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    ImGui::TextUnformatted(koreanFont_ ? "에이전트 파일 접근 (최근)" :
                                         "agent file access (recent)");
    ImGui::SameLine();
    if (ImGui::SmallButton(koreanFont_ ? "새로고침" : "refresh")) {
        SendQuery("files_audit", "{}", Query::Audit);
    }
    if (ImGui::BeginChild("auditlist",
                          ImVec2(0, 0), ImGuiChildFlags_None)) {
        for (const FileAuditRowUi& row : audit_) {
            ImGui::PushID(static_cast<int>(row.ts) *
                              31 + static_cast<int>(row.tool.size()));
            const std::string line =
                FmtStamp(row.ts) + "  " +
                (row.tool == "files_list"
                     ? "list"
                     : row.tool == "files_read" ? "read" : "audit") +
                "  " + row.path + (row.ok ? "" : koreanFont_ ? "  [거부]" : "  [denied]");
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopID();
        }
        if (audit_.empty()) {
            ImGui::TextDisabled("%s", koreanFont_
                ? "(에이전트 접근 기록 없음)"
                : "(no agent access yet)");
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace jk