#ifndef JKSDLAUDIOBACKEND_H
#define JKSDLAUDIOBACKEND_H

#include <JKAudioBackend.h>
#include <SDL_mixer.h>
#include <string>
#include <unordered_map>
#include <cstdint>

namespace jk {

// SDL_mixer based default implementation of IAudioBackend.
class SDLAudioBackend : public IAudioBackend {
public:
    SDLAudioBackend() = default;
    ~SDLAudioBackend() override { Quit(); }

    bool Init() override;
    void Quit() override;
    bool IsReady() const override { return initialized_; }

    uint64_t LoadSFX(const std::string& filepath) override;
    void UnloadSFX(uint64_t handle) override;
    void PlaySFX(uint64_t handle, int loops, float masterVolume, float busVolume) override;

    uint64_t LoadBGM(const std::string& filepath) override;
    void UnloadBGM(uint64_t handle) override;
    void PlayBGM(uint64_t handle, int loops, float masterVolume, float bgmVolume) override;
    void StopBGM() override;
    void PauseBGM() override;
    void ResumeBGM() override;

    void SetMasterVolume(float volume) override;
    void SetBGMVolume(float masterVolume, float bgmVolume) override;
    void SetBusVolume(uint64_t activeSfxHandle, float masterVolume, float busVolume) override;

    void HaltAllSFX() override;

private:
    bool initialized_ = false;
    uint64_t nextHandle_ = 1;

    std::unordered_map<uint64_t, Mix_Chunk*> sfxCache_;
    std::unordered_map<uint64_t, Mix_Music*> bgmCache_;
    std::unordered_map<int, uint64_t> channelToSfxHandle_;

    static constexpr int kDefaultFrequency = 44100;
    static constexpr int kDefaultChannels = 2;
    static constexpr int kDefaultBufferSize = 1024;
    static constexpr int kSFXChannels = 32;

    static int ClampVolume(float scaled);
};

} // namespace jk

#endif // JKSDLAUDIOBACKEND_H
