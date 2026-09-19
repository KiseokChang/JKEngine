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

// One window in a shell window-list snapshot (docs/28). flags mirror
// ipc::ShellWindowEntry: bit0 = active (keyboard focus), bit1 = minimized.
// pid is the client-reported OS process id (protocol v2, 0 = unknown).
struct ShellWindowInfo {
    uint32_t surfaceId = 0;
    uint32_t flags = 0;
    uint32_t pid = 0;
    std::string title;
};

// Desktop Agent API (M2a): one AgentReply pulled off the window connection.
// ok mirrors the wire reply flag; json is the tool result body.
struct AgentReply {
    uint32_t queryId = 0;
    bool ok = false;
    std::string json;
};

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

    // Apply a server-initiated resize queued by the read thread
    // (MsgType::ResizeSurface). Main-thread only: opens the new shared memory
    // mapping and updates Width()/Height() before the caller re-lays out.
    // Returns true if a resize was applied.
    bool ApplyPendingResize();

    // Shell protocol (docs/28). A shell client registers right after
    // Connect(), then receives WindowList snapshots (JKEventType::
    // WindowListChanged queues; fetch the latest with GetWindowList).
    // WindowActivate asks the server to focus (and restore) a window.
    // dockEdge: 0 = bottom edge; barHeight: requested thickness (0 = auto).
    bool SendShellRegister(uint32_t dockEdge = 0, uint32_t barHeight = 0);
    bool SendWindowActivate(uint32_t surfaceId);
    bool SendWindowMinimizeToggle(uint32_t surfaceId);
    // Non-shell opt-in to WindowList pushes (taskmgr; the shell gets them
    // by role). Subscribing yields an immediate initial snapshot.
    bool SendWindowListSubscribe(bool subscribe);
    // C -> S: update this window's title on the server (raw UTF-8 payload,
    // 96-byte cap). Drives the taskbar button text — the notification
    // center's unread badge uses it (docs/33).
    bool SendWindowTitle(const std::string& utf8);
    // Main-thread only: copy of the latest window-list snapshot.
    bool GetWindowList(std::vector<ShellWindowInfo>& out) const;

    // Desktop Agent API over the window connection (M2a): the same pipe that
    // carries CommitSurface/InputEvent also carries AgentQuery/Reply — the
    // server's ProcessClientMessage answers any client ("one API, many
    // faces"). Replies and subscribed events queue in the read loop and are
    // polled on the frame loop.
    bool SendAgentQuery(uint32_t queryId, const std::string& json);
    bool SendAgentEventSubscribe(bool subscribe);
    // Drain one queued AgentReply. Returns false if none are queued.
    bool PollAgentReply(AgentReply& out);
    // Drain all queued agent events (raw JSON bodies); returns the count.
    size_t DrainAgentEvents(std::vector<std::string>& out);

    // 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §8.2): 런치 시 자기 도구를
    // 선언하고, 서버 중계 호출(AgentToolCall)을 프레임 루프에서 폴링한다.
    struct AgentToolDecl {
        std::string name, description, inputSchema;  // inputSchema = 원문 JSON
    };
    struct AgentToolCallMsg {
        uint32_t reqId = 0;
        std::string app, tool, args;
    };
    bool SendAgentToolRegister(const std::string& app,
                               const std::vector<AgentToolDecl>& tools,
                               bool modal = false);
        // modal (스펙 2026-09-19-filedlg-voice-nav §0 결정 3/§4): 쿼리-수명
        // 모달 앱(filedlg) 표기 — top-level "modal":true (부재=false). 서버
        // HandleToolRegister가 GetInt("modal")로 판독(quickjs가 JSON bool을
        // 0/1로 수용). 기존 발신자(vplayer)는 기본 false라 와이어 무변경.
    bool PollToolCall(AgentToolCallMsg& out);
    bool SendAgentToolResult(uint32_t reqId, bool ok, const std::string& resultJson);

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

    // Latest server-initiated resize, coalesced by the read thread and
    // applied on the main thread via ApplyPendingResize().
    struct PendingResize {
        bool valid = false;
        int width = 0;
        int height = 0;
        std::string shmName;
    };
    std::mutex pendingResizeMutex_;
    PendingResize pendingResize_;

    // Latest window-list snapshot pushed by the server (MsgType::WindowList),
    // coalesced by the read thread; GetWindowList() copies it on the main
    // thread. Same pattern as pendingResize_.
    struct PendingWindowList {
        bool valid = false;
        std::vector<ShellWindowInfo> windows;
    };
    mutable std::mutex pendingWindowListMutex_;
    PendingWindowList pendingWindowList_;

    // Agent channel (M2a): queued by the read loop, drained on the main
    // thread. Bounded the same way as inputEvents_.
    std::deque<AgentReply> pendingAgentReplies_;
    std::mutex agentReplyMutex_;
    std::deque<std::string> pendingAgentEvents_;
    std::mutex agentEventMutex_;

    // 앱 도구 허브: ReadLoop가 적재, 메인 스레드가 PollToolCall로 소비.
    std::deque<AgentToolCallMsg> pendingToolCalls_;
    std::mutex agentToolMutex_;

    static std::string ShmNameFromSurfaceId(uint32_t id);
};

} // namespace client
} // namespace jk

#endif // JKCLIENTSURFACE_H
