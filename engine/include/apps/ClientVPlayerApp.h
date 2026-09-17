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
    void OnThemeChanged() override;  // ImGui palette re-apply (docs/52)
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
    // 외부 토글 동기화(스펙 §2.2 → 설정 허브 §2.3 단일 펌프 이관): agentctl 등
    // 제3자가 window_fullscreen을 호출하면 서버가 window.fullscreen(_exit)
    // 이벤트를 발행한다 — 이벤트는 코어 펌프(JKClientApplication 유일 소비자)가
    // 받아 이 훅으로 전달하고, 여기선 자기 id 이벤트만 미러에 반영한다
    // (도구 응답만으론 외부 토글을 못 본다 — vpt12 1차런 결함).
    void OnAgentEvent(const std::string& eventJson) override;

    // 전체화면/극장 모드(스펙 2026-09-17 vplayer-fullscreen-osd §2.2-2.3):
    // RequestFullscreen은 file_open과 같은 1-in-flight agent 쿼리(명시 on —
    // 생략형 반전 대신 클라 의도 고정). fullscreenUi_는 도구 응답의
    // "fullscreen" 값으로만 갱신된다(외부 토글 포함 최신 상태 거울).
    // TheaterUi는 BuildUi 최상단에서 분기 진입(창 모드 본문 무손상) —
    // PlayerCore가 헤더에서 불완전 중첩 타입이라 파라미터 없이 자체 조회한다.
    void RequestFullscreen(bool on);
    void TheaterUi(int w, int h);

    uint32_t fileOpenQueryId_ = 0; // 0 = no file_open in flight
    uint32_t fsQueryId_ = 0;       // 0 = no window_fullscreen in flight
    uint32_t nextQueryId_ = 1;
    std::string lastDir_;

    bool fullscreenUi_ = false;
    // OSD: 하단 밴드(22%) + 2.5s 아이들 + 200ms 페이드(스펙 §2.3).
    float osdAlpha_ = 0.f; // 0..1 (페이드 보간)
    bool osdShown_ = false;
    std::chrono::steady_clock::time_point osdLastActivity_{};
    std::chrono::steady_clock::time_point lastOsdTick_{};

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
    double jogLastSent_ = -1;   // last target sent to JogTo/SeekScrub
    float knobCX_ = 0, knobCY_ = 0;
    float jogMouseX_ = 0, jogMouseY_ = 0;
    std::chrono::steady_clock::time_point jogLastSeek_{};

    // Render-rate metric: UI-thread-only counter refreshed by
    // SyncVideoTexture (uploads/sec over a 1 s window), shown in the status
    // row — the probe reads it as the 4K upload-bottleneck verdict.
    int renderFrames_ = 0;          // uploads in the current 1 s window
    double renderWindowStart_ = 0;  // SDL_GetTicks-based window start (s)
    int renderHz_ = 0;              // last completed window's rate

    // Mouse-wheel scrub (spec 1d/D5): wheel ticks over the knob drive the
    // same jogTarget_ frame-scrub pump (ring hit = JogTo, ring-miss =
    // debounced SeekScrub fallback); there is no wheel-release event,
    // so the session ends 400 ms after the last tick (idle timeout).
    bool wheelScrubbing_ = false;
    std::chrono::steady_clock::time_point wheelLastTick_{};

    // Reverse auto-play (spec 2026-09-15 section 7 v2): the jog ring walked
    // backward at content fps by the UI cadence (Task 3); ring exhaustion
    // falls back to the debounced keyframe SeekScrub (GOP-boundary stutter
    // accepted by spec). The session opens/closes exactly like a drag/wheel
    // scrub (auto-pause + SetJog(true) = silent), so the same finishScrub
    // precision-seek contract applies on every exit.
    bool reverseActive_ = false;
    double reverseAcc_ = 0.0; // fractional-frame cadence carry (seconds)
    std::chrono::steady_clock::time_point reverseLastTick_{};
    // [vpt11] pacing-log throttle — once per second while reverseActive_
    // (probe cadence gate; structure-only checks cannot see pacing).
    std::chrono::steady_clock::time_point reverseLastLog_{};
};

} // namespace jk

#endif // APPS_CLIENTVPLAYERAPP_H