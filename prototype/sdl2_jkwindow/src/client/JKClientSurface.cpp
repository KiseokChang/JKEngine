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

    StartReadThread();
    return true;
}

void JKClientSurface::Close() {
    StopReadThread();
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

bool JKClientSurface::PostAudioCommand(const AudioCommand& cmd) {
    if (!IsConnected()) return false;
    return ipc::WriteMessage(*transport_, ipc::MsgType::AudioCommand, &cmd, sizeof(cmd));
}

bool JKClientSurface::PollInputEvent(JKEvent& out) {
    std::lock_guard<std::mutex> lock(inputMutex_);
    if (inputEvents_.empty()) return false;
    out = std::move(inputEvents_.front());
    inputEvents_.pop_front();
    return true;
}

void JKClientSurface::StartReadThread() {
    if (readThread_.joinable()) return;
    running_ = true;
    readThread_ = std::thread([this] { ReadLoop(); });
}

void JKClientSurface::StopReadThread() {
    running_ = false;
    if (transport_) transport_->Close();
    if (readThread_.joinable()) readThread_.join();
}

void JKClientSurface::ReadLoop() {
    if (!transport_) return;

    while (running_ && transport_->IsConnected()) {
        ipc::Message msg;
        if (!ipc::ReadMessage(*transport_, msg)) {
            break;
        }
        if (msg.type == ipc::MsgType::Close) {
            // Notify the client application so its main loop exits cleanly.
            JKEvent quit{};
            quit.type = JKEventType::Quit;
            QueueInputEvent(quit);
            break;
        }
        if (msg.type == ipc::MsgType::InputEvent &&
            msg.payload.size() >= sizeof(ipc::InputEventPayload)) {
            ipc::InputEventPayload payload{};
            std::memcpy(&payload, msg.payload.data(), sizeof(payload));

            JKEvent ev{};
            ev.winId = payload.surfaceId;
            ev.x = payload.x;
            ev.y = payload.y;
            ev.dx = payload.dx;
            ev.dy = payload.dy;
            ev.keyCode = payload.keyCode;
            ev.detail = payload.detail;
            ev.option = payload.option;
            std::strncpy(ev.text, payload.text, sizeof(ev.text) - 1);

            switch (payload.type) {
                case ipc::InputEventType::MouseMove:  ev.type = JKEventType::MouseMove; break;
                case ipc::InputEventType::MouseDown:  ev.type = JKEventType::MouseDown; break;
                case ipc::InputEventType::MouseUp:    ev.type = JKEventType::MouseUp; break;
                case ipc::InputEventType::MouseWheel: ev.type = JKEventType::MouseWheel; break;
                case ipc::InputEventType::KeyDown:    ev.type = JKEventType::KeyDown; break;
                case ipc::InputEventType::KeyUp:      ev.type = JKEventType::KeyUp; break;
                case ipc::InputEventType::Char:       ev.type = JKEventType::Char; break;
                default:                              ev.type = JKEventType::None; break;
            }

            QueueInputEvent(ev);
        }
    }

    running_ = false;
    if (transport_) transport_->Close();
}

void JKClientSurface::QueueInputEvent(const JKEvent& ev) {
    std::lock_guard<std::mutex> lock(inputMutex_);
    // Bound queue size to avoid unbounded growth under heavy input.
    if (inputEvents_.size() >= 256) {
        inputEvents_.pop_front();
    }
    inputEvents_.push_back(ev);
}

std::string JKClientSurface::ShmNameFromSurfaceId(uint32_t id) {
    return std::string("JKSurfaceShm_") + std::to_string(id);
}

} // namespace client
} // namespace jk
