#include <server/JKWindowServer.h>
#include <agent/JKAgentJson.h>

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
#include <ctime>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <map>
#include <thread>
#include <vector>

// stb_image_write (docs/35): single-TU implementation — STBIW_STATIC keeps
// the symbols file-local so other TUs (imgui) are unaffected.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb_image_write.h>

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
extern "C" __declspec(dllimport) int __stdcall GetExitCodeProcess(
    void* hProcess, unsigned long* lpExitCode);
static const unsigned long kStillActiveExit = 259;  // STILL_ACTIVE

extern "C" __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(
    void* hModule, char* lpFilename, unsigned long nSize);

extern "C" __declspec(dllimport) int __stdcall CreateDirectoryA(
    const char* lpPathName, void* lpSecurityAttributes);

extern "C" __declspec(dllimport) int __stdcall WaitNamedPipeA(
    const char* lpNamedPipeName, unsigned long nTimeOut);

extern "C" __declspec(dllimport) void __stdcall Sleep(unsigned long dwMilliseconds);

extern "C" __declspec(dllimport) unsigned long __stdcall GetFileAttributesA(
    const char* lpFileName);

constexpr unsigned long kInvalidFileAttributes = 0xFFFFFFFF;

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

    // Client apps need SDL_TEXTINPUT (Char events) for text fields/IME. In
    // client/single-process modes the app starts text input itself; the
    // server window must too, or no client ever receives a Char event
    // (vplayer path field stayed empty under synthetic typing).
    SDL_StartTextInput();

    compositor_ = std::make_unique<JKCompositor>(renderer_);
    UpdateOutputBounds();
    InitLauncher();

    // Directional cursors for chrome resize hotspots (hover feedback).
    chromeCursors_[static_cast<int>(CursorShape::Arrow)] =
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    chromeCursors_[static_cast<int>(CursorShape::SizeWE)] =
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
    chromeCursors_[static_cast<int>(CursorShape::SizeNS)] =
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
    chromeCursors_[static_cast<int>(CursorShape::SizeNWSE)] =
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
    chromeCursors_[static_cast<int>(CursorShape::SizeNESW)] =
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);

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

#ifdef _WIN32
    // Auto-spawn the shell (docs/28): the taskbar is a privileged client, not
    // an app — the server boots it itself when its module is installed next
    // to the exe. Clients have no connect-retry, so wait for the acceptor's
    // first pipe instance before spawning (bounded ~200 ms).
    for (int i = 0; i < 20; ++i) {
        if (WaitNamedPipeA(pipeName_.c_str(), 20)) break;
        Sleep(10);
    }
    char modulePath[1024] = {};
    const unsigned long len = GetModuleFileNameA(nullptr, modulePath, sizeof(modulePath));
    if (len > 0 && len < sizeof(modulePath)) {
        char* lastSlash = modulePath;
        for (char* p = modulePath; *p; ++p) {
            if (*p == '\\' || *p == '/') lastSlash = p;
        }
        *lastSlash = '\0';
        std::string dllPath = std::string(modulePath[0] ? modulePath : ".") + "\\jkapp_taskbar.dll";
        if (GetFileAttributesA(dllPath.c_str()) != kInvalidFileAttributes) {
            SpawnClient("taskbar");
        } else {
            std::fprintf(stderr, "JKWindowServer: no jkapp_taskbar.dll — desktop runs without a shell\n");
        }
    }
#endif
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

        // Expect Hello. Protocol v2 carries the client's OS pid; a v1 Hello
        // (4-byte payload) is accepted with pid = 0.
        ipc::Message hello;
        if (!ipc::ReadMessage(*transport, hello) || hello.type != ipc::MsgType::Hello) {
            std::fprintf(stderr, "JKWindowServer::AcceptorLoop: expected Hello, got type=%u\n",
                         static_cast<uint32_t>(hello.type));
            continue;
        }
        uint32_t helloPid = 0;
        if (hello.payload.size() >= sizeof(ipc::HelloPayload)) {
            ipc::HelloPayload helloPayload{};
            std::memcpy(&helloPayload, hello.payload.data(), sizeof(helloPayload));
            helloPid = helloPayload.pid;
        }

        // Second message: CreateSurface for a regular window client, or
        // AgentEventSubscribe for a control-only agent connection.
        ipc::Message second;
        if (!ipc::ReadMessage(*transport, second)) {
            std::fprintf(stderr, "JKWindowServer::AcceptorLoop: second message read failed\n");
            continue;
        }

        uint32_t id = nextSurfaceId_++;
        auto client = std::make_unique<JKClientConnection>(id, std::move(transport));
        client->SetPid(helloPid);

        if (second.type == ipc::MsgType::AgentEventSubscribe) {
            // Control-only agent connection (Desktop Agent API, spec §3):
            // skip the surface/shm handshake entirely — pipe-only. Queued
            // through pendingClients_ so registration happens on the main
            // thread like every other client.
            ipc::AgentEventSubscribePayload sub{};
            if (second.payload.size() >= sizeof(sub)) {
                std::memcpy(&sub, second.payload.data(), sizeof(sub));
                client->SetAgentEventSubscriber(sub.subscribe != 0);
            }
            client->SetControlOnly(true);
            client->StartReadThread();
            {
                std::lock_guard<std::mutex> lock(pendingClientsMutex_);
                pendingClients_.push_back(std::move(client));
            }
            std::fprintf(stderr, "JKWindowServer: control-only client %u connected\n", id);
            continue;
        }

        if (second.type != ipc::MsgType::CreateSurface ||
            second.payload.size() < sizeof(ipc::SurfaceCreatePayload)) {
            std::fprintf(stderr, "JKWindowServer::AcceptorLoop: expected CreateSurface\n");
            continue;
        }

        ipc::SurfaceCreatePayload create{};
        std::memcpy(&create, second.payload.data(), sizeof(create));

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

        // Control-only clients have no layer: no placement, no focus, no
        // window-list entry — just join the client table.
        if (client->IsControlOnly()) {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clients_.push_back(std::move(client));
            continue;
        }

        int ww = 0, wh = 0;
        SDL_GetWindowSize(window_, &ww, &wh);
        // Work-area reserve (docs/28): the shell's docked bar height keeps new
        // windows out of the taskbar zone. Computed per batch — the shell is
        // not yet layered when it is placed itself.
        const int reserve = compositor_ ? compositor_->ShellReserveHeight() : 0;
        // Surfaces larger than the desktop (apps designed for 1920x1080) are
        // displayed scaled down to fit; the client keeps rendering at its
        // designed surface size. Chrome zones are proportional to the layer
        // size, so title-drag, the close overlay and resize hotspots keep
        // working under a fit scale.
        const float fit = std::min(1.0f,
            std::min(ww / static_cast<float>(client->Width()),
                     (wh - reserve) / static_cast<float>(client->Height())));
        const int dispW = static_cast<int>(client->Width() * fit);
        const int dispH = static_cast<int>(client->Height() * fit);
        int x = std::max(0, (ww - dispW) / 2) + existingCount * 20;
        int y = std::max(0, (wh - reserve - dispH) / 2) + existingCount * 20;
        // A full-desktop fit layer (dispW == ww) would push its close-button
        // corner past the window edge with the cascade offset — clamp so the
        // whole layer, chrome included, stays inside the work area.
        x = std::min(x, std::max(0, ww - dispW));
        y = std::min(y, std::max(0, wh - reserve - dispH));
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
        // The shell never takes focus (docs/28) — keys stay with app windows.
        if (client->IsShell()) {
            // ShellRegister can beat this intake (client sends it right after
            // the CreateSurface handshake) — SetLayerShell was a no-op then,
            // so (re)apply the role here and dock for real; the centered
            // placement above is overwritten by the bottom-edge dock.
            compositor_->SetLayerShell(client->Id(), true);
            DockShellClient(client.get());
        } else {
            FocusClient(client->Id());
        }

        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            const std::string createdTitle = client->Title();
            const uint32_t createdPid = client->Pid();
            const uint32_t createdId = client->Id();
            clients_.push_back(std::move(client));
            PushAgentEvent("window.created", createdId, createdTitle, createdPid);
        }

        // Shell protocol: the new window shows up in the taskbar.
        PushWindowList();
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

    for (SDL_Cursor* cursor : chromeCursors_) {
        if (cursor) SDL_FreeCursor(cursor);
    }

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

