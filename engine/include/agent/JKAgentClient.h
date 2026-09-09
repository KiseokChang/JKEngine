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

// One AgentReply parked by the read pump, keyed by its queryId so an
// out-of-order flush (ping round-trips during an approval wait) can be
// matched to the query that sent it.
struct AgentReplyMsg {
    uint32_t queryId = 0;
    std::string json;
};

// Control-only window-server client for the Desktop Agent API. Shared by the
// agentctl CLI mode, the jkagentd MCP broker and the chat window. Blocking
// mode: Query/QueryRaw wait for the matching AgentReply (the server answers
// every query exactly once — except "ask"-gated ones, which reply when the
// inline approval resolves; chat uses the non-blocking form for those).
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

    // Non-blocking query (chat MVP): send and return at once — the reply
    // arrives during later pumps (ping round-trips) and is picked up with
    // PollReply. Used when the tool result may be gated behind an inline
    // approval, which must not freeze the chat UI.
    uint32_t SendQuery(const std::string& tool, const std::string& argsJson);
    // Send a complete request JSON as-is; returns its queryId (0 = failed).
    uint32_t SendRaw(const std::string& requestJson);
    // Pick up the reply for queryId once it has been flushed into the queue.
    bool PollReply(uint32_t queryId, std::string& jsonOut);

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
    std::deque<AgentReplyMsg> pendingReplies_;
    std::deque<AgentEvent> eventQueue_;
};

} // namespace agent
} // namespace jk

#endif // JKAGENTCLIENT_H