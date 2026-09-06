#include <server/JKWindowServer.h>

#include <apps/AppLauncherItem.h>
#include <JKAudioCommand.h>
#include <JKAudioThread.h>
#include <JKImageLoader.h>
#include <JKJkxFile.h>
#include <JKMessageBus.h>
#include <JKSDLAudioBackend.h>
#include <JKSoundManager.h>
#include <JKPlatform.h>

#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <thread>

#ifdef _WIN32
// Minimal Windows API declarations for spawning client processes without
// pulling in the full Windows headers (which conflict with legacy JKENGINE
// typedefs in other translation units).
struct LauncherStartupInfoA {
    unsigned long cb = 0;
    char* lpReserved = nullptr;
    char* lpDesktop = nullptr;
    char* lpTitle = nullptr;
    unsigned long dwX = 0;
    unsigned long dwY = 0;
    unsigned long dwXSize = 0;
    unsigned long dwYSize = 0;
    unsigned long dwXCountChars = 0;
    unsigned long dwYCountChars = 0;
    unsigned long dwFillAttribute = 0;
    unsigned long dwFlags = 0;
    unsigned short wShowWindow = 0;
    unsigned short cbReserved2 = 0;
    unsigned char* lpReserved2 = nullptr;
    void* hStdInput = nullptr;
    void* hStdOutput = nullptr;
    void* hStdError = nullptr;
};

struct LauncherProcessInformation {
    void* hProcess = nullptr;
    void* hThread = nullptr;
    unsigned long dwProcessId = 0;
    unsigned long dwThreadId = 0;
};

extern "C" __declspec(dllimport) int __stdcall CreateProcessA(
    const char* lpApplicationName,
    char* lpCommandLine,
    void* lpProcessAttributes,
    void* lpThreadAttributes,
    int bInheritHandles,
    unsigned long dwCreationFlags,
    void* lpEnvironment,
    const char* lpCurrentDirectory,
    LauncherStartupInfoA* lpStartupInfo,
    LauncherProcessInformation* lpProcessInformation);

extern "C" __declspec(dllimport) int __stdcall CloseHandle(void* hObject);

extern "C" __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(
    void* hModule, char* lpFilename, unsigned long nSize);

// .jkx app discovery (ScanJkxApps).
struct JkxFindData {
    unsigned long dwFileAttributes = 0;
    unsigned long ftCreationTime[2] = {};
    unsigned long ftLastAccessTime[2] = {};
    unsigned long ftLastWriteTime[2] = {};
    unsigned long nFileSizeHigh = 0;
    unsigned long nFileSizeLow = 0;
    unsigned long dwReserved0 = 0;
    unsigned long dwReserved1 = 0;
    char cFileName[260] = {};
    char cAlternateFileName[14] = {};
};

extern "C" __declspec(dllimport) void* __stdcall FindFirstFileA(
    const char* lpFileName, JkxFindData* lpFindFileData);
extern "C" __declspec(dllimport) int __stdcall FindNextFileA(
    void* hFindFile, JkxFindData* lpFindFileData);
extern "C" __declspec(dllimport) int __stdcall FindClose(void* hFindFile);
#endif // _WIN32

