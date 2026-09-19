// File open dialog (file dialog design §1b, Task 2). Structure mirrors
// ClientPaletteApp (16 ms timer, ImGui over a root window, agent queries on
// the window connection) with the snap app's Esc self-close precedent. The
// listing/filter logic is ported from the legacy JKFileDialog (src/
// JKFileDialog.cpp MatchFilter + dirs-first enumeration) — dialog-local on
// std::filesystem, NOT promoted to jkcore (a module owns its core copy).
//
// Channel contract (Task 1 tools only — no new tool names): after connect the
// app queries `file_dialog_params` once (the server spawn argv json is a
// transport convenience, design D3) and applies filter/start; at the end it
// sends `file_open_result {ok, path?}` exactly once on every exit path.
#include <apps/ClientFileDialogApp.h>

#include <agent/JKAgentJson.h>
#include <imgui_impl_jkwindow.h>
#include "theme/JKThemeImGui.h"
#include <JKWindow.h>
#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>

namespace jk {
namespace fs = std::filesystem;

namespace {

// Root window paints the client area with the theme clear color (palette
// idiom) — the chrome (title bar + border, non-resizable) is painted by
// JKWindow::PaintWindow because the root is NOT chromeless.
class FileDlgRoot : public JKWindow {
public:
    explicit FileDlgRoot(const std::string& title) : JKWindow(title) {}
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

bool IEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// Windows-style filter label "동영상 (*.mp4;*.mkv;...)" carries the matchable
// pattern list inside the parens — the raw string split on ';' would yield
// first pattern "동영상 (*.mp4" (an exact-name pattern matching nothing) and
// last pattern "*.mov)" (suffix ".mov)"), hiding every real file. Extract the
// parenthesized list when the string contains both parens in order and the
// content is non-empty; otherwise the trimmed whole string IS the list.
std::string ExtractPatterns(const std::string& filter) {
    const std::string trimmed = Trim(filter);
    const size_t open = trimmed.find('(');
    if (open == std::string::npos) return trimmed;
    const size_t close = trimmed.rfind(')');
    if (close == std::string::npos || close <= open + 1) return trimmed;
    return Trim(trimmed.substr(open + 1, close - open - 1));
}

// Verbatim port of JKFileDialog.cpp:36-63 MatchFilter — `*.ext` suffix match,
// exact name match, `;`-separated pattern list, `*`/`*.*` = everything.
// (The Windows label wrapper is stripped first — ExtractPatterns above.)
bool MatchFilter(const std::string& name, const std::string& filter) {
    std::string f = ExtractPatterns(filter);
    if (f.empty() || f == "*" || f == "*.*") return true;

    std::string ext;
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) ext = name.substr(dot);

    size_t start = 0;
    while (start < f.size()) {
        size_t sep = f.find(';', start);
        std::string pat = (sep == std::string::npos)
                              ? f.substr(start) : f.substr(start, sep - start);
        pat = Trim(pat);
        if (pat == "*" || pat == "*.*") return true;
        if (!pat.empty() && pat[0] == '*') {
            std::string suffix = pat.substr(1);
            if (suffix.empty()) return true;
            if (ext.size() >= suffix.size() &&
                IEquals(ext.substr(ext.size() - suffix.size()), suffix)) {
                return true;
            }
        } else if (IEquals(name, pat)) {
            return true;
        }
        start = (sep == std::string::npos) ? f.size() : sep + 1;
    }
    return false;
}

} // namespace

ClientFileDialogApp::~ClientFileDialogApp() = default;

void ClientFileDialogApp::OnInit() {
    auto main = std::make_unique<FileDlgRoot>("파일 열기");
    main->SetWindowRect(JKRect{ 0, 0, 560, 400 });
    main->SetAttrFlags(WA_TITLEMOVEABLE);  // normal chrome, non-resizable —
                                           // the server close X resolves us too
    SetMainWindow(std::move(main));

    SetTimerInterval(16); // ~60 Hz frame cadence (palette/snap idiom)

    ImGui::CreateContext();
    jk::theme::ApplyImGuiTheme(); // JKTheme 팔레트 봉합 (P2 단계 3)
    ImGui::GetIO().IniFilename = nullptr;
    // Korean UI (열기/취소/새로고침/오버레이) — Malgun Gothic like the
    // notify/snap/shot apps; failure degrades to the default font.
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f,
                                 nullptr,
                                 io.Fonts->GetGlyphRangesKorean());

