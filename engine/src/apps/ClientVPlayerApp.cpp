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
    bool useWallClock = false;               // files without usable audio
    std::string lastError;

    std::mutex m;                            // decode/transport state
    std::condition_variable cv;
    std::mutex ringM;                        // audio ring only (SDL callback takes this)
    std::condition_variable cvRing;
    std::thread worker;
    bool paused = false, ended = false, stop = false, wantSeek = false;
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
        return audioRate ? (double)framesPlayed / audioRate : 0.0;
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

    void Seek(double t) {
        if (duration > 0) t = std::clamp(t, 0.0, duration);
        std::lock_guard<std::mutex> lk(m);
        wantSeek = true;
        seekTarget = t;
        if (useWallClock) {
            if (wallPlaying) { wallAccum += WallSec(wallStart); }
            wallBase = t;
            wallAccum = 0;
            wallStart = std::chrono::steady_clock::now();
            wallPlaying = !paused;
        }
        cv.notify_all();
    }

    // Stage-1 seek: runs on the worker with m held. Flush codecs + queues, park
    // the clock at the target and drop frames older than target (dropBeforePts).
    void DoSeekLockedStage1() {
        double t = seekTarget;
        if (duration > 0) t = std::clamp(t, 0.0, duration);
        dropBeforePts = t - 0.05;
        videoQ.clear();
        if (vctx) avcodec_flush_buffers(vctx);
        if (actx) avcodec_flush_buffers(actx);
        {
            std::lock_guard<std::mutex> lk(ringM);
            ++seekGen;
            ringR = ringW = 0;
            if (audioRate) framesPlayed = (uint64_t)(t * audioRate);
            cvRing.notify_all();
        }
        if (fmt && videoStream >= 0) {
            const int64_t ts = (int64_t)(t / av_q2d(videoTb));
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
            }
        }

        useWallClock = (audioStream < 0);
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
            } else if (pkt->stream_index == audioStream && actx) {
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
                                   ? -1.0 : frame->pts * av_q2d(videoTb);
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
    if (!p->PopVideoFrame(p->ClockNow(), vf)) return;

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
    FormatTime(tbuf, sizeof(tbuf), st.pos);
    FormatTime(dbuf, sizeof(dbuf), st.dur);
    ImGui::SameLine();
    ImGui::Text("%s / %s", tbuf, dbuf);

    ImGui::SameLine();
    float vol = st.vol;
    ImGui::SetNextItemWidth(120);
    if (ImGui::SliderFloat("##vol", &vol, 0.0f, 1.0f, "vol %.2f"))
        p->SetVolume(vol);

    ImGui::Separator();

    // Video area: aspect-fit, centered.
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (hasFrame_ && videoTex_ && texW_ > 0 && texH_ > 0) {
        float scale = std::min(avail.x / texW_, avail.y / texH_);
        scale = std::max(scale, 0.01f);
        const ImVec2 size(texW_ * scale, texH_ * scale);
        const ImVec2 cur = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cur.x + (avail.x - size.x) * 0.5f,
                                   cur.y + (avail.y - size.y) * 0.5f));
        ImGui::Image((ImTextureID)videoTex_, size);
    } else if (!openError_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", openError_.c_str());
    } else {
        ImGui::TextUnformatted("no frame yet");
    }

    // Space toggles pause (unless typing in the path field).
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space, false) &&
        st.dur > 0 && !st.ended) {
        p->SetPaused(!st.paused);
    }

    ImGui::End();
}

} // namespace jk