#include <apps/ClientVPlayerApp.h>

#include <agent/JKAgentJson.h>
#include <imgui_impl_jkwindow.h>
#include <imgui.h>
#include "theme/JKThemeImGui.h"
#include <JKWindow.h>
#include <SDL.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/rational.h>
}

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace jk {

namespace {

// Root window paints the dark clear color so the surface never flashes white
// behind the ImGui player panel (docs/23 §11 app idiom).
class VPlayerRoot : public JKWindow {
public:
    explicit VPlayerRoot(const std::string& title) : JKWindow(title) {}
    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetClientRect();
        const auto& t = jk::theme::current();
        dc.SetColor(t.appClearBg.r, t.appClearBg.g, t.appClearBg.b, 255);
        dc.FillRect(client);
    }
};

void FormatTime(char* buf, size_t n, double t) {
    if (t < 0) t = 0;
    const int total = static_cast<int>(t);
    std::snprintf(buf, n, "%02d:%02d", total / 60, total % 60);
}

std::string AvErr(int err) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(err, buf, sizeof(buf));
    return buf;
}

// Sanctioned semantic-red for open/playback error text (docs/45 theme token
// path; dedup per review MINOR-3 — was copy-pasted at 4 call sites).
const ImVec4 kErrorRed(1.0f, 0.4f, 0.4f, 1.0f);

// JSON string escape for agent-query args (same shape as the palette's
// EscapeJson / the server's JsonEsc: quotes, backslashes, control bytes).
std::string EscapeJson(const std::string& in) {
    std::string out;
    for (char c : in) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (static_cast<unsigned char>(c) < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", c);
            out += buf;
        } else {
            out += c;
        }
    }
    return out;
}

// Brief-specified filter, verbatim (filedlg treats it as display text; the
// dialog app parses the *.ext list out of it).
constexpr const char* kVideoFilter =
    "동영상 (*.mp4;*.mkv;*.avi;*.webm;*.mov)";

// One decoded video frame in a single contiguous NV12 buffer (Y plane at
// offset 0, stride w; interleaved UV plane at offset w*h, stride w) — 3/8
// the bytes of the old RGBA layout (12 bpp vs 32 bpp, ~12.4 MB at 4K), so
// both the sws conversion and the SDL_UpdateNVTexture upload cost 62% less
// (the suspected 27 Hz render bottleneck, docs/50 §7.4-① — later measured
// NOT the bottleneck, §9). Odd-dimension sources keep RGBA (NV12 needs
// even w/h) — nv12 flags which layout this buffer holds. pix is a pooled
// shared buffer: at 4K a fresh allocation per frame measured ~20 ms; the
// pool recycles released slots (docs/50 §7). Ownership follows the
// refcount — frames parked in videoQ/jogRing or held by the last UI
// upload keep their buffer alive.
struct VideoFrame {
    double pts = 0;
    int w = 0, h = 0;
    bool nv12 = true;
    std::shared_ptr<std::vector<uint8_t>> pix;
};
static inline size_t bytesFor(int w, int h) { return (size_t)w * (size_t)h * 3 / 2; }
static inline size_t FrameBytes(const VideoFrame& vf) {
    return vf.nv12 ? bytesFor(vf.w, vf.h) : (size_t)vf.w * (size_t)vf.h * 4;
}

} // namespace

// PlayerCore owns everything FFmpeg/SDL-audio: demux+decode runs on a worker
// thread, the UI thread only snapshots state, pulls timed-out video frames and
// pokes paused/seek/volume. Decoded audio is pushed into a byte ring that the
// SDL audio callback drains — framesPlayed there is the A/V master clock
// (research doc's audio-master-clock rule). The server never sees pixels/PCM.
struct ClientVPlayerApp::PlayerCore {
    AVFormatContext* fmt = nullptr;
    AVCodecContext* vctx = nullptr;
    AVCodecContext* actx = nullptr;
    SwsContext* sws = nullptr;
    SwrContext* swr = nullptr;
    AVRational videoTb{};
    AVRational audioTb{};                    // demuxer seek domain for audio-only files
    int videoStream = -1, audioStream = -1;
    int videoW = 0, videoH = 0;
    int audioRate = 0;                       // output rate == input rate (no resample)
    static constexpr int kAudioCh = 2;       // always mix down/up to stereo S16
    double duration = 0;
    // Presentation-time origin. Streams need not start at 0 (MPEG-TS starts
    // ~1.4 s; MP4 audio can lag video). Video pts are normalized by
    // ptsOrigin, and the audio clock (which counts consumed samples from the
    // first played sample, i.e. from the audio stream's own start) by
    // audioRef = the audio start in UI time. Clock, seek targets and the UI
    // slider all live in [0, duration].
    double ptsOrigin = 0;                    // file start_time, seconds
    double audioLead = 0;                    // audio stream start - ptsOrigin
    double audioRef = 0;                     // clock value at framesPlayed==0
    double devLatency = 0;                   // device buffer period (got.samples/rate):
                                             // copied samples become audible this much later
    double fps = 0;                          // video frame rate (0 = unknown), for ±1F stepping
    bool useWallClock = false;               // files without usable audio

    // Even-dimension sources render NV12 (half the upload bytes); odd ones
    // keep RGBA (NV12 needs even w/h) — decided at open, spec §4.
    bool nv12Out = false;
    std::string lastError;

    // --- Async open state machine (vplayer-stability T1, spec D1) ------------
    // OpenPath only arms the format context + spawns the EXISTING worker; the
    // demux/codec bring-up runs as the worker's first stage (OpenStage), so a
    // hostile file can stall the worker but never the UI thread. The UI polls
    // `phase` through SnapNow; everything OpenStage writes before the
    // Running transition happens-before the UI's observation of it (same `m`).
    enum class Phase { Opening, Running, Failed };
    Phase phase = Phase::Opening;
    std::string openPath_;                       // path the worker opens
    std::chrono::steady_clock::time_point openDeadline{}; // set pre-spawn (UI)
    std::atomic<bool> cancelOpen{false};         // interrupt_callback: abort now
    std::atomic<bool> openDeadlineActive{true};  // open-phase guard; disarmed
                                                 // once playback starts
    int eagainStreak = 0;                        // consecutive av_read_frame EAGAIN
    // Consecutive decode failures. Worker (audio) and the video decode thread
    // both bump it, so it must be atomic (DecodeFail takes m; the counter
    // bump itself stays lock-free).
    std::atomic<int> decodeFailStreak{0};        // consecutive decode failures

    std::mutex m;                            // decode/transport state
    std::condition_variable cv;
    std::mutex ringM;                        // audio ring only (SDL callback takes this)
    std::condition_variable cvRing;
    std::thread worker;
    bool workerDone = false;                 // worker set before its last cv notify
    std::atomic<bool> paused{false};         // atomic: ClockNow reads it under ringM
    bool ended = false, stop = false, wantSeek = false;
    bool jogSeek = false;                    // setter-owned: pending seek is a scrub (keyframe-only)
    std::atomic<bool> jogging{false};        // scrub drag in progress: worker skips audio decode
    double seekTarget = 0;
    double dropBeforePts = -1;               // frames older than this are stale (post-seek)
    // Audio twin of dropBeforePts. Worker-thread-only (written by the seek's
    // Ok stage, read by the worker's read loop — never the UI thread, so no
    // lock). The demuxer seek restarts BOTH tracks at the landing keyframe
    // (mp4 aligns every stream to the video keyframe's DTS), so after a
    // keyframe-clamped backward seek the first audio packets sit up to a
    // whole GOP below the clock target. Decoding them into the ring fills it
    // with content the clock has already passed and parks the demuxer on the
    // full ring BELOW the video clock gate — the decode-forward then never
    // reaches dropBeforePts, videoQ stays empty and the picture freezes on
    // the pre-seek frame (the jog fallback wedge: ±1F cannot revive, the
    // landing keyframe is the same). Dropped at the source instead; the
    // first packet at/above the target disarms the gate. This also removes
    // the latent A/V desync of keyframe-clamped seeks (landing-position
    // audio played over target-position video).
    double audioSkipBelow = -1;
    bool postSeekJump = false;               // gate-starved first frame may display (see PopVideoFrame)
    uint64_t seekGen = 0;                    // bumped on seek; in-flight audio pushes abort
    // Seek generation for the video gate loop (VideoLoop). Stage (a) bumps it
    // under m; the gate loop compares against the value it captured at
    // dequeue. wantSeek alone cannot abort a hold: the worker consumes
    // wantSeek at its loop top and a PARKED worker (ended) completes the
    // whole seek in a few ms, so the gate loop's 10 ms poll can miss the
    // window entirely and hold the pre-seek packet forever — the pinned
    // paused clock never advances the gate past it, the decode-forward never
    // runs and the picture freezes (the jog fallback wedge, which ±1F cannot
    // revive: its seek window is just as short). A generation survives that
    // race — stage (a) bumps it before stage (b)'s I/O and it never unwinds,
    // so the very next poll sees the change. Relaxed is enough: the gate
    // loop re-reads it every poll under m, and stage (a) writes it under m.
    std::atomic<uint64_t> vSeekSeq{0};

    // Failed-seek correction (T2, spec D3). Stage (a) snapshots the audio
    // clock + ring + stale-frame gates under the same locks it mutates them;
    // the wall-clock snapshot is taken in SeekCommon because THAT rebase runs
    // at request time on the UI thread. On a failed avformat_seek_file the
    // whole snapshot goes back (the demuxer never moved, so the restored
    // pipeline stays consistent) and seekError surfaces a one-shot notice.
    double undoAudioRef = 0;
    uint64_t undoFramesPlayed = 0;
    size_t undoRingR = 0, undoRingW = 0;
    double undoDropBeforePts = -1;
    bool undoPostSeekJump = false, undoEnded = false;
    double undoWallBase = 0, undoWallAccum = 0;
    std::chrono::steady_clock::time_point undoWallStart{};
    bool undoWallPlaying = false;
    // The snapshot must describe the last REAL position, never an
    // intermediate park (review MINOR-1): while a seek is between stage (a)
    // and its stage-(c) outcome, superseding requests (SeekCommon) and
    // chained stage-(a) passes skip the capture, so one snapshot survives
    // the whole supersede cascade and is consumed only by the cascade's
    // final Failed exit.
    bool seekInFlight = false;
    std::string seekError;                   // one-shot failed-seek notice (under m)
    std::chrono::steady_clock::time_point seekErrorAt{};

    // Pending decoded-frame cap. Must hold a decode burst: the clock gate
    // passes B-frame-reordered packets in clumps (3-4 frames decoded back to
    // back), and the display drop rule below keeps unexpired frames queued,
    // so the queue temporarily carries them. 6 slots ~ 200 ms at 30 fps /
    // ~120 ms at 50 fps (~75 MB at 4K NV12).
    static constexpr size_t kVideoQMaxFrames = 6;
    std::deque<VideoFrame> videoQ;

    // --- Jog frame-scrub history ring (spec 2026-09-15 §3.1) -------------
    // Retained decoded-frame suffix: the dial's instant-reverse window.
    // Contiguous decoded pixels — keyframe boundaries do NOT clear it (only
    // a seek does, stage (a)); GOP is a FALLBACK-path concept only. Trimmed
    // at push time to kJogRingMaxSecs / kJogRingMaxBytes; no backpressure —
    // the caps are the bound. m-protected (same domain as videoQ).
    static constexpr double kJogRingMaxSecs = 10.0;
    static constexpr size_t kJogRingMaxBytes = (size_t)1536 * 1024 * 1024;
    std::deque<VideoFrame> jogRing;
    size_t jogRingBytes = 0;
    // Live jog dial target (UI time). -1 = no frame-scrub session. Written
    // by JogTo under m; read by the video thread's clock gate and JogFrame
    // (UI thread). (The fallback scrub seek — SeekCommon scrub branch —
    // feeds it too, armed with the UI switch: Task 3.)
    double jogTargetPts = -1;

    // Pixel buffer pool for decoded frames (NV12, or RGBA for odd dims;
    // video-thread-only state): see the VideoFrame comment. Slots whose
    // refcount dropped to 1 (only the pool holds them) are recycled; the
    // cap bounds pathological bursts.
    std::vector<std::shared_ptr<std::vector<uint8_t>>> vPool;

