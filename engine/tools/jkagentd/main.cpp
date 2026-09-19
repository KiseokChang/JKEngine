// jkagentd — Desktop Agent MCP broker (spec §2 sidecar).
// stdin: newline-delimited MCP JSON-RPC 2.0 → window-server pipe queries →
// stdout: JSON-RPC replies. The desktop never depends on this process dying;
// conversely it adds no UI or state of its own.
//
// --selftest runs the HandleLine router against scripted lines and returns
// the failure count as the exit code (no unit-test framework in this repo —
// same convention as jkdesktop test).

#include <agent/JKAgentClient.h>
#include <agent/JKAgentJson.h>
#include <terminal/JKConPtyBridge.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace {

// Minimal JSON string escape (mirrors the server-side helper — jkcore links
// are shared, but this tool keeps its serialization self-contained).
std::string JsonEsc(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    char num[8];
    for (char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c < 0x20)  { std::snprintf(num, sizeof(num), "\\u%04x", c); out += num; }
        else                out += ch;
    }
    return out;
}

// MCP tools/list result — the spec §3 tool surface, byte-for-byte what a
// client sees. Schemas mirror the server tool args (Tasks 3/4 + 6/7).
// 스펙 §6: 정적부(코어)만 담는다 — 동적 앱 도구부는 ComposeToolsListJson이
// tools/list 시점에 서버 질의로 합성해 이 문자열 끝에 접합한다.
const char* kCoreToolsListJson =
"{\"tools\":["
"{\"name\":\"list_windows\",\"description\":\"List desktop windows with id/title/pid/geometry/focus/minimized\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
"{\"name\":\"launch_app\",\"description\":\"Launch a built-in app (app) or a .jkx package (jkx)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"app\":{\"type\":\"string\"},\"jkx\":{\"type\":\"string\"}}}},"
"{\"name\":\"run_console_app\",\"description\":\"Spawn an installed console app (apps/<name>/manifest.json) in a terminal window (permission-gated, P4 SDK)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"}},\"required\":[\"name\"]}},"
"{\"name\":\"focus_window\",\"description\":\"Focus (and restore) a window by id\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\"}},\"required\":[\"id\"]}},"
"{\"name\":\"window_fullscreen\",\"description\":\"Toggle a window's fullscreen layer state (vplayer theater mode; id omitted = caller's own window; on=0/1, omitted=invert)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\"},\"on\":{\"type\":\"integer\",\"enum\":[0,1]}}}},"
"{\"name\":\"close_window\",\"description\":\"Close a window by id (permission-gated)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\"}},\"required\":[\"id\"]}},"
"{\"name\":\"save_layout\",\"description\":\"Snapshot current window positions to state/layout_<name>.json\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"}},\"required\":[\"name\"]}},"
"{\"name\":\"restore_layout\",\"description\":\"Restore window positions from a layout snapshot (matched by title)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"}},\"required\":[\"name\"]}},"
"{\"name\":\"read_log\",\"description\":\"Tail the jkdesktop server log (env JKDESKTOP_LOG or jkdesktop_run.log)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"lines\":{\"type\":\"integer\"},\"match\":{\"type\":\"string\"}}}},"
"{\"name\":\"read_events\",\"description\":\"Drain desktop events (window.created/destroyed/focused) received since the last call\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
"{\"name\":\"terminal_exec\",\"description\":\"Run a command in a ConPTY session and return its output (Windows only)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\"},\"timeoutSec\":{\"type\":\"integer\"}},\"required\":[\"command\"]}},"
"{\"name\":\"theme_set\",\"description\":\"Switch the desktop theme preset: dark, light, or classic (P3 hot-swap, docs/52)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"preset\":{\"type\":\"string\",\"enum\":[\"dark\",\"light\",\"classic\"]}},\"required\":[\"preset\"]}},"
"{\"name\":\"trust_list\",\"description\":\"List trusted script fingerprints from state/trust.json\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
"{\"name\":\"agent_permissions\",\"description\":\"Show the permission matrix: tool x allow/ask/deny with gate badge (server/broker/none)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
"{\"name\":\"permission_set\",\"description\":\"Change a tool permission (allow/ask/deny) - always requires approval (fixed ask gate)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"tool\":{\"type\":\"string\"},\"decision\":{\"type\":\"string\",\"enum\":[\"allow\",\"ask\",\"deny\"]}},\"required\":[\"tool\",\"decision\"]}},"
"{\"name\":\"trust_revoke\",\"description\":\"Revoke a trusted script fingerprint (approval-gated)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"fingerprint\":{\"type\":\"string\",\"pattern\":\"sha256:[0-9a-f]{64}\"}},\"required\":[\"fingerprint\"]}},"
"{\"name\":\"installed_list\",\"description\":\"List installed apps (console + .jkx) with kind\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
"{\"name\":\"read_receipts\",\"description\":\"Tail the broker receipt log (ts/tool/ok only)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"limit\":{\"type\":\"integer\"}}}},"
"{\"name\":\"settings_read\",\"description\":\"Read desktop settings (theme, triggers, idle threshold, receipt retention, audio master, layouts)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
"{\"name\":\"settings_set\",\"description\":\"Set a whitelisted desktop setting: idle_minutes, receipt_retention_days, audio_master_mute, audio_master_volume, capture_allow\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"key\":{\"type\":\"string\"},\"value\":{}}}},"
"{\"name\":\"notes_read\",\"description\":\"Read the notes hub: margin comments + backlog items (state/notes.json)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
"{\"name\":\"notes_write\",\"description\":\"Write the notes hub: op=add_note(text<=512,win)/add_item(title<=128,state)/move_item(id,state)/del(kind by id)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"op\":{\"type\":\"string\"},\"text\":{\"type\":\"string\"},\"win\":{\"type\":\"integer\"},\"state\":{\"type\":\"integer\"},\"id\":{\"type\":\"integer\"}}}},"
"{\"name\":\"files_list\",\"description\":\"List a directory (absolute drive path only): entries name/kind/size/mtime, dirs first, 512 cap. Agent calls park for approval unless permissions.json marks the tool allow\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}},"
"{\"name\":\"files_read\",\"description\":\"Read a text preview (<=64KiB, NUL sniff marks binary). Absolute path. Agent calls park for approval unless permissions.json marks allow\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"maxBytes\":{\"type\":\"integer\"}},\"required\":[\"path\"]}},"
"{\"name\":\"files_audit\",\"description\":\"Tail the broker receipts for files_* tool calls (ts/tool/ok/path) — agent file access audit\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"limit\":{\"type\":\"integer\"}}}},"
// file_open (filedlg 음성 내비게이션, 스펙 §7): wait은 브로커 주입 전용 —
// MCP 호출자에게 노출하지 않는다(재조립 분기가 무조건 "event"로 덮어쓴다).
"{\"name\":\"file_open\",\"description\":\"Open a file picker dialog (filter/start/title optional; start = initial directory). Returns parked immediately — the resolution arrives as the file.open_result desktop event (poll read_events)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"filter\":{\"type\":\"string\"},\"start\":{\"type\":\"string\"},\"title\":{\"type\":\"string\"}}}},"
"{\"name\":\"list_app_tools\",\"description\":\"List registered app tools (app/name/inputSchema/windowId rows) — the app tool hub catalog (spec 2026-09-19-app-tool-hub)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
"{\"name\":\"app_tool\",\"description\":\"Call a registered app tool directly (app, tool, args; windowId disambiguates instances). The per-app-tool 3-tier gate still applies\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"app\":{\"type\":\"string\"},\"tool\":{\"type\":\"string\"},\"args\":{},\"windowId\":{\"type\":\"integer\"}}}}"
"]}";

