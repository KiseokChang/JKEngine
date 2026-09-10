#include <client/JKClientSurface.h>

#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#endif

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

    // Send Hello. Protocol v2 self-reports this process's OS pid so window
    // lists can attribute per-process stats without server involvement.
    {
        ipc::HelloPayload hello{};
        hello.protocolVersion = 2;
#ifdef _WIN32
        hello.pid = ::GetCurrentProcessId();
#endif
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
    // Same discipline as the server's JKClientConnection::StopReadThread:
    // unblock the parked reader, join it, and only then close the handle.
    if (transport_) transport_->CancelPendingIo();
    if (readThread_.joinable()) readThread_.join();
    if (transport_) transport_->Close();
}

void JKClientSurface::ReadLoop() {
    if (!transport_) return;

    while (running_ && transport_->IsConnected()) {
        ipc::Message msg;
        if (!ipc::ReadMessage(*transport_, msg)) {
            // Read() only fails when the transport is dead (broken pipe) or
            // cancelled. Cancel path sets running_ = false before unblocking
            // the reader, so a failure here while running_ means the server
            // died without sending Close (crash / taskkill). The client has
            // nothing to render to once the server is gone — queue Quit so
            // the app loop exits instead of lingering as an invisible
            // orphan process.
            if (running_) {
                JKEvent quit{};
                quit.type = JKEventType::Quit;
                QueueInputEvent(quit);
            }
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
                case ipc::InputEventType::MouseDown:
                case ipc::InputEventType::MouseUp: {
                    ev.type = (payload.type == ipc::InputEventType::MouseDown)
                                  ? JKEventType::MouseDown
                                  : JKEventType::MouseUp;
                    // Wire puts the SDL button in keyCode and the click count in
                    // detail. The JKEvent convention (TranslateSDLEvent, the
                    // single-process path and the whole control library) carries
                    // the BUTTON in detail — map here so client apps behave
                    // identically in both paths. Without this, left clicks only
                    // worked by coincidence (1 click == SDL_BUTTON_LEFT) and a
                    // right click was read as left (e.g. minesweeper opened
                    // instead of flagging).
                    ev.detail = payload.keyCode;   // SDL button
                    ev.keyCode = payload.detail;   // click count
                    break;
                }
                case ipc::InputEventType::MouseWheel: ev.type = JKEventType::MouseWheel; break;
                case ipc::InputEventType::KeyDown:    ev.type = JKEventType::KeyDown; break;
                case ipc::InputEventType::KeyUp:      ev.type = JKEventType::KeyUp; break;
                case ipc::InputEventType::Char:       ev.type = JKEventType::Char; break;
                case ipc::InputEventType::TextEditing:
                    ev.type = JKEventType::TextEditing;
                    ev.editStart = static_cast<int32_t>(payload.detail);
                    ev.editLength = static_cast<int32_t>(payload.option);
                    break;
                default:                              ev.type = JKEventType::None; break;
            }

            QueueInputEvent(ev);
        } else if (msg.type == ipc::MsgType::ResizeSurface &&
                   msg.payload.size() >= sizeof(ipc::SurfaceResizePayload)) {
            // Server-initiated resize. Coalesce: keep only the latest request;
            // the main thread applies it when it drains the SizeChanged event.
            ipc::SurfaceResizePayload payload{};
            std::memcpy(&payload, msg.payload.data(), sizeof(payload));

            JKEvent resize{};
            resize.type = JKEventType::SizeChanged;
            resize.x = payload.width;
            resize.y = payload.height;
            QueueInputEvent(resize);

            std::lock_guard<std::mutex> lock(pendingResizeMutex_);
            pendingResize_.valid = true;
            pendingResize_.width = payload.width;
            pendingResize_.height = payload.height;
            pendingResize_.shmName = payload.shmName;
        } else if (msg.type == ipc::MsgType::WindowList &&
                   msg.payload.size() >= sizeof(ipc::WindowListPayload)) {
            // Shell protocol: full snapshot. Coalesce (keep only the latest —
            // snapshots supersede each other) and wake the app with a light
            // event; it pulls the list with GetWindowList().
            ipc::WindowListPayload payload{};
            std::memcpy(&payload, msg.payload.data(), sizeof(payload));

            PendingWindowList snapshot;
            snapshot.valid = true;
            snapshot.windows.reserve(payload.count);
            for (uint32_t i = 0; i < payload.count && i < 32; ++i) {
                ShellWindowInfo info;
                info.surfaceId = payload.windows[i].surfaceId;
                info.flags = payload.windows[i].flags;
                info.pid = payload.windows[i].pid;
                info.title = payload.windows[i].title;
                snapshot.windows.push_back(std::move(info));
            }

            JKEvent ev{};
            ev.type = JKEventType::WindowListChanged;
            QueueInputEvent(ev);

            std::lock_guard<std::mutex> lock(pendingWindowListMutex_);
            pendingWindowList_ = std::move(snapshot);
        } else if (msg.type == ipc::MsgType::ShellRegisterAck &&
                   msg.payload.size() >= sizeof(ipc::ShellRegisterAckPayload)) {
            // One-shot handshake result. Denied means another shell is
            // active; the app sees no WindowList and stays inert (log-only
            // for now — v1 runs a single auto-spawned shell).
            ipc::ShellRegisterAckPayload payload{};
            std::memcpy(&payload, msg.payload.data(), sizeof(payload));
            std::fprintf(stderr, "[surface] shell register %s\n",
                         payload.accepted ? "accepted" : "DENIED");
            std::fflush(stderr);
        } else if (msg.type == ipc::MsgType::AgentReply ||
                   msg.type == ipc::MsgType::AgentEvent) {
            // Desktop Agent API (M2a): coalesce into per-kind queues and wake
            // the app; it polls PollAgentReply/DrainAgentEvents on the frame
            // loop (same queue + light-event pattern as WindowList).
            uint32_t queryId = 0, ok = 0;
            std::string json;
            if (ipc::ReadAgentJson(msg, queryId, ok, json)) {
                JKEvent ev{};
                ev.type = JKEventType::AgentReply;
                if (msg.type == ipc::MsgType::AgentReply) {
                    AgentReply reply{ queryId, ok != 0, std::move(json) };
                    std::lock_guard<std::mutex> lock(agentReplyMutex_);
                    if (pendingAgentReplies_.size() >= 64) {
                        pendingAgentReplies_.pop_front();
                    }
                    pendingAgentReplies_.push_back(std::move(reply));
                    ev.keyCode = queryId;   // reply wake carries its query id
                } else {
                    std::lock_guard<std::mutex> lock(agentEventMutex_);
                    if (pendingAgentEvents_.size() >= 256) {
                        pendingAgentEvents_.pop_front();
                    }
                    pendingAgentEvents_.push_back(std::move(json));
                }
                QueueInputEvent(ev);
            }
        }
    }

    running_ = false;
    // Do NOT close the transport here: the main thread may be inside an
    // overlapped WriteFile on the same handle (CommitSurface), and
    // CloseHandle under in-flight I/O is undefined behavior. The writer
    // self-closes on failure.
}