    // --- Demux/decode split (docs/50 follow-up, 4K playback root fix) ----
    // The demux position runs up to the audio ring's 600 ms horizon ahead of
    // the presentation clock (audio-first refill), but a decoded 4K frame
    // is tens of MB (33 MB RGBA, ~12.4 MB NV12) — the old 3-frame videoQ
    // could never span that lead,
    // so every decoded frame was "future", the display starved ~90% of its
    // ticks and then collapsed several frames at once (freeze-then-jump), and
    // the T3 refill windows skipped mid-GOP video packets on top (dav1d
    // corruption). Video PACKETS, in contrast, are ~100 KB at 4K AV1 — so
    // the worker only ENQUEUES video packets into this bounded queue and a
    // dedicated decode thread drains it gated by the presentation clock:
    // decode just-in-time (packet order preserved — the reference chain
    // stays intact), convert, push to videoQ. At most a few decoded frames
    // exist at any moment, and no video packet is ever skipped.
    std::mutex vPktM;
    std::condition_variable vPktCv;
    std::deque<AVPacket*> vPktQ;             // owned packets (av_packet_ref'd)
    size_t vPktQBytes = 0;
    std::thread vthread;                     // video decode thread
    // ~16 MB ≈ 5 s of 4K AV1 packets. For A/V files the audio ring's own
    // backpressure parks the demuxer well before this; the bound exists for
    // video-only files (no ring to park on) so read-ahead cannot swallow the
    // whole file into memory.
    static constexpr size_t kVPktQMaxBytes = 16u * 1024 * 1024;
    // Decode clock gate lead, in seconds past ClockNow(). Small by design:
    // it is the queue-span headroom, and anything larger only adds display
    // latency (frames render when the clock reaches them).
    static constexpr double kVideoLead = 0.05;
    // Clump lead (docs/50 section 9 residual, task 5): with B-frame reordering
    // the gate passes a P packet once the P's OWN pts is within the lead, and
    // the lower-pts B packets attached behind it then cascade through
    // immediately (their pts already satisfies the condition) — decode output
    // arrives as a 3-frame clump whose tail B frames land ~1-2 frame intervals
    // AFTER their display time (measured push lateness: 480p +22-30 ms avg,
    // 4K +5-16 ms), so the display side drops them as expired. Widening the
    // gate lead by the clump width (P + its 2 attached B frames) lets the
    // whole clump through early enough that its tail arrives in the future,
    // where PopVideoFrame (task 4) keeps it queued until its turn. This
    // advances DECODE start only — display timing is still decided by the
    // PopVideoFrame clock gate, so A/V sync is untouched. Cost: ~2 frames of
    // extra decode-ahead (~67 ms @30 fps, ~40 ms @50 fps) and +2 frames in
    // the video pipeline/pool. Set from fps at open (clamped [1,240] there,
    // so always finite); the 30 fps seed covers streams whose fps stays
    // unknown.
    static constexpr double kClumpFrames = 2.0;
    double clumpLead = kClumpFrames / 30.0;
    // Frame-threaded decoders (dav1d with auto threads) emit a frame N
    // packets after N was sent — the decoder is a pipeline of depth D, and
    // throughput requires ~D packets in flight. Gating packets by a small
    // fixed lead leaves the pipeline starved (measured: dav1d fell to
    // 17 fps with a 120 ms runway vs 84 fps unthrottled). So the gate adds
    // the MEASURED pipeline delay (sent-packet pts minus received-frame
    // pts, EMA-smoothed; seeded from has_b_frames/fps at open). This both
    // saturates the decoder and keeps the emitted frames near-due — the
    // runway exists inside the decoder, not in decoded frames.
    double vPipeDelay = 0;
    // Seed only (see vPipeDelay): has_b_frames measured in OpenStage once
    // fps is known, in seconds.
    double vDecodeDelay = 0;
    // vctx is touched by the video thread (send/receive) and the worker
    // (DoSeekStages stage-(a) flush); this mutex serializes them. Held only
    // across the codec calls — never across an m acquisition (lock order:
    // m -> vdecM on the seek side; the video thread takes vdecM alone and
    // releases it before touching m, so no nesting inversion exists).
    std::mutex vdecM;

    // Audio byte ring (S16 stereo). One byte of slack distinguishes full/empty.
    uint8_t* ring = nullptr;
    size_t ringCap = 0, ringR = 0, ringW = 0;
    uint64_t framesPlayed = 0;               // master clock, in sample frames
    std::atomic<float> volume{0.8f};

    // --- Audio-first refill water marks (T3, spec 1b) --------------------
    // Derived from the opened stream's format in OpenStage (rate x stereo x
    // 2 bytes x ms — the same arithmetic as the ring capacity comment). The
    // T3 refill-window video-skip branch was removed with the demux/decode
    // split (video packets no longer delay the worker's audio decode, so
    // audio-first is structural), but the marks still document the ring's
    // intended depth and remain the reference for future pacing work.
    size_t audioLowWater = 0, audioHighWater = 0; // 200 / 600 ms in ring bytes

    // Lock-free diagnostics the SDL callback maintains (relaxed atomics
    // only — that thread must stay lock-light; ringM above is all it takes).
    std::atomic<uint64_t> underruns{0};     // silence-fill callbacks, cumulative
    std::atomic<bool> audioEof{false};      // no further audio will be produced:
                                            // demuxer EOF/read-error/decode-fail
                                            // park, or the refill dry cap (audio
                                            // stream ended before video) —
                                            // post-EOF silence is not starvation

    SDL_AudioDeviceID dev = 0;
    bool audioDeviceFailed = false;          // SDL open failed: silent playback (T3)

    // Wall-clock fallback (no audio) — UI-thread-only, no lock needed.
    std::chrono::steady_clock::time_point wallStart{};
    double wallAccum = 0, wallBase = 0;
    bool wallPlaying = true;

    ~PlayerCore() { Close(); }

    size_t RingUsedLocked() const { return (ringW + ringCap - ringR) % ringCap; }
    size_t RingFreeLocked() const { return ringCap - 1 - RingUsedLocked(); }