    // Defaults until the params reply lands: process cwd + everything. The
    // params reply (below) overrides both when the server slot answers.
    try {
        currentDir_ = fs::current_path().string();
    } catch (const std::exception&) {
        currentDir_ = "C:\\";
    }
    filterAll_ = "*.*";
    filterActive_ = filterAll_;
    BuildFilterChoices();  // seed the combo before the params reply lands
    SyncDirBuffer();
    SyncFilterBuffer();
    RefreshList();

    lastFrame_ = std::chrono::steady_clock::now();
}

void ClientFileDialogApp::OnClose() {
    if (imguiReady_) {
        ImGui_ImplJKWindow_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
}

bool ClientFileDialogApp::PreProcessMessage(const JKEvent& ev) {
    // Every event goes to the backend before the next NewFrame consumes the
    // input queue (docs/23 §5.2-3). The 16 ms timer is the frame clock —
    // re-arm the frame gate here (palette idiom).
    ImGui_ImplJKWindow_ProcessJKEvent(ev);
    if (ev.type == JKEventType::Timer) {
        frameDirty_ = true;
    }
    return true;
}

void ClientFileDialogApp::OnFrameCommitted() {
    frameDirty_ = false;
}

void ClientFileDialogApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
    if (!imguiReady_) {
        if (!ImGui_ImplJKWindow_Init(renderer))
            return;
        imguiReady_ = true;
    }

    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - lastFrame_).count();
    lastFrame_ = now;

    RequestParams();
    PumpReplies();

    ImGui_ImplJKWindow_NewFrame(dt, w, h);
    ImGui::NewFrame();

    BuildUi(w, h);

    ImGui::Render();
    ImGui_ImplJKWindow_RenderDrawData(ImGui::GetDrawData(), renderer);
}

