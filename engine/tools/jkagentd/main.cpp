// jkagentd — Desktop Agent MCP broker (spec §2 sidecar).
// stdin: newline-delimited MCP JSON-RPC 2.0 → window-server pipe queries →
// stdout: JSON-RPC replies. The desktop never depends on this process dying;
// conversely it adds no UI or state of its own.
//
// --selftest runs the HandleLine router against scripted lines and returns
// the failure count as the exit code (no unit-test framework in this repo —
// same convention as jkdesktop test).

#include <agent/JKAgentJson.h>

#include <cstdio>
#include <iostream>
#include <string>

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

// Known tool names (server wiring arrives in the next tasks — Task 5 routes).
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
        // Task 6 connects these to the window server; the router shape is
        // final here.
        return result("{\"ok\":false,\"error\":\"not_connected\"}");
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