    static double WallSec(std::chrono::steady_clock::time_point tp) {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - tp).count();
    }

    double ClockNow() {
        if (useWallClock) {
            double t = wallBase + wallAccum + (wallPlaying ? WallSec(wallStart) : 0.0);
            if (duration > 0) t = std::min(t, duration);
            return t;
        }
        std::lock_guard<std::mutex> lk(ringM);
        if (!audioRate) return 0.0;
        double t = audioRef + (double)framesPlayed / audioRate;
        // framesPlayed counts samples *copied to* the device; they become
        // audible one device-buffer period later, so track the audible
        // position while playing. While paused the rebase already pins the
        // clock exactly on the seek target — subtracting here would make
        // paused frame-stepping drift one period per step.
        if (!paused.load(std::memory_order_relaxed)) t -= devLatency;
        // The audio stream can outlive the video by encoder tail padding
        // (AAC priming/padding); the clock must not display past the end.
        if (duration > 0) t = std::min(t, duration);
        return std::max(0.0, t);
    }

    struct Snap {
        bool opened = false, paused = false, ended = false;
        bool opening = false, openFailed = false; // async-open state (D1)
        double pos = 0, dur = 0;
        float vol = 0.8f;
        std::string error;                    // classified stop reason (T1)
        std::string seekError;                // one-shot failed-seek notice (T2)
        std::chrono::steady_clock::time_point seekErrorAt{};
        bool audioDeviceFailed = false;       // SDL open failed: silent playback (T3)
        double jogRingLo = -1;                // oldest retained jog-ring pts (-1 = empty)
    };

    Snap SnapNow() {
        std::lock_guard<std::mutex> lk(m);
        Snap s;
        s.opening = phase == Phase::Opening;
        s.openFailed = phase == Phase::Failed;
        s.opened = phase == Phase::Running && fmt != nullptr;
        s.paused = paused;
        s.ended = ended;
        // ClockNow reads OpenStage-written state (duration/useWallClock/...)
        // that is only safe after the m-ordered Running transition — during
        // Opening the worker is mid-write, so skip the clock entirely.
        if (s.opened) s.pos = ClockNow(); // lock order m -> ringM, consistent
        if (s.opened) s.dur = duration;   // same race as ClockNow (review MINOR-1)
        s.vol = volume.load(std::memory_order_relaxed);
        s.error = lastError;
        s.seekError = seekError;
        s.seekErrorAt = seekErrorAt;
        s.jogRingLo = jogRing.empty() ? -1.0 : jogRing.front().pts;
        // Written in OpenStage before the m-held Running transition, so
        // reading it under m is safe (same happens-before as duration).
        s.audioDeviceFailed = audioDeviceFailed;
        return s;
    }

    // True once OpenStage finished successfully (happens-before boundary for
    // the worker-written decode state the UI reads locklessly: videoW, fps...).
    bool IsRunning() {
        std::lock_guard<std::mutex> lk(m);
        return phase == Phase::Running;
    }

    void SetPaused(bool p) {
        {
            std::lock_guard<std::mutex> lk(m);
            if (paused == p) return;
            paused = p;
            if (useWallClock) {
                if (p) { wallAccum += WallSec(wallStart); wallPlaying = false; }
                else   { wallStart = std::chrono::steady_clock::now(); wallPlaying = true; }
            }
            cv.notify_all();
        }
        if (dev) SDL_PauseAudioDevice(dev, p ? 1 : 0);
    }

    void SetVolume(float v) { volume.store(std::clamp(v, 0.0f, 1.0f), std::memory_order_relaxed); }

    // User A/V offset, seconds. Positive = audio later relative to video
    // (PotPlayer-style "음성 지연"). Applied ONLY as a display-gate shift in
    // SyncVideoTexture — the clock/seek domain stays pure, so scrubbing and
    // frame stepping never accumulate the offset.
    std::atomic<float> avDelay{0.0f};
    void SetAvDelay(float v) {
        avDelay.store(std::clamp(v, -1.0f, 1.0f), std::memory_order_relaxed);
    }

    // Seek() = precision (decode forward to the target). SeekScrub() = jog-dial
    // scrub (keyframe-only). Both write the flag together with the target
    // under m, so a pending seek always carries the flag of the LAST request —
    // scrub-then-release coalesces into one precision seek (latest-wins).
    void Seek(double t) { SeekCommon(t, false); }
    void SeekScrub(double t) { SeekCommon(t, true); }

    void SeekCommon(double t, bool scrub) {
        if (duration > 0) t = std::clamp(t, 0.0, duration);
        std::lock_guard<std::mutex> lk(m);
        wantSeek = true;
        seekTarget = t;
        jogSeek = scrub;
        // The fallback scrub seek (ring start crossed) ALSO feeds the jog
        // display: JogFrame shows each decoded frame as decode creeps toward
        // the target, so the fallback is frame-smooth too, not a keyframe pop.
        if (scrub) jogTargetPts = t;
        // A fresh request supersedes a failed-seek notice, and the wall-clock
        // rebase below happens HERE (request time, UI thread) — so the
        // pre-seek values for the failed-seek restore must be captured now,
        // before they are overwritten.
        seekError.clear();
        // While a seek is in flight the snapshot already holds the last real
        // position; re-capturing here would record the intermediate park of
        // the seek being superseded (review MINOR-1).
        if (!seekInFlight) {
            undoWallBase = wallBase;
            undoWallAccum = wallAccum;
            undoWallStart = wallStart;
            undoWallPlaying = wallPlaying;
        }
        if (useWallClock) {
            if (wallPlaying) { wallAccum += WallSec(wallStart); }
            wallBase = t;
            wallAccum = 0;
            wallStart = std::chrono::steady_clock::now();
            wallPlaying = !paused;
        }
        cv.notify_all();
        // RingPush parks on cvRing (never on cv) whenever the ring is full —
        // which while PAUSED is permanent: the SDL device is stopped, nothing
        // drains, so the predicate's ring-space escape never opens. A seek
        // that only notified cv left the worker parked there forever (the
        // clock/video froze at the pre-seek position; T4 e2e symptom ③).
        // Notify cvRing under ringM — the same shape Close/TryClose use — so
        // the parked worker wakes and re-evaluates a predicate that now also
        // sees wantSeek. Lock order m -> ringM (documented), and holding
        // ringM closes the check-vs-park window on the waiter side.
        {
            std::lock_guard<std::mutex> lk2(ringM);
            cvRing.notify_all();
        }
        // Same reasoning for the video side: the decode thread's clock-gate
        // hold must re-see wantSeek (it drops the held pre-seek packet), and
        // a demuxer parked on a full vPktQ must escape to run the seek.
        // Lock order m -> vPktM (same shape as the ring notify above).
        {
            std::lock_guard<std::mutex> lk2(vPktM);
            vPktCv.notify_all();
        }
    }

    // Jog dial target update (frame-scrub): zero-blocking. The UI thread
    // calls this every frame the dial moves; the video thread's clock gate
    // re-evaluates against it on its 10 ms poll — no notify needed.
    void JogTo(double t) {
        if (duration > 0) t = std::clamp(t, 0.0, duration);
        std::lock_guard<std::mutex> lk(m);
        jogTargetPts = t;
    }

    // UI thread: the frame to display for the live jog target — the newest
    // decoded frame at or before jogTargetPts. Nothing is popped: dialing
    // back re-displays history straight from the ring (the ring holds every
    // decoded frame, videoQ included — videoQ pushes also ring-push).
    bool JogFrame(VideoFrame& out) {
        std::lock_guard<std::mutex> lk(m);
        if (jogTargetPts < 0) return false;
        // Half-frame tolerance: the dial target is continuous time, decoded
        // pts are quantized — pick the frame the target lands on.
        const double eps = fps > 0.0 ? 0.5 / fps : 0.005;
        for (auto it = jogRing.rbegin(); it != jogRing.rend(); ++it) {
            if (it->pts <= jogTargetPts + eps) {
                out = *it; // shared_ptr copy — the frame stays for re-display
                return true;
            }
        }
        return false;
    }

    // Jog mode: while scrubbing (usually paused) the audio ring never drains,
    // so RingPush would park the worker mid-GOP and stall video decode. Skip
    // audio decode entirely — packets are still read + unref'd, keeping the
    // demuxer position in sync for the next seek.
    void SetJog(bool j) {
        jogging.store(j, std::memory_order_relaxed);
        if (!j) {
            std::lock_guard<std::mutex> lk(m);
            jogTargetPts = -1; // stale gate input must not outlive the session
        }
    }

    // Three-stage seek (T2, spec D3). Runs on the worker; takes and releases
    // `lk` itself. The point of the split: avformat_seek_file is blocking disk
    // I/O, and it used to run under `m` — freezing the UI thread's SnapNow
    // (and the jog knob) for the whole seek. Now only the cheap transport
    // mutations hold locks; the I/O runs with `m` RELEASED.
    //
    //   (a) under m/ringM — flush queues + codecs, snapshot the pre-seek
    //       state, park the clock at the target (dropBeforePts / seekGen).
    //   (b) m RELEASED   — avformat_seek_file (the disk I/O). SnapNow,
    //       SetPaused and a newer SeekCommon all proceed meanwhile.
    //   (c) m re-acquired — three outcomes:
    //         stop or a newer seek request arrived  -> Superseded: discard
    //             everything this seek did (the newer request's stage (a)
    //             re-flushes and re-rebases; it owns the pipeline).
    //         avformat_seek_file < 0                -> Failed: restore the
    //             stage-(a) snapshot (the demuxer never moved, so the restored
    //             clock/ring/gates are consistent with it again), surface a
    //             one-shot notice, and keep playing from the pre-seek
    //             position. No decode-forward fallback — this task is failure
    //             correction, not long-range scan.
    //         otherwise                             -> Ok: demux continues from
    //             the new position.
    //
    // Scrub seeks are keyframe-only: dropBeforePts = -1 keeps every decoded
    // frame, so the landing keyframe displays immediately and decode creeps
    // toward the target (the pop gate converges as videoQ drains). NOPTS
    // frames are already rejected by pts < 0 in DecodeVideoPacket, so -1.0 is
    // a safe "drop nothing" sentinel.
    enum class SeekResult { Ok, Failed, Superseded };

    SeekResult DoSeekStages(std::unique_lock<std::mutex>& lk) {
        // ---- Stage (a): under m — flush, snapshot, park the clock. ---------
        double t = seekTarget;
        if (duration > 0) t = std::clamp(t, 0.0, duration);
        // Capture the undo baseline only when no seek is in flight (review
        // MINOR-1): a chained stage (a) after a superseded seek would
        // otherwise snapshot the parked (never-rendered) clock of the seek
        // it supersedes, and a later failure would "restore" to that
        // phantom instead of the real position.
        const bool captureUndo = !seekInFlight;
        if (captureUndo) undoEnded = ended;
        // With a negative user A/V offset the display gate sits at
        // clock + avDelay (< t), so the first pushable frame (t - 0.05)
        // would exceed the gate and nothing pops until the clock crawls
        // |avDelay| forward — a frozen picture after every seek, forever
        // while paused. Extend the stale cutoff to cover the shifted gate
        // (positive offsets need nothing: the gate is in the future).
        if (captureUndo) {
            undoDropBeforePts = dropBeforePts;
            undoPostSeekJump = postSeekJump;
        }
        dropBeforePts = jogSeek
                            ? -1.0
                            : t - 0.05 +
                                  std::min(0.0, (double)avDelay.load(std::memory_order_relaxed));
        postSeekJump = true;
        // Invalidate the video gate loop's in-hand packet (see vSeekSeq): it
        // was dequeued before this flush and is pre-seek, even though the
        // queue drain below cannot see it.
        vSeekSeq.fetch_add(1, std::memory_order_relaxed);
        videoQ.clear();
        // The jog ring is pre-seek history: drop it so the ring rebuilds
        // from the landing position (dropBeforePts guard keeps stale
        // frames out of both queues at push time).
        jogRing.clear();
        jogRingBytes = 0;
        // vctx is decoded on the video decode thread now (demux/decode
        // split): serialize the flush against its send/receive. The hold is
        // bounded by one packet's decode (~20 ms); DecodeVideoPacket releases
        // vdecM before it takes m, so no lock-order inversion. Any packet the
        // video thread already popped but not yet decoded is dropped by its
        // own gate loop (it re-checks wantSeek) or by dropBeforePts at push
        // time — both post-flush, so no stale frame can enter videoQ.
        {
            std::lock_guard<std::mutex> lkDec(vdecM);
            if (vctx) avcodec_flush_buffers(vctx);
        }
        if (actx) avcodec_flush_buffers(actx);
        // Drain queued video packets: they belong to the pre-seek demux
        // position. The worker is the only enqueuer and it is inside the
        // seek, so everything in the queue right now is pre-seek; the next
        // enqueue comes from the post-seek read loop.
        {
            std::lock_guard<std::mutex> lkV(vPktM);
            while (!vPktQ.empty()) {
                av_packet_unref(vPktQ.front());
                av_packet_free(&vPktQ.front());
                vPktQ.pop_front();
            }
            vPktQBytes = 0;
            vPktCv.notify_all();
        }
        const bool seekVideo = fmt && videoStream >= 0;
        // Audio-only files must seek the demuxer too — skipping it used to
        // leave the demuxer at the old position with the clock parked at the
        // target, a guaranteed A/V desync (T2).
        const bool seekAudio = fmt && !seekVideo && audioStream >= 0;
        AVRational tb{};
        if (seekVideo) tb = videoTb;
        if (seekAudio) tb = audioTb;
        {
            std::lock_guard<std::mutex> lk2(ringM);
            ++seekGen;
            if (captureUndo) {
                undoAudioRef = audioRef;
                undoFramesPlayed = framesPlayed;
                undoRingR = ringR;
                undoRingW = ringW;
            }
            seekInFlight = true;
            ringR = ringW = 0;
            // The ring stays empty for the whole seek gap; that silence is
            // the seek, not starvation — gate the underrun counter for the
            // duration (the Ok path clears it; the Failed path restores the
            // pre-seek undoEnded verdict).
            audioEof.store(true, std::memory_order_relaxed);
            // Rebase the audio clock so ClockNow() == t at zero consumed
            // samples. Post-seek audio resumes at file time t + ptsOrigin,
            // not at the stream's original start, hence the rebase (a plain
            // seed would go negative when audio starts after ptsOrigin).
            if (audioRate) {
                const double f = (t - audioLead) * audioRate;
                framesPlayed = f > 0 ? (uint64_t)f : 0;
                audioRef = t - (double)framesPlayed / audioRate;
            }
            cvRing.notify_all();
        }

        // ---- Stage (b): m RELEASED — the blocking disk I/O. ----------------
        int r = 0;
        if (seekVideo || seekAudio) {
            const int64_t ts =
                (int64_t)((t + ptsOrigin) / av_q2d(tb));
            lk.unlock();
            // AVSEEK_FLAG_BACKWARD: land on a keyframe <= target. With
            // flags=0 mkv/webm demuxers can park mid-GOP and every following
            // frame fails to decode (h264 "co located POCs unavailable",
            // dav1d "Error parsing frame header") — video never recovers.
            // Backward landing + the dropBeforePts gate = standard
            // decode-forward-to-target; mp4's keyframe table made flags=0
            // work by accident.
            r = avformat_seek_file(fmt, seekVideo ? videoStream : audioStream,
                                   INT64_MIN, ts, ts, AVSEEK_FLAG_BACKWARD);
            lk.lock();
        }

        // ---- Stage (c): m re-acquired — commit, restore, or discard. -------
        if (stop || wantSeek) {
            // Deliberately NOT clearing seekInFlight (review MINOR-1): the
            // superseding request's stage (a) must skip its capture so the
            // snapshot keeps describing the last real position across the
            // whole supersede cascade. The cascade ends in exactly one Ok
            // or Failed exit, and both clear the flag.
            return SeekResult::Superseded;
        }

        if (r < 0) {
            // Failed seek: put the pipeline back the way stage (a) found it.
            // ringR/ringW are exact (the ring was empty the whole time — the
            // only producer is this worker, and it was inside the seek).
            double restoredClock = 0;
            {
                std::lock_guard<std::mutex> lk2(ringM);
                audioRef = undoAudioRef;
                framesPlayed = undoFramesPlayed;
                ringR = undoRingR;
                ringW = undoRingW;
                restoredClock = audioRef +
                    (audioRate ? (double)framesPlayed / audioRate : 0.0);
                cvRing.notify_all();
            }
            dropBeforePts = undoDropBeforePts;
            postSeekJump = undoPostSeekJump;
            ended = undoEnded;
            // Restore the callback's underrun-counting gate with it: the
            // pre-seek "no more audio" verdict is the restored truth.
            audioEof.store(undoEnded, std::memory_order_relaxed);
            if (useWallClock) {
                // Fold the snapshot into a position, then RE-PIN it against
                // the current paused state (review MINOR-2): a SetPaused
                // during the I/O window mutated wallAccum/wallPlaying under
                // m, so writing the old quadruple back verbatim could leave
                // ClockNow() advancing while paused (stale
                // undoWallPlaying=true + old wallStart).
                const double pos = undoWallBase + undoWallAccum +
                    (undoWallPlaying ? WallSec(undoWallStart) : 0.0);
                wallBase = 0;
                wallAccum = pos;
                wallStart = std::chrono::steady_clock::now();
                wallPlaying = !paused.load(std::memory_order_relaxed);
                restoredClock = pos;
            }
            seekError = "시크 불가 위치: " + AvErr(r);
            seekErrorAt = std::chrono::steady_clock::now();
            // Not touching lastError (review MINOR-4): it only displays on
            // `ended`, so a failed seek must not masquerade as the stop
            // reason of a later natural EOF — the 3 s seekError notice is
            // the whole failed-seek UI.
            seekInFlight = false;
            // One-shot diagnostic (same convention as the audio-device open
            // failure): a failed seek is invisible otherwise — the pipeline
            // keeps running from the restored position.
            std::fprintf(stderr,
                         "[vplayer] seek failed r=%d (%s); clock restored to "
                         "%.3fs\n", r, AvErr(r).c_str(), restoredClock);
            std::fflush(stderr);
            cv.notify_all();
            return SeekResult::Failed;
        }

        ended = false;
        audioEof.store(false, std::memory_order_relaxed); // audio flows again
        // Arm the stale-audio gate (see audioSkipBelow): everything below t
        // is demuxer landing rewind, not playable content. Only the Ok path
        // sets it — a failed seek restores the pre-seek demux position, whose
        // audio is exactly what the gate would (wrongly) drop.
        audioSkipBelow = t;
        lastError.clear(); // a successful seek supersedes a stale stop reason
        seekError.clear();
        seekInFlight = false;
        return SeekResult::Ok;
    }

    // UI thread, returns immediately. Arms the interrupt callback on a
    // pre-allocated AVFormatContext (so even avformat_open_input is bounded)
    // and hands the actual demux/codec work to the existing worker thread.
    void BeginOpen(std::string path) {
        openPath_ = std::move(path);
        openDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        fmt = avformat_alloc_context();
        if (fmt) {
            fmt->interrupt_callback.callback = &PlayerCore::InterruptCb;
            fmt->interrupt_callback.opaque = this;
        }
        worker = std::thread([this] { WorkerLoop(); });
    }

    void CancelOpen() { cancelOpen.store(true, std::memory_order_relaxed); }

    // FFmpeg calls this on the worker thread inside open/find_stream_info/seek.
    // Returning 1 aborts the blocking call with AVERROR_EXIT.
    static int InterruptCb(void* ud) {
        auto* p = static_cast<PlayerCore*>(ud);
        if (p->cancelOpen.load(std::memory_order_relaxed)) return 1;
        // The 10 s deadline guards the OPEN phase only — it is disarmed
        // (same thread) before the first playback read, so seeking/reading
        // on a slow file is never aborted by it.
        if (p->openDeadlineActive.load(std::memory_order_relaxed) &&
            std::chrono::steady_clock::now() > p->openDeadline)
            return 1;
        return 0;
    }

    // Worker thread, first stage (spec D1): the demux/codec bring-up that used
    // to block the UI thread. Every failure lands in phase=Failed with a
    // human-readable lastError; the interrupt callback bounds it to 10 s or
    // a user cancel.
    bool OpenStage() {
        auto fail = [&](const std::string& msg) {
            std::lock_guard<std::mutex> lk(m);
            lastError = msg;
            phase = Phase::Failed;
            ended = true; // park the worker loop; UI adopts lastError
            cv.notify_all();
        };
        // Cheap existence gate (T1 review MINOR-2 carry): used to run on the
        // UI thread in OpenPath, where a dead UNC path blocked the UI in
        // fopen for the network timeout. Same check, same classification —
        // now on the worker, where I/O belongs.
        {
            std::FILE* f = std::fopen(openPath_.c_str(), "rb");
            if (!f) { fail("파일을 찾을 수 없습니다"); return false; }
            std::fclose(f);
        }
        int r = avformat_open_input(&fmt, openPath_.c_str(), nullptr, nullptr);
        if (r < 0) {
            if (cancelOpen.load(std::memory_order_relaxed))
                fail("열기가 취소되었습니다");
            else if (openDeadlineActive.load(std::memory_order_relaxed) &&
                     std::chrono::steady_clock::now() > openDeadline)
                fail("열기 시간 초과");
            else
                fail("open: " + AvErr(r));
            return false;
        }
        if (avformat_find_stream_info(fmt, nullptr) < 0) {
            if (cancelOpen.load(std::memory_order_relaxed))
                fail("열기가 취소되었습니다");
            else if (openDeadlineActive.load(std::memory_order_relaxed) &&
                     std::chrono::steady_clock::now() > openDeadline)
                fail("열기 시간 초과");
            else
                fail("스트림 정보를 읽을 수 없는 파일입니다");
            return false;
        }
        // Open phase survived: disarm the deadline before playback I/O.
        openDeadlineActive.store(false, std::memory_order_relaxed);

        if (fmt->duration > 0) duration = fmt->duration / (double)AV_TIME_BASE;

        const AVCodec* vdec = nullptr;
        const AVCodec* adec = nullptr;
        videoStream = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, &vdec, 0);
        if (videoStream >= 0) {
            AVStream* st = fmt->streams[videoStream];
            // Dimension caps BEFORE the decoder opens (spec 1a-2): a lying
            // header must not reach vf.pix.resize — a worker bad_alloc would
            // std::terminate the whole desktop process.
            const int64_t w = st->codecpar->width;
            const int64_t h = st->codecpar->height;
            // w/h <= 0: a lying header would slip the maxBytes check on the
            // RGBA branch (negative int64 passes the cap) — reject up front.
            if (w <= 0 || h <= 0) {
                fail("지원하지 않는 비디오 차원");
                return false;
            }
            nv12Out = (w % 2 == 0) && (h % 2 == 0);
            const int64_t maxBytes = nv12Out ? (int64_t)bytesFor(w, h)
                                             : (int64_t)w * h * 4;
            if (w > 8192 || h > 8192 || maxBytes > (int64_t)256 * 1024 * 1024) {
                fail("지원하지 않는 해상도 (최대 8192x8192, 프레임 256MiB)");
                return false;
            }
            videoTb = st->time_base;
            vctx = avcodec_alloc_context3(vdec);
            avcodec_parameters_to_context(vctx, st->codecpar);
            // The AVCodecContext default is thread_count=1 — measured on the
            // user's 4K50 AV1 file that is 43 ms/frame (~23 fps). Frame
            // threading is what makes dav1d usable here (threads=12 decoded
            // at 98 fps in the standalone probe). Cap at 12: the frame-thread
            // pool holds ~threads decoded 4K surfaces, so unbounded auto
            // threading on a 24+ core machine would pin hundreds of MB, and
            // the pipeline depth delays the clock gate (vPipeDelay).
            vctx->thread_count =
                (int)std::clamp(std::thread::hardware_concurrency() / 2, 1u, 12u);
            if (avcodec_open2(vctx, vdec, nullptr) < 0) {
                avcodec_free_context(&vctx);
                videoStream = -1;
            } else {
                videoW = st->codecpar->width;
                videoH = st->codecpar->height;
                // Frame rate for ±1F stepping. r_frame_rate is a sane fallback
                // but some containers carry fake values (1000 fps tbn hacks),
                // so clamp hard.
                AVRational fr = st->avg_frame_rate;
                if (fr.num <= 0 || fr.den <= 0) fr = st->r_frame_rate;
                if (fr.num > 0 && fr.den > 0) {
                    fps = std::clamp((double)fr.num / (double)fr.den, 1.0, 240.0);
                    // Decode-thread clock-gate seed (see vPipeDelay).
                    vDecodeDelay = std::max(0, vctx->has_b_frames) / fps;
                    vPipeDelay = std::max(0.10, vDecodeDelay);
                    // Clump-lead widening (see clumpLead): fps was clamped
                    // [1,240] above, so this is always finite and positive.
                    clumpLead = kClumpFrames / fps;
                }
            }
        }
        audioStream = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, &adec, 0);
        if (audioStream >= 0) {
            AVStream* st = fmt->streams[audioStream];
            audioTb = st->time_base; // seek timebase for audio-only files (T2)
            actx = avcodec_alloc_context3(adec);
            avcodec_parameters_to_context(actx, st->codecpar);
            if (avcodec_open2(actx, adec, nullptr) < 0) {
                avcodec_free_context(&actx);
                audioStream = -1;
            }
        }
        if (videoStream < 0 && audioStream < 0) {
            fail("재생 가능한 오디오/비디오 스트림이 없습니다");
            return false;
        }

        if (videoStream >= 0) {
            sws = sws_getContext(videoW, videoH, vctx->pix_fmt,
                                 videoW, videoH,
                                 nv12Out ? AV_PIX_FMT_NV12 : AV_PIX_FMT_RGBA,
                                 SWS_BILINEAR, nullptr, nullptr, nullptr);
            if (!sws) { fail("sws_getContext failed"); return false; }
        }

        if (audioStream >= 0) {
            audioRate = actx->sample_rate;
            AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
            if (swr_alloc_set_opts2(&swr, &outLayout, AV_SAMPLE_FMT_S16, audioRate,
                                    &actx->ch_layout, actx->sample_fmt, actx->sample_rate,
                                    0, nullptr) < 0 || swr_init(swr) < 0) {
                swr_free(&swr); swr = nullptr;
                avcodec_free_context(&actx);
                audioStream = -1;
            }
        }
        if (audioStream >= 0) {
            // The app framework only inits SDL video; the audio subsystem is
            // per-process and SDL_OpenAudioDevice fails without it.
            if (!SDL_WasInit(SDL_INIT_AUDIO)) SDL_InitSubSystem(SDL_INIT_AUDIO);
            ringCap = 1u << 20; // 1 MiB of S16 stereo (~5.8 s at 44.1 kHz)
            ring = (uint8_t*)av_mallocz(ringCap);
            // T3 refill water marks in ring bytes, from the opened format
            // (rate x stereo x 2 bytes x ms — the capacity arithmetic above):
            // refill the ring when under 200 ms, decode video again at 600 ms.
            const size_t bytesPerMs =
                (size_t)audioRate * kAudioCh * sizeof(int16_t) / 1000;
            audioLowWater = bytesPerMs * 200;
            audioHighWater = std::min(bytesPerMs * 600, ringCap - 1);
            SDL_AudioSpec want{};
            want.freq = audioRate;
            want.format = AUDIO_S16SYS;
            want.channels = kAudioCh;
            want.samples = 2048;
            want.callback = &PlayerCore::AudioCallbackC;
            want.userdata = this;
            SDL_AudioSpec got{};
            dev = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);
            if (!dev || !ring) {
                // One-shot diagnostic: silent-video fallbacks are hard to
                // distinguish from device problems otherwise.
                std::fprintf(stderr, "[vplayer] audio device open failed: %s\n",
                             SDL_GetError());
                // Surface it in the status text too (T3, spec 1b-3): silent
                // playback must say why. Informational only — the demux/
                // decode pipeline is unaffected, so this deliberately does
                // NOT go through lastError (the T1 classification channel).
                audioDeviceFailed = true;
                if (dev) { SDL_CloseAudioDevice(dev); dev = 0; }
                if (ring) { av_free(ring); ring = nullptr; }
                swr_free(&swr); swr = nullptr;
                avcodec_free_context(&actx);
                audioStream = -1;
            } else {
                devLatency = (double)got.samples / (double)audioRate;
            }
        }

        useWallClock = (audioStream < 0);
        // Presentation-time origin + audio clock rebase (see member docs).
        if (fmt->start_time != AV_NOPTS_VALUE)
            ptsOrigin = fmt->start_time / (double)AV_TIME_BASE;
        if (audioStream >= 0 &&
            fmt->streams[audioStream]->start_time != AV_NOPTS_VALUE)
            audioLead = fmt->streams[audioStream]->start_time *
                            av_q2d(fmt->streams[audioStream]->time_base) -
                        ptsOrigin;
        audioRef = audioLead;
        wallStart = std::chrono::steady_clock::now();
        wallAccum = 0; wallBase = 0; wallPlaying = true;

        if (dev) SDL_PauseAudioDevice(dev, 0); // start unpaused; underruns are silence
        {
            std::lock_guard<std::mutex> lk(m);
            phase = Phase::Running; // publishes ALL OpenStage state to the UI
        }
        cv.notify_all();
        return true;
    }

    void Close() {
        // Abort a hung open first: the interrupt callback (worker thread) sees
        // this without taking any lock and unwinds the blocking FFmpeg call.
        // No early return on a second call: TryClose already set stop, and the
        // dtor must still free the FFmpeg/SDL state (all idempotent below).
        cancelOpen.store(true, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lk(m);
            stop = true;
        }
        cv.notify_all();
        { std::lock_guard<std::mutex> lk(ringM); cvRing.notify_all(); }
        { std::lock_guard<std::mutex> lk(vPktM); vPktCv.notify_all(); }
        if (worker.joinable()) worker.join();
        if (dev) { SDL_CloseAudioDevice(dev); dev = 0; }
        if (sws) { sws_freeContext(sws); sws = nullptr; }
        if (swr) { swr_free(&swr); swr = nullptr; }
        if (actx) avcodec_free_context(&actx);
        if (vctx) avcodec_free_context(&vctx);
        if (fmt) avformat_close_input(&fmt);
        if (ring) { av_free(ring); ring = nullptr; }
        videoQ.clear();
    }

    // Bounded shutdown for the UI thread. Returns true once the worker has
    // exited (caller may destroy the core). False means the worker is stuck
    // inside a never-returning FFmpeg call (silent named pipe, dead network
    // mount): the interrupt callback is only consulted BETWEEN FFmpeg
    // operations, so it cannot unwind a blocked synchronous read (measured,
    // task-1 report 4c) — the caller must Abandon() instead of joining, or
    // the UI thread would freeze forever.
    bool TryClose(int timeoutMs) {
        cancelOpen.store(true, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lk(m);
            stop = true;
        }
        cv.notify_all();
        { std::lock_guard<std::mutex> lk(ringM); cvRing.notify_all(); }
        { std::lock_guard<std::mutex> lk(vPktM); vPktCv.notify_all(); }
        bool done;
        {
            std::unique_lock<std::mutex> lk(m);
            done = cv.wait_for(lk, std::chrono::milliseconds(timeoutMs),
                               [&] { return workerDone; });
        }
        if (done && worker.joinable()) worker.join();
        return done;
    }

    // Give up on a stuck worker: detach the thread and let the caller leak
    // this core (bounded — one per abandoned open). The worker eventually
    // finishes into memory that is never freed and exits; its FFmpeg/SDL
    // state stays alive with it. Deleting here would be a use-after-free.
    void Abandon() {
        if (worker.joinable()) worker.detach();
    }

    // SDL audio thread: pull S16 bytes from the ring, apply volume, advance
    // the master clock. Never blocks (underrun = silence) so the callback is
    // safe to run while the worker holds locks.
    static void AudioCallbackC(void* ud, uint8_t* stream, int len) {
        auto* p = static_cast<PlayerCore*>(ud);
        std::lock_guard<std::mutex> lk(p->ringM);
        const size_t avail = p->RingUsedLocked();
        const size_t n = std::min<size_t>((size_t)len, avail);
        const size_t first = std::min(n, p->ringCap - p->ringR);
        std::memcpy(stream, p->ring + p->ringR, first);
        if (n > first) std::memcpy(stream + first, p->ring, n - first);
        p->ringR = (p->ringR + n) % p->ringCap;
        // Wake a producer parked on a full ring (RingPush's cvRing.wait):
        // a condition_variable's predicate is only re-checked on notify, so
        // a consumer that pops without notifying leaves the worker asleep
        // forever once the ring fills. The baseline never hit the wait
        // (video decode paced the loop); the T3 audio-first refill can.
        // Notify with no waiter is a no-op, so the callback stays lock-light.
        p->cvRing.notify_all();
        if (n < (size_t)len) {
            // Underrun (T3, spec 1b-2): the ring ran dry mid-fill — silence
            // the tail and count it. Lock-free relaxed atomics only: this
            // thread must stay lock-light (ringM above is all it takes).
            // Post-EOF silence is not starvation — the worker raises
            // audioEof when no further audio will be produced, and the
            // count stops there.
            if (!p->audioEof.load(std::memory_order_relaxed))
                p->underruns.fetch_add(1, std::memory_order_relaxed);
            std::memset(stream + n, 0, len - n);
        }
        const float v = p->volume.load(std::memory_order_relaxed);
        if (v < 0.999f) {
            auto* s = reinterpret_cast<int16_t*>(stream);
            for (int i = 0; i < len / 2; ++i) s[i] = (int16_t)(s[i] * v);
        }
        // The master clock advances with consumed TIME, not copied bytes:
        // an underrun's silence IS the presentation position — counting only
        // copied bytes permanently lagged the clock by every silent gap, and
        // the lag accumulated across underruns (A/V desync). The seek-gap /
        // end-of-audio gate (audioEof) keeps the old pinned behaviour: seek
        // silence must not run the clock past its target, and a parked
        // worker must not advance the clock into content that was never
        // decoded.
        const bool starving =
            n < (size_t)len && !p->audioEof.load(std::memory_order_relaxed);
        p->framesPlayed += (starving ? (size_t)len : n) /
                           (kAudioCh * sizeof(int16_t));
    }

    void WorkerLoop() {
        AVPacket* pkt = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        try {
            // Stage 1 (D1): demux/codec bring-up on the worker, not the UI.
            const bool opened = (pkt && frame) ? OpenStage() : false;
            if (!opened && phase != Phase::Failed) {
                // pkt/frame allocation died before any classification.
                std::lock_guard<std::mutex> lk(m);
                lastError = "메모리 부족 (재생 초기화 실패)";
                phase = Phase::Failed;
                ended = true;
            }
            if (opened) {
                // Demux/decode split: video packets flow through vPktQ to a
                // dedicated decode thread gated by the presentation clock.
                // Started after OpenStage published videoStream/vctx (the
                // happens-before boundary is Running's m transition below,
                // but the worker itself only touches these after open too).
                if (videoStream >= 0)
                    vthread = std::thread([this] { VideoLoop(); });
                while (true) {
                    {
                        std::unique_lock<std::mutex> lk(m);
                        cv.wait(lk, [&] { return stop || wantSeek || !ended; });
                        if (stop) break;
                        if (wantSeek) {
                            // Consume this request BEFORE the unlocked I/O so
                            // a wantSeek seen in stage (c) can only mean a
                            // newer request (which then owns the pipeline).
                            wantSeek = false;
                            // Stage (a) under lk; stage (b) releases lk for
                            // the avformat_seek_file I/O; stage (c) re-locks
                            // (see DoSeekStages).
                            const SeekResult sr = DoSeekStages(lk);
                            if (sr == SeekResult::Superseded)
                                continue; // newest target re-seeks next pass
                            // Ok: demux below continues from the new position.
                            // Failed: clock restored — demux below continues
                            // from the (unmoved) pre-seek position. Either
                            // way the stage-(a) flush emptied the ring, so
                            // the next pass re-arms audio decode, and a
                            // pre-seek "audio produces nothing" verdict is
                            // stale.
                        }
                    }
                    const int r = av_read_frame(fmt, pkt);
                    if (r == AVERROR(EAGAIN)) {
                        // Transient resource shortage: brief sleep + bounded
                        // retry so a chatty demuxer can't spin the CPU forever.
                        if (++eagainStreak > 50) {
                            std::lock_guard<std::mutex> lk(m);
                            lastError = "읽기 지연 (EAGAIN 반복)";
                            ended = true;
                            audioEof.store(true, std::memory_order_relaxed);
                            eagainStreak = 0;
                            cv.notify_all();
                            continue;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        continue;
                    }
                    eagainStreak = 0;
                    if (r < 0) {
                        std::lock_guard<std::mutex> lk(m);
                        // EOF parks quietly (normal end); anything else records
                        // WHY playback stopped — the UI shows it next to Replay.
                        if (r != AVERROR_EOF)
                            lastError = "읽기 오류: " + AvErr(r);
                        ended = true; // park; Seek()/stop wake us again
                        // Parked = no further audio packets will be pushed
                        // (T3): the callback's silence fills after the ring
                        // drains are end-of-playback, not starvation.
                        audioEof.store(true, std::memory_order_relaxed);
                        cv.notify_all();
                        continue;
                    }
                    // Video packets are only enqueued (demux/decode split —
                    // see the vPktQ member docs): enqueueing is a memcpy of
                    // ~100 KB, so the demuxer reaches interleaved audio
                    // packets immediately and the T3 video-skip branch is
                    // unnecessary — audio-first is now structural, and no
                    // video packet is ever dropped mid-GOP.
                    if (pkt->stream_index == videoStream) {
                        EnqueueVideoPacket(pkt);
                    } else if (pkt->stream_index == audioStream && actx &&
                               !jogging.load(std::memory_order_relaxed)) {
                        // Stale-audio gate (audioSkipBelow): audio below the
                        // seek target is landing rewind — decoding it would
                        // park the demuxer on a full ring below the video
                        // clock gate (frozen picture after every
                        // keyframe-clamped seek) and desync A/V. Drop until
                        // the demuxer reaches the target; NOPTS pts cannot be
                        // classified, so they decode (a missed drop is the
                        // pre-fix behaviour, a wrong drop loses audio).
                        const double aPts =
                            pkt->pts == AV_NOPTS_VALUE
                                ? -1.0
                                : pkt->pts * av_q2d(audioTb) - ptsOrigin;
                        if (audioSkipBelow >= 0 && aPts >= 0 &&
                            aPts < audioSkipBelow) {
                            // stale: keep gating — the demuxer must reach t
                        } else {
                            audioSkipBelow = -1; // audio caught up: normal decode
                            if (DecodeAudioPacket(pkt, frame))
                                audioEof.store(false, std::memory_order_relaxed);
                        }
                    }
                    av_packet_unref(pkt);
                }
            }
        } catch (...) {
            // Last-resort barrier (spec 1a-3): a worker exception — bad_alloc
            // on a giant frame, STL misuse — must end playback, not terminate
            // the desktop process. Error codes stay the normal path.
            std::lock_guard<std::mutex> lk(m);
            lastError = "재생 중 내부 오류가 발생했습니다";
            ended = true;
            audioEof.store(true, std::memory_order_relaxed); // parked (T3)
            // An exception mid-seek (between stage (a) and stage (c)) must
            // not leave the flag latched: a stuck seekInFlight would make a
            // later successful open's first seek skip its undo capture and a
            // later failed seek restore a phantom snapshot.
            seekInFlight = false;
            if (phase == Phase::Opening) phase = Phase::Failed;
        }
        // The video decode thread parks on vPktCv once the queue drains; it
        // only exits on stop (Close/TryClose notify vPktCv with the other
        // shutdown gates). Join before freeing the worker's own AVPacket.
        if (vthread.joinable()) vthread.join();
        av_packet_free(&pkt);
        av_frame_free(&frame);
        // Open failed (or exception): park until Close() stops us — the UI
        // keeps the core alive to display lastError.
        {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [&] { return stop; });
            workerDone = true; // TryClose's shutdown predicate
        }
        cv.notify_all();
    }

    // Consecutive decode failures across streams. One success clears the
    // streak; 30 in a row stop playback with a classified reason instead of
    // spinning silently on garbage (spec 1a-4). decodeFailStreak itself is
    // worker-only, so the counter bump needs no lock.
    void DecodeFail(const char* what) {
        if (++decodeFailStreak < 30) return;
        std::lock_guard<std::mutex> lk(m);
        lastError = std::string(what) + " 디코딩 오류 지속";
        decodeFailStreak = 0;
        ended = true;
        audioEof.store(true, std::memory_order_relaxed); // parked (T3)
        cv.notify_all();
    }

    // m held. Retain one decoded frame in the jog ring and trim the caps
    // (time from the newest pts, bytes). The ring has NO backpressure: the
    // trim IS the bound, so the caller never parks on it.
    void JogRingPushLocked(VideoFrame vf) {
        const size_t fb = FrameBytes(vf);
        jogRing.push_back(std::move(vf));
        jogRingBytes += fb;
        while (!jogRing.empty() &&
               (jogRing.back().pts - jogRing.front().pts > kJogRingMaxSecs ||
                jogRingBytes > kJogRingMaxBytes)) {
            jogRingBytes -= FrameBytes(jogRing.front());
            jogRing.pop_front();
        }
    }

    // Returns false when decoding should stop (seek/stop requested).
    // Runs on the video decode thread (VideoLoop). Decode + convert one
    // packet, then push the frames under m. vdecM covers only the codec
    // calls — released before any m acquisition so DoSeekStages' stage-(a)
    // flush (m -> vdecM) can never invert against us.
    bool DecodeVideoPacket(AVPacket* pkt, AVFrame* frame) {
        std::deque<VideoFrame> ready;
        // Pipeline-delay sample for the clock gate (vPipeDelay): the sent
        // packet's pts minus the received frame's pts. Only valid when both
        // pts are known; the EMA tracks the decoder's steady-state depth.
        const bool haveSentPts =
            pkt->pts != AV_NOPTS_VALUE && videoTb.den > 0 && videoTb.num > 0;
        const double sentPts = haveSentPts
                                   ? pkt->pts * av_q2d(videoTb) - ptsOrigin
                                   : 0.0;
        {
            std::lock_guard<std::mutex> lkDec(vdecM);
            if (avcodec_send_packet(vctx, pkt) < 0) { DecodeFail("비디오"); return true; }
            while (true) {
                if (avcodec_receive_frame(vctx, frame) < 0) break;
                decodeFailStreak = 0; // a decoded frame resets the failure streak
                const double pts = frame->pts == AV_NOPTS_VALUE
                                       ? -1.0
                                       : frame->pts * av_q2d(videoTb) - ptsOrigin;
                if (pts < 0) { av_frame_unref(frame); continue; }
                if (haveSentPts) {
                    const double lag = std::clamp(sentPts - pts, 0.0, 0.5);
                    vPipeDelay += 0.25 * (lag - vPipeDelay);
                }
                VideoFrame vf;
                vf.pts = pts;
                vf.w = videoW;
                vf.h = videoH;
                // Pooled buffer: reuse a released allocation, else allocate
                // (bounded by videoQ + pipeline depth at steady state; the
                // pool itself is capped so a burst cannot grow it forever).
                std::shared_ptr<std::vector<uint8_t>> buf;
                for (auto& slot : vPool) {
                    if (slot.use_count() == 1) { buf = slot; break; }
                }
                if (!buf) {
                    buf = std::make_shared<std::vector<uint8_t>>();
                    if (vPool.size() < 10) vPool.push_back(buf);
                }
                vf.nv12 = nv12Out;
                vf.pix = buf;
                if (vf.pix->size() != FrameBytes(vf))
                    vf.pix->resize(FrameBytes(vf));
                uint8_t* dst[4] = { vf.pix->data(), nullptr, nullptr, nullptr };
                int dstStride[4] = { videoW * 4, 0, 0, 0 };
                if (nv12Out) {
                    dst[1] = vf.pix->data() + (size_t)videoW * videoH;
                    dstStride[0] = videoW;
                    dstStride[1] = videoW;
                }
                sws_scale(sws, frame->data, frame->linesize, 0, frame->height, dst, dstStride);
                av_frame_unref(frame);
                ready.push_back(std::move(vf));
            }
        } // vdecM released — the seek flush can proceed while we push
        for (VideoFrame& vf : ready) {
            std::unique_lock<std::mutex> lk(m);
            if (stop || wantSeek) return false;
            if (vf.pts < dropBeforePts) continue; // stale frame from before the seek
            // Jog ring retention (frame-scrub history): always, before any
            // videoQ decision. During a frame-scrub jog the videoQ push
            // below is SKIPPED: nothing pops while paused, so the videoQ cap
            // would park this thread and stall the dial — the ring is the
            // jog path's sink. The skip is gated on jogTargetPts: the legacy
            // keyframe jog (SetJog without JogTo) keeps the videoQ push so
            // the picture keeps updating; the gate arms when JogTo starts
            // driving the target.
            const bool jogFrameMode = jogging.load(std::memory_order_relaxed) &&
                                      jogTargetPts >= 0;
            if (jogFrameMode) {
                JogRingPushLocked(std::move(vf));
                continue;
            }
            JogRingPushLocked(vf); // copy — vf still moves to videoQ below
            // Backpressure: hold at most kVideoQMaxFrames pending frames
            // (~200 ms at 30 fps / ~120 ms at 50 fps — see kVideoQMaxFrames).
            // Unlike the pre-split worker loop, a park here blocks ONLY video
            // decode — the demuxer keeps feeding the audio ring independently,
            // so no RefillDue() escape is needed. Timed wait (review
            // Important-1) stays: while paused nothing pops, so NOTHING notifies
            // this wait; the 50 ms escape re-checks the predicate and drops the
            // frame on timeout (the display gate drops late frames the same way,
            // and the clock gate upstream stops decode from running ahead of a
            // frozen clock by more than kVideoLead + vDecodeDelay anyway).
            const bool ok = cv.wait_for(lk, std::chrono::milliseconds(50), [&] {
                return stop || wantSeek || videoQ.size() < kVideoQMaxFrames;
            });
            if (stop || wantSeek) return false;
            if (!ok) continue; // queue still full at timeout: drop
            videoQ.push_back(std::move(vf));
        }
        return true;
    }

    // Worker thread: move one video packet into the decode queue. Bounded by
    // kVPktQMaxBytes — parking here backpressures the demuxer itself (which
    // is the point: the audio ring's backpressure only exists for A/V files).
    void EnqueueVideoPacket(AVPacket* pkt) {
        AVPacket* copy = av_packet_alloc();
        if (!copy) return; // OOM: drop one packet; DecodeFail catches streaks
        if (av_packet_ref(copy, pkt) < 0) { av_packet_free(&copy); return; }
        {
            std::unique_lock<std::mutex> lk(vPktM);
            // wantSeek in the predicate: a seek requested while the demuxer
            // is parked here must reach the loop top (SeekCommon notifies
            // vPktCv under vPktM).
            vPktCv.wait(lk, [&] {
                return stop || wantSeek || vPktQBytes < kVPktQMaxBytes;
            });
            if (stop || wantSeek) { av_packet_free(&copy); return; }
            vPktQBytes += (size_t)copy->size + sizeof(AVPacket);
            vPktQ.push_back(copy);
        }
        vPktCv.notify_all(); // hand the packet to the decode thread
    }

    // Video decode thread (demux/decode split). Drains vPktQ in order,
    // holding each packet until it is near-due on the presentation clock
    // (kVideoLead + the decoder's frame-threading latency + clumpLead — the
    // B-frame clump width, see kVideoLead). Packet order is never broken, so
    // the reference chain stays intact; the clock gate is what bounds
    // decode-ahead.
    void VideoLoop() {
        AVPacket* pkt = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        if (pkt && frame) {
            while (true) {
                AVPacket* mine = nullptr;
                uint64_t mySeq = 0;
                {
                    std::unique_lock<std::mutex> lk(vPktM);
                    vPktCv.wait(lk, [&] { return stop || !vPktQ.empty(); });
                    if (stop) break;
                    // Capture the seek generation BEFORE dequeuing: a packet
                    // taken from the queue before stage (a)'s drain must
                    // abort in the gate loop below even if the seek's
                    // wantSeek window was missed (see vSeekSeq). A stale
                    // read here can only cause one harmless dropped packet.
                    mySeq = vSeekSeq.load(std::memory_order_relaxed);
                    mine = vPktQ.front();
                    vPktQ.pop_front();
                    vPktQBytes -= (size_t)mine->size + sizeof(AVPacket);
                }
                vPktCv.notify_all(); // a freed slot may unpark the demuxer
                // Clock gate: hold the packet until it is near-due. A seek
                // aborts the hold — the held packet is pre-seek (stage (a)
                // flushed the queue and the codec behind us), so it is
                // dropped, not decoded. The abort keys on vSeekSeq, not just
                // wantSeek: a seek that ran while we slept 10 ms here is
                // invisible to a plain wantSeek re-check (see vSeekSeq).
                bool seekAbort = false;
                while (!stop) {
                    double gateClock;
                    {
                        std::lock_guard<std::mutex> lk(m);
                        if (wantSeek ||
                            vSeekSeq.load(std::memory_order_relaxed) != mySeq) {
                            seekAbort = true;
                            break;
                        }
                        // Jog frame-scrub: decode runs to the DIAL target, not
                        // the (pinned) clock — max() keeps non-jog playback
                        // identical (jogTargetPts is -1 outside a session).
                        gateClock = std::max(ClockNow(), jogTargetPts);
                    }
                    const double pts = mine->pts == AV_NOPTS_VALUE
                                           ? -1.0
                                           : mine->pts * av_q2d(videoTb) - ptsOrigin;
                    if (pts < 0 ||
                        pts <= gateClock + kVideoLead + vPipeDelay + clumpLead)
                        break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                if (!stop && !seekAbort) {
                    // A false return means stop/seek fired mid-decode: on
                    // stop the outer wait's predicate ends this loop; on
                    // seek the queue is empty (stage (a) flushed it) and the
                    // thread parks until the post-seek packets arrive. The
                    // loop itself must NEVER exit on wantSeek — that would
                    // permanently kill video after the first seek.
                    DecodeVideoPacket(mine, frame);
                }
                av_packet_unref(mine);
                av_packet_free(&mine);
            }
        }
        if (frame) av_frame_free(&frame);
        if (pkt) av_packet_free(&pkt);
    }

    // Returns whether any converted audio reached the ring — the refill
    // window's productivity signal (T3): a window over a stream that
    // decodes to nothing must be abandoned, not held open forever.
    bool DecodeAudioPacket(AVPacket* pkt, AVFrame* frame) {
        if (avcodec_send_packet(actx, pkt) < 0) { DecodeFail("오디오"); return false; }
        uint64_t gen;
        { std::lock_guard<std::mutex> lk(ringM); gen = seekGen; }
        bool pushedAny = false;
        while (avcodec_receive_frame(actx, frame) == 0) {
            decodeFailStreak = 0; // a decoded frame resets the failure streak
            const int maxOut = swr_get_out_samples(swr, frame->nb_samples);
            if (maxOut <= 0) { av_frame_unref(frame); continue; }
            uint8_t* out = (uint8_t*)av_malloc((size_t)maxOut * kAudioCh * 2);
            if (!out) { av_frame_unref(frame); continue; }
            const int conv = swr_convert(swr, &out, maxOut,
                                         (const uint8_t**)frame->data, frame->nb_samples);
            if (conv > 0 && RingPush(gen, out, (size_t)conv * kAudioCh * 2))
                pushedAny = true;
            av_free(out);
            av_frame_unref(frame);
            if (stop) break;
        }
        return pushedAny;
    }

    // Blocks while the ring is full (backpressure); aborts on stop or seek.
    // The predicate must see wantSeek: while paused nothing drains the ring,
    // so without it a parked worker would sleep through every seek request
    // (the SeekCommon cvRing notify is what guarantees it is re-checked).
    // Reading wantSeek here is ordered: the notifier sets it under m, then
    // takes and releases ringM around the notify; this waiter re-reads under
    // the ringM it just acquired — the same benign-by-construction shape the
    // `stop` read below always had.
    bool RingPush(uint64_t gen, const uint8_t* src, size_t bytes) {
        std::unique_lock<std::mutex> lk(ringM);
        cvRing.wait(lk, [&] {
            return stop || wantSeek || seekGen != gen ||
                   RingFreeLocked() >= bytes;
        });
        if (stop || wantSeek || seekGen != gen) return false;
        const size_t first = std::min(bytes, ringCap - ringW);
        std::memcpy(ring + ringW, src, first);
        if (bytes > first) std::memcpy(ring, src + first, bytes - first);
        ringW = (ringW + bytes) % ringCap;
        return true;
    }

    // UI thread: hand back the next frame whose pts is due at `clock`.
    // Only frames TRULY expired (older than one frame interval behind the
    // clock) are dropped — anything merely not-yet-due stays queued and pops
    // on its own later tick. (The old latest-wins rule dropped every frame up
    // to clock+0.02 after the first pop, so a decode burst — B-frame reordering
    // makes the decode gate admit packets in 3-4 frame clumps — was consumed
    // as 1 display + N-1 discards, capping the playback display rate at ~0.5x
    // content fps. docs/50 section 9.) False = nothing due yet.
    bool PopVideoFrame(double clock, VideoFrame& out) {
        bool popped = false;
        {
            std::lock_guard<std::mutex> lk(m);
            if (videoQ.empty()) return false;
            if (videoQ.front().pts > clock + 0.02) {
                // Gate-starved first frame after a seek: the demuxer can only
                // land on a keyframe, which may sit above the shifted gate
                // (negative avDelay parks the gate |avDelay| below the target
                // and no decodable frame exists that far back). Display the
                // landing frame once instead of freezing the picture until
                // the clock crawls |avDelay| forward — forever while paused.
                if (!postSeekJump) return false;
                out = std::move(videoQ.front());
                videoQ.pop_front();
                postSeekJump = false;
                popped = true;
            } else {
                out = std::move(videoQ.front());
                videoQ.pop_front();
                // Unrenderable history = older than a full frame interval
                // behind the clock. Anything in between keeps its queue slot
                // and is displayed on a later tick (the UI chain polls at
                // 61 Hz, faster than any supported frame rate).
                const double frameInterval =
                    fps > 0.0 ? 1.0 / fps : 1.0 / 30.0;
                while (!videoQ.empty() &&
                       videoQ.front().pts < clock - frameInterval) {
                    out = std::move(videoQ.front());
                    videoQ.pop_front();
                }
                postSeekJump = false;
                popped = true;
            }
        }
        // The producer's backpressure wait (videoQ full) is only re-checked on
        // notify; without this, a drained queue leaves it parked forever and
        // EOF is never detected (ended never set).
        cv.notify_all();
        return popped;
    }
};

