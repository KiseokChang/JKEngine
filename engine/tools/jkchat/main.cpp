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
    // Token streaming (docs/31 §6): stream-json + partial messages gives
    // line-delimited events with content_block_delta text fragments. --verbose
    // is REQUIRED by stream-json in -p mode.
    std::string claudeArgs =
        "-p \"" + esc +
        "\" --output-format stream-json --verbose --include-partial-messages";
    if (cfg.skipPermissions) claudeArgs += " --dangerously-skip-permissions";
    if (!resumeSessionId.empty()) {
        claudeArgs += " --resume \"" + resumeSessionId + "\"";
    }
    // NOTE: claude CLI has no --directory flag (guide table was wrong for
    // CLI 2.1.x — only --add-dir exists). cfg.directory is applied as the
    // worker process's current directory in LlmThread instead; session
    // history binds to cwd, so --resume needs the same dir every turn.

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

// Token streaming (docs/31 §6): a "[LLM] " transcript line is currently open
// (first delta logged, more fragments appending raw).
static bool g_streamLineOpen = false;

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

// Append without a leading newline — token-stream fragments keep typing into
// the same transcript line.
static void LogRaw(const std::wstring& chunk) {
    if (!g_hLog) return;
    const int len = GetWindowTextLengthW(g_hLog);
    SendMessageW(g_hLog, EM_SETSEL, len, len);
    SendMessageW(g_hLog, EM_REPLACESEL, FALSE,
                 reinterpret_cast<LPARAM>(chunk.c_str()));
    SendMessageW(g_hLog, EM_SCROLLCARET, 0, 0);
}

// 승인 스트립 큐 (docs/53 §9 잔여 — 단일 슬롯은 후발 요청이 선행 요청을
// 덮어썼고, 파킹된 선행 요청은 시간초과까지 보이지 않았다). 스트립 1개를
// 큐로 순환: 점유 중엔 대기, resolved되면 다음 요청이 스트립에 오른다.
struct ApprovalUi {
    uint32_t request;
    std::wstring text;
};
static std::vector<ApprovalUi> g_approvalQueue;

static void ShowApprovalText(const std::wstring& text) {
    SetWindowTextW(g_hPrompt, text.c_str());
    ShowWindow(g_hPrompt, SW_SHOWNORMAL);
    ShowWindow(g_hAllow, SW_SHOWNORMAL);
    ShowWindow(g_hDeny, SW_SHOWNORMAL);
    EnableWindow(g_hAllow, TRUE);
    EnableWindow(g_hDeny, TRUE);
}

// approval_request 수신 경유점: 스트립이 비었으면 즉시 표시, 아니면 대기 큐.
static void EnqueueApproval(uint32_t request, const std::wstring& text) {
    if (g_approvalRequest == 0) {
        g_approvalRequest = request;
        ShowApprovalText(text);
    } else {
        g_approvalQueue.push_back({request, text});
        Log(L"[대기] 승인 요청 #" + std::to_wstring(request) +
            L" — 현재 승인 처리 후 표시");
    }
}

static void ShowApproval(const std::string& title, uint32_t targetId,
                         uint32_t request) {
    wchar_t buf[512];
    _snwprintf_s(buf, _TRUNCATE, L"[%s #%u] 창을 닫을까요?", Utf8ToWide(title).c_str(),
                 targetId);
    EnqueueApproval(request, buf);
}

// Script trust prompt (docs/37 spec): same strip, different copy. The
// fingerprint shows as "sha256:"+8 hex — enough to eyeball against
// /trust output, full value in the transcript log.
static void ShowTrustApproval(const std::string& name, const std::string& origin,
                              const std::string& fingerprint, uint32_t request) {
    const std::string fp8 =
        fingerprint.size() > 15 ? fingerprint.substr(0, 15) : fingerprint;
    wchar_t buf[512];
    _snwprintf_s(buf, _TRUNCATE,
                 L"[신뢰 요청] %s (%s) 해시 %s… 승인할까요?",
                 Utf8ToWide(name).c_str(), Utf8ToWide(origin).c_str(),
                 Utf8ToWide(fp8).c_str());
    EnqueueApproval(request, buf);
}