void ClientFileDialogApp::BuildUi(int w, int h) {
    // ESC cancels (snap precedent).
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        Finish(false, {});
        return;
    }
    const bool enter = (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                        ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) &&
                       // A combo popup swallows Enter to pick the highlighted
                       // item — that pick must not also fire OnOk.
                       !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);

    // Sit inside the client area the root chrome leaves us (kBorder=2,
    // kTitle=24 — the server eats clicks in the title strip anyway, vplayer
    // lesson 8), not over the whole surface like the chromeless apps do.
    JKRect client{ 0, 0, w, h };
    if (JKWindow* root = GetMainWindow()) {
        client = root->GetClientRect();
    }
    ImGui::SetNextWindowPos(ImVec2((float)client.x, (float)client.y));
    ImGui::SetNextWindowSize(ImVec2((float)client.w, (float)client.h));
    if (!ImGui::Begin("filedlg", nullptr,
                      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoSavedSettings |
                          ImGuiWindowFlags_NoBringToFrontOnFocus)) {
        ImGui::End();
        return;
    }

    // (1) up + current-dir edit (Enter = navigate) + refresh.
    if (ImGui::Button("↑")) {
        NavigateUp();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90.0f);
    const bool dirEnter = ImGui::InputText("##dir", dirBuf_, sizeof(dirBuf_),
                                           ImGuiInputTextFlags_EnterReturnsTrue);
    if (dirEnter) {
        NavigateTo(dirBuf_);
    }
    ImGui::SameLine();
    if (ImGui::Button("새로고침")) {
        RefreshList();
    }

    // (2) entries list. The bottom rows (filter+name, buttons) reserve two
    // frame heights; the list takes the rest.
    const float bottomH = ImGui::GetFrameHeightWithSpacing() * 2.0f +
                          ImGui::GetStyle().ItemSpacing.y;
    ImGui::BeginChild("entries", ImVec2(0.0f, -bottomH),
                      ImGuiChildFlags_Borders);
    if (!error_.empty()) {
        // 의도적 잔존 — 의미색 (P2 테마 스왑 제외): 접근 거부/부재 디렉터리
        // 오버레이는 성공/실패를 말하는 의미색이라 테마 토큰화 대상이 아니다.
        ImGui::TextColored(ImVec4(0.94f, 0.30f, 0.28f, 1.0f), "%s",
                           error_.c_str());
    }
    // List clipper: large directories (thousands of files) must not submit
    // every Selectable every frame — frame time collapses and the list could
    // not be scrolled to the end. The double-click action is DEFERRED to
    // after the loop: its handlers (NavigateTo/OnOk) refresh entries_, and
    // acting mid-clipper would keep indexing the clipper's cached
    // DisplayEnd past the shrunken vector (the plain loop re-read
    // entries_.size() every iteration and never had this hazard).
    int openIdx = -1;
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(entries_.size()));
    while (clipper.Step()) {
        // 클리퍼가 이번 패스에 실제로 그린 행수 — navigate pgup/pgdn의
        // 페이지 크기 캐시(스펙 §3.1). 도구 핸들러는 코어 Run 스윕(NewFrame
        // 밖)에서 불리므로 ImGui 실측 대신 이 캐시를 읽는다.
        visibleRows_ = std::max(1, clipper.DisplayEnd - clipper.DisplayStart);
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const Entry& e = entries_[static_cast<size_t>(i)];
            const std::string label =
                e.isDir ? ("[D] " + e.name) : ("     " + e.name);
            if (ImGui::Selectable(label.c_str(), selectedIdx_ == i)) {
                selectedIdx_ = i;
                // Legacy JKFileDialog OnSelect: a single click (folder
                // included) mirrors the name into the file box; 열기/Enter on
                // a folder then descends (OnOk).
                SetFileName(e.name);
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                openIdx = i;
            }
            // 음성 내비게이션 스크롤 인뷰 (스펙 §3.1): navigate가 옮긴 선택을
            // 다음 프레임 1회 가운데로 — 폴더 더블클릭 유사 위치.
            if (i == selectedIdx_ && scrollToSelection_) {
                ImGui::SetScrollHereY(0.5f);
                scrollToSelection_ = false;
            }
        }
    }
    if (openIdx >= 0 && openIdx < static_cast<int>(entries_.size())) {
        const Entry e = entries_[static_cast<size_t>(openIdx)];  // copy — the
        // handlers below refresh entries_, invalidating the reference.
        if (e.isDir) {
            if (e.name == "..") {
                NavigateUp();
            } else {
                NavigateTo((fs::path(currentDir_) / e.name).string());
            }
        } else {
            SetFileName(e.name);
            OnOk();  // double-click file = open
        }
    }
    ImGui::EndChild();

    // (3) filter combo + file-name edit (Enter = OK).
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::BeginCombo("##filter", filterBuf_)) {
        for (const FilterChoice& choice : filterChoices_) {
            if (ImGui::Selectable(choice.label.c_str(),
                                  choice.pattern == filterActive_)) {
                filterActive_ = choice.pattern;  // pattern, never the label —
                                                 // the 모든 파일 label is not a
                                                 // matchable pattern (MAJOR fix)
                SyncFilterBuffer();
                RefreshList();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (focusFileName_) {
        ImGui::SetKeyboardFocusHere();
        focusFileName_ = false;
    }
    const bool fileEnter =
        ImGui::InputText("##file", fileBuf_, sizeof(fileBuf_),
                         ImGuiInputTextFlags_EnterReturnsTrue);
    if (fileEnter) {
        OnOk();  // legacy RespondMessage: Enter on the edit = OK
    } else if (enter && !dirEnter) {
        // Enter outside a text field — the legacy dialog answers Enter with
        // OK wherever the focus sits (RespondMessage 계승).
        OnOk();
    }

    // (4) [열기] [취소], right-aligned.
    const float okW = 90.0f;
    ImGui::SetCursorPosX(
        ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x -
        okW * 2.0f - ImGui::GetStyle().ItemSpacing.x);
    if (ImGui::Button("열기", ImVec2(okW, 0.0f))) {
        OnOk();
    }
    ImGui::SameLine();
    if (ImGui::Button("취소", ImVec2(okW, 0.0f))) {
        Finish(false, {});
    }

    ImGui::End();
}

void ClientFileDialogApp::RefreshList() {
    entries_.clear();
    error_.clear();
    selectedIdx_ = -1;

    std::string dir = Trim(currentDir_);
    if (dir.empty()) {
        currentDir_ = ".";
        SyncDirBuffer();
        dir = ".";
    }

    std::error_code ec;
    const fs::path dirPath(dir);
    if (!fs::exists(dirPath, ec) || !fs::is_directory(dirPath, ec)) {
        error_ = "디렉터리를 열 수 없습니다: " + dir;
        return;
    }

    // Legacy enumeration (JKFileDialog:150): skip_permission_denied keeps
    // unreadable siblings visible; a refused open or a refused advance
    // surfaces as the error overlay (non-throwing forms throughout).
    std::vector<Entry> found;
    fs::directory_iterator it(dirPath,
                              fs::directory_options::skip_permission_denied,
                              ec);
    if (ec) {
        error_ = "디렉터리 접근이 거부되었습니다: " + dir;
        return;
    }
    for (fs::directory_iterator end; it != end; it.increment(ec)) {
        std::error_code entryEc;
        const bool isDir = it->is_directory(entryEc);
        std::string name = it->path().filename().string();
        if (name.empty()) continue;
        if (isDir || MatchFilter(name, filterActive_)) {
            found.push_back(Entry{ std::move(name), isDir });
        }
    }
    // increment(ec) sets the iterator to end on error ([fs.class.directory
    // .iterator]), so the loop condition exits before an in-body check could
    // run — the error is observable only after the loop (MAJOR-fix round 1,
    // MINOR A): a mid-scan permission failure surfaces as the overlay.
    if (ec) {
        error_ = "디렉터리 접근이 거부되었습니다: " + dir;
    }

    // Legacy sort (JKFileDialog:153): dirs first, then by name.
    std::sort(found.begin(), found.end(),
              [](const Entry& a, const Entry& b) {
                  if (a.isDir != b.isDir) return a.isDir;
                  return a.name < b.name;
              });
    entries_ = std::move(found);

    // Legacy ".." row (JKFileDialog:158): only when a real parent exists.
    const fs::path parent = dirPath.parent_path();
    if (!parent.empty() && parent != dirPath) {
        entries_.insert(entries_.begin(), Entry{ "..", true });
    }
}

void ClientFileDialogApp::NavigateTo(const std::string& path) {
    const std::string target = Trim(path);
    if (target.empty()) return;
    std::error_code ec;
    const fs::path p = fs::path(target).lexically_normal();
    if (!fs::exists(p, ec) || !fs::is_directory(p, ec)) {
        error_ = "디렉터리를 열 수 없습니다: " + target;
        return;
    }
    currentDir_ = p.string();
    SyncDirBuffer();
    SetFileName("");  // legacy: descending clears the file edit
    RefreshList();
}

void ClientFileDialogApp::NavigateUp() {
    const fs::path dir(currentDir_);
    if (dir.has_parent_path() && dir.parent_path() != dir) {
        NavigateTo(dir.parent_path().string());
    }
}

void ClientFileDialogApp::OnOk() {
    std::string name = Trim(fileBuf_);
    if (name.empty() && selectedIdx_ >= 0 &&
        selectedIdx_ < static_cast<int>(entries_.size())) {
        name = entries_[static_cast<size_t>(selectedIdx_)].name;
    }
    if (name.empty()) return;
    if (name == "..") {
        NavigateUp();
        return;
    }
    std::error_code ec;
    const fs::path selected = fs::path(currentDir_) / name;
    if (fs::is_directory(selected, ec)) {
        NavigateTo(selected.string());  // legacy OnOk: folder name descends
        return;
    }
    Finish(true, selected.string());
}

void ClientFileDialogApp::Finish(bool ok, const std::string& path) {
    SendResult(ok, path);
    // Stop the run loop; Close() (destructor) tears down ImGui/surface. This
    // is the RequestQuit idiom (ClientJangoApp exit button) — the frame in
    // flight finishes first, mid-frame Close is avoided.
    RequestQuit();
}

void ClientFileDialogApp::SendResult(bool ok, const std::string& path) {
    if (resultSent_) return;  // exactly-once guard across all exit paths
    resultSent_ = true;
    jk::client::JKClientSurface* surface = Surface();
    if (!surface || !surface->IsConnected()) {
        // Chrome-Quit after a dead pipe, or a spawn that never connected: the
        // parked query expires (600 s scan) and reclaims the slot — harmless
        // by ruling (progress.md Ruling 1).
        return;
    }
    std::string json = "{\"tool\":\"file_open_result\",\"args\":{\"ok\":";
    json += ok ? "1" : "0";
    // Sender correlation (final-review MAJOR-1): echo the requesterConnId
    // from the params reply — the server resolves the parked query only when
    // this matches its slot, so a stale/orphan dialog cannot deliver another
    // request's result.
    json += ",\"requesterConnId\":" + std::to_string(requesterConnId_);
    if (ok && !path.empty()) {
        json += ",\"path\":\"" + EscapeJson(path) + "\"";
    }
    json += "}}";
    surface->SendAgentQuery(nextQueryId_++, json);
}

void ClientFileDialogApp::FlushPendingResult() {
    // Last send attempt for the chrome-Quit path: Run() exits on the read
    // thread's Quit before any UI hook can fire, so the module calls this
    // between Run() returning and destruction while the pipe is still open.
    // Failure here is harmless — the parked-query expiry scan reclaims the
    // slot (Ruling 1).
    SendResult(false, {});
}

void ClientFileDialogApp::RequestParams() {
    if (paramsRequested_) return;
    jk::client::JKClientSurface* surface = Surface();
    if (!surface || !surface->IsConnected()) return;  // retry next frame
    const uint32_t id = nextQueryId_++;
    if (!surface->SendAgentQuery(
            id, "{\"tool\":\"file_dialog_params\",\"args\":{}}")) {
        return;  // retry next frame
    }
    paramsQueryId_ = id;
    paramsRequested_ = true;
}

void ClientFileDialogApp::PumpReplies() {
    jk::client::JKClientSurface* surface = Surface();
    jk::client::AgentReply reply;
    while (surface && surface->PollAgentReply(reply)) {
        if (reply.queryId != paramsQueryId_) {
            continue;  // params is the only leg we wait on
        }
        paramsQueryId_ = 0;
        // 슬롯 소유 불변식(스펙 §0 결정 2): 와이어 ok 바이트는 "응답 도달"
        // 의미라 no_pending_dialog도 ok=1로 온다(220230e부터 HandleAgentQuery
        // 가 하드코딩 1 — `if (!reply.ok)` 게이트는 한 번도 작동하지 않았다).
        // 등록 게이트는 params JSON의 requesterConnId(진짜 슬롯 소유 마커)로
        // 판정한다 — 슬롯 없는 수기 기동은 도구 미등록(스펙 §9 체크 10).
        agent::AgentJson json(reply.json);
        int reqConn = 0;
        if (!(json.GetInt("requesterConnId", reqConn) && reqConn > 0)) {
            continue;  // no_pending_dialog → keep the defaults, no tools
        }
        if (!reply.ok) continue;
        std::string filter, start, title;
        if (json.GetStr("filter", filter) && !Trim(filter).empty()) {
            filterAll_ = Trim(filter);
            filterActive_ = filterAll_;
            BuildFilterChoices();
            SyncFilterBuffer();
        }
        if (json.GetStr("start", start) && !Trim(start).empty()) {
            NavigateTo(start);  // refreshes the list with the new filter
        } else {
            RefreshList();
        }
        // Sender correlation: remember who to echo in file_open_result.
        requesterConnId_ = static_cast<uint32_t>(reqConn);
        // 요청 타이틀 적용 (final-review MINOR-1) — params가 공백/부재면
        // 기본 "파일 열기"를 유지한다. KSSM 크롬 변환은 그리기 직전
        // (cbaa53c)이므로 여기선 UTF-8 원문만 세팅하면 된다.
        if (json.GetStr("title", title) && !Trim(title).empty()) {
            const std::string reqTitle = Trim(title);
            if (JKWindow* root = GetMainWindow()) {
                root->SetTitle(reqTitle);
            }
            // 서버 측 크롬/카탈로그도 따라가게 전파 — JKWindow::SetTitle은
            // 로컬 전용이라 전파가 없으면 서버 크롬과 도구 카탈로그 title이
            // 구 타이틀(스폰 시 메타 타이틀)에 고정된다 (docs/33,
            // ClientNotifyApp SendWindowTitle 선례).
            if (jk::client::JKClientSurface* s = Surface()) {
                s->SendWindowTitle(reqTitle);
            }
        }
        // 음성 내비게이션 도구 등록 (스펙 2026-09-19-filedlg-voice-nav §0
        // 결정 2): 등록 시점은 params 수락 직후 — 도구 가시성 == 슬롯 소유
        // 불변식을 처음부터 성립시킨다(§5 가드가 등록 직후 공백창에서
        // tool_gone 오판하지 않게). 수동 --client filedlg 기동은 params가
        // 안 오므로 도구도 미등록 — 슬롯 없는 choose는 무의미하고, 이
        // 부작용이 곧 고아 가드 검증(스펙 §9 체크 10).
        if (!toolsRegistered_) {
            toolsRegistered_ = true;
            if (jk::client::JKClientSurface* s = Surface()) {
                using Decl = jk::client::JKClientSurface::AgentToolDecl;
                const std::vector<Decl> tools = {
                    {"navigate", "Move the file dialog selection (key: up/down/pgup/pgdn/parent/select) or jump to index; select acts like Enter (folder descends, file resolves)",
                     "{\"type\":\"object\",\"properties\":{\"key\":{\"type\":\"string\",\"enum\":[\"up\",\"down\",\"pgup\",\"pgdn\",\"parent\",\"select\"]},\"index\":{\"type\":\"integer\"}}}"},
                    {"list", "Page the dialog's current directory listing (offset 0-based, limit <= 200, default 50)",
                     "{\"type\":\"object\",\"properties\":{\"offset\":{\"type\":\"integer\"},\"limit\":{\"type\":\"integer\"}}}"},
                    {"choose", "Resolve the dialog: optional entry name (folder descends, file resolves); omitted = current selection/file box",
                     "{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"}}}"},
                };
                // 모달 플래그 (스펙 §0 결정 3/§4): 이 다이얼로그는 쿼리-수명
                // 모달임을 카탈로그에 명시 — 서버 모달 가드(§5)의 표기 근거.
                s->SendAgentToolRegister("filedlg", tools, true);
            }
        }
    }
}