namespace jk {
namespace server {

JKWindowServer::JKWindowServer() = default;

JKWindowServer::~JKWindowServer() {
    Stop();
}

bool JKWindowServer::Init(const std::string& title, int width, int height) {
#ifdef _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
#endif
    // A click on an unfocused window must BOTH activate it and act (grab the
    // title bar, press a button...). SDL's default drops the activation click,
    // which breaks "click title bar of a background surface to move it".
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::fprintf(stderr, "JKWindowServer::Init: SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    window_ = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window_) {
        std::fprintf(stderr, "JKWindowServer::Init: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        std::fprintf(stderr, "JKWindowServer::Init: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        return false;
    }

    compositor_ = std::make_unique<JKCompositor>(renderer_);
    UpdateOutputBounds();
    InitLauncher();

#ifdef _WIN32
    // The server forwards raw keys to client surfaces and does no text
    // composition of its own. With an IME attached, Enter/Esc/letters arrive
    // as VK_PROCESSKEY + committed text only, so clients would never see
    // those keydowns — detach the IME context from the SDL window.
    JKPlatform::DetachIme(window_);
#endif

    return true;
}

void JKWindowServer::StartAcceptor(const std::string& pipeName) {
    pipeName_ = pipeName;
    InitAudio();
    running_ = true;
    acceptorThread_ = std::thread([this] { AcceptorLoop(); });
}

void JKWindowServer::AcceptorLoop() {
    while (running_) {
        auto transport = ipc::JKPipeTransport::CreateServer(pipeName_);
        if (!transport) {
            if (!running_) break;
            std::fprintf(stderr, "JKWindowServer::AcceptorLoop: accept failed\n");
            continue;
        }
        if (!running_) break;

        // Expect Hello.
        ipc::Message hello;
        if (!ipc::ReadMessage(*transport, hello) || hello.type != ipc::MsgType::Hello) {
            std::fprintf(stderr, "JKWindowServer::AcceptorLoop: expected Hello, got type=%u\n",
                         static_cast<uint32_t>(hello.type));
            continue;
        }

        // Expect CreateSurface.
        ipc::Message createMsg;
        if (!ipc::ReadMessage(*transport, createMsg) ||
            createMsg.type != ipc::MsgType::CreateSurface ||
            createMsg.payload.size() < sizeof(ipc::SurfaceCreatePayload)) {
            std::fprintf(stderr, "JKWindowServer::AcceptorLoop: expected CreateSurface\n");
            continue;
        }

        ipc::SurfaceCreatePayload create{};
        std::memcpy(&create, createMsg.payload.data(), sizeof(create));

        uint32_t id = nextSurfaceId_++;
        auto client = std::make_unique<JKClientConnection>(id, std::move(transport));

        if (!client->CreateSurface(create.width, create.height, create.title)) {
            std::fprintf(stderr, "JKWindowServer::AcceptorLoop: failed to create surface\n");
            continue;
        }

        ipc::SurfaceCreatedPayload created{};
        created.surfaceId = id;
        std::string shmName = std::string("Local\\JKSurfaceShm_") + std::to_string(id);
        std::strncpy(created.shmName, shmName.c_str(), sizeof(created.shmName) - 1);
        if (!client->Send(ipc::MsgType::SurfaceCreated, &created, sizeof(created))) {
            std::fprintf(stderr, "JKWindowServer::AcceptorLoop: failed to send SurfaceCreated\n");
            continue;
        }

        client->StartReadThread();

        {
            std::lock_guard<std::mutex> lock(pendingClientsMutex_);
            pendingClients_.push_back(std::move(client));
        }

        std::fprintf(stderr, "JKWindowServer: client surface %u created (%dx%d)\n",
                     id, create.width, create.height);
    }
}

void JKWindowServer::ProcessPendingClients() {
    std::vector<std::unique_ptr<JKClientConnection>> newClients;
    {
        std::lock_guard<std::mutex> lock(pendingClientsMutex_);
        newClients = std::move(pendingClients_);
        pendingClients_.clear();
    }

    int existingCount = 0;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        existingCount = static_cast<int>(clients_.size());
    }

    for (auto& client : newClients) {
        if (!client) continue;

        int ww = 0, wh = 0;
        SDL_GetWindowSize(window_, &ww, &wh);
        // Surfaces larger than the desktop (apps designed for 1920x1080) are
        // displayed scaled down to fit; the client keeps rendering at its
        // designed surface size. Chrome zones are proportional to the layer
        // size, so title-drag, the close overlay and resize hotspots keep
        // working under a fit scale.
        const float fit = std::min(1.0f,
            std::min(ww / static_cast<float>(client->Width()),
                     wh / static_cast<float>(client->Height())));
        const int dispW = static_cast<int>(client->Width() * fit);
        const int dispH = static_cast<int>(client->Height() * fit);
        int x = std::max(0, (ww - dispW) / 2) + existingCount * 20;
        int y = std::max(0, (wh - dispH) / 2) + existingCount * 20;
        // A full-desktop fit layer (dispW == ww) would push its close-button
        // corner past the window edge with the cascade offset — clamp so the
        // whole layer, chrome included, stays inside the desktop.
        x = std::min(x, std::max(0, ww - dispW));
        y = std::min(y, std::max(0, wh - dispH));
        client->SetPosition(x, y);
        ++existingCount;

        // Register the client surface with the compositor.
        auto* layer = compositor_->AddLayer(
            client->Id(),
            client->Width(),
            client->Height(),
            client->Title(),
            client->SurfaceData());
        if (!layer) {
            std::fprintf(stderr, "JKWindowServer: failed to add layer for surface %u\n",
                         client->Id());
            client->StopReadThread();
            continue;
        }
        compositor_->SetLayerPosition(client->Id(), x, y);
        if (fit < 1.0f) {
            compositor_->SetLayerScale(client->Id(), fit, fit);
        }
        // A newly spawned client takes keyboard focus unconditionally, like a
        // new desktop window: focusedClientId_ drives key/text/wheel routing
        // while FocusLayer only fixes z-order. Only calling FocusLayer here
        // left focusedClientId_ at 0, so keys were silently dropped until the
        // first click on the surface (tetris arrows appeared dead at spawn).
        FocusClient(client->Id());

        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clients_.push_back(std::move(client));
        }
    }
}

void JKWindowServer::Run() {
    if (!renderer_) return;
    running_ = true;

    while (running_) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running_ = false;
                break;
            }
            if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE) {
                running_ = false;
                break;
            }
            HandleSDLEvent(ev);
        }
        if (!running_) break;

        ProcessPendingClients();
        ProcessPendingMessages();
        Composite();
        CleanupDisconnectedClients();

        SDL_Delay(1);
    }
}

void JKWindowServer::Stop() {
    running_ = false;

    UnblockAcceptor();

    if (acceptorThread_.joinable()) {
        acceptorThread_.join();
    }

    if (audioThread_) {
        AudioCommand quitCmd{};
        quitCmd.type = AudioCommand::Type::Quit;
        PostAudioCommand(quitCmd);
        (*audioThread_).Stop();
        audioThread_.reset();
    }
    messageBus_.reset();

    // Move clients out of the locked vectors before joining their read threads
    // to avoid holding clientsMutex_/pendingClientsMutex_ during a potentially
    // blocking join and to prevent deadlocks if a read thread tries to queue a
    // message during shutdown.
    {
        std::vector<std::unique_ptr<JKClientConnection>> clientsToStop;
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clientsToStop = std::move(clients_);
            clients_.clear();
        }
        for (auto& client : clientsToStop) {
            if (client) client->StopReadThread();
        }
    }

    {
        std::vector<std::unique_ptr<JKClientConnection>> pendingToStop;
        {
            std::lock_guard<std::mutex> lock(pendingClientsMutex_);
            pendingToStop = std::move(pendingClients_);
            pendingClients_.clear();
        }
        for (auto& client : pendingToStop) {
            if (client) client->StopReadThread();
        }
    }

    pendingCleanup_.clear();

    DestroyLauncher();
    compositor_.reset();

    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

void JKWindowServer::InitAudio() {
    messageBus_ = std::make_unique<JKMessageBus>();
    audioThread_ = std::make_unique<JKAudioThread>();
    (*audioThread_).Start(messageBus_.get(), std::make_unique<SDLAudioBackend>());

    AudioCommand initCmd{};
    initCmd.type = AudioCommand::Type::Init;
    PostAudioCommand(initCmd);
}

