#ifndef JKWINDOWSERVER_H
#define JKWINDOWSERVER_H

#include <server/JKClientConnection.h>
#include <server/JKCompositor.h>
#include <JKAudioCommand.h>
#include <JKTypes.h>
#include <SDL.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace jk {

class JKMessageBus;
class JKAudioThread;
struct LoadedImage;

namespace desktop { class JKDesktopShell; }

namespace server {

// M2 chat: what the server-side permission gate says about a tool.
// Ask = park the query until the chat window's inline approval resolves it.
enum class AgentDecision { Allow, Ask, Deny };

// Single-display window server.
// Owns the SDL window and renderer, accepts clients over a named pipe,
// creates shared-memory surfaces for them, and composites the surfaces to
// the screen on the server main thread.
class JKWindowServer {
public:
    JKWindowServer();
    ~JKWindowServer();

    bool Init(const std::string& title, int width, int height);

    // Start the background acceptor thread. Must be called before Run().
    void StartAcceptor(const std::string& pipeName);

    // Run the server main loop. This thread owns the SDL renderer and must
    // be the one that calls Init/StartAcceptor.
    void Run();

    // Signal the server to stop and unblock the acceptor thread if necessary.
    void Stop();

    // P1 ③: which exe hosts client modules (SpawnClient builds its command
    // line from this). The server exe name and the client host name are
    // different concerns — jkwinserver.exe spawns jkdesktop.exe clients.
    void SetClientHostExe(const std::string& exeName) { clientHostExe_ = exeName; }

private:
    void AcceptorLoop();
    void ProcessPendingClients();
    void ProcessPendingMessages();
    void ProcessClientMessage(JKClientConnection& client, const ipc::Message& msg);
    // Desktop Agent API (spec §3): route one AgentQuery to its tool and send
    // the AgentReply. Called with clientsMutex_ held (ProcessPendingMessages).
    void HandleAgentQuery(JKClientConnection& client, uint32_t queryId,
                          const std::string& json);
    // Push one desktop event JSON to subscribed clients (control-only agent
    // connections plus any window client that opted in).
    // Callers hold clientsMutex_.
    void PushAgentEvent(const char* topic, uint32_t id,
                        const std::string& title, uint32_t pid);
    // Push a fully-formed agent event JSON (topic included) to subscribers.
    // Server-loop-thread only: call sites either hold clientsMutex_
    // (ProcessPendingMessages / HandleAgentQuery / CleanupDisconnectedClients)
    // or run on the SDL event path where clients_ is only mutated by this
    // same thread (TryChromeGrab chrome events, e.g. docs/39 maximize).
    void PushAgentEventJson(const std::string& json);
    // docs/39: window.maximized / window.restored envelope — id/title at top
    // level like the PushAgentEvent sites, minus pid (no process change).
    void PushMaximizeEvent(const char* topic, JKClientConnection& client);
    // M2a server-side permission gate: close_window is denied by default;
    // <exeDir>\permissions.json (the same file the broker reads) is the
    // approval act — it gates any connected face, not just the broker.
    // M2 chat: "ask" values become AgentDecision::Ask (inline approval).
    AgentDecision AgentToolAllowed(const std::string& tool) const;
    // Alt+Space (spec §6.2): focus the palette if one is open, else spawn it.
    void TogglePalette();
    // Generalized title-match toggle core (docs/33): focus-or-spawn any
    // client by its surface title; app is the SpawnClient argument when not
    // open. Caller holds clientsMutex_ (AgentQuery hot path precondition) —
    // returns true when an existing client was focused.
    bool ToggleClientByTitleUnsafe(const char* title, const char* app);
    // <exeDir>/state directory for agent-created files (layout snapshots).
    // Created on first use; returns the path.
    std::string StateDir() const;
    // Shell protocol (docs/28): build a full window-list snapshot from the
    // client table and send it to the shell client.
    void PushWindowList();         // takes clientsMutex_
    void PushWindowListUnsafe();   // caller already holds clientsMutex_
    // Dock the shell layer to the bottom edge (full desktop width, work-area
    // reserve). Pass the shell connection, or nullptr to look it up.
    void DockShellClient(JKClientConnection* shell);
    JKClientConnection* FindShellClient();  // takes clientsMutex_
    void HandleSDLEvent(const SDL_Event& ev);
    void SendInputEvent(JKClientConnection& client, const ipc::InputEventPayload& payload);
    // Window chrome (title-bar move / close button / border resize).
    // HandleChromeGrab applies an in-progress grab and consumes the event;
    // TryChromeGrab starts a grab (or performs a close click) on MouseDown.
    bool HandleChromeGrab(const SDL_Event& ev, int mx, int my, float scale);
    // `clicks` is the SDL button event's repeat count — 2 means a title-bar
    // double-click, which toggles maximize instead of starting a move grab.
    bool TryChromeGrab(int mx, int my, float scale, int clicks);
    void CommitChromeResize(JKClientConnection& client, uint32_t layerId,
                            int width, int height, int dispW, int dispH);
    // Maximize/restore (docs/39): toggle from the chrome button or the title
    // double-click. RestoreFromMaximize is the shared core — also used to
    // un-maximize before a Move/Resize grab starts on a maximized layer
    // (drag-restore). Returns false (no-op) when the layer is not maximized.
    void ToggleMaximize(JKClientConnection& client, JKCompositorLayer& layer);
    bool RestoreFromMaximize(JKClientConnection& client, JKCompositorLayer& layer);
    // vplayer 전체화면(스펙 2026-09-17 vplayer-fullscreen-osd §2.1):
    // maximize 형제 — preFsRects_가 상태의 단일 소유, SetFullscreen은 그리기
    // 경로 거울. on=false = RestoreFullscreen(저장 rect 복원 + exit 이벤트).
    void ToggleFullscreen(JKClientConnection& client, JKCompositorLayer& layer,
                          bool on);
    bool RestoreFullscreen(JKClientConnection& client, JKCompositorLayer& layer);
    // Directional system cursors for chrome resize feedback.
    enum class CursorShape { Arrow = 0, SizeWE, SizeNS, SizeNWSE, SizeNESW };
    // Chrome hover feedback: show a directional system cursor over resize
    // hotspots (↔ / ↕ / corner diagonals), arrow everywhere else.
    void UpdateChromeHoverCursor(int mx, int my, float scale);
    void SetChromeCursor(CursorShape shape);
    static CursorShape ChromeCursorFromEdges(bool left, bool right, bool top, bool bottom);
    void FocusClient(uint32_t surfaceId);
    JKClientConnection* HitTestClient(int32_t x, int32_t y);
    JKClientConnection* FindClientById(uint32_t surfaceId);
    void Composite();
    // capture_region (docs/35): draw the frame without presenting so the
    // pixels can be read back (requester's layer hidden by the caller).
    void Composite(bool present);
    void CleanupDisconnectedClients();
    void UnblockAcceptor();
    void InitAudio();
    void PostAudioCommand(const AudioCommand& cmd);
    void UpdateOutputBounds();