// UI-thread teardown. Bounded: a worker stuck inside a never-returning FFmpeg
// call can't be joined (see PlayerCore::TryClose) — then detach + release so
// the core is intentionally leaked (never deleted) instead of freezing the UI
// thread forever or freeing state the worker is still using.
template <typename CorePtr>
static void ClosePlayer(CorePtr& p) {
    if (!p) return;
    if (p->TryClose(2000)) { p.reset(); return; }
    p->Abandon();
    p.release();
}

ClientVPlayerApp::~ClientVPlayerApp() = default;

void ClientVPlayerApp::PlayerCoreDeleter::operator()(PlayerCore* p) const noexcept {
    delete p; // full type visible here; only reached when TryClose succeeded
}

void ClientVPlayerApp::OnInit() {
    auto main = std::make_unique<VPlayerRoot>("Video Player");
    main->SetWindowRect(JKRect{ 0, 0, 960, 640 });
    main->SetAttrFlags(WA_CHROMELESS); // server close button only (docs/23 §9)
    SetMainWindow(std::move(main));

    SetTimerInterval(16); // ~60 Hz frame cadence

    ImGui::CreateContext();
    jk::theme::ApplyImGuiTheme(); // JKTheme 팔레트 봉합 (P2 단계 3)
    ImGui::GetIO().IniFilename = nullptr;
    // Korean UI (열기... picker button) — Malgun Gothic like the shot/filedlg
    // apps; failure degrades to the default font.
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f,
                                 nullptr,
                                 io.Fonts->GetGlyphRangesKorean());
    lastFrame_ = std::chrono::steady_clock::now();

    // Probe affordance (vpt8): open a file without injected keyboard/mouse —
    // the path row needs focus, which synthetic input can't win reliably
    // while the desktop is in use. Probes set JK_VPLAYER_OPEN and spawn the
    // jkx directly; harmless no-op when the variable is absent.
    if (const char* env = std::getenv("JK_VPLAYER_OPEN"); env && env[0]) {
        std::snprintf(pathBuf_, sizeof(pathBuf_), "%s", env);
        OpenPath(pathBuf_);
    }
}

