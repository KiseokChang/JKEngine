// push-to-talk 발화 수집 본체 — Win SDL2 오디오 캡처 (#112 T3 — 스펙
// 2026-10-25-stt-voice-design §3 ① Win 축·⑤ 무음 계약). 계약은
// include/voice/SttVoice.h namespace capture가 진실원.
//
// 원존 수형 = vplayer/JKSDLAudioBackend의 SDL_OpenAudioDevice 재생 장치와
// **별개 캡처 장치**(iscapture=1 — WASAPI 공유 모드 → 재생 동시 성립의 Win 축
// 근거). 요청 spec = 16 kHz/모노/s16le, allowed_changes=0 — SDL이 변환을
// 스스로 맞춘다(기기 채널/샘플레이트를 그대로 먹고 WAV를 재합성하는 합성
// 금지 — 2v-⑥/⑦ 거부 규약의 사전 계약; 필요한 정규화는 SDL 개방 지정 1회로
// 끝난다). 콜백은 누적 버퍼 뮤텍스 1개 — 재생 코드와 접점 0(무음 계약: 재생
// 계통 코드 무변조·조그 무음 원존 무영향). 실패 = 정직 거부(fail-closed).
//
// 캡처 본체는 selftest 밖(스펙 §3 ⑦ — 실기기 probe/T6 몫)이고 전사 비결정
// 원칙(§2.4)에 따라 selftest 어셜션 0 — 본 TU는 빌드 등호만 캐논에 실린다.

#include <SDL.h>