// Known tool names.
bool IsKnownTool(const std::string& name) {
    static const char* kNames[] = {
        "list_windows", "launch_app", "run_console_app", "focus_window",
        "window_fullscreen",
        "close_window", "save_layout", "restore_layout", "read_log",
        "read_events", "terminal_exec", "trust_list", "theme_set",
        "agent_permissions", "permission_set", "trust_revoke",
        "installed_list", "read_receipts",
        "settings_read", "settings_set",
        "notes_read", "notes_write",
        "files_list", "files_read", "files_audit",
        // 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §6): 코어 2종 — 카탈로그
        // 조회와 직접 중계. 브로커 기본 allow(키 부재=deny 레슨 재발 방지).
        "list_app_tools", "app_tool",
        // file_open (filedlg 음성 내비게이션, 스펙 §7): 분기 부재로 MCP 경로가
        // unknown_tool로 떨어지던 잠복 결함 — IsKnownTool 등록이 재조립 분기의
        // 전제다(미등록 시 tools/call이 동적 역매칭 → 즉시 unknown_tool).
        "file_open"
    };
    for (const char* n : kNames) {
        if (name == n) return true;
    }
    return false;
}

// --- window-server connection (lazy; the first server tool connects) -------
jk::agent::JKAgentClient g_agent;
bool g_agentConnected = false;

bool EnsureConnected() {
    if (g_agentConnected) return true;
    g_agentConnected = g_agent.Connect();
    return g_agentConnected;
}

// --- permissions gate (spec §5) --------------------------------------------
// permissions.json next to jkagentd.exe: {"<tool>":"allow"|"ask"|"deny"}.
// Missing file or missing key → defaults: everything allowed EXCEPT
// close_window — M1 has no approval UI, so editing the file IS the approval
// act. M2 chat: "ask" is not a deny — the server runs the inline-approval
// pipeline (chat window) and the broker's query simply blocks until the user
// (or a timeout) resolves it, so the broker passes ask through.
std::map<std::string, bool> LoadPermissions() {
    static const char* kNames[] = {
        "list_windows", "launch_app", "run_console_app", "focus_window",
        "window_fullscreen",
        "close_window", "save_layout", "restore_layout", "read_log",
        "read_events", "terminal_exec", "trust_list", "theme_set",
        "agent_permissions", "permission_set", "trust_revoke",
        "installed_list", "read_receipts",
        "settings_read", "settings_set",
        "notes_read", "notes_write",
        "files_list", "files_read", "files_audit",
        // 앱 도구 허브 코어 2종 — 기본 allow 명시 (브로커 키 부재=deny 레슨).
        // 동적 <app>_<tool>명의 게이트는 BrokerAppToolAllowed 3단 키가 별도로
        // 담당한다(아래 — 도구별 > 앱별 > 전역).
        "list_app_tools", "app_tool",
        // file_open — 서버 게이트도 none/allow(JKWindowServer kToolMatrix)라
        // 브로커 기본 allow가 정합. 다이얼로그 해소는 사람 몫이라 ask 불요.
        "file_open"
    };
    std::map<std::string, bool> perms;
    for (const char* n : kNames) perms[n] = true;
    perms["close_window"] = false;
    // P4 SDK §5: run_console_app은 broker에서도 기본 차단 — "ask"는 서버의
    // inline-approval 파이프라인으로 통과되므로 permissions.json에
    // {"run_console_app":"ask"}를 넣는 것이 승인 행위다 (close_window 동일).
    perms["run_console_app"] = false;
    // 매니저 쓰기 2종 — 브로커 기본 deny (run_console_app 선례: MCP 에이전트가
    // 직접 권한/신뢰를 바꾸려면 permissions.json 편집이 선행 승인 행위다.
    // 서버 게이트(Ask 파이프라인)는 별도로 살아 있다 — 여기선 정직한 기본값).
    perms["permission_set"] = false;
    perms["trust_revoke"] = false;

    std::string dir = ".";
#ifdef _WIN32
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
        dir = exePath;
        const size_t slash = dir.find_last_of("\\/");
        if (slash != std::string::npos) dir.resize(slash + 1);
    }
#endif
    std::FILE* f = std::fopen((dir + "permissions.json").c_str(), "rb");
    if (!f) return perms;
    char buf[4096];
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    jk::agent::AgentJson p(buf);
    if (p.ok()) {
        for (const char* name : kNames) {
            std::string v;
            if (p.GetStr(name, v)) {
                // "ask" passes through to the server's approval pipeline.
                perms[name] = (v == "allow" || v == "ask");
            }
        }
    }
    return perms;
}