void ClientVPlayerApp::OnClose() {
    ClosePlayer(player_); // stops worker + audio device before ImGui teardown
    if (renderer_ && videoTex_) {
        SDL_DestroyTexture(static_cast<SDL_Texture*>(videoTex_));
    }
    videoTex_ = nullptr;
    if (imguiReady_) {
        ImGui_ImplJKWindow_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
}

bool ClientVPlayerApp::PreProcessMessage(const JKEvent& ev) {
    ImGui_ImplJKWindow_ProcessJKEvent(ev);
    if (ev.type == JKEventType::Timer) {
        frameDirty_ = true; // frame clock (docs/23 §11.5 lesson 5)
    }
    return true;
}

void ClientVPlayerApp::OnFrameCommitted() {
    frameDirty_ = false;
}

void ClientVPlayerApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
    if (!imguiReady_) {
        if (!ImGui_ImplJKWindow_Init(renderer))
            return;
        imguiReady_ = true;
    }
    renderer_ = renderer;

    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - lastFrame_).count();
    lastFrame_ = now;

    ImGui_ImplJKWindow_NewFrame(dt, w, h);
    ImGui::NewFrame();

    PumpAgentReplies();
    SyncVideoTexture(renderer);
    BuildUi(w, h);

    ImGui::Render();
    ImGui_ImplJKWindow_RenderDrawData(ImGui::GetDrawData(), renderer);
}

