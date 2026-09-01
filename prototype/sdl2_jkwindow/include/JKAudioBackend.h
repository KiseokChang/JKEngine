#ifndef JKAUDIOBACKEND_H
#define JKAUDIOBACKEND_H

#include <string>
#include <cstdint>

namespace jk {

// Audio backend abstraction used by JKSoundManager. The default implementation
// is SDLAudioBackend (SDL_mixer). Future window-server phases can swap this
// for an IPC backend that forwards commands to a centralized audio server.
class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    // Initialize/shutdown the backend. Returns true if audio output is ready.
    virtual bool Init() = 0;
    virtual void Quit() = 0;

    // Returns true when the backend is initialized and usable.
    virtual bool IsReady() const = 0;

    // SFX load/play. A loaded sound returns an opaque handle; 0 means invalid.
    virtual uint64_t LoadSFX(const std::string& filepath) = 0;
    virtual void UnloadSFX(uint64_t handle) = 0;
    virtual void PlaySFX(uint64_t handle, int loops, float masterVolume, float busVolume) = 0;

    // BGM load/play.
    virtual uint64_t LoadBGM(const std::string& filepath) = 0;
    virtual void UnloadBGM(uint64_t handle) = 0;
    virtual void PlayBGM(uint64_t handle, int loops, float masterVolume, float bgmVolume) = 0;
    virtual void StopBGM() = 0;
    virtual void PauseBGM() = 0;
    virtual void ResumeBGM() = 0;

    // Global volume controls.
    virtual void SetMasterVolume(float volume) = 0;
    virtual void SetBGMVolume(float masterVolume, float bgmVolume) = 0;
    virtual void SetBusVolume(uint64_t activeSfxHandle, float masterVolume, float busVolume) = 0;

    // Halt all currently playing sounds.
    virtual void HaltAllSFX() = 0;
};

} // namespace jk

#endif // JKAUDIOBACKEND_H