// --- receipts (spec §5): append-only JSONL next to the exe -----------------
void WriteReceipt(const std::string& tool, const std::string& argsJson,
                  const std::string& resultJson) {
    std::string dir = ".";
#ifdef _WIN32
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
        dir = exePath;
        const size_t slash = dir.find_last_of("\\/");
        if (slash != std::string::npos) dir.resize(slash + 1);
    }
    dir += "state";
    CreateDirectoryA(dir.c_str(), nullptr);
    dir += "\\";
#endif
    std::FILE* f = std::fopen((dir + "receipts.jsonl").c_str(), "ab");
    if (!f) return;
    const long long ts = static_cast<long long>(std::time(nullptr)) * 1000;
    std::fprintf(f, "{\"ts\":%lld,\"tool\":\"%s\",\"args\":%s,\"result\":%s}\n",
                 ts, tool.c_str(), argsJson.c_str(), resultJson.c_str());
    std::fclose(f);
}

// --- local tools (no server roundtrip) --------------------------------------

// Tail the server log: env JKDESKTOP_LOG, else jkdesktop_run.log in the cwd
// (the run_test.sh redirection target). Reads at most the last 256 KiB.
std::string ReadLog(const jk::agent::AgentJson& req) {
    const char* envPath = std::getenv("JKDESKTOP_LOG");
    const std::string path = envPath ? envPath : "jkdesktop_run.log";
    int lines = 50;
    req.GetDeepInt("params", "arguments", "lines", lines);
    if (lines <= 0) lines = 50;
    std::string match;
    const bool filter = req.GetDeepStr("params", "arguments", "match", match);

    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return "{\"ok\":false,\"error\":\"log_file_missing\"}";
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    const long start = size > 262144 ? size - 262144 : 0;
    std::fseek(f, start, SEEK_SET);
    std::vector<char> buf(static_cast<size_t>(size - start) + 1);
    const size_t n = std::fread(buf.data(), 1, buf.size() - 1, f);
    std::fclose(f);
    buf[n] = '\0';

    std::vector<std::string> tail;
    if (!match.empty()) {
        try {
            const std::regex re(match);
            size_t pos = 0;
            while (pos < n && tail.size() < static_cast<size_t>(lines)) {
                const char* begin = buf.data() + pos;
                const char* nl = static_cast<const char*>(
                    std::memchr(begin, '\n', n - pos));
                const size_t len = nl ? static_cast<size_t>(nl - begin) : (n - pos);
                std::string lineText(begin, len);
                if (std::regex_search(lineText, re)) tail.push_back(lineText);
                pos += len + (nl ? 1 : 0);
            }
        } catch (const std::regex_error&) {
            return "{\"ok\":false,\"error\":\"bad_regex\"}";
        }
    } else {
        size_t pos = 0;
        while (pos < n && tail.size() < static_cast<size_t>(lines)) {
            const char* begin = buf.data() + pos;
            const char* nl = static_cast<const char*>(
                std::memchr(begin, '\n', n - pos));
            const size_t len = nl ? static_cast<size_t>(nl - begin) : (n - pos);
            tail.emplace_back(begin, len);
            pos += len + (nl ? 1 : 0);
        }
    }

    std::string out = "{\"ok\":true,\"lines\":[";
    for (size_t i = 0; i < tail.size(); ++i) {
        if (i) out += ",";
        out += "\"" + JsonEsc(tail[i]) + "\"";
    }
    out += "]}";
    return out;
}

// Drain desktop events queued since the last call. The pipe transport's
// Read has no timeout, so a cheap ping round-trip flushes the queue first.
std::string ReadEvents() {
    if (!EnsureConnected()) return "{\"ok\":false,\"error\":\"not_connected\"}";
    static bool subscribed = false;
    if (!subscribed) {
        g_agent.SubscribeEvents(true);
        subscribed = true;
    }
    std::string reply;
    g_agent.Query("ping", "{}", reply);
    std::vector<jk::agent::AgentEvent> evs;
    g_agent.PollEvents(evs);
    std::string out = "{\"ok\":true,\"events\":[";
    for (size_t i = 0; i < evs.size(); ++i) {
        if (i) out += ",";
        out += evs[i].json;
    }
    return out + "]}";
}

// Run a command in a ConPTY session and return its output (spec §3 execute
// tier). Windows only — POSIX builds have no ConPTY (JKConPtyBridge stub).
std::string TerminalExec(const jk::agent::AgentJson& req) {
#if defined(_WIN32)
    std::string cmd;
    if (!req.GetDeepStr("params", "arguments", "command", cmd) || cmd.empty()) {
        return "{\"ok\":false,\"error\":\"missing_command\"}";
    }
    int timeoutSec = 30;
    req.GetDeepInt("params", "arguments", "timeoutSec", timeoutSec);
    if (timeoutSec <= 0 || timeoutSec > 600) timeoutSec = 30;

    jk::JKConPtyBridge pty;
    if (!pty.Start(cmd, 80, 25)) {
        return "{\"ok\":false,\"error\":\"pty_start_failed\"}";
    }
    std::string output;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(timeoutSec);
    bool exited = false;
    while (std::chrono::steady_clock::now() < deadline) {
        std::string chunk;
        pty.DrainOutput(chunk);
        output += chunk;
        if (output.size() > (1u << 20)) {   // 1 MiB cap — same as the bridge
            output.resize(1u << 20);
            break;
        }
        // ShellExited = pipe closed (conhost); ProcessExited = the command
        // itself finished — the meaningful "ended" for one-shot commands.
        if (pty.ShellExited() || pty.ProcessExited()) {
            Sleep(50);   // let the reader thread land the last bytes
            std::string rest;
            pty.DrainOutput(rest);
            output += rest;
            exited = true;
            break;
        }
        Sleep(30);   // TerminalApp's pump period (docs/27 단계 1)
    }
    pty.Stop();
    // JsonEsc escapes quotes and control chars — ANSI sequences in pty
    // output become  escapes, valid JSON.
    return "{\"ok\":true,\"ended\":\"" + std::string(exited ? "exited" : "timeout") +
           "\",\"output\":\"" + JsonEsc(output) + "\"}";
#else
    (void)req;
    return "{\"ok\":false,\"error\":\"unsupported_platform\"}";
#endif
}

