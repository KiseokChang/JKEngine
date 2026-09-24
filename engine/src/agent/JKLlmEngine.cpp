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

// LLM 턴에 kill 계열 Bash deny를 고정 주입 (docs/59 유보 ①, 2026-09-20
// 재량 채택 — 사용자 "쭉쭉 재량껏"). 배경: 폰 세션의 MCP 불안정 스톰 동안
// LLM이 jkdesktop 프로세스 kill/재기동으로 에스컬레이션한 실측 — 근원 치료
// (docs/59 §10-13)이 끝났어도 재발 스톰에서 같은 에스컬레이션이 데스크탑
// 전체를 죽인다. 전면 Bash 차단(§10 ① 원안)은 진단 능력을 함께 잃으므로
// 프로세스 kill 계열만 deny한다. 실측: --dangerously-skip-permissions
// 하에서도 deny 규칙은 강제된다(permission_denials로 차단, 직접·cmd /c +
// ollama launch 경로 양쪽). 잔여 우회: powershell 래퍼 등 접두 외 경로는
// 미커버 — 재발 시 전면 Bash 차단으로 상향(사용자 판정).
// 인용: JSON 따옴표는 명령행 임베드를 위해 \" 로 이스케이프 — prompt의
// -p 이스케이프와 같은 CRT 규칙(2026-09-20 cmd.exe 경로 실측).
constexpr const char* kLlmDenySettings =
    "{\"permissions\":{\"deny\":["
    "\"Bash(taskkill:*)\",\"Bash(taskkill.exe:*)\","
    "\"Bash(Stop-Process:*)\",\"Bash(kill:*)\",\"Bash(pkill:*)\","
    "\"Bash(wmic process:*)\",\"Bash(Stop-Service:*)\","
    "\"Bash(net stop:*)\"]}}";

// 폰 채팅 턴 프리앰블 (docs/60 실전 ⑥, 2026-09-21): 모델이 최종 답변 앞에
// 사고 과정 내레이션("먼저 ~를 확인하겠습니다"류)과 마크다운 문법(코드펜스,
// 헤딩, 굵게, 인라인 백틱)을 그대로 흘려 보내는 실측 — 폰 웹 UI는 출력을
// 플레인 텍스트로 렌더링하므로 문법 문자가 그대로 노출된다. 모든 엔진 턴의
// 프롬프트 앞에 고정 지시문을 붙여 최종 답변만·플레인 텍스트로 내보내게 한다.
// stub 엔진은 프롬프트를 무시하므로 무영향. 이스케이프 루프는 따옴표만
// 건드리므로 상수에 따옴표·백슬래시를 넣지 않는다(CRT argv 재파싱 안전).
constexpr const char* kLlmTurnPreamble =
    "[시스템 지시] 아래 사용자 요청에 대해 최종 답변만 출력한다. "
    "사고 과정이나 계획, 진행 안내를 말하지 않는다. "
    "먼저 무엇을 확인하겠다는 식의 서두도 쓰지 않는다. "
    "출력은 플레인 텍스트로 전달되므로 마크다운 문법을 쓰지 않는다. "
    "코드펜스(백틱 3개), 헤딩(#), 굵게(**), 인라인 백틱 모두 금지다. "
    "도구 사용이 필요하면 조용히 실행하고 결과만 간결하게 보고한다. "
    "도구를 호출하는 동안에도 진행 안내나 중간 보고를 출력하지 않는다 "
    "(확인해볼게요, 이제 쓰겠습니다 류의 문장도 금지다). "
    "이전 턴에 상태를 바꾸는 도구를 썼다면, 다음 행동 전에 상태 읽기 도구로 "
    "최신 상태를 확인한다(이전 턴의 응답만 믿고 행동하지 않는다). "
    "사용자가 앱·게임·토이·도구를 새로 만들어 달라고 하면 워크숍 앱으로 "
    "만든다: launch_app에서 jkx를 workshop으로 지정해 띄운 뒤 app_tool의 "
    "api 도구로 함수 목록을 확인하고 set_script로 스크립트를 쓴다. "
    "[사용자] ";

// claude_wrapper guide §2.2: ollama launch claude --model <m> -- [claude args]
std::wstring BuildEngineCmd(const ChatConfig& cfg,
                            const std::string& prompt,
                            const std::string& resumeSessionId) {
    // Every turn gets the fixed Korean preamble (CoT/markdown leak guard,
    // above) prepended to the raw prompt, before quote escaping.
    const std::string fullPrompt = kLlmTurnPreamble + prompt;
    // -p argument escaping: only quotes (the rest reaches claude verbatim).
    std::string esc;
    for (char ch : fullPrompt) {
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
    // JSON 인용 이스케이프 — 원문 따옴표는 CRT argv 재파싱에서 스팬을 끊어
    // claude가 파산 JSON을 받는다(prompt의 -p 이스케이프와 동일 규칙).
    std::string escSettings;
    for (const char* p = kLlmDenySettings; *p; ++p) {
        if (*p == '"') escSettings += "\\\"";
        else escSettings += *p;
    }
    claudeArgs += " --settings \"" + escSettings + "\"";
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