#include <server/JKWindowServer.h>

#include <apps/AppLauncherItem.h>
#include <JKAudioCommand.h>
#include <JKAudioThread.h>
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
        int x = std::max(0, (ww - client->Width()) / 2) + existingCount * 20;
        int y = std::max(0, (wh - client->Height()) / 2) + existingCount * 20;
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
        if (client->Id() == focusedClientId_ || focusedClientId_ == 0) {
            compositor_->FocusLayer(client->Id());
        }

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

        // Client surfaces are rendered on top of the launcher, so they should
        // receive input first. Only treat a click as a launcher icon click if
        // it did not hit any client surface.
        JKClientConnection* client = HitTestClient(mx, my);
        if (!client && ev.type == SDL_MOUSEBUTTONDOWN) {
            int icon = HitTestLauncherIcon(mx, my);
            if (icon >= 0) {
                SpawnClient(launcherIcons_[icon].appName);
                return;
            }
        }
        if (!client) return;

        // mx/my are physical client px. The client surface is client->Width() x
        // client->Height() surface pixels, stretched by outputScale when drawn.
        // Convert the physical mouse position back into the client's surface
        // pixel space: (mx - client->X()*outputScale) / outputScale.
        ipc::InputEventPayload payload{};
        payload.surfaceId = client->Id();
        payload.x = static_cast<int>(std::llround(mx / outputScale)) - client->X();
        payload.y = static_cast<int>(std::llround(my / outputScale)) - client->Y();

        if (ev.type == SDL_MOUSEMOTION) {
            payload.type = ipc::InputEventType::MouseMove;
            payload.dx = ev.motion.xrel;
            payload.dy = ev.motion.yrel;
        } else if (ev.type == SDL_MOUSEBUTTONDOWN) {
            payload.type = ipc::InputEventType::MouseDown;
            payload.keyCode = ev.button.button;
            payload.detail = ev.button.clicks;
            FocusClient(client->Id());
        } else if (ev.type == SDL_MOUSEBUTTONUP) {
            payload.type = ipc::InputEventType::MouseUp;
            payload.keyCode = ev.button.button;
            payload.detail = ev.button.clicks;
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
                if (compositor_) {
                    compositor_->RemoveLayer(client->Id());
                }
                disconnected.push_back(std::move(client));
                it = clients_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Join read threads outside the lock to avoid blocking the server main loop
    // and to prevent shutdown deadlocks.
    for (auto& client : disconnected) {
        if (client) client->StopReadThread();
    }
}

void JKWindowServer::InitLauncher() {
    if (!renderer_) return;

    // Server-side launcher: two desktop icons for Minesweeper and Tetris.
    // These live as simple SDL textures drawn behind the composited surfaces.
    launcherIcons_.clear();
    {
        LauncherIcon icon;
        icon.rect = JKRect{ 50, 50, 64, 80 };
        icon.appName = "minesweeper";
        launcherIcons_.push_back(icon);
    }
    {
        LauncherIcon icon;
        icon.rect = JKRect{ 150, 50, 64, 80 };
        icon.appName = "tetris";
        launcherIcons_.push_back(icon);
    }

    DrawLauncher();
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

    // For Phase 2 the launcher is a minimal placeholder: a grey desktop and
    // two colored rectangles representing app icons. Full icons would need a
    // server-side text renderer; for now labels are rendered by drawing colored
    // squares and (on Windows) relying on the user knowing which is which.
    // This only draws; the compositor calls SDL_RenderPresent once per frame.
    SDL_SetRenderDrawColor(renderer_, 96, 96, 96, 255);
    SDL_RenderClear(renderer_);

    for (const auto& icon : launcherIcons_) {
        SDL_Rect rc{
            static_cast<int>(icon.rect.x * s),
            static_cast<int>(icon.rect.y * s),
            static_cast<int>(icon.rect.w * s),
            static_cast<int>(icon.rect.h * s),
        };
        if (std::strcmp(icon.appName, "minesweeper") == 0) {
            SDL_SetRenderDrawColor(renderer_, 128, 128, 128, 255);
        } else {
            SDL_SetRenderDrawColor(renderer_, 128, 0, 128, 255);
        }
        SDL_RenderFillRect(renderer_, &rc);
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer_, &rc);
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

void JKWindowServer::SpawnClient(const char* appName) {
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
    // Build a command line of the form: jkproto_sdl2_jkwindow.exe --client minesweeper
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
    std::snprintf(cmdLine, sizeof(cmdLine),
                  "\"%s\\jkproto_sdl2_jkwindow.exe\" --client %s",
                  modulePath[0] ? modulePath : ".",
                  appName);

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

    std::fprintf(stderr, "JKWindowServer: spawned client --client %s\n", appName);
#else
    (void)appName;
    std::fprintf(stderr, "JKWindowServer: SpawnClient is Windows-only in this prototype\n");
#endif // _WIN32
}

} // namespace server
} // namespace jk