// --- app tool hub (스펙 2026-09-19-app-tool-hub §6) -------------------------

// 서버 전송 공용 — 기존 tools/call 전송 경로의 추출(코어/동적 양 경로 재사용).
// 파이프 사망 시 연결 플래그 리셋(다음 호출이 재접속 시도).
bool SendServerQuery(const std::string& tool, const std::string& argsJson,
                     std::string& resultJson) {
    if (!EnsureConnected()) {
        resultJson = "{\"ok\":false,\"error\":\"not_connected\"}";
        return false;
    }
    std::string reply;
    if (!g_agent.Query(tool, argsJson, reply)) {
        g_agentConnected = false;
        resultJson = "{\"ok\":false,\"error\":\"pipe_error\"}";
        return false;
    }
    resultJson = reply;
    return true;
}

// 등록된 (app, tool) 조합 역매칭 — 접두 추측 금지(스펙 §6). app/도구명에 _
// 가 포함될 수 있으므로 mcpName == app + "_" + tool 전 행 검사만이 정확한
// 파싱이다. 서버에 실시간 질의(앱이 방금 종료해도 정확). 서버 부재 시
// EnsureConnected가 즉시 실패 — 행블록 없이 폴백(unknown_tool).
// 반환: 0=미일치, 1=유니크, 2=복합명 충돌(서로 다른 (app,tool) 쌍이 같은
// 합성명을 낸다 — app "a"+tool "b_c" vs app "a_b"+tool "c" → a_b_c). 복합
// 충돌은 서버 §4.2 자기교정을 거울처럼 따라 임의 선택 금지 — 후보 목록을
// candidatesJson에 [{"app":..,"tool":..}...]로 채운다. 다중 인스턴스(같은
// (app,tool) 쌍의 복수 행)는 동일 조합이라 모호가 아니다.
int ResolveAppTool(const std::string& mcpName, std::string& app,
                   std::string& tool, std::string& candidatesJson) {
    if (!EnsureConnected()) return 0;
    std::string reply;
    if (!g_agent.QueryRaw("{\"tool\":\"list_app_tools\",\"args\":{}}", reply) ||
        reply.empty()) {
        g_agentConnected = false;   // 파이프 사망 — 다음 호출 재접속
        return 0;
    }
    jk::agent::AgentJson r(reply);
    int n = 0;
    if (!r.ok() || !r.GetArraySize("tools", n)) return 0;
    std::vector<std::pair<std::string, std::string>> pairs;
    for (int i = 0; i < n; ++i) {
        std::string a, t;
        if (r.GetArrStr("tools", i, "app", a) &&
            r.GetArrStr("tools", i, "name", t) && a + "_" + t == mcpName) {
            bool dup = false;
            for (const auto& p : pairs) {
                if (p.first == a && p.second == t) { dup = true; break; }
            }
            if (!dup) pairs.emplace_back(a, t);
        }
    }
    if (pairs.empty()) return 0;
    if (pairs.size() == 1) {
        app = pairs[0].first;
        tool = pairs[0].second;
        return 1;
    }
    // 복합명 충돌 — 게이트 오적용(app_tool.<app>.<tool> 키가 어긋난다)과
    // 임의 라우팅을 봉쇄한다(코디네이터 판정 1). 후보 제시로 자기교정.
    candidatesJson = "[";
    for (size_t i = 0; i < pairs.size(); ++i) {
        if (i) candidatesJson += ",";
        candidatesJson += "{\"app\":\"" + JsonEsc(pairs[i].first) +
                          "\",\"tool\":\"" + JsonEsc(pairs[i].second) + "\"}";
    }
    candidatesJson += "]";
    return 2;
}

// 브로커 3단 게이트 (스펙 §4.3) — 서버 JKWindowServer::AppToolAllowed와 동일
// 순서: app_tool.<app>.<tool> > app_tool.<app> > app_tool (도구별 > 앱별 >
// 전역). permissions.json은 점을 포함한 평면 키 리터럴 — AgentJson::GetStr은
// JS_GetPropertyStr 한 단계라 중첩 객체는 못 읽지만 점 키 프로퍼티는 읽힌다.
// 파일/키 부재 = allow 명시(브로커 키 부재=deny 레슨 재발 방지). "ask"는
// 브로커가 막지 않는다 — 서버의 ask 파킹 파이프라인으로 통과(LoadPermissions
// 의 ask pass-through 선례). permissions.json 핫리드는 호출마다.
bool BrokerAppToolAllowed(const std::string& app, const std::string& tool) {
    std::string k0 = "app_tool." + app + "." + tool;
    std::string k1 = "app_tool." + app;
    const char* keys[3] = {k0.c_str(), k1.c_str(), "app_tool"};
    std::string dir = ".";
#ifdef _WIN32
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
        dir = exePath;
        const size_t slash = dir.find_last_of("\\/");
        if (slash != std::string::npos) dir.resize(slash + 1);
    }
#endif
    std::FILE* f = std::fopen((dir + "permissions.json").c_str(), "rb");
    if (!f) return true;
    char buf[4096];
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    jk::agent::AgentJson p(buf);
    if (!p.ok()) return true;
    // 값 파싱은 서버 AppToolAllowed와 바이트 동일: 정확한 "allow"/"ask"/"deny"
    // 만 인정 — 그 외 값은 그 키를 무시하고 다음 단계로 폴백(코디네이터 판정
    // 2 — 두 게이트가 같은 파일을 같게 읽는다).
    for (const char* k : keys) {
        std::string v;
        if (!p.GetStr(k, v)) continue;
        if (v == "allow") return true;
        if (v == "ask") return true;   // 서버 파이프라인 통과
        if (v == "deny") return false;
    }
    return true;
}

