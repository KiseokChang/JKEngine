#include <JKSoundManager.h>
#include <JKSDLAudioBackend.h>
#include <SDL.h>
#include <cstdio>

namespace jk {

void JKSoundManager::SetBackend(std::unique_ptr<IAudioBackend> backend) {
    Quit();
    backend_ = std::move(backend);
}

bool JKSoundManager::Init() {
    if (!backend_) {
        backend_ = std::make_unique<SDLAudioBackend>();
    }

    if (!backend_->Init()) {
        return false;
    }

    // 기본 버스 볼륨 초기화.
    busVolumes_[kAudioBusUI] = 1.0f;
    busVolumes_[kAudioBusTetris] = 1.0f;
    busVolumes_[kAudioBusMine] = 1.0f;
    busVolumes_[kAudioBusSystem] = 1.0f;

    LoadSFX("button_click", AssetPath("button_click.wav"));
    LoadSFX("mine_open", AssetPath("mine_open.wav"));
    LoadSFX("mine_explosion", AssetPath("mine_explosion.wav"));
    LoadSFX("tetris_move", AssetPath("tetris_move.wav"));
    LoadSFX("tetris_rotate", AssetPath("tetris_rotate.wav"));
    LoadSFX("tetris_drop", AssetPath("tetris_drop.wav"));
    LoadSFX("tetris_clear", AssetPath("tetris_clear.wav"));
    LoadSFX("tetris_gameover", AssetPath("tetris_gameover.wav"));

    LoadBGM("tetris_theme", AssetPath("tetris_theme.wav"));
    LoadBGM("mine_ambient", AssetPath("mine_ambient.wav"));

    return true;
}

void JKSoundManager::Quit() {
    if (backend_) {
        backend_->Quit();
        backend_.reset();
    }
    sfxHandles_.clear();
    bgmHandles_.clear();
    busVolumes_.clear();
}

std::string JKSoundManager::AssetPath(const std::string& filename) {
    char* basePath = SDL_GetBasePath();
    std::string path;
    if (basePath) {
        path = std::string(basePath) + "assets/sounds/" + filename;
        SDL_free(basePath);
    } else {
        path = std::string("assets/sounds/") + filename;
    }
    return path;
}

bool JKSoundManager::LoadSFX(const std::string& id, const std::string& filepath) {
    if (!backend_ || !backend_->IsReady()) return false;
    if (sfxHandles_.find(id) != sfxHandles_.end()) return true;

    uint64_t handle = backend_->LoadSFX(filepath);
    if (handle == 0) {
        std::fprintf(stderr, "JKSoundManager::LoadSFX failed (%s)\n", filepath.c_str());
        return false;
    }
    sfxHandles_[id] = handle;
    return true;
}

void JKSoundManager::PlaySFX(const std::string& id, const std::string& busId, int loops) {
    if (!backend_ || !backend_->IsReady()) return;
    auto it = sfxHandles_.find(id);
    if (it == sfxHandles_.end()) return;

    float busVolume = GetBusVolume(busId);
    backend_->PlaySFX(it->second, loops, masterVolume_, busVolume);
}

bool JKSoundManager::LoadBGM(const std::string& id, const std::string& filepath) {
    if (!backend_ || !backend_->IsReady()) return false;
    if (bgmHandles_.find(id) != bgmHandles_.end()) return true;

    uint64_t handle = backend_->LoadBGM(filepath);
    if (handle == 0) {
        std::fprintf(stderr, "JKSoundManager::LoadBGM failed (%s)\n", filepath.c_str());
        return false;
    }
    bgmHandles_[id] = handle;
    return true;
}

void JKSoundManager::PlayBGM(const std::string& id, int loops) {
    if (!backend_ || !backend_->IsReady()) return;
    auto it = bgmHandles_.find(id);
    if (it == bgmHandles_.end()) return;

    backend_->PlayBGM(it->second, loops, masterVolume_, bgmVolume_);
}

void JKSoundManager::StopBGM() {
    if (backend_) backend_->StopBGM();
}

void JKSoundManager::PauseBGM() {
    if (backend_) backend_->PauseBGM();
}

void JKSoundManager::ResumeBGM() {
    if (backend_) backend_->ResumeBGM();
}

void JKSoundManager::SetMasterVolume(float volume) {
    masterVolume_ = Clamp01(volume);
    if (backend_) {
        backend_->SetMasterVolume(masterVolume_);
        backend_->SetBGMVolume(masterVolume_, bgmVolume_);
    }
}

void JKSoundManager::SetBusVolume(const std::string& busId, float volume) {
    busVolumes_[busId] = Clamp01(volume);
    if (backend_) {
        backend_->SetBusVolume(0, masterVolume_, busVolumes_[busId]);
    }
}

void JKSoundManager::SetBGMVolume(float volume) {
    bgmVolume_ = Clamp01(volume);
    if (backend_) {
        backend_->SetBGMVolume(masterVolume_, bgmVolume_);
    }
}

float JKSoundManager::GetBusVolume(const std::string& busId) const {
    auto it = busVolumes_.find(busId);
    return (it != busVolumes_.end()) ? it->second : 1.0f;
}

} // namespace jk