namespace {

std::string ResolveAudioPath(const char* id, AudioCommand::Type type) {
    const char* ext = (type == AudioCommand::Type::LoadBGM) ? ".wav" : ".wav";
    return JKSoundManager::AssetPath(std::string(id) + ext);
}

} // anonymous namespace

void JKWindowServer::PostAudioCommand(const AudioCommand& cmd) {
    if (!messageBus_) return;
    std::vector<uint8_t> data(sizeof(AudioCommand));
    std::memcpy(data.data(), &cmd, sizeof(AudioCommand));
    (*messageBus_).Push(JKMessageBus::Channel::Audio,
        JKMessageBus::Payload(static_cast<uint32_t>(cmd.type), std::move(data)));
}

void JKWindowServer::UnblockAcceptor() {
    if (pipeName_.empty()) return;
    // The acceptor thread blocks in ConnectNamedPipe. Open a short-lived
    // client connection so it unblocks and notices running_ == false.
    for (int i = 0; i < 50; ++i) {
        auto poison = ipc::JKPipeTransport::ConnectClient(pipeName_);
        if (poison) {
            poison->Close();
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::fprintf(stderr, "JKWindowServer::UnblockAcceptor: failed to unblock acceptor\n");
}

bool JKWindowServer::HandleChromeGrab(const SDL_Event& ev, int mx, int my, float scale) {
    if (chromeGrab_ == ChromeGrab::None) {
        return false;
    }
    JKClientConnection* client = FindClientById(chromeGrabClient_);
    JKCompositorLayer* layer =
        compositor_ ? compositor_->FindLayerById(chromeGrabLayerId_) : nullptr;
    if (!client || !layer) {
        // The grabbed layer vanished (client disconnected during drag).
        chromeGrab_ = ChromeGrab::None;
        chromeGrabClient_ = 0;
        chromeGrabLayerId_ = 0;
        return true;
    }

    const int lmX = static_cast<int>(std::llround(mx / scale));
    const int lmY = static_cast<int>(std::llround(my / scale));

    if (ev.type == SDL_MOUSEBUTTONDOWN) {
        return true;  // other buttons during a drag are consumed
    }

    if (ev.type == SDL_MOUSEMOTION) {
        if (chromeGrab_ == ChromeGrab::Move) {
            int winW = 0, winH = 0;
            SDL_GetWindowSize(window_, &winW, &winH);
            // Display size in logical points (a fit-scaled layer is smaller
            // than its surface size).
            const int lw = static_cast<int>(std::llround(
                layer->Width() * layer->ScaleX()));
            const int lh = static_cast<int>(std::llround(
                layer->Height() * layer->ScaleY()));
            int nx = lmX - chromeGrabDX_;
            int ny = lmY - chromeGrabDY_;
            nx = std::max(-lw + 40, std::min(nx, std::max(0, winW - 40)));
            ny = std::max(0, std::min(ny, std::max(0, winH - 40)));
            client->SetPosition(nx, ny);
            compositor_->SetLayerPosition(client->Id(), nx, ny);
        } else {  // Resize: stretch-preview via layer scale.
            int newW = chromeResizeW_;
            int newH = chromeResizeH_;
            int newX = chromeResizeX_;
            if (chromeEdgeRight_) newW = lmX - chromeResizeX_;
            if (chromeEdgeBottom_) newH = lmY - chromeResizeY_;
            if (chromeEdgeLeft_) {
                newW = chromeResizeW_ + (chromeResizeX_ - lmX);
            }
            newW = std::max(64, newW);
            newH = std::max(48, newH);
            // Absolute display scale = display target / surface width (the
            // surface size does not change until the resize is committed).
            layer->SetScale(newW / static_cast<float>(layer->Width()),
                            newH / static_cast<float>(layer->Height()));
            if (chromeEdgeLeft_) {
                newX = chromeResizeX_ + chromeResizeW_ - newW;
                client->SetPosition(newX, chromeResizeY_);
                compositor_->SetLayerPosition(client->Id(), newX, chromeResizeY_);
            }
        }
        return true;
    }

    if (ev.type == SDL_MOUSEBUTTONUP) {
        if (chromeGrab_ == ChromeGrab::Resize) {
            // Recompute the final logical size at the release point.
            int newW = chromeResizeW_;
            int newH = chromeResizeH_;
            int newX = chromeResizeX_;
            if (chromeEdgeRight_) newW = lmX - chromeResizeX_;
            if (chromeEdgeBottom_) newH = lmY - chromeResizeY_;
            if (chromeEdgeLeft_) {
                newW = chromeResizeW_ + (chromeResizeX_ - lmX);
            }
            newW = std::max(64, newW);
            newH = std::max(48, newH);
            if (newW != chromeResizeW_ || newH != chromeResizeH_) {
                if (chromeEdgeLeft_) {
                    newX = chromeResizeX_ + chromeResizeW_ - newW;
                    client->SetPosition(newX, chromeResizeY_);
                    compositor_->SetLayerPosition(client->Id(), newX, chromeResizeY_);
                }
                // Keep the grab-time surface:display ratio so a fit-scaled
                // surface resizes without cutting off its layout (1:1 layers
                // get surface == display, unchanged).
                const float fitX = static_cast<float>(layer->Width()) / std::max(1, chromeResizeW_);
                const float fitY = static_cast<float>(layer->Height()) / std::max(1, chromeResizeH_);
                const int commitW = std::max(64,
                    static_cast<int>(std::llround(newW / fitX)));
                const int commitH = std::max(48,
                    static_cast<int>(std::llround(newH / fitY)));
                CommitChromeResize(*client, layer->Id(), commitW, commitH, newW, newH);
            }
        }
        chromeGrab_ = ChromeGrab::None;
        chromeGrabClient_ = 0;
        chromeGrabLayerId_ = 0;
        return true;
    }

    return true;
}

bool JKWindowServer::TryChromeGrab(int mx, int my, float scale) {
    if (!compositor_) {
        return false;
    }
    JKCompositorLayer* layer = compositor_->HitTest(mx, my);
    if (!layer) {
        return false;
    }
    JKClientConnection* client = FindClientById(layer->Id());
    if (!client) {
        return false;
    }

    // Chrome zones are in SURFACE-local px (they shrink proportionally on
    // fit-scaled layers, §7.3), so convert display px → surface px here.
    // For 1:1 layers ScaleX/Y == 1 and this is the plain logical-local map.
    const int lx = static_cast<int>(std::llround(
        (mx / scale - layer->X()) / layer->ScaleX()));
    const int ly = static_cast<int>(std::llround(
        (my / scale - layer->Y()) / layer->ScaleY()));
    const int w = layer->Width();
    const int h = layer->Height();

    // 1) Close overlay (top-right of the title bar, server-drawn).
    const bool inCloseX = (lx >= w - kChromeCloseSize - kChromeCloseMargin) &&
                          (lx < w - kChromeCloseMargin);
    const bool inCloseY = (ly >= kChromeCloseMargin) &&
                          (ly < kChromeCloseMargin + kChromeCloseSize);
    if (inCloseX && inCloseY) {
        FocusClient(client->Id());
        client->Send(ipc::MsgType::Close, nullptr, 0);
        return true;
    }

    // 2) Resize edges: left/right/bottom (6px inset). The top edge stays
    //    title-drag, matching the client-painted frame.
    const bool edgeLeft = (lx < kResizeHotspot);
    const bool edgeRight = (lx >= w - kResizeHotspot);
    const bool edgeBottom = (ly >= h - kResizeHotspot);
    if (edgeLeft || edgeRight || edgeBottom) {
        FocusClient(client->Id());
        capturedClientId_ = 0;
        chromeGrab_ = ChromeGrab::Resize;
        chromeGrabClient_ = client->Id();
        chromeGrabLayerId_ = layer->Id();
        chromeResizeX_ = layer->X();
        chromeResizeY_ = layer->Y();
        // Display size at grab time (logical points) — the resize drag and
        // its commit threshold work in display space.
        chromeResizeW_ = static_cast<int>(std::llround(w * layer->ScaleX()));
        chromeResizeH_ = static_cast<int>(std::llround(h * layer->ScaleY()));
        chromeEdgeLeft_ = edgeLeft;
        chromeEdgeRight_ = edgeRight;
        chromeEdgeBottom_ = edgeBottom;
        return true;
    }

    // 3) Title bar: start a move grab.
    if (ly < kChromeTitleBar) {
        FocusClient(client->Id());
        capturedClientId_ = 0;
        chromeGrab_ = ChromeGrab::Move;
        chromeGrabClient_ = client->Id();
        chromeGrabLayerId_ = layer->Id();
        // Move works in desktop-logical positions, but lx/ly are surface-local
        // and a fit-scaled layer maps surface px to logical px at ScaleX/Y.
        chromeGrabDX_ = static_cast<int>(std::llround(lx * layer->ScaleX()));
        chromeGrabDY_ = static_cast<int>(std::llround(ly * layer->ScaleY()));
        return true;
    }
    return false;
}

void JKWindowServer::CommitChromeResize(JKClientConnection& client, uint32_t layerId,
                                        int width, int height, int dispW, int dispH) {
    // Order matters: (1) create the new shared memory generation and retire
    // the old one, (2) atomically swap the layer texture/pixels to the new
    // size (a layer with the new width and the old pixel buffer would make
    // SDL_UpdateTexture read out of bounds), (3) only then notify the client.
    ipc::SurfaceResizePayload payload{};
    if (!client.BeginResizeSurface(width, height, payload)) {
        return;
    }
    if (!compositor_ || !compositor_->ResizeLayer(layerId, width, height,
                                                  client.SurfaceData())) {
        return;
    }
    // Restore the display size the drag asked for (ResizeLayer resets the
    // layer scale to 1; a fit-scaled surface needs its scale back).
    if (dispW != width || dispH != height) {
        compositor_->SetLayerScale(layerId,
                                   dispW / static_cast<float>(width),
                                   dispH / static_cast<float>(height));
    }
    client.Send(ipc::MsgType::ResizeSurface, &payload, sizeof(payload));
}

void JKWindowServer::HandleSDLEvent(const SDL_Event& ev) {
    if (ev.type == SDL_WINDOWEVENT &&
        (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
         ev.window.event == SDL_WINDOWEVENT_MOVED ||
         ev.window.event == SDL_WINDOWEVENT_DISPLAY_CHANGED)) {
        UpdateOutputBounds();
    }

    if (ev.type == SDL_MOUSEMOTION || ev.type == SDL_MOUSEBUTTONDOWN ||
        ev.type == SDL_MOUSEBUTTONUP) {
        // Everything in this server is in physical client pixels: the launcher
        // icons and compositor layers are drawn at logical_pt * outputScale =
        // physical px, and the hit-test compares against those same physical
        // px. So we want the mouse in raw physical client px (no DPI division).
        int mx = 0, my = 0;
        const float outputScale = compositor_ ? compositor_->OutputScale() : 1.0f;
        int physX = 0, physY = 0;
        if (window_ &&
            JKPlatform::GetPhysicalClientMousePos(window_, physX, physY)) {
            mx = physX;
            my = physY;
        } else if (ev.type == SDL_MOUSEMOTION) {
            // SDL coords are logical points; convert to physical to match the
            // rest of the pipeline.
            mx = static_cast<int>(std::llround(ev.motion.x * outputScale));
            my = static_cast<int>(std::llround(ev.motion.y * outputScale));
        } else {
            mx = static_cast<int>(std::llround(ev.button.x * outputScale));
            my = static_cast<int>(std::llround(ev.button.y * outputScale));
        }

        // Server window chrome (title-bar move / close / border resize)
        // intercepts mouse input before anything reaches the client.
        if (HandleChromeGrab(ev, mx, my, outputScale)) {
            return;
        }
        if (ev.type == SDL_MOUSEBUTTONDOWN && TryChromeGrab(mx, my, outputScale)) {
            return;
        }

        // Client surfaces are rendered on top of the launcher, so they should
        // receive input first. Only treat a click as a launcher icon click if
        // it did not hit any client surface.
        //
        // While a mouse button is held inside a client surface (captured),
        // keep routing motion/release to that client even when the cursor
        // leaves the surface — the server-side equivalent of Win32
        // SetCapture. The client runs the same capture logic as the
        // single-process path, so out-of-bounds payload coordinates are
        // expected and handled there.
        JKClientConnection* client = nullptr;
        if (capturedClientId_ != 0) {
            client = FindClientById(capturedClientId_);
            if (!client) capturedClientId_ = 0;  // captured client vanished
        }
        if (!client) client = HitTestClient(mx, my);
        if (!client && ev.type == SDL_MOUSEBUTTONDOWN) {
            int icon = HitTestLauncherIcon(mx, my);
            if (icon >= 0) {
                const LauncherIcon& item = launcherIcons_[static_cast<size_t>(icon)];
                if (item.jkxPath.empty()) {
                    SpawnClient(item.appName.c_str());
                } else {
                    SpawnClient(item.jkxPath.c_str(), /*fromJkx=*/true);
                }
                return;
            }
        }
        if (!client) return;

        // mx/my are physical client px. The client surface is client->Width() x
        // client->Height() surface pixels, stretched by outputScale when drawn
        // and possibly shrunk by the fit scale (surface larger than desktop).
        // Convert the physical mouse position back into the client's surface
        // pixel space: ((mx/outputScale) - client->X()) / layerScale.
        float layerScaleX = 1.0f, layerScaleY = 1.0f;
        if (compositor_) {
            if (auto* layer = compositor_->FindLayerById(client->Id())) {
                layerScaleX = layer->ScaleX();
                layerScaleY = layer->ScaleY();
            }
        }
        ipc::InputEventPayload payload{};
        payload.surfaceId = client->Id();
        payload.x = static_cast<int>(std::llround(
            (mx / outputScale - client->X()) / layerScaleX));
        payload.y = static_cast<int>(std::llround(
            (my / outputScale - client->Y()) / layerScaleY));

        if (ev.type == SDL_MOUSEMOTION) {
            payload.type = ipc::InputEventType::MouseMove;
            payload.dx = static_cast<int32_t>(std::llround(ev.motion.xrel / layerScaleX));
            payload.dy = static_cast<int32_t>(std::llround(ev.motion.yrel / layerScaleY));
        } else if (ev.type == SDL_MOUSEBUTTONDOWN) {
            payload.type = ipc::InputEventType::MouseDown;
            payload.keyCode = ev.button.button;
            payload.detail = ev.button.clicks;
            FocusClient(client->Id());
            capturedClientId_ = client->Id();
        } else if (ev.type == SDL_MOUSEBUTTONUP) {
            payload.type = ipc::InputEventType::MouseUp;
            payload.keyCode = ev.button.button;
            payload.detail = ev.button.clicks;
            capturedClientId_ = 0;
        }

        SendInputEvent(*client, payload);
    } else if (ev.type == SDL_MOUSEWHEEL) {
        JKClientConnection* client = FindClientById(focusedClientId_);
        if (!client) return;
        ipc::InputEventPayload payload{};
        payload.surfaceId = client->Id();
        payload.type = ipc::InputEventType::MouseWheel;
        payload.dx = ev.wheel.x;
        payload.dy = ev.wheel.y;
        SendInputEvent(*client, payload);
    } else if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
        JKClientConnection* client = FindClientById(focusedClientId_);
        if (!client) return;
        ipc::InputEventPayload payload{};
        payload.surfaceId = client->Id();
        payload.type = (ev.type == SDL_KEYDOWN) ? ipc::InputEventType::KeyDown
                                                : ipc::InputEventType::KeyUp;
        payload.keyCode = ev.key.keysym.sym;
        payload.detail = ev.key.repeat;
        payload.option = ev.key.keysym.mod;
        SendInputEvent(*client, payload);
    } else if (ev.type == SDL_TEXTINPUT) {
        JKClientConnection* client = FindClientById(focusedClientId_);
        if (!client) return;
        ipc::InputEventPayload payload{};
        payload.surfaceId = client->Id();
        payload.type = ipc::InputEventType::Char;
        std::strncpy(payload.text, ev.text.text, sizeof(payload.text) - 1);
        SendInputEvent(*client, payload);
    } else if (ev.type == SDL_TEXTEDITING) {
        JKClientConnection* client = FindClientById(focusedClientId_);
        if (!client) return;
        ipc::InputEventPayload payload{};
        payload.surfaceId = client->Id();
        payload.type = ipc::InputEventType::TextEditing;
        std::strncpy(payload.text, ev.edit.text, sizeof(payload.text) - 1);
        payload.detail = ev.edit.start;
        payload.option = ev.edit.length;
        SendInputEvent(*client, payload);
    }
}

void JKWindowServer::SendInputEvent(JKClientConnection& client, const ipc::InputEventPayload& payload) {
    client.Send(ipc::MsgType::InputEvent, &payload, sizeof(payload));
}

JKClientConnection* JKWindowServer::HitTestClient(int32_t x, int32_t y) {
    if (!compositor_) return nullptr;
    auto* layer = compositor_->HitTest(x, y);
    if (!layer) return nullptr;
    return FindClientById(layer->Id());
}

JKClientConnection* JKWindowServer::FindClientById(uint32_t surfaceId) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& client : clients_) {
        if (client && client->Id() == surfaceId) {
            return client.get();
        }
    }
    return nullptr;
}