// 스펙 §6: 코어 정적부 + 앱 도구 동적부. 동적부는 tools/list 시점에 서버
// list_app_tools 질의 — 앱 도구명은 <app>_<tool>(MCP 도구명 규약: 점 부재),
// description 앞에 [<app>] 접두, inputSchema는 앱 선언 그대로. 서버 질의
// 실패 시 정적부만 반환(폴백 — 앱 도구만 잠깐 안 보이는 수준).
std::string ComposeToolsListJson() {
    std::string core = kCoreToolsListJson;   // {"tools":[ ... ]} 완결 형태
    std::string dyn;
    if (EnsureConnected()) {
        std::string reply;
        if (g_agent.QueryRaw("{\"tool\":\"list_app_tools\",\"args\":{}}",
                             reply) &&
            !reply.empty()) {
            jk::agent::AgentJson r(reply);
            int n = 0;
            if (r.ok() && r.GetArraySize("tools", n)) {
                // 2-패스: 1차로 mcpName별 서로 다른 (app,tool) 쌍을 수집해
                // 복합명 충돌(a.b_c vs a_b.c → 같은 합성명)을 검출 — 스트리밍
                // 1-패스는 최초 행을 이미 방출한 뒤에 충돌을 알게 되는 결함이
                // 있다(픽스 라운드 1 회귀). 2차에서 방출: 충돌명 제외, 다중
                // 인스턴스(같은 쌍 재등장)는 이름당 1개. 충돌명은 라우팅
                // 불가라 목록에서도 제외(코디네이터 판정 1 — resolve 측
                // ambiguous_tool과 짝; 코어 app_tool 직접 호출이 변별로 남음).
                std::map<std::string, std::pair<std::string, std::string>> seen;
                std::set<std::string> ambiguous;
                for (int i = 0; i < n; ++i) {
                    std::string app, name;
                    if (!r.GetArrStr("tools", i, "app", app) ||
                        !r.GetArrStr("tools", i, "name", name)) continue;
                    const std::string mcpName = app + "_" + name;
                    const auto it = seen.find(mcpName);
                    if (it == seen.end()) {
                        seen.emplace(mcpName, std::make_pair(app, name));
                        continue;
                    }
                    if (it->second.first != app || it->second.second != name) {
                        // 임의 1개만 노출하면 스킵 이유를 알 수 없어 감람
                        // (판정 3) — 노트는 충돌당 1회.
                        if (ambiguous.insert(mcpName).second) {
                            std::fprintf(
                                stderr,
                                "tools/list: skip %s: ambiguous composite name"
                                " (first=(%s,%s) also=(%s,%s))\n",
                                mcpName.c_str(), it->second.first.c_str(),
                                it->second.second.c_str(), app.c_str(),
                                name.c_str());
                        }
                    }
                }
                std::set<std::string> emitted;   // 이름당 1행 (다중 인스턴스)
                for (int i = 0; i < n; ++i) {
                    std::string app, name, desc, schema;
                    if (!r.GetArrStr("tools", i, "app", app) ||
                        !r.GetArrStr("tools", i, "name", name)) continue;
                    const std::string mcpName = app + "_" + name;
                    if (ambiguous.count(mcpName)) continue;
                    if (!emitted.insert(mcpName).second) continue;
                    // 동적명이 코어 도구명과 충돌(app="files", tool="list" →
                    // files_list 등)하면 코어가 승리한다 — 목록 중복/라우팅
                    // 불가 허수 항목을 봉쇄(tools/call은 IsKnownTool 선검).
                    if (IsKnownTool(mcpName)) {
                        std::fprintf(stderr,
                                     "tools/list: skip %s: collides with core tool name\n",
                                     mcpName.c_str());
                        continue;
                    }
                    r.GetArrStr("tools", i, "description", desc);
                    if (!r.GetArrRaw("tools", i, "inputSchema", schema)) {
                        schema = "{}";   // 서버 부재 기본과 동일
                    }
                    // 스키마 유효성 가드 (Task 2 리뷰 이월): inputSchema를
                    // 템플릿에 원문 임베드 — 앱 하나의 파산 스키마가 tools/list
                    // 전체 응답(모든 도구)을 깨는 것을 봉쇄. 파싱 실패 행은
                    // 건너뛴다(해당 도구만 목록에서 감람).
                    jk::agent::AgentJson s(schema);
                    if (!s.ok()) {
                        std::fprintf(stderr,
                                     "tools/list: skip %s: malformed inputSchema\n",
                                     mcpName.c_str());
                        continue;
                    }
                    if (dyn.empty()) dyn = ",";
                    dyn += std::string("{\"name\":\"") + JsonEsc(mcpName) +
                           "\",\"description\":\"[" + JsonEsc(app) + "] " +
                           JsonEsc(desc) + "\",\"inputSchema\":" + schema + "}";
                }
            }
        }
    }
    if (dyn.empty()) return core;
    // core 끝 "]}"}를 열어 동적부 삽입 — 정적 문자열의 마지막 "]}") 절단 후
    // 재조립(kCoreToolsListJson은 {"tools":[...]}} 형태 유지). rfind라 정적부
    // 어디에 "]}")가 있어도 항상 꼬리를 가른다.
    const size_t tail = core.rfind("]}");
    if (tail == std::string::npos) return core;
    return core.substr(0, tail) + dyn + "]}";
}

// file_open 서버 args 재조립 (filedlg 음성 내비게이션, 스펙 §7): MCP 경로는
// wait:event를 무조건 주입한다 — 파킹 응답이 해소 때까지 안 오면 브로커가
// 600s 블록해 음성 흐름이 죽는다(스펙 §0 결정 5). 해소는 file.open_result
// 이벤트로 온다. filter/start/title은 선택 전달(서버 계약 — 빈 값은 키 생략).
// 부수 픽스: file_open은 원래 재조립 분기가 없어 MCP 경로의 filter/start/
// title이 기본 "{}" 폴백에 유실되던 잠복 결함이 있었다 — 본 분기로 치유.
// 헬퍼로 뽑은 이유: selftest가 서버 질의 없이(라이브 서버 접속·파킹 유발
// 없이) 재조립 결과를 직접 검증하게 하기 위함.
std::string BuildFileOpenArgs(const jk::agent::AgentJson& req) {
    std::string body = "{\"wait\":\"event\"";
    std::string f, s, t;
    if (req.GetDeepStr("params", "arguments", "filter", f))
        body += ",\"filter\":\"" + JsonEsc(f) + "\"";
    if (req.GetDeepStr("params", "arguments", "start", s))
        body += ",\"start\":\"" + JsonEsc(s) + "\"";
    if (req.GetDeepStr("params", "arguments", "title", t))
        body += ",\"title\":\"" + JsonEsc(t) + "\"";
    return body + "}";
}

