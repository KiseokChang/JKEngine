#ifndef JKWINDOWSERVER_H
#define JKWINDOWSERVER_H

#include <server/JKClientConnection.h>
#include <JKAudioCommand.h>
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

class JKMessageBus;
class JKAudioThread;

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
    void HandleSDLEvent(const SDL_Event& ev);
    void SendInputEvent(JKClientConnection& client, const ipc::InputEventPayload& payload);
    JKClientConnection* HitTestClient(int32_t x, int32_t y);
    JKClientConnection* FindClientById(uint32_t surfaceId);
    void SetFocusedClient(uint32_t surfaceId);
    void Composite();
    void CleanupDisconnectedClients();
    void UnblockAcceptor();
    void InitAudio();
    void PostAudioCommand(const AudioCommand& cmd);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;

    std::unique_ptr<JKMessageBus> messageBus_;
    std::unique_ptr<JKAudioThread> audioThread_;

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
};

} // namespace server
} // namespace jk

#endif // JKWINDOWSERVER_H