void JKWindowServer::FocusClient(uint32_t surfaceId) {
    focusedClientId_ = surfaceId;
    if (compositor_) {
        compositor_->FocusLayer(surfaceId);
    }
}

void JKWindowServer::UpdateOutputBounds() {
    if (!window_ || !compositor_ || !renderer_) return;
    int logW = 0, logH = 0;
    SDL_GetWindowSize(window_, &logW, &logH);
    int physW = 0, physH = 0;
    SDL_GetRendererOutputSize(renderer_, &physW, &physH);

    // Match the single-process render thread: the renderer works in physical
    // pixels, but all layer positions/sizes are stored in SDL logical points.
    // Scale logical points to physical pixels when compositing.
    float scale = 1.0f;
    if (logW > 0 && logH > 0) {
        scale = physW / static_cast<float>(logW);
    }
    compositor_->SetOutput(JKCompositorOutput(0, JKRect{0, 0, logW, logH}, scale));
}

void JKWindowServer::ProcessPendingMessages() {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& client : clients_) {
        if (!client) continue;

        ipc::Message msg;
        while (client->PopMessage(msg)) {
            ProcessClientMessage(*client, msg);
        }
    }
}

void JKWindowServer::ProcessClientMessage(JKClientConnection& client, const ipc::Message& msg) {
    if (msg.type == ipc::MsgType::CommitSurface) {
        if (msg.payload.size() >= sizeof(ipc::CommitSurfaceHeader)) {
            const auto* header = reinterpret_cast<const ipc::CommitSurfaceHeader*>(
                msg.payload.data());
            const size_t expected = sizeof(ipc::CommitSurfaceHeader) +
                                    header->dirtyCount * sizeof(ipc::DirtyRect);
            if (msg.payload.size() >= expected) {
                client.MarkDirty();
                if (compositor_) {
                    compositor_->MarkDirty(client.Id());
                }
            }
        }
    } else if (msg.type == ipc::MsgType::AudioCommand) {
        if (msg.payload.size() >= sizeof(AudioCommand)) {
            AudioCommand cmd{};
            std::memcpy(&cmd, msg.payload.data(), sizeof(AudioCommand));
            // Resolve asset paths on the server so clients only need the id.
            if ((cmd.type == AudioCommand::Type::LoadSFX ||
                 cmd.type == AudioCommand::Type::LoadBGM) && cmd.path[0] == '\0') {
                std::string path = ResolveAudioPath(cmd.id, cmd.type);
                std::strncpy(cmd.path, path.c_str(), sizeof(cmd.path) - 1);
            }
            PostAudioCommand(cmd);
        }
    } else if (msg.type == ipc::MsgType::Close) {
        // Client explicitly closed.
    }
}