static void HideApproval() {
    g_approvalRequest = 0;
    ShowWindow(g_hPrompt, SW_HIDE);
    ShowWindow(g_hAllow, SW_HIDE);
    ShowWindow(g_hDeny, SW_HIDE);
}

// 파일 허브 (스펙 2026-09-18-file-hub): files_access 전용 문구 — close_window
// 재용은 승인 대상을 기만적으로 표기한다(opus 리뷰 MAJOR-1). title = 경로.
static void ShowFilesApproval(const std::string& tool, const std::string& path,
                              uint32_t request) {
    wchar_t buf[512];
    _snwprintf_s(buf, _TRUNCATE, L"[파일 요청] %s → %s 승인할까요?",
                 Utf8ToWide(tool).c_str(), Utf8ToWide(path).c_str());
    EnqueueApproval(request, buf);
}

// 매니저 권한 변경 (스펙 §2.2): 동일 스트립, 다른 문구. approve 도구는
// kind 불문 공용이라 서버 변경 없이 허용/거부가 해소한다.
static void ShowPermissionApproval(const std::string& targetTool,
                                   const std::string& decision,
                                   uint32_t request) {
    wchar_t buf[512];
    _snwprintf_s(buf, _TRUNCATE, L"[권한 변경] %s → %s 승인할까요?",
                 Utf8ToWide(targetTool).c_str(),
                 Utf8ToWide(decision).c_str());
    EnqueueApproval(request, buf);
}

// 매니저 신뢰 해지 (스펙 §2.3): 지문은 15자 절단 표시(ShowTrustApproval 선례).
static void ShowTrustRevokeApproval(const std::string& name,
                                    const std::string& fingerprint,
                                    uint32_t request) {
    const std::string fp8 =
        fingerprint.size() > 15 ? fingerprint.substr(0, 15) : fingerprint;
    wchar_t buf[512];
    _snwprintf_s(buf, _TRUNCATE, L"[신뢰 해지] %s (%s…) 승인할까요?",
                 Utf8ToWide(name).c_str(), Utf8ToWide(fp8).c_str());
    EnqueueApproval(request, buf);
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
    bool streamed = false;  // ≥1 text delta reached the UI (live typing) —
                            // the done handler skips re-printing the result
    std::string result;     // the reply JSON's "result" field
    std::string sessionId;  // its "session_id" field ("" on parse failure)
};

static const UINT WM_APP_LLM_DONE = WM_APP + 1;
// Token streaming (docs/31 §6): lparam = heap std::wstring* (UTF-16, owned
// and freed by the UI thread).
static const UINT WM_APP_LLM_DELTA = WM_APP + 2;
static std::string g_sessionId;  // claude session continuity (--resume)
static std::atomic<int> g_llmBusy{0};

// One stream-json line → turn state. Text deltas go to the UI immediately
// (PostMessage, heap wstring) so the transcript types live. Lines without a
// "type" (the stub engine's plain echo-JSON) are ignored — the legacy
// whole-buffer fallback handles them after EOF.
static bool ParseStreamLine(const std::string& line, LlmTurnResult* out) {
    jk::agent::AgentJson j(line);
    std::string type;
    if (!j.ok() || !j.GetStr("type", type)) return false;
    j.GetStr("session_id", out->sessionId);  // last one wins (init/result agree)
    if (type == "stream_event") {
        // event.delta.text — three levels, so pull "delta" raw and re-parse
        // (AgentJson's object accessors are two levels deep).
        std::string deltaRaw;
        if (!j.GetObjRaw("event", "delta", deltaRaw)) return false;
        jk::agent::AgentJson delta(deltaRaw);
        std::string text;
        if (!delta.GetStr("text", text) || text.empty()) return false;
        out->streamed = true;
        PostMessageW(g_hMain, WM_APP_LLM_DELTA, 0,
                     reinterpret_cast<LPARAM>(new std::wstring(Utf8ToWide(text))));
        return true;
    }
    if (type == "result") {
        out->ok = true;
        j.GetStr("result", out->result);
    }
    return false;
}

