#ifndef JKLLMENGINE_H
#define JKLLMENGINE_H

#include <atomic>
#include <string>

namespace jk {
namespace agent {

// stub 턴의 셸 명령 리터럴 — 윈32는 cmd.exe 접두 유지(플랜 F2), posix는
// 접두 없는 sh echo. selftest 케이스 10(engine/tools/posix_selftest/main.cpp
// TestLlmStubShell)도 같은 리터럴을 재실행하므로 한 곳에서 관리 — 3처 복제
// 소각(docs/71 리뷰 MEDIUM).
inline constexpr char kStubShellCmdWin32[] =
    "cmd.exe /c echo {\"result\":\"stub ok\",\"session_id\":\"stub-1\"}";
inline constexpr char kStubShellCmdPosix[] =
    "echo '{\"result\":\"stub ok\",\"session_id\":\"stub-1\"}'";

// One LLM turn outcome (jkchat docs/31 §6 lineage). "streamed" means at
// least one text delta reached the delta callback — the consumer skips
// re-printing the result when it has already typed itself in.
struct LlmTurnResult {
    bool ok = false;
    bool streamed = false;
    bool sawResult = false;  // a type:"result" stream line was seen (success
                             // or error) — gates the legacy EOF re-parse
    std::string result;     // the reply JSON's "result" field
    std::string sessionId;  // its "session_id" field ("" on parse failure)
};

// state/chat.json — user tunables (workbench config.json convention).
struct ChatConfig {
    std::string engine =
        "ollama";  // "ollama" | "claude" | "stub" | "ollama-direct"
    std::string model = "glm-5.3-flash:cloud";  // 2026-09-24 사용자 지정 디폴트
                                                // (구 kimi-k2.7-code:cloud)
    bool skipPermissions = true;    // workbench default; headless auto-denies
                                    // tool consent when off
    std::string directory =
        "I:\\progwork\\JKENGINE";   // worker cwd: .mcp.json (jkagentd) lives here
    // ollama-direct 전용 스폰 명령 주입(empty = 기본 조립 BuildOllamaDirectCmd).
    // 셀프테스트 전용 씽크다: 기본 명령이 실 ollama를 쏘면 셀프테스트 캐논이
    // 그 기기의 ollama 설치/네트워크에 의존하게 된다(환경 의존 함정). echo
    // 스터브로 스폰 단정을 대체한다. 실제 런타임에서는 쓰지 않는다.
    std::string directCmd;
};

// Reads ExeDir()\state\chat.json — falls back to the defaults above.
ChatConfig LoadChatConfig();

// ollama-direct 분기의 기본 스폰 명령 조립 — BuildEngineCmd(비공개)가 유일한
// 소비자고, selftest가 같은 조립식 원문을 그대로 단정하기 위한 계약 락 선
// (kStubShellCmd* 상수 선례: 엔진 리터럴의 단일 근원 — 3처 복제 소각). cfg.
// directCmd 주입이 비어 있을 때만 이 형태가 실제로 스폰된다.
std::string BuildOllamaDirectCmd(const ChatConfig& cfg,
                                 const std::string& promptUtf8);

// Headless LLM turn runner (claude CLI subprocess, claude_wrapper guide
// §1-2) — extracted from the jkchat window so jkbridge reuses the exact
// engine. One turn at a time per instance; token deltas and the turn
// outcome arrive through the callbacks ON THE WORKER THREAD, so the
// consumer must be thread-safe (jkchat: PostMessage; jkbridge: WS send
// under a mutex).
//
// claude session continuity is the CONSUMER's state (it was jkchat's
// g_sessionId): pass the id to resume via resumeSessionId, and store the
// session_id that arrives in the turn result. The engine itself only owns
// the busy gate — a per-instance state a dying session can't strand.
class JKLlmEngine {
public:
    using DeltaFn = void (*)(const std::string& utf8Delta, void* user);
    using DoneFn = void (*)(LlmTurnResult&& result, void* user);

    // Spawns the worker thread and returns at once. Exactly one DoneFn call
    // always follows (spawn failure included — a latent jkchat gap, fixed
    // here so jkbridge can rely on it). Returns false when a turn is
    // already running.
    bool StartTurn(const std::string& promptUtf8,
                   const std::string& resumeSessionId, DeltaFn onDelta,
                   DoneFn onDone, void* user);

    // 동기 1회 턴 브리지 (스펙 2026-10-08 chat-llm-promotion 설계 결정 2) —
    // StartTurn 위에서 worker 스레드의 Done을 condition_variable로 기다리는
    // 얇은 래퍼일 뿐이다(스폰·파이프·stream-json 파서·kill 계약 전부 재용,
    // 복제 금지 계약). 폰 jkweb/jktalk의 연결당 1스레드 소관이 쓴다.
    //
    // 반환 값 = out.ok (턴이 완료되어 성공적으로 끝났을 때만 true):
    //   false면 out.ok=false가 확정이고 out.result에 정직 이유가 실린다 —
    //   busy(다른 턴 진행 중), 스레드 스폰 실패, timeoutMs 내 Done 미도착,
    //   그리고 실제 턴의 정직 실패(스폰 실패·빈 stdout)가 전부 같은 문.
    //   스펙의 fallback 원칙(엔진 실패 → 기존 라우터로 조용히 폴백)은 이
    //   false를 보고 소비자(jkweb/jktalk)가 수행한다.
    // 타임아웃 시에도 자식 kill은 StartTurn 내부 계약(10분 idle kill +
    // kill-on-close job tree)에 이미 있다 — 이 래퍼는 중복 kill을 만들지
    // 않는다. 지연 Done은 refcount된 대기 상태로 흡수한다(대기자가 떠난 뒤
    // 도착한 Done은 상태 해제와 경합하지 않는다 — Dangling 방지).
    bool TurnSync(const std::string& promptUtf8, LlmTurnResult& out,
                  int timeoutMs = 120000);

    bool Busy() const { return busy_ != 0; }

private:
    std::atomic<int> busy_{0};
};

} // namespace agent
} // namespace jk

#endif // JKLLMENGINE_H