void JKWindowServer::Composite() {
    if (!compositor_ || !renderer_) {
        return;
    }

    // All drawing in this server is done in physical pixels: the launcher icons
    // and compositor layers are scaled by outputScale manually, and the mouse
    // hit-test uses raw physical client pixels. We deliberately do NOT call
    // SDL_RenderSetScale here because its effect differs across SDL render
    // backends (D3D vs OpenGL) and caused coordinate drift on the primary
    // high-DPI monitor. Keeping everything in physical px removes that
    // ambiguity.
    SDL_RenderSetScale(renderer_, 1.0f, 1.0f);

    // Draw the launcher desktop into the renderer first; the compositor will
    // layer client surfaces on top and then present once.
    DrawLauncherBackground();
    compositor_->Composite();
}

void JKWindowServer::CleanupDisconnectedClients() {
    std::vector<std::unique_ptr<JKClientConnection>> disconnected;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = clients_.begin();
        while (it != clients_.end()) {
            auto& client = *it;
            if (client && client->IsDisconnected()) {
                if (focusedClientId_ == client->Id()) {
                    focusedClientId_ = 0;
                }
                if (capturedClientId_ == client->Id()) {
                    capturedClientId_ = 0;
                }
                if (compositor_) {
                    compositor_->RemoveLayer(client->Id());
                }
                disconnected.push_back(std::move(client));
                it = clients_.erase(it);
            } else {
                ++it;
            }
        }

        // When the focused client went away, move keyboard focus to the
        // topmost surviving layer so keys keep working without a click.
        if (focusedClientId_ == 0 && compositor_ && !clients_.empty()) {
            if (uint32_t top = compositor_->TopmostLayerId()) {
                FocusClient(top);
            }
        }
    }

    // Join read threads outside the lock to avoid blocking the server main loop
    // and to prevent shutdown deadlocks.
    for (auto& client : disconnected) {
        if (client) client->StopReadThread();
    }
}