#include <voice/SttVoice.h>

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace jk {
namespace voice {
namespace capture {

namespace {

// 세션 상태 — SDL 오디오 스레드(콜백)와 소유 스레드(Start/Stop) 사이.
std::mutex g_capMtx;
SDL_AudioDeviceID g_dev = 0;      // 캡처 장치(별개 — 재생 장치 무접촉)
std::string g_wavPath;            // StopCapture가 WAV를 쓰는 예약 경로
std::string g_lastError;          // 최근 정직 거부 원문(표면 표기)
std::vector<unsigned char> g_buf;  // 누적 PCM(s16le 모노 16 kHz — SDL 변환)

// SDL 개방 spec 계약 — SttVoice.h wav 계약 상수의 SDL 판(16 kHz·모노·s16le).
constexpr int kCaptureRate = static_cast<int>(wav::kVoiceSampleRate);   // 16000
constexpr int kCaptureChannels = static_cast<int>(wav::kVoiceChannels);  // 1
constexpr Uint16 kCaptureFormat = AUDIO_S16LSB;                         // s16le
constexpr int kCaptureSamples = 1024;  // 콜백 청크 — push-to-talk 지연 감도

void CaptureCallback(void* /*userdata*/, Uint8* stream, int len) {
    if (len <= 0) return;
    std::lock_guard<std::mutex> lock(g_capMtx);
    g_buf.insert(g_buf.end(), stream, stream + len);
}

// 캐논 44바이트 RIFF/WAVE 헤더 + PCM — 2v-b riffBlob이 단정한 계약 형태의
// 런타임판(LE 직기입 — 암묵 리샘플·축소 재합성 없음: 받은 몸통을 그대로 쓴다).
void AppendLe16(std::vector<unsigned char>& b, unsigned v) {
    b.push_back(static_cast<unsigned char>(v & 0xFF));
    b.push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
}
void AppendLe32(std::vector<unsigned char>& b, uint32_t v) {
    for (int i = 0; i < 4; ++i)
        b.push_back(static_cast<unsigned char>((v >> (8 * i)) & 0xFF));
}
void AppendTag(std::vector<unsigned char>& b, const char* tag4) {
    b.insert(b.end(), tag4, tag4 + 4);
}

bool BuildWavBlob(const std::vector<unsigned char>& pcm,
                  std::vector<unsigned char>& blob) {
    if (pcm.empty()) return false;  // 결실 0 — 빈 WAV는 whisper에 무의미
    const uint32_t dataBytes = static_cast<uint32_t>(pcm.size());
    blob.clear();
    AppendTag(blob, "RIFF");
    AppendLe32(blob, 36 + dataBytes);  // riff size
    AppendTag(blob, "WAVE");
    AppendTag(blob, "fmt ");
    AppendLe32(blob, 16);              // fmt 청크 크기(PCM 캐논 16)
    AppendLe16(blob, wav::kPcmFormatTag);
    AppendLe16(blob, wav::kVoiceChannels);
    AppendLe32(blob, static_cast<uint32_t>(wav::kVoiceSampleRate));
    AppendLe32(blob, wav::kVoiceSampleRate * wav::kVoiceChannels *
                         wav::kVoiceBitsPerSample / 8);  // nAvgBytesPerSec
    AppendLe16(blob, wav::kVoiceChannels * wav::kVoiceBitsPerSample / 8);
    AppendLe16(blob, wav::kVoiceBitsPerSample);
    AppendTag(blob, "data");
    AppendLe32(blob, dataBytes);
    blob.insert(blob.end(), pcm.begin(), pcm.end());
    return true;
}

}  // namespace

bool StartCapture(const std::string& wavPath, std::string& err) {
    err.clear();
    // SDL 오디오 서브시스템 — 재생 계통(Mix_Open)이 이미 열었어도 멱등(
    // SDL_InitSubSystem — 이미 개시된 서브시스템은 무동작).
    SDL_InitSubSystem(SDL_INIT_AUDIO);

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = kCaptureRate;
    want.format = kCaptureFormat;
    want.channels = kCaptureChannels;
    want.samples = kCaptureSamples;
    want.callback = CaptureCallback;
    // 재생 장치와 별개 장치 — device=NULL(기본 캡처 장치)·iscapture=1.
    // allowed_changes=0 = SDL이 변환까지 맡아 obtained가 계약 spec과 일치(
    // 리샘플 재합성 금지 계약의 SDL 지정 1회). 개방 실패(마이크 부재·장치
    // 충돌) = 정직 거부.
    const SDL_AudioDeviceID dev =
        SDL_OpenAudioDevice(nullptr, 1, &want, &have, 0);
    if (dev == 0) {
        err = std::string("구두 캡처 개방 실패 (SDL: ") +
              (SDL_GetError() ? SDL_GetError() : "unknown") + ")";
        std::lock_guard<std::mutex> lock(g_capMtx);
        g_lastError = err;
        return false;
    }
    // 방어 검증 — allowed_changes=0 계약의 이중 봉합(비적합 = 사용하지 않고
    // 즉시 닫는다 — 조용한 48k 수용 없음).
    if (have.freq != kCaptureRate || have.channels != kCaptureChannels ||
        have.format != kCaptureFormat) {
        SDL_CloseAudioDevice(dev);
        err = "구두 캡처 spec 부적합 (16 kHz 모노 s16le 불성립) — 정규화 사전 "
              "계약";
        std::lock_guard<std::mutex> lock(g_capMtx);
        g_lastError = err;
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(g_capMtx);
        if (g_dev != 0) {  // 이중 개방 — 이미 진행 중 = 정직 거부
            SDL_CloseAudioDevice(dev);
            err = "구두 캡처 이미 진행 중";
            g_lastError = err;
            return false;
        }
        g_dev = dev;
        g_wavPath = wavPath;
        g_buf.clear();
        g_lastError.clear();
        SDL_PauseAudioDevice(dev, 0);  // 캡처 개시
    }
    return true;
}

bool StopCapture(std::string& err) {
    err.clear();
    SDL_AudioDeviceID dev = 0;
    std::string wavPath;
    {
        std::lock_guard<std::mutex> lock(g_capMtx);
        if (g_dev == 0) {
            err = "구두 캡처 진행 중 아님";
            return false;  // 상태 무변(진행 중이 아니었다)
        }
        dev = g_dev;
        wavPath = g_wavPath;
    }
    SDL_PauseAudioDevice(dev, 1);  // 캡처 정지(마지막 콜백 배달은 아래 잠금 순)
    // SDL_LockAudioDevice — 진행 중 콜백 완료까지 대기+이후 배달 봉인(SDL
    // 규약). 뮤텍스 순서: 장치 잠금 → g_capMtx — 콜백은 g_capMtx만 잡으므로
    // 순환 없음(콜백이 g_capMtx를 기다려도 장치 잠금은 콜백 완료 후 획득).
    SDL_LockAudioDevice(dev);
    std::vector<unsigned char> pcm;
    {
        std::lock_guard<std::mutex> lock(g_capMtx);
        pcm.swap(g_buf);  // 잠겨 있으므로 이 사이 배달 없음 — 수확 누락 0
    }
    SDL_UnlockAudioDevice(dev);
    SDL_CloseAudioDevice(dev);  // 장치 소멸 — push-to-talk 수명의 끝
    {
        std::lock_guard<std::mutex> lock(g_capMtx);
        g_dev = 0;
        g_wavPath.clear();
    }
    std::vector<unsigned char> blob;
    if (pcm.empty() || !BuildWavBlob(pcm, blob)) {
        err = "구두 캡처 빈 결실 (PCM 0바이트)";
        std::lock_guard<std::mutex> lock(g_capMtx);
        g_lastError = err;
        return false;
    }
    std::FILE* f = std::fopen(wavPath.c_str(), "wb");
    if (!f) {
        err = "구두 캡처 wav 쓰기 실패: " + wavPath;
        std::lock_guard<std::mutex> lock(g_capMtx);
        g_lastError = err;
        return false;
    }
    const size_t wrote = std::fwrite(blob.data(), 1, blob.size(), f);
    std::fclose(f);
    if (wrote != blob.size()) {
        std::remove(wavPath.c_str());  // 절반 WAV를 잔산으로 남기지 않는다
        err = "구두 캡처 wav 쓰기 실패(부분 기록): " + wavPath;
        std::lock_guard<std::mutex> lock(g_capMtx);
        g_lastError = err;
        return false;
    }
    return true;
}

bool IsCapturing() {
    std::lock_guard<std::mutex> lock(g_capMtx);
    return g_dev != 0;
}

CaptureState GetCaptureState() {
    std::lock_guard<std::mutex> lock(g_capMtx);
    CaptureState st;
    st.capturing = g_dev != 0;
    st.wavPath = g_wavPath;
    st.lastError = g_lastError;
    return st;
}

}  // namespace capture
}  // namespace voice
}  // namespace jk