    // Returns false when nothing was spawned (throttle skip or CreateProcess
    // failure) — file_open uses this to resolve its parked query at once.
    bool SpawnClient(const char* appName, bool fromJkx = false);
    // Launch an arbitrary exe from the server's directory (chat MVP: jkchat).
    // Same 500ms throttle as SpawnClient; throttleKey defaults to exeName.
    bool SpawnProcess(const char* exeName, const std::string& args,
                      const char* throttleKey = nullptr);
    // 콘솔 앱 스폰 (P4 SDK §3/§5): 터미널 위에 cmd — cwd는 앱 폴더. 런처 셀
    // (ShellHost.spawnConsole)과 run_console_app 도구가 공유한다.
    void SpawnConsoleApp(const std::string& cmd, const std::string& cwd,
                         const std::string& name);

    // Decode a decoded RGBA image into a blended SDL texture — exposed to the
    // desktop shell through ShellHost.makeTexture (spec D7: the shell never
    // includes jkserver headers, so texture creation stays a host service).
    SDL_Texture* TextureFromRGBA(const jk::LoadedImage& img, const char* label);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;

    std::unique_ptr<JKMessageBus> messageBus_;
    std::unique_ptr<JKAudioThread> audioThread_;
    std::unique_ptr<JKCompositor> compositor_;

    std::string pipeName_;
    std::atomic<bool> running_{false};
    std::thread acceptorThread_;

    std::mutex clientsMutex_;
    std::vector<std::unique_ptr<JKClientConnection>> clients_;
    std::vector<std::unique_ptr<JKClientConnection>> pendingCleanup_;

    std::mutex pendingClientsMutex_;
    std::vector<std::unique_ptr<JKClientConnection>> pendingClients_;

    uint32_t nextSurfaceId_ = 1;
    uint32_t focusedClientId_ = 0;

