// JKLlmEngine — headless LLM turn runner, extracted from the jkchat window
// (docs/31 §6) so jkbridge (phone web gateway) drives the identical engine.
// The UI coupling went out with the extraction: PostMessage became the
// onDelta/onDone callbacks, and the pipe-failure path that silently dropped
// the turn (no done callback, busy cleared) now reports through DoneFn —
// jkbridge holds a turn job alive across a dead session only via the done
// callback, so "exactly one DoneFn call" is a load-bearing promise.
#include <agent/JKLlmEngine.h>
#include <agent/JKAgentJson.h>
#include <fs/JKFs.h>
#include <process/JKProcess.h>

#include <chrono>
#include <cstdio>
#include <system_error>  // std::system_error — thread spawn failure contract
#include <thread>

// stage-1 남은 Win32 접촉은 WaitForSingleObject 1건뿐(spawn reap, :364 —
// 2단계 프로세스 마이그레이션 몫). 시간(GetTickCount64)·스레드(CreateThread)
// 접촉은 W6 표준화로 std::chrono/std::thread 치환 완료. 스폰·파이프·Job 계열은
// jk::process 어댑터(docs/68 W4)로 흡수 완료 — 이 TU는 kernel32 dllimport
// 선언 1건만 남는다(어댑터 TU가 본래의 windows.h 소유, windows.h 미 include
// 관례 따라 수기 선언). 가드 없음: 원문 windows.h 무가드 include와 동일한
// 윈도우 전용 TU 상태(stage 2에서 파일 분할).
extern "C" __declspec(dllimport) unsigned long __stdcall WaitForSingleObject(
    void* hHandle, unsigned long dwMilliseconds);

