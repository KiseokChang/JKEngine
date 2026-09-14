#ifndef APPS_CLIENTVPLAYERAPP_H
#define APPS_CLIENTVPLAYERAPP_H

#include <client/JKClientApplication.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace jk {

// ImGui Phase 3 long-term track (docs/23 §11.8): an FFmpeg video player
// client. Decoding runs on a worker thread inside the app; decoded video
// frames are uploaded to an SDL texture and drawn with ImGui::Image (the
// fused backend already binds arbitrary GetTexID SDL textures), and decoded
// audio streams to a client-side SDL audio device whose delivered-sample
// counter is the A/V master clock (research doc's audio-master-clock rule).
// The server stays a dumb compositor — it never sees pixels or PCM.
class ClientVPlayerApp : public JKClientApplication {
public:
    ~ClientVPlayerApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    bool PreProcessMessage(const JKEvent& ev) override;
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;

private:
    void BuildUi(int w, int h);
    void SyncVideoTexture(SDL_Renderer* renderer);
    void OpenPath(const char* path);

    // File picker (specs/2026-09-13-file-dialog Task 3): "열기..." sends one
    // file_open query on the agent channel; the server parks it until the
    // filedlg dialog resolves (open path / cancel). 1-in-flight — the query
    // id is 0 while nothing is pending. lastDir_ seeds the dialog's start
    // folder for the process lifetime (no persistence).
    void RequestOpenDialog();
    void PumpAgentReplies();

    uint32_t fileOpenQueryId_ = 0; // 0 = no file_open in flight
    uint32_t nextQueryId_ = 1;
    std::string lastDir_;

    struct PlayerCore; // FFmpeg demux/decode + SDL audio state, defined in cpp
    // Custom deleter: an out-of-line delete keeps PlayerCore incomplete in TUs
    // that only construct the app (EH cleanup paths still instantiate the
    // member's destructor there — default_delete would static_assert).
    struct PlayerCoreDeleter { void operator()(PlayerCore* p) const noexcept; };
    std::unique_ptr<PlayerCore, PlayerCoreDeleter> player_;

    bool frameDirty_ = true;
    bool imguiReady_ = false;
    std::chrono::steady_clock::time_point lastFrame_;

    // SDL_Texture*/SDL_Renderer* kept as void* so this header stays SDL-free.
    void* videoTex_ = nullptr;
    void* renderer_ = nullptr;
    int texW_ = 0, texH_ = 0;
    bool hasFrame_ = false;
    char pathBuf_[1024] = {0};
    float seekUi_ = 0.f;
    bool seekingUi_ = false;
    std::string openError_;

    // Jog dial (translucent rotary scrub overlay on the video). Screen-space
    // floats keep this header ImGui-free; the knob center and last mouse pos
    // only matter while a drag is active.
    bool jogActive_ = false;
    bool jogWasPlaying_ = false;
    double jogTarget_ = 0;      // scrub target while dragging (UI time)
    double jogLastSent_ = -1;   // last target sent to SeekScrub (debounce)
    float knobCX_ = 0, knobCY_ = 0;
    float jogMouseX_ = 0, jogMouseY_ = 0;
    std::chrono::steady_clock::time_point jogLastSeek_{};

    // Mouse-wheel scrub (spec 1d/D5): wheel ticks over the knob drive the
    // same jogTarget_/debounce as a drag; there is no wheel-release event,
    // so the session ends 400 ms after the last tick (idle timeout).
    bool wheelScrubbing_ = false;
    std::chrono::steady_clock::time_point wheelLastTick_{};
};

} // namespace jk

#endif // APPS_CLIENTVPLAYERAPP_H