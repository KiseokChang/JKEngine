#include <apps/ClientVPlayerApp.h>

#include <imgui_impl_jkwindow.h>
#include <imgui.h>
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
        dc.SetColor(18, 18, 22, 255);
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

// One decoded video frame, already converted to RGBA for SDL_UpdateTexture.
struct VideoFrame {
    double pts = 0;
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
};

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
    std::string lastError;

    std::mutex m;                            // decode/transport state
    std::condition_variable cv;
    std::mutex ringM;                        // audio ring only (SDL callback takes this)
    std::condition_variable cvRing;
    std::thread worker;
    std::atomic<bool> paused{false};         // atomic: ClockNow reads it under ringM
    bool ended = false, stop = false, wantSeek = false;
    bool jogSeek = false;                    // setter-owned: pending seek is a scrub (keyframe-only)
    std::atomic<bool> jogging{false};        // scrub drag in progress: worker skips audio decode
    double seekTarget = 0;
    double dropBeforePts = -1;               // frames older than this are stale (post-seek)
    uint64_t seekGen = 0;                    // bumped on seek; in-flight audio pushes abort
    std::deque<VideoFrame> videoQ;

    // Audio byte ring (S16 stereo). One byte of slack distinguishes full/empty.
    uint8_t* ring = nullptr;
    size_t ringCap = 0, ringR = 0, ringW = 0;
    uint64_t framesPlayed = 0;               // master clock, in sample frames
    std::atomic<float> volume{0.8f};

    SDL_AudioDeviceID dev = 0;

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
        double pos = 0, dur = 0;
        float vol = 0.8f;
    };

    Snap SnapNow() {
        std::lock_guard<std::mutex> lk(m);
        Snap s;
        s.opened = fmt != nullptr;
        s.paused = paused;
        s.ended = ended;
        s.pos = ClockNow(); // lock order m -> ringM, consistent everywhere
        s.dur = duration;
        s.vol = volume.load(std::memory_order_relaxed);
        return s;
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
        if (useWallClock) {
            if (wallPlaying) { wallAccum += WallSec(wallStart); }
            wallBase = t;
            wallAccum = 0;
            wallStart = std::chrono::steady_clock::now();
            wallPlaying = !paused;
        }
        cv.notify_all();
    }

    // Jog mode: while scrubbing (usually paused) the audio ring never drains,
    // so RingPush would park the worker mid-GOP and stall video decode. Skip
    // audio decode entirely — packets are still read + unref'd, keeping the
    // demuxer position in sync for the next seek.
    void SetJog(bool j) { jogging.store(j, std::memory_order_relaxed); }

    // Stage-1 seek: runs on the worker with m held. Flush codecs + queues, park
    // the clock at the target and drop frames older than target (dropBeforePts).
    // Scrub seeks are keyframe-only: dropBeforePts = -1 keeps every decoded
    // frame, so the landing keyframe displays immediately and decode creeps
    // toward the target (the pop gate converges as videoQ drains). NOPTS
    // frames are already rejected by pts < 0 in DecodeVideoPacket, so -1.0 is
    // a safe "drop nothing" sentinel.
    void DoSeekLockedStage1() {
        double t = seekTarget;
        if (duration > 0) t = std::clamp(t, 0.0, duration);
        // With a negative user A/V offset the display gate sits at
        // clock + avDelay (< t), so the first pushable frame (t - 0.05)
        // would exceed the gate and nothing pops until the clock crawls
        // |avDelay| forward — a frozen picture after every seek, forever
        // while paused. Extend the stale cutoff to cover the shifted gate
        // (positive offsets need nothing: the gate is in the future).
        dropBeforePts = jogSeek
                            ? -1.0
                            : t - 0.05 +
                                  std::min(0.0, (double)avDelay.load(std::memory_order_relaxed));
        videoQ.clear();
        if (vctx) avcodec_flush_buffers(vctx);
        if (actx) avcodec_flush_buffers(actx);
        {
            std::lock_guard<std::mutex> lk(ringM);
            ++seekGen;
            ringR = ringW = 0;
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
        if (fmt && videoStream >= 0) {
            const int64_t ts =
                (int64_t)((t + ptsOrigin) / av_q2d(videoTb));
            avformat_seek_file(fmt, videoStream, INT64_MIN, ts, ts, 0);
        }
        ended = false;
    }

    bool Open(const char* path) {
        fmt = nullptr;
        int r = avformat_open_input(&fmt, path, nullptr, nullptr);
        if (r < 0) { lastError = "open: " + AvErr(r); return false; }
        if (avformat_find_stream_info(fmt, nullptr) < 0) { lastError = "find_stream_info failed"; return false; }
        if (fmt->duration > 0) duration = fmt->duration / (double)AV_TIME_BASE;

        const AVCodec* vdec = nullptr;
        const AVCodec* adec = nullptr;
        videoStream = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, &vdec, 0);
        if (videoStream >= 0) {
            AVStream* st = fmt->streams[videoStream];
            videoTb = st->time_base;
            vctx = avcodec_alloc_context3(vdec);
            avcodec_parameters_to_context(vctx, st->codecpar);
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
                if (fr.num > 0 && fr.den > 0)
                    fps = std::clamp((double)fr.num / (double)fr.den, 1.0, 240.0);
            }
        }
        audioStream = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, &adec, 0);
        if (audioStream >= 0) {
            AVStream* st = fmt->streams[audioStream];
            actx = avcodec_alloc_context3(adec);
            avcodec_parameters_to_context(actx, st->codecpar);
            if (avcodec_open2(actx, adec, nullptr) < 0) {
                avcodec_free_context(&actx);
                audioStream = -1;
            }
        }
        if (videoStream < 0 && audioStream < 0) {
            lastError = "no decodable audio/video stream";
            return false;
        }

        if (videoStream >= 0) {
            sws = sws_getContext(videoW, videoH, vctx->pix_fmt,
                                 videoW, videoH, AV_PIX_FMT_RGBA,
                                 SWS_BILINEAR, nullptr, nullptr, nullptr);
            if (!sws) { lastError = "sws_getContext failed"; return false; }
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

        worker = std::thread([this] { WorkerLoop(); });
        if (dev) SDL_PauseAudioDevice(dev, 0); // start unpaused; underruns are silence
        return true;
    }

    void Close() {
        {
            std::lock_guard<std::mutex> lk(m);
            if (stop) return;
            stop = true;
        }
        cv.notify_all();
        { std::lock_guard<std::mutex> lk(ringM); cvRing.notify_all(); }
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
        if (n < (size_t)len) std::memset(stream + n, 0, len - n);
        const float v = p->volume.load(std::memory_order_relaxed);
        if (v < 0.999f) {
            auto* s = reinterpret_cast<int16_t*>(stream);
            for (int i = 0; i < len / 2; ++i) s[i] = (int16_t)(s[i] * v);
        }
        p->framesPlayed += n / (kAudioCh * sizeof(int16_t));
    }

    void WorkerLoop() {
        AVPacket* pkt = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        if (!pkt || !frame) return;
        while (true) {
            {
                std::unique_lock<std::mutex> lk(m);
                cv.wait(lk, [&] { return stop || wantSeek || !ended; });
                if (stop) break;
                if (wantSeek) {
                    DoSeekLockedStage1();
                    wantSeek = false;
                }
            }
            const int r = av_read_frame(fmt, pkt);
            if (r < 0) {
                std::lock_guard<std::mutex> lk(m);
                ended = true; // park; Seek()/stop wake us again
                cv.notify_all();
                continue;
            }
            bool cont = true;
            if (pkt->stream_index == videoStream) {
                cont = DecodeVideoPacket(pkt, frame);
            } else if (pkt->stream_index == audioStream && actx &&
                       !jogging.load(std::memory_order_relaxed)) {
                DecodeAudioPacket(pkt, frame);
            }
            av_packet_unref(pkt);
            if (!cont) continue; // seek/stop fired mid-packet; loop top handles it
        }
        av_packet_free(&pkt);
        av_frame_free(&frame);
    }

    // Returns false when decoding should stop (seek/stop requested).
    bool DecodeVideoPacket(AVPacket* pkt, AVFrame* frame) {
        if (avcodec_send_packet(vctx, pkt) < 0) return true;
        while (true) {
            if (avcodec_receive_frame(vctx, frame) < 0) return true;
            const double pts = frame->pts == AV_NOPTS_VALUE
                                   ? -1.0
                                   : frame->pts * av_q2d(videoTb) - ptsOrigin;
            if (pts < 0) { av_frame_unref(frame); continue; }
            VideoFrame vf;
            vf.pts = pts;
            vf.w = videoW;
            vf.h = videoH;
            vf.rgba.resize((size_t)videoW * videoH * 4);
            uint8_t* dst[4] = { vf.rgba.data(), nullptr, nullptr, nullptr };
            int dstStride[4] = { videoW * 4, 0, 0, 0 };
            sws_scale(sws, frame->data, frame->linesize, 0, frame->height, dst, dstStride);
            av_frame_unref(frame);
            {
                std::unique_lock<std::mutex> lk(m);
                // Backpressure: hold at most 3 pending frames (~100 ms at 30 fps).
                cv.wait(lk, [&] { return stop || wantSeek || videoQ.size() < 3; });
                if (stop || wantSeek) return false;
                if (pts < dropBeforePts) continue; // stale frame from before the seek
                videoQ.push_back(std::move(vf));
            }
        }
    }

    void DecodeAudioPacket(AVPacket* pkt, AVFrame* frame) {
        if (avcodec_send_packet(actx, pkt) < 0) return;
        uint64_t gen;
        { std::lock_guard<std::mutex> lk(ringM); gen = seekGen; }
        while (avcodec_receive_frame(actx, frame) == 0) {
            const int maxOut = swr_get_out_samples(swr, frame->nb_samples);
            if (maxOut <= 0) { av_frame_unref(frame); continue; }
            uint8_t* out = (uint8_t*)av_malloc((size_t)maxOut * kAudioCh * 2);
            if (!out) { av_frame_unref(frame); continue; }
            const int conv = swr_convert(swr, &out, maxOut,
                                         (const uint8_t**)frame->data, frame->nb_samples);
            if (conv > 0) RingPush(gen, out, (size_t)conv * kAudioCh * 2);
            av_free(out);
            av_frame_unref(frame);
            if (stop) break;
        }
    }

    // Blocks while the ring is full (backpressure); aborts on stop or seek.
    bool RingPush(uint64_t gen, const uint8_t* src, size_t bytes) {
        std::unique_lock<std::mutex> lk(ringM);
        cvRing.wait(lk, [&] { return stop || seekGen != gen || RingFreeLocked() >= bytes; });
        if (stop || seekGen != gen) return false;
        const size_t first = std::min(bytes, ringCap - ringW);
        std::memcpy(ring + ringW, src, first);
        if (bytes > first) std::memcpy(ring, src + first, bytes - first);
        ringW = (ringW + bytes) % ringCap;
        return true;
    }

    // UI thread: hand back the latest frame whose pts is due at `clock`,
    // dropping older ones. False = nothing due yet.
    bool PopVideoFrame(double clock, VideoFrame& out) {
        bool popped = false;
        {
            std::lock_guard<std::mutex> lk(m);
            if (videoQ.empty() || videoQ.front().pts > clock + 0.02) return false;
            out = std::move(videoQ.front());
            videoQ.pop_front();
            // Everything older than the frame we just took is unrenderable history.
            while (!videoQ.empty() && videoQ.front().pts <= clock + 0.02) {
                out = std::move(videoQ.front());
                videoQ.pop_front();
            }
            popped = true;
        }
        // The producer's backpressure wait (videoQ full) is only re-checked on
        // notify; without this, a drained queue leaves it parked forever and
        // EOF is never detected (ended never set).
        cv.notify_all();
        return popped;
    }
};

