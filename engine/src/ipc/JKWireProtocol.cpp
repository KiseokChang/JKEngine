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

} // namespace ipc
} // namespace jk