static DWORD WINAPI LlmThread(LPVOID param) {
    // param = heap-allocated prompt (owned and freed here)
    std::wstring* prompt = static_cast<std::wstring*>(param);
    const ChatConfig cfg = LoadChatConfig();
    const std::wstring cmd =
        BuildEngineCmd(cfg, WideToUtf8(*prompt), g_sessionId);
    delete prompt;

    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    // stdout and stderr get SEPARATE pipes: claude CLI prints warnings (e.g.
    // "[claude-code:unrecognized_model] {...}") to stderr, and merging them
    // into stdout would break the reply-JSON parse.
    HANDLE readOut = nullptr, writeOut = nullptr;
    HANDLE readErr = nullptr, writeErr = nullptr;
    if (!CreatePipe(&readOut, &writeOut, &sa, 0) ||
        !CreatePipe(&readErr, &writeErr, &sa, 0)) {
        g_llmBusy = 0;
        return 0;
    }
    // Our read ends must NOT be inherited by the child.
    SetHandleInformation(readOut, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(readErr, HANDLE_FLAG_INHERIT, 0);

    std::wstring full = L"cmd.exe /c " + cmd;
    std::vector<wchar_t> mutableCmd(full.begin(), full.end());
    mutableCmd.push_back(L'\0');
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = writeOut;
    si.hStdError = writeErr;
    PROCESS_INFORMATION pi{};
    // Session history binds to cwd (claude --resume lookup); cfg.directory
    // pins it (default: repo root where .mcp.json lives).
    const std::wstring cwd = cfg.directory.empty()
                                 ? std::wstring()
                                 : Utf8ToWide(cfg.directory);
    const BOOL spawned = CreateProcessW(nullptr, mutableCmd.data(), nullptr,
                                        nullptr, TRUE, CREATE_NO_WINDOW,
                                        nullptr,
                                        cwd.empty() ? nullptr : cwd.c_str(),
                                        &si, &pi);
    CloseHandle(writeOut);  // the child holds its end now
    CloseHandle(writeErr);

    LlmTurnResult* out = new LlmTurnResult;
    if (!spawned) {
        out->result = "engine spawn failed";
        PostMessageW(g_hMain, WM_APP_LLM_DONE, 0,
                     reinterpret_cast<LPARAM>(out));
        CloseHandle(readOut);
        CloseHandle(readErr);
        g_llmBusy = 0;
        return 0;
    }
    // Read stdout to EOF (cmd /c echo paths exit immediately; claude turns
    // can take minutes). Complete lines are parsed AS THEY LAND so stream
    // deltas reach the UI while claude is still generating. stdoutBuf stays
    // intact — the line scan advances a separate offset (the stub engine's
    // plain echo-JSON needs the whole buffer in the EOF fallback below).
    std::string stdoutBuf, stderrBuf;
    size_t lineScan = 0;
    char chunk[4096];
    DWORD got = 0;
    while (ReadFile(readOut, chunk, sizeof(chunk), &got, nullptr) && got > 0) {
        stdoutBuf.append(chunk, got);
        size_t nl;
        while ((nl = stdoutBuf.find('\n', lineScan)) != std::string::npos) {
            std::string line = stdoutBuf.substr(lineScan, nl - lineScan);
            lineScan = nl + 1;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) ParseStreamLine(line, out);
        }
    }
    CloseHandle(readOut);
    while (ReadFile(readErr, chunk, sizeof(chunk), &got, nullptr) && got > 0) {
        stderrBuf.append(chunk, got);
    }
    CloseHandle(readErr);
    // Turn timeout: kill a hung engine after 10 minutes (bridge convention).
    if (WaitForSingleObject(pi.hProcess, 600000) == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (!out->ok) {
        // No stream result line (stub engine echoes plain JSON) — legacy
        // whole-buffer parse of the reply object.
        jk::agent::AgentJson reply(stdoutBuf);
        out->ok = reply.ok();
        if (!out->ok && stderrBuf.size() > 0) {
            // Surface the engine's stderr tail (parse errors are opaque
            // without it — e.g. claude warnings or cmd-level failures).
            out->result = "stderr: " +
                          stderrBuf.substr(stderrBuf.size() > 400
                                               ? stderrBuf.size() - 400
                                               : 0);
        }
        reply.GetStr("result", out->result);
        reply.GetStr("session_id", out->sessionId);
    }
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
        Log(L"/list /launch <app> /close <id> /chat /notify /shot /triggers /trust");
        Log(L"/trigger <name> on|off /events /theme dark|light|classic /save <name> /restore <name>");
        Log(L"/undo /new");
    } else if (cmd == "new") {
        g_sessionId.clear();
        Log(L"새 LLM 세션");
    } else if (cmd == "list") {
        SendTool("list_windows", "{}", "list");
    } else if (cmd == "launch") {
        if (arg.empty()) { Log(L"사용법: /launch <app>"); return; }
        SendTool("launch_app", "{\"app\":\"" + arg + "\"}", "launch " + arg);
    } else if (cmd == "theme") {
        // P3 hot-swap (docs/52): /theme dark|light|classic.
        if (arg != "dark" && arg != "light" && arg != "classic") {
            Log(L"사용법: /theme dark|light|classic");
            return;
        }
        SendTool("theme_set", "{\"preset\":\"" + arg + "\"}", "theme " + arg);
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
    } else if (cmd == "notify") {
        // Palette parity (docs/33): open/toggle the notification center.
        SendTool("open_notify", "{}", "notify");
    } else if (cmd == "shot") {
        // Palette parity (docs/35): open the screenshot viewer.
        SendTool("launch_app", "{\"app\":\"shot\"}", "shot");
    } else if (cmd == "triggers") {
        // Palette parity (docs/34): list trigger bundles + flags.
        SendTool("trigger_list", "{}", "triggers");
    } else if (cmd == "trigger") {
        // /trigger <name> on|off — toggle one trigger bundle (docs/34).
        const size_t sp2 = arg.find(' ');
        if (sp2 == std::string::npos) {
            Log(L"사용법: /trigger <name> on|off");
        } else {
            const std::string name = arg.substr(0, sp2);
            const std::string mode = arg.substr(sp2 + 1);
            if (name.empty() || name.find('"') != std::string::npos ||
                (mode != "on" && mode != "off")) {
                Log(L"사용법: /trigger <name> on|off");
            } else {
                SendTool("trigger_toggle",
                         "{\"name\":\"" + name + "\",\"on\":" +
                             (mode == "on" ? "1" : "0") + "}",
                         "trigger " + name + " " + mode);
            }
        }
    } else if (cmd == "chat") {
        // Palette parity (docs/31): spawn another chat window (inline
        // approval surface).
        SendTool("launch_chat", "{}", "chat");
    } else if (cmd == "events") {
        // Structured event catalog (docs/32).
        SendTool("events_list", "{}", "events");
    } else if (cmd == "trust") {
        // Script trust store (docs/37, palette parity).
        SendTool("trust_list", "{}", "trust");
    } else {
        Log(L"알 수 없는 커맨드 — /help 참고");
    }
}

