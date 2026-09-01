#include <client/JKClientSurface.h>

#include <cstdio>
#include <cstring>

namespace jk {
namespace client {

JKClientSurface::JKClientSurface(const std::string& pipeName,
                                 int width, int height,
                                 const std::string& title)
    : pipeName_(pipeName), title_(title), width_(width), height_(height) {
}

JKClientSurface::~JKClientSurface() {
    Close();
}

bool JKClientSurface::Connect() {
    if (transport_) return false;

    transport_ = ipc::JKPipeTransport::ConnectClient(pipeName_);
    if (!transport_ || !transport_->IsConnected()) {
        std::fprintf(stderr, "JKClientSurface: failed to connect to '%s'\n",
                     pipeName_.c_str());
        transport_.reset();
        return false;
    }

    // Send Hello.
    {
        ipc::HelloPayload hello{1};
        if (!ipc::WriteMessage(*transport_, ipc::MsgType::Hello, hello)) {
            std::fprintf(stderr, "JKClientSurface: Hello write failed\n");
            Close();
            return false;
        }
    }

    // Send CreateSurface.
    {
        ipc::SurfaceCreatePayload create{};
        create.width = width_;
        create.height = height_;
        std::strncpy(create.title, title_.c_str(), sizeof(create.title) - 1);
        if (!ipc::WriteMessage(*transport_, ipc::MsgType::CreateSurface, create)) {
            std::fprintf(stderr, "JKClientSurface: CreateSurface write failed\n");
            Close();
            return false;
        }
    }

    // Wait for SurfaceCreated.
    ipc::Message msg;
    if (!ipc::ReadMessage(*transport_, msg)) {
        std::fprintf(stderr, "JKClientSurface: SurfaceCreated read failed\n");
        Close();
        return false;
    }
    if (msg.type != ipc::MsgType::SurfaceCreated ||
        msg.payload.size() < sizeof(ipc::SurfaceCreatedPayload)) {
        std::fprintf(stderr, "JKClientSurface: unexpected message type=%u size=%zu\n",
                     static_cast<uint32_t>(msg.type), msg.payload.size());
        Close();
        return false;
    }

    ipc::SurfaceCreatedPayload created{};
    std::memcpy(&created, msg.payload.data(), sizeof(created));
    surfaceId_ = created.surfaceId;

    // Open shared memory.
    const size_t bytes = static_cast<size_t>(width_) * height_ * 4;
    sharedMemory_ = std::make_unique<ipc::JKSharedMemory>();
    if (!sharedMemory_->Open(created.shmName, bytes)) {
        std::fprintf(stderr, "JKClientSurface: failed to open shared memory '%s'\n",
                     created.shmName);
        Close();
        return false;
    }

    std::fprintf(stderr, "JKClientSurface: connected surfaceId=%u size=%dx%d\n",
                 surfaceId_, width_, height_);
    return true;
}

void JKClientSurface::Close() {
    if (transport_ && transport_->IsConnected()) {
        ipc::WriteMessage(*transport_, ipc::MsgType::Close,
                          nullptr, 0);
    }
    sharedMemory_.reset();
    transport_.reset();
    surfaceId_ = 0;
}

bool JKClientSurface::IsConnected() const {
    return transport_ && transport_->IsConnected();
}

bool JKClientSurface::IsValid() const {
    return IsConnected() && sharedMemory_ && sharedMemory_->IsValid();
}

uint8_t* JKClientSurface::Pixels() const {
    return sharedMemory_ ? sharedMemory_->Data() : nullptr;
}

size_t JKClientSurface::PixelBytes() const {
    return static_cast<size_t>(width_) * height_ * 4;
}

bool JKClientSurface::CommitFull() {
    ipc::DirtyRect full{};
    full.x = 0;
    full.y = 0;
    full.w = width_;
    full.h = height_;
    return Commit({full});
}

bool JKClientSurface::Commit(const std::vector<ipc::DirtyRect>& dirty) {
    if (!IsConnected()) return false;

    const size_t headerBytes = sizeof(ipc::CommitSurfaceHeader);
    const size_t rectBytes = dirty.size() * sizeof(ipc::DirtyRect);
    std::vector<uint8_t> payload(headerBytes + rectBytes);

    auto* header = reinterpret_cast<ipc::CommitSurfaceHeader*>(payload.data());
    header->surfaceId = surfaceId_;
    header->dirtyCount = static_cast<uint32_t>(dirty.size());

    if (!dirty.empty()) {
        std::memcpy(payload.data() + headerBytes, dirty.data(), rectBytes);
    }

    return ipc::WriteMessage(*transport_, ipc::MsgType::CommitSurface, payload);
}

std::string JKClientSurface::ShmNameFromSurfaceId(uint32_t id) {
    return std::string("JKSurfaceShm_") + std::to_string(id);
}

} // namespace client
} // namespace jk
