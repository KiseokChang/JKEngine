// jkchat — Desktop Agent chat window (spec §6.2, chat MVP).
//
// Deliberately OUTSIDE the JKWindow/SDL system (user constraint — a plain
// Win32 process keeps the door open for STT later). It is a control-only
// client on the window-server pipe: the M1 JKAgentClient stack does the
// talking, so MCP/agentctl/this window are all faces of the same API.
//
// UI: read-only transcript + input + Send, plus an inline approval strip
// ([허용][거부]) that appears while an "ask"-gated request is pending.
// Slash commands mirror the palette's set; everything goes out with the
// non-blocking SendQuery (an ask-gated close parks server-side — the UI must
// stay alive to answer it) and the 400 ms ping round-trip flushes replies.
#include <agent/JKAgentClient.h>
#include <agent/JKAgentJson.h>

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// UTF-8 <-> UTF-16 (source is UTF-8; the Win32 W API wants UTF-16 — and unlike
// the ImGui stack, the default GUI font renders hangul).
// ---------------------------------------------------------------------------
static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                      static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                        &w[0], n);
    return w;
}

static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
                                      static_cast<int>(w.size()), nullptr, 0,
                                      nullptr, nullptr);
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
                        &s[0], n, nullptr, nullptr);
    return s;
}

// ---------------------------------------------------------------------------
// LLM engine (claude CLI subprocess, claude_wrapper guide §1-2).
// ---------------------------------------------------------------------------
// state/chat.json — user tunables (workbench config.json convention).
struct ChatConfig {
    std::string engine = "ollama";  // "ollama" | "claude" | "stub"
    std::string model = "kimi-k2.7-code:cloud";
    bool skipPermissions = true;    // workbench default; headless auto-denies
                                    // tool consent when off
    std::string directory =
        "I:\\progwork\\JKENGINE";   // --directory: .mcp.json (jkagentd) lives here
};

static std::string ExeDirA() {
    char path[1024] = {};
    GetModuleFileNameA(nullptr, path, sizeof(path));
    std::string dir = path;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    return dir;
}

static ChatConfig LoadChatConfig() {
    ChatConfig cfg;
    std::FILE* f =
        std::fopen((ExeDirA() + "\\state\\chat.json").c_str(), "rb");
    if (!f) return cfg;
    char buf[4096] = {};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    jk::agent::AgentJson c(buf);
    std::string v;
    if (c.ok()) {
        if (c.GetStr("engine", v)) cfg.engine = v;
        if (c.GetStr("model", v)) cfg.model = v;
        if (c.GetStr("directory", v)) cfg.directory = v;
        int skip = -1;
        if (c.GetInt("skip_permissions", skip)) {
            cfg.skipPermissions = (skip != 0);
        }
    }
    return cfg;
}

// claude_wrapper guide §2.2: ollama launch claude --model <m> -- [claude args]
static std::wstring BuildEngineCmd(const ChatConfig& cfg,
                                   const std::string& prompt,
                                   const std::string& resumeSessionId) {
    // -p argument escaping: only quotes (the rest reaches claude verbatim).
    std::string esc;
    for (char ch : prompt) {
        if (ch == '"') esc += "\\\"";
        else esc += ch;
    }
    std::string claudeArgs = "-p \"" + esc + "\" --output-format json";
    if (cfg.skipPermissions) claudeArgs += " --dangerously-skip-permissions";
    if (!resumeSessionId.empty()) {
        claudeArgs += " --resume \"" + resumeSessionId + "\"";
    }
    if (!cfg.directory.empty()) {
        claudeArgs += " --directory \"" + cfg.directory + "\"";
    }

    std::string cmd;
    if (cfg.engine == "stub") {
        // No-network machinery test: emits a valid reply JSON.
        cmd =
            "cmd.exe /c echo {\"result\":\"stub ok\",\"session_id\":\"stub-1\"}";
    } else if (cfg.engine == "claude") {
        cmd = "claude " + claudeArgs;
    } else {  // "ollama" (default)
        cmd = "ollama launch claude --model \"" + cfg.model + "\" -- " +
              claudeArgs;
    }
    return Utf8ToWide(cmd);
}

// ---------------------------------------------------------------------------
// Control ids and shared state.
// ---------------------------------------------------------------------------
static const int IDC_LOG = 103;
static const int IDC_PROMPT = 106;
static const int IDC_INPUT = 101;
static const int IDC_SEND = 102;
static const int IDC_ALLOW = 104;
static const int IDC_DENY = 105;