// Minimal JSON string escape for agent replies (quotes, backslash, control
// chars). UTF-8 bytes pass through untouched — titles are KSSM-decoded
// UTF-8 already (taskbar convention).
std::string JsonEsc(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    char num[8];
    for (char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c < 0x20)  { std::snprintf(num, sizeof(num), "\\u%04x", c); out += num; }
        else                out += ch;
    }
    return out;
}

// PNG write for screenshots (docs/35). pixels must be RGBA32, w*h*4 bytes —
// the same layout the client surfaces use in their shm mapping.
bool WritePng(const std::string& path, int w, int h, const uint8_t* px) {
    return stbi_write_png(path.c_str(), w, h, 4, px, w * 4) != 0;
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
            // Keep the dragged window inside the work area: the shell's
            // docked bar stays visible under it (docs/28).
            const int reserve = compositor_ ? compositor_->ShellReserveHeight() : 0;
            nx = std::max(-lw + 40, std::min(nx, std::max(0, winW - 40)));
            ny = std::max(0, std::min(ny, std::max(0, winH - reserve - 40)));
            client->SetPosition(nx, ny);
            compositor_->SetLayerPosition(client->Id(), nx, ny);
        } else {  // Resize: stretch-preview via layer scale.
            int newW = chromeResizeW_;
            int newH = chromeResizeH_;
            int newX = chromeResizeX_;
            int newY = chromeResizeY_;
            if (chromeEdgeRight_) newW = lmX - chromeResizeX_;
            if (chromeEdgeBottom_) newH = lmY - chromeResizeY_;
            if (chromeEdgeLeft_) {
                newW = chromeResizeW_ + (chromeResizeX_ - lmX);
            }
            if (chromeEdgeTop_) {
                newH = chromeResizeH_ + (chromeResizeY_ - lmY);
                // The grab-time bottom edge is fixed — growing past it would
                // push the layer origin above the desktop.
                newH = std::min(newH, chromeResizeY_ + chromeResizeH_);
            }
            newW = std::max(64, newW);
            newH = std::max(48, newH);
            // Absolute display scale = display target / surface width (the
            // surface size does not change until the resize is committed).
            layer->SetScale(newW / static_cast<float>(layer->Width()),
                            newH / static_cast<float>(layer->Height()));
            if (chromeEdgeLeft_ || chromeEdgeTop_) {
                // The fixed (opposite) edge stays put: right edge for a left
                // resize, bottom edge for a top resize.
                if (chromeEdgeLeft_) {
                    newX = chromeResizeX_ + chromeResizeW_ - newW;
                }
                if (chromeEdgeTop_) {
                    newY = chromeResizeY_ + chromeResizeH_ - newH;
                }
                client->SetPosition(newX, newY);
                compositor_->SetLayerPosition(client->Id(), newX, newY);
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
            int newY = chromeResizeY_;
            if (chromeEdgeRight_) newW = lmX - chromeResizeX_;
            if (chromeEdgeBottom_) newH = lmY - chromeResizeY_;
            if (chromeEdgeLeft_) {
                newW = chromeResizeW_ + (chromeResizeX_ - lmX);
            }
            if (chromeEdgeTop_) {
                newH = chromeResizeH_ + (chromeResizeY_ - lmY);
                newH = std::min(newH, chromeResizeY_ + chromeResizeH_);
            }
            newW = std::max(64, newW);
            newH = std::max(48, newH);
            if (newW != chromeResizeW_ || newH != chromeResizeH_) {
                if (chromeEdgeLeft_ || chromeEdgeTop_) {
                    if (chromeEdgeLeft_) {
                        newX = chromeResizeX_ + chromeResizeW_ - newW;
                    }
                    if (chromeEdgeTop_) {
                        newY = chromeResizeY_ + chromeResizeH_ - newH;
                    }
                    client->SetPosition(newX, newY);
                    compositor_->SetLayerPosition(client->Id(), newX, newY);
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
    // The shell has no window chrome (docs/28): no close X, no title drag,
    // no resize edges — clicks fall through to the shell's own UI.
    if (client->IsShell()) {
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
        PushWindowList();  // active highlight follows click focus
        client->Send(ipc::MsgType::Close, nullptr, 0);
        return true;
    }

    // 2) Resize edges: all four sides + corners (6px inset). The top strip's
    //    first 6px are resize; title drag starts below that (Windows-like).
    const bool edgeLeft = (lx < kResizeHotspot);
    const bool edgeRight = (lx >= w - kResizeHotspot);
    const bool edgeBottom = (ly >= h - kResizeHotspot);
    const bool edgeTop = (ly < kResizeHotspot);
    if (edgeLeft || edgeRight || edgeBottom || edgeTop) {
        FocusClient(client->Id());
        PushWindowList();  // active highlight follows click focus
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
        chromeEdgeTop_ = edgeTop;
        SetChromeCursor(ChromeCursorFromEdges(edgeLeft, edgeRight, edgeTop, edgeBottom));
        return true;
    }

    // 3) Title bar: start a move grab (y ∈ [kResizeHotspot, kChromeTitleBar) —
    //    the top resize strip above returned first).
    if (ly < kChromeTitleBar) {
        FocusClient(client->Id());
        PushWindowList();  // active highlight follows click focus
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

JKWindowServer::CursorShape JKWindowServer::ChromeCursorFromEdges(bool left, bool right,
                                                                  bool top, bool bottom) {
    const bool horiz = left || right;
    const bool vert = top || bottom;
    if (horiz && vert) {
        // TL/BR share one diagonal, TR/BL the other.
        return (left == top) ? CursorShape::SizeNWSE : CursorShape::SizeNESW;
    }
    if (horiz) return CursorShape::SizeWE;
    if (vert) return CursorShape::SizeNS;
    return CursorShape::Arrow;
}

void JKWindowServer::SetChromeCursor(CursorShape shape) {
    if (shape == cursorShape_) {
        return;
    }
    SDL_Cursor* cursor = chromeCursors_[static_cast<int>(shape)];
    if (cursor) {
        SDL_SetCursor(cursor);
        cursorShape_ = shape;
    }
}

void JKWindowServer::UpdateChromeHoverCursor(int mx, int my, float scale) {
    CursorShape shape = CursorShape::Arrow;
    if (compositor_ && chromeGrab_ == ChromeGrab::None) {
        JKCompositorLayer* layer = compositor_->HitTest(mx, my);
        JKClientConnection* client = layer ? FindClientById(layer->Id()) : nullptr;
        // The shell has no chrome — its own UI keeps the arrow (docs/28).
        if (layer && client && !client->IsShell()) {
            const int lx = static_cast<int>(std::llround(
                (mx / scale - layer->X()) / layer->ScaleX()));
            const int ly = static_cast<int>(std::llround(
                (my / scale - layer->Y()) / layer->ScaleY()));
            const int w = layer->Width();
            const int h = layer->Height();
            // The close overlay stays a plain arrow even though its corner
            // overlaps the top resize strip.
            const bool inCloseX = (lx >= w - kChromeCloseSize - kChromeCloseMargin) &&
                                  (lx < w - kChromeCloseMargin);
            const bool inCloseY = (ly >= kChromeCloseMargin) &&
                                  (ly < kChromeCloseMargin + kChromeCloseSize);
            if (!(inCloseX && inCloseY)) {
                shape = ChromeCursorFromEdges(lx < kResizeHotspot,
                                              lx >= w - kResizeHotspot,
                                              ly < kResizeHotspot,
                                              ly >= h - kResizeHotspot);
            }
        }
    }
    SetChromeCursor(shape);
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
        // Hover feedback for chrome hotspots (drag-active cursor was already
        // set at grab start and survives until the next free motion).
        if (ev.type == SDL_MOUSEMOTION) {
            UpdateChromeHoverCursor(mx, my, outputScale);
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
            // Clicking the shell does not steal keyboard focus (docs/28).
            if (!client->IsShell()) {
                FocusClient(client->Id());
                PushWindowList();  // active highlight follows click focus
            }
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
        // Alt+Space opens (or refocuses) the command palette (M2a, spec §6.2).
        // A shell-level chord: the server interprets it instead of forwarding.
        if (ev.type == SDL_KEYDOWN && !ev.key.repeat &&
            (ev.key.keysym.mod & KMOD_ALT) &&
            ev.key.keysym.sym == SDLK_SPACE) {
            TogglePalette();
            return;
        }
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
    // Desktop Agent event (spec §4). Resolves nothing when the id is not (yet)
    // in the table (e.g. focus at spawn intake, before push_back).
    for (auto& c : clients_) {
        if (c && c->Id() == surfaceId) {
            PushAgentEvent("window.focused", surfaceId, c->Title(), c->Pid());
            break;
        }
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

    // Keep the shell docked across desktop size changes (SIZE_CHANGED /
    // MOVED / DISPLAY_CHANGED all funnel here).
    DockShellClient(nullptr);
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

    // M2 chat: expire parked approvals — answer the parked query with
    // approval_timeout and broadcast the resolution (same lock scope).
    const time_t now = std::time(nullptr);
    for (auto it = pendingApprovals_.begin(); it != pendingApprovals_.end();) {
        if (now < it->expiresAt) { ++it; continue; }
        for (auto& c : clients_) {
            if (c && c->Id() == it->requesterId && !c->IsDisconnected()) {
                ipc::WriteAgentJson(c->Transport(), ipc::MsgType::AgentReply,
                                    it->queryId, 0,
                                    "{\"ok\":false,\"error\":\"approval_timeout\"}");
                break;
            }
        }
        char buf[256];
        std::snprintf(buf, sizeof(buf),
                      "{\"topic\":\"agent.approval_resolved\","
                      "\"request\":%u,\"decision\":\"timeout\"}",
                      it->requestId);
        PushAgentEventJson(buf);
        it = pendingApprovals_.erase(it);
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
    } else if (msg.type == ipc::MsgType::ShellRegister) {
        // Shell protocol (docs/28): the FIRST client to register becomes the
        // desktop shell (taskbar). Later registrations are ignored while one
        // is active — the shell is a role, not an app.
        bool alreadyShell = false;
        for (const auto& other : clients_) {
            if (other && other->Id() != client.Id() && other->IsShell()) {
                alreadyShell = true;
                break;
            }
        }
        if (alreadyShell) {
            std::fprintf(stderr, "JKWindowServer: surface %u shell register denied (shell already active)\n",
                         client.Id());
            ipc::ShellRegisterAckPayload ack{};  // accepted = 0
            client.Send(ipc::MsgType::ShellRegisterAck, &ack, sizeof(ack));
        } else {
            client.SetShell(true);
            if (compositor_) {
                compositor_->SetLayerShell(client.Id(), true);
            }
            ipc::ShellRegisterAckPayload ack{};
            ack.accepted = 1;
            client.Send(ipc::MsgType::ShellRegisterAck, &ack, sizeof(ack));
            std::fprintf(stderr, "JKWindowServer: surface %u registered as shell\n",
                         client.Id());
            PushWindowListUnsafe();  // initial snapshot (clientsMutex_ held)
            DockShellClient(&client);  // bottom edge + full desktop width
        }
    } else if (msg.type == ipc::MsgType::WindowActivate) {
        if (msg.payload.size() >= sizeof(ipc::WindowActivatePayload)) {
            ipc::WindowActivatePayload payload{};
            std::memcpy(&payload, msg.payload.data(), sizeof(payload));
            if (payload.surfaceId != 0 && payload.surfaceId != client.Id()) {
                // Restore-on-activate: a minimized window comes back first.
                if (compositor_) {
                    compositor_->SetLayerVisible(payload.surfaceId, true);
                }
                FocusClient(payload.surfaceId);
                PushWindowListUnsafe();  // active highlight follows focus
            }
        }
    } else if (msg.type == ipc::MsgType::WindowMinimizeToggle) {
        if (msg.payload.size() >= sizeof(ipc::WindowActivatePayload)) {
            ipc::WindowActivatePayload payload{};
            std::memcpy(&payload, msg.payload.data(), sizeof(payload));
            if (payload.surfaceId != 0 && payload.surfaceId != client.Id() && compositor_) {
                const bool visible = compositor_->IsLayerVisible(payload.surfaceId);
                compositor_->SetLayerVisible(payload.surfaceId, !visible);
                if (visible) {
                    // Hiding it loses focus — hand the keyboard to the next
                    // topmost app window (same fallback as a disconnect).
                    if (focusedClientId_ == payload.surfaceId) {
                        FocusClient(compositor_->TopmostLayerId());
                    }
                }
                PushWindowListUnsafe();  // minimized flag follows visibility
            }
        }
    } else if (msg.type == ipc::MsgType::WindowListSubscribe) {
        if (msg.payload.size() >= sizeof(ipc::WindowListSubscribePayload)) {
            ipc::WindowListSubscribePayload payload{};
            std::memcpy(&payload, msg.payload.data(), sizeof(payload));
            client.SetWindowListSubscriber(payload.subscribe != 0);
            if (payload.subscribe) {
                PushWindowListUnsafe();  // initial snapshot (clientsMutex_ held)
            }
        }
    } else if (msg.type == ipc::MsgType::AgentEventSubscribe) {
        // Subscribe/unsubscribe to desktop event pushes (agent connections).
        if (msg.payload.size() >= sizeof(ipc::AgentEventSubscribePayload)) {
            ipc::AgentEventSubscribePayload payload{};
            std::memcpy(&payload, msg.payload.data(), sizeof(payload));
            client.SetAgentEventSubscriber(payload.subscribe != 0);
        }
    } else if (msg.type == ipc::MsgType::WindowTitle) {
        // C -> S title update (docs/33): the notification center's unread
        // badge. Raw UTF-8 payload, capped so a rogue client can't bloat the
        // window-list entries.
        if (!msg.payload.empty() && msg.payload.size() <= 96) {
            client.SetTitle(std::string(msg.payload.begin(), msg.payload.end()));
            PushWindowListUnsafe();  // taskbar button text follows (mutex held)
        }
    } else if (msg.type == ipc::MsgType::AgentQuery) {
        uint32_t queryId = 0, ok = 0;
        std::string json;
        if (ipc::ReadAgentJson(msg, queryId, ok, json)) {
            HandleAgentQuery(client, queryId, json);
        }
    }
}

void JKWindowServer::PushWindowList() {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    PushWindowListUnsafe();
}

// Caller must hold clientsMutex_: ProcessPendingMessages and
// CleanupDisconnectedClients iterate under it, and std::mutex is not
// recursive.
void JKWindowServer::PushWindowListUnsafe() {
    JKClientConnection* shell = nullptr;
    ipc::WindowListPayload payload{};
    for (auto& c : clients_) {
        if (!c || c->IsDisconnected()) continue;
        if (c->IsShell()) {
            shell = c.get();          // the shell never lists itself
            continue;
        }
        if (c->IsControlOnly()) {
            continue;                 // agent connections are not windows
        }
        if (payload.count < 32) {
            ipc::ShellWindowEntry& entry = payload.windows[payload.count++];
            entry.surfaceId = c->Id();
            entry.flags = 0;
            if (focusedClientId_ == c->Id()) {
                entry.flags |= ipc::kShellWindowActive;
            }
            // Minimized = server-side layer visibility (docs/25 §C reuse).
            if (compositor_ && !compositor_->IsLayerVisible(c->Id())) {
                entry.flags |= ipc::kShellWindowMinimized;
            }
            std::strncpy(entry.title, c->Title().c_str(), sizeof(entry.title) - 1);
            entry.pid = c->Pid();
        }
    }
    if (shell) {
        shell->Send(ipc::MsgType::WindowList, &payload, sizeof(payload));
    }
    // Non-shell subscribers (taskmgr) get the same snapshot; unlike the
    // shell they are regular windows and appear in it.
    for (auto& c : clients_) {
        if (c && !c->IsDisconnected() && !c->IsShell() && c->WantsWindowList()) {
            c->Send(ipc::MsgType::WindowList, &payload, sizeof(payload));
        }
    }
}

// Desktop Agent API (spec §3): normally answers at once — the agent client
// blocks on ReadMessage waiting for the reply with the matching queryId.
// Exception (M2 chat): an "ask"-gated close_window parks its query and replies
// later, when the inline approval resolves.
// Precondition: clientsMutex_ held (called from ProcessClientMessage), so
// iterate clients_ directly — FindClientById/FocusClient-style helpers that
// lock would deadlock on the non-recursive mutex.
void JKWindowServer::HandleAgentQuery(JKClientConnection& client,
                                      uint32_t queryId, const std::string& json) {
    jk::agent::AgentJson req(json);
    std::string tool, reply;
    bool replied = true;
    if (!req.ok() || !req.GetStr("tool", tool)) {
        reply = "{\"ok\":false,\"error\":\"bad_request\"}";
    } else if (tool == "ping") {
        reply = "{\"ok\":true,\"pong\":true}";
    } else if (tool == "list_windows") {
        std::string out = "{\"ok\":true,\"windows\":[";
        bool first = true;
        for (auto& c : clients_) {
            if (!c || c->IsDisconnected() || c->IsControlOnly() || c->IsShell()) continue;
            char item[640];
            std::snprintf(item, sizeof(item),
                "%s{\"id\":%u,\"title\":\"%s\",\"pid\":%u,\"x\":%d,\"y\":%d,"
                "\"w\":%d,\"h\":%d,\"focused\":%s,\"minimized\":%s}",
                first ? "" : ",", c->Id(), JsonEsc(c->Title()).c_str(), c->Pid(),
                c->X(), c->Y(), c->Width(), c->Height(),
                focusedClientId_ == c->Id() ? "true" : "false",
                (compositor_ && !compositor_->IsLayerVisible(c->Id()))
                    ? "true" : "false");
            out += item;
            first = false;
        }
        reply = out + "]}";
    } else if (tool == "focus_window") {
        int id = 0;
        JKClientConnection* target = nullptr;
        if (req.GetObjInt("args", "id", id)) {
            for (auto& c : clients_) {
                if (c && c->Id() == static_cast<uint32_t>(id)) { target = c.get(); break; }
            }
        }
        if (target && !target->IsControlOnly()) {
            // Same restore-on-activate semantics as WindowActivate (docs/28).
            if (compositor_) {
                compositor_->SetLayerVisible(target->Id(), true);
            }
            FocusClient(target->Id());
            PushWindowListUnsafe();
            reply = "{\"ok\":true}";
        } else {
            reply = "{\"ok\":false,\"error\":\"window_not_found\"}";
        }
    } else if (tool == "close_window") {
        int id = 0;
        JKClientConnection* target = nullptr;
        if (req.GetObjInt("args", "id", id)) {
            for (auto& c : clients_) {
                if (c && c->Id() == static_cast<uint32_t>(id)) { target = c.get(); break; }
            }
        }
        if (!target || target->IsControlOnly()) {
            reply = "{\"ok\":false,\"error\":\"window_not_found\"}";
        } else {
            switch (AgentToolAllowed("close_window")) {
                case AgentDecision::Allow: {
                    // Server-initiated close: the client's read loop treats
                    // Close as quit; the disconnect cleanup path then removes
                    // the layer and fires window.destroyed.
                    ipc::WriteMessage(target->Transport(), ipc::MsgType::Close,
                                      std::vector<uint8_t>{});
                    reply = "{\"ok\":true}";
                    break;
                }
                case AgentDecision::Ask: {
                    // M2 chat inline approval: park the query and broadcast
                    // the request — the reply goes out only when the approval
                    // resolves (or the expiry scan answers with timeout).
                    bool subscriber = false;
                    for (auto& c : clients_) {
                        if (c && c->AgentEventSubscriber() && !c->IsDisconnected()) {
                            subscriber = true;
                            break;
                        }
                    }
                    if (!subscriber) {
                        reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
                        break;
                    }
                    PendingApproval p;
                    p.requestId = nextApprovalId_++;
                    p.queryId = queryId;
                    p.requesterId = client.Id();
                    p.targetId = target->Id();
                    p.expiresAt = std::time(nullptr) + 60;
                    char buf[640];
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"close_window\","
                                  "\"target_id\":%u,\"title\":\"%s\",\"ts\":%lld}",
                                  p.requestId, p.targetId,
                                  JsonEsc(target->Title()).c_str(),
                                  static_cast<long long>(std::time(nullptr)) * 1000);
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;  // answered when the approval resolves
                    break;
                }
                case AgentDecision::Deny:
                default:
                    // M2a: the server-side gate denied it — permissions.json
                    // is the approval act (same file the broker reads).
                    reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                    break;
            }
        }
    } else if (tool == "launch_app") {
        std::string app, jkx;
        req.GetObjStr("args", "app", app);
        req.GetObjStr("args", "jkx", jkx);
        if (!app.empty()) {
            SpawnClient(app.c_str(), false);
            reply = "{\"ok\":true}";
        } else if (!jkx.empty()) {
            SpawnClient(jkx.c_str(), true);
            reply = "{\"ok\":true}";
        } else {
            reply = "{\"ok\":false,\"error\":\"missing_app\"}";
        }
    } else if (tool == "open_notify") {
        // docs/33: toggle the notification center — safe UI command, no
        // permission gate (same tier as launch_app).
        if (ToggleClientByTitleUnsafe("Notifications", "notify")) {
            PushWindowListUnsafe();  // taskbar highlight follows the refocus
        }
        reply = "{\"ok\":true}";
    } else if (tool == "save_layout") {
        std::string name;
        if (!req.GetObjStr("args", "name", name) || name.empty()) {
            reply = "{\"ok\":false,\"error\":\"missing_name\"}";
        } else {
            std::string snapshot = "{\"name\":\"" + JsonEsc(name) + "\",\"windows\":[";
            bool first = true;
            int count = 0;
            for (auto& c : clients_) {
                if (!c || c->IsDisconnected() || c->IsControlOnly() || c->IsShell()) continue;
                char item[640];
                std::snprintf(item, sizeof(item),
                    "%s{\"title\":\"%s\",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d}",
                    first ? "" : ",", JsonEsc(c->Title()).c_str(),
                    c->X(), c->Y(), c->Width(), c->Height());
                snapshot += item;
                first = false;
                ++count;
            }
            snapshot += "]}";
            const std::string path = StateDir() + "\\layout_" + name + ".json";
            std::FILE* f = std::fopen(path.c_str(), "wb");
            if (!f) {
                reply = "{\"ok\":false,\"error\":\"write_failed\"}";
            } else {
                std::fwrite(snapshot.data(), 1, snapshot.size(), f);
                std::fclose(f);
                reply = "{\"ok\":true,\"count\":" + std::to_string(count) + "}";
            }
        }
    } else if (tool == "restore_layout") {
        std::string name;
        if (!req.GetObjStr("args", "name", name) || name.empty()) {
            reply = "{\"ok\":false,\"error\":\"missing_name\"}";
        } else {
            const std::string path = StateDir() + "\\layout_" + name + ".json";
            std::FILE* f = std::fopen(path.c_str(), "rb");
            if (!f) {
                reply = "{\"ok\":false,\"error\":\"layout_not_found\"}";
            } else {
                char buf[65536];
                const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
                std::fclose(f);
                buf[n] = '\0';
                jk::agent::AgentJson layout(buf);
                int arrLen = 0, restored = 0;
                std::string unmatched;
                if (!layout.ok() || !layout.GetArraySize("windows", arrLen)) {
                    reply = "{\"ok\":false,\"error\":\"bad_layout\"}";
                } else {
                    for (int i = 0; i < arrLen; ++i) {
                        std::string title;
                        int x = 0, y = 0;
                        layout.GetArrStr("windows", i, "title", title);
                        layout.GetArrInt("windows", i, "x", x);
                        layout.GetArrInt("windows", i, "y", y);
                        if (title.empty()) continue;
                        // Match by title — the stable key a layout snapshot
                        // has (surface ids change across restarts).
                        bool matched = false;
                        for (auto& c : clients_) {
                            if (!c || c->IsDisconnected() || c->IsControlOnly() || c->IsShell()) continue;
                            if (c->Title() != title) continue;
                            c->SetPosition(x, y);
                            if (compositor_) {
                                compositor_->SetLayerPosition(c->Id(), x, y);
                            }
                            matched = true;
                            ++restored;
                            break;
                        }
                        if (!matched) {
                            if (!unmatched.empty()) unmatched += ",";
                            unmatched += "\"" + JsonEsc(title) + "\"";
                        }
                    }
                    reply = "{\"ok\":true,\"restored\":" + std::to_string(restored) +
                            ",\"unmatched\":[" + unmatched + "]}";
                }
            }
        }
    } else if (tool == "publish_event") {
        // M2b trigger scripts: client→subscriber event publishing. Any
        // client may publish; the server stamps the envelope and relays it
        // to every agent-event subscriber. "data" passes through as raw
        // JSON so trigger filters can shape arbitrary payloads.
        std::string topic, data;
        if (!req.GetObjStr("args", "topic", topic) || topic.empty() ||
            topic.size() > 96 || JsonEsc(topic).size() > 96 ||
            !req.GetObjRaw("args", "data", data) || data.empty() ||
            data.size() > 4096) {
            reply = "{\"ok\":false,\"error\":\"bad_request\"}";
        } else {
            char ev[4352];
            std::snprintf(ev, sizeof(ev),
                          "{\"topic\":\"%s\",\"data\":%s,\"ts\":%lld}",
                          JsonEsc(topic).c_str(), data.c_str(),
                          static_cast<long long>(
                              std::chrono::duration_cast<
                                  std::chrono::milliseconds>(
                                  std::chrono::system_clock::now()
                                      .time_since_epoch())
                                  .count()));
            PushAgentEventJson(ev);
            reply = "{\"ok\":true}";
        }
    } else if (tool == "capture_window") {
        // docs/35: read the client's shm surface (RGBA32) directly — the
        // app framebuffer, no screen DPI involvement. Safe tier (same as
        // launch_app); permissions.json can deny "capture_window".
        int id = 0;
        req.GetObjInt("args", "id", id);
        JKCompositorLayer* layer =
            compositor_ ? compositor_->FindLayerById(static_cast<uint32_t>(id))
                        : nullptr;
        if (!layer || !layer->Pixels() || layer->Width() <= 0 ||
            layer->Height() <= 0) {
            reply = "{\"ok\":false,\"error\":\"window_not_found\"}";
        } else {
            const std::string dir = StateDir() + "\\screenshots";
            CreateDirectoryA(dir.c_str(), nullptr);
            const long long ts =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
            const std::string path = dir + "\\shot_" + std::to_string(ts) +
                                     "_" + std::to_string(id) + ".png";
            // Copy out — the client may commit into the shm while encoding.
            const int w = layer->Width(), h = layer->Height();
            std::vector<uint8_t> px(layer->Pixels(),
                                    layer->Pixels() +
                                        static_cast<size_t>(w) * h * 4);
            if (WritePng(path, w, h, px.data())) {
                reply = "{\"ok\":true,\"path\":\"" + JsonEsc(path) + "\"}";
            } else {
                reply = "{\"ok\":false,\"error\":\"write_failed\"}";
            }
        }
    } else if (tool == "trigger_toggle") {
        // docs/34: write state/triggers.json (single source of truth) then
        // publish triggers.reload — jktriggers re-reads the file on the
        // event. Safe tier (no gate), same as launch_app.
        std::string name;
        int on = -1;
        req.GetObjStr("args", "name", name);
        req.GetObjInt("args", "on", on);
        if (name.empty() || on < 0) {
            reply = "{\"ok\":false,\"error\":\"missing_name\"}";
        } else {
            const std::string path = StateDir() + "\\triggers.json";
            std::map<std::string, int> flags;
            std::FILE* f = std::fopen(path.c_str(), "rb");
            if (f) {
                char buf[4096] = {};
                const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
                std::fclose(f);
                jk::agent::AgentJson json(buf);
                int cnt = 0;
                if (json.ok() && json.GetArraySize("triggers", cnt)) {
                    for (int i = 0; i < cnt && i < 64; ++i) {
                        std::string nm;
                        int en = 1;
                        if (json.GetArrStr("triggers", i, "name", nm)) {
                            json.GetArrInt("triggers", i, "enabled", en);
                            flags[nm] = en;
                        }
                    }
                }
            }
            flags[name] = on;
            std::string out = "{\"triggers\":[";
            bool first = true;
            for (const auto& kv : flags) {
                if (!first) out += ",";
                first = false;
                out += "{\"name\":\"" + JsonEsc(kv.first) + "\",\"enabled\":" +
                       std::to_string(kv.second) + "}";
            }
            out += "]}";
            reply = "{\"ok\":true}";
            if (std::FILE* w = std::fopen(path.c_str(), "wb")) {
                std::fwrite(out.data(), 1, out.size(), w);
                std::fclose(w);
            } else {
                reply = "{\"ok\":false,\"error\":\"write_failed\"}";
            }
            if (reply.find("\"ok\":true") != std::string::npos) {
                char ev[128];
                std::snprintf(ev, sizeof(ev),
                              "{\"topic\":\"triggers.reload\",\"data\":{},"
                              "\"ts\":%lld}",
                              static_cast<long long>(
                                  std::chrono::duration_cast<
                                      std::chrono::milliseconds>(
                                      std::chrono::system_clock::now()
                                          .time_since_epoch())
                                      .count()));
                PushAgentEventJson(ev);
            }
        }
    } else if (tool == "trigger_list") {
        // docs/34: flat rows from jktriggers' loaded manifest merged with
        // the flags file (missing entry = enabled).
        std::map<std::string, std::pair<std::vector<std::string>, int>> merged;
        auto readState = [&](const char* file, bool isManifest) {
            std::FILE* f =
                std::fopen((StateDir() + "\\" + file).c_str(), "rb");
            if (!f) return;
            char buf[8192] = {};
            const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
            std::fclose(f);
            jk::agent::AgentJson json(buf);
            int cnt = 0;
            if (!json.ok() || !json.GetArraySize("triggers", cnt)) return;
            for (int i = 0; i < cnt && i < 256; ++i) {
                std::string nm, topic;
                if (!json.GetArrStr("triggers", i, "name", nm)) continue;
                auto& entry = merged[nm];
                if (isManifest) {
                    // operator[] default is 0 — absent flags mean enabled.
                    entry.second = 1;
                    if (json.GetArrStr("triggers", i, "topic", topic) &&
                        !topic.empty()) {
                        entry.first.push_back(topic);
                    }
                } else {
                    int en = 1;
                    json.GetArrInt("triggers", i, "enabled", en);
                    entry.second = en;
                }
            }
        };
        readState("triggers_loaded.json", true);
        readState("triggers.json", false);
        std::string out = "{\"ok\":true,\"triggers\":[";
        bool first = true;
        for (const auto& kv : merged) {
            if (!first) out += ",";
            first = false;
            out += "{\"name\":\"" + JsonEsc(kv.first) + "\",\"topics\":[";
            for (size_t i = 0; i < kv.second.first.size(); ++i) {
                if (i) out += ",";
                out += "\"" + JsonEsc(kv.second.first[i]) + "\"";
            }
            out += "],\"enabled\":" + std::to_string(kv.second.second) + "}";
        }
        reply = out + "]}";
    } else if (tool == "launch_chat") {
        // M2 chat: open the Win32 chat window (approval surface). One API,
        // many faces — MCP agents can open it too.
        SpawnProcess("jkchat.exe", "");
        reply = "{\"ok\":true}";
    } else if (tool == "approve") {
        // M2 chat: resolve one pending approval. The parked query's reply
        // goes to the ORIGINAL requester; the approver gets the ack below.
        int request = 0;
        std::string decision;
        req.GetObjInt("args", "request", request);
        req.GetObjStr("args", "decision", decision);
        const bool allow = (decision == "allow");
        bool resolved = false;
        for (auto it = pendingApprovals_.begin();
             it != pendingApprovals_.end(); ++it) {
            if (it->requestId != static_cast<uint32_t>(request)) continue;
            resolved = true;
            if (allow) {
                for (auto& c : clients_) {
                    if (c && c->Id() == it->targetId && !c->IsDisconnected()) {
                        ipc::WriteMessage(c->Transport(), ipc::MsgType::Close,
                                          std::vector<uint8_t>{});
                        break;
                    }
                }
            }
            for (auto& c : clients_) {
                if (c && c->Id() == it->requesterId && !c->IsDisconnected()) {
                    const std::string result = allow
                        ? "{\"ok\":true}"
                        : "{\"ok\":false,\"error\":\"denied_by_user\"}";
                    ipc::WriteAgentJson(c->Transport(), ipc::MsgType::AgentReply,
                                        it->queryId, allow ? 1 : 0, result);
                    break;
                }
            }
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "{\"topic\":\"agent.approval_resolved\","
                          "\"request\":%u,\"decision\":\"%s\"}",
                          it->requestId, allow ? "allow" : "deny");
            PushAgentEventJson(buf);
            pendingApprovals_.erase(it);
            reply = allow ? "{\"ok\":true,\"approved\":true}"
                          : "{\"ok\":true,\"approved\":false}";
            break;
        }
        if (!resolved) reply = "{\"ok\":false,\"error\":\"unknown_request\"}";
    } else {
        reply = "{\"ok\":false,\"error\":\"not_implemented\"}";
    }
    // The ask path parks the query — its reply is sent when the approval
    // resolves (approve tool) or times out (expiry scan below).
    if (replied) {
        ipc::WriteAgentJson(client.Transport(), ipc::MsgType::AgentReply,
                            queryId, 1, reply);
    }
}

// Push a desktop event JSON to every subscribed control-only client.
// Callers hold clientsMutex_ (the call sites do).
void JKWindowServer::PushAgentEvent(const char* topic, uint32_t id,
                                    const std::string& title, uint32_t pid) {
    const long long ts = static_cast<long long>(std::time(nullptr)) * 1000;
    char buf[640];
    std::snprintf(buf, sizeof(buf),
                  "{\"topic\":\"%s\",\"id\":%u,\"title\":\"%s\",\"pid\":%u,\"ts\":%lld}",
                  topic, id, JsonEsc(title).c_str(), pid, ts);
    PushAgentEventJson(buf);
}

// Push a fully-formed agent event JSON (topic included) to subscribers.
void JKWindowServer::PushAgentEventJson(const std::string& json) {
    for (auto& c : clients_) {
        // M2a: window clients can opt in too (the palette does, via
        // AgentEventSubscribe on its regular connection); control-only agent
        // connections declare the flag at connect time.
        if (c && c->AgentEventSubscriber() && !c->IsDisconnected()) {
            ipc::WriteAgentJson(c->Transport(), ipc::MsgType::AgentEvent, 0, 1, json);
        }
    }
}

// Alt+Space (M2a): bring the palette to front if it is already open,
// otherwise spawn it.
void JKWindowServer::TogglePalette() {
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        found = ToggleClientByTitleUnsafe("Command Palette", "palette");
    }
    if (found) {
        PushWindowList();  // taskbar active highlight follows the refocus
    }
}

// Title-match toggle core (docs/33), caller holds clientsMutex_ (the
// AgentQuery hot path pre-condition — the non-recursive mutex deadlocks if
// we lock here). Focuses the existing client or spawns a new one.
// Returns true when an existing client was focused (caller may push the
// window list).
namespace {
// The notify app appends " (N)" to its own title via MsgType::WindowTitle
// (unread badge), so an exact-match toggle loses the key after the first
// badge update and spawns a duplicate — accept the bare title or the
// badged form "key (…)".
bool TitleMatchesToggleKey(const std::string& actual, const char* key) {
    const std::string k(key);
    if (actual == k) return true;
    return actual.size() > k.size() + 2 &&
           actual.rfind(k + " (", 0) == 0 && actual.back() == ')';
}
} // namespace

bool JKWindowServer::ToggleClientByTitleUnsafe(const char* title,
                                               const char* app) {
    for (auto& c : clients_) {
        if (!c || c->IsDisconnected() || c->IsControlOnly() || c->IsShell()) {
            continue;
        }
        if (TitleMatchesToggleKey(c->Title(), title)) {
            if (compositor_) compositor_->SetLayerVisible(c->Id(), true);
            FocusClient(c->Id());  // caller-holds-clientsMutex_ contract
            return true;
        }
    }
    SpawnClient(app);  // no lock inside — launch_app precedent
    return false;
}

// M2a server-side permission gate (spec §5): the broker (jkagentd) gates its
// own tool calls, but any connected face can also send AgentQuery directly
// (the palette does over its window connection). close_window is denied by
// default; <exeDir>\permissions.json — the same file the broker reads, both
// exes live in the same build directory — is the approval act.
// M2 chat: "ask" means the inline-approval pipeline (chat window) — wired for
// close_window; other tools degrade to allow since nothing parks them.
AgentDecision JKWindowServer::AgentToolAllowed(const std::string& tool) const {
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    const std::string path = dir + "\\permissions.json";
    const bool defaultAllowed = (tool != "close_window");
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return defaultAllowed ? AgentDecision::Allow : AgentDecision::Deny;
    char buf[4096] = {};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    jk::agent::AgentJson perm(buf);
    std::string value;
    if (!perm.ok() || !perm.GetStr(tool.c_str(), value)) {
        return defaultAllowed ? AgentDecision::Allow : AgentDecision::Deny;
    }
    if (value == "allow") return AgentDecision::Allow;
    if (value == "ask") {
        return (tool == "close_window") ? AgentDecision::Ask
                                        : AgentDecision::Allow;
    }
    return value == "deny" ? AgentDecision::Deny
                           : (defaultAllowed ? AgentDecision::Allow
                                             : AgentDecision::Deny);
}

// <exeDir>/state — agent-created files (layout snapshots). CreateDirectoryA
// fails harmlessly when the directory already exists.
std::string JKWindowServer::StateDir() const {
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    dir = (slash == std::string::npos) ? std::string(".") : dir.substr(0, slash);
    dir += "\\state";
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
}

JKClientConnection* JKWindowServer::FindShellClient() {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& c : clients_) {
        if (c && !c->IsDisconnected() && c->IsShell()) {
            return c.get();
        }
    }
    return nullptr;
}

// Dock the shell to the bottom edge: surface width = desktop logical width
// (ResizeSurface via the chrome-resize machinery), position (0, wh - h).
// Re-docked on every desktop size change (UpdateOutputBounds).
void JKWindowServer::DockShellClient(JKClientConnection* shell) {
    if (!compositor_ || !window_) {
        return;
    }
    if (!shell) {
        shell = FindShellClient();
    }
    if (!shell) {
        return;
    }

    int ww = 0, wh = 0;
    SDL_GetWindowSize(window_, &ww, &wh);
    const uint32_t id = shell->Id();
    const int barH = shell->Height();  // the client decides the thickness

    // Re-resize only when the desktop width actually changed (a ResizeSurface
    // forces the client to remap shared memory and re-layout).
    if (shell->Width() != ww) {
        CommitChromeResize(*shell, id, ww, barH, ww, barH);
    }
    compositor_->SetLayerPosition(id, 0, wh - barH);
    // Keep the connection-side position in sync: the input mapping converts
    // the mouse with client->X()/Y() (ProcessPendingClients stores the spawn
    // placement there), while drawing reads the compositor layer. Updating
    // only the layer left the input map at the centered spawn position, so
    // taskbar button clicks landed at y=361 on a 40px-tall surface and were
    // dropped — the bar rendered fine but every button was dead.
    shell->SetPosition(0, wh - barH);
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
                // Desktop Agent event (spec §4) — app windows only: the shell
                // and control-only agents are not listable windows, so their
                // teardown is not a desktop event. Captured before the move.
                if (!client->IsControlOnly() && !client->IsShell()) {
                    // Crash detection (M2b): a spawned client whose process
                    // exited non-zero did not leave gracefully. pids not in
                    // the spawn table (externally started clients) read as
                    // graceful — we can only judge what we spawned.
                    bool crashed = false;
                    auto sh = spawnedClients_.find(client->Pid());
                    if (sh != spawnedClients_.end()) {
                        unsigned long code = 0;
                        if (GetExitCodeProcess(sh->second, &code) &&
                            code != kStillActiveExit && code != 0) {
                            crashed = true;
                        }
                        CloseHandle(sh->second);
                        spawnedClients_.erase(sh);
                    }
                    PushAgentEvent(crashed ? "app.crashed" : "window.destroyed",
                                   client->Id(), client->Title(),
                                   client->Pid());
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

        // Shell protocol: the dead window disappears from the taskbar.
        if (!disconnected.empty()) {
            PushWindowListUnsafe();
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

// Launch an arbitrary exe from the server's directory (SpawnClient core).
// throttleKey defaults to exeName; SpawnClient keeps the per-app key so two
// DIFFERENT apps can still launch back-to-back.
void JKWindowServer::SpawnProcess(const char* exeName, const std::string& args,
                                  const char* throttleKey) {
#ifdef _WIN32
    const char* key = throttleKey ? throttleKey : exeName;
    // Throttle repeated spawns for the same key to avoid launching many
    // copies from a single double-click.
    {
        auto now = std::chrono::steady_clock::now();
        auto it = lastSpawnTimes_.find(key);
        if (it != lastSpawnTimes_.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - it->second);
            if (elapsed.count() < 500) {
                std::fprintf(stderr,
                             "JKWindowServer: ignoring rapid spawn for %s (%lld ms)\n",
                             key, static_cast<long long>(elapsed.count()));
                return;
            }
        }
        lastSpawnTimes_[key] = now;
    }

    // Assume the server executable is in the same directory as the target.
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
    if (args.empty()) {
        std::snprintf(cmdLine, sizeof(cmdLine), "\"%s\\%s\"",
                      modulePath[0] ? modulePath : ".", exeName);
    } else {
        std::snprintf(cmdLine, sizeof(cmdLine), "\"%s\\%s\" %s",
                      modulePath[0] ? modulePath : ".", exeName, args.c_str());
    }

    LauncherStartupInfoA si{};
    si.cb = sizeof(si);
    LauncherProcessInformation pi{};

    // Set the child's working directory to the executable directory so it can
    // locate the assets/ folder regardless of where the server was launched from.
    const char* workDir = modulePath[0] ? modulePath : nullptr;

    if (!CreateProcessA(nullptr, cmdLine, nullptr, nullptr, 0, 0,
                        nullptr, workDir, &si, &pi)) {
        std::fprintf(stderr, "JKWindowServer: CreateProcessA failed for %s\n", exeName);
        return;
    }

    // Keep the child's process handle for crash classification (M2b): when
    // the spawned client disconnects, CleanupDisconnectedClients checks the
    // exit code and emits app.crashed for non-zero exits. The thread handle
    // is never needed again.
    if (pi.hProcess) spawnedClients_[pi.dwProcessId] = pi.hProcess;
    if (pi.hThread) CloseHandle(pi.hThread);

    std::fprintf(stderr, "JKWindowServer: spawned %s %s\n", exeName, args.c_str());
#else
    (void)exeName;
    (void)args;
    std::fprintf(stderr, "JKWindowServer: SpawnProcess is Windows-only in this prototype\n");
#endif // _WIN32
}

void JKWindowServer::SpawnClient(const char* appName, bool fromJkx) {
#ifdef _WIN32
    if (fromJkx) {
        // A .jkx container path — may contain spaces, so quote it.
        std::string arg = std::string("--jkx \"") + appName + "\"";
        SpawnProcess("jkdesktop.exe", arg, appName);
    } else {
        SpawnProcess("jkdesktop.exe", std::string("--client ") + appName, appName);
    }
#else
    (void)appName;
    (void)fromJkx;
    std::fprintf(stderr, "JKWindowServer: SpawnClient is Windows-only in this prototype\n");
#endif // _WIN32
}

} // namespace server
} // namespace jk
