#include <JKAudioThread.h>
#include <JKAudioCommand.h>
#include <JKSDLAudioBackend.h>
#include <JKSoundManager.h>
#include <SDL.h>
#include <cstring>
#include <cstdio>

namespace jk {

static AudioCommand Decode(const std::vector<uint8_t>& data) {
    AudioCommand cmd;
    if (data.size() >= sizeof(AudioCommand)) {
        std::memcpy(&cmd, data.data(), sizeof(AudioCommand));
    }
    return cmd;
}

JKAudioThread::JKAudioThread() = default;

JKAudioThread::~JKAudioThread() {
    Stop();
}

void JKAudioThread::Start(JKMessageBus* bus, std::unique_ptr<IAudioBackend> backend) {
    if (running_ || thread_.joinable()) return;
    bus_ = bus;
    backend_ = std::move(backend);
    if (!backend_) {
        backend_ = std::make_unique<SDLAudioBackend>();
    }
    running_ = true;
    thread_ = std::thread(&JKAudioThread::Run, this);
}

void JKAudioThread::Stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    if (backend_) {
        backend_->Quit();
        backend_.reset();
    }
    bus_ = nullptr;
}

void JKAudioThread::Run() {
    while (running_) {
        JKMessageBus::Payload payload;
        if (bus_ && bus_->PopWait(JKMessageBus::Channel::Audio, payload, 100)) {
            ProcessCommand(payload);
        }
    }
}

std::string JKAudioThread::AssetPath(const std::string& filename) {
    return JKSoundManager::AssetPath(filename);
}

void JKAudioThread::ProcessCommand(const JKMessageBus::Payload& payload) {
    AudioCommand cmd = Decode(payload.data);
    switch (cmd.type) {
        case AudioCommand::Type::Init: {
            if (SDL_InitSubSystem(SDL_INIT_AUDIO) >= 0) {
                if (!backend_->Init()) {
                    SDL_QuitSubSystem(SDL_INIT_AUDIO);
                }
            }
            break;
        }
        case AudioCommand::Type::Quit: {
            if (backend_) {
                backend_->Quit();
            }
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            break;
        }
        case AudioCommand::Type::LoadSFX: {
            if (!backend_ || !backend_->IsReady()) break;
            std::string id(cmd.id);
            if (sfxHandles_.find(id) != sfxHandles_.end()) break;
            uint64_t h = backend_->LoadSFX(cmd.path);
            if (h != 0) sfxHandles_[id] = h;
            break;
        }
        case AudioCommand::Type::PlaySFX: {
            if (!backend_ || !backend_->IsReady()) break;
            std::string id(cmd.id);
            auto it = sfxHandles_.find(id);
            if (it == sfxHandles_.end()) break;
            std::string bus(cmd.busId);
            float busVol = 1.0f;
            auto b = busVolumes_.find(bus);
            if (b != busVolumes_.end()) busVol = b->second;
            backend_->PlaySFX(it->second, cmd.loops, masterVolume_, busVol);
            break;
        }
        case AudioCommand::Type::LoadBGM: {
            if (!backend_ || !backend_->IsReady()) break;
            std::string id(cmd.id);
            if (bgmHandles_.find(id) != bgmHandles_.end()) break;
            uint64_t h = backend_->LoadBGM(cmd.path);
            if (h != 0) bgmHandles_[id] = h;
            break;
        }
        case AudioCommand::Type::PlayBGM: {
            if (!backend_ || !backend_->IsReady()) break;
            std::string id(cmd.id);
            auto it = bgmHandles_.find(id);
            if (it == bgmHandles_.end()) break;
            backend_->PlayBGM(it->second, cmd.loops, masterVolume_, bgmVolume_);
            break;
        }
        case AudioCommand::Type::StopBGM:
            if (backend_) backend_->StopBGM();
            break;
        case AudioCommand::Type::PauseBGM:
            if (backend_) backend_->PauseBGM();
            break;
        case AudioCommand::Type::ResumeBGM:
            if (backend_) backend_->ResumeBGM();
            break;
        case AudioCommand::Type::SetMasterVolume:
            masterVolume_ = cmd.volume;
            if (backend_) backend_->SetMasterVolume(masterVolume_);
            break;
        case AudioCommand::Type::SetBusVolume:
            busVolumes_[std::string(cmd.busId)] = cmd.busVolume;
            if (backend_) backend_->SetBusVolume(0, masterVolume_, cmd.busVolume);
            break;
        case AudioCommand::Type::SetBGMVolume:
            bgmVolume_ = cmd.volume;
            if (backend_) backend_->SetBGMVolume(masterVolume_, bgmVolume_);
            break;
        case AudioCommand::Type::HaltAllSFX:
            if (backend_) backend_->HaltAllSFX();
            break;
        default:
            break;
    }
}

} // namespace jk
