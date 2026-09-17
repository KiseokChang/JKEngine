#include <server/JKWindowServer.h>
#include <agent/JKAgentJson.h>

#include <apps/AppLauncherItem.h>
#include <desktop/JKDesktopShell.h>
#include <JKAudioCommand.h>
#include <JKAudioThread.h>
#include <JKImageLoader.h>
#include <JKMessageBus.h>
#include <JKSDLAudioBackend.h>
#include <JKSoundManager.h>
#include <JKPlatform.h>
#include <theme/JKTheme.h>

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

// W variant (docs/48 후속 CP949 레저): the A variants round-trip the command
// line through CP_ACP (CP949 on Korean Windows), mangling UTF-8 args — the
// filedlg json filter/title Korean labels arrived corrupted. Spawn wide:
// convert UTF-8 args to UTF-16 here and let the child's wmain entry
// (main.cpp) convert back with CP_UTF8.
struct LauncherStartupInfoW {
    unsigned long cb = 0;
    wchar_t* lpReserved = nullptr;
    wchar_t* lpDesktop = nullptr;
    wchar_t* lpTitle = nullptr;
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

extern "C" __declspec(dllimport) int __stdcall CreateProcessW(
    const wchar_t* lpApplicationName,
    wchar_t* lpCommandLine,
    void* lpProcessAttributes,
    void* lpThreadAttributes,
    int bInheritHandles,
    unsigned long dwCreationFlags,
    void* lpEnvironment,
    const wchar_t* lpCurrentDirectory,
    LauncherStartupInfoW* lpStartupInfo,
    LauncherProcessInformation* lpProcessInformation);

extern "C" __declspec(dllimport) int __stdcall MultiByteToWideChar(
    unsigned int codePage, unsigned long dwFlags, const char* lpMultiByteStr,
    int cbMultiByte, wchar_t* lpWideCharStr, int cchWideChar);

extern "C" __declspec(dllimport) int __stdcall CloseHandle(void* hObject);
extern "C" __declspec(dllimport) int __stdcall GetExitCodeProcess(
    void* hProcess, unsigned long* lpExitCode);
static const unsigned long kStillActiveExit = 259;  // STILL_ACTIVE

extern "C" __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(
    void* hModule, char* lpFilename, unsigned long nSize);

extern "C" __declspec(dllimport) unsigned long __stdcall GetModuleFileNameW(
    void* hModule, wchar_t* lpFilename, unsigned long nSize);

extern "C" __declspec(dllimport) int __stdcall CreateDirectoryA(
    const char* lpPathName, void* lpSecurityAttributes);

extern "C" __declspec(dllimport) int __stdcall WaitNamedPipeA(
    const char* lpNamedPipeName, unsigned long nTimeOut);

extern "C" __declspec(dllimport) void __stdcall Sleep(unsigned long dwMilliseconds);

extern "C" __declspec(dllimport) unsigned long __stdcall GetFileAttributesA(
    const char* lpFileName);

constexpr unsigned long kInvalidFileAttributes = 0xFFFFFFFF;
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
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI |
        SDL_WINDOW_RESIZABLE);
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

    // P1 ③: the launcher is the in-process privileged shell (spec D7) — the
    // shell owns the grid + background; the server only supplies host
    // services through ShellHost (renderer, scale, texture factory, spawn).
    jk::desktop::JKDesktopShell::ShellHost shellHost;
    shellHost.renderer = renderer_;
    shellHost.outputScale = [this]() {
        return compositor_ ? compositor_->OutputScale() : 1.0f;
    };
    shellHost.makeTexture = [this](const jk::LoadedImage& img, const char* label) {
        return TextureFromRGBA(img, label);
    };
    shellHost.launch = [this](const char* app, bool fromJkx) {
        SpawnClient(app, fromJkx);
    };
    // 콘솔 앱 스폰 (P4 SDK §3): 터미널 위에 cmd — cwd는 앱 폴더. SpawnProcess가
    // 인용을 만들므로 cmd/cwd에 뒤따르는 백슬래시가 없어야 한다(453a327 레슨) —
    // 매니페스트는 상대경로 규칙으로 이를 보장한다.
    shellHost.spawnConsole = [this](const std::string& cmd, const std::string& cwd,
                                    const std::string& name) {
        SpawnConsoleApp(cmd, cwd, name);
    };
    shell_ = std::make_unique<jk::desktop::JKDesktopShell>();
    shell_->Init(shellHost);

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
        } else if (client->Title() == kCaptureOverlayTitle) {
            // Consume the pending launch_app requester — this overlay now
            // knows which client to hide during its own capture_region.
            if (pendingSnapSpawnerConnId_ != 0) {
                overlaySpawner_[client->Id()] = pendingSnapSpawnerConnId_;
                pendingSnapSpawnerConnId_ = 0;
            }
            // docs/35: the rubber-band capture overlay always covers the
            // whole desktop, taskbar included — it is momentary (dismissed
            // by mouse-up or ESC) so it neither reserves work area nor keeps
            // its meta size. The meta size is a placeholder: the same
            // ResizeSurface round-trip DockShellClient uses dictates the
            // real (output) size, and the layer sits at (0,0) with scale 1
            // so the client's drag coords are desktop logical coords.
            CommitChromeResize(*client, client->Id(), ww, wh, ww, wh);
            compositor_->SetLayerPosition(client->Id(), 0, 0);
            client->SetPosition(0, 0);
            FocusClient(client->Id());
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

    if (shell_) {
        shell_->Destroy();
        shell_.reset();
    }
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
        chromeRestorePendingId_ = 0;
        return true;
    }

    const int lmX = static_cast<int>(std::llround(mx / scale));
    const int lmY = static_cast<int>(std::llround(my / scale));

    if (ev.type == SDL_MOUSEBUTTONDOWN) {
        return true;  // other buttons during a drag are consumed
    }

    if (ev.type == SDL_MOUSEMOTION) {
        // Deferred drag-restore (docs/39 fix 1): the first motion of a grab
        // that was started on a maximized layer restores it (single
        // window.restored event), then the grab anchors are re-derived from
        // the restored geometry before the normal logic below runs.
        if (chromeRestorePendingId_ != 0) {
            // Drag threshold (docs/39 final review): restore only once the
            // accumulated motion from grab start exceeds kResizeHotspot (the
            // edge-hotspot 6px). A real hand's double-click has ~1px jitter
            // between its clicks — that must NOT trigger the restore, or the
            // second click (clicks==2) would re-maximize the now-normal
            // window. Below threshold the motion is consumed and ignored.
            const int adx = lmX > chromeGrabStartX_
                                ? lmX - chromeGrabStartX_
                                : chromeGrabStartX_ - lmX;
            const int ady = lmY > chromeGrabStartY_
                                ? lmY - chromeGrabStartY_
                                : chromeGrabStartY_ - lmY;
            if (std::max(adx, ady) <= kResizeHotspot) {
                return true;
            }
            if (chromeRestorePendingId_ == layer->Id() &&
                preMaxRects_.count(chromeRestorePendingId_) != 0) {
                RestoreFromMaximize(*client, *layer);
                chromeRestorePendingId_ = 0;
                const int dispW = static_cast<int>(std::llround(
                    layer->Width() * layer->ScaleX()));
                const int dispH = static_cast<int>(std::llround(
                    layer->Height() * layer->ScaleY()));
                if (chromeGrab_ == ChromeGrab::Move) {
                    // Put the restored window under the cursor: keep the
                    // fractional grab point inside the window.
                    chromeGrabDX_ = static_cast<int>(std::llround(
                        chromeGrabFX_ * dispW));
                    chromeGrabDY_ = static_cast<int>(std::llround(
                        chromeGrabFY_ * dispH));
                } else {  // Resize: re-evaluate the hotspots on the restored
                          // rect — the maximized-geometry edges are stale.
                    const int dx = lmX - layer->X();
                    const int dy = lmY - layer->Y();
                    const bool eLeft = (dx < kResizeHotspot);
                    const bool eRight = (dx >= dispW - kResizeHotspot);
                    const bool eTop = (dy < kResizeHotspot);
                    const bool eBottom = (dy >= dispH - kResizeHotspot);
                    if (!(eLeft || eRight || eTop || eBottom)) {
                        // The click no longer sits on any resize edge after
                        // restore — cancel the grab, window stays restored
                        // and grab-free.
                        chromeGrab_ = ChromeGrab::None;
                        chromeGrabClient_ = 0;
                        chromeGrabLayerId_ = 0;
                        chromeRestorePendingId_ = 0;
                        SetChromeCursor(CursorShape::Arrow);
                        return true;
                    }
                    chromeEdgeLeft_ = eLeft;
                    chromeEdgeRight_ = eRight;
                    chromeEdgeTop_ = eTop;
                    chromeEdgeBottom_ = eBottom;
                    chromeResizeX_ = layer->X();
                    chromeResizeY_ = layer->Y();
                    chromeResizeW_ = dispW;
                    chromeResizeH_ = dispH;
                    SetChromeCursor(ChromeCursorFromEdges(
                        eLeft, eRight, eTop, eBottom));
                }
            } else {
                // Stale pending id (grab state mismatch) — drop it.
                chromeRestorePendingId_ = 0;
            }
        }
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
        // A click without motion never restores (Windows parity): drop the
        // armed restore so a later unrelated grab cannot fire it.
        chromeRestorePendingId_ = 0;
        return true;
    }

    return true;
}