// Async open (spec D1): validates the path cheaply, arms a PlayerCore in the
// `opening` state and returns immediately — the worker thread does the demux/
// codec work and reports openError_ / playback through the state machine.
void ClientVPlayerApp::OpenPath(const char* path) {
    openError_.clear();
    ClosePlayer(player_); // bounded: never freezes the UI on a stuck worker
    // A wheel scrub session must not outlive its PlayerCore: the flag has no
    // release event of its own, and a stale session would fire its idle
    // release (a precision Seek to a dead target) against the next file.
    // The drag path self-heals on release; the wheel needs this explicit cut.
    wheelScrubbing_ = false;
    if (renderer_ && videoTex_) SDL_DestroyTexture(static_cast<SDL_Texture*>(videoTex_));
    videoTex_ = nullptr;
    texW_ = texH_ = 0;
    hasFrame_ = false;
    // The render gauge is a per-file verdict instrument — a stale window
    // straddling re-open would post the previous file's counts (final review).
    renderFrames_ = 0;
    renderHz_ = 0;
    renderWindowStart_ = 0.0;
    if (!path || !path[0]) { openError_ = "empty path"; return; }
    // The picker's next start folder is the CURRENT file's folder — every
    // open site feeds this (CLI arg, drag-drop, picker reply alike), not just
    // picker replies. A drive-root file ("I:\x.mp4") yields "I:", which Win32
    // resolves as current-dir-on-drive, not the root — normalize to "I:\".
    {
        const std::string p(path);
        const size_t slash = p.find_last_of("/\\");
        if (slash != std::string::npos) {
            std::string dir = p.substr(0, slash);
            if (dir.size() == 2 && dir[1] == ':') dir += '\\';
            lastDir_ = std::move(dir);
        }
    }
    // Existence gate moved into OpenStage (T2, T1 review MINOR-2 carry):
    // fopen on a dead UNC path blocks for the network timeout — on the UI
    // thread that froze the whole app. The worker classifies it as a failed
    // open ("파일을 찾을 수 없습니다") instead.
    PlayerCore* raw = nullptr;
    try {
        raw = new PlayerCore();
        raw->BeginOpen(path); // spawns the (existing) worker; never blocks
    } catch (const std::exception&) {
        openError_ = "메모리 부족"; // new/BeginOpen failure (spec 1a-3)
        delete raw; // dtor runs Close() on any half-armed state
        return;
    } catch (...) {
        openError_ = "플레이어 초기화 실패";
        delete raw;
        return;
    }
    player_.reset(raw);
}