// Process one MCP JSON-RPC line. isResponse is false for notifications
// (nothing to send back). Testability is the point: RunSelfTest feeds the
// same function scripted lines.
std::string HandleLine(const std::string& line, bool& isResponse) {
    isResponse = true;
    jk::agent::AgentJson req(line);
    if (!req.ok()) {
        return "{\"jsonrpc\":\"2.0\",\"id\":null,"
               "\"error\":{\"code\":-32700,\"message\":\"parse error\"}}";
    }
    std::string method;
    if (!req.GetStr("method", method)) {
        return "{\"jsonrpc\":\"2.0\",\"id\":null,"
               "\"error\":{\"code\":-32600,\"message\":\"invalid request\"}}";
    }
    int id = 0;
    const bool hasId = req.GetInt("id", id);
    const auto result = [&](const std::string& r) {
        return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) +
               ",\"result\":" + r + "}";
    };
    const auto err = [&](int code, const char* msg) {
        return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) +
               ",\"error\":{\"code\":" + std::to_string(code) +
               ",\"message\":\"" + msg + "\"}}";
    };

    if (method == "initialize") {
        return result("{\"protocolVersion\":\"2024-11-05\","
                      "\"capabilities\":{\"tools\":{}},"
                      "\"serverInfo\":{\"name\":\"jkagentd\",\"version\":\"0.1.0\"}}");
    }
    if (method.rfind("notifications/", 0) == 0) {
        isResponse = false;
        return "";
    }
    if (method == "tools/list") {
        // 스펙 §6: 코어 정적부 + 앱 도구 동적부 합성.
        return result(ComposeToolsListJson());
    }
    if (method == "tools/call") {
        std::string tool;
        if (!req.GetObjStr("params", "name", tool)) {
            return result("{\"ok\":false,\"error\":\"missing_tool\"}");
        }
        std::string dynApp, dynToolName, dynArgsJson;
        if (!IsKnownTool(tool)) {
            // 동적 앱 도구 라우팅 (스펙 §6): 접두 추측 아님 — 등록된
            // (app, tool) 조합 역매칭으로 파싱한다(app/도구명에 _ 포함 가능).
            std::string candidates;
            const int matched =
                ResolveAppTool(tool, dynApp, dynToolName, candidates);
            if (matched == 0) {
                return result("{\"ok\":false,\"error\":\"unknown_tool\"}");
            }
            if (matched == 2) {
                // 복합명 충돌 — 임의 선택 금지(서버 §4.2 자기교정 거울).
                // 게이트 키(app_tool.<app>.<tool>)가 어긋나는 것까지 같이
                // 봉쇄된다(코디네이터 판정 1).
                return result("{\"ok\":false,\"error\":\"ambiguous_tool\",\"candidates\":" +
                              candidates + "}");
            }
            std::string argsRaw;
            req.GetObjRaw("params", "arguments", argsRaw);
            // 서버 app_tool args: app/tool + 앱 페이로드는 중첩 args 패스스루
            // (windowId는 호출자가 직접 실어 인스턴스 변별).
            dynArgsJson = "{\"app\":\"" + JsonEsc(dynApp) + "\",\"tool\":\"" +
                          JsonEsc(dynToolName) + "\",\"args\":" +
                          (argsRaw.empty() ? "{}" : argsRaw) + "}";
        }

        // Rebuild the server args JSON from typed reads (the server speaks
        // {"tool":...,"args":{...}}, not MCP envelopes).
        std::string argsJson = "{}";
        if (tool == "launch_app") {
            std::string app, jkx;
            req.GetDeepStr("params", "arguments", "app", app);
            req.GetDeepStr("params", "arguments", "jkx", jkx);
            if (!app.empty())     argsJson = "{\"app\":\"" + JsonEsc(app) + "\"}";
            else if (!jkx.empty()) argsJson = "{\"jkx\":\"" + JsonEsc(jkx) + "\"}";
        } else if (tool == "run_console_app") {
            // P4 SDK §5: 콘솔 앱 스폰 — 서버가 ask 게이트를 담당한다.
            std::string name;
            if (req.GetDeepStr("params", "arguments", "name", name)) {
                argsJson = "{\"name\":\"" + JsonEsc(name) + "\"}";
            }
        } else if (tool == "theme_set") {
            // P3 hot-swap (docs/52): 프리셋 문자열을 서버 args로 전달.
            std::string preset;
            if (req.GetDeepStr("params", "arguments", "preset", preset)) {
                argsJson = "{\"preset\":\"" + JsonEsc(preset) + "\"}";
            }
        } else if (tool == "focus_window" || tool == "close_window") {
            int id = 0;
            if (req.GetDeepInt("params", "arguments", "id", id)) {
                argsJson = "{\"id\":" + std::to_string(id) + "}";
            }
        } else if (tool == "window_fullscreen") {
            // vplayer 전체화면(스펙 §2.2): id 생략 = 호출자 자기 창(control-only
            // 브로커라 실질 미사용 — 서버가 no_window 즉답), on 생략 = 반전.
            int id = 0;
            std::string body;
            if (req.GetDeepInt("params", "arguments", "id", id)) {
                body = "{\"id\":" + std::to_string(id);
                int on2 = -1;
                if (req.GetDeepInt("params", "arguments", "on", on2) &&
                    (on2 == 0 || on2 == 1))
                    body += ",\"on\":" + std::to_string(on2);
                body += "}";
                argsJson = body;
            } else {
                // opus 리뷰 MINOR-1: id 생략분에도 on을 실어 보낸다 — 셀프
                // 경로가 생기는 순간 반전 의도가 조용히 소실되는 것을 봉쇄
                // (서버는 현재 no_window 즉답, 포맷 정합만 보장).
                int on2 = -1;
                if (req.GetDeepInt("params", "arguments", "on", on2) &&
                    (on2 == 0 || on2 == 1))
                    argsJson = "{\"on\":" + std::to_string(on2) + "}";
                else
                    argsJson = "{}";
            }
        } else if (tool == "save_layout" || tool == "restore_layout") {
            std::string name;
            if (req.GetDeepStr("params", "arguments", "name", name)) {
                argsJson = "{\"name\":\"" + JsonEsc(name) + "\"}";
            }
        } else if (tool == "permission_set") {
            // 매니저 (스펙 §2.2): 서버 args는 {tool, decision}.
            std::string pt, dec;
            if (req.GetDeepStr("params", "arguments", "tool", pt) &&
                req.GetDeepStr("params", "arguments", "decision", dec)) {
                argsJson = "{\"tool\":\"" + JsonEsc(pt) +
                           "\",\"decision\":\"" + JsonEsc(dec) + "\"}";
            }
        } else if (tool == "trust_revoke") {
            std::string fp;
            if (req.GetDeepStr("params", "arguments", "fingerprint", fp)) {
                argsJson = "{\"fingerprint\":\"" + JsonEsc(fp) + "\"}";
            }
        } else if (tool == "read_receipts") {
            int limit = 50;
            if (req.GetDeepInt("params", "arguments", "limit", limit)) {
                argsJson = "{\"limit\":" + std::to_string(limit) + "}";
            }
        } else if (tool == "settings_set") {
            // 설정 허브 (스펙 2026-09-18-settings-hub §2.2): key + int value
            // (파서 계약 — bool은 0/1 int로 직렬화). settings_read는 인자
            // 없음 — 기본 passthrough.
            std::string k;
            if (req.GetDeepStr("params", "arguments", "key", k)) {
                int v = 0;
                if (req.GetDeepInt("params", "arguments", "value", v)) {
                    argsJson = "{\"key\":\"" + JsonEsc(k) +
                               "\",\"value\":" + std::to_string(v) + "}";
                } else {
                    argsJson = "{\"key\":\"" + JsonEsc(k) + "\"}";
                }
            }
        } else if (tool == "notes_write") {
            // 노트 허브 (스펙 2026-09-18-notes-hub §2.2): op + 필드 재조립
            // (파서 계약). notes_read는 인자 없음 — 기본 passthrough.
            std::string op;
            if (req.GetDeepStr("params", "arguments", "op", op)) {
                argsJson = "{\"op\":\"" + JsonEsc(op) + "\"";
                std::string t;
                if (req.GetDeepStr("params", "arguments", "text", t)) {
                    argsJson += ",\"text\":\"" + JsonEsc(t) + "\"";
                }
                int win = 0, state = 0, id = 0;
                if (req.GetDeepInt("params", "arguments", "win", win)) {
                    argsJson += ",\"win\":" + std::to_string(win);
                }
                if (req.GetDeepInt("params", "arguments", "state", state)) {
                    argsJson += ",\"state\":" + std::to_string(state);
                }
                if (req.GetDeepInt("params", "arguments", "id", id)) {
                    argsJson += ",\"id\":" + std::to_string(id);
                }
                argsJson += "}";
            }
        } else if (tool == "files_list" || tool == "files_read") {
            // 파일 허브 (스펙 2026-09-18-file-hub §2.2): path + maxBytes
            // 재조립 (파서 계약). files_audit은 아래 limit 분기.
            std::string path;
            if (req.GetDeepStr("params", "arguments", "path", path)) {
                argsJson = "{\"path\":\"" + JsonEsc(path) + "\"";
                int maxBytes = 0;
                if (req.GetDeepInt("params", "arguments", "maxBytes",
                                   maxBytes)) {
                    argsJson += ",\"maxBytes\":" + std::to_string(maxBytes);
                }
                argsJson += "}";
            }
        } else if (tool == "files_audit") {
            int limit = 0;
            if (req.GetDeepInt("params", "arguments", "limit", limit)) {
                argsJson = "{\"limit\":" + std::to_string(limit) + "}";
            }
        } else if (tool == "list_app_tools") {
            // 앱 도구 허브 (스펙 §6): 카탈로그 조회 — 인자 없음.
            argsJson = "{}";
        } else if (tool == "file_open") {
            argsJson = BuildFileOpenArgs(req);
        } else if (tool == "app_tool") {
            // 직접 중계 (스펙 §6): 원문 패스스루 — 서버 도구와 동일 스키마
            // (app/tool/args/windowId). 서버 측 AppToolAllowed 게이트가 그대로
            // 적용된다(중계 경로라 브로커 단 재작성 없음).
            std::string raw;
            if (req.GetObjRaw("params", "arguments", raw) && !raw.empty() &&
                raw != "{}") {
                argsJson = raw;
            }
        }

        // 동적 앱 도구 중계 (스펙 §6): 성공/앱 보고 실패 모두 원문 통과 —
        // 소비자는 error 멤버 존재 여부로 분기한다(Task 2 와이어 계약), 셰이프
        // 재작성 금지. 브로커 3단 게이트(스펙 §4.3)가 MCP 경로를 선차단한다.
        if (!dynApp.empty()) {
            std::string dynResult;
            if (BrokerAppToolAllowed(dynApp, dynToolName)) {
                SendServerQuery("app_tool", dynArgsJson, dynResult);
            } else {
                dynResult = "{\"ok\":false,\"error\":\"denied\"}";
            }
            WriteReceipt(tool, dynArgsJson, dynResult);
            return result("{\"content\":[{\"type\":\"text\",\"text\":\"" +
                          JsonEsc(dynResult) + "\"}]}");
        }

        // Permission gate (spec §5) — see LoadPermissions for the defaults.
        const std::map<std::string, bool> perms = LoadPermissions();
        const auto it = perms.find(tool);
        std::string resultJson;
        if (it != perms.end() && !it->second) {
            resultJson = "{\"ok\":false,\"error\":\"permission_denied\"}";
        } else if (tool == "read_log") {
            resultJson = ReadLog(req);
        } else if (tool == "read_events") {
            resultJson = ReadEvents();
        } else if (tool == "terminal_exec") {
            resultJson = TerminalExec(req);
        } else {
            SendServerQuery(tool, argsJson, resultJson);
        }

        WriteReceipt(tool, argsJson, resultJson);
        // MCP envelope: the tool result JSON rides as content[0].text.
        return result("{\"content\":[{\"type\":\"text\",\"text\":\"" +
                      JsonEsc(resultJson) + "\"}]}");
    }
    return err(-32601, "method not found");
}

