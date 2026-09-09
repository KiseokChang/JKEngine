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

namespace server {

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

private:
    void AcceptorLoop();
    void ProcessPendingClients();
    void ProcessPendingMessages();
    void ProcessClientMessage(JKClientConnection& client, const ipc::Message& msg);
    // Desktop Agent API (spec §3): route one AgentQuery to its tool and send
    // the AgentReply. Called with clientsMutex_ held (ProcessPendingMessages).
    void HandleAgentQuery(JKClientConnection& client, uint32_t queryId,
                          const std::string& json);
    // Push one desktop event JSON to subscribed control-only clients.
    // Callers hold clientsMutex_.
    void PushAgentEvent(const char* topic, uint32_t id,
                        const std::string& title, uint32_t pid);
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
    bool TryChromeGrab(int mx, int my, float scale);
    void CommitChromeResize(JKClientConnection& client, uint32_t layerId,
                            int width, int height, int dispW, int dispH);
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
    void CleanupDisconnectedClients();
    void UnblockAcceptor();
    void InitAudio();
    void PostAudioCommand(const AudioCommand& cmd);
    void UpdateOutputBounds();

    void InitLauncher();
    void ScanJkxApps();
    void RelayoutLauncherIcons();
    void DrawLauncher();
    void DrawLauncherBackground();
    void DestroyLauncher();
    int HitTestLauncherIcon(int x, int y) const;
    void SpawnClient(const char* appName, bool fromJkx = false);

    // Load a PNG asset pair ("<base>@1x.png" / "@2x.png") — @2x when the
    // output scale is >= 1.5 — into a blended SDL texture. Returns nullptr
    // when the asset is missing (callers fall back to flat drawing).
    SDL_Texture* LoadTextureScaled(const char* assetBase);
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

    // Server-side launcher state: simple icon textures drawn behind client layers.
    // Apps come from installed .jkx containers (apps/*.jkx, spawn via --jkx)
    // and, as a fallback, the built-in process modes (spawn via --client).
    struct LauncherIcon {
        JKRect rect;
        std::string appName;   // spawn key / display name
        std::string jkxPath;   // non-empty → spawn "--jkx <path>"
        SDL_Texture* texture = nullptr;
    };
    std::vector<LauncherIcon> launcherIcons_;

    // Launcher desktop background photo (PNG asset), stretched to the window.
    SDL_Texture* backgroundTexture_ = nullptr;

    // Throttle launcher icon double-clicks / rapid spawns to one per app per
    // 500 ms. Stores the last spawn time keyed by app name.
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> lastSpawnTimes_;
};

} // namespace server
} // namespace jk

#endif // JKWINDOWSERVER_H