void JKClientSurface::QueueInputEvent(const JKEvent& ev) {
    std::lock_guard<std::mutex> lock(inputMutex_);
    // Bound queue size to avoid unbounded growth under heavy input.
    if (inputEvents_.size() >= 256) {
        inputEvents_.pop_front();
    }
    inputEvents_.push_back(ev);
}

bool JKClientSurface::ApplyPendingResize() {
    PendingResize pending;
    {
        std::lock_guard<std::mutex> lock(pendingResizeMutex_);
        if (!pendingResize_.valid) {
            return false;
        }
        pending = pendingResize_;
        pendingResize_.valid = false;
    }
    if (pending.width <= 0 || pending.height <= 0) {
        return false;
    }

    // Open the new mapping in a temporary object first: reusing sharedMemory_
    // with Close-then-Open would leave it unusable if the Open failed (Open
    // refuses while a handle exists). On success, adopt the new mapping.
    auto fresh = std::make_unique<ipc::JKSharedMemory>();
    const size_t bytes = static_cast<size_t>(pending.width) * pending.height * 4;
    if (!fresh->Open(pending.shmName, bytes)) {
        std::fprintf(stderr, "JKClientSurface::ApplyPendingResize: failed to open %s (%dx%d)\n",
                     pending.shmName.c_str(), pending.width, pending.height);
        return false;
    }
    sharedMemory_ = std::move(fresh);
    width_ = pending.width;
    height_ = pending.height;
    return true;
}

