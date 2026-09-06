#include <server/JKClientConnection.h>

#include <cstdio>
#include <cstring>

namespace jk {
namespace server {

JKClientConnection::JKClientConnection(uint32_t id,
                                       std::unique_ptr<ipc::JKPipeTransport> transport)
    : id_(id), transport_(std::move(transport)) {
}

JKClientConnection::~JKClientConnection() {
    StopReadThread();
    if (transport_) transport_->Close();
}

bool JKClientConnection::IsConnected() const {
    return transport_ && transport_->IsConnected();
}

bool JKClientConnection::CreateSurface(int width, int height, const std::string& title) {
    if (width <= 0 || height <= 0) return false;

    width_ = width;
    height_ = height;
    title_ = title;

    std::string shmName = std::string("Local\\JKSurfaceShm_") + std::to_string(id_);
    memory_ = std::make_unique<ipc::JKSharedMemory>();
    const size_t bytes = static_cast<size_t>(width_) * height_ * 4;
    if (!memory_->Create(shmName, bytes)) {
        std::fprintf(stderr, "JKClientConnection[%u]: failed to create shared memory\n", id_);
        return false;
    }
    if (memory_->Data()) {
        std::memset(memory_->Data(), 0, bytes);
    }
    dirty_ = true;
    return true;
}

uint8_t* JKClientConnection::SurfaceData() const {
    return memory_ ? memory_->Data() : nullptr;
}

bool JKClientConnection::BeginResizeSurface(int width, int height,
                                            ipc::SurfaceResizePayload& outPayload) {
    if (width <= 0 || height <= 0) return false;

    auto fresh = std::make_unique<ipc::JKSharedMemory>();
    const std::string shmName = std::string("Local\\JKSurfaceShm_")
        + std::to_string(id_) + "_" + std::to_string(++shmGeneration_);
    const size_t bytes = static_cast<size_t>(width) * height * 4;
    if (!fresh->Create(shmName, bytes)) {
        std::fprintf(stderr, "JKClientConnection[%u]: failed to create resized shared memory\n",
                     id_);
        return false;
    }
    if (fresh->Data()) {
        std::memset(fresh->Data(), 0, bytes);
    }

    if (memory_) {
        retiredMemories_.push_back(std::move(memory_));
    }
    memory_ = std::move(fresh);
    width_ = width;
    height_ = height;

    ipc::SurfaceResizePayload payload{};
    payload.surfaceId = id_;
    payload.width = width;
    payload.height = height;
    std::strncpy(payload.shmName, shmName.c_str(), sizeof(payload.shmName) - 1);
    outPayload = payload;
    return true;
}

size_t JKClientConnection::SurfaceBytes() const {
    return static_cast<size_t>(width_) * height_ * 4;
}

void JKClientConnection::QueueMessage(ipc::Message msg) {
    std::lock_guard<std::mutex> lock(messageMutex_);
    messages_.push_back(std::move(msg));
}

bool JKClientConnection::PopMessage(ipc::Message& out) {
    std::lock_guard<std::mutex> lock(messageMutex_);
    if (messages_.empty()) return false;
    out = std::move(messages_.front());
    messages_.pop_front();
    return true;
}

void JKClientConnection::StartReadThread() {
    if (readThread_.joinable()) return;
    running_ = true;
    readThread_ = std::thread([this] { ReadLoop(); });
}

void JKClientConnection::StopReadThread() {
    running_ = false;
    // Wake a reader parked in a pending overlapped ReadFile FIRST, join it,
    // and only then close the handle. Closing (or CancelIo-less teardown)
    // while the read thread may still touch the handle or its stack OVERLAPPED
    // is undefined behavior — killing several clients at once made the main
    // thread race each reader's broken-pipe exit and crash the server.
    if (transport_) transport_->CancelPendingIo();
    if (readThread_.joinable()) readThread_.join();
    if (transport_) transport_->Close();
}

bool JKClientConnection::Send(const ipc::Message& msg) {
    if (!transport_) return false;
    return ipc::WriteMessage(*transport_, msg.type, msg.payload);
}

bool JKClientConnection::Send(ipc::MsgType type, const void* data, size_t len) {
    if (!transport_) return false;
    return ipc::WriteMessage(*transport_, type, data, len);
}

void JKClientConnection::ReadLoop() {
    if (!transport_) return;

    while (running_ && transport_->IsConnected()) {
        ipc::Message msg;
        if (!ipc::ReadMessage(*transport_, msg)) {
            break;
        }
        if (msg.type == ipc::MsgType::Close) {
            break;
        }
        QueueMessage(std::move(msg));
    }

    disconnected_ = true;
    // Do NOT close the transport here: the server main thread may be inside
    // an overlapped WriteFile on the same handle, and CloseHandle under an
    // in-flight I/O is undefined behavior. The writer self-closes on failure.
}

} // namespace server
} // namespace jk