ClientVPlayerApp::~ClientVPlayerApp() = default;

void ClientVPlayerApp::PlayerCoreDeleter::operator()(PlayerCore* p) const noexcept {
    delete p; // full type visible here
}

void ClientVPlayerApp::OnInit() {
    auto main = std::make_unique<VPlayerRoot>("Video Player");
    main->SetWindowRect(JKRect{ 0, 0, 960, 640 });
    main->SetAttrFlags(WA_CHROMELESS); // server close button only (docs/23 §9)
    SetMainWindow(std::move(main));

    SetTimerInterval(16); // ~60 Hz frame cadence

    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    lastFrame_ = std::chrono::steady_clock::now();
}

void ClientVPlayerApp::OnClose() {
    player_.reset(); // stops worker + audio device before ImGui teardown
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

    SyncVideoTexture(renderer);
    BuildUi(w, h);

    ImGui::Render();
    ImGui_ImplJKWindow_RenderDrawData(ImGui::GetDrawData(), renderer);
}

void ClientVPlayerApp::OpenPath(const char* path) {
    openError_.clear();
    player_.reset();
    if (renderer_ && videoTex_) SDL_DestroyTexture(static_cast<SDL_Texture*>(videoTex_));
    videoTex_ = nullptr;
    texW_ = texH_ = 0;
    hasFrame_ = false;
    if (!path || !path[0]) { openError_ = "empty path"; return; }
    PlayerCore* raw = new PlayerCore();
    if (!raw->Open(path)) {
        openError_ = raw->lastError;
        delete raw; // dtor runs Close() on any half-opened state
        return;
    }
    player_.reset(raw);
}

