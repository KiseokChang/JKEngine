#ifndef JKCLIENTSURFACE_H
#define JKCLIENTSURFACE_H

#include <ipc/JKPipeTransport.h>
#include <ipc/JKSharedMemory.h>
#include <ipc/JKWireProtocol.h>
#include <JKAudioCommand.h>
#include <JKEvent.h>
#include <JKTypes.h>
#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace jk {
namespace client {

// Client-side view of a surface managed by the window server.
// The surface pixels live in shared memory; this class owns the named-pipe
// connection used to synchronize creation and commits with the server.
class JKClientSurface {
public:
    JKClientSurface(const std::string& pipeName,
                    int width, int height,
                    const std::string& title);
    ~JKClientSurface();

    JKClientSurface(const JKClientSurface&) = delete;
    JKClientSurface& operator=(const JKClientSurface&) = delete;

    // Connect to the server, create the surface, and open shared memory.
    // Returns false if any step fails.
    bool Connect();

    void Close();

    bool IsConnected() const;
    bool IsValid() const;

    int Width() const { return width_; }
    int Height() const { return height_; }
    uint32_t SurfaceId() const { return surfaceId_; }

    // RGBA8888 pixel buffer. Stride is width * 4 bytes.
    uint8_t* Pixels() const;
    size_t PixelBytes() const;

    // Mark the entire surface dirty and send a CommitSurface message.
    bool CommitFull();

    // Send a CommitSurface message with the supplied dirty rectangles.
    bool Commit(const std::vector<ipc::DirtyRect>& dirty);

    // Forward an audio command to the server, which routes it to the audio thread.
    bool PostAudioCommand(const AudioCommand& cmd);

    // Drain one server-forwarded input event. Returns false if none are queued.
    bool PollInputEvent(JKEvent& out);

private:
    void StartReadThread();
    void StopReadThread();
    void ReadLoop();
    void QueueInputEvent(const JKEvent& ev);

    std::string pipeName_;
    std::string title_;
    int width_ = 0;
    int height_ = 0;

    std::unique_ptr<ipc::JKPipeTransport> transport_;
    std::unique_ptr<ipc::JKSharedMemory> sharedMemory_;
    uint32_t surfaceId_ = 0;

    std::atomic<bool> running_{false};
    std::thread readThread_;

    std::mutex inputMutex_;
    std::deque<JKEvent> inputEvents_;

    static std::string ShmNameFromSurfaceId(uint32_t id);
};

} // namespace client
} // namespace jk

#endif // JKCLIENTSURFACE_H
