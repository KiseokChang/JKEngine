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
