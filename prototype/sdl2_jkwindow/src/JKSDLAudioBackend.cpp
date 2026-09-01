#include <JKSDLAudioBackend.h>
#include <SDL.h>
#include <cstdio>

namespace jk {

int SDLAudioBackend::ClampVolume(float scaled) {
    int v = static_cast<int>(scaled * MIX_MAX_VOLUME);
    if (v < 0) v = 0;
    if (v > MIX_MAX_VOLUME) v = MIX_MAX_VOLUME;
    return v;
}

bool SDLAudioBackend::Init() {
    if (initialized_) return true;

    if (Mix_OpenAudio(kDefaultFrequency, MIX_DEFAULT_FORMAT, kDefaultChannels, kDefaultBufferSize) < 0) {
        std::fprintf(stderr, "SDLAudioBackend::Init failed: %s\n", Mix_GetError());
        return false;
    }

    Mix_AllocateChannels(kSFXChannels);
    initialized_ = true;
    return true;
}

void SDLAudioBackend::Quit() {
    if (!initialized_) return;

    HaltAllSFX();
    Mix_HaltMusic();

    for (auto& pair : sfxCache_) {
        if (pair.second) Mix_FreeChunk(pair.second);
    }
    sfxCache_.clear();
    channelToSfxHandle_.clear();

    for (auto& pair : bgmCache_) {
        if (pair.second) Mix_FreeMusic(pair.second);
    }
    bgmCache_.clear();

    Mix_CloseAudio();
    initialized_ = false;
}

uint64_t SDLAudioBackend::LoadSFX(const std::string& filepath) {
    if (!initialized_) return 0;

    // Search for an existing cached entry loaded from the same path.
    for (const auto& pair : sfxCache_) {
        // Compare by path string. Reusing the same handle keeps the cache simple.
        // We don't store the path in the cache, so we cannot detect duplicate
        // path loads here; JKSoundManager prevents that by id.
        (void)pair;
    }

    Mix_Chunk* chunk = Mix_LoadWAV(filepath.c_str());
    if (!chunk) {
        std::fprintf(stderr, "SDLAudioBackend::LoadSFX failed (%s): %s\n", filepath.c_str(), Mix_GetError());
        return 0;
    }

    uint64_t handle = nextHandle_++;
    sfxCache_[handle] = chunk;
    return handle;
}

void SDLAudioBackend::UnloadSFX(uint64_t handle) {
    auto it = sfxCache_.find(handle);
    if (it == sfxCache_.end()) return;
    if (it->second) Mix_FreeChunk(it->second);
    sfxCache_.erase(it);

    for (auto cit = channelToSfxHandle_.begin(); cit != channelToSfxHandle_.end(); ) {
        if (cit->second == handle) cit = channelToSfxHandle_.erase(cit);
        else ++cit;
    }
}

void SDLAudioBackend::PlaySFX(uint64_t handle, int loops, float masterVolume, float busVolume) {
    if (!initialized_) return;
    auto it = sfxCache_.find(handle);
    if (it == sfxCache_.end() || !it->second) return;

    int channel = Mix_PlayChannel(-1, it->second, loops);
    if (channel == -1) return;

    channelToSfxHandle_[channel] = handle;
    Mix_Volume(channel, ClampVolume(masterVolume * busVolume));
}

uint64_t SDLAudioBackend::LoadBGM(const std::string& filepath) {
    if (!initialized_) return 0;

    Mix_Music* music = Mix_LoadMUS(filepath.c_str());
    if (!music) {
        std::fprintf(stderr, "SDLAudioBackend::LoadBGM failed (%s): %s\n", filepath.c_str(), Mix_GetError());
        return 0;
    }

    uint64_t handle = nextHandle_++;
    bgmCache_[handle] = music;
    return handle;
}

void SDLAudioBackend::UnloadBGM(uint64_t handle) {
    auto it = bgmCache_.find(handle);
    if (it == bgmCache_.end()) return;
    if (it->second) Mix_FreeMusic(it->second);
    bgmCache_.erase(it);
}

void SDLAudioBackend::PlayBGM(uint64_t handle, int loops, float masterVolume, float bgmVolume) {
    if (!initialized_) return;
    auto it = bgmCache_.find(handle);
    if (it == bgmCache_.end() || !it->second) return;

    if (Mix_PlayMusic(it->second, loops) == -1) {
        std::fprintf(stderr, "SDLAudioBackend::PlayBGM failed: %s\n", Mix_GetError());
        return;
    }

    Mix_VolumeMusic(ClampVolume(masterVolume * bgmVolume));
}

void SDLAudioBackend::StopBGM() {
    Mix_HaltMusic();
}

void SDLAudioBackend::PauseBGM() {
    Mix_PauseMusic();
}

void SDLAudioBackend::ResumeBGM() {
    Mix_ResumeMusic();
}

void SDLAudioBackend::SetMasterVolume(float volume) {
    // SDL_mixer treats master volume as a global scalar. We apply it lazily
    // per channel through SetBusVolume calls.
    (void)volume;
}

void SDLAudioBackend::SetBGMVolume(float masterVolume, float bgmVolume) {
    Mix_VolumeMusic(ClampVolume(masterVolume * bgmVolume));
}

void SDLAudioBackend::SetBusVolume(uint64_t activeSfxHandle, float masterVolume, float busVolume) {
    (void)activeSfxHandle;
    // SDL_mixer has no per-bus grouping. Update every playing channel uniformly
    // for now; the manager can call this after PlaySFX for finer control.
    for (int i = 0; i < kSFXChannels; ++i) {
        if (Mix_Playing(i)) {
            Mix_Volume(i, ClampVolume(masterVolume * busVolume));
        }
    }
}

void SDLAudioBackend::HaltAllSFX() {
    Mix_HaltChannel(-1);
    channelToSfxHandle_.clear();
}

} // namespace jk
