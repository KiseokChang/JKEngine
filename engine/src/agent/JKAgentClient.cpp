#include <agent/JKAgentClient.h>

#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace jk {
namespace agent {

bool JKAgentClient::Connect(const std::string& pipeName) {
    transport_ = ipc::JKPipeTransport::ConnectClient(pipeName);
    if (!transport_) return false;

    // Same handshake as a window client, minus CreateSurface: the server's
    // acceptor decides the connection kind by the SECOND message, so a
    // control connection declares itself with AgentEventSubscribe{0} —
    // "control-only, not (yet) subscribed to event pushes". SubscribeEvents
    // flips the flag later over the live connection.
    ipc::HelloPayload hello{};
#ifdef _WIN32
    hello.pid = static_cast<uint32_t>(GetCurrentProcessId());
#else
    hello.pid = static_cast<uint32_t>(getpid());
#endif
    if (!ipc::WriteMessage(*transport_, ipc::MsgType::Hello, hello)) {
        transport_.reset();
        return false;
    }
    ipc::AgentEventSubscribePayload declare{};
    declare.subscribe = 0;
    if (!ipc::WriteMessage(*transport_, ipc::MsgType::AgentEventSubscribe, declare)) {
        transport_.reset();
        return false;
    }
    return true;
}

bool JKAgentClient::IsConnected() const {
    return transport_ && transport_->IsConnected();
}

bool JKAgentClient::QueryRaw(const std::string& requestJson,
                             std::string& replyJsonOut) {
    replyJsonOut.clear();
    if (!IsConnected()) return false;

    const uint32_t queryId = nextQueryId_++;
    if (!ipc::WriteAgentJson(*transport_, ipc::MsgType::AgentQuery,
                             queryId, 0, requestJson)) {
        return false;
    }

    // Read until OUR reply arrives. Interleaved AgentEvents are queued, not
    // dropped — pushes may arrive while we wait. Replies for earlier
    // non-blocking sends stay parked in pendingReplies_ with their ids.
    while (true) {
        for (auto it = pendingReplies_.begin(); it != pendingReplies_.end(); ++it) {
            if (it->queryId == queryId) {
                replyJsonOut = std::move(it->json);
                pendingReplies_.erase(it);
                return true;
            }
        }
        if (!PumpMessages()) return false;
    }
}

bool JKAgentClient::Query(const std::string& tool, const std::string& argsJson,
                          std::string& replyJsonOut) {
    std::string req = "{\"tool\":\"" + tool + "\",\"args\":" +
                      (argsJson.empty() ? std::string("{}") : argsJson) + "}";
    return QueryRaw(req, replyJsonOut);
}

uint32_t JKAgentClient::SendQuery(const std::string& tool,
                                  const std::string& argsJson) {
    return SendRaw("{\"tool\":\"" + tool + "\",\"args\":" +
                   (argsJson.empty() ? std::string("{}") : argsJson) + "}");
}

uint32_t JKAgentClient::SendRaw(const std::string& requestJson) {
    if (!IsConnected()) return 0;
    const uint32_t queryId = nextQueryId_++;
    if (!ipc::WriteAgentJson(*transport_, ipc::MsgType::AgentQuery,
                             queryId, 0, requestJson)) {
        return 0;
    }
    return queryId;
}

bool JKAgentClient::PollReply(uint32_t queryId, std::string& jsonOut) {
    for (auto it = pendingReplies_.begin(); it != pendingReplies_.end(); ++it) {
        if (it->queryId == queryId) {
            jsonOut = std::move(it->json);
            pendingReplies_.erase(it);
            return true;
        }
    }
    return false;
}

bool JKAgentClient::SubscribeEvents(bool subscribe) {
    if (!IsConnected()) return false;
    ipc::AgentEventSubscribePayload payload{};
    payload.subscribe = subscribe ? 1 : 0;
    return ipc::WriteMessage(*transport_, ipc::MsgType::AgentEventSubscribe,
                             payload);
}

size_t JKAgentClient::PollEvents(std::vector<AgentEvent>& out) {
    out.clear();
    while (!eventQueue_.empty()) {
        out.push_back(std::move(eventQueue_.front()));
        eventQueue_.pop_front();
    }
    return out.size();
}

bool JKAgentClient::PumpMessages() {
    if (!IsConnected()) return false;
    ipc::Message msg;
    if (!ipc::ReadMessage(*transport_, msg)) {
        transport_.reset();
        return false;
    }
    uint32_t queryId = 0, ok = 0;
    std::string json;
    if (msg.type == ipc::MsgType::AgentReply && ipc::ReadAgentJson(msg, queryId, ok, json)) {
        pendingReplies_.push_back(AgentReplyMsg{queryId, std::move(json)});
    } else if (msg.type == ipc::MsgType::AgentEvent && ipc::ReadAgentJson(msg, queryId, ok, json)) {
        AgentEvent ev;
        // The topic is duplicated into a field for consumers; a cheap prefix
        // extract instead of a full JSON parse (event payloads stay opaque).
        const size_t key = json.find("\"topic\":\"");
        if (key != std::string::npos) {
            const size_t start = key + 9;
            const size_t end = json.find('"', start);
            if (end != std::string::npos)
                ev.topic = json.substr(start, end - start);
        }
        ev.json = std::move(json);
        eventQueue_.push_back(std::move(ev));
    }
    // Other message types on a control connection are ignored.
    return true;
}

} // namespace agent
} // namespace jk