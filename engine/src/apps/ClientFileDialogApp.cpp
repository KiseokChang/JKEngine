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
        if (!reply.ok) continue;  // no_pending_dialog → keep the defaults
        agent::AgentJson json(reply.json);
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
        int connId = 0;
        if (json.GetInt("requesterConnId", connId) && connId > 0) {
            requesterConnId_ = static_cast<uint32_t>(connId);
        }
        // 요청 타이틀 적용 (final-review MINOR-1) — params가 공백/부재면
        // 기본 "파일 열기"를 유지한다. KSSM 크롬 변환은 그리기 직전
        // (cbaa53c)이므로 여기선 UTF-8 원문만 세팅하면 된다.
        if (json.GetStr("title", title) && !Trim(title).empty()) {
            if (JKWindow* root = GetMainWindow()) {
                root->SetTitle(Trim(title));
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

} // namespace jk
