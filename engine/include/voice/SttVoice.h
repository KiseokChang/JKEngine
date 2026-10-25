#ifndef STTVOICE_H
#define STTVOICE_H
// STT 구두 전사 사슬 순수 부품 (#112 T2 — 스펙 2026-10-25-stt-voice-design
// §3 ② whisper.cpp 통합·⑦ selftest 2v 계보·§2.4 전사 비결정 원칙).
//
// 배치 근거 (원존 판정 실측 — 브리프 "기존 pure 부품 배치 원존 승계"):
//   * 순수 부품 = 헤더 인라인 — GalleryModel.h/MusicModel.h 선례 그대로
//     ("Header-inline on purpose: the selftest twins (engine/src/main.cpp
//     selftest 2g/2m) assert these parts by direct link with no extra TU").
//     selftest 2v-b/c와 T3/T4 소비자가 같은 진실원을 직소비한다. imgui/SDL/
//     client 형 무접촉 계약 유지.
//   * 도메인 디렉터리 쌍 = include/<domain>/ + src/<domain>/ — include/
//     process↔src/process(JKProcess.h)·include/fs↔src/fs(JKFs.h)·
//     include/agent↔src/agent(JKLmEngine.h) 원존 승계 → include/voice/ +
//     src/voice/. engine/src에 헤더를 두는 관행은 없다(실측 — src/agent·
//     src/process 전부 TU만).
//   * 스폰(비순수) leg = TU — JKLmEngine.cpp 선형(include/agent/JKLlmEngine.h
//     선언 + src/agent/JKLmEngine.cpp 본체 BuildOllamaDirectCmd/턴 스레드).
//     engine/src/voice/SttVoice.cpp가 jk::voice::run::WhisperTranscribe를
//     소유한다(jk::process 어댑터 — CreateProcessW/fork+exec 접점은 어댑터
//     몫, 이 TU는 축 무분기 1개).
//
// 본 수형 (T2 계약): 3부품 —
//   jk::voice::wav::Validate16kMono          — 16 kHz 모노 PCM 헤더 판정부
//   jk::voice::transcript::ParseLastText     — whisper-cli stdout 최후 텍스트 절단
//   jk::voice::run::WhisperTranscribe        — whisper-cli 별도 프로세스 스폰
// 전자 2종 = 순수(std만) — selftest 2v-b/c가 고정 입력으로 단정한다(스폰은
// 비결정 — selftest 단정 대상 아님, 런타임 실측은 probe/EYES 승계 — §2.4).

#include <cstdint>
#include <cstring>  // memcmp — RIFF/WAVE/fmt/data 매직 판별
#include <string>
#include <vector>