static const int kTimerPump = 1;
static const UINT kPumpMs = 400;

static jk::agent::JKAgentClient g_agent;
static HWND g_hMain = nullptr;
static HWND g_hLog = nullptr, g_hInput = nullptr, g_hSend = nullptr,
            g_hPrompt = nullptr, g_hAllow = nullptr, g_hDeny = nullptr;
static HFONT g_font = nullptr;
static WNDPROC g_origInputProc = nullptr;

// queryId → transcript label, so replies land next to their commands.
static std::vector<std::pair<uint32_t, std::string>> g_pendingSends;

// The approval currently on the strip (0 = none).
static uint32_t g_approvalRequest = 0;

// /close and /restore take a pre_undo snapshot first; the destructive tool
// fires only when the save reply lands (spec §9 undo, palette model).
static uint32_t g_undoSaveQueryId = 0;
static std::string g_afterUndoClose;    // target id waiting for the snapshot
static std::string g_afterUndoRestore;  // layout name waiting for the snapshot

static void Log(const std::wstring& line) {
    if (!g_hLog) return;
    // Append at the end and scroll to it.
    const int len = GetWindowTextLengthW(g_hLog);
    SendMessageW(g_hLog, EM_SETSEL, len, len);
    const std::wstring chunk = (len == 0) ? L"" : L"\r\n";
    SendMessageW(g_hLog, EM_REPLACESEL, FALSE,
                 reinterpret_cast<LPARAM>(chunk.c_str()));
    SendMessageW(g_hLog, EM_REPLACESEL, FALSE,
                 reinterpret_cast<LPARAM>(line.c_str()));
    SendMessageW(g_hLog, EM_SCROLLCARET, 0, 0);
}

static void Log(const std::string& utf8) { Log(Utf8ToWide(utf8)); }

static void ShowApproval(const std::string& title, uint32_t targetId,
                         uint32_t request) {
    g_approvalRequest = request;
    wchar_t buf[512];
    _snwprintf_s(buf, _TRUNCATE, L"[%s #%u] 창을 닫을까요?", Utf8ToWide(title).c_str(),
                 targetId);
    SetWindowTextW(g_hPrompt, buf);
    ShowWindow(g_hPrompt, SW_SHOWNORMAL);
    ShowWindow(g_hAllow, SW_SHOWNORMAL);
    ShowWindow(g_hDeny, SW_SHOWNORMAL);
}

static void HideApproval() {
    g_approvalRequest = 0;
    ShowWindow(g_hPrompt, SW_HIDE);
    ShowWindow(g_hAllow, SW_HIDE);
    ShowWindow(g_hDeny, SW_HIDE);
}

// Send one tool request (non-blocking) and remember its label so the reply
// can be attributed when the ping pump flushes it back.
static void SendTool(const std::string& tool, const std::string& args,
                     const std::string& label) {
    const uint32_t id = g_agent.SendQuery(tool, args);
    if (id == 0) {
        Log(L"[!] 서버에 연결되어 있지 않습니다");
        return;
    }
    g_pendingSends.emplace_back(id, label);
}

// --- slash commands (mirror the palette set) -------------------------------
static void Submit();  // forward: the LLM path logs before parsing

// --- LLM turn (claude headless, worker thread) ------------------------------
struct LlmTurnResult {
    bool ok = false;
    std::string result;     // the reply JSON's "result" field
    std::string sessionId;  // its "session_id" field ("" on parse failure)
};

static const UINT WM_APP_LLM_DONE = WM_APP + 1;
static std::string g_sessionId;  // claude session continuity (--resume)
static std::atomic<int> g_llmBusy{0};