std::string JKClientSurface::ShmNameFromSurfaceId(uint32_t id) {
    return std::string("JKSurfaceShm_") + std::to_string(id);
}

bool JKClientSurface::SendShellRegister(uint32_t dockEdge, uint32_t barHeight) {
    if (!IsConnected()) return false;
    ipc::ShellRegisterPayload payload{};
    payload.protocolVersion = 1;
    payload.dockEdge = dockEdge;
    payload.barHeight = barHeight;
    return ipc::WriteMessage(*transport_, ipc::MsgType::ShellRegister, payload);
}

bool JKClientSurface::SendWindowActivate(uint32_t surfaceId) {
    if (!IsConnected()) return false;
    ipc::WindowActivatePayload payload{};
    payload.surfaceId = surfaceId;
    return ipc::WriteMessage(*transport_, ipc::MsgType::WindowActivate, payload);
}

bool JKClientSurface::SendWindowMinimizeToggle(uint32_t surfaceId) {
    if (!IsConnected()) return false;
    ipc::WindowActivatePayload payload{};
    payload.surfaceId = surfaceId;
    return ipc::WriteMessage(*transport_, ipc::MsgType::WindowMinimizeToggle, payload);
}

bool JKClientSurface::SendWindowListSubscribe(bool subscribe) {
    if (!IsConnected()) return false;
    ipc::WindowListSubscribePayload payload{};
    payload.subscribe = subscribe ? 1 : 0;
    return ipc::WriteMessage(*transport_, ipc::MsgType::WindowListSubscribe, payload);
}

bool JKClientSurface::GetWindowList(std::vector<ShellWindowInfo>& out) const {
    std::lock_guard<std::mutex> lock(pendingWindowListMutex_);
    if (!pendingWindowList_.valid) {
        return false;
    }
    out = pendingWindowList_.windows;
    return true;
}

bool JKClientSurface::SendAgentQuery(uint32_t queryId, const std::string& json) {
    if (!IsConnected()) return false;
    return ipc::WriteAgentJson(*transport_, ipc::MsgType::AgentQuery,
                               queryId, 0, json);
}

bool JKClientSurface::SendAgentEventSubscribe(bool subscribe) {
    if (!IsConnected()) return false;
    ipc::AgentEventSubscribePayload payload{};
    payload.subscribe = subscribe ? 1 : 0;
    return ipc::WriteMessage(*transport_, ipc::MsgType::AgentEventSubscribe,
                             &payload, sizeof(payload));
}

bool JKClientSurface::SendWindowTitle(const std::string& utf8) {
    if (!IsConnected()) return false;
    if (utf8.size() > 96) return false;   // server cap — don't send garbage
    return ipc::WriteMessage(
        *transport_, ipc::MsgType::WindowTitle,
        std::vector<uint8_t>(utf8.begin(), utf8.end()));
}

bool JKClientSurface::PollAgentReply(AgentReply& out) {
    std::lock_guard<std::mutex> lock(agentReplyMutex_);
    if (pendingAgentReplies_.empty()) return false;
    out = std::move(pendingAgentReplies_.front());
    pendingAgentReplies_.pop_front();
    return true;
}

size_t JKClientSurface::DrainAgentEvents(std::vector<std::string>& out) {
    std::lock_guard<std::mutex> lock(agentEventMutex_);
    out.clear();
    while (!pendingAgentEvents_.empty()) {
        out.push_back(std::move(pendingAgentEvents_.front()));
        pendingAgentEvents_.pop_front();
    }
    return out.size();
}

} // namespace client
} // namespace jk
