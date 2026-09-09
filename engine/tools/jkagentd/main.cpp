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

#ifdef _WIN32
#include <windows.h>
#endif

#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <map>
#include <regex>
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
const char* kToolsListJson =
"{\"tools\":["
"{\"name\":\"list_windows\",\"description\":\"List desktop windows with id/title/pid/geometry/focus/minimized\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
"{\"name\":\"launch_app\",\"description\":\"Launch a built-in app (app) or a .jkx package (jkx)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"app\":{\"type\":\"string\"},\"jkx\":{\"type\":\"string\"}}}},"
"{\"name\":\"focus_window\",\"description\":\"Focus (and restore) a window by id\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\"}},\"required\":[\"id\"]}},"
"{\"name\":\"close_window\",\"description\":\"Close a window by id (permission-gated)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\"}},\"required\":[\"id\"]}},"
"{\"name\":\"save_layout\",\"description\":\"Snapshot current window positions to state/layout_<name>.json\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"}},\"required\":[\"name\"]}},"
"{\"name\":\"restore_layout\",\"description\":\"Restore window positions from a layout snapshot (matched by title)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"}},\"required\":[\"name\"]}},"
"{\"name\":\"read_log\",\"description\":\"Tail the jkdesktop server log (env JKDESKTOP_LOG or jkdesktop_run.log)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"lines\":{\"type\":\"integer\"},\"match\":{\"type\":\"string\"}}}},"
"{\"name\":\"read_events\",\"description\":\"Drain desktop events (window.created/destroyed/focused) received since the last call\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
"{\"name\":\"terminal_exec\",\"description\":\"Run a command in a ConPTY session and return its output (Windows only)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\"},\"timeoutSec\":{\"type\":\"integer\"}},\"required\":[\"command\"]}}"
"]}";

// Known tool names.
bool IsKnownTool(const std::string& name) {
    static const char* kNames[] = {
        "list_windows", "launch_app", "focus_window", "close_window",
        "save_layout", "restore_layout", "read_log", "read_events",
        "terminal_exec"
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
// permissions.json next to jkagentd.exe: {"<tool>":"allow"|"deny"}. Missing
// file or missing key → defaults: everything allowed EXCEPT close_window —
// M1 has no approval UI, so editing the file IS the approval act.
std::map<std::string, bool> LoadPermissions() {
    static const char* kNames[] = {
        "list_windows", "launch_app", "focus_window", "close_window",
        "save_layout", "restore_layout", "read_log", "read_events",
        "terminal_exec"
    };
    std::map<std::string, bool> perms;
    for (const char* n : kNames) perms[n] = true;
    perms["close_window"] = false;

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
            if (p.GetStr(name, v)) perms[name] = (v == "allow");
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
        return result(kToolsListJson);
    }
    if (method == "tools/call") {
        std::string tool;
        if (!req.GetObjStr("params", "name", tool)) {
            return result("{\"ok\":false,\"error\":\"missing_tool\"}");
        }
        if (!IsKnownTool(tool)) {
            return result("{\"ok\":false,\"error\":\"unknown_tool\"}");
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
        } else if (tool == "focus_window" || tool == "close_window") {
            int id = 0;
            if (req.GetDeepInt("params", "arguments", "id", id)) {
                argsJson = "{\"id\":" + std::to_string(id) + "}";
            }
        } else if (tool == "save_layout" || tool == "restore_layout") {
            std::string name;
            if (req.GetDeepStr("params", "arguments", "name", name)) {
                argsJson = "{\"name\":\"" + JsonEsc(name) + "\"}";
            }
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
            // Task 7 wires ConPTY here.
            resultJson = "{\"ok\":false,\"error\":\"not_implemented\"}";
        } else if (EnsureConnected()) {
            std::string reply;
            if (g_agent.Query(tool, argsJson, reply)) {
                resultJson = reply;
            } else {
                g_agentConnected = false;
                resultJson = "{\"ok\":false,\"error\":\"pipe_error\"}";
            }
        } else {
            resultJson = "{\"ok\":false,\"error\":\"not_connected\"}";
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