#ifndef JKCLIENTCONNECTION_H
#define JKCLIENTCONNECTION_H

#include <ipc/JKPipeTransport.h>
#include <ipc/JKSharedMemory.h>
#include <ipc/JKWireProtocol.h>
#include <JKTypes.h>
#include <SDL.h>

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace jk {
namespace server {

// Server-side state for one connected client process.
// Owns the named-pipe transport, the shared-memory surface backing, and the
// message queue fed by the per-client read thread.
class JKClientConnection {
public:
    JKClientConnection(uint32_t id, std::unique_ptr<ipc::JKPipeTransport> transport);
    ~JKClientConnection();

    JKClientConnection(const JKClientConnection&) = delete;
    JKClientConnection& operator=(const JKClientConnection&) = delete;

    uint32_t Id() const { return id_; }
    bool IsConnected() const;
    bool IsDisconnected() const { return disconnected_.load(); }

    // Create the shared memory segment for the surface. Returns false on failure.
    bool CreateSurface(int width, int height, const std::string& title);

    // Begin a server-initiated resize: create a fresh shared memory mapping
    // (Windows file mappings cannot grow in place, so each generation gets a
    // new name), retire the old one (the compositor's pixel pointer may still
    // point into it until ResizeLayer swaps it), and fill the payload the
    // caller sends to the client. Returns false if the new mapping failed.
    bool BeginResizeSurface(int width, int height,
                            ipc::SurfaceResizePayload& outPayload);

    // Accessors for the compositor.
    int Width() const { return width_; }
    int Height() const { return height_; }
    const std::string& Title() const { return title_; }
    int X() const { return x_; }
    int Y() const { return y_; }
    void SetPosition(int x, int y) { x_ = x; y_ = y; }

    // Shell role (docs/28): granted via MsgType::ShellRegister. Exactly one
    // connected client may act as the desktop shell (taskbar).
    bool IsShell() const { return shell_; }
    void SetShell(bool shell) { shell_ = shell; }

    // Client-reported OS process id (Hello protocol v2). 0 when unknown
    // (legacy client). Consumed by window-list entries (docs/23 §11.2).
    uint32_t Pid() const { return pid_; }
    void SetPid(uint32_t pid) { pid_ = pid; }

    // WindowList push subscription (MsgType::WindowListSubscribe): how
    // non-shell utility windows (taskmgr) receive snapshots. The shell
    // always receives them by role.
    bool WantsWindowList() const { return windowListSubscriber_; }
    void SetWindowListSubscriber(bool s) { windowListSubscriber_ = s; }

    // Control-only connection (Desktop Agent API, spec §3): no surface, no
    // shared memory, no compositor layer — the pipe is the whole relationship
    // (agentctl, jkagentd). Excluded from window lists and placement.
    bool IsControlOnly() const { return controlOnly_; }
    void SetControlOnly(bool c) { controlOnly_ = c; }

    // AgentEvent push subscription (MsgType::AgentEventSubscribe). Meaningful
    // for control-only clients; regular windows ignore desktop event pushes.
    bool AgentEventSubscriber() const { return agentEventSubscriber_; }
    void SetAgentEventSubscriber(bool s) { agentEventSubscriber_ = s; }

    // Raw transport access — server-initiated sends to this client that do
    // not fit the fixed-POD Send helpers (e.g. agent JSON replies, Close).
    ipc::IWireTransport& Transport() { return *transport_; }

    uint8_t* SurfaceData() const;
    size_t SurfaceBytes() const;

    // Texture management. texture is owned by the server main thread.
    SDL_Texture* GetTexture() const { return texture_; }
    void SetTexture(SDL_Texture* texture) { texture_ = texture; }
    bool IsDirty() const { return dirty_.load(); }
    void MarkDirty() { dirty_ = true; }
    void ClearDirty() { dirty_ = false; }

    // Message queue used by the read thread and the server main thread.
    void QueueMessage(ipc::Message msg);
    bool PopMessage(ipc::Message& out);

    // Start/stop the read thread.
    void StartReadThread();
    void StopReadThread();

    // Send a message to the client.
    bool Send(const ipc::Message& msg);
    bool Send(ipc::MsgType type, const void* data, size_t len);

private:
    void ReadLoop();

    uint32_t id_ = 0;
    std::unique_ptr<ipc::JKPipeTransport> transport_;
    std::unique_ptr<ipc::JKSharedMemory> memory_;
    // Retired shared memory generations. The compositor layer's pixel pointer
    // may still reference a retired mapping until ResizeLayer swaps it, so
    // they are kept alive until this connection is destroyed.
    std::vector<std::unique_ptr<ipc::JKSharedMemory>> retiredMemories_;
    uint32_t shmGeneration_ = 0;

    int width_ = 0;
    int height_ = 0;
    std::string title_;
    int x_ = 0;
    int y_ = 0;
    bool shell_ = false;   // desktop-shell (taskbar) role, granted on request
    uint32_t pid_ = 0;     // client-reported OS pid (Hello v2)
    bool windowListSubscriber_ = false;  // opted into WindowList pushes
    bool controlOnly_ = false;           // agent connection: no surface/layer
    bool agentEventSubscriber_ = false;  // opted into AgentEvent pushes

    SDL_Texture* texture_ = nullptr;
    std::atomic<bool> dirty_{true};
    std::atomic<bool> running_{true};
    std::atomic<bool> disconnected_{false};

    std::mutex messageMutex_;
    std::deque<ipc::Message> messages_;
    std::thread readThread_;
};

} // namespace server
} // namespace jk

#endif // JKCLIENTCONNECTION_H
