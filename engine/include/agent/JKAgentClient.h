#ifndef JKAGENTCLIENT_H
#define JKAGENTCLIENT_H

#include <ipc/JKWireProtocol.h>
#include <ipc/JKPipeTransport.h>

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace jk {
namespace agent {

// One desktop event pushed by the server (MsgType::AgentEvent).
struct AgentEvent {
    std::string topic;  // e.g. "window.created" — the JSON "topic" field
    std::string json;   // the raw event JSON
};

// Control-only window-server client for the Desktop Agent API. Shared by the
// agentctl CLI mode and the jkagentd MCP broker. One query at a time: the
// server answers every AgentQuery exactly once, so blocking on the matching
// AgentReply is safe (M1 — no timeout; the pipe closing unblocks with false).
class JKAgentClient {
public:
    // Handshake: Hello (pid self-reported), no surface request.
    bool Connect(const std::string& pipeName = "\\\\.\\pipe\\JKWindowServerPipe");
    bool IsConnected() const;

    // Send {"tool":"<tool>","args":<argsJson>} and wait for the reply JSON.
    bool Query(const std::string& tool, const std::string& argsJson,
               std::string& replyJsonOut);
    // Send a complete request JSON as-is (agentctl passes it through).
    bool QueryRaw(const std::string& requestJson, std::string& replyJsonOut);

    // Join/leave the AgentEvent push stream (MsgType::AgentEventSubscribe).
    bool SubscribeEvents(bool subscribe);

    // Drain events received as a side effect of earlier pumps into out.
    size_t PollEvents(std::vector<AgentEvent>& out);

private:
    // Read messages until a reply is available or the pipe dies. Events seen
    // along the way are queued in eventQueue_.
    bool PumpMessages();

    std::unique_ptr<ipc::JKPipeTransport> transport_;
    uint32_t nextQueryId_ = 1;
    std::deque<std::string> pendingReplies_;
    std::deque<AgentEvent> eventQueue_;
};

} // namespace agent
} // namespace jk

#endif // JKAGENTCLIENT_H