static DWORD WINAPI LlmThread(LPVOID param) {
    // param = heap-allocated prompt (owned and freed here)
    std::wstring* prompt = static_cast<std::wstring*>(param);
    const ChatConfig cfg = LoadChatConfig();
    const std::wstring cmd =
        BuildEngineCmd(cfg, WideToUtf8(*prompt), g_sessionId);
    delete prompt;

    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE readEnd = nullptr, writeEnd = nullptr;
    if (!CreatePipe(&readEnd, &writeEnd, &sa, 0)) {
        g_llmBusy = 0;
        return 0;
    }
    // Our read end must NOT be inherited by the child.
    SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

    std::wstring full = L"cmd.exe /c " + cmd;
    std::vector<wchar_t> mutableCmd(full.begin(), full.end());
    mutableCmd.push_back(L'\0');
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = writeEnd;
    si.hStdError = writeEnd;
    PROCESS_INFORMATION pi{};
    const BOOL spawned = CreateProcessW(nullptr, mutableCmd.data(), nullptr,
                                        nullptr, TRUE, CREATE_NO_WINDOW,
                                        nullptr, nullptr, &si, &pi);
    CloseHandle(writeEnd);  // the child holds its end now

    LlmTurnResult* out = new LlmTurnResult;
    if (!spawned) {
        out->result = "engine spawn failed";
        PostMessageW(g_hMain, WM_APP_LLM_DONE, 0,
                     reinterpret_cast<LPARAM>(out));
        CloseHandle(readEnd);
        g_llmBusy = 0;
        return 0;
    }
    // Read stdout to EOF (cmd /c echo paths exit immediately; claude turns
    // can take minutes).
    std::string stdoutBuf;
    char chunk[4096];
    DWORD got = 0;
    while (ReadFile(readEnd, chunk, sizeof(chunk), &got, nullptr) && got > 0) {
        stdoutBuf.append(chunk, got);
    }
    CloseHandle(readEnd);
    // Turn timeout: kill a hung engine after 10 minutes (bridge convention).
    if (WaitForSingleObject(pi.hProcess, 600000) == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    jk::agent::AgentJson reply(stdoutBuf);
    out->ok = reply.ok();
    reply.GetStr("result", out->result);
    reply.GetStr("session_id", out->sessionId);
    PostMessageW(g_hMain, WM_APP_LLM_DONE, 0, reinterpret_cast<LPARAM>(out));
    g_llmBusy = 0;
    return 0;
}

static void StartLlmTurn(const std::string& prompt) {
    if (g_llmBusy.exchange(1) == 1) {
        Log(L"[!] LLM이 이미 실행 중입니다");
        return;
    }
    Log(L"… LLM 실행 중 (claude 헤드리스)");
    CloseHandle(CreateThread(nullptr, 0, LlmThread,
                             new std::wstring(Utf8ToWide(prompt)), 0, nullptr));
}

static void Submit() {
    wchar_t buf[512] = {};
    GetWindowTextW(g_hInput, buf, 512);
    SetWindowTextW(g_hInput, L"");
    const std::string line = WideToUtf8(buf);
    if (line.empty()) return;
    Log(L"> " + Utf8ToWide(line));

    if (line[0] != '/') {
        StartLlmTurn(line);  // natural language → claude headless
        return;
    }

    const size_t space = line.find(' ');
    const std::string cmd = line.substr(1, space == std::string::npos
                                             ? std::string::npos
                                             : space - 1);
    const std::string arg = (space == std::string::npos)
                                ? std::string()
                                : line.substr(space + 1);

    if (cmd == "help") {
        Log(L"자연어 입력 → LLM(claude 헤드리스) 위임 / 슬래시: 결정적 커맨드");
        Log(L"/list /launch <app> /close <id> /save <name> /restore <name> /undo /new");
    } else if (cmd == "new") {
        g_sessionId.clear();
        Log(L"새 LLM 세션");
    } else if (cmd == "list") {
        SendTool("list_windows", "{}", "list");
    } else if (cmd == "launch") {
        if (arg.empty()) { Log(L"사용법: /launch <app>"); return; }
        SendTool("launch_app", "{\"app\":\"" + arg + "\"}", "launch " + arg);
    } else if (cmd == "close") {
        if (arg.empty()) { Log(L"사용법: /close <id>"); return; }
        // Parked close needs the UI alive — non-blocking by design. The
        // pre_undo snapshot precedes it (palette state machine, 1-deep).
        g_afterUndoClose = arg;
        g_undoSaveQueryId = g_agent.SendQuery("save_layout", "{\"name\":\"pre_undo\"}");
        if (g_undoSaveQueryId == 0) {
            Log(L"[!] 서버에 연결되어 있지 않습니다");
            g_afterUndoClose.clear();
            return;
        }
        g_pendingSends.push_back({g_undoSaveQueryId, "save pre_undo"});
    } else if (cmd == "save") {
        if (arg.empty()) { Log(L"사용법: /save <name>"); return; }
        SendTool("save_layout", "{\"name\":\"" + arg + "\"}", "save " + arg);
    } else if (cmd == "restore") {
        if (arg.empty()) { Log(L"사용법: /restore <name>"); return; }
        g_afterUndoRestore = arg;
        g_undoSaveQueryId = g_agent.SendQuery("save_layout", "{\"name\":\"pre_undo\"}");
        if (g_undoSaveQueryId == 0) {
            Log(L"[!] 서버에 연결되어 있지 않습니다");
            g_afterUndoRestore.clear();
            return;
        }
        g_pendingSends.push_back({g_undoSaveQueryId, "save pre_undo"});
    } else if (cmd == "undo") {
        SendTool("restore_layout", "{\"name\":\"pre_undo\"}", "undo");
    } else {
        Log(L"알 수 없는 커맨드 — /help 참고");
    }
}

static void HandleEvent(const jk::agent::AgentEvent& ev) {
    if (ev.topic == "agent.approval_request") {
        jk::agent::AgentJson e(ev.json);
        std::string title;
        int request = 0, target = 0;
        e.GetInt("request", request);
        e.GetInt("target_id", target);
        e.GetStr("title", title);
        ShowApproval(title, static_cast<uint32_t>(target),
                     static_cast<uint32_t>(request));
        Log("[승인 요청] close_window → " + title + " (#" +
            std::to_string(target) + ")");
    } else if (ev.topic == "agent.approval_resolved") {
        jk::agent::AgentJson e(ev.json);
        std::string decision;
        int request = 0;
        e.GetInt("request", request);
        e.GetStr("decision", decision);
        if (g_approvalRequest == static_cast<uint32_t>(request)) {
            HideApproval();
            // Re-arm for the next request.
            EnableWindow(g_hAllow, TRUE);
            EnableWindow(g_hDeny, TRUE);
        }
        Log("[승인] request " + std::to_string(request) + " → " + decision);
    } else if (ev.topic.rfind("window.", 0) == 0) {
        Log("[" + ev.topic + "] " + ev.json);
    }
}

// The pump: a ping round-trip flushes parked replies into the queue, then
// drain replies and events. Blocking ping is a few ms (M1 RunAgentEvents
// pattern) — acceptable on the UI thread.
static void Pump() {
    if (!g_agent.IsConnected()) {
        if (g_agent.Connect()) {
            g_agent.SubscribeEvents(true);
            Log(L"[연결됨] 윈도우 서버");
        }
        return;
    }
    std::string pong;
    g_agent.Query("ping", "{}", pong);
    if (!g_agent.IsConnected()) {
        Log(L"[!] 윈도우 서버 연결 끊김");
        return;
    }

    std::string json;
    for (auto it = g_pendingSends.begin(); it != g_pendingSends.end();) {
        if (g_agent.PollReply(it->first, json)) {
            Log("[" + it->second + "] " + json);
            if (it->first == g_undoSaveQueryId) {
                g_undoSaveQueryId = 0;
                if (json.find("\"ok\":true") != std::string::npos) {
                    if (!g_afterUndoClose.empty()) {
                        SendTool("close_window", "{\"id\":" + g_afterUndoClose + "}",
                                 "close " + g_afterUndoClose);
                        g_afterUndoClose.clear();
                    } else if (!g_afterUndoRestore.empty()) {
                        SendTool("restore_layout",
                                 "{\"name\":\"" + g_afterUndoRestore + "\"}",
                                 "restore " + g_afterUndoRestore);
                        g_afterUndoRestore.clear();
                    }
                } else {
                    g_afterUndoClose.clear();
                    g_afterUndoRestore.clear();
                    Log(L"[!] 스냅샷 실패 — 파괴적 커맨드 취소");
                }
            }
            it = g_pendingSends.erase(it);
        } else {
            ++it;
        }
    }

    std::vector<jk::agent::AgentEvent> events;
    g_agent.PollEvents(events);
    for (const auto& ev : events) HandleEvent(ev);
}

// --- input edit subclass: Enter submits ------------------------------------
static LRESULT CALLBACK InputProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN && w == VK_RETURN) {
        Submit();
        return 0;
    }
    return CallWindowProcW(g_origInputProc, h, m, w, l);
}