    // Pending approval (M2 chat): an "ask"-gated AgentQuery parked until the
    // chat window (any agent-event subscriber) resolves it with the approve
    // tool, or until it expires. Connections are stored by id — pointers
    // would dangle if the requester disconnects while the approval is open.
    struct PendingApproval {
        uint32_t requestId = 0;
        uint32_t queryId = 0;      // AgentQuery to complete on resolution
        uint32_t requesterId = 0;  // requesting connection's id
        uint32_t targetId = 0;     // window the request would touch
                                   // (trust_request: 0 — no target window)
        time_t expiresAt = 0;
        // Script trust model (docs/37 spec): one pipeline, several kinds.
        std::string kind = "close_window";  // "close_window" | "trust_request"
                                            // | "run_console_app" (P4 SDK)
                                            // | "files_access" (file hub)
        std::string name;          // trust_request: script display name
                                   // run_console_app: 콘솔 앱 이름 (P4 SDK)
        std::string origin;        // trust_request: "dev" | "package"
        std::string fingerprint;   // trust_request: "sha256:<64hex>"
        // 매니저 권한 (specs/2026-09-16-agent-manager §2.2): permission_set의
        // 대상 도구와 결정 — kind 전용 페이로드(name 재용용 금지).
        std::string permTool;      // permission_set: 대상 도구
        std::string permDecision;  // permission_set: "allow"|"ask"|"deny"
        // 파일 허브 (specs/2026-09-18-file-hub §2.2): files_access의 재실행
        // 원본 — 도구/경로/읽기 상한. 승인 시점에 FilesPermRaw deny 재검사 +
        // 재실행(파킹 대기 중 파일값이 바뀌면 최신 게이트가 강제 —
        // run_console_app의 승인 시점 재조회 선례).
        std::string filesTool;     // files_access: "files_list"|"files_read"
                                   // |"files_audit" (opus 리뷰 MINOR-3 파킹)
        std::string filesPath;
        int filesMaxBytes = 0;     // files_read 전용 (0 = 기본 64KiB)
        int filesLimit = 0;        // files_audit 전용 (0 = 기본 50)
    };
    std::vector<PendingApproval> pendingApprovals_;
    uint32_t nextApprovalId_ = 1;

    // 파일 열기 대화상자 (filedlg 설계 specs/2026-09-13-file-dialog §1b):
    // file_open이 채우는 1슬롯 파라미터 — file_dialog_params가 기동 직후
    // 파라미터를 꺼내 가고(paramsTaken), file_open_result가 파킹 쿼리 해소에
    // 쓴다. requesterConnId+requesterId 상관관계는 파라미터 소진 후에도
    // 결과 회수까지 살아 있어야 한다 (다이얼로그 수명 = 슬롯 수명).
    // pendingSnapSpawnerConnId_/overlaySpawner_의 conn-id 페어링 선례를
    // 도구화한 것 (snap의 하드코딩 타이틀 페어링 일반화).
    struct PendingFileDialog {
        uint32_t requesterConnId = 0;  // 0 = 빈 슬롯 (대화상자 미기동/종료)
        uint32_t requestId = 0;        // 파킹된 pendingApprovals_ 항목 id
        bool paramsTaken = false;      // file_dialog_params가 이미 꺼냈는가
        std::string filter;            // 선택 필드 — 없으면 빈 문자열
        std::string start;
        std::string title;
    };
    PendingFileDialog pendingFileDialog_;

    // publish_event connection rate budget (docs/38 spec §4) — fixed window.
    // Accessed only on the clientsMutex_-held HandleAgentQuery path (lesson
    // 35: no helper takes the lock — this member must stay lock-free there).
    struct PublishBudget {
        uint64_t windowStartMs = 0;
        int count = 0;
        bool logged = false;  // one log line per window per connection
    };
    std::map<uint64_t, PublishBudget> publishBudgets_;

    // docs/35: when a client launches the capture overlay (launch_app snap),
    // remember the requester so the overlay's capture_region can hide the
    // spawner's layer too — the shot viewer that opened the overlay would
    // otherwise appear in its own screenshot. Pending is consumed when the
    // overlay connection registers; stale entries are harmless (connection
    // ids are monotonic, dead ids resolve to no layer).
    uint32_t pendingSnapSpawnerConnId_ = 0;
    std::unordered_map<uint32_t, uint32_t> overlaySpawner_;

    // docs/32: per-topic runtime stats for the events_list catalog — fired
    // count and last fire timestamp (epoch ms), keyed by topic. Updated in
    // PushAgentEventJson (single server-loop thread).
    struct TopicStat {
        uint64_t fired = 0;
        long long lastTs = 0;
    };
    std::unordered_map<std::string, TopicStat> topicStats_;

    // Server-side mouse capture (Win32 SetCapture equivalent): the surface id
    // that received the last MouseDown and has not seen its MouseUp yet.
    // While set, MouseMove/MouseUp are routed to this client even when the
    // cursor leaves the surface, so client-side drags survive outside bounds.
    uint32_t capturedClientId_ = 0;

