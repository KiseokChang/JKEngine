#ifndef JKWIREPROTOCOL_H
#define JKWIREPROTOCOL_H

#include <cstdint>
#include <vector>
#include <string>

namespace jk {
namespace ipc {

// Wire format magic: "JK" version 1 (0x4A = 'J', 0x4B = 'K').
constexpr uint32_t kWireMagic = 0x4A4B0001;

enum class MsgType : uint32_t {
    Hello         = 1,
    HelloAck      = 2,
    CreateSurface = 3,
    SurfaceCreated= 4,
    CommitSurface = 5,
    InputEvent    = 6,
    TimerEvent    = 7,
    AudioCommand  = 8,
    Close         = 9
};

#pragma pack(push, 1)
struct WireHeader {
    uint32_t magic = kWireMagic;
    uint32_t type  = 0;
    uint32_t length = 0;
};

// Protocol payload structures shared by server and client.
// All payloads are trivially-copyable and sent raw over the wire.

struct HelloPayload {
    uint32_t protocolVersion = 1;
};

struct SurfaceCreatePayload {
    int32_t  width = 0;
    int32_t  height = 0;
    char     title[128] = {};
};

struct SurfaceCreatedPayload {
    uint32_t surfaceId = 0;
    char     shmName[256] = {};
};

struct DirtyRect {
    int32_t x = 0;
    int32_t y = 0;
    int32_t w = 0;
    int32_t h = 0;
};

struct CommitSurfaceHeader {
    uint32_t surfaceId = 0;
    uint32_t dirtyCount = 0;
    // Followed by dirtyCount DirtyRect entries.
};

struct SurfaceMovePayload {
    uint32_t surfaceId = 0;
    int32_t  x = 0;
    int32_t  y = 0;
    int32_t  width = 0;
    int32_t  height = 0;
};

enum class InputEventType : uint32_t {
    None       = 0,
    MouseMove  = 1,
    MouseDown  = 2,
    MouseUp    = 3,
    MouseWheel = 4,
    KeyDown    = 5,
    KeyUp      = 6,
    Char       = 7
};

struct InputEventPayload {
    uint32_t         surfaceId = 0;
    InputEventType   type = InputEventType::None;
    int32_t          x = 0;       // Surface-local coordinate or wheel delta.
    int32_t          y = 0;
    int32_t          dx = 0;      // Relative motion / wheel delta.
    int32_t          dy = 0;
    uint32_t         keyCode = 0; // SDL keycode or mouse button.
    uint32_t         detail = 0;  // Click count / repeat / modifiers.
    uint32_t         option = 0;
    char             text[64] = {};
};
#pragma pack(pop)

struct Message {
    MsgType type = MsgType::Close;
    std::vector<uint8_t> payload;
};

// Abstract byte transport used by the wire protocol.
// Named-pipe and socket transports implement this interface.
class IWireTransport {
public:
    virtual ~IWireTransport() = default;

    // Write exactly len bytes. Returns false on failure.
    virtual bool Write(const void* data, size_t len) = 0;

    // Read exactly len bytes. Returns false on failure or EOF.
    virtual bool Read(void* data, size_t len) = 0;

    // Close the transport. Safe to call multiple times.
    virtual void Close() = 0;

    // True if the transport is still connected.
    virtual bool IsConnected() const = 0;
};

// Send a single message. The payload may be empty.
bool WriteMessage(IWireTransport& transport, MsgType type,
                  const std::vector<uint8_t>& payload);

// Receive a single message. Blocks until a complete message arrives or the
// transport is closed.
bool ReadMessage(IWireTransport& transport, Message& out);

// Convenience helpers for fixed-size payloads.
bool WriteMessage(IWireTransport& transport, MsgType type,
                  const void* data, size_t len);

template <typename T>
bool WriteMessage(IWireTransport& transport, MsgType type, const T& payload) {
    static_assert(std::is_trivially_copyable<T>::value, "wire payload must be POD");
    return WriteMessage(transport, type, &payload, sizeof(T));
}

} // namespace ipc
} // namespace jk

#endif // JKWIREPROTOCOL_H
