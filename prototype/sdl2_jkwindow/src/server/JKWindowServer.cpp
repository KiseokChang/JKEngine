#include <server/JKWindowServer.h>

#include <cstdio>
#include <cstring>
#include <algorithm>

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

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
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

    return true;
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
        }
        if (!running_) break;

        ProcessPendingMessages();
        Composite();
        CleanupDisconnectedClients();

        SDL_Delay(1);
    }
}

void JKWindowServer::Stop() {
    running_ = false;

    if (acceptorThread_.joinable()) {
        acceptorThread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (auto& client : clients_) {
            client->StopReadThread();
            if (client->GetTexture()) {
                SDL_DestroyTexture(client->GetTexture());
                client->SetTexture(nullptr);
            }
        }
        clients_.clear();
    }

    for (auto& client : pendingCleanup_) {
        client->StopReadThread();
        if (client->GetTexture()) {
            SDL_DestroyTexture(client->GetTexture());
            client->SetTexture(nullptr);
        }
    }
    pendingCleanup_.clear();

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

void JKWindowServer::RunSingleAccept(const std::string& pipeName) {
    pipeName_ = pipeName;

    std::fprintf(stderr, "JKWindowServer: waiting for client on '%s'\n", pipeName_.c_str());
    auto transport = ipc::JKPipeTransport::CreateServer(pipeName_);
    if (!transport) {
        std::fprintf(stderr, "JKWindowServer: accept failed\n");
        return;
    }

    // Expect Hello.
    ipc::Message hello;
    if (!ipc::ReadMessage(*transport, hello) || hello.type != ipc::MsgType::Hello) {
        std::fprintf(stderr, "JKWindowServer: expected Hello, got type=%u\n",
                     static_cast<uint32_t>(hello.type));
        return;
    }

    // Expect CreateSurface.
    ipc::Message createMsg;
    if (!ipc::ReadMessage(*transport, createMsg) || createMsg.type != ipc::MsgType::CreateSurface ||
        createMsg.payload.size() < sizeof(ipc::SurfaceCreatePayload)) {
        std::fprintf(stderr, "JKWindowServer: expected CreateSurface\n");
        return;
    }

    ipc::SurfaceCreatePayload create{};
    std::memcpy(&create, createMsg.payload.data(), sizeof(create));

    uint32_t id = nextSurfaceId_++;
    auto client = std::make_unique<JKClientConnection>(id, std::move(transport));

    if (!client->CreateSurface(create.width, create.height, create.title)) {
        std::fprintf(stderr, "JKWindowServer: failed to create surface\n");
        return;
    }

    ipc::SurfaceCreatedPayload created{};
    created.surfaceId = id;
    std::string shmName = std::string("Local\\JKSurfaceShm_") + std::to_string(id);
    std::strncpy(created.shmName, shmName.c_str(), sizeof(created.shmName) - 1);
    if (!client->Send(ipc::MsgType::SurfaceCreated, &created, sizeof(created))) {
        std::fprintf(stderr, "JKWindowServer: failed to send SurfaceCreated\n");
        return;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer_,
                                             SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             client->Width(),
                                             client->Height());
    if (!texture) {
        std::fprintf(stderr, "JKWindowServer: failed to create texture: %s\n", SDL_GetError());
        return;
    }
    client->SetTexture(texture);

    // Center the first client surface in the server window.
    int ww = 0, wh = 0;
    SDL_GetWindowSize(window_, &ww, &wh);
    client->SetPosition(std::max(0, (ww - client->Width()) / 2),
                        std::max(0, (wh - client->Height()) / 2));

    client->StartReadThread();

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_.push_back(std::move(client));
    }

    std::fprintf(stderr, "JKWindowServer: client surface %u created (%dx%d)\n",
                 id, create.width, create.height);

    Run();
}

void JKWindowServer::AcceptorLoop() {
    // Future multi-client accept loop. Not used by RunSingleAccept.
    while (running_) {
        auto transport = ipc::JKPipeTransport::CreateServer(pipeName_);
        if (!transport) {
            std::fprintf(stderr, "JKWindowServer::AcceptorLoop: accept failed\n");
            continue;
        }
        // TODO: read Hello/CreateSurface, create shared memory, add client.
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
            }
        }
    } else if (msg.type == ipc::MsgType::Close) {
        // Client explicitly closed.
    }
}

void JKWindowServer::Composite() {
    if (!renderer_) return;

    SDL_SetRenderDrawColor(renderer_, 64, 64, 64, 255);
    SDL_RenderClear(renderer_);

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (auto& client : clients_) {
            if (!client || !client->GetTexture() || !client->SurfaceData()) {
                continue;
            }

            if (client->IsDirty()) {
                SDL_UpdateTexture(client->GetTexture(),
                                  nullptr,
                                  client->SurfaceData(),
                                  client->Width() * 4);
                client->ClearDirty();
            }

            SDL_Rect dst{ client->X(), client->Y(), client->Width(), client->Height() };
            SDL_RenderCopy(renderer_, client->GetTexture(), nullptr, &dst);
        }
    }

    SDL_RenderPresent(renderer_);
}

void JKWindowServer::CleanupDisconnectedClients() {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    auto it = clients_.begin();
    while (it != clients_.end()) {
        auto& client = *it;
        if (client && client->IsDisconnected()) {
            client->StopReadThread();
            pendingCleanup_.push_back(std::move(client));
            it = clients_.erase(it);
        } else {
            ++it;
        }
    }

    auto pit = pendingCleanup_.begin();
    while (pit != pendingCleanup_.end()) {
        auto& client = *pit;
        if (client->GetTexture()) {
            SDL_DestroyTexture(client->GetTexture());
            client->SetTexture(nullptr);
        }
        pit = pendingCleanup_.erase(pit);
    }
}

} // namespace server
} // namespace jk