SDL_Texture* JKWindowServer::TextureFromRGBA(const jk::LoadedImage& img, const char* label) {
    if (!renderer_) return nullptr;
    // stb decodes to byte-order R,G,B,A; SDL_PIXELFORMAT_RGBA32 is exactly
    // that layout regardless of platform endianness.
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        const_cast<uint8_t*>(img.rgba.data()), img.w, img.h, 32, img.w * 4,
        SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        std::fprintf(stderr, "JKWindowServer: surface for '%s' failed: %s\n",
                     label, SDL_GetError());
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_FreeSurface(surface);
    if (!texture) {
        std::fprintf(stderr, "JKWindowServer: texture for '%s' failed: %s\n",
                     label, SDL_GetError());
        return nullptr;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return texture;
}

SDL_Texture* JKWindowServer::LoadTextureScaled(const char* assetBase) {
    if (!renderer_) return nullptr;

    // Pick the @2x asset when the display scale is high enough for the extra
    // pixels to pay off (mixed-DPI rule: renderer ratio drives the choice).
    const float s = compositor_ ? compositor_->OutputScale() : 1.0f;
    char path[512];
    std::snprintf(path, sizeof(path), "%s@%s.png", assetBase, s >= 1.5f ? "2x" : "1x");

    jk::LoadedImage img;
    if (!jk::LoadImageFile(jk::ResolveAssetPath(path), img)) {
        return nullptr;
    }
    return TextureFromRGBA(img, path);
}

void JKWindowServer::InitLauncher() {
    if (!renderer_) return;

    launcherIcons_.clear();

    // Installed .jkx containers first (Phase C): one launcher cell per
    // apps/<name>.jkx, icon decoded from the container itself.
    ScanJkxApps();

    // Built-in process-mode fallback for apps that have no .jkx installed.
    auto hasJkx = [this](const char* name) {
        for (const auto& icon : launcherIcons_) {
            if (icon.appName == name) return true;
        }
        return false;
    };
    if (!hasJkx("minesweeper")) {
        LauncherIcon icon;
        icon.appName = "minesweeper";
        launcherIcons_.push_back(icon);
    }
    if (!hasJkx("tetris")) {
        LauncherIcon icon;
        icon.appName = "tetris";
        launcherIcons_.push_back(icon);
    }

    // Desktop background photo + launcher icon art (PNG assets, see
    // ARCHITECTURE_DOCS/20). Missing assets fall back to the flat placeholder.
    if (backgroundTexture_) {
        SDL_DestroyTexture(backgroundTexture_);
        backgroundTexture_ = nullptr;
    }
    backgroundTexture_ = LoadTextureScaled("assets/backgrounds/desktop");

    for (auto& icon : launcherIcons_) {
        if (icon.texture) continue;   // .jkx apps carry their own icon texture

        // Built-in apps: assets/icons/launcher_<pfx>; legacy cell layout kept
        // for them so the flat-placeholder fallback still matches by name.
        const char* base = (icon.appName == "minesweeper") ? "assets/icons/launcher_mine"
                                                           : "assets/icons/launcher_tetris";
        icon.texture = LoadTextureScaled(base);
        if (icon.texture) {
            std::fprintf(stderr, "JKWindowServer: launcher icon '%s' loaded\n", base);
        }
    }

    // Grid layout: one source of truth for cell rects, wrapping to the window
    // width (the single row overflowed the 1280px desktop at 13 .jkx apps).
    RelayoutLauncherIcons();
    DrawLauncher();
}

// Launcher cell grid (docs/21 §2): wraps cells into multiple rows so a
// growing app list stays on screen. Cells sit 100x100 apart starting at
// (50, 50); the column count derives from the window's logical width.
void JKWindowServer::RelayoutLauncherIcons() {
    if (!renderer_) return;
    const float s = compositor_ ? compositor_->OutputScale() : 1.0f;
    int pw = 1280;
    int ph = 720;
    SDL_GetRendererOutputSize(renderer_, &pw, &ph);
    const int logicalW = static_cast<int>(pw / s);
    int cols = (logicalW - 50) / 100;  // (left margin .. right edge) / pitch
    if (cols < 1) cols = 1;
    for (size_t i = 0; i < launcherIcons_.size(); ++i) {
        const int col = static_cast<int>(i) % cols;
        const int row = static_cast<int>(i) / cols;
        launcherIcons_[i].rect = JKRect{ 50 + col * 100, 50 + row * 100, 64, 80 };
    }
}

void JKWindowServer::ScanJkxApps() {
#ifdef _WIN32
    // Enumerate <exe-dir>/apps/*.jkx. Icon textures are decoded from the
    // container's ICON entries (no temp files); spawning uses --jkx <path>.
    char basePath[1024] = {};
    if (!GetModuleFileNameA(nullptr, basePath, sizeof(basePath))) return;
    char* lastSlash = basePath;
    for (char* p = basePath; *p; ++p) {
        if (*p == '\\' || *p == '/') lastSlash = p;
    }
    *lastSlash = '\0';

    char pattern[1024];
    std::snprintf(pattern, sizeof(pattern), "%s\\apps\\*.jkx", basePath);
    JkxFindData fd{};
    void* find = FindFirstFileA(pattern, &fd);
    if (!find) return;

    const float s = compositor_ ? compositor_->OutputScale() : 1.0f;

    do {
        char path[1024];
        std::snprintf(path, sizeof(path), "%s\\apps\\%s", basePath, fd.cFileName);

        jk::JKJkxFile jkx;
        if (!jkx.Open(path)) continue;
        const jk::JkxManifest& mani = jkx.Manifest();
        if (mani.name.empty()) continue;

        LauncherIcon icon;
        icon.appName = mani.name;
        icon.jkxPath = path;

        // Icon entry: prefer @2x on high-scale displays.
        std::string wanted = (s >= 1.5f && !mani.icon2x.empty()) ? mani.icon2x : mani.icon;
        if (wanted.empty()) wanted = !mani.icon2x.empty() ? mani.icon2x : mani.icon;
        const int entry = wanted.empty() ? -1 : jkx.FindEntry("ICON", wanted);
        std::vector<uint8_t> png;
        jk::LoadedImage img;
        if (entry >= 0 && jkx.ReadEntry(entry, png) &&
            jk::LoadImageMemory(png.data(), png.size(), img)) {
            icon.texture = TextureFromRGBA(img, mani.name.c_str());
        }

        launcherIcons_.push_back(std::move(icon));
        std::fprintf(stderr, "JKWindowServer: installed app '%s' from %s (icon %s)\n",
                     mani.name.c_str(), fd.cFileName,
                     launcherIcons_.back().texture ? "decoded" : "missing");
    } while (FindNextFileA(find, &fd));
    FindClose(find);
#endif // _WIN32
}

void JKWindowServer::DrawLauncher() {
    DrawLauncherBackground();
}

void JKWindowServer::DrawLauncherBackground() {
    if (!renderer_ || launcherIcons_.empty()) return;

    // All server drawing is in physical pixels. icon.rect is stored in SDL
    // logical points, so multiply by the compositor output scale to get the
    // physical-pixel rect. The mouse hit-test uses the same physical rect.
    const float s = compositor_ ? compositor_->OutputScale() : 1.0f;

    // This only draws; the compositor calls SDL_RenderPresent once per frame.
    SDL_SetRenderDrawColor(renderer_, 96, 96, 96, 255);
    SDL_RenderClear(renderer_);

    // Desktop background photo stretched to the full window.
    if (backgroundTexture_) {
        int pw = 0;
        int ph = 0;
        SDL_GetRendererOutputSize(renderer_, &pw, &ph);
        SDL_Rect dst{ 0, 0, pw, ph };
        SDL_RenderCopy(renderer_, backgroundTexture_, nullptr, &dst);
    }

    for (const auto& icon : launcherIcons_) {
        SDL_Rect rc{
            static_cast<int>(icon.rect.x * s),
            static_cast<int>(icon.rect.y * s),
            static_cast<int>(icon.rect.w * s),
            static_cast<int>(icon.rect.h * s),
        };
        if (icon.texture) {
            // Square icon art in the top part of the 64x80 cell; the rest of
            // the cell is label space (the server has no text renderer).
            SDL_Rect art{
                rc.x,
                rc.y,
                static_cast<int>(icon.rect.w * s),
                static_cast<int>(icon.rect.w * s),
            };
            SDL_RenderCopy(renderer_, icon.texture, nullptr, &art);
        } else {
            if (icon.appName == "minesweeper") {
                SDL_SetRenderDrawColor(renderer_, 128, 128, 128, 255);
            } else if (icon.appName == "tetris") {
                SDL_SetRenderDrawColor(renderer_, 128, 0, 128, 255);
            } else {
                SDL_SetRenderDrawColor(renderer_, 100, 100, 100, 255);
            }
            SDL_RenderFillRect(renderer_, &rc);
            SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer_, &rc);
        }
    }
}

