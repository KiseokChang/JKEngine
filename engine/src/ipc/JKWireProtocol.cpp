#include <ipc/JKWireProtocol.h>
#include <cstring>

namespace jk {
namespace ipc {

bool WriteMessage(IWireTransport& transport, MsgType type,
                  const std::vector<uint8_t>& payload) {
    return WriteMessage(transport, type, payload.data(), payload.size());
}

bool WriteMessage(IWireTransport& transport, MsgType type,
                  const void* data, size_t len) {
    WireHeader header;
    header.type = static_cast<uint32_t>(type);
    header.length = static_cast<uint32_t>(len);

    if (!transport.Write(&header, sizeof(header))) {
        return false;
    }
    if (len > 0 && !transport.Write(data, len)) {
        return false;
    }
    return true;
}

bool ReadMessage(IWireTransport& transport, Message& out) {
    WireHeader header;
    if (!transport.Read(&header, sizeof(header))) {
        return false;
    }
    if (header.magic != kWireMagic) {
        return false;
    }

    out.type = static_cast<MsgType>(header.type);
    out.payload.assign(header.length, 0);
    if (header.length > 0) {
        if (!transport.Read(out.payload.data(), header.length)) {
            return false;
        }
    }
    return true;
}

bool WriteAgentJson(IWireTransport& transport, MsgType type,
                    uint32_t queryId, uint32_t ok, const std::string& json) {
    if (json.size() > 0x7FFFFFFFull) return false;
    const uint32_t jsonLen = static_cast<uint32_t>(json.size());
    std::vector<uint8_t> payload;
    if (type == MsgType::AgentQuery) {
        AgentQueryHeader h{queryId, jsonLen};
        payload.resize(sizeof(h) + jsonLen);
        std::memcpy(payload.data(), &h, sizeof(h));
    } else if (type == MsgType::AgentReply) {
        AgentReplyHeader h{queryId, ok, jsonLen};
        payload.resize(sizeof(h) + jsonLen);
        std::memcpy(payload.data(), &h, sizeof(h));
    } else if (type == MsgType::AgentEvent) {
        AgentEventHeader h{jsonLen};
        payload.resize(sizeof(h) + jsonLen);
        std::memcpy(payload.data(), &h, sizeof(h));
    } else if (type == MsgType::AgentToolResult) {
        AgentReplyHeader h{queryId, ok, jsonLen};   // queryId = reqId
        payload.resize(sizeof(h) + jsonLen);
        std::memcpy(payload.data(), &h, sizeof(h));
    } else {
        return false;
    }
    if (jsonLen)
        std::memcpy(payload.data() + payload.size() - jsonLen,
                    json.data(), jsonLen);
    return WriteMessage(transport, type, payload);
}

bool ReadAgentJson(const Message& msg, uint32_t& queryId, uint32_t& ok,
                   std::string& json) {
    queryId = 0; ok = 0; json.clear();
    size_t headerLen = 0;
    if (msg.type == MsgType::AgentQuery)       headerLen = sizeof(AgentQueryHeader);
    else if (msg.type == MsgType::AgentReply)  headerLen = sizeof(AgentReplyHeader);
    else if (msg.type == MsgType::AgentEvent)  headerLen = sizeof(AgentEventHeader);
    else if (msg.type == MsgType::AgentToolResult) headerLen = sizeof(AgentReplyHeader);
    else return false;
    if (msg.payload.size() < headerLen) return false;
    uint32_t jsonLen = 0;
    if (msg.type == MsgType::AgentQuery) {
        AgentQueryHeader h; std::memcpy(&h, msg.payload.data(), sizeof(h));
        queryId = h.queryId; jsonLen = h.jsonLen;
    } else if (msg.type == MsgType::AgentReply ||
               msg.type == MsgType::AgentToolResult) {
        AgentReplyHeader h; std::memcpy(&h, msg.payload.data(), sizeof(h));
        queryId = h.queryId; ok = h.ok; jsonLen = h.jsonLen;
    } else {
        AgentEventHeader h; std::memcpy(&h, msg.payload.data(), sizeof(h));
        jsonLen = h.jsonLen;
    }
    if (msg.payload.size() != headerLen + jsonLen) return false;
    json.assign(reinterpret_cast<const char*>(msg.payload.data()) + headerLen, jsonLen);
    return true;
}

// --- 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §3) -------------------------

bool WriteAgentToolRegister(IWireTransport& transport, const std::string& json) {
    if (json.size() > 0x7FFFFFFFull) return false;
    const uint32_t jsonLen = static_cast<uint32_t>(json.size());
    std::vector<uint8_t> payload(sizeof(AgentToolRegisterHeader) + jsonLen);
    AgentToolRegisterHeader h{jsonLen};
    std::memcpy(payload.data(), &h, sizeof(h));
    if (jsonLen) std::memcpy(payload.data() + sizeof(h), json.data(), jsonLen);
    return WriteMessage(transport, MsgType::AgentToolRegister, payload);
}

bool ReadAgentToolRegister(const Message& msg, std::string& json) {
    json.clear();
    if (msg.type != MsgType::AgentToolRegister) return false;
    if (msg.payload.size() < sizeof(AgentToolRegisterHeader)) return false;
    AgentToolRegisterHeader h; std::memcpy(&h, msg.payload.data(), sizeof(h));
    if (msg.payload.size() < sizeof(h) + h.jsonLen) return false;
    json.assign(reinterpret_cast<const char*>(msg.payload.data()) + sizeof(h), h.jsonLen);
    return true;
}

bool WriteAgentToolCall(IWireTransport& transport, uint32_t reqId, const std::string& json) {
    if (json.size() > 0x7FFFFFFFull) return false;
    const uint32_t jsonLen = static_cast<uint32_t>(json.size());
    std::vector<uint8_t> payload(sizeof(AgentToolCallHeader) + jsonLen);
    AgentToolCallHeader h{reqId, jsonLen};
    std::memcpy(payload.data(), &h, sizeof(h));
    if (jsonLen) std::memcpy(payload.data() + sizeof(h), json.data(), jsonLen);
    return WriteMessage(transport, MsgType::AgentToolCall, payload);
}

bool ReadAgentToolCall(const Message& msg, uint32_t& reqId, std::string& json) {
    reqId = 0; json.clear();
    if (msg.type != MsgType::AgentToolCall) return false;
    if (msg.payload.size() < sizeof(AgentToolCallHeader)) return false;
    AgentToolCallHeader h; std::memcpy(&h, msg.payload.data(), sizeof(h));
    if (msg.payload.size() < sizeof(h) + h.jsonLen) return false;
    reqId = h.reqId;
    json.assign(reinterpret_cast<const char*>(msg.payload.data()) + sizeof(h), h.jsonLen);
    return true;
}

} // namespace ipc
} // namespace jk
