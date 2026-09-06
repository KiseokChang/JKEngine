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
    Close         = 9,
    // Server -> client: the surface was resized. The client must remap the
    // shared memory named in the payload (a new mapping per generation —
    // Windows file mappings cannot grow in place) and re-layout at w x h.
    ResizeSurface = 10,
    // Shell protocol (docs/28): a client registers as the desktop shell
    // (taskbar) and receives window-list snapshots; shell commands flow
    // back client -> server. This is the seed of the privileged shell API.
    ShellRegister  = 11,  // C -> S: register as the (single) shell client
    WindowList     = 12,  // S -> C: full window-list snapshot (shell + subscribers)
    WindowActivate = 13,  // C -> S: focus (+restore) a window
    ShellRegisterAck = 14, // S -> C: shell role granted (1) or denied (0)
    // C -> S: show/hide the window (taskbar active-button re-click). Same
    // {surfaceId} payload as WindowActivate.
    WindowMinimizeToggle = 15,
    // C -> S: opt in/out of WindowList snapshot pushes without claiming the
    // single shell slot — utility windows (taskmgr, docs/23 §11.2) use this.
    WindowListSubscribe = 16
};

#pragma pack(push, 1)
struct WireHeader {
    uint32_t magic = kWireMagic;
    uint32_t type  = 0;
    uint32_t length = 0;
};

// Protocol payload structures shared by server and client.
// All payloads are trivially-copyable and sent raw over the wire.

// Client -> server with MsgType::Hello. Protocol v2 adds the client's OS
// process id: window-list entries carry it so utility windows can query
// per-process stats (GetProcessTimes/GetProcessMemoryInfo) directly, keeping
// the server a dumb compositor (docs/23 §11.2). v1 Hellos (4 bytes) are
// accepted; pid then reads as 0.
struct HelloPayload {
    uint32_t protocolVersion = 2;
    uint32_t pid = 0;
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

// Server -> client with MsgType::ResizeSurface. shmName is a fresh mapping
// (Local\JKSurfaceShm_<id>_<gen>) sized w*h*4.
struct SurfaceResizePayload {
    uint32_t surfaceId = 0;
    int32_t  width = 0;
    int32_t  height = 0;
    char     shmName[256] = {};
};

// Client -> server with MsgType::ShellRegister: the sender wants to act as
// the desktop shell (taskbar). One shell at a time — the server denies later
// registrations with a ShellRegisterAck. dockEdge: 0 = bottom edge (v1);
// barHeight: requested bar thickness in logical points (0 = surface decides).
struct ShellRegisterPayload {
    uint32_t protocolVersion = 1;
    uint32_t dockEdge = 0;
    uint32_t barHeight = 0;
};

// Server -> client with MsgType::ShellRegisterAck: shell role granted
// (accepted = 1) or denied (0, e.g. another shell is already active).
struct ShellRegisterAckPayload {
    uint32_t accepted = 0;
};

// Window-list entry (docs/28). flags: bit0 = active (keyboard focus),
// bit1 = minimized (layer hidden server-side). pid: the client's OS process
// id, self-reported in Hello v2 (0 for legacy clients).
struct ShellWindowEntry {
    uint32_t surfaceId = 0;
    uint32_t flags = 0;
    uint32_t pid = 0;
    char     title[128] = {};
};

// Server -> shell client with MsgType::WindowList: a FULL snapshot. The
// receiver replaces its entire view with this list (idempotent, so a
// restarted shell heals itself from the next snapshot).
struct WindowListPayload {
    uint32_t         count = 0;
    ShellWindowEntry windows[32] = {};
};

// ShellWindowEntry::flags bits.
constexpr uint32_t kShellWindowActive = 1u << 0;     // has keyboard focus
constexpr uint32_t kShellWindowMinimized = 1u << 1;  // layer hidden server-side

// Client -> server with MsgType::WindowActivate: focus the window (and
// restore it first when it is minimized).
struct WindowActivatePayload {
    uint32_t surfaceId = 0;
};

// Client -> server with MsgType::WindowListSubscribe: opt in to (subscribe=1)
// or out of (0) WindowList snapshot pushes. Unlike the shell (excluded from
// its own list, chrome-exempt) a subscriber is a regular window and appears
// in the snapshot it receives.
struct WindowListSubscribePayload {
    uint32_t subscribe = 0;
};

enum class InputEventType : uint32_t {
    None       = 0,
    MouseMove  = 1,
    MouseDown  = 2,
    MouseUp    = 3,
    MouseWheel = 4,
    KeyDown    = 5,
    KeyUp      = 6,
    Char       = 7,
    TextEditing= 8
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