void JKWindowServer::DestroyLauncher() {
    for (auto& icon : launcherIcons_) {
        if (icon.texture) {
            SDL_DestroyTexture(icon.texture);
            icon.texture = nullptr;
        }
    }
    launcherIcons_.clear();
    if (backgroundTexture_) {
        SDL_DestroyTexture(backgroundTexture_);
        backgroundTexture_ = nullptr;
    }
}

int JKWindowServer::HitTestLauncherIcon(int x, int y) const {
    // (x, y) are physical client px. icon.rect is in logical points.
    const float s = compositor_ ? compositor_->OutputScale() : 1.0f;
    for (size_t i = 0; i < launcherIcons_.size(); ++i) {
        const auto& r = launcherIcons_[i].rect;
        const int rx = static_cast<int>(r.x * s);
        const int ry = static_cast<int>(r.y * s);
        const int rw = static_cast<int>(r.w * s);
        const int rh = static_cast<int>(r.h * s);
        if (x >= rx && x < rx + rw && y >= ry && y < ry + rh) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void JKWindowServer::SpawnClient(const char* appName, bool fromJkx) {
#ifdef _WIN32
    // Throttle repeated spawns for the same app to avoid launching many copies
    // from a single double-click.
    {
        auto now = std::chrono::steady_clock::now();
        auto it = lastSpawnTimes_.find(appName);
        if (it != lastSpawnTimes_.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - it->second);
            if (elapsed.count() < 500) {
                std::fprintf(stderr,
                             "JKWindowServer: ignoring rapid spawn for %s (%lld ms)\n",
                             appName, static_cast<long long>(elapsed.count()));
                return;
            }
        }
        lastSpawnTimes_[appName] = now;
    }

    // Assume the server executable is in the same directory as the client.
    // Build a command line of the form: jkdesktop.exe --client minesweeper
    char modulePath[1024] = {};
    const unsigned long len = GetModuleFileNameA(nullptr, modulePath, sizeof(modulePath));
    if (len == 0 || len >= sizeof(modulePath)) {
        std::fprintf(stderr, "JKWindowServer: GetModuleFileNameA failed\n");
        return;
    }

    // Find the directory component.
    char* lastSlash = modulePath;
    for (char* p = modulePath; *p; ++p) {
        if (*p == '\\' || *p == '/') lastSlash = p;
    }
    // Leave a NUL after the directory; exe name is appended below.
    if (lastSlash != modulePath) {
        *lastSlash = '\0';
    } else {
        modulePath[0] = '\0';
    }

    char cmdLine[2048] = {};
    if (fromJkx) {
        // A .jkx container path — may contain spaces, so quote it.
        std::snprintf(cmdLine, sizeof(cmdLine),
                      "\"%s\\jkdesktop.exe\" --jkx \"%s\"",
                      modulePath[0] ? modulePath : ".",
                      appName);
    } else {
        std::snprintf(cmdLine, sizeof(cmdLine),
                      "\"%s\\jkdesktop.exe\" --client %s",
                      modulePath[0] ? modulePath : ".",
                      appName);
    }

    LauncherStartupInfoA si{};
    si.cb = sizeof(si);
    LauncherProcessInformation pi{};

    // Set the child's working directory to the executable directory so it can
    // locate the assets/ folder regardless of where the server was launched from.
    const char* workDir = modulePath[0] ? modulePath : nullptr;

    if (!CreateProcessA(nullptr, cmdLine, nullptr, nullptr, 0, 0,
                        nullptr, workDir, &si, &pi)) {
        std::fprintf(stderr, "JKWindowServer: CreateProcessA failed for %s\n", appName);
        return;
    }

    if (pi.hProcess) CloseHandle(pi.hProcess);
    if (pi.hThread) CloseHandle(pi.hThread);

    std::fprintf(stderr, "JKWindowServer: spawned client %s %s\n",
                 fromJkx ? "--jkx" : "--client", appName);
#else
    (void)appName;
    (void)fromJkx;
    std::fprintf(stderr, "JKWindowServer: SpawnClient is Windows-only in this prototype\n");
#endif // _WIN32
}

} // namespace server
} // namespace jk