    // Server-side window chrome grab: title-bar move and border resize on
    // client layers. Only the layer id is stored (layer pointers die on
    // RemoveLayer), so grab handlers re-resolve the layer each event.
    enum class ChromeGrab { None, Move, Resize };
    ChromeGrab chromeGrab_ = ChromeGrab::None;
    uint32_t chromeGrabClient_ = 0;
    uint32_t chromeGrabLayerId_ = 0;
    // Move grab: mouse offset from the layer origin (logical points).
    int chromeGrabDX_ = 0;
    int chromeGrabDY_ = 0;
    // Move grab: fractional grab point inside the surface (lx/w, ly/h) —
    // kept so a deferred drag-restore (docs/39) can re-anchor the restored
    // window under the cursor.
    float chromeGrabFX_ = 0.0f;
    float chromeGrabFY_ = 0.0f;
    // Grab-start mouse position (logical points) — the deferred drag-restore
    // fires only past kResizeHotspot of accumulated motion from here, so a
    // hand's ~1px jitter between a double-click's clicks stays a click.
    int chromeGrabStartX_ = 0;
    int chromeGrabStartY_ = 0;
    // Deferred drag-restore (docs/39 fix 1): the surface id whose grab is
    // running on a maximized layer. Restore fires on the first motion BEYOND
    // the kResizeHotspot drag threshold (not at mousedown), so a double-click
    // — zero-motion or jittered — still reaches zone 1c while maximized and
    // toggles exactly once. Cleared at grab start (re-armed per grab), on
    // mouse-up, and when the grabbed layer vanishes.
    uint32_t chromeRestorePendingId_ = 0;
    // Resize grab: layer origin and DISPLAY size at grab time (logical
    // points; a fit-scaled surface is displayed smaller than its pixels).
    int chromeResizeX_ = 0;
    int chromeResizeY_ = 0;
    int chromeResizeW_ = 0;
    int chromeResizeH_ = 0;
    bool chromeEdgeLeft_ = false;
    bool chromeEdgeRight_ = false;
    bool chromeEdgeBottom_ = false;
    bool chromeEdgeTop_ = false;
    // Directional system cursors for chrome resize feedback, indexed by
    // CursorShape; built once in Init, freed with SDL in the destructor.
    SDL_Cursor* chromeCursors_[5] = {};
    CursorShape cursorShape_ = CursorShape::Arrow;

    // Maximize/restore (docs/39): pre-maximize rect per layer. Presence in
    // the map = currently maximized. Accessed only on the server loop thread
    // (SDL event handling + CleanupDisconnectedClients) — no lock of its own,
    // like the other chrome grab state above.
    struct MaxState { int x, y, surfW, surfH, dispW, dispH; };
    std::map<uint32_t, MaxState> preMaxRects_;
    // vplayer 전체화면(스펙 §2.1): preMaxRects_와 별개 맕 — maximize 글리프/
    // 재발행 경로와 얽히지 않게 하려는 룰링(형제 상태는 별도 소유). 같은
    // 스레드 접근 규약(preMaxRects_와 동일 — 락 없음).
    std::map<uint32_t, MaxState> preFsRects_;

    // docs/39 §8: last logical desktop size seen by UpdateOutputBounds, so a
    // SIZE_CHANGED can be told apart from a MOVED/DISPLAY_CHANGED (only a
    // real size change re-issues maximized layers). -1 = not seeded yet (the
    // first Init call only records the initial size).
    int lastDesktopW_ = -1;
    int lastDesktopH_ = -1;

    // 설정 허브 KV (스펙 2026-09-18-settings-hub §2.2): state/settings.json의
    // 런타임 미러 — 부팅 로드(LoadSettingsKv), settings_set 쓰기.
    bool audioMasterMute_ = false;
    int audioMasterVolume_ = 80;      // 0..100
    int receiptRetentionDays_ = 0;    // 0 = 무기한(현재 관행 — 0은 set 불가)

    // In-process privileged shell (P1 ③): owns the launcher grid + desktop
    // background. Wired in Init, torn down in the destructor.
    std::unique_ptr<desktop::JKDesktopShell> shell_;

    // Throttle launcher icon double-clicks / rapid spawns to one per app per
    // 500 ms. Stores the last spawn time keyed by app name.
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> lastSpawnTimes_;

    // Client host exe (P1 ③, SetClientHostExe). Defaults to jkdesktop.exe so
    // behavior is identical even if nobody injects a name.
    std::string clientHostExe_ = "jkdesktop.exe";

    // Child process handles kept from SpawnProcess, keyed by pid, so a
    // disconnecting client can be classified as crashed (non-zero exit) vs
    // graceful (M2b app.crashed detection). void* avoids <windows.h>.
    std::unordered_map<unsigned long, void*> spawnedClients_;
};

} // namespace server
} // namespace jk

#endif // JKWINDOWSERVER_H
