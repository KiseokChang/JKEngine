#ifndef JKSOUNDMANAGER_H
#define JKSOUNDMANAGER_H

#include <JKAudioBackend.h>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <memory>

namespace jk {

// 게임/창별 오디오를 분리하는 버스 식별자 상수.
constexpr const char* kAudioBusUI = "UI";
constexpr const char* kAudioBusTetris = "Tetris";
constexpr const char* kAudioBusMine = "Mine";
constexpr const char* kAudioBusSystem = "System";

class JKSoundManager {
public:
    static JKSoundManager& GetInstance() {
        static JKSoundManager instance;
        return instance;
    }

    JKSoundManager(const JKSoundManager&) = delete;
    JKSoundManager& operator=(const JKSoundManager&) = delete;

    // Replace the default SDL backend. Must be called before Init().
    void SetBackend(std::unique_ptr<IAudioBackend> backend);

    // When true, all load/play/volume calls are forwarded to the application's
    // audio thread via AudioCommand messages instead of touching a local SDL_mixer
    // backend. This is set by JKApplication in the threaded build.
    void SetCommandMode(bool enabled) { commandMode_ = enabled; }

    bool Init();
    void Quit();

    // 효과음(SFX) 로드/재생.
    bool LoadSFX(const std::string& id, const std::string& filepath);
    void PlaySFX(const std::string& id, const std::string& busId = kAudioBusUI, int loops = 0);

    // 배경음악(BGM) 로드/재생.
    bool LoadBGM(const std::string& id, const std::string& filepath);
    void PlayBGM(const std::string& id, int loops = -1);
    void StopBGM();
    void PauseBGM();
    void ResumeBGM();

    // 볼륨 제어 (0.0 ~ 1.0).
    void SetMasterVolume(float volume);
    void SetBusVolume(const std::string& busId, float volume);
    void SetBGMVolume(float volume);

    float GetMasterVolume() const { return masterVolume_; }
    float GetBusVolume(const std::string& busId) const;
    float GetBGMVolume() const { return bgmVolume_; }

    // Internal helper used by the default SDL backend to locate asset files.
    static std::string AssetPath(const std::string& filename);

private:
    JKSoundManager() = default;
    ~JKSoundManager() { Quit(); }

    std::unique_ptr<IAudioBackend> backend_;
    bool commandMode_ = false;

    std::unordered_map<std::string, uint64_t> sfxHandles_;
    std::unordered_map<std::string, uint64_t> bgmHandles_;
    std::unordered_map<std::string, float> busVolumes_;

    float masterVolume_ = 1.0f;
    float bgmVolume_ = 0.4f;

    static float Clamp01(float v) {
        if (v < 0.0f) return 0.0f;
        if (v > 1.0f) return 1.0f;
        return v;
    }
};

} // namespace jk

#endif // JKSOUNDMANAGER_H