// --- File picker (specs/2026-09-13-file-dialog Task 3) ----------------------
// One file_open query in flight at a time (palette SendTool pattern). The
// server parks the query, spawns the filedlg dialog, and completes the query
// when the dialog closes — the reply body is {"ok":true,"path":"..."} on open
// or {"ok":false} on cancel (plus the immediate error variants
// dialog_busy/bad_request/spawn_failed and the 600 s dialog_timeout).

void ClientVPlayerApp::RequestOpenDialog() {
    jk::client::JKClientSurface* surface = Surface();
    if (!surface || !surface->IsConnected()) return;
    if (fileOpenQueryId_ != 0) return; // already in flight — ignore
    std::string args = std::string("{\"filter\":\"") + EscapeJson(kVideoFilter) +
                       "\"";
    if (!lastDir_.empty())
        args += ",\"start\":\"" + EscapeJson(lastDir_) + "\"";
    args += "}";
    const uint32_t id = nextQueryId_++;
    if (!surface->SendAgentQuery(id, "{\"tool\":\"file_open\",\"args\":" +
                                        args + "}"))
        return;
    fileOpenQueryId_ = id;
}

void ClientVPlayerApp::PumpAgentReplies() {
    if (fileOpenQueryId_ == 0) return; // nothing parked; nothing sent otherwise
    jk::client::JKClientSurface* surface = Surface();
    jk::client::AgentReply reply;
    while (surface && surface->PollAgentReply(reply)) {
        if (reply.queryId != fileOpenQueryId_) continue;
        fileOpenQueryId_ = 0; // cleared on open AND on cancel/error
        agent::AgentJson body(reply.json);
        std::string path;
        // Any reply without a usable path is "cancelled, ignore" — 취소,
        // dialog_busy, bad_request, spawn_failed, dialog_timeout alike.
        if (!body.ok() || !body.GetStr("path", path) || path.empty()) continue;
        // lastDir_ is recorded in OpenPath — every open site feeds it there.
        OpenPath(path.c_str());
    }
}

void ClientVPlayerApp::SyncVideoTexture(SDL_Renderer* renderer) {
    PlayerCore* p = player_.get();
    // IsRunning() is the happens-before gate for the worker-written decode
    // state read below locklessly (videoW/fps/avDelay-adjacent fields): during
    // the Opening phase the worker is still mid-write, so don't touch them.
    if (!p || !p->IsRunning() || p->videoW <= 0) return;
    VideoFrame vf;
    if (p->jogging.load(std::memory_order_relaxed)) {
        // Frame-scrub: display the frame at the live dial target (ring hit)
        // or the decode creep toward it (fallback seek) — the clock gate
        // does not apply, the dial owns the picture. avDelay is scrub-domain
        // pure (spec: display-gate shift only for playback) and not applied.
        if (!p->JogFrame(vf)) return;
    } else {
        // The user A/V offset lives here — a display-gate shift only. Positive
        // avDelay shows frames earlier relative to the audio clock (= audio
        // later), without touching the clock/seek domain (no stepping drift).
        const double gate = p->ClockNow() +
                            (double)p->avDelay.load(std::memory_order_relaxed);
        if (!p->PopVideoFrame(gate, vf)) return;
    }

    if (!videoTex_ || texW_ != vf.w || texH_ != vf.h) {
        if (videoTex_) SDL_DestroyTexture(static_cast<SDL_Texture*>(videoTex_));
        // NV12 for even-dimension sources (the D3D11/GL backends render it
        // with a YUV→RGB shader — no CPU conversion), RGBA fallback for odd.
        videoTex_ = SDL_CreateTexture(renderer,
                                      vf.nv12 ? SDL_PIXELFORMAT_NV12
                                              : SDL_PIXELFORMAT_ABGR8888,
                                      SDL_TEXTUREACCESS_STREAMING, vf.w, vf.h);
        texW_ = vf.w;
        texH_ = vf.h;
        hasFrame_ = false;
        if (videoTex_) SDL_SetTextureScaleMode(static_cast<SDL_Texture*>(videoTex_),
                                               SDL_ScaleModeLinear);
    }
    if (videoTex_) {
        // Count the upload only when it succeeded — a failed upload must not
        // inflate the verdict gauge while the picture goes stale/black.
        // SDL_Update* return 0 on success, -1 on failure (SDL_render.h) — the
        // inverted `if (!uploaded) return;` here (398fe30's hardening wave,
        // landed after task 4's e2e run) took every SUCCESSFUL upload as a
        // failure: hasFrame_ never set, "no frame yet" at 0 Hz for both NV12
        // and RGBA playback while audio kept the clock running.
        const int upRet =
            vf.nv12 ? SDL_UpdateNVTexture(static_cast<SDL_Texture*>(videoTex_), nullptr,
                                          vf.pix->data(), vf.w,
                                          vf.pix->data() + (size_t)vf.w * vf.h, vf.w)
                    : SDL_UpdateTexture(static_cast<SDL_Texture*>(videoTex_), nullptr,
                                        vf.pix->data(), vf.w * 4);
        if (upRet != 0) return;
        hasFrame_ = true;
        // Render-rate window (see header comment).
        const double now = SDL_GetTicks() / 1000.0;
        if (renderHz_ == 0 || now - renderWindowStart_ >= 1.0) {
            renderHz_ = renderFrames_;
            renderFrames_ = 0;
            renderWindowStart_ = now;
        }
        ++renderFrames_;
    }
}

