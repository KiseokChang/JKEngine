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
    if (transport_) transport_->Close();
    if (readThread_.joinable()) readThread_.join();
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
    if (transport_) transport_->Close();
}

} // namespace server
} // namespace jk