namespace jk {
namespace agent {

ChatConfig LoadChatConfig() {
    ChatConfig cfg;
    // chat.json 위치: exe-dir(뒤 "\\" 없음 — ExeDirA 원문 규약). GMFN 콜사이트는
    // jk::fs::GetExecutablePath 어댑터로 흡수(docs/68 W5) — ExeDirA 소각.
    std::string exeDir = jk::fs::GetExecutablePath();
    const size_t cut = exeDir.find_last_of("\\/");
    if (cut != std::string::npos) exeDir = exeDir.substr(0, cut);
    std::FILE* f =
        std::fopen((exeDir + "\\state\\chat.json").c_str(), "rb");
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
    "이미 만든 워크숍 앱의 작은 수정은 set_script에 live:1로 "
    "라이브 패치한다(앱이 계속 살아 있다); 위젯 추가·삭제 같은 구조 변경은 "
    "live 없이 다시 쓴다. "
    "의미 커서 앱의 커서 위치 질문(지금 어디?)과 판 상태 진술은 read 도구 "
    "결과를 근거로 답한다(이전 추론으로 말하지 않는다). "
    "act가 성공하면 커서는 그 칸으로 이동하므로 '거기'는 마지막 act 칸이다. "
    "[사용자] ";

// claude_wrapper guide §2.2: ollama launch claude --model <m> -- [claude args]
// UTF-8 in/out (docs/68 W4 흡수): 커맨드라인은 UTF-8 문자열로 어댑터 계약
// (commandLineUtf8)에 전달되고 와이딩은 어댑터가 소유 — 이 TU의 UTF-8↔UTF-16
// 헬퍼(Utf8ToWide/WideToUtf8)는 소각됐다. prompt는 UTF-8 원문(StartTurn이
// 받은 promptUtf8을 그대로 — 와일드 왕복 변환은 무손실이라 동일 관측).
std::string BuildEngineCmd(const ChatConfig& cfg,
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
        // stage-1 marking: shell literal, docs/68 W4 — stub 테스트 리터럴
        // (기계 검증용 무연결 왕복 데이터), 2단계 셸 추상 치환 대상 아님.
        cmd =
            "cmd.exe /c echo {\"result\":\"stub ok\",\"session_id\":\"stub-1\"}";
    } else if (cfg.engine == "claude") {
        cmd = "claude " + claudeArgs;
    } else {  // "ollama" (default)
        cmd = "ollama launch claude --model \"" + cfg.model + "\" -- " +
              claudeArgs;
    }
    return cmd;
}

// The heap job: everything the worker thread needs (void* user is opaque
// here — the owner owns its lifetime). "busy" clears the busy gate at turn
// end — the CONTRACT (header) is that the engine outlives its turns, so the
// pointer stays valid for the turn's lifetime.
struct TurnJob {
    std::atomic<int>* busy = nullptr;
    std::string prompt;         // UTF-8 원문 — 흡수 전 wstring 왕복 변환은 무손실이라 동일 관측
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
        // claude CLI failure paths STILL emit a type:"result" line — with
        // is_error:true / errors[] and NO "result" field (e.g. --resume of
        // an unknown session id). Treating it as ok produced silent empty
        // replies (2026-09-27 폰 빈 응답: stub 잔여 세션 id가 매 턴
        // resume 실패). Fail closed and surface the errors[0] text; clear
        // the session id — the error line's fresh UUID is not a resumable
        // conversation, and propagating it would poison the next resume.
        std::string subtype, err0;
        j.GetStr("subtype", subtype);
        j.GetArrValStr("errors", 0, err0);  // 단락 금지 — subtype 참이어도 추출
        if (subtype == "error_during_execution" || !err0.empty()) {
            out->ok = false;
            out->result = err0.empty() ? subtype : err0;
            out->sawResult = true;
            out->sessionId.clear();
            return false;
        }
        out->sawResult = true;
        out->ok = true;
        j.GetStr("result", out->result);
    }
    return false;
}

// std::thread direct form (W6 standardization) — the __stdcall/DWORD thread
// entry point shape went out with CreateThread. The return value has no
// consumer (the spawn site detaches; the legacy CreateThread discard applies
// unchanged), so the signature carries the job pointer directly instead of
// round-tripping through void*.
int LlmTurnThread(TurnJob* job) {
    // job = heap-allocated (owned and freed here)
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

    // Spawn through the jk::process adapter (docs/68 W4): the pipe pair
    // creation, STARTUPINFO, CreateProcessW, write-end handover and the
    // read-end non-inheritance live in JKProcess_win32.cpp now. Contracts
    // carried verbatim — (a) the parent keeps READ ends only (the adapter
    // closes the write ends right after spawn, so stdout EOFs) and stdout/
    // stderr stay SEPARATE pipes (merging them would break the reply-JSON
    // parse, :251-256 comment above), (b) the job handle below is
    // "close == tree death". Exactly-once DoneFn (Finish) is untouched.
    jk::process::SpawnOptions opt;
    // stage-1 marking: shell literal, docs/68 W4 — cmd.exe 접두는 2단계 셸
    // 추상(engine별 cfg) 치환 대상, 1단계는 원문 유지.
    opt.commandLineUtf8 =
        "cmd.exe /c " + BuildEngineCmd(cfg, job->prompt, job->resumeSession);
    // Session history binds to cwd (claude --resume lookup); cfg.directory
    // pins it (default: repo root where .mcp.json lives).
    opt.workingDir = cfg.directory;
    opt.hideWindow = true;            // CREATE_NO_WINDOW + SW_HIDE
    opt.inheritedStdioPipes = true;   // separate stdout/stderr parent pipes
    const jk::process::SpawnResult spawned = jk::process::Spawn(opt);
    if (!spawned.ok) {
        // Partial-success pipe cleanup is the adapter's job now (opus NIT-2
        // moved inside Spawn); Finish keeps the exactly-once DoneFn contract.
        Finish(true);
        return 0;
    }
    void* readOut = spawned.stdoutRead;
    void* readErr = spawned.stderrRead;

    // Kill-on-close job: the engine tree (cmd → ollama → claude → its MCP
    // children) dies with the turn. Without it, grandchildren survive the
    // 10-min TerminateProcess (the wrapper is not the pipe holder) and leak
    // — and a surviving grandchild holding the stdout pipe makes ReadFile
    // block FOREVER, which is how a hung turn used to dead-lock the busy
    // gate until a bridge restart. Creation (KILL_ON_JOB_CLOSE) lives in the
    // adapter; the handle-close == tree-death contract is preserved (close
    // at the end of this function is still the kill).
    void* jobTree = jk::process::CreateKillOnCloseJob();
    // (2026-10-05 결함 픽스 e373339 수선 승계 — 플랜 B 조사 중 TDD 셀프테스트가
    // 포획한 인자 스왑(job↔process, FALSE err=6 — kill-on-close 계약 무효 운용)
    // 는 어댑터 AssignToJob의 (job, process) 시그니처 계약으로 흡수 완료. 원문과
    // 같이 반환값은 무시한다.)
    jk::process::AssignToJob(jobTree, spawned);

    // Read stdout+stderr CONCURRENTLY with an idle deadline. The old design
    // read stdout to EOF before touching stderr (stderr pipe could fill and
    // stall the child) and checked its 10-min timeout only AFTER EOF —
    // unreachable while ReadFile was stuck on a pipe a grandchild held open.
    // Complete lines are parsed AS THEY LAND so stream deltas reach the
    // consumer while claude is still generating. stdoutBuf stays intact —
    // the line scan advances a separate offset (the stub engine's plain
    // echo-JSON needs the whole buffer in the EOF fallback below).
    // W6 standardization: steady_clock replaces GetTickCount64 — same
    // monotonic-ms semantics, portable to stage 2. The 10-min idle-kill
    // contract (kTurnIdleKillMs) is unchanged.
    const auto tTurn = std::chrono::steady_clock::now();
    constexpr auto kTurnIdleKillMs = std::chrono::minutes{ 10 };  // bridge convention
    bool outOpen = true, errOpen = true;
    std::string stdoutBuf, stderrBuf;
    size_t lineScan = 0;
    char chunk[4096];
    // Peer-closed verdict values — the adapter reports the old
    // GetLastError()==ERROR_BROKEN_PIPE predicate through brokenError
    // (109 observed; 232 is the alternate closed-pipe report — same contract
    // as selftest case 14).
    constexpr int kErrBrokenPipe = 109;  // ERROR_BROKEN_PIPE
    constexpr int kErrNoData = 232;      // ERROR_NO_DATA (pipe closed)
    while (outOpen || errOpen) {
        bool progressed = false;
        if (outOpen) {
            uint32_t avail = 0;
            int broken = 0;
            if (jk::process::PeekPipeAvail(readOut, &avail, &broken) &&
                avail > 0) {
                const int got =
                    jk::process::ReadPipeData(readOut, chunk, sizeof(chunk));
                if (got > 0) {
                    progressed = true;
                    stdoutBuf.append(chunk, static_cast<size_t>(got));
                    size_t nl;
                    while ((nl = stdoutBuf.find('\n', lineScan)) !=
                           std::string::npos) {
                        std::string line = stdoutBuf.substr(lineScan,
                                                            nl - lineScan);
                        lineScan = nl + 1;
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        if (!line.empty()) ParseStreamLine(line, out, *job);
                    }
                } else {
                    outOpen = false;  // read==0 (EOF) / -1 — this pipe is done
                }
            } else if (broken == kErrBrokenPipe || broken == kErrNoData) {
                outOpen = false;
            }
        }
        if (errOpen) {
            uint32_t avail = 0;
            int broken = 0;
            if (jk::process::PeekPipeAvail(readErr, &avail, &broken) &&
                avail > 0) {
                const int got =
                    jk::process::ReadPipeData(readErr, chunk, sizeof(chunk));
                if (got > 0) {
                    progressed = true;
                    stderrBuf.append(chunk, static_cast<size_t>(got));
                } else {
                    errOpen = false;  // read==0 (EOF) / -1 — this pipe is done
                }
            } else if (broken == kErrBrokenPipe || broken == kErrNoData) {
                errOpen = false;
            }
        }
        if (!progressed) {
            // No data flowing — the kill criterion. A turn streaming deltas
            // (the user SEES it live) is never killed by the deadline.
            if (std::chrono::steady_clock::now() - tTurn >=
                kTurnIdleKillMs) {
                jk::process::TerminateJobTree(jobTree, 1);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    jk::process::CloseHandleLike(readOut);
    jk::process::CloseHandleLike(readErr);
    // Reap the wrapper; the job close below kills any stragglers (claude's
    // MCP children) — per-turn processes, not the user's ollama daemon.
    WaitForSingleObject(spawned.process, 5000);
    jk::process::CloseHandleLike(spawned.process);
    // The primary thread handle is closed inside Spawn (adapter-owned
    // handover) — no separate hThread close here anymore.
    jk::process::CloseHandleLike(jobTree);  // close IS the kill (contract b)

    if (!out->ok && !out->sawResult) {
        // No stream result line (stub engine echoes plain JSON) — legacy
        // whole-buffer parse of the reply object. sawResult gates it: the
        // buffer holds CONCATENATED stream lines and the lenient parse of
        // the first object re-OKed failed turns (2026-09-27 빈 응답 —
        // init 라인의 session_id가 오류 라인을 덮어 다음 resume을 오염).
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
    job->prompt = promptUtf8;  // UTF-8 그대로 — 와이드 왕복 변환 소각(무손실)
    job->resumeSession = resumeSessionId;
    job->onDelta = onDelta;
    job->onDone = onDone;
    job->user = user;
    // W6 standardization: std::thread replaces CreateThread. The legacy
    // CreateThread failure contract ("no thread, no turn" — the thread is
    // the busy flag's releaser) is preserved: std::thread reports spawn
    // failure by throwing std::system_error instead of returning a null
    // handle, so the rollback observes identically (job deleted, busy
    // cleared, turn not started). Detach keeps the original handle-close —
    // the owner takes no interest in the thread's lifetime.
    try {
        std::thread(LlmTurnThread, job).detach();
    } catch (const std::system_error&) {
        // The thread is the busy flag's releaser — no thread, no turn.
        delete job;
        busy_ = 0;
        return false;
    }
    return true;
}

} // namespace agent
} // namespace jk