void ClientVPlayerApp::BuildUi(int w, int h) {
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
    const ImGuiWindowFlags wf = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                ImGuiWindowFlags_NoSavedSettings |
                                ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("vplayer", nullptr, wf);

    // The server reserves the top 24pt of every surface as window chrome
    // (title drag + close X, JKWindow kTitle) — clicks there start a move
    // grab and never reach this app. Push the first row below the strip.
    ImGui::SetCursorPosY(30.0f);

    // Path row + Open + "열기..." (file_open picker, agent channel).
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 170);
    ImGui::InputTextWithHint("##path", "media file path (mp4 / mkv / wav ...)",
                             pathBuf_, sizeof(pathBuf_));
    ImGui::SameLine();
    if (ImGui::Button("Open"))
        OpenPath(pathBuf_);
    ImGui::SameLine();
    if (ImGui::Button("열기..."))
        RequestOpenDialog();

    PlayerCore* p = player_.get();
    if (!p) {
        if (!openError_.empty())
            // 의도적 잔존 — 의미색 (P2 테마 스왑 제외)
            ImGui::TextColored(kErrorRed, "%s", openError_.c_str());
        ImGui::End();
        return;
    }

    const PlayerCore::Snap st = p->SnapNow();

    // Async open (D1): the worker owns demux/codec init — the UI reports
    // progress and offers the cancel affordance (interrupt_callback flag).
    if (st.opening) {
        ImGui::TextUnformatted("여는 중...");
        ImGui::SameLine();
        if (ImGui::Button("취소"))
            p->CancelOpen();
        ImGui::End();
        return;
    }
    if (st.openFailed) {
        // Adopt the classified open failure and drop the core — the idle
        // error screen below renders the same text from openError_.
        openError_ = st.error.empty() ? "파일을 열 수 없습니다" : st.error;
        ClosePlayer(player_);
        ImGui::TextColored(kErrorRed, "%s",
                           openError_.c_str());
        ImGui::End();
        return;
    }
    if (!st.opened) {
        ImGui::End();
        return;
    }

    // Transport controls.
    if (ImGui::Button(st.paused ? "Play" : "Pause"))
        p->SetPaused(!st.paused);
    if (st.ended) {
        ImGui::SameLine();
        if (ImGui::Button("Replay")) {
            p->Seek(0);
            p->SetPaused(false);
        }
        // Classified stop reason (read error / decode streak), next to
        // 다시 재생 — semantic red, same path as openError_.
        if (!st.error.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(kErrorRed, "%s",
                               st.error.c_str());
        }
    }

    // Seek slider: while dragged, keep the local value; commit on release.
    if (!seekingUi_)
        seekUi_ = (float)st.pos;
    const float dur = (float)(st.dur > 0 ? st.dur : 1.0);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 160);
    ImGui::SliderFloat("##seek", &seekUi_, 0.0f, dur, "");
    if (ImGui::IsItemActivated())
        seekingUi_ = true;
    if (seekingUi_ && ImGui::IsItemDeactivatedAfterEdit()) {
        p->Seek(seekUi_);
        seekingUi_ = false;
    }

    char tbuf[16], dbuf[16];
    FormatTime(tbuf, sizeof(tbuf),
               (jogActive_ || wheelScrubbing_) ? jogTarget_ : st.pos);
    FormatTime(dbuf, sizeof(dbuf), st.dur);
    ImGui::SameLine();
    ImGui::Text("%s / %s", tbuf, dbuf);

    ImGui::SameLine();
    float vol = st.vol;
    ImGui::SetNextItemWidth(120);
    if (ImGui::SliderFloat("##vol", &vol, 0.0f, 1.0f, "vol %.2f"))
        p->SetVolume(vol);

    // Manual A/V offset (user request): positive = audio later relative to
    // video. Pure display-gate shift — see SyncVideoTexture.
    float avd = p->avDelay.load(std::memory_order_relaxed);
    ImGui::SetNextItemWidth(160);
    if (ImGui::SliderFloat("##avsync", &avd, -1.0f, 1.0f, "A/V sync %+.2f s"))
        p->SetAvDelay(avd);
    ImGui::SameLine();
    // 의도적 잔존 — 의미색 (P2 테마 스왑 제외)
    ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.7f, 1.0f), "(+) audio later");

    // Render-rate metric (docs/50 §7.4-① verdict gauge) — always shown.
    ImGui::SameLine();
    ImGui::Text("렌더 %dHz", renderHz_);

    // One-shot failed-seek notice (T2, spec D3): the clock was restored to
    // the pre-seek position and playback continues, so the notice auto-
    // expires instead of sticking (a new seek request clears it too).
    if (!st.seekError.empty() &&
        std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                      st.seekErrorAt).count() < 3.0)
        ImGui::TextColored(kErrorRed, "%s", st.seekError.c_str());

    // Device-failure surfacing (T3, spec 1b-3): silent playback says why.
    // Informational status of a degraded-but-running pipeline — deliberately
    // not lastError/openError_ (the T1 classification channels stay
    // separate); the same notice color as the failed-seek status above.
    if (st.audioDeviceFailed)
        ImGui::TextColored(kErrorRed, "%s", "오디오 장치를 열 수 없음(무음 재생)");

    ImGui::Separator();

    // Video area: aspect-fit, centered.
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 vidMin(0, 0), vidMax(0, 0);
    if (hasFrame_ && videoTex_ && texW_ > 0 && texH_ > 0) {
        float scale = std::min(avail.x / texW_, avail.y / texH_);
        scale = std::max(scale, 0.01f);
        const ImVec2 size(texW_ * scale, texH_ * scale);
        const ImVec2 cur = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cur.x + (avail.x - size.x) * 0.5f,
                                   cur.y + (avail.y - size.y) * 0.5f));
        ImGui::Image((ImTextureID)videoTex_, size);
        vidMin = ImGui::GetItemRectMin();
        vidMax = ImGui::GetItemRectMax();
    } else if (!openError_.empty()) {
        // 의도적 잔존 — 의미색 (P2 테마 스왑 제외)
        ImGui::TextColored(kErrorRed, "%s", openError_.c_str());
    } else {
        ImGui::TextUnformatted("no frame yet");
    }

    // --- Jog knob overlay (translucent, floats over the video's bottom-right)
    // Drag = rotary scrub: tangential mouse motion spins the dial, time
    // follows, the clock is auto-paused and re-pinned per debounced seek so
    // the picture tracks the knob. Release = one precision seek to the
    // snapped frame, then playback state is restored.
    const bool jogUi = vidMax.x > vidMin.x && st.dur > 0 &&
                       (vidMax.x - vidMin.x) >= 220.0f &&
                       (vidMax.y - vidMin.y) >= 120.0f;
    if (jogUi) {
        const float kD = 64.0f;  // knob diameter
        const float kM = 16.0f;  // inset from the video rect
        const ImVec2 kmin(vidMax.x - kM - kD, vidMax.y - kM - kD);
        const ImVec2 kmax(kmin.x + kD, kmin.y + kD);
        const ImVec2 c((kmin.x + kmax.x) * 0.5f, (kmin.y + kmax.y) * 0.5f);
        const double dur = (double)st.dur;

        // ±1F step (frame-precise, paused-friendly). Guarded against an
        // active knob drag or wheel scrub (both own the transport while
        // active); Left/Right keys below share this path.
        const double fps = p->fps;
        // One full revolution = sPerRev seconds (tuned; scales with clip
        // length but never coarser than ~1 s/rev on short clips). Shared by
        // the drag (dθ per pixel) and the wheel (1/16 rev per tick).
        const double sPerRev = std::clamp(dur / 8.0, 1.0, 30.0);
        auto stepFrame = [&](int n) {
            if (fps <= 0.0 || jogActive_ || wheelScrubbing_) return;
            double t = std::clamp(st.pos + (double)n / fps, 0.0, dur);
            t = std::round(t * fps) / fps;
            p->Seek(t);
        };

        // Translucent step buttons left of the knob.
        ImGui::SetCursorScreenPos(ImVec2(kmin.x - 78.0f, c.y - 11.0f));
        // 의도적 잔존 — 의미색 (P2 테마 스왑 제외): 비디오 위 반투명 오버레이
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.20f, 0.55f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.32f, 0.80f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.32f, 0.42f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.95f, 0.75f));
        if (ImGui::Button("<", ImVec2(30, 22))) stepFrame(-1);
        ImGui::SameLine();
        if (ImGui::Button(">", ImVec2(30, 22))) stepFrame(+1);
        ImGui::PopStyleColor(4);

        ImGui::SetCursorScreenPos(kmin);
        ImGui::InvisibleButton("##jogknob", ImVec2(kD, kD));
        const bool hot = ImGui::IsItemHovered() || ImGui::IsItemActive();

        // Shared scrub finish (drag release / wheel idle release): one
        // precision seek to the snapped frame, audio decode back on, and the
        // pre-scrub pause state restored.
        auto finishScrub = [&]() {
            const double t = fps > 0.0
                                 ? std::round(std::clamp(jogTarget_, 0.0, dur) * fps) / fps
                                 : jogTarget_;
            p->Seek(t);
            p->SetJog(false);
            if (jogWasPlaying_) p->SetPaused(false);
        };
        const bool activated = ImGui::IsItemActivated();
        if (activated) {
            // Drag start: freeze the clock (auto-pause) and skip audio decode
            // while scrubbing so the worker never parks on the idle ring.
            // A drag takes over any wheel session — its release below becomes
            // the single finish (no stale wheel flag, no double seek).
            wheelScrubbing_ = false;
            jogActive_ = true;
            jogWasPlaying_ = !st.paused && !st.ended;
            if (jogWasPlaying_) p->SetPaused(true);
            p->SetJog(true);
            jogTarget_ = st.pos;
            jogLastSent_ = -1;
            knobCX_ = c.x; knobCY_ = c.y;
            jogMouseX_ = io.MousePos.x; jogMouseY_ = io.MousePos.y;
            jogLastSeek_ = std::chrono::steady_clock::now();
        }
        if (jogActive_ && !ImGui::IsItemActive()) {
            // Release: precision seek to the snapped frame, restore transport.
            finishScrub();
            jogActive_ = false;
        }
        if (jogActive_ && !activated) {
            // Tangential delta: dθ = cross(r, dMouse) / |r|². Wrap-free and
            // exact for any drag speed; skip when the pointer sits on the
            // center (r too small for a stable direction). The activation
            // frame is excluded: its MouseDelta is the press-placement jump
            // (the cursor can teleport onto the knob), not a rotation —
            // feeding it in once spun the dial by whole seconds.
            const float rx = jogMouseX_ - knobCX_, ry = jogMouseY_ - knobCY_;
            const float r2 = rx * rx + ry * ry;
            const float dx = io.MouseDelta.x, dy = io.MouseDelta.y;
            if (r2 > 36.0f && (dx != 0.0f || dy != 0.0f)) {
                const double dth = (double)(rx * dy - ry * dx) / (double)r2;
                jogTarget_ = std::clamp(
                    jogTarget_ + dth * 0.15915494309 * sPerRev, 0.0, dur);
            }
            jogMouseX_ += dx;
            jogMouseY_ += dy;
        }

        // Mouse-wheel scrub (spec 1d, D5): ticks over the knob feed the same
        // jogTarget_/debounce as a drag — 1/16 revolution per tick, wheel-up
        // = forward (the drag's clockwise = +dθ convention). Wheel input has
        // no release event, so the session ends 400 ms after the LAST tick
        // (every tick re-arms the timer) with exactly the drag-release
        // finish. While a drag is held the drag owns the finish: ticks still
        // move the shared target (latest wins) but the flag and timer stay
        // with the drag.
        if (hot && io.MouseWheel != 0.0f) {
            if (!jogActive_ && !wheelScrubbing_) {
                // Entry mirrors the drag start: auto-pause if playing, skip
                // audio decode, seed the target from the live position.
                wheelScrubbing_ = true;
                jogWasPlaying_ = !st.paused && !st.ended;
                if (jogWasPlaying_) p->SetPaused(true);
                p->SetJog(true);
                jogTarget_ = st.pos;
                jogLastSent_ = -1;
                jogLastSeek_ = std::chrono::steady_clock::now();
            }
            jogTarget_ = std::clamp(
                jogTarget_ + (double)io.MouseWheel * sPerRev / 16.0, 0.0, dur);
            if (!jogActive_)
                wheelLastTick_ = std::chrono::steady_clock::now();
        }

        // Frame-scrub pump: inside the retained ring the dial is zero-blocking
        // (JogTo per moved target — decode runs to it, JogFrame displays it).
        // Crossing the ring start falls back to the keyframe scrub seek
        // (blocking I/O — keeps the 40 ms latest-wins debounce; SeekCommon's
        // scrub branch feeds jogTargetPts so the fallback display is
        // frame-smooth too). The single jogTargetPts slot coalesces both.
        if ((jogActive_ && !activated) || (wheelScrubbing_ && !jogActive_)) {
            const double halfFrame = fps > 0.0 ? 0.5 / fps : 0.0;
            const bool ringHit = st.jogRingLo >= 0.0 &&
                                 jogTarget_ >= st.jogRingLo - halfFrame;
            if (ringHit) {
                if (jogTarget_ != jogLastSent_) {
                    p->JogTo(jogTarget_);
                    jogLastSent_ = jogTarget_;
                    // Arm the fallback debounce too: a later ring-miss must
                    // not burst-seek through every missed frame.
                    jogLastSeek_ = std::chrono::steady_clock::now();
                }
            } else {
                const auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration<double>(now - jogLastSeek_).count() >= 0.040 &&
                    jogTarget_ != jogLastSent_) {
                    p->SeekScrub(jogTarget_);
                    jogLastSent_ = jogTarget_;
                    jogLastSeek_ = now;
                }
            }
        }

        // Idle release (D5): 400 ms with no wheel tick while a wheel session
        // is open — same finish as the drag release, then the flag clears.
        if (wheelScrubbing_ &&
            std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                          wheelLastTick_).count() >= 0.400) {
            finishScrub();
            wheelScrubbing_ = false;
        }

        // Knob visuals: base disc + rim, position arc from 12 o'clock and a
        // head dot at the arc end. Drag/hover brightens the glass.
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float rad = kD * 0.5f;
        const ImU32 colBase = ImGui::IsItemActive()
                                  ? IM_COL32(30, 60, 110, 170)
                                  : hot ? IM_COL32(24, 24, 32, 150)
                                        : IM_COL32(16, 16, 22, 90);
        const ImU32 colRim = hot ? IM_COL32(140, 190, 255, 190)
                                 : IM_COL32(160, 170, 190, 110);
        dl->AddCircleFilled(c, rad, colBase, 24);
        dl->AddCircle(c, rad, colRim, 24, 2.0f);
        const double frac = std::clamp(
            ((jogActive_ || wheelScrubbing_) ? jogTarget_ : (double)st.pos) /
                dur,
            0.0, 1.0);
        if (frac > 0.002) {
            const float kTwoPi = 6.28318530718f;
            const float a0 = -kTwoPi * 0.25f; // 12 o'clock
            const float a1 = a0 + (float)(frac * 2.0) * kTwoPi;
            dl->PathArcTo(c, rad - 6.0f, a0, a1, 24);
            dl->PathStroke(IM_COL32(120, 220, 140, 200), 0, 3.5f);
            dl->AddCircleFilled(ImVec2(c.x + (rad - 6.0f) * std::cos(a1),
                                       c.y + (rad - 6.0f) * std::sin(a1)),
                                4.0f, IM_COL32(120, 220, 140, 255), 12);
        }
    }

    // Underrun diagnostic (T3, spec 1b-2): display-only debug counter in the
    // bottom-right status region, neutral status color (not an error). The
    // count is cumulative since the file opened and never resets mid-file;
    // hidden when there is no audio stream at all — a missing device is not
    // an underrun (spec edge).
    if (p->audioStream >= 0) {
        char ubuf[32];
        std::snprintf(ubuf, sizeof(ubuf), "underrun: %llu",
                      (unsigned long long)p->underruns.load(std::memory_order_relaxed));
        const ImVec2 tsz = ImGui::CalcTextSize(ubuf);
        ImGui::SetCursorScreenPos(
            ImVec2(w - tsz.x - 12.0f, h - tsz.y - 10.0f));
        ImGui::TextUnformatted(ubuf);
    }

    // Space toggles pause (unless typing in the path field).
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space, false) &&
        st.dur > 0 && !st.ended) {
        p->SetPaused(!st.paused);
    }

    // Left/Right = ±1 frame. repeat=false on purpose: key repeat would fire a
    // full flush-seek per repeat (~20/s). Skipped while a knob drag or wheel
    // scrub owns the transport (the debounced scrub seek would override it).
    if (!io.WantTextInput && st.dur > 0 && p->fps > 0.0 && !jogActive_ &&
        !wheelScrubbing_) {
        int step = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)) step = -1;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)) step = +1;
        if (step) {
            double t = std::clamp(st.pos + (double)step / p->fps, 0.0, (double)st.dur);
            p->Seek(std::round(t * p->fps) / p->fps);
        }
    }

    ImGui::End();
}

} // namespace jk