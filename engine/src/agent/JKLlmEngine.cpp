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
#include <condition_variable>  // TurnSync 동기 브리지 — Done 대기(T3)
#include <cstdio>
#include <mutex>
#include <system_error>  // std::system_error — thread spawn failure contract
#include <thread>

// stage-1 남은 Win32 접촉 1건(WaitForSingleObject spawn reap)은 stage-3
// task 5에서 jk::process::WaitForExit 어댑터로 승계 — 시간(GetTickCount64)·
// 스레드(CreateThread) 접촉은 W6 표준화로 std::chrono/std::thread 치환 완료.
// 스폰·파이프·Job 계열은 jk::process 어댑터(docs/68 W4)로 흡수 완료 — 이 TU의
// 수기 kernel32 dllimport 선언은 소각됐다(win32 본문은 어댑터 TU가 windows.h
// 소유 관례로 이동; posix 본문은 waitpid WNOHANG 예산 루프).

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
        std::fopen((exeDir + "/state/chat.json").c_str(), "rb");
    if (!f) return cfg;
    char buf[4096] = {};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    AgentJson c(buf);
    std::string v;
    if (c.ok()) {
        // fileKnown 계약(ChatConfig 멤버 주석): engine 키가 실제로 명시된
        // 파일만 승격 opt-in 표지로 세운다 — 파일 부재·파스 실패·키 누락은
        // 전부 false(데이타 기본값의 우연 일치를 구성으로 쳐주지 않는다).
        if (c.GetStr("engine", v)) {
            cfg.engine = v;
            cfg.fileKnown = true;
        }
        if (c.GetStr("model", v)) cfg.model = v;
        if (c.GetStr("directory", v)) cfg.directory = v;
        if (c.GetStr("direct_cmd", v)) cfg.directCmd = v;
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
// 인용: 명령행 임베드는 ShellDqEscape 한 근원으로(prompt의 -p 이스케이프와
// 동일 — T3 fix r1, F3-1 cmd 토글 주입 수리 계보 상동).
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

// Escaper used for EVERY dynamic text that lands inside the command string —
// not just the prompt (final review F1: --resume's session id and the model
// name are dynamic text too). EscDq 람다에서 승격(T3): ollama-direct 조립식
// (BuildOllamaDirectCmd, 공개 계약 락)이 같은 이스케이프를 재용해야 하므로
// 람다 밖으로 올렸다 — 복제 금지 계약과 같은 뿌리.
//
// F3-1 (T3 fix r1, 2026-10-08): win32 leg는 cmd.exe /c 접두를 타므로 조립식의
// 인용은 "cmd 토글"+"CRT argv 재파싱"의 이중 파서를 통과한다. 현행 `\"`는
// CRT에겐 리터럴 따옴표지만 cmd에겐 그대로 토글이어서, 프롬프트 본문의
// 불균형 따옴표가 cmd의 인용 지역을 일찍 닫아 뒤따르는 & | < > 를 살아있는
// 메타문자로 만든다(실측: cmd.exe /c claude -p "she said \"hi & echo ..."에서
// stream-json 플래그가 전부 도둑맞고 cmd2로 echo가 별행 개통 — 2026-10-08
// probe_claude_leg A 영수증, claude/ollama 양 leg 공통 노출).
//
// 수리: win32 분기는 내용 따옴표를 `""` 로 두 배화한다 — 3 파서가 한 문자열을
// 각기 다르게 읽는다(2026-10-08 argprn/probe 실측): cmd는 토글 짝(=중립,
// 지역이 계속 열려 메타문자 사망), node(msvc CRT — claude.cmd 사슬)는 지역 안
// 리터럴 따옴표(컨텐츠 원문), shell32/Go(ollama.exe)는 리터럴 따옴표+토글
// (균형 인용 원문 도달, 2026-10-08 probe_ollama_leg3 C2 영수증). 불균형 인용
// 컨텐츠는 Go 파서에서 지역을 일찍 닫아 첫 따옴표 뒤 잘림이 남는다(S 영수증)
// — 개선 전의 "주입+플래그 도난"보다 안전이 우선이고, 잔여는 원장에 기록.
// 케릿 갑옷(^&)은 F2 실측상 인용 안에서 리터럴로 살아남아 컨텐츠를 변형하므로
// 기각 — 지역이 계속 열려 있어야 메타문자가 자연 사망한다.
// posix 분기는 sh 이중 따옴표 라이브 문자(`\ $ ` 백틱) 이스케이프 — 본 수리와
// 무관(조건부 실행 계약 c: posix 분기 원문 유지).
//
// winDoubled=false 분기(kLlmDenySettings 전용 — T3 fix r1 회귀 원장):
// settings는 claudeArgs에 얹혀 ollama leg에서 ollama의 재인용 층을 한 번 더
// 통과한다. 이중화 형태는 그 층에서 따옴표가 통째로 벗겨져 claude에
// {"permissions:{deny:[Bash(taskkill:*),... 같은 무따옴표 잔해로 도달한다
// (실측 probe_ollama_shape O1: "Error: Settings file not found", 전형 0 —
// O2 legacy \" 제어 런은 전 스트림 생존). 상수라 주입 표면이 아니고, 인용
// 짝이 균형이어서 cmd 토글 정수합(중립)+CRT/shell32 리터럴(`\"`)이므로 legacy
// 형태가 3leg 전부 주입 없음 — prompt(동적 컨텐츠 = 주입 표면)만 이중화 모드,
// 설정 상수는 legacy 모드로 한 함수 안에 두 계약을 공존시킨다(3leg 복제 금지
// 계약은 유지).
std::string ShellDqEscape(const std::string& s, bool winDoubled = true) {
    std::string out;
    for (char ch : s) {
        if (ch == '"') {
#ifdef _WIN32
            out += winDoubled ? "\"\""   // cmd 토글 중립 짝 + CRT/shell32 리터럴
                              : "\\\"";  // legacy — settings(ollama 재인용 층 생존 형태)
#else
            out += "\\\"";  // sh 이중 따옴표 지역 안 리터럴 따옴표(기존 계약)
#endif
        }
#ifndef _WIN32
        // posix leg executes via /bin/sh -c (jk::process posix mapping),
        // and inside sh double quotes `\`, `$` and backtick stay LIVE (a
        // lone backslash also acts as an escape character before these).
        // Escape them backslash-prefixed so preamble+prompt text lands
        // literally — otherwise `$(...)` or backticks from the user chat
        // prompt or attached bytes would EXECUTE. Windows CreateProcessW
        // never touches a shell, so the win32 leg keeps the original case
        // verbatim (동작 변화 0 — the escaped forms agree for the shared
        // case: `"`).
        else if (ch == '\\') out += "\\\\";
        else if (ch == '$') out += "\\$";
        else if (ch == '`') out += "\\`";
#endif
        else out += ch;
    }
    return out;
}

std::string BuildEngineCmd(const ChatConfig& cfg,
                           const std::string& prompt,
                           const std::string& resumeSessionId) {
    // Every turn gets the fixed Korean preamble (CoT/markdown leak guard,
    // above) prepended to the raw prompt, before quote escaping.
    const std::string fullPrompt = kLlmTurnPreamble + prompt;
    std::string esc = ShellDqEscape(fullPrompt);
    // Token streaming (docs/31 §6): stream-json + partial messages gives
    // line-delimited events with content_block_delta text fragments. --verbose
    // is REQUIRED by stream-json in -p mode.
    std::string claudeArgs =
        "-p \"" + esc +
        "\" --output-format stream-json --verbose --include-partial-messages";
    if (cfg.skipPermissions) claudeArgs += " --dangerously-skip-permissions";
    // JSON 인용 — 상수는 legacy `\"` 모드(ShellDqEscape 2차 인자 false). 이중
    // 화(`""`)는 ollama leg의 재인용 층에서 따옴표가 통째로 벗겨져 claude가
    // "Settings file not found"로 파산한다(실측 probe_ollama_shape O1/O2 —
    // T3 fix r1 회귀 원장). 상수는 인용 균형+cmd 메타문자 0이어서 legacy 형태
    // 도 cmd 토글 중립(정수합)+CRT 리터럴 — 주입 없음.
    claudeArgs += " --settings \"" +
                  ShellDqEscape(kLlmDenySettings, false) + "\"";
    if (!resumeSessionId.empty()) {
        // F1 (docs/70 final review): the session id is dynamic text too —
        // same escaper class as the prompt (posix triple-escape inside).
        claudeArgs += " --resume \"" + ShellDqEscape(resumeSessionId) + "\"";
    }
    // NOTE: claude CLI has no --directory flag (guide table was wrong for
    // CLI 2.1.x — only --add-dir exists). cfg.directory is applied as the
    // worker process's current directory in the turn thread instead; session
    // history binds to cwd, so --resume needs the same dir every turn.

    std::string cmd;
    if (cfg.engine == "stub") {
        // No-network machinery test: emits a valid reply JSON.
#ifdef _WIN32
        // stage-1 marking: shell literal, docs/68 W4 — stub 테스트 리터럴
        // (기계 검증용 무연결 왕복 데이터), 2단계 셸 추상 치환 대상 아님.
        cmd = kStubShellCmdWin32;
#else
        // posix: jk::process::Spawn rides /bin/sh -c — single-quote keeps the
        // JSON verbatim. Same stub reply bytes, no cmd.exe.
        cmd = kStubShellCmdPosix;
#endif
    } else if (cfg.engine == "claude") {
        cmd = "claude " + claudeArgs;
    } else if (cfg.engine == "ollama-direct") {
        // ollama-direct (스펙 2026-10-08 chat-llm-promotion 설계 결정 3) —
        // stdout이 곧 답변인 일반 텍스트 턴(stream-json 아님). 폰에 claude
        // CLI(node 스택)가 없어도 되는 1차 경로(T2 실측 NO-CLAUDE-CLI).
        // direct_cmd 주입은 셀프테스트 전용 씽크다 — 기본 조립이 실 ollama를
        // 쏘면 캐논이 그 기기의 설치/네트워크에 의존하게 된다(환경 의존 함정).
        cmd = cfg.directCmd.empty() ? BuildOllamaDirectCmd(cfg, prompt)
                                    : cfg.directCmd;
    } else {  // "ollama" (default)
        // F1: cfg.model rides the same sh double-quote string — escape it
        // like every other dynamic text (win32 keeps bare quotes verbatim).
        cmd = "ollama launch claude --model \"" + ShellDqEscape(cfg.model) +
              "\" -- " + claudeArgs;
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
    // Shell prefix (플랜 F2 — docs/70 §6 #2): win32 rides cmd.exe /c (shell
    // literal 원문 유지); posix's jk::process::Spawn passes commandLineUtf8
    // to /bin/sh -c directly, so no prefix. 2단계 셸 추상(engine별 cfg) 대상
    // 이 아니라 플랫폼 접두 — 접두만 플랫폼 조건이어도 stub/engine 본선 전부
    // 개통된다(셸 본체의 선택은 cfg가 소유).
#ifdef _WIN32
    opt.commandLineUtf8 =
        "cmd.exe /c " + BuildEngineCmd(cfg, job->prompt, job->resumeSession);
#else
    opt.commandLineUtf8 =
        BuildEngineCmd(cfg, job->prompt, job->resumeSession);
#endif
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
    // WaitForSingleObject(5000) 원문 관측 = 기다리고 실패 무시 — WaitForExit
    // (어댑터, stage-3 task 5)가 그대로 승계한다.
    jk::process::WaitForExit(spawned.process, 5000);
    jk::process::CloseHandleLike(spawned.process);
    // The primary thread handle is closed inside Spawn (adapter-owned
    // handover) — no separate hThread close here anymore.
    jk::process::CloseHandleLike(jobTree);  // close IS the kill (contract b)

    if (cfg.engine == "ollama-direct") {
        // stdout은 곧 답변인 일반 텍스트(stream-json 아님 — 설계 결정 3)라
        // stream-json 파서의 경로를 타지 않는다: 경계 공백·개행만 잘라 수집한
        // 전체를 결과로. 파서 복제 금지 계약 — stream-json leg와 공유하는 것은
        // 스폰(어댑터)·파이프·kill 계약뿐이고 텍스트 해석은 얇게 유지한다.
        std::string text = stdoutBuf;
        auto TrimWsp = [](const std::string& s) {
            const size_t b = s.find_first_not_of(" \t\r\n");
            if (b == std::string::npos) return std::string();
            const size_t e = s.find_last_not_of(" \t\r\n");
            return s.substr(b, e - b + 1);
        };
        text = TrimWsp(text);
        out->ok = !text.empty();  // 비영(stdout 공백) = 정직 실패
        out->result = text;
        if (!out->ok) {
            // 진단 가능성 — stderr 꼬리를 실어 보낸다(legacy leg와 동일 관측).
            out->result = stderrBuf.size() > 0
                              ? "stderr: " +
                                    stderrBuf.substr(
                                        stderrBuf.size() > 400
                                            ? stderrBuf.size() - 400
                                            : 0)
                              : std::string("ollama-direct: 빈 응답");
        }
        // plain-text leg에서 delta 콜백은 합법적으로 한 번도 안 온다 — 어떤
        // 텍스트 조각이 우연히 stream-event JSON으로 해석됐더라도 전체 수집
        // 결과가 진실이라 streamed는 지운다.
        out->streamed = false;
    } else if (!out->ok && !out->sawResult) {
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

// 동기 브리지(TurnSync)의 대기 상태 — 힙+refcount가 계약이다. StartTurn의 Done
// 콜백은 "정확히 한 번"이 항상 온다(스폰 실패 포함), 그러나 TurnSync가
// timeoutMs 안에 못 기다리고 돌아가면 대기자는 이미 사라졌다 — 지연 Done이
// 죽은 스택을 건드리는 dangling을 막으려면 상태는 한 쪽의 수명이 아니라 마지막
// 접근자의 수명을 따라야 한다. 호출자와 Done 콜백이 각자 1씩 놓아간다.
struct SyncWaiter {
    std::mutex m;
    std::condition_variable cv;
    LlmTurnResult r;
    bool done = false;
    std::atomic<int> refs{2};  // caller + worker(DoneFn)
    void Release() {
        if (refs.fetch_sub(1) == 1) delete this;
    }
};

// DoneFn 시그니처(함수 포인터) 계약이라 람다 캡처 대신 자유 함수로 — user는
// SyncWaiter*를 실어 온다. busy 지우기(=store(0))는 worker가 onDone 앞에서
// 이미 했으므로 지연 Done 역시 후속 턴의 시작을 막지 않는다.
void SyncWaiterDone(LlmTurnResult&& res, void* user) {
    SyncWaiter* w = static_cast<SyncWaiter*>(user);
    {
        std::lock_guard<std::mutex> lk(w->m);
        w->r = std::move(res);
        w->done = true;
    }
    w->cv.notify_all();
    w->Release();
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

bool JKLlmEngine::TurnSync(const std::string& promptUtf8, LlmTurnResult& out,
                           int timeoutMs) {
    // 반환 값 = out.ok (헤더 계약) — false는 모든 실패 문(busy/스폰/타임아웃/
    // 정직 턴 실패)을 하나로 통과시키고, 스펙의 fallback 원칙(조용한 폴백)은
    // false를 보고 소비자가 수행한다.
    out = LlmTurnResult();
    SyncWaiter* w = new SyncWaiter;
    if (!StartTurn(promptUtf8, "", nullptr, &SyncWaiterDone, w)) {
        w->Release();  // StartTurn이 씽크다 소유권을 안 받았다 — 즉시 정산
        out.result = "engine busy";
        return false;
    }
    bool arrived = false;
    {
        std::unique_lock<std::mutex> lk(w->m);
        arrived = w->cv.wait_for(lk, std::chrono::milliseconds(timeoutMs),
                                 [&w] { return w->done; });
    }
    if (!arrived) {
        // 타임아웃 — ok=false 정직. 자식 kill은 StartTurn 내부 계약(10분
        // idle kill + kill-on-close job tree)이 소관이라 이 래퍼는 중복 kill
        // 을 만들지 않는다; worker는 돌아가고 지연 Done은 refcount로 흡수된다
        // (이 뒤에 w는 소유권 반납 — 더 이상 접근 없음).
        out.ok = false;
        out.result = "turn timeout (";
        out.result += std::to_string(timeoutMs);
        out.result += "ms)";
        w->Release();
        return false;
    }
    out = std::move(w->r);
    w->Release();  // caller 몫 — Done 콜백 몫은 SyncWaiterDone이 놓아간다
    return out.ok;
}

// ollama-direct의 기본 스폰 명령(공개 계약 락 — 헤더 선언의 정의 본체).
// BuildEngineCmd의 비영 분기가 유일 런타임 소비자고, selftest가 같은 조립식
// 원문을 단정한다(kStubShellCmd* 상수 선례 — 엔진 리터럴의 단일 근원, 3처
// 복제 소각). 프리앰블은 plain-text leg에도 공통 적용: 폰 웹 회신이 플레인
// 텍스트로 렌더링되는 실측(docs/60 ⑥)은 엔진 경로와 무관하다.
std::string BuildOllamaDirectCmd(const ChatConfig& cfg,
                                 const std::string& promptUtf8) {
    return "ollama run \"" + ShellDqEscape(cfg.model) + "\" \"" +
           ShellDqEscape(kLlmTurnPreamble + promptUtf8) + "\"";
}

} // namespace agent
} // namespace jk