namespace jk {
namespace voice {

// ---- wav — 16 kHz 모노 PCM WAV 헤더 판정부 --------------------------------
// 입력 전제 = ffmpeg 정규화 결과(스펙 §3 ①: 폰 leg `ffmpeg -ar 16000 -ac 1`
// ·Win leg SDL 캡처가 같은 형태로 쓴다). 정규화 자체는 ffmpeg 몫 — 여기는
// **헤더만** 검증하고 비-16k/스테레오/비PCM 입력은 정직 거부(암묵 리샘플·
// 축소 재합성 금지 — "리샘플 단순 계약"). RIFF/WAVE 청크 수동 파독 —
// 캐논 44바이트 헤더(RIFF+fmt16+data) 초석, 보조 청크(LIST 등)는 스킵.
namespace wav {

// 계약 상수 (스펙 §3 ① 정규화 규약의 문자 그대로).
inline constexpr unsigned kPcmFormatTag = 1;      // WAVE_FORMAT_PCM
inline constexpr unsigned kVoiceChannels = 1;     // 모노
inline constexpr unsigned kVoiceSampleRate = 16000;
inline constexpr unsigned kVoiceBitsPerSample = 16;  // s16le

// LE 정수 판독 — RIFF 규약(little-endian) 고정. 이상 계열 blob(잘린 청크
// 헤더)은 경계 가드로 정직 거부한다.
inline unsigned Le16(const unsigned char* p) {
    return static_cast<unsigned>(p[0]) | (static_cast<unsigned>(p[1]) << 8);
}
inline unsigned Le32(const unsigned char* p) {
    return static_cast<unsigned>(p[0]) | (static_cast<unsigned>(p[1]) << 8) |
           (static_cast<unsigned>(p[2]) << 16) |
           (static_cast<unsigned>(p[3]) << 24);
}

inline bool Validate16kMono(const unsigned char* p, size_t n) {
    if (!p || n < 44) return false;  // RIFF(8)+fmt(+8+16)+data(+8) 최소 캐논
    if (std::memcmp(p, "RIFF", 4) != 0) return false;
    if (std::memcmp(p + 8, "WAVE", 4) != 0) return false;
    bool fmtOk = false, dataOk = false;
    size_t off = 12;  // RIFF(4)+size(4)+WAVE(4) 뒤부터 청크 열
    while (off + 8 <= n) {
        const unsigned char* id = p + off;
        const unsigned csize = Le32(p + off + 4);
        if (csize == 0xFFFFFFFFu || csize > n - (off + 8))
            return false;  // 잘린/무한 청크 — 정직 거부
        if (std::memcmp(id, "fmt ", 4) == 0) {
            if (csize < 16) return false;
            const unsigned char* f = p + off + 8;
            const unsigned wFormatTag = Le16(f + 0);
            const unsigned nChannels = Le16(f + 2);
            const unsigned nSamplesPerSec = Le32(f + 4);
            const unsigned nAvgBytesPerSec = Le32(f + 8);
            const unsigned nBlockAlign = Le16(f + 12);
            const unsigned wBitsPerSample = Le16(f + 14);
            if (wFormatTag != kPcmFormatTag) return false;  // PCM 고정(FFFE 확장 밖)
            if (nChannels != kVoiceChannels) return false;  // 모노 규약
            if (nSamplesPerSec != kVoiceSampleRate) return false;  // 16 kHz 규약
            if (wBitsPerSample != kVoiceBitsPerSample) return false;  // s16le
            if (nBlockAlign != nChannels * wBitsPerSample / 8) return false;
            if (nAvgBytesPerSec != nSamplesPerSec * nBlockAlign) return false;
            fmtOk = true;
        } else if (std::memcmp(id, "data", 4) == 0) {
            if (csize == 0) return false;  // 음성 결실 0 — whisper에 무의미
            dataOk = true;                 // 몸통 소비는 whisper-cli 몫(헤더만 검증)
        }
        off += 8 + csize + (csize & 1);  // 청크는 2바이트 정렬 패딩(RIFF 규약)
    }
    return fmtOk && dataOk;
}

}  // namespace wav

// ---- transcript — whisper-cli stdout 최후 텍스트 절단 ----------------------
// cli.cpp 원문 구조(실측 — examples/cli/cli.cpp whisper_print_segment_callback):
//   시스템 로그/usage/진행률은 전부 stderr — **stdout은 세그먼트 행만**.
//   첫 배치에 `printf("\n")`(s0==0 원문)·각 세그먼트 행 =
//   `[HH:MM:SS.mmm --> HH:MM:SS.mmm]  텍스트\n`(to_timestamp 규약
//   %02d:%02d:%02d.%03d — common-whisper.cpp 원문).
// 방어 원료 규약(ChatLlmActionParse 동형 — 스펙 §3 ②): 빈/이상 출력 = 빈
// 문자열 반환, err 없이 정직. 세그먼트 구조(`[` → ` --> ` → `]`)에 적합한
// 행만 텍스트로 셈 하고 최후의 것(=최후 세그먼트)을 절단한다.
namespace transcript {

inline std::string ParseLastText(const std::string& cliOutput) {
    std::string last;
    const size_t n = cliOutput.size();
    size_t pos = 0;
    while (pos < n) {
        size_t nl = cliOutput.find('\n', pos);
        if (nl == std::string::npos) nl = n;
        std::string line = cliOutput.substr(pos, nl - pos);
        pos = nl + 1 <= n ? nl + 1 : n;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() < 2 || line[0] != '[') continue;  // 개행·이상 행 무시
        const size_t arrow = line.find(" --> ");
        if (arrow == std::string::npos) continue;  // 끊긴 타임스탬프 = 비적합
        const size_t close = line.find(']', arrow);
        if (close == std::string::npos) continue;
        std::string text = line.substr(close + 1);
        const size_t beg = text.find_first_not_of(" \t");
        if (beg == std::string::npos) continue;  // 텍스트 결실 0 세그먼트
        const size_t end = text.find_last_not_of(" \t\r\n");
        last = text.substr(beg, end - beg + 1);
    }
    return last;
}

}  // namespace transcript

// ---- run — whisper-cli 별도 프로세스 스폰 ----------------------------------
// 본체 = engine/src/voice/SttVoice.cpp (JKLmEngine.cpp 선형 — 선언과 스폰
// 본체 분리: 순수 파서는 헤더 인라인, 스폰은 TU). 계약은 헤더가 진실원.
namespace run {

// whisper-cli 별도 프로세스 스폰 (스펙 §3 ② 권안 — 크래시 격리(폰 #3117
// 원존)·모델 교체 무재링크). 인자 형식 = cli.cpp 원문(§3 ②):
//   whisper-cli -m <model.ggml> -f <wav> -l ko
// (-o* 플래그 금지 — cli.cpp 파싱 크래시 원장(T1 함정 3)·stdout 파싱 계약.)
// 바이너리 resolve: jk::fs GetExecutablePath 형제(dir)의 whisper-build/
// whisper-cli(.exe — build/whisper-build·buildwsl/whisper-build·폰
// buildterm/whisper-build 공통 모양) — WHISPER_BIN env로 지역 오버라이드.
// 부재 = 정직 거부(fail-closed — T1 판정부 reason 원문 승계).
// 타임아웃: **전체 마감 600s — 단일 마감 1종** — 판정은 드레인 루프의 매
// 반복(fix r1 — stderr 진행률이 흘러도 마감 산다: !progressed 분기 안에
// 두면 idle-kill로 퇴화해 선언 계약과 부정했던 T2 본판정 수형) — 초과 =
// TerminateJobTree kill(kill-on-close job 계약 — JKLmEngine 원존). stdout
// 수취 후 transcript::ParseLastText 절단. 스폰 본체는 비결정 — selftest
// 단정 밖(§2.4), 성립 반환 = out에 절단 텍스트(빈 전사 = 정직 실패 +
// stderr 꼬리).
bool WhisperTranscribe(const std::string& wavPath,
                       const std::string& modelPath, std::string& out,
                       std::string& err);

}  // namespace run

}  // namespace voice
}  // namespace jk

#endif  // STTVOICE_H