static void HandleEvent(const jk::agent::AgentEvent& ev) {
    if (ev.topic == "agent.approval_request") {
        jk::agent::AgentJson e(ev.json);
        std::string kind;
        int request = 0;
        e.GetInt("request", request);
        e.GetStr("kind", kind);
        if (kind == "trust_request") {
            std::string name, origin, fp;
            e.GetStr("name", name);
            e.GetStr("origin", origin);
            e.GetStr("fingerprint", fp);
            ShowTrustApproval(name, origin, fp, static_cast<uint32_t>(request));
            Log("[신뢰 요청] " + name + " (" + origin + ") — 해시 " +
                (fp.size() > 15 ? fp.substr(0, 15) : fp) + "…");
        } else if (kind == "permission_set") {
            std::string targetTool, decision;
            e.GetStr("target_tool", targetTool);
            e.GetStr("decision", decision);
            ShowPermissionApproval(targetTool, decision,
                                   static_cast<uint32_t>(request));
            Log("[권한 변경] " + targetTool + " → " + decision);
        } else if (kind == "capture_allow") {
            // 설정 허브 (스펙 2026-09-18-settings-hub §2.2): 캡처 허용 스위치
            // 키별 Ask — 승인 시 캡처 도구 2종을 함께 쓴다(서버 해소).
            std::string decision;
            e.GetStr("decision", decision);
            // opus 리뷰 MINOR-5: 승인 시 capture_window+capture_region 둘 다
            // 기록되므로 라벨도 쌍으로 정직하게.
            ShowPermissionApproval("capture_window/region", decision,
                                   static_cast<uint32_t>(request));
            Log("[캡처 허용] capture_window/region → " + decision);
        } else if (kind == "trust_revoke") {
            std::string name, fp;
            e.GetStr("name", name);
            e.GetStr("fingerprint", fp);
            ShowTrustRevokeApproval(name, fp, static_cast<uint32_t>(request));
            Log("[신뢰 해지] " + name);
        } else if (kind == "files_access") {
            // 파일 허브 (스펙 2026-09-18-file-hub §2.2): 에이전트 파일 접근
            // ask 파킹 — 승인 시 서버가 원 요청을 재실행한다(deny 재검사).
            // title = 요청 경로, tool = files_list|files_read.
            std::string tool, title;
            e.GetStr("tool", tool);
            e.GetStr("title", title);
            ShowFilesApproval(tool, title, static_cast<uint32_t>(request));
            Log("[파일 요청] " + tool + " → " + title);
        } else {
            std::string title;
            int target = 0;
            e.GetInt("target_id", target);
            e.GetStr("title", title);
            ShowApproval(title, static_cast<uint32_t>(target),
                         static_cast<uint32_t>(request));
            Log("[승인 요청] close_window → " + title + " (#" +
                std::to_string(target) + ")");
        }
    } else if (ev.topic == "agent.approval_resolved") {
        jk::agent::AgentJson e(ev.json);
        std::string decision;
        int request = 0;
        e.GetInt("request", request);
        e.GetStr("decision", decision);
        if (g_approvalRequest == static_cast<uint32_t>(request)) {
            // 프론트 해소 — 큐의 다음 요청이 스트립에 오르거나 숨김.
            // approval_timeout도 decision=timeout의 approval_resolved로 온다
            // (서버 만료 스캔), 타임아웃된 프론트도 자동 순환한다.
            if (!g_approvalQueue.empty()) {
                ApprovalUi next = g_approvalQueue.front();
                g_approvalQueue.erase(g_approvalQueue.begin());
                g_approvalRequest = next.request;
                ShowApprovalText(next.text);
            } else {
                HideApproval();
            }
        } else {
            // 다른 표면(다른 채팅창)이 해소한 대기 항목 — 큐에서 제거.
            for (auto it = g_approvalQueue.begin(); it != g_approvalQueue.end();
                 ++it) {
                if (it->request == static_cast<uint32_t>(request)) {
                    g_approvalQueue.erase(it);
                    break;
                }
            }
        }
        Log("[승인] request " + std::to_string(request) + " → " + decision);
    } else if (ev.topic == "agent.notify") {
        // M2b: trigger hosts broadcast desktop.notify here (envelope
        // {"topic","data":{title,body},"ts"}). 알림 센터 앱은 이후 §7 후속.
        jk::agent::AgentJson e(ev.json);
        std::string title, body;
        e.GetObjStr("data", "title", title);
        e.GetObjStr("data", "body", body);
        Log("[알림] " + (title.empty() ? std::string("(무제)") : title) +
            (body.empty() ? "" : " — " + body));
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
        case WM_APP_LLM_DELTA: {
            // Live token (docs/31 §6): first fragment opens the "[LLM] " line,
            // the rest keep appending to it.
            std::wstring* frag = reinterpret_cast<std::wstring*>(l);
            if (!frag) return 0;
            if (!g_streamLineOpen) {
                Log(L"[LLM] ");
                g_streamLineOpen = true;
            }
            LogRaw(*frag);
            delete frag;
            return 0;
        }
        case WM_APP_LLM_DONE: {
            LlmTurnResult* r = reinterpret_cast<LlmTurnResult*>(l);
            if (g_streamLineOpen) {
                LogRaw(L"\r\n");
                g_streamLineOpen = false;
            }
            if (!r->ok || r->result.empty()) {
                Log(L"[!] LLM 응답 파싱 실패 — 엔진/모델 설정(state\\chat.json) 확인");
            } else if (r->streamed) {
                // The text already typed itself in — just close the turn out.
                Log(L"[LLM 완료]");
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