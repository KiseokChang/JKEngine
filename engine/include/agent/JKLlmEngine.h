#ifndef JKLLMENGINE_H
#define JKLLMENGINE_H

#include <atomic>
#include <string>

namespace jk {
namespace agent {

// One LLM turn outcome (jkchat docs/31 §6 lineage). "streamed" means at
// least one text delta reached the delta callback — the consumer skips
// re-printing the result when it has already typed itself in.
struct LlmTurnResult {
    bool ok = false;
    bool streamed = false;
    std::string result;     // the reply JSON's "result" field
    std::string sessionId;  // its "session_id" field ("" on parse failure)
};

// state/chat.json — user tunables (workbench config.json convention).
struct ChatConfig {
    std::string engine = "ollama";  // "ollama" | "claude" | "stub"
    std::string model = "kimi-k2.7-code:cloud";
    bool skipPermissions = true;    // workbench default; headless auto-denies
                                    // tool consent when off
    std::string directory =
        "I:\\progwork\\JKENGINE";   // worker cwd: .mcp.json (jkagentd) lives here
};

// Reads ExeDir()\state\chat.json — falls back to the defaults above.
ChatConfig LoadChatConfig();

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

    bool Busy() const { return busy_ != 0; }

private:
    std::atomic<int> busy_{0};
};

} // namespace agent
} // namespace jk

#endif // JKLLMENGINE_H