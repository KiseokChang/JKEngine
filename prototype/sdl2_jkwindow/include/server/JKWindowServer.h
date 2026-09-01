#ifndef JKWINDOWSERVER_H
#define JKWINDOWSERVER_H

#include <server/JKClientConnection.h>
#include <JKTypes.h>
#include <SDL.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace jk {
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
    void Run();
    void Stop();

    // Run a one-shot accept of a single client and then enter the render loop.
    // Useful for the first scaffolding test before adding a background acceptor.
    void RunSingleAccept(const std::string& pipeName);

private:
    void AcceptorLoop();
    void ProcessPendingMessages();
    void ProcessClientMessage(JKClientConnection& client, const ipc::Message& msg);
    void Composite();
    void CleanupDisconnectedClients();

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;

    std::string pipeName_;
    std::atomic<bool> running_{false};
    std::thread acceptorThread_;

    std::mutex clientsMutex_;
    std::vector<std::unique_ptr<JKClientConnection>> clients_;
    std::vector<std::unique_ptr<JKClientConnection>> pendingCleanup_;
    uint32_t nextSurfaceId_ = 1;
};

} // namespace server
} // namespace jk

#endif // JKWINDOWSERVER_H