static void Decision(const std::string& decision) {
    if (g_approvalRequest == 0) return;
    SendTool("approve",
             "{\"request\":" + std::to_string(g_approvalRequest) +
                 ",\"decision\":\"" + decision + "\"}",
             "approve " + decision);
    // One decision per request — the strip re-arms on approval_resolved.
    EnableWindow(g_hAllow, FALSE);
    EnableWindow(g_hDeny, FALSE);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
        case WM_CREATE: {
            if (!g_agent.Connect()) {
                // The pump timer retries until the server shows up.
                Log(L"[!] 윈도우 서버에 연결할 수 없음 — 재시도 중");
            }
            g_agent.SubscribeEvents(true);

            g_font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

            g_hLog = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                    ES_AUTOVSCROLL | ES_READONLY,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_LOG), nullptr,
                nullptr);
            g_hPrompt = CreateWindowW(L"STATIC", L"", WS_CHILD,
                                      0, 0, 0, 0, hwnd,
                                      reinterpret_cast<HMENU>(IDC_PROMPT),
                                      nullptr, nullptr);
            g_hAllow = CreateWindowW(L"BUTTON", L"허용",
                                     WS_CHILD | BS_PUSHBUTTON, 0, 0, 0, 0,
                                     hwnd,
                                     reinterpret_cast<HMENU>(IDC_ALLOW),
                                     nullptr, nullptr);
            g_hDeny = CreateWindowW(L"BUTTON", L"거부",
                                    WS_CHILD | BS_PUSHBUTTON, 0, 0, 0, 0,
                                    hwnd,
                                    reinterpret_cast<HMENU>(IDC_DENY),
                                    nullptr, nullptr);
            g_hInput = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd,
                reinterpret_cast<HMENU>(IDC_INPUT), nullptr, nullptr);
            g_hSend = CreateWindowW(L"BUTTON", L"보내기",
                                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                    0, 0, 0, 0, hwnd,
                                    reinterpret_cast<HMENU>(IDC_SEND),
                                    nullptr, nullptr);

            HWND ctrls[] = {g_hLog, g_hPrompt, g_hAllow, g_hDeny, g_hInput,
                            g_hSend};
            for (HWND c : ctrls) SendMessageW(c, WM_SETFONT,
                                              reinterpret_cast<WPARAM>(g_font),
                                              TRUE);
            g_origInputProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(g_hInput, GWLP_WNDPROC,
                                  reinterpret_cast<LONG_PTR>(InputProc)));
            HideApproval();
            SetTimer(hwnd, kTimerPump, kPumpMs, nullptr);
            Log(L"에이전트 채팅 — /help 로 커맨드 보기");
            return 0;
        }
        case WM_SIZE: {
            const int w = LOWORD(l), h = HIWORD(l);
            const int rowH = 32;       // input row
            const int apprH = 40;      // approval strip
            const int logH = h - rowH - apprH - 16;
            MoveWindow(g_hLog, 4, 4, w - 8, logH > 0 ? logH : 40, TRUE);
            MoveWindow(g_hPrompt, 8, h - rowH - apprH - 4, w - 220, 24, TRUE);
            MoveWindow(g_hAllow, w - 200, h - rowH - apprH - 8, 96, 30, TRUE);
            MoveWindow(g_hDeny, w - 100, h - rowH - apprH - 8, 96, 30, TRUE);
            MoveWindow(g_hInput, 4, h - rowH - 4, w - 108, rowH - 8, TRUE);
            MoveWindow(g_hSend, w - 100, h - rowH - 4, 96, rowH - 8, TRUE);
            return 0;
        }
        case WM_TIMER:
            Pump();
            return 0;
        case WM_APP_LLM_DONE: {
            LlmTurnResult* r = reinterpret_cast<LlmTurnResult*>(l);
            if (!r->ok || r->result.empty()) {
                Log(L"[!] LLM 응답 파싱 실패 — 엔진/모델 설정(state\\chat.json) 확인");
            } else {
                Log(Utf8ToWide(r->result));
            }
            if (!r->sessionId.empty()) g_sessionId = r->sessionId;
            delete r;
            return 0;
        }
        case WM_COMMAND: {
            switch (LOWORD(w)) {
                case IDC_SEND: Submit(); return 0;
                case IDC_ALLOW:
                    Decision("allow");
                    return 0;
                case IDC_DENY:
                    Decision("deny");
                    return 0;
                default: return 0;
            }
        }
        case WM_DESTROY:
            KillTimer(hwnd, kTimerPump);
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, w, l);
    }
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"JKAgentChat";
    RegisterClassExW(&wc);

    RECT r = {0, 0, 560, 480};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Agent Chat",
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                              r.right - r.left, r.bottom - r.top, nullptr,
                              nullptr, hInst, nullptr);
    ShowWindow(hwnd, nCmdShow);
    g_hMain = hwnd;
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}