bool JKWindowServer::TryChromeGrab(int mx, int my, float scale, int clicks) {
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
    // no resize edges — clicks fall through to the shell's own UI. The
    // capture overlay (docs/35) likewise: a title-bar grab over its top
    // strip would swallow the first 24 px of the rubber band.
    if (client->IsShell() || client->Title() == kCaptureOverlayTitle) {
        return false;
    }

    // 전체화면 레이어(vplayer 스펙 §2.1)는 크롬이 없다 — 상단 24pt 포함 모든
    // 클릭이 앱에 도달한다. 클릭 포커스는 1241행 일반 경로라 살아 있다.
    if (layer->IsFullscreen()) {
        return false;
    }

    // Chrome zones are in SURFACE-local px (they shrink proportionally on
    // fit-scaled layers, §7.3), so convert display px → surface px here.
    // For 1:1 layers ScaleX/Y == 1 and this is the plain logical-local map.
    // The deferred drag-restore (zone 1d) keeps maximized geometry here; the
    // grab anchors are re-derived from the restored layer on the first motion.
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

    // 1b) Maximize/restore button (left of the close X, server-drawn — docs/39).
    const int maxBtnX0 = w - kChromeCloseMargin - kChromeCloseSize -
                         kChromeMaximizeGap - kChromeMaximizeSize;
    const bool inMaxX = (lx >= maxBtnX0) && (lx < maxBtnX0 + kChromeMaximizeSize);
    if (inMaxX && inCloseY) {
        FocusClient(client->Id());
        PushWindowList();  // active highlight follows click focus
        ToggleMaximize(*client, *layer);
        return true;
    }

    // 1c) Title double-click toggles maximize/restore (docs/39) — checked
    // before the deferred restore is armed in zone 1d, so a maximized
    // window's double-click (second click, still maximized) toggles exactly
    // ONCE, Windows-like. The top resize strip stays a resize zone (a move
    // grab never starts there either).
    if (ly >= kResizeHotspot && ly < kChromeTitleBar && clicks == 2) {
        FocusClient(client->Id());
        PushWindowList();  // active highlight follows click focus
        ToggleMaximize(*client, *layer);
        return true;
    }

    // 1d) Deferred drag-restore (docs/39 fix 1): a Move or Resize grab on a
    // maximized layer only ARMS the restore (chromeRestorePendingId_) — the
    // restore fires on the grab's FIRST motion (HandleChromeGrab), so a plain
    // double-click still reaches zone 1c while maximized and toggles exactly
    // once. Anchors below use the maximized geometry; they are discarded and
    // re-derived from the restored layer on the first motion.
    const bool grabMaximized = (preMaxRects_.count(layer->Id()) != 0);

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
        // Deferred drag-restore (docs/39 fix 1): re-armed per grab.
        chromeRestorePendingId_ = grabMaximized ? layer->Id() : 0;
        chromeGrabStartX_ = static_cast<int>(std::llround(mx / scale));
        chromeGrabStartY_ = static_cast<int>(std::llround(my / scale));
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
        // Deferred drag-restore (docs/39 fix 1): re-armed per grab.
        chromeRestorePendingId_ = grabMaximized ? layer->Id() : 0;
        chromeGrabStartX_ = static_cast<int>(std::llround(mx / scale));
        chromeGrabStartY_ = static_cast<int>(std::llround(my / scale));
        // Move works in desktop-logical positions, but lx/ly are surface-local
        // and a fit-scaled layer maps surface px to logical px at ScaleX/Y.
        chromeGrabDX_ = static_cast<int>(std::llround(lx * layer->ScaleX()));
        chromeGrabDY_ = static_cast<int>(std::llround(ly * layer->ScaleY()));
        // Fractional grab point inside the surface — the anchor a deferred
        // drag-restore re-uses to put the restored window under the cursor.
        chromeGrabFX_ = (w > 0) ? (lx / static_cast<float>(w)) : 0.0f;
        chromeGrabFY_ = (h > 0) ? (ly / static_cast<float>(h)) : 0.0f;
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

// docs/39: window.maximized / window.restored envelope — id/title at top
// level like the PushAgentEvent sites (window.created / window.destroyed),
// minus pid (no process change). Emitted from the chrome path on the server
// loop thread: the same locking regime as FocusClient's PushAgentEvent call
// (clientsMutex_ is NOT held here, matching that existing call site).
void JKWindowServer::PushMaximizeEvent(const char* topic, JKClientConnection& client) {
    char buf[640];
    std::snprintf(buf, sizeof(buf),
                  "{\"topic\":\"%s\",\"id\":%u,\"title\":\"%s\",\"ts\":%lld}",
                  topic, client.Id(), JsonEsc(client.Title()).c_str(),
                  static_cast<long long>(std::time(nullptr)) * 1000);
    PushAgentEventJson(buf);
}

// docs/39: chrome maximize/restore toggle (button click or title
// double-click). The state map preMaxRects_ is the single source of truth:
// presence = maximized; JKCompositorLayer::SetMaximized mirrors it only so
// the compositor draw path can pick the button glyph.
void JKWindowServer::ToggleMaximize(JKClientConnection& client, JKCompositorLayer& layer) {
    if (preMaxRects_.count(layer.Id()) != 0) {
        RestoreFromMaximize(client, layer);
        return;
    }
    if (!compositor_ || !window_) {
        return;
    }
    // Capture the pre-maximize rect BEFORE the resize below replaces the
    // surface: layer origin, surface size, and the on-screen display size
    // (a fit-scaled layer shows a shrunk surface).
    MaxState saved;
    saved.x = layer.X();
    saved.y = layer.Y();
    saved.surfW = layer.Width();
    saved.surfH = layer.Height();
    saved.dispW = static_cast<int>(std::llround(saved.surfW * layer.ScaleX()));
    saved.dispH = static_cast<int>(std::llround(saved.surfH * layer.ScaleY()));

    int ww = 0, wh = 0;
    SDL_GetWindowSize(window_, &ww, &wh);
    // Work-area reserve (docs/28): a maximized window must not cover the
    // taskbar.
    const int reserve = compositor_->ShellReserveHeight();
    // Grow the surface to the whole work area via the same machinery a
    // resize drag uses (CommitChromeResize): shared-memory remap + layer
    // texture swap. dispW/dispH equal the new surface size, so the layer
    // scale resets to 1 — the window is drawn 1:1 across the work area.
    CommitChromeResize(client, layer.Id(), ww, wh - reserve, ww, wh - reserve);
    preMaxRects_[layer.Id()] = saved;
    compositor_->SetLayerPosition(layer.Id(), 0, 0);
    // Keep the connection-side position in sync: the input mapping reads
    // client->X()/Y() while drawing reads the compositor layer (see
    // DockShellClient for the same pairing).
    client.SetPosition(0, 0);
    layer.SetMaximized(true);
    PushMaximizeEvent("window.maximized", client);
}

// Shared restore core: pre-maximize rect back, map entry erased, flag
// cleared, window.restored published exactly once. Also used as the
// drag-restore step before a Move/Resize grab starts on a maximized layer.
bool JKWindowServer::RestoreFromMaximize(JKClientConnection& client,
                                         JKCompositorLayer& layer) {
    auto it = preMaxRects_.find(layer.Id());
    if (it == preMaxRects_.end()) {
        return false;  // not maximized
    }
    const MaxState saved = it->second;
    preMaxRects_.erase(it);
    // Shrink the surface back to the pre-maximize size (CommitChromeResize
    // restores the saved fit via dispW/dispH) and put the layer back where
    // it was, both on the compositor side and the connection side.
    CommitChromeResize(client, layer.Id(), saved.surfW, saved.surfH,
                       saved.dispW, saved.dispH);
    compositor_->SetLayerPosition(layer.Id(), saved.x, saved.y);
    client.SetPosition(saved.x, saved.y);
    layer.SetMaximized(false);
    PushMaximizeEvent("window.restored", client);
    return true;
}

// vplayer 전체화면(스펙 2026-09-17 vplayer-fullscreen-osd §2.1): maximize
// 형제. 최대화 중이면 먼저 복원에서 출발(Windows 관례)하고, 전체 출력 크기로
// CommitChromeResize — 작업 영역 예약 없음(전체화면은 작업표시줄을 덮는다,
// 표준). 크롬 스킵은 layer.SetFullscreen 거울 + TryChromeGrab / Composite /
// UpdateChromeHoverCursor의 플래그 검사 3곳.
void JKWindowServer::ToggleFullscreen(JKClientConnection& client,
                                      JKCompositorLayer& layer, bool on) {
    if (!on) {
        RestoreFullscreen(client, layer);
        return;
    }
    if (preFsRects_.count(layer.Id()) != 0) {
        return;  // already fullscreen — idempotent, no duplicate event
    }
    if (!compositor_ || !window_) {
        return;
    }
    // 최대화 중이면 먼저 정상 크기로 복원한 뒤 출발(Windows 관례 — 최대화
    // →전체화면 토글은 복원 rect에서 시작). window.restored 이벤트는
    // RestoreFromMaximize가 정직 발행한다.
    if (preMaxRects_.count(layer.Id()) != 0) {
        RestoreFromMaximize(client, layer);
    }
    MaxState saved;
    saved.x = layer.X();
    saved.y = layer.Y();
    saved.surfW = layer.Width();
    saved.surfH = layer.Height();
    saved.dispW = static_cast<int>(std::llround(saved.surfW * layer.ScaleX()));
    saved.dispH = static_cast<int>(std::llround(saved.surfH * layer.ScaleY()));
    int ww = 0, wh = 0;
    SDL_GetWindowSize(window_, &ww, &wh);
    CommitChromeResize(client, layer.Id(), ww, wh, ww, wh);
    preFsRects_[layer.Id()] = saved;
    compositor_->SetLayerPosition(layer.Id(), 0, 0);
    client.SetPosition(0, 0);
    layer.SetFullscreen(true);
    PushMaximizeEvent("window.fullscreen", client);
}

// 전체화면 복원: RestoreFromMaximize의 대응물 — 저장 rect 복원 + 이벤트 1회.
// 데스크탑 리사이즈 재발행은 이 함수를 경유하지 않는다(재발행은 이벤트
// 없음, 저장 rect는 무효화하지 않음 — maximize와 동일 규약).
bool JKWindowServer::RestoreFullscreen(JKClientConnection& client,
                                       JKCompositorLayer& layer) {
    auto it = preFsRects_.find(layer.Id());
    if (it == preFsRects_.end()) {
        return false;  // not fullscreen
    }
    const MaxState saved = it->second;
    preFsRects_.erase(it);
    CommitChromeResize(client, layer.Id(), saved.surfW, saved.surfH,
                       saved.dispW, saved.dispH);
    compositor_->SetLayerPosition(layer.Id(), saved.x, saved.y);
    client.SetPosition(saved.x, saved.y);
    layer.SetFullscreen(false);
    PushMaximizeEvent("window.fullscreen_exit", client);
    return true;
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
        // 전체화면 레이어는 리사이즈 핫스팟이 없다(스펙 §2.1) — 화살표 고정.
        if (layer && layer->IsFullscreen()) {
            SetChromeCursor(CursorShape::Arrow);
            return;
        }
        // The shell has no chrome — its own UI keeps the arrow (docs/28).
        if (layer && client && !client->IsShell()) {
            const int lx = static_cast<int>(std::llround(
                (mx / scale - layer->X()) / layer->ScaleX()));
            const int ly = static_cast<int>(std::llround(
                (my / scale - layer->Y()) / layer->ScaleY()));
            const int w = layer->Width();
            const int h = layer->Height();
            // The close overlay and the maximize/restore button (docs/39)
            // stay a plain arrow even though their corner overlaps the top
            // resize strip.
            const bool inCloseX = (lx >= w - kChromeCloseSize - kChromeCloseMargin) &&
                                  (lx < w - kChromeCloseMargin);
            const bool inCloseY = (ly >= kChromeCloseMargin) &&
                                  (ly < kChromeCloseMargin + kChromeCloseSize);
            const int maxBtnX0 = w - kChromeCloseMargin - kChromeCloseSize -
                                 kChromeMaximizeGap - kChromeMaximizeSize;
            const bool inMaxX = (lx >= maxBtnX0) &&
                                (lx < maxBtnX0 + kChromeMaximizeSize);
            if (!(inCloseX && inCloseY) && !(inMaxX && inCloseY)) {
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
        if (ev.type == SDL_MOUSEBUTTONDOWN &&
            TryChromeGrab(mx, my, outputScale, ev.button.clicks)) {
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
            // Launcher icons live in the desktop shell (P1 ③): LaunchAt
            // hit-tests in physical pixels and dispatches the spawn through
            // the ShellHost launch callback (this server's SpawnClient, so
            // the 500 ms throttle below stays server-side).
            if (shell_ && shell_->LaunchAt(mx, my)) {
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
            // Live modifier state for mouse reports (docs/26 단계 3) — the
            // mouse structs carry no mods, the server owns the real keyboard
            // state; JKClientSurface copies payload.option through verbatim.
            payload.option = SDL_GetModState();
        } else if (ev.type == SDL_MOUSEBUTTONDOWN) {
            payload.type = ipc::InputEventType::MouseDown;
            payload.keyCode = ev.button.button;
            payload.detail = ev.button.clicks;
            payload.option = SDL_GetModState();
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
            payload.option = SDL_GetModState();
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
        payload.option = SDL_GetModState();   // mouse-report mods (단계 3)
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

    // docs/39 §8: the server window is now RESIZABLE (and maximizable via the
    // OS title button). SIZE_CHANGED funnels here too, but MOVED /
    // DISPLAY_CHANGED must NOT touch the maximized layers — only an actual
    // logical SIZE change re-issues their maximize against the new work area.
    // The first call (Init, before any client exists) only seeds the trackers.
    if (lastDesktopW_ != logW || lastDesktopH_ != logH) {
        const bool firstCall = (lastDesktopW_ < 0);
        lastDesktopW_ = logW;
        lastDesktopH_ = logH;
        if (!firstCall) {
            size_t nMax = 0;
            if (!preMaxRects_.empty()) {
                const int reserve = compositor_->ShellReserveHeight();
                const int workH = logH - reserve;
                for (const auto& kv : preMaxRects_) {
                    // FindClientById locks clientsMutex_; this path never runs
                    // with that lock held (Init / HandleSDLEvent), matching
                    // FocusClient's PushAgentEvent locking regime. Entries
                    // whose client is gone (died or disconnected while the
                    // desktop was resized) are skipped —
                    // CleanupDisconnectedClients erases their map entries
                    // later anyway.
                    JKClientConnection* client = FindClientById(kv.first);
                    if (!client || client->IsDisconnected()) {
                        continue;
                    }
                    ++nMax;
                    CommitChromeResize(*client, kv.first, logW, workH,
                                       logW, workH);
                    compositor_->SetLayerPosition(kv.first, 0, 0);
                    // Connection-side position in sync (DockShellClient
                    // comment: the input mapping reads client->X()/Y(),
                    // drawing reads the compositor layer).
                    client->SetPosition(0, 0);
                }
            }
            // vplayer 전체화면(스펙 §2.1) 재발행: 새 출력 전체(예약 없음).
            // 저장 rect는 무효화하지 않는다 — 복원 대상은 여전히 진짜 원 rect.
            size_t nFs = 0;
            for (const auto& kv : preFsRects_) {
                JKClientConnection* client = FindClientById(kv.first);
                if (!client || client->IsDisconnected()) {
                    continue;
                }
                ++nFs;
                CommitChromeResize(*client, kv.first, logW, logH, logW, logH);
                compositor_->SetLayerPosition(kv.first, 0, 0);
                client->SetPosition(0, 0);
            }
            // Desktop-size adjustment, NOT a toggle: no window.maximized /
            // window.restored event, and the saved pre-max rects stay valid.
            // One log line — stdout is the server log (run_test.sh redirects
            // it, read_log tails it).
            std::printf("[server] desktop size changed to %dx%d "
                        "(re-maximized %zu, re-fullscreened %zu layer(s))\n",
                        logW, logH, nMax, nFs);
            std::fflush(stdout);
        }
    }
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
        // file_open (filedlg 설계)은 승인이 아니라 대화상자가 해소자 — 같은
        // 만료 기계로 회수하되 오류 문자열만 대화상자에 맞춘다.
        const char* timeoutErr =
            (it->kind == "file_open") ? "dialog_timeout" : "approval_timeout";
        // 다이얼로그가 result 없이 죽으면(크래시/kill/요청자 먼저 종료) 슬롯도
        // 같이 비워야 한다 — 안 그러면 이후 file_open이 서버 재시작까지
        // dialog_busy로 막힌다. 새 무효화 기계 없이 이 스캔 안에서 회수.
        if (it->kind == "file_open" &&
            pendingFileDialog_.requestId == it->requestId) {
            pendingFileDialog_ = PendingFileDialog{};
        }
        for (auto& c : clients_) {
            if (c && c->Id() == it->requesterId && !c->IsDisconnected()) {
                ipc::WriteAgentJson(c->Transport(), ipc::MsgType::AgentReply,
                                    it->queryId, 0,
                                    (std::string("{\"ok\":false,\"error\":\"") +
                                     timeoutErr + "\"}").c_str());
                break;
            }
        }
        // file_open 만료는 승인 결정이 아니라 대화상자 수명 만료다
        // (final-review NOTE-4) — agent.approval_resolved 승인 이벤트를
        // 브로드캐스트하면 구독자에게 존재하지 않는 승인의 timeout 결정을
        // 날조해 전달하게 된다. 슬롯 회수(위)는 그대로 유지.
        if (it->kind != "file_open") {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "{\"topic\":\"agent.approval_resolved\","
                          "\"request\":%u,\"decision\":\"timeout\"}",
                          it->requestId);
            PushAgentEventJson(buf);
        }
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

// 권한 매트릭스의 행 — 게이트 소비처별 정직 표기 (스펙 §2.1). 서버는
// AgentToolAllowed를 3종(+신규 2종)에서만 검사하고 브로커 bool 맵이 MCP
// 경로만 걸러낸다. "none" 행의 파일값은 서버 경로에서 무력.
struct AgentPermRow { const char* tool; const char* gate; const char* deflt; };
static const AgentPermRow kPermMatrix[] = {
    {"close_window", "server", "deny"},
    {"trust_request", "server", "ask"},
    {"run_console_app", "server", "ask"},
    {"trust_revoke", "server", "ask"},
    {"permission_set", "server(fixed)", "ask"},
    {"read_log", "broker", "allow"},
    {"read_events", "broker", "allow"},
    {"terminal_exec", "broker", "allow"},
    {"list_windows", "none", "allow"},
    {"focus_window", "none", "allow"},
    {"window_fullscreen", "none", "allow"},
    {"launch_app", "none", "allow"},
    {"save_layout", "none", "allow"},
    {"restore_layout", "none", "allow"},
    {"publish_event", "none", "allow"},
    {"capture_window", "none", "allow"},
    {"capture_region", "none", "allow"},
    {"trigger_toggle", "none", "allow"},
    {"theme_set", "none", "allow"},
    {"open_notify", "none", "allow"},
    {"launch_chat", "none", "allow"},
    {"approve", "none", "allow"},
    {"file_open", "none", "allow"},
    {"file_dialog_params", "none", "allow"},
    {"agent_permissions", "none", "allow"},
    {"installed_list", "none", "allow"},
    {"read_receipts", "none", "allow"},
};

// permissions.json RMW (스펙 §2.2): 알려진 도구 키 전부 명시 기록 — 없던 키는
// 현재 기본값으로 채워 다음 편집자가 기본값을 온전히 본다. permission_set 행은
// 기록하지 않는다(파일값 무시 게이트). 반환: 빈 문자열 = 성공, 아니면
// write_failed. kPermMatrix는 gate "server" 행의 기본값에도 쓰인다.
static std::string WritePermissionsEntry(const std::string& permTool,
                                         const std::string& decision) {
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    const std::string path = dir + "\\permissions.json";

    std::map<std::string, std::string> values;
    if (std::FILE* f = std::fopen(path.c_str(), "rb")) {
        // 전체 읽기 (docs/53 §9 잔여): 4KB 스택 버프는 파일 뒤쪽의 알려진
        // 도구 행을 잘라내 RMW가 기본값으로 되돌렸다. 256KiB 상한 = 이상
        // 파일 메모리 가드 — 초과 시 잘린 JSON이 파싱 실패하면 전재기록이
        // 기본값으로 복원한다(자기 치유, 알려진 키만 기록되는 RMW 원래 의미).
        std::fseek(f, 0, SEEK_END);
        const long sz = std::ftell(f);
        if (sz < 0) {
            // ftell 실패에도 RMW를 진행하면 기존 파일 전체가 기본값으로
            // 되돌려진다(opus 최종리뷰 n3) — 지문 없는 not-found RMW보다
            // 나은 정직 오류로 파킹 전 즉답 경로에 표면화한다.
            std::fclose(f);
            return "permissions_unreadable";
        }
        std::fseek(f, 0, SEEK_SET);
        if (sz > 0) {
            const size_t cap =
                std::min<size_t>(static_cast<size_t>(sz), 256 * 1024);
            std::vector<char> buf(cap + 1, '\0');
            const size_t n = std::fread(buf.data(), 1, cap, f);
            buf[n] = '\0';
            jk::agent::AgentJson json(buf.data());
            std::string v;
            if (json.ok()) {
                for (const AgentPermRow& r : kPermMatrix) {
                    if (json.GetStr(r.tool, v) &&
                        (v == "allow" || v == "ask" || v == "deny")) {
                        values[r.tool] = v;
                    }
                }
            }
        }
        std::fclose(f);
    }
    values[permTool] = decision;
    std::string out = "{";
    bool first = true;
    for (const AgentPermRow& r : kPermMatrix) {
        if (std::string(r.gate) == "server(fixed)") continue;
        if (!first) out += ",";
        first = false;
        const auto it = values.find(r.tool);
        out += "\"" + std::string(r.tool) + "\":\"" +
               (it != values.end() ? it->second : std::string(r.deflt)) + "\"";
    }
    out += "}";
    std::FILE* w = std::fopen(path.c_str(), "wb");
    if (!w) return "write_failed";
    const size_t wrote = std::fwrite(out.data(), 1, out.size(), w);
    std::fclose(w);
    return wrote == out.size() ? std::string() : std::string("write_failed");
}

// 지문 형식: 정확히 "sha256:" + 64 소문자 hex (로더 형식 — docs/37).
static bool ValidFingerprint(const std::string& fp) {
    if (fp.size() != 7 + 64 || fp.compare(0, 7, "sha256:") != 0) return false;
    for (size_t i = 7; i < fp.size(); ++i) {
        const char c = fp[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

// trust.json 원본 텍스트에서 지문 레코드의 { ... } 경계를 찾는다. 레코드는
// 로더 쓰기 형식(name/source/fingerprint/ts — 중첩 객체 없음)이므로 중괄호
// 스캔이 안전하다. AgentJson 재직렬화는 ts(int64)를 잃는다(AgentJson에 int64
// 접근자 없음 — docs/38) — 그래서 원문 수술.
static bool TrustRecordText(const std::string& text,
                            const std::string& fingerprint,
                            std::string& recOut) {
    const std::string needle = "\"fingerprint\":\"" + fingerprint + "\"";
    const size_t hit = text.find(needle);
    if (hit == std::string::npos) return false;
    const size_t begin = text.rfind('{', hit);
    const size_t end = text.find('}', hit);
    if (begin == std::string::npos || end == std::string::npos) return false;
    recOut = text.substr(begin, end - begin + 1);
    return true;
}

// trust.json에서 해당 지문 레코드 제거 + .bak 1회 보존(북마크 선례 — 최초
// 덮어쓰기 시점 원본만). 반환: 빈 문자열 = 성공(제거 1건), 아니면 오류 문자열.
static std::string RevokeTrustRecord(const std::string& fingerprint) {
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    CreateDirectoryA((dir + "\\state").c_str(), nullptr);
    const std::string path = dir + "\\state\\trust.json";

    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return "trust_store_unreadable";
    // 전체 읽기 (docs/53 §9 잔여): 64KB 캡은 장기 설치의 스토어에서 뒤쪽
    // 레코드를 not_found로 미끄러뜨린다. 8MiB 상한 = 이상 파일 가드.
    // 상한 초과/읽기 미달(stale write 중 등)에는 스플라이스를 하지 않는다 —
    // 잘린 접두어를 되돌려 쓰면 상한 너머의 레코드가 파괴되고 .bak도 잘린
    // 원본이라 복구 불가(opus 최종리뷰 m3). 정직한 not_found 반환.
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    if (sz < 0) { std::fclose(f); return "trust_store_unreadable"; }
    std::fseek(f, 0, SEEK_SET);
    const size_t tsz = static_cast<size_t>(sz);
    const bool overCap = tsz > 8u * 1024 * 1024;
    const size_t cap = overCap ? 8u * 1024 * 1024 : tsz;
    std::vector<char> buf(cap + 1, '\0');
    const size_t n = std::fread(buf.data(), 1, cap, f);
    std::fclose(f);
    if (overCap || n < cap) return "not_found";
    buf[n] = '\0';
    const std::string text(buf.data());

    std::string rec;
    if (!TrustRecordText(text, fingerprint, rec)) return "not_found";
    const size_t hit = text.find(rec);
    size_t begin = hit;
    size_t end = hit + rec.size() - 1;
    // 선행 쉼표 흡수 — "},{" 형태에서 앞 레코드의 쉼표를 남기지 않는다.
    size_t cutBegin = begin;
    if (cutBegin > 0 && text[cutBegin - 1] == ',') --cutBegin;
    else if (end + 1 < text.size() && text[end + 1] == ',') ++end;
    const std::string out = text.substr(0, cutBegin) +
                            text.substr(end + 1);

    const std::string bak = path + ".bak";
    if (std::FILE* b = std::fopen(bak.c_str(), "rb")) {
        std::fclose(b);   // .bak 이미 있음 — 1회 보존 규약
    } else if (std::FILE* b = std::fopen(bak.c_str(), "wb")) {
        std::fwrite(text.data(), 1, text.size(), b);
        std::fclose(b);
    }
    std::FILE* w = std::fopen(path.c_str(), "wb");
    if (!w) return "write_failed";
    const size_t wrote = std::fwrite(out.data(), 1, out.size(), w);
    std::fclose(w);
    return wrote == out.size() ? std::string() : std::string("write_failed");
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
    } else if (tool == "window_fullscreen") {
        // vplayer 전체화면(스펙 2026-09-17 vplayer-fullscreen-osd §2.2): id
        // 생략 = 호출자 자기 창(창 클라이언트 — vplayer 경로), 명시 id =
        // 임의 창(스크립트/프로브). control-only 호출자의 생략형은 no_window.
        // on 생략 = 현재 상태 반전(AgentJson에 bool 접근자 없음 — GetObjInt
        // 0/1로 읽는다). 화면 상태 변경일 뿐 승인 행위가 아니어서 none gate
        // (focus_window 분류). reply의 fullscreen은 토글 후 실제 플래그 —
        // 클라 플래그의 유일 신뢰원.
        int id = 0;
        JKClientConnection* target = nullptr;
        if (req.GetObjInt("args", "id", id)) {
            for (auto& c : clients_) {
                if (c && c->Id() == static_cast<uint32_t>(id)) { target = c.get(); break; }
            }
        } else if (!client.IsControlOnly()) {
            target = &client;
        }
        JKCompositorLayer* fsLayer = nullptr;
        if (target && !target->IsControlOnly() && !target->IsShell() && compositor_) {
            // opus 리뷰 MINOR-2: 명시 id로 shell(태스크바) 레이어를 노리면
            // 전체화면 토글이 데스크탑 셸을 덮는다 — shell은 '창'이 아니므로
            // window_not_found로 거절한다(생략형 경로는 애초 shell 불가).
            fsLayer = compositor_->FindLayerById(target->Id());
        }
        if (!fsLayer) {
            reply = (target ? "{\"ok\":false,\"error\":\"window_not_found\"}"
                            : "{\"ok\":false,\"error\":\"no_window\"}");
        } else {
            int onArg = -1;
            bool on = fsLayer->IsFullscreen() ? false : true;
            if (req.GetObjInt("args", "on", onArg) &&
                (onArg == 0 || onArg == 1)) {
                on = (onArg == 1);
            }
            ToggleFullscreen(*target, *fsLayer, on);
            reply = std::string("{\"ok\":true,\"fullscreen\":") +
                    (fsLayer->IsFullscreen() ? "true" : "false") + "}";
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
    } else if (tool == "trust_request") {
        // Script trust gate (docs/37 spec): jktriggers asks before the first
        // eval of an unknown fingerprint. Default permission is "ask" — the
        // same inline-approval pipeline as close_window. The server only
        // relays the decision; the loader owns trust.json.
        std::string name, origin, fingerprint;
        req.GetObjStr("args", "name", name);
        req.GetObjStr("args", "origin", origin);
        req.GetObjStr("args", "fingerprint", fingerprint);
        // Trim the name — whitespace-only is still missing (final-review fix:
        // validate before parking an approval, so no face can feed a
        // malformed fingerprint/origin into the broadcast payload).
        {
            const size_t b = name.find_first_not_of(" \t\r\n");
            if (b == std::string::npos) {
                name.clear();
            } else {
                name = name.substr(b, name.find_last_not_of(" \t\r\n") - b + 1);
            }
        }
        if (name.empty()) {
            reply = "{\"ok\":false,\"error\":\"missing_name\"}";
        } else if (name.size() > 96) {
            // docs/38: bound the display name before it reaches the approval
            // broadcast payload and the parked PendingApproval (same shape as
            // bad_fingerprint — validated before parking).
            reply = "{\"ok\":false,\"error\":\"bad_name\"}";
        } else if (!ValidFingerprint(fingerprint)) {
            reply = "{\"ok\":false,\"error\":\"bad_fingerprint\"}";
        } else if (origin != "dev" && origin != "package") {
            reply = "{\"ok\":false,\"error\":\"bad_origin\"}";
        } else {
            switch (AgentToolAllowed("trust_request")) {
                case AgentDecision::Allow:
                    // permissions.json says allow — every script is trusted.
                    reply = "{\"ok\":true}";
                    break;
                case AgentDecision::Ask: {
                    bool subscriber = false;
                    for (auto& c : clients_) {
                        if (c && c->AgentEventSubscriber() &&
                            !c->IsDisconnected()) {
                            subscriber = true;
                            break;
                        }
                    }
                    if (!subscriber) {
                        reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
                        break;
                    }
                    PendingApproval p;
                    p.kind = "trust_request";
                    p.name = name;
                    p.origin = origin;
                    p.fingerprint = fingerprint;
                    p.requestId = nextApprovalId_++;
                    p.queryId = queryId;
                    p.requesterId = client.Id();
                    p.targetId = 0;
                    p.expiresAt = std::time(nullptr) + 60;
                    char buf[1024];
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"trust_request\","
                                  "\"kind\":\"trust_request\",\"name\":\"%s\","
                                  "\"origin\":\"%s\",\"fingerprint\":\"%s\","
                                  "\"ts\":%lld}",
                                  p.requestId, JsonEsc(name).c_str(),
                                  JsonEsc(origin).c_str(),
                                  JsonEsc(fingerprint).c_str(),
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;  // answered when the approval resolves
                    break;
                }
                case AgentDecision::Deny:
                default:
                    reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                    break;
            }
        }
    } else if (tool == "permission_set") {
        // 스펙 §2.2: 매트릭스 쓰기 — AgentToolAllowed("permission_set")은
        // 파일값 무시하고 항상 Ask(하드코딩, §2.2 2단 우회 봉쇄). 검증은
        // 파킹 전(trust_request의 bad_* 선례).
        std::string permTool, decision;
        req.GetObjStr("args", "tool", permTool);
        req.GetObjStr("args", "decision", decision);
        bool known = false;
        for (const AgentPermRow& r : kPermMatrix) {
            if (permTool == r.tool) { known = true; break; }
        }
        if (permTool.empty()) {
            reply = "{\"ok\":false,\"error\":\"missing_tool\"}";
        } else if (!known) {
            reply = "{\"ok\":false,\"error\":\"unknown_tool\"}";
        } else if (decision != "allow" && decision != "ask" &&
                   decision != "deny") {
            reply = "{\"ok\":false,\"error\":\"bad_decision\"}";
        } else if (permTool == "permission_set") {
            // fixed-ask 행은 WritePermissionsEntry가 스킵하므로 승인해도
            // 파일이 불변 — written:true 거짓 보고 대신 즉답 거절(리뷰 MINOR).
            reply = "{\"ok\":false,\"error\":\"bad_target\"}";
        } else {
            switch (AgentToolAllowed("permission_set")) {
                case AgentDecision::Ask: {
                    bool subscriber = false;
                    for (auto& c : clients_) {
                        if (c && c->AgentEventSubscriber() &&
                            !c->IsDisconnected()) {
                            subscriber = true;
                            break;
                        }
                    }
                    if (!subscriber) {
                        reply = "{\"ok\":false,"
                                "\"error\":\"approval_unavailable\"}";
                        break;
                    }
                    PendingApproval p;
                    p.kind = "permission_set";
                    p.permTool = permTool;
                    p.permDecision = decision;
                    p.requestId = nextApprovalId_++;
                    p.queryId = queryId;
                    p.requesterId = client.Id();
                    p.targetId = 0;
                    p.expiresAt = std::time(nullptr) + 60;
                    char buf[640];
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"permission_set\","
                                  "\"kind\":\"permission_set\","
                                  "\"target_tool\":\"%s\","
                                  "\"decision\":\"%s\",\"ts\":%lld}",
                                  p.requestId, JsonEsc(permTool).c_str(),
                                  decision.c_str(),
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;   // 해소 시 답신
                    break;
                }
                case AgentDecision::Allow:
                case AgentDecision::Deny:
                default:
                    // 도달하지 않는다(고정 Ask) — 방어선으로 deny 유지.
                    reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                    break;
            }
        }
    } else if (tool == "trust_revoke") {
        // 스펙 §2.3: 신뢰 해지 — 안전 방향이지만 무게이트는 신뢰 저장소
        // 전면 소각 DoS 통로. 기본 Ask, 파일로 allow/deny 변경 가능(해지는
        // 권한 부여가 아니라 2단 우회 위험이 없다). 파킹 전 검증+실측.
        std::string fingerprint;
        req.GetObjStr("args", "fingerprint", fingerprint);
        std::string rec, name;
        if (fingerprint.empty()) {
            reply = "{\"ok\":false,\"error\":\"missing_fingerprint\"}";
        } else if (!ValidFingerprint(fingerprint)) {
            reply = "{\"ok\":false,\"error\":\"bad_fingerprint\"}";
        } else {
            std::string trustText;
            char exePath[1024] = {};
            GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
            std::string dir = exePath;
            const size_t dslash = dir.find_last_of("\\/");
            if (dslash != std::string::npos) dir = dir.substr(0, dslash);
            if (std::FILE* f = std::fopen(
                    (dir + "\\state\\trust.json").c_str(), "rb")) {
                // 전체 읽기 (docs/53 §9 잔여 — RevokeTrustRecord와 동일 근거):
                // 64KB 캡이면 뒤쪽 레코드가 not_found로 미끄러져 해지가
                // RMW까지 못 간다. 8MiB 상한 = 이상 파일 가드.
                std::fseek(f, 0, SEEK_END);
                const long tsz = std::ftell(f);
                std::fseek(f, 0, SEEK_SET);
                if (tsz > 0) {
                    const size_t tcap = std::min<size_t>(
                        static_cast<size_t>(tsz), 8u * 1024 * 1024);
                    std::vector<char> tbuf(tcap + 1, '\0');
                    const size_t tn = std::fread(tbuf.data(), 1, tcap, f);
                    tbuf[tn] = '\0';
                    trustText = tbuf.data();
                }
                std::fclose(f);
            }
            if (!TrustRecordText(trustText, fingerprint, rec)) {
                reply = "{\"ok\":false,\"error\":\"not_found\"}";
            } else {
                jk::agent::AgentJson(rec.c_str()).GetStr("name", name);
                switch (AgentToolAllowed("trust_revoke")) {
                    case AgentDecision::Ask: {
                        bool subscriber = false;
                        for (auto& c : clients_) {
                            if (c && c->AgentEventSubscriber() &&
                                !c->IsDisconnected()) {
                                subscriber = true;
                                break;
                            }
                        }
                        if (!subscriber) {
                            reply = "{\"ok\":false,"
                                    "\"error\":\"approval_unavailable\"}";
                            break;
                        }
                        PendingApproval p;
                        p.kind = "trust_revoke";
                        p.name = name.empty() ? "script" : name;
                        p.fingerprint = fingerprint;
                        p.requestId = nextApprovalId_++;
                        p.queryId = queryId;
                        p.requesterId = client.Id();
                        p.targetId = 0;
                        p.expiresAt = std::time(nullptr) + 60;
                        char buf[1024];
                        std::snprintf(buf, sizeof(buf),
                                      "{\"topic\":\"agent.approval_request\","
                                      "\"request\":%u,\"tool\":\"trust_revoke\","
                                      "\"kind\":\"trust_revoke\",\"name\":\"%s\","
                                      "\"fingerprint\":\"%s\",\"ts\":%lld}",
                                      p.requestId, JsonEsc(p.name).c_str(),
                                      fingerprint.c_str(),
                                      static_cast<long long>(
                                          std::time(nullptr)) * 1000);
                        pendingApprovals_.push_back(p);
                        PushAgentEventJson(buf);
                        replied = false;
                        break;
                    }
                    case AgentDecision::Allow:
                        // 파일이 allow로 명시한 설치 — 즉시 해지(사용자가
                        // 매트릭스에서 그렇게 정한 것).
                        {
                            const std::string err =
                                RevokeTrustRecord(fingerprint);
                            reply = err.empty()
                                ? "{\"ok\":true,\"written\":true,"
                                  "\"restart_needed\":true}"
                                : "{\"ok\":false,\"error\":\"" + err + "\"}";
                        }
                        break;
                    case AgentDecision::Deny:
                    default:
                        reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                        break;
                }
            }
        }
    } else if (tool == "launch_app") {
        std::string app, jkx;
        req.GetObjStr("args", "app", app);
        req.GetObjStr("args", "jkx", jkx);
        if (!app.empty()) {
            // docs/35: pair the capture overlay with the client that asked
            // for it (see overlaySpawner_ member comment).
            pendingSnapSpawnerConnId_ = (app == "snap") ? client.Id() : 0;
            SpawnClient(app.c_str(), false);
            reply = "{\"ok\":true}";
        } else if (!jkx.empty()) {
            SpawnClient(jkx.c_str(), true);
            reply = "{\"ok\":true}";
        } else {
            reply = "{\"ok\":false,\"error\":\"missing_app\"}";
        }
    } else if (tool == "run_console_app") {
        // P4 SDK §5: 에이전트가 콘솔 앱(apps/<name>/manifest.json)을 스폰.
        // ask 기본 게이트 — close_window/trust_request와 같은 inline-approval
        // 파이프라인. target 창이 없으므로 targetId=0, 식별자는 앱 name.
        std::string name;
        req.GetObjStr("args", "name", name);
        std::string cmd, dir, fp;
        if (name.empty() || name.size() > 64) {
            reply = "{\"ok\":false,\"error\":\"bad_name\"}";
        } else if (!shell_ ||
                   !shell_->ConsoleAppInfo(name, cmd, dir, fp)) {
            reply = "{\"ok\":false,\"error\":\"unknown_app\"}";
        } else {
            switch (AgentToolAllowed("run_console_app")) {
                case AgentDecision::Allow:
                    SpawnConsoleApp(cmd, dir, name);
                    reply = "{\"ok\":true}";
                    break;
                case AgentDecision::Ask: {
                    bool subscriber = false;
                    for (auto& c : clients_) {
                        if (c && c->AgentEventSubscriber() &&
                            !c->IsDisconnected()) {
                            subscriber = true;
                            break;
                        }
                    }
                    if (!subscriber) {
                        reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
                        break;
                    }
                    PendingApproval p;
                    p.kind = "run_console_app";
                    p.name = name;
                    p.requestId = nextApprovalId_++;
                    p.queryId = queryId;
                    p.requesterId = client.Id();
                    p.targetId = 0;
                    p.expiresAt = std::time(nullptr) + 60;
                    char buf[512];
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"run_console_app\","
                                  "\"name\":\"%s\",\"ts\":%lld}",
                                  p.requestId, JsonEsc(name).c_str(),
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;  // answered when the approval resolves
                    break;
                }
                case AgentDecision::Deny:
                default:
                    reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                    break;
            }
        }
    } else if (tool == "theme_set") {
        // P3 hot-swap (docs/52): write theme.json (the same truth the boot
        // loader and every client's 500ms mtime poll read) then swap
        // in-process. Paint-time consumers (shell, JKDC default args) follow
        // instantly; ctor-captured widget tokens re-capture via the client
        // poll's ApplyTheme walk. Allow by default — appearance only.
        std::string preset;
        req.GetObjStr("args", "preset", preset);
        const jk::theme::JKTheme* t = nullptr;
        if (preset == "light") t = &jk::theme::kLight;
        else if (preset == "classic") t = &jk::theme::kClassic;
        else if (preset == "dark") t = &jk::theme::kDefault;
        if (!t) {
            reply = "{\"ok\":false,\"error\":\"bad_preset\"}";
        } else {
            jk::theme::setTheme(t);
            if (!jk::theme::WriteThemePresetFile(preset)) {
                // Swap happened in-process but the truth file didn't land —
                // clients' mtime poll would never follow. Fail loudly
                // instead of replying ok (docs/52 review MINOR).
                reply = "{\"ok\":false,\"error\":\"write_failed\"}";
            } else {
                char buf[96];
                std::snprintf(buf, sizeof(buf),
                              "{\"ok\":true,\"preset\":\"%s\"}",
                              preset.c_str());
                reply = buf;
            }
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
            // docs/38: connection-level cap — 60 events / 10s (same figure as
            // the local handler cap so the local cap binds first for
            // self-loops — deterministic notify/stop point; spec §2 interaction
            // note). Over-cap events are dropped but answered ok so senders
            // don't turn into retry bombs. The budget counts allowed events
            // only (a drop consumes nothing — same semantics as the jktriggers
            // source cap) and must stay lock-free — this branch runs with
            // clientsMutex_ already held.
            constexpr int kPublishCap = 60;
            constexpr uint64_t kPublishWindowMs = 10000;
            const uint64_t nowMs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
            PublishBudget& b = publishBudgets_[client.Id()];
            if (nowMs - b.windowStartMs >= kPublishWindowMs) {
                b.windowStartMs = nowMs;
                b.count = 0;
                b.logged = false;
            }
            if (b.count >= kPublishCap) {
                // Over cap: no budget consumption on the drop path.
                if (!b.logged) {
                    b.logged = true;
                    // One log line per connection per window — stdout is the
                    // server log (run_test.sh redirects it, read_log tails it).
                    std::printf("[server] publish_event rate-capped (conn %u)\n",
                                client.Id());
                    std::fflush(stdout);
                }
                reply = "{\"ok\":true,\"dropped\":1}";
            } else {
                ++b.count;
                PushAgentEventJson(ev);
                reply = "{\"ok\":true}";
            }
        }
    } else if (tool == "capture_window") {
        // docs/35: read the client's shm surface (RGBA32) directly — the
        // app framebuffer, no screen DPI involvement. Safe tier (same as
        // launch_app). Note: permissions.json does NOT gate this tool
        // server-side (only close_window/trust_request/run_console_app are
        // checked via AgentToolAllowed). The broker's LoadPermissions bool
        // map filters MCP-agent calls; agent_permissions reports this row
        // as gate "none". Kept ungated — see
        // specs/2026-09-16-agent-manager §2.1.
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
    } else if (tool == "capture_region") {
        // docs/35: composited-frame readback (SDL_RenderReadPixels), then
        // crop. The requester's own layer is hidden for the readback so a
        // rubber-band overlay does not appear in its own screenshot. Args
        // are logical desktop points — the framebuffer is physical pixels.
        int x = 0, y = 0, w = 0, h = 0;
        req.GetObjInt("args", "x", x);
        req.GetObjInt("args", "y", y);
        req.GetObjInt("args", "w", w);
        req.GetObjInt("args", "h", h);
        const int outW = compositor_ ? compositor_->OutputWidth() : 0;
        const int outH = compositor_ ? compositor_->OutputHeight() : 0;
        if (outW <= 0 || outH <= 0 || w <= 0 || h <= 0) {
            reply = "{\"ok\":false,\"error\":\"bad_request\"}";
        } else {
            // Intersect with the output bounds (logical points).
            int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
            int x1 = x + w > outW ? outW : x + w;
            int y1 = y + h > outH ? outH : y + h;
            if (x0 >= x1 || y0 >= y1) {
                reply = "{\"ok\":false,\"error\":\"bad_request\"}";
            } else {
                const float scale = compositor_->OutputScale();
                const int fx = static_cast<int>(x0 * scale);
                const int fy = static_cast<int>(y0 * scale);
                const int fw = static_cast<int>((x1 - x0) * scale);
                const int fh = static_cast<int>((y1 - y0) * scale);
                const int fullW =
                    static_cast<int>(outW * scale);
                const int fullH = static_cast<int>(outH * scale);

                JKCompositorLayer* self =
                    compositor_->FindLayerById(client.Id());
                const bool hideSelf = self && self->IsVisible();
                if (hideSelf) self->SetVisible(false);
                // Hide the viewer that launched this overlay too (docs/35):
                // it is the capture UI and must not appear in its own shot.
                JKCompositorLayer* spawner = nullptr;
                auto sp = overlaySpawner_.find(client.Id());
                if (sp != overlaySpawner_.end() && sp->second != client.Id()) {
                    spawner = compositor_->FindLayerById(sp->second);
                }
                const bool hideSpawner = spawner && spawner->IsVisible();
                if (hideSpawner) spawner->SetVisible(false);
                Composite(false);   // clear + draw, no present
                std::vector<uint8_t> full(
                    static_cast<size_t>(fullW) * fullH * 4);
                SDL_Rect fullRect{0, 0, fullW, fullH};
                const int got = SDL_RenderReadPixels(
                    renderer_, &fullRect, SDL_PIXELFORMAT_RGBA32,
                    full.data(), fullW * 4);
                if (hideSpawner) spawner->SetVisible(true);
                if (hideSelf) self->SetVisible(true);
                Composite(false);
                if (got != 0) {
                    reply = "{\"ok\":false,\"error\":\"read_failed\"}";
                } else {
                    // Row-wise crop into the capture buffer.
                    std::vector<uint8_t> crop(
                        static_cast<size_t>(fw) * fh * 4);
                    for (int row = 0; row < fh; ++row) {
                        std::memcpy(crop.data() +
                                        static_cast<size_t>(row) * fw * 4,
                                    full.data() +
                                        (static_cast<size_t>(fy + row) *
                                             fullW +
                                         fx) * 4,
                                    static_cast<size_t>(fw) * 4);
                    }
                    const std::string dir = StateDir() + "\\screenshots";
                    CreateDirectoryA(dir.c_str(), nullptr);
                    const long long ts =
                        std::chrono::duration_cast<
                            std::chrono::milliseconds>(
                            std::chrono::system_clock::now()
                                .time_since_epoch())
                            .count();
                    const std::string path =
                        dir + "\\shot_" + std::to_string(ts) + "_region.png";
                    if (WritePng(path, fw, fh, crop.data())) {
                        reply = "{\"ok\":true,\"path\":\"" + JsonEsc(path) +
                                "\"}";
                    } else {
                        reply = "{\"ok\":false,\"error\":\"write_failed\"}";
                    }
                }
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
    } else if (tool == "trust_list") {
        // Script trust store (docs/37 spec): the loader's trust.json records,
        // fingerprints truncated to 15 chars ("sha256:"+8hex) for display.
        // docs/38: a missing/corrupt store is distinguished from an empty one
        // — trust_store_unreadable on fopen or parse failure; a valid file
        // with zero records still answers ok with an empty array.
        std::FILE* f =
            std::fopen((StateDir() + "\\trust.json").c_str(), "rb");
        if (!f) {
            reply = "{\"ok\":false,\"error\":\"trust_store_unreadable\"}";
        } else {
            char buf[65536] = {};
            const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
            std::fclose(f);
            buf[n] = '\0';
            jk::agent::AgentJson json(buf);
            int cnt = 0;
            if (!json.ok() || !json.GetArraySize("records", cnt)) {
                reply = "{\"ok\":false,\"error\":\"trust_store_unreadable\"}";
            } else {
                std::string out = "{\"ok\":true,\"records\":[";
                bool first = true;
                for (int i = 0; i < cnt && i < 512; ++i) {
                    std::string fp, name, source;
                    // ts is epoch ms (int64 in the loader's writer) but the
                    // AgentJson accessor has no int64 getter — display-only
                    // int truncation is accepted (jktriggers reads ts via a
                    // direct QuickJS reader instead).
                    int ts = 0;
                    if (!json.GetArrStr("records", i, "fingerprint", fp))
                        continue;
                    json.GetArrStr("records", i, "name", name);
                    json.GetArrStr("records", i, "source", source);
                    json.GetArrInt("records", i, "ts", ts);
                    if (!first) out += ",";
                    first = false;
                    // 에이전트 관리자 해지용 전체 지문 (스펙 §2.3 전제) — 15자
                    // 절단 fingerprint는 표시용으로 유지.
                    out += "{\"fingerprint\":\"" +
                           (fp.size() > 15 ? fp.substr(0, 15) : fp) +
                           "\",\"name\":\"" + JsonEsc(name) +
                           "\",\"source\":\"" + JsonEsc(source) +
                           "\",\"fp\":\"" + JsonEsc(fp) +
                           "\",\"ts\":" + std::to_string(ts) + "}";
                }
                out += "]}";
                reply = out;
            }
        }
    } else if (tool == "events_list") {
        // docs/32: the structured event catalog. Static rows describe the
        // system topics (source + payload shape); fired/last_ts come from
        // the runtime stats and subscribers from the live connections.
        // Topics published via publish_event that are not cataloged are
        // appended as dynamic "app" rows.
        size_t subscribers = 0;
        for (auto& c : clients_) {
            if (c && c->AgentEventSubscriber() && !c->IsDisconnected()) {
                ++subscribers;
            }
        }
        static const struct {
            const char* topic;
            const char* source;  // server | app
            const char* desc;
            const char* fields;  // JSON array literal of payload paths
        } kCatalog[] = {
            {"window.created", "server", "클라 창(레이어) 생성",
             "[\"id\",\"title\",\"pid\"]"},
            {"window.focused", "server", "창 포커스 이동",
             "[\"id\",\"title\",\"pid\"]"},
            {"window.destroyed", "server", "창 소멸(연결 종료, 정상 종료 포함)",
             "[\"id\",\"title\",\"pid\"]"},
            {"window.maximized", "server",
             "창 최대화 (크롬 최대화 버튼 / 제목 더블클릭, docs/39)",
             "[\"id\",\"title\"]"},
            {"window.restored", "server",
             "창 복원 (최대화 해제 — 버튼/더블클릭/제목·가장자리 드래그, docs/39)",
             "[\"id\",\"title\"]"},
            {"window.fullscreen", "server",
             "창 전체화면 진입 (window_fullscreen 도구/vplayer F11·더블클릭, "
             "docs/50 §11)",
             "[\"id\",\"title\"]"},
            {"window.fullscreen_exit", "server",
             "창 전체화면 해제 (window_fullscreen 도구/vplayer F11·더블클릭·OSD "
             "버튼, docs/50 §11)",
             "[\"id\",\"title\"]"},
            {"app.crashed", "server", "앱 비정상 종료(exit code 0/259 외)",
             "[\"id\",\"title\",\"pid\"]"},
            {"agent.approval_request", "server",
             "ask 권한 승인 요청 (docs/31) + trust_request (docs/37)",
             "\"request,tool,kind,title/target_id/name,origin,fingerprint\""},
            {"agent.approval_resolved", "server",
             "승인 결정: allow/deny/timeout", "[\"request\",\"decision\"]"},
            {"terminal.output", "app", "터미널 출력(250ms 코얼레싱, VT 제거)",
             "[\"data.text\"]"},
            {"agent.notify", "app",
             "알림 방송 — desktop.notify()가 발행, 채팅 [알림] 줄 + 알림 센터가 소비",
             "[\"data.title\",\"data.body\"]"},
            {"triggers.reload", "server", "트리거 플래그 변경 재적재 신호", "[]"},
        };
        std::string out = "{\"ok\":true,\"subscribers\":" +
                          std::to_string(subscribers) + ",\"events\":[";
        bool first = true;
        auto appendEntry = [&](const std::string& topic, const char* source,
                               const char* desc, const char* fields) {
            const auto it = topicStats_.find(topic);
            const uint64_t fired =
                it == topicStats_.end() ? 0 : it->second.fired;
            const long long lastTs =
                it == topicStats_.end() ? 0 : it->second.lastTs;
            if (!first) out += ",";
            first = false;
            out += "{\"topic\":\"" + JsonEsc(topic) + "\",\"source\":\"" +
                   source + "\",\"desc\":\"" + JsonEsc(desc) +
                   "\",\"fields\":" + fields + ",\"fired\":" +
                   std::to_string(fired) + ",\"last_ts\":" +
                   std::to_string(lastTs) + "}";
        };
        for (const auto& e : kCatalog) {
            appendEntry(e.topic, e.source, e.desc, e.fields);
        }
        for (const auto& kv : topicStats_) {
            bool inCatalog = false;
            for (const auto& e : kCatalog) inCatalog |= (e.topic == kv.first);
            if (!inCatalog) {
                appendEntry(kv.first, "app",
                            "publish_event로 발행된 동적 토픽", "[]");
            }
        }
        reply = out + "]}";
    } else if (tool == "agent_permissions") {
        // 스펙 §2.1: permissions.json + 기본값 병합. gate 뱃지로 게이트
        // 소비처를 정직 표기 — "none" 행의 파일값은 서버 무력(브로커만).
        // file은 "" = 오버라이드 없음 (AgentJson이 null을 못 읽는다).
        char exePath[1024] = {};
        GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
        std::string dir = exePath;
        const size_t slash = dir.find_last_of("\\/");
        if (slash != std::string::npos) dir = dir.substr(0, slash);
        const std::string permPath = dir + "\\permissions.json";
        char pbuf[4096] = {};
        bool fileExists = false;
        if (std::FILE* f = std::fopen(permPath.c_str(), "rb")) {
            fileExists = true;
            std::fread(pbuf, 1, sizeof(pbuf) - 1, f);
            std::fclose(f);
        }
        // AgentJson은 복사/대입 불가(docs/38) — 파일이 없으면 "{}"로 생성.
        jk::agent::AgentJson perm(fileExists ? pbuf : "{}");
        if (fileExists && !perm.ok()) {
            // 파일 없음(기본값)과 파싱 실패(수동 편집 실수)를 구분 —
            // trust_list의 docs/38 선례.
            reply = "{\"ok\":false,\"error\":\"permissions_unreadable\"}";
        } else {
            std::string out = "{\"ok\":true,\"perms\":[";
            bool first = true;
            for (const AgentPermRow& row : kPermMatrix) {
                std::string fileVal;
                const bool hasFile = perm.ok() &&
                    perm.GetStr(row.tool, fileVal) &&
                    (fileVal == "allow" || fileVal == "ask" ||
                     fileVal == "deny");
                std::string effective = row.deflt;
                if (std::string(row.gate) == "server(fixed)") {
                    effective = "ask";
                } else if (std::string(row.gate) == "server") {
                    if (hasFile) effective = fileVal;
                } else {
                    effective = "allow";   // broker/none — 서버 미게이트
                }
                if (!first) out += ",";
                first = false;
                out += "{\"tool\":\"" + std::string(row.tool) +
                       "\",\"gate\":\"" + row.gate + "\",\"file\":\"" +
                       (hasFile ? fileVal : std::string()) +
                       "\",\"effective\":\"" + effective +
                       "\",\"default\":\"" + row.deflt + "\"}";
            }
            reply = out + "]}";
        }
    } else if (tool == "installed_list") {
        // 스펙 §2.4: 셸 런처 스캔의 이름/kind. shell_ 미기동 = 빈 배열(정상).
        std::vector<std::pair<std::string, const char*>> rows;
        if (shell_) shell_->ListInstalled(rows);
        std::string out = "{\"ok\":true,\"installed\":[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ",";
            out += "{\"name\":\"" + JsonEsc(rows[i].first) +
                   "\",\"kind\":\"" + rows[i].second + "\"}";
        }
        reply = out + "]}";
    } else if (tool == "read_receipts") {
        // 스펙 §2.5: 브로커 receipts.jsonl 꼬리 — ts/tool/ok만 반환
        // (result 전문은 args에 경로/명령어가 실릴 수 있다).
        int limit = 50;
        req.GetObjInt("args", "limit", limit);
        if (limit <= 0) limit = 50;
        if (limit > 200) limit = 200;
        const std::string path = StateDir() + "\\receipts.jsonl";
        std::FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) {
            reply = "{\"ok\":true,\"rows\":[]}";   // 브로커 미사용 = 정상
        } else {
            std::fseek(f, 0, SEEK_END);
            const long size = std::ftell(f);
            const long start = size > 262144 ? size - 262144 : 0;
            std::fseek(f, start, SEEK_SET);
            std::vector<char> buf(static_cast<size_t>(size - start) + 1);
            const size_t n = std::fread(buf.data(), 1, buf.size() - 1, f);
            std::fclose(f);
            buf[n] = '\0';
            std::vector<std::string> lines;
            size_t pos = 0;
            while (pos < n) {
                const char* begin = buf.data() + pos;
                const char* nl = static_cast<const char*>(
                    std::memchr(begin, '\n', n - pos));
                const size_t len = nl ? (size_t)(nl - begin) : (n - pos);
                if (len > 0) lines.push_back(std::string(begin, len));
                pos += len + (nl ? 1 : 0);
            }
            std::string out = "{\"ok\":true,\"rows\":[";
            int used = 0;
            for (size_t i = lines.size(); i-- > 0 && used < limit;) {
                const std::string& line = lines[i];
                // JSONL 행의 result 중첩은 2레벨 리더의 관심사가 아니다 —
                // ts/tool은 행 파서(AgentJson) + ts는 raw 스캔(숫자 필드),
                // ok는 result 내 raw 스캔.
                jk::agent::AgentJson row(line.c_str());
                std::string toolName;
                if (!row.ok() || !row.GetStr("tool", toolName)) continue;
                long long ts = 0;
                {
                    const size_t tp = line.find("\"ts\":");
                    if (tp != std::string::npos) {
                        ts = std::atoll(line.c_str() + tp + 5);
                    }
                }
                const bool okFlag =
                    line.find("\"result\":") != std::string::npos &&
                    line.find("\"ok\":true") != std::string::npos;
                if (used) out += ",";
                // 직렬화 규약(파서 계약): ts = epoch 초(2레벨 GetArrInt 경유,
                // ms → 초 절단), ok = "0"/"1" 문자열(리더가 bool을 못 읽는다).
                out += "{\"ts\":" + std::to_string(ts / 1000) +
                       ",\"tool\":\"" + JsonEsc(toolName) +
                       "\",\"ok\":\"" + (okFlag ? "1" : "0") + "\"}";
                ++used;
            }
            reply = out + "]}";
        }
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
        bool selfApprove = false;
        for (auto it = pendingApprovals_.begin();
             it != pendingApprovals_.end(); ++it) {
            if (it->requestId != static_cast<uint32_t>(request)) continue;
            // 승인 파이프라인의 2단 우회 봉쇄(스펙 §7 리뷰 후속): 파킹을
            // 자기 연결에서 approve하면 승인 없는 허가가 된다. kind를
            // 2종(permission_set/trust_revoke)으로 한정하면 trust_request/
            // run_console_app 파킹이 우회를 남긴다(opus 최종리뷰 M1 — 자기
            // 구독으로 승인 불가 상태를 스스로 해소해 지문 선기록/스폰이
            // 그대로 관통). close_window만 예외 — ask 모드에서 채팅 자신의
            // /close를 자기 승인 스트립으로 해소하는 것은 docs/31 §3의 설계된
            // UX다(non-blocking 채팅의 존재 이유). 나머지 전종에는 legit
            // 요청자-자체승인 경로가 없다. 파킹은 건드리지 않는다 — 다른
            // 표면(채팅)의 승인은 여전히 가능하다.
            if (it->requesterId == client.Id() && it->kind != "close_window") {
                selfApprove = true;
                break;
            }
            resolved = true;
            if (allow && it->kind == "close_window") {
                for (auto& c : clients_) {
                    if (c && c->Id() == it->targetId && !c->IsDisconnected()) {
                        ipc::WriteMessage(c->Transport(), ipc::MsgType::Close,
                                          std::vector<uint8_t>{});
                        break;
                    }
                }
            }
            if (allow && it->kind == "run_console_app" && shell_) {
                // P4 SDK §5: 승인 시점에 스캔에서 cmd를 다시 읽는다 — 승인
                // 대기 중 매니페스트가 바뀌었으면 최신 cmd가 스폰된다(파킹된
                // cmd를 신뢰하지 않음 — 지문 재계산이 아니라 재조회로 방어).
                std::string cmd, dir, fp;
                if (shell_->ConsoleAppInfo(it->name, cmd, dir, fp)) {
                    SpawnConsoleApp(cmd, dir, it->name);
                }
            }
            for (auto& c : clients_) {
                if (c && c->Id() == it->requesterId && !c->IsDisconnected()) {
                    std::string result;
                    if (!allow) {
                        result = "{\"ok\":false,\"error\":\"denied_by_user\"}";
                    } else if (it->kind == "permission_set") {
                        // 쓰기 실패는 요청자 reply에 표면화 (docs/52 선례).
                        const std::string err = WritePermissionsEntry(
                            it->permTool, it->permDecision);
                        result = err.empty()
                            ? "{\"ok\":true,\"written\":true}"
                            : "{\"ok\":false,\"error\":\"" + err + "\"}";
                    } else if (it->kind == "trust_revoke") {
                        const std::string err =
                            RevokeTrustRecord(it->fingerprint);
                        result = err.empty()
                            ? "{\"ok\":true,\"written\":true,"
                              "\"restart_needed\":true}"
                            : "{\"ok\":false,\"error\":\"" + err + "\"}";
                    } else {
                        result = "{\"ok\":true}";
                    }
                    const int flag = (result.find("\"ok\":true") !=
                                      std::string::npos)
                                         ? 1 : 0;
                    ipc::WriteAgentJson(c->Transport(), ipc::MsgType::AgentReply,
                                        it->queryId, flag, result);
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
        if (selfApprove) {
            reply = "{\"ok\":false,\"error\":\"self_approve\"}";
        } else if (!resolved) {
            reply = "{\"ok\":false,\"error\":\"unknown_request\"}";
        }
    } else if (tool == "file_open") {
        // 파일 열기 대화상자 (설계 specs/2026-09-13-file-dialog §1b): 쿼리를
        // 파킹한 뒤 filedlg:<json args> appName 접두로 다이얼로그를 띄운다.
        // 파킹은 jkchat close_window/trust_request와 같은 pendingApprovals_
        // 기계를 재사용 — 해소는 다이얼로그의 file_open_result가 담당하고,
        // 요청자가 먼저 닫히면 만료 스캔이 회수한다. 권한 게이트 없음 —
        // launch_app 같은 안전 계층 (AgentToolAllowed 기본 allow).
        std::string filter, start, title;
        req.GetObjStr("args", "filter", filter);
        req.GetObjStr("args", "start", start);
        req.GetObjStr("args", "title", title);
        // 256자 상한 (trust_request의 bad_name 선례) — 1차 가지치기. 실제
        // cmdLine 경계는 아래의 이스케이프 후 크기 검사다 (원시 길이만으로는
        // 인용 확장을 못 잡는다).
        if (filter.size() > 256 || start.size() > 256 || title.size() > 256) {
            reply = "{\"ok\":false,\"error\":\"bad_request\"}";
        } else if (pendingFileDialog_.requesterConnId != 0) {
            // 설계 리스크 2: 1슬롯 선착순 — 대화상자가 열려 있으면 후발은
            // 파킹해도 해소자가 없으므로 즉시 오류 (만료 대기보다 정직).
            reply = "{\"ok\":false,\"error\":\"dialog_busy\"}";
        } else {
            // 스폰 인자: args 그대로의 json (선택 필드만 — 없으면 키 생략).
            std::string jsonArgs = "{";
            bool first = true;
            auto appendField = [&](const char* key, const std::string& v) {
                if (v.empty()) return;
                if (!first) jsonArgs += ",";
                first = false;
                jsonArgs += std::string("\"") + key + "\":\"" + JsonEsc(v) +
                            "\"";
            };
            appendField("filter", filter);
            appendField("start", start);
            appendField("title", title);
            jsonArgs += "}";
            // publish_event의 이스케이프 후 크기 검사 선례: cmdLine 잘림은
            // 조용히 일어나므로 스폰 전에 이스케이프된 크기를 검문한다. 1000
            // = SpawnProcess cmdLine 2048 − 최악 exe 경로(~1026, 인용 포함)
            // − "--filedlg \"\"" 골격 — 원시 json에서 인용 확장분까지 포함한
            // 상계(JsonEsc는 인용+백슬래시를 모두 늘리므로 보수적).
            if (JsonEsc(jsonArgs).size() > 1000) {
                reply = "{\"ok\":false,\"error\":\"bad_request\"}";
            } else {
                // 만료는 승인 파이프라인의 60s가 아니라 600s — 사용자가
                // 다이얼로그에서 고민하는 시간을 감안한다. 요청자 연결이 먼저
                // 닫혀도 이 만료 스캔이 회수한다 (신규 무효화 코드 없음).
                PendingApproval p;
                p.kind = "file_open";
                p.requestId = nextApprovalId_++;
                p.queryId = queryId;
                p.requesterId = client.Id();
                p.targetId = 0;  // 대상 창 없음 — 다이얼로그가 해소자
                p.expiresAt = std::time(nullptr) + 600;
                pendingApprovals_.push_back(p);
                auto parkedIt = std::prev(pendingApprovals_.end());
                if (!SpawnClient(("filedlg:" + jsonArgs).c_str(), false)) {
                    // 스폰 실패 — 파킹 즉시 해소 (오류 응답). 저장해둔
                    // 반복자로 지운다 (pop_back의 순서 가정 제거).
                    pendingApprovals_.erase(parkedIt);
                    reply = "{\"ok\":false,\"error\":\"spawn_failed\"}";
                } else {
                    pendingFileDialog_.requesterConnId = client.Id();
                    pendingFileDialog_.requestId = p.requestId;
                    pendingFileDialog_.filter = filter;
                    pendingFileDialog_.start = start;
                    pendingFileDialog_.title = title;
                    replied = false;  // file_open_result(또는 만료)가 응답한다
                }
            }
        }
    } else if (tool == "file_dialog_params") {
        // filedlg 앱 기동 직후 1회 — file_open이 채운 파라미터를 꺼내 간다
        // (설계 D3: 모듈 ABI 무변경, 스폰 인자 회수는 쿼리로). 선착순 1회 —
        // 재요청은 오류. requesterConnId로 요청자-다이얼로그 상관관계를
        // 전달하고, 상관관계 자체는 결과 회수까지 슬롯에 남는다.
        if (pendingFileDialog_.requesterConnId == 0 ||
            pendingFileDialog_.paramsTaken) {
            reply = "{\"ok\":false,\"error\":\"no_pending_dialog\"}";
        } else {
            std::string out = "{\"ok\":true,\"requesterConnId\":" +
                              std::to_string(pendingFileDialog_.requesterConnId);
            auto appendParam = [&](const char* key, const std::string& v) {
                out += std::string(",\"") + key + "\":\"" + JsonEsc(v) + "\"";
            };
            appendParam("filter", pendingFileDialog_.filter);
            appendParam("start", pendingFileDialog_.start);
            appendParam("title", pendingFileDialog_.title);
            reply = out + "}";
            pendingFileDialog_.paramsTaken = true;  // 선착순 소진
        }
    } else if (tool == "file_open_result") {
        // filedlg 앱 종료 결과 — 요청자의 파킹 쿼리를 완료한다 (approve 도구의
        // 파킹 해소 선례). 파킹이 이미 만료/부재면 no-op + parked:false.
        int ok = 0;
        std::string path;
        req.GetObjInt("args", "ok", ok);
        req.GetObjStr("args", "path", path);
        // 발신자 상관 검증 (final-review MAJOR-1): filedlg가
        // file_dialog_params로 받은 requesterConnId를 결과에 되울린다.
        // 필드 부재(구버전 다이얼로그/수조작) 또는 슬롯의 요청자와 불일치면
        // 아무것도 해소하지 않는 parked:false no-op — 슬롯도 지우지 않는다.
        // 검증이 없으면 만료 회수 후에도 살아 있던 고아 다이얼로그의 결과가
        // 새 요청자에게 잘못 전달된다(회귀 시나리오). 수동 `--client
        // filedlg` 실행(파킹 슬롯 없음)은 requesterConnId 0으로 발신하므로
        // 어느 경로든 무해한 no-op으로 수렴한다.
        int senderConnId = -1;
        req.GetObjInt("args", "requesterConnId", senderConnId);
        const bool senderMatched =
            senderConnId >= 0 &&
            static_cast<uint32_t>(senderConnId) ==
                pendingFileDialog_.requesterConnId;
        bool resolved = false;
        if (senderMatched && pendingFileDialog_.requesterConnId != 0) {
            for (auto it = pendingApprovals_.begin();
                 it != pendingApprovals_.end(); ++it) {
                if (it->kind != "file_open" ||
                    it->requestId != pendingFileDialog_.requestId) {
                    continue;
                }
                resolved = true;
                for (auto& c : clients_) {
                    if (c && c->Id() == it->requesterId &&
                        !c->IsDisconnected()) {
                        // ok=false(취소)는 ok 플래그 0 + {"ok":false}.
                        const std::string result = ok
                            ? (path.empty()
                                   ? "{\"ok\":true}"
                                   : "{\"ok\":true,\"path\":\"" +
                                         JsonEsc(path) + "\"}")
                            : "{\"ok\":false}";
                        ipc::WriteAgentJson(c->Transport(),
                                            ipc::MsgType::AgentReply,
                                            it->queryId, ok ? 1 : 0, result);
                        break;
                    }
                }
                pendingApprovals_.erase(it);
                break;
            }
            pendingFileDialog_ = PendingFileDialog{};  // 슬롯 소진 (결과와 무관)
        }
        reply = resolved ? "{\"ok\":true,\"parked\":true}"
                         : "{\"tool\":\"file_open_result\","
                           "\"ok\":true,\"parked\":false}";
    } else {
        reply = "{\"ok\":false,\"error\":\"not_implemented\"}";
    }
    // The ask path parks the query — its reply is sent when the approval
    // resolves (approve tool), when the filedlg app answers (file_open_result)
    // or when it times out (expiry scan below).
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
    // events_list stats (docs/32): every emit site starts the envelope with
    // {"topic":"..." — a raw scan avoids a JSON parse on the hot path.
    static constexpr char kKey[] = "\"topic\":\"";
    const size_t keyPos = json.find(kKey);
    if (keyPos != std::string::npos) {
        const size_t start = keyPos + sizeof(kKey) - 1;
        const size_t end = json.find('"', start);
        if (end != std::string::npos) {
            TopicStat& st = topicStats_[json.substr(start, end - start)];
            ++st.fired;
            st.lastTs = static_cast<long long>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count());
        }
    }
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
// M2 chat / docs/37: "ask" means the inline-approval pipeline (chat window) —
// wired for close_window + trust_request; other tools degrade to allow since
// nothing parks them.
AgentDecision JKWindowServer::AgentToolAllowed(const std::string& tool) const {
    // permission_set은 파일 값을 무시하고 항상 Ask — 파일로 이 도구를
    // allow로 바꿔두면 이후 모든 권한 변경이 무승인이 되는 2단 우회 봉쇄
    // (스펙 §2.2 핵심 안전 결정).
    if (tool == "permission_set") return AgentDecision::Ask;
    char exePath[1024] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir = exePath;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    const std::string path = dir + "\\permissions.json";
    // Missing entry defaults: close_window denies (M1 rule), trust_request
    // ASKS (the gate would be pointless if unknown scripts loaded silently),
    // run_console_app ASKS (P4 SDK §5 — the agent launching local apps is an
    // explicit-approval act), trust_revoke ASKS (agent-manager — revoking is
    // safe-direction but ungated would let an agent burn the whole trust
    // store), everything else allows. "ask" pipelines: close_window +
    // trust_request + run_console_app + trust_revoke; permission_set is
    // hardwired Ask regardless of the file; other tools degrade to allow
    // since nothing parks them.
    const bool askCapable = (tool == "close_window" || tool == "trust_request" ||
                             tool == "run_console_app" || tool == "trust_revoke");
    auto defaultDecision = [&]() -> AgentDecision {
        if (tool == "close_window") return AgentDecision::Deny;
        if (tool == "trust_request") return AgentDecision::Ask;
        if (tool == "run_console_app") return AgentDecision::Ask;
        if (tool == "trust_revoke") return AgentDecision::Ask;
        return AgentDecision::Allow;
    };
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return defaultDecision();
    char buf[4096] = {};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    jk::agent::AgentJson perm(buf);
    std::string value;
    if (!perm.ok() || !perm.GetStr(tool.c_str(), value)) {
        return defaultDecision();
    }
    if (value == "allow") return AgentDecision::Allow;
    if (value == "ask") {
        return askCapable ? AgentDecision::Ask : AgentDecision::Allow;
    }
    if (value == "deny") return AgentDecision::Deny;
    return defaultDecision();
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

// 콘솔 앱 스폰 (P4 SDK §3/§5): 터미널 위에 cmd — cwd는 앱 폴더(상대경로).
// SpawnProcess가 인용을 만들므로 cmd/cwd 끝에 백슬래시가 없어야 한다
// (453a327 레슨) — 매니페스트의 상대경로 규칙이 이를 보장한다.
void JKWindowServer::SpawnConsoleApp(const std::string& cmd, const std::string& cwd,
                                     const std::string& name) {
    SpawnProcess(clientHostExe_.c_str(),
                 std::string("terminal --cwd \"") + cwd + "\" --shell \"" + cmd + "\"",
                 name.c_str());
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
    Composite(true);
}

void JKWindowServer::Composite(bool present) {
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
    // layer client surfaces on top and then present once. The desktop shell
    // (P1 ③) owns that background; Draw no-ops on an empty desktop.
    if (shell_) {
        shell_->Draw(renderer_);
    }
    compositor_->Composite(present);
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
                // docs/39: the layer is going away — drop its maximize state
                // so a recycled surface id cannot inherit a stale pre-max rect.
                preMaxRects_.erase(client->Id());
                // vplayer 전체화면(스펙 §2.1): maximize 동일 — 죽은 레이어의
                // 저장 rect는 무의미.
                preFsRects_.erase(client->Id());
                if (compositor_) {
                    compositor_->RemoveLayer(client->Id());
                }
                // filedlg 슬롯 회수 (final-review MINOR-2): 요청자가 다이얼로그
                // 도중 죽으면 슬롯을 즉시 비운다 — 픽스 전엔 만료 스캔(600s)까지
                // dialog_busy로 모든 file_open을 막았다. 이 연결의 파킹 쿼리는
                // 만료 스캔이 회수한다(요청자가 죽었으면 응답 대상이 없어
                // no-op) — pendingApprovals_ 기계는 건드리지 않는다.
                if (pendingFileDialog_.requesterConnId == client->Id()) {
                    pendingFileDialog_ = PendingFileDialog{};
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

// Launch an arbitrary exe from the server's directory (SpawnClient core).
// throttleKey defaults to exeName; SpawnClient keeps the per-app key so two
// DIFFERENT apps can still launch back-to-back.
bool JKWindowServer::SpawnProcess(const char* exeName, const std::string& args,
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
                return false;
            }
        }
        lastSpawnTimes_[key] = now;
    }

    // Assume the server executable is in the same directory as the target.
    // Wide path: GetModuleFileNameA would break on non-ANSI install dirs.
    wchar_t modulePathW[1024] = {};
    const unsigned long len =
        GetModuleFileNameW(nullptr, modulePathW, 1024);
    if (len == 0 || len >= 1024) {
        std::fprintf(stderr, "JKWindowServer: GetModuleFileNameW failed\n");
        return false;
    }

    // Find the directory component.
    wchar_t* lastSlash = modulePathW;
    for (wchar_t* p = modulePathW; *p; ++p) {
        if (*p == L'\\' || *p == L'/') lastSlash = p;
    }
    // Leave a NUL after the directory; exe name is appended below.
    const bool haveDir = (lastSlash != modulePathW);
    std::wstring dirW(modulePathW, haveDir ? (lastSlash - modulePathW) : 0);
    const wchar_t* workDir = haveDir ? dirW.c_str() : nullptr;

    // Wide command line, UTF-8 args converted with CP_UTF8 (see the W-variant
    // note above). 2048 chars upper-bounds the ANSI version's byte budget.
    std::wstring cmdLine;
    cmdLine.reserve(2048);
    cmdLine += L"\"";
    cmdLine += (haveDir ? dirW : L".");
    cmdLine += L"\\";
    {
        // exeName is an ASCII literal from the spawn table; convert anyway so
        // the whole line is one encoding. The path quote is already open —
        // just append the exe and close it (a quote here would escape on the
        // backslash: "dir\"exe" is one broken token).
        int n = MultiByteToWideChar(65001, 0, exeName, -1, nullptr, 0);
        std::wstring exeW(static_cast<size_t>(n > 0 ? n : 1), L'\0');
        if (n > 0) MultiByteToWideChar(65001, 0, exeName, -1, exeW.data(), n);
        cmdLine += exeW.c_str();
        cmdLine += L"\"";
    }
    if (!args.empty()) {
        int n = MultiByteToWideChar(65001, 0, args.c_str(),
                                    static_cast<int>(args.size()),
                                    nullptr, 0);
        if (n <= 0) {
            std::fprintf(stderr, "JKWindowServer: arg UTF-8 conversion failed for %s\n",
                         exeName);
            return false;
        }
        std::wstring argsW(static_cast<size_t>(n), L'\0');
        MultiByteToWideChar(65001, 0, args.c_str(),
                            static_cast<int>(args.size()), argsW.data(), n);
        cmdLine += L" ";
        cmdLine += argsW;
    }

    LauncherStartupInfoW si{};
    si.cb = sizeof(si);
    LauncherProcessInformation pi{};

    // Set the child's working directory to the executable directory so it can
    // locate the assets/ folder regardless of where the server was launched from.
    if (!CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr, 0, 0,
                        nullptr, workDir, &si, &pi)) {
        std::fprintf(stderr, "JKWindowServer: CreateProcessW failed for %s\n", exeName);
        return false;
    }

    // Keep the child's process handle for crash classification (M2b): when
    // the spawned client disconnects, CleanupDisconnectedClients checks the
    // exit code and emits app.crashed for non-zero exits. The thread handle
    // is never needed again.
    if (pi.hProcess) spawnedClients_[pi.dwProcessId] = pi.hProcess;
    if (pi.hThread) CloseHandle(pi.hThread);

    std::fprintf(stderr, "JKWindowServer: spawned %s %s\n", exeName, args.c_str());
    return true;
#else
    (void)exeName;
    (void)args;
    std::fprintf(stderr, "JKWindowServer: SpawnProcess is Windows-only in this prototype\n");
    return false;
#endif // _WIN32
}

bool JKWindowServer::SpawnClient(const char* appName, bool fromJkx) {
#ifdef _WIN32
    if (fromJkx) {
        // A .jkx container path — may contain spaces, so quote it.
        std::string arg = std::string("--jkx \"") + appName + "\"";
        return SpawnProcess(clientHostExe_.c_str(), arg, appName);
    }
    // Phase A 흡수 (docs/44): appName "terminal:<cmdline>" — 콘솔 TUI 앱을
    // 터미널 위에 띄운다. 런처 fallback 셀이 이 관례를 쓴다.
    std::string name(appName);
    constexpr const char* kTermPrefix = "terminal:";
    if (name.rfind(kTermPrefix, 0) == 0) {
        return SpawnProcess(clientHostExe_.c_str(),
                            std::string("terminal --shell ") +
                                name.substr(strlen(kTermPrefix)),
                            appName);
    }
    // 파일 열기 대화상자 (설계 specs/2026-09-13-file-dialog §1b): appName
    // "filedlg:<json args>" — terminal:과 같은 계열의 두 번째 접두 관례.
    // json을 따옴표로 감싸고 내부 "만 \"로 이스케이프(--jkx 인용 선례 + CRT
    // argv 규칙) — 자식의 argv[2]가 json 그대로 온다 (--jkx argv 계약과
    // 동일). terminal: 쪽은 건드리지 않는다.
    constexpr const char* kFileDlgPrefix = "filedlg:";
    if (name.rfind(kFileDlgPrefix, 0) == 0) {
        const std::string json = name.substr(strlen(kFileDlgPrefix));
        std::string quoted = "--filedlg \"";
        for (char ch : json) {
            if (ch == '"') quoted += "\\\"";
            else           quoted += ch;
        }
        quoted += "\"";
        return SpawnProcess(clientHostExe_.c_str(), quoted, appName);
    }
    return SpawnProcess(clientHostExe_.c_str(),
                        std::string("--client ") + appName, appName);
#else
    (void)appName;
    (void)fromJkx;
    std::fprintf(stderr, "JKWindowServer: SpawnClient is Windows-only in this prototype\n");
    return false;
#endif // _WIN32
}

} // namespace server
} // namespace jk