int RunSelfTest() {
    int failures = 0;
    bool isResp = false;
    std::string r = HandleLine(
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}", isResp);
    if (!isResp || r.find("\"protocolVersion\"") == std::string::npos) ++failures;
    r = HandleLine("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}", isResp);
    if (isResp || !r.empty()) ++failures;
    r = HandleLine("{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\"}", isResp);
    if (!isResp || r.find("list_windows") == std::string::npos
                || r.find("terminal_exec") == std::string::npos) ++failures;
    r = HandleLine("{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\","
                   "\"params\":{\"name\":\"nope\",\"arguments\":{}}}", isResp);
    if (!isResp || r.find("unknown_tool") == std::string::npos) ++failures;
    r = HandleLine("not json", isResp);
    if (!isResp || r.find("-32700") == std::string::npos) ++failures;
    // 에이전트 관리자 도구 5종이 tools/list에 노출되는가 (스펙 §3).
    r = HandleLine("{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/list\"}",
                   isResp);
    if (!isResp || r.find("agent_permissions") == std::string::npos ||
        r.find("installed_list") == std::string::npos ||
        r.find("read_receipts") == std::string::npos ||
        r.find("permission_set") == std::string::npos ||
        r.find("trust_revoke") == std::string::npos) ++failures;
    // 설정 허브 도구 2종이 tools/list에 노출되는가 (스펙 2026-09-18 §2.2).
    r = HandleLine("{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"tools/list\"}",
                   isResp);
    if (!isResp || r.find("settings_read") == std::string::npos ||
        r.find("settings_set") == std::string::npos) ++failures;
    // 노트 허브 도구 2종이 tools/list에 노출되는가 (스펙 2026-09-18 §2.2).
    r = HandleLine("{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"tools/list\"}",
                   isResp);
    if (!isResp || r.find("notes_read") == std::string::npos ||
        r.find("notes_write") == std::string::npos) ++failures;
    // 파일 허브 도구 3종이 tools/list에 노출되는가 (스펙 2026-09-18-file-hub
    // §2.2) — 최초의 파일 콘텐츠 도구.
    r = HandleLine("{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"tools/list\"}",
                   isResp);
    if (!isResp || r.find("files_list") == std::string::npos ||
        r.find("files_read") == std::string::npos ||
        r.find("files_audit") == std::string::npos) ++failures;
    // 앱 도구 허브 코어 2종이 tools/list에 노출되는가 (스펙
    // 2026-09-19-app-tool-hub §6) — 브로커 키 부재=deny 레슨: 기본 allow가
    // 목록 노출의 전제.
    r = HandleLine("{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"tools/list\"}",
                   isResp);
    if (!isResp || r.find("list_app_tools") == std::string::npos ||
        r.find("app_tool") == std::string::npos) ++failures;
    // file_open (filedlg 음성 내비게이션, 스펙 §7): tools/list 정적부 노출 +
    // 알려진 도구 등록 — 서버 부재(selftest 환경)에서 not_connected 즉답이
    // 정상 경로다(unknown_tool이면 등록 누락).
    r = HandleLine("{\"jsonrpc\":\"2.0\",\"id\":11,\"method\":\"tools/list\"}",
                   isResp);
    if (!isResp || r.find("file_open") == std::string::npos ||
        r.find("file.open_result") == std::string::npos) ++failures;
    // file_open 등록 + 재조립 (스펙 §7): 서버 질의 없이 검증한다 — 라이브
    // 서버가 살아 있으면 tools/call file_open이 진짜 파킹 쿼리를 날려 셀프테스트
    // 가 600s 블록된다(실측). wait:event 주입은 BuildFileOpenArgs 직접 검증.
    if (!IsKnownTool("file_open") || IsKnownTool("file_openX")) ++failures;
    {
        const char* calls[] = {
            "{\"jsonrpc\":\"2.0\",\"id\":12,\"method\":\"tools/call\","
            "\"params\":{\"name\":\"file_open\",\"arguments\":{\"filter\":"
            "\"*.mkv\",\"start\":\"D:\\\\media\",\"title\":\"movie\"}}}",
            "{\"jsonrpc\":\"2.0\",\"id\":13,\"method\":\"tools/call\","
            "\"params\":{\"name\":\"file_open\",\"arguments\":{}}}"};
        const char* wants[] = {
            "{\"wait\":\"event\",\"filter\":\"*.mkv\","
            "\"start\":\"D:\\\\media\",\"title\":\"movie\"}",
            "{\"wait\":\"event\"}"};
        for (int i = 0; i < 2; ++i) {
            jk::agent::AgentJson cq(calls[i]);
            if (!cq.ok() || BuildFileOpenArgs(cq) != wants[i]) ++failures;
        }
    }
    // 동적명 tools/call의 폴백: 서버 부재(selftest 환경)에서 ResolveAppTool이
    // 즉시 실패 — 행블록 없이 unknown_tool 즉답.
    r = HandleLine("{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"tools/call\","
                   "\"params\":{\"name\":\"vplayer_nope\",\"arguments\":{}}}",
                   isResp);
    if (!isResp || r.find("unknown_tool") == std::string::npos) ++failures;
    std::fprintf(stderr, "selftest: %d failures\n", failures);
    return failures;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--selftest") {
        return RunSelfTest() == 0 ? 0 : 1;
    }
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        bool isResp = false;
        std::string out = HandleLine(line, isResp);
        if (!out.empty()) {
            std::fputs(out.c_str(), stdout);
            std::fputc('\n', stdout);
            std::fflush(stdout);
        }
    }
    return 0;
}