void ClientFileDialogApp::SetFileName(const std::string& name) {
    std::snprintf(fileBuf_, sizeof(fileBuf_), "%s", name.c_str());
}

void ClientFileDialogApp::SyncDirBuffer() {
    std::snprintf(dirBuf_, sizeof(dirBuf_), "%s", currentDir_.c_str());
}

void ClientFileDialogApp::SyncFilterBuffer() {
    std::snprintf(filterBuf_, sizeof(filterBuf_), "%s", filterActive_.c_str());
}

void ClientFileDialogApp::BuildFilterChoices() {
    filterChoices_.clear();
    // Windows-style label "동영상 (*.mp4;...)" → the parenthesized pattern
    // list (ExtractPatterns); the label text itself is not matchable.
    const std::string list = ExtractPatterns(filterAll_);
    size_t start = 0;
    while (start < list.size()) {
        size_t sep = list.find(';', start);
        std::string pat = (sep == std::string::npos)
                              ? list.substr(start)
                              : list.substr(start, sep - start);
        pat = Trim(pat);
        if (!pat.empty() &&
            std::find_if(filterChoices_.begin(), filterChoices_.end(),
                         [&](const FilterChoice& c) {
                             return c.pattern == pat;
                         }) == filterChoices_.end()) {
            filterChoices_.push_back(FilterChoice{ pat, pat });
        }
        start = (sep == std::string::npos) ? list.size() : sep + 1;
    }
    // The all-files entry: Korean display LABEL, but the pattern handed to
    // MatchFilter is "*.*" — the label itself would match nothing
    // (MAJOR-fix round 1).
    static const char* kAllLabel = "모든 파일 (*.*)";
    static const char* kAllPattern = "*.*";
    if (std::find_if(filterChoices_.begin(), filterChoices_.end(),
                     [](const FilterChoice& c) {
                         return c.pattern == kAllPattern;
                     }) == filterChoices_.end()) {
        filterChoices_.push_back(FilterChoice{ kAllLabel, kAllPattern });
    }
}

std::string ClientFileDialogApp::EscapeJson(const std::string& in) {
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


// P3 theme hot-swap (docs/52): the palette was snapshotted into ImGuiStyle
// at OnInit - re-apply it after a preset swap.
void ClientFileDialogApp::OnThemeChanged() { jk::theme::ApplyImGuiTheme(); }

// --- filedlg 음성 내비게이션 도구 (스펙 2026-09-19-filedlg-voice-nav §3) ---

// 공통 성공 필드(스펙 §3.1/§3.2, 브리프 Step 3 규약): dir/count/selected/file.
// list는 이 위에 total(=count 동일값)/offset/entries/error를 얹는다.
std::string ClientFileDialogApp::CommonFields() const {
    return "\"dir\":\"" + EscapeJson(currentDir_) +
           "\",\"count\":" + std::to_string(entries_.size()) +
           ",\"selected\":" + std::to_string(selectedIdx_) +
           ",\"file\":\"" + EscapeJson(fileBuf_) + "\"";
}

// navigate 전용 선택 엔트리 스냅샷(스펙 §3.1) — 선행 콤마 포함. 선택 없으면
// selectedName은 빈 문자열, selectedIsDir는 false(파서 단순화 — 항상 존재).
std::string ClientFileDialogApp::SelectedSnapshot() const {
    if (selectedIdx_ < 0 ||
        selectedIdx_ >= static_cast<int>(entries_.size())) {
        return ",\"selectedName\":\"\",\"selectedIsDir\":false";
    }
    const Entry& e = entries_[static_cast<size_t>(selectedIdx_)];
    return ",\"selectedName\":\"" + EscapeJson(e.name) +
           "\",\"selectedIsDir\":" + (e.isDir ? "true" : "false");
}

// navigate.select / choose 공용 — OnOk()와 동일 경로(스펙 §3.1/§3.3): 폴더면
// 하강(descended), 파일이면 해소(resolved), 대상 없으면 nothing_selected.
// 해소 순서(스펙 §3.3): out을 먼저 세팅한 뒤 Finish를 부른다 — 코어는 훅
// 반환 직후 SendAgentToolResult를 보내고(JKClientApplication.cpp :286→:287)
// 그 다음 !running_ 체크(:292)로 빠지므로, 훅 안에서 RequestQuit해도 결과
// 전송이 보장된다. (OnOk()를 직접 부르지 않는 이유: Finish가 응답 형태를
// 모르고, 응답을 먼저 만들려면 하강/해소 판정이 필요하기 때문.)
void ClientFileDialogApp::ResolveOnOk(std::string& out) {
    std::string name = Trim(fileBuf_);
    if (name.empty() && selectedIdx_ >= 0 &&
        selectedIdx_ < static_cast<int>(entries_.size())) {
        name = entries_[static_cast<size_t>(selectedIdx_)].name;
    }
    if (name.empty()) {
        out = "{\"ok\":true,\"error\":\"nothing_selected\"}";  // 스펙 §8
        return;
    }
    if (name == "..") {
        NavigateUp();  // legacy OnOk: ".."는 상위로
        out = "{\"ok\":true,\"descended\":true," + CommonFields() + "}";
        return;
    }
    std::error_code ec;
    const fs::path selected = fs::path(currentDir_) / name;
    if (fs::is_directory(selected, ec)) {
        NavigateTo(selected.string());  // legacy OnOk: 폴더명은 하강
        out = "{\"ok\":true,\"descended\":true," + CommonFields() + "}";
        return;
    }
    const std::string path = selected.string();
    // 해소 — out 선세팅 후 Finish(위 순서 주석 참조).
    out = "{\"ok\":true,\"resolved\":true,\"path\":\"" + EscapeJson(path) +
          "\"}";
    Finish(true, path);
}

bool ClientFileDialogApp::OnAgentToolCall(const std::string& tool,
                                          const std::string& argsJson,
                                          std::string& out) {
    const agent::AgentJson args(argsJson);
    const int count = static_cast<int>(entries_.size());
    // 앱 실패도 {"ok":true,"error":...} — 플래그는 와이어 헤더가 아니다
    // (스펙 §3.4, docs/58 레슨 f).
    auto err = [&out](const char* code) {
        out = std::string("{\"ok\":true,\"error\":\"") + code + "\"}";
    };
    // 이동/하강 후 현재 상태 스냅샷(스펙 §3.1 응답) — index+공통 필드+선택
    // 스냅샷. NavigateUp/NavigateTo 뒤라도 currentDir_/entries_가 이미 갱신된
    // 뒤이므로 그대로 읽으면 된다.
    auto moveResponse = [&out, this]() {
        out = "{\"ok\":true,\"index\":" + std::to_string(selectedIdx_) + "," +
              CommonFields() + SelectedSnapshot() + "}";
    };

    if (tool == "navigate") {
        int index = 0;
        std::string key;
        const bool hasIndex = args.ok() && args.GetInt("index", index);
        const bool hasKey =
            args.ok() && args.GetStr("key", key) && !Trim(key).empty();
        if (!hasIndex && !hasKey) {
            err("bad_args");  // 둘 다 없음 (스펙 §8)
            return true;
        }
        if (hasIndex) {
            // 둘 다 있으면 index가 이긴다 — 명시 인자 우선, 모호 추측 금지
            // (스펙 §3.1).
            if (index < 0 || index >= count) {
                out = "{\"ok\":true,\"error\":\"bad_index\",\"count\":" +
                      std::to_string(count) + "}";
                return true;
            }
            selectedIdx_ = index;
            // 이동 시 레거시 OnSelect 계약: SetFileName 미러(스펙 §3.1).
            SetFileName(entries_[static_cast<size_t>(index)].name);
            scrollToSelection_ = true;  // 다음 프레임 SetScrollHereY 1회
            moveResponse();
            return true;
        }
        key = Trim(key);
        if (key == "up" || key == "down" || key == "pgup" || key == "pgdn") {
            if (entries_.empty()) {
                err("empty_list");  // 빈 리스트 이동 무의미 (스펙 §3.1)
                return true;
            }
            // 페이지 크기는 BuildUi 클리퍼가 갱신한 visibleRows_ 캐시(최소 1)
            // — 핸들러는 NewFrame 밖이라 ImGui 실측 불가.
            const int step =
                (key == "pgup" || key == "pgdn") ? std::max(visibleRows_, 1) : 1;
            const int delta = (key == "down" || key == "pgdn") ? step : -step;
            // 스펙 §3.1 그대로 selectedIdx_ ±1(±페이지) 클램프 — 미선택(-1)에서
            // down하면 -1+1=0으로 첫 항목을 하이라이트한다(0 기준 재계산으로
            // index 1을 건너뛰는 오판 방지; up은 -1-1=-2 → 클램프로 0).
            const int next = std::clamp(selectedIdx_ + delta, 0, count - 1);
            selectedIdx_ = next;
            SetFileName(entries_[static_cast<size_t>(next)].name);
            scrollToSelection_ = true;
            moveResponse();
            return true;
        }
        if (key == "parent") {
            NavigateUp();
            moveResponse();
            return true;
        }
        if (key == "select") {
            ResolveOnOk(out);  // OnOk() 동등 — 하강/해소/nothing_selected
            return true;
        }
        err("bad_args");  // 스키마 밖 key 값 — 방어선
        return true;
    }

    if (tool == "list") {
        int offset = 0, limit = 50;
        args.GetInt("offset", offset);  // 부재/비정수 → 0
        if (args.GetInt("limit", limit) && limit > 200) limit = 200;
        if (limit < 1) limit = 1;  // 비양수 limit 클램프 — 빈 페이지 요청 방지
        std::string entriesJson = "[";
        if (offset >= 0 && offset < count) {
            // 음수/범위 밖 offset은 빈 배열(스펙 §3.2) — 클램프가 아니라
            // "그 위치엔 아무것도 없다"로 응답한다.
            const int end = std::min(count, offset + limit);
            bool first = true;
            for (int i = offset; i < end; ++i) {
                const Entry& e = entries_[static_cast<size_t>(i)];
                entriesJson += first ? "{" : ",{";
                first = false;
                entriesJson += "\"name\":\"" + EscapeJson(e.name) +
                               "\",\"isDir\":" + (e.isDir ? "true" : "false") +
                               "}";
            }
        }
        entriesJson += "]";
        // error_ 비어있지 않으면(접근 거부 등) 그대로 전달 — 에이전트가
        // 상황을 말로 전달할 수 있게(스펙 §3.2).
        std::string errorField;
        if (!error_.empty()) errorField = ",\"error\":\"" + EscapeJson(error_) + "\"";
        // dir는 CommonFields()가 1회 실어준다 — 중복 키 금지(같은 값을
        // 두 번 쓰면 파서에 따라 마지막 값만 남거나 중복 키 경고).
        out = std::string("{\"ok\":true,\"total\":") +
              std::to_string(count) +
              ",\"offset\":" + std::to_string(offset) +
              ",\"entries\":" + entriesJson + "," + CommonFields() +
              errorField + "}";
        return true;
    }

    if (tool == "choose") {
        std::string name;
        if (args.ok() && args.GetStr("name", name) && !Trim(name).empty()) {
            // 이름 탐색 — 대소문자 구분 없음(MatchFilter와 동일 IEquals).
            const std::string want = Trim(name);
            int found = -1;
            for (int i = 0; i < count; ++i) {
                if (IEquals(entries_[static_cast<size_t>(i)].name, want)) {
                    found = i;
                    break;
                }
            }
            if (found < 0) {
                err("no_such_entry");  // 상태 불변 (스펙 §3.3/§8)
                return true;
            }
            selectedIdx_ = found;
            SetFileName(entries_[static_cast<size_t>(found)].name);
            // fileBuf/selectedIdx_ 세팅 후 OnOk 동등 경로로 통과 — 폴더면
            // 하강, 파일이면 해소(스펙 §3.3).
        }
        ResolveOnOk(out);  // name 없으면 현재 fileBuf/선택지로 OnOk 동등
        return true;
    }

    // 도달 불가 방어선 — 서버가 app_tool 중계에서 역매칭하므로 미등록 도구명은
    // 여기 못 온다(vplayer 선례, 스펙 2026-09-19-app-tool-hub §8.2).
    out = "{\"error\":\"unknown_tool\"}";
    return false;
}

} // namespace jk
