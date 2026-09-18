// JKLlmEngine — headless LLM turn runner, extracted from the jkchat window
// (docs/31 §6) so jkbridge (phone web gateway) drives the identical engine.
// The UI coupling went out with the extraction: PostMessage became the
// onDelta/onDone callbacks, and the pipe-failure path that silently dropped
// the turn (no done callback, busy cleared) now reports through DoneFn —
// jkbridge holds a turn job alive across a dead session only via the done
// callback, so "exactly one DoneFn call" is a load-bearing promise.
#include <agent/JKLlmEngine.h>
#include <agent/JKAgentJson.h>

#include <windows.h>

#include <cstdio>
#include <vector>

namespace jk {
namespace agent {

namespace {

// UTF-8 <-> UTF-16 (source is UTF-8; the Win32 W API wants UTF-16). Local to
// the engine — jkchat keeps its own UI-side copies untouched.
std::wstring Utf8ToWide(const std::string& s) {
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

static std::string ExeDirA() {
    char path[1024] = {};
    GetModuleFileNameA(nullptr, path, sizeof(path));
    std::string dir = path;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    return dir;
}

} // namespace

ChatConfig LoadChatConfig() {
    ChatConfig cfg;
    std::FILE* f =
        std::fopen((ExeDirA() + "\\state\\chat.json").c_str(), "rb");
    if (!f) return cfg;
    char buf[4096] = {};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    AgentJson c(buf);
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

namespace {

// claude_wrapper guide §2.2: ollama launch claude --model <m> -- [claude args]
std::wstring BuildEngineCmd(const ChatConfig& cfg,
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
    // worker process's current directory in the turn thread instead; session
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

// The heap job: everything the worker thread needs (void* user is opaque
// here — the owner owns its lifetime). "busy" clears the busy gate at turn
// end — the CONTRACT (header) is that the engine outlives its turns, so the
// pointer stays valid for the turn's lifetime.
struct TurnJob {
    std::atomic<int>* busy = nullptr;
    std::wstring prompt;
    std::string resumeSession;  // snapshot of the engine's session id
    JKLlmEngine::DeltaFn onDelta = nullptr;
    JKLlmEngine::DoneFn onDone = nullptr;
    void* user = nullptr;
};

// One stream-json line → turn state. Text deltas go to the delta callback
// immediately so the consumer types live. Lines without a "type" (the stub
// engine's plain echo-JSON) are ignored — the legacy whole-buffer fallback
// handles them after EOF.
bool ParseStreamLine(const std::string& line, LlmTurnResult* out,
                     const TurnJob& job) {
    AgentJson j(line);
    std::string type;
    if (!j.ok() || !j.GetStr("type", type)) return false;
    j.GetStr("session_id", out->sessionId);  // last one wins (init/result agree)
    if (type == "stream_event") {
        // event.delta.text — three levels, so pull "delta" raw and re-parse
        // (AgentJson's object accessors are two levels deep).
        std::string deltaRaw;
        if (!j.GetObjRaw("event", "delta", deltaRaw)) return false;
        AgentJson delta(deltaRaw);
        std::string text;
        if (!delta.GetStr("text", text) || text.empty()) return false;
        out->streamed = true;
        if (job.onDelta) job.onDelta(text, job.user);
        return true;
    }
    if (type == "result") {
        out->ok = true;
        j.GetStr("result", out->result);
    }
    return false;
}

DWORD WINAPI LlmTurnThread(LPVOID param) {
    // param = heap-allocated job (owned and freed here)
    TurnJob* job = static_cast<TurnJob*>(param);
    const ChatConfig cfg = LoadChatConfig();
    LlmTurnResult* out = new LlmTurnResult;

    // The done path must fire exactly once and free everything — every exit
    // below funnels through Finish().
    const auto Finish = [&](bool spawnFailed) {
        if (spawnFailed) out->result = "engine spawn failed";
        job->busy->store(0);  // before onDone — a follow-up turn may start
        if (job->onDone) job->onDone(std::move(*out), job->user);
        delete out;
        delete job;
    };

    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    // stdout and stderr get SEPARATE pipes: claude CLI prints warnings (e.g.
    // "[claude-code:unrecognized_model] {...}") to stderr, and merging them
    // into stdout would break the reply-JSON parse.
    HANDLE readOut = nullptr, writeOut = nullptr;
    HANDLE readErr = nullptr, writeErr = nullptr;
    if (!CreatePipe(&readOut, &writeOut, &sa, 0) ||
        !CreatePipe(&readErr, &writeErr, &sa, 0)) {
        // Partial success must not leak the first pair (opus NIT-2).
        if (readOut) CloseHandle(readOut);
        if (writeOut) CloseHandle(writeOut);
        if (readErr) CloseHandle(readErr);
        if (writeErr) CloseHandle(writeErr);
        Finish(true);
        return 0;
    }
    // Our read ends must NOT be inherited by the child.
    SetHandleInformation(readOut, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(readErr, HANDLE_FLAG_INHERIT, 0);

    std::wstring full = L"cmd.exe /c " + BuildEngineCmd(cfg, WideToUtf8(job->prompt),
                                                        job->resumeSession);
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

    if (!spawned) {
        CloseHandle(readOut);
        CloseHandle(readErr);
        Finish(true);
        return 0;
    }
    // Read stdout to EOF (cmd /c echo paths exit immediately; claude turns
    // can take minutes). Complete lines are parsed AS THEY LAND so stream
    // deltas reach the consumer while claude is still generating. stdoutBuf
    // stays intact — the line scan advances a separate offset (the stub
    // engine's plain echo-JSON needs the whole buffer in the EOF fallback
    // below).
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
            if (!line.empty()) ParseStreamLine(line, out, *job);
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
        AgentJson reply(stdoutBuf);
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
    Finish(false);
    return 0;
}

} // namespace

bool JKLlmEngine::StartTurn(const std::string& promptUtf8,
                            const std::string& resumeSessionId, DeltaFn onDelta,
                            DoneFn onDone, void* user) {
    if (busy_.exchange(1) == 1) return false;
    TurnJob* job = new TurnJob;
    job->busy = &busy_;
    job->prompt = Utf8ToWide(promptUtf8);
    job->resumeSession = resumeSessionId;
    job->onDelta = onDelta;
    job->onDone = onDone;
    job->user = user;
    const HANDLE h = CreateThread(nullptr, 0, LlmTurnThread, job, 0, nullptr);
    if (!h) {
        // The thread is the busy flag's releaser — no thread, no turn.
        delete job;
        busy_ = 0;
        return false;
    }
    CloseHandle(h);
    return true;
}

} // namespace agent
} // namespace jk