#ifndef JKAUDIOTHREAD_H
#define JKAUDIOTHREAD_H

#include <JKAudioBackend.h>
#include <JKMessageBus.h>
#include <atomic>
#include <thread>
#include <memory>

namespace jk {

// Audio thread owns the active IAudioBackend (by default SDLAudioBackend with
// SDL_mixer). It reads AudioCommand payloads from the message bus Audio
// channel and executes them.
class JKAudioThread {
public:
    JKAudioThread();
    ~JKAudioThread();

    void Start(JKMessageBus* bus, std::unique_ptr<IAudioBackend> backend);
    void Stop();

private:
    JKMessageBus* bus_ = nullptr;
    std::unique_ptr<IAudioBackend> backend_;
    std::atomic<bool> running_{false};
    std::thread thread_;

    float masterVolume_ = 1.0f;
    float bgmVolume_ = 0.4f;
    std::unordered_map<std::string, float> busVolumes_;
    std::unordered_map<std::string, uint64_t> sfxHandles_;
    std::unordered_map<std::string, uint64_t> bgmHandles_;

    void Run();
    void ProcessCommand(const JKMessageBus::Payload& payload);
    std::string AssetPath(const std::string& filename);
};

} // namespace jk

#endif // JKAUDIOTHREAD_H