void ClientVPlayerApp::SyncVideoTexture(SDL_Renderer* renderer) {
    PlayerCore* p = player_.get();
    if (!p || p->videoW <= 0) return;
    VideoFrame vf;
    // The user A/V offset lives here — a display-gate shift only. Positive
    // avDelay shows frames earlier relative to the audio clock (= audio
    // later), without touching the clock/seek domain (no stepping drift).
    const double gate = p->ClockNow() +
                        (double)p->avDelay.load(std::memory_order_relaxed);
    if (!p->PopVideoFrame(gate, vf)) return;

    if (!videoTex_ || texW_ != vf.w || texH_ != vf.h) {
        if (videoTex_) SDL_DestroyTexture(static_cast<SDL_Texture*>(videoTex_));
        videoTex_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                      SDL_TEXTUREACCESS_STREAMING, vf.w, vf.h);
        texW_ = vf.w;
        texH_ = vf.h;
        hasFrame_ = false;
        if (videoTex_) SDL_SetTextureScaleMode(static_cast<SDL_Texture*>(videoTex_),
                                               SDL_ScaleModeLinear);
    }
    if (videoTex_) {
        SDL_UpdateTexture(static_cast<SDL_Texture*>(videoTex_), nullptr,
                          vf.rgba.data(), vf.w * 4);
        hasFrame_ = true;
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

    // Path row + Open.
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70);
    ImGui::InputTextWithHint("##path", "media file path (mp4 / mkv / wav ...)",
                             pathBuf_, sizeof(pathBuf_));
    ImGui::SameLine();
    if (ImGui::Button("Open"))
        OpenPath(pathBuf_);

    PlayerCore* p = player_.get();
    if (!p) {
        if (!openError_.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", openError_.c_str());
        ImGui::End();
        return;
    }

    const PlayerCore::Snap st = p->SnapNow();
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
    FormatTime(tbuf, sizeof(tbuf), jogActive_ ? jogTarget_ : st.pos);
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
    ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.7f, 1.0f), "(+) audio later");

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
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", openError_.c_str());
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
        // active knob drag; Left/Right keys below share this path.
        const double fps = p->fps;
        auto stepFrame = [&](int n) {
            if (fps <= 0.0 || jogActive_) return;
            double t = std::clamp(st.pos + (double)n / fps, 0.0, dur);
            t = std::round(t * fps) / fps;
            p->Seek(t);
        };

        // Translucent step buttons left of the knob.
        ImGui::SetCursorScreenPos(ImVec2(kmin.x - 78.0f, c.y - 11.0f));
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

        const bool activated = ImGui::IsItemActivated();
        if (activated) {
            // Drag start: freeze the clock (auto-pause) and skip audio decode
            // while scrubbing so the worker never parks on the idle ring.
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
            const double t = fps > 0.0
                                 ? std::round(std::clamp(jogTarget_, 0.0, dur) * fps) / fps
                                 : jogTarget_;
            p->Seek(t);
            p->SetJog(false);
            if (jogWasPlaying_) p->SetPaused(false);
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
                // One full revolution = sPerRev seconds (tuned; scales with
                // clip length but never coarser than ~1 s/rev on short clips).
                const double sPerRev = std::clamp(dur / 8.0, 1.0, 30.0);
                jogTarget_ = std::clamp(
                    jogTarget_ + dth * 0.15915494309 * sPerRev, 0.0, dur);
            }
            jogMouseX_ += dx;
            jogMouseY_ += dy;
            // Debounced latest-wins scrub seek: 40 ms cadence, target only
            // when it moved. The single wantSeek/seekTarget slot coalesces.
            const auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration<double>(now - jogLastSeek_).count() >= 0.040 &&
                jogTarget_ != jogLastSent_) {
                p->SeekScrub(jogTarget_);
                jogLastSent_ = jogTarget_;
                jogLastSeek_ = now;
            }
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
            (jogActive_ ? jogTarget_ : (double)st.pos) / dur, 0.0, 1.0);
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

    // Space toggles pause (unless typing in the path field).
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space, false) &&
        st.dur > 0 && !st.ended) {
        p->SetPaused(!st.paused);
    }

    // Left/Right = ±1 frame. repeat=false on purpose: key repeat would fire a
    // full flush-seek per repeat (~20/s).
    if (!io.WantTextInput && st.dur > 0 && p->fps > 0.0 && !jogActive_) {
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