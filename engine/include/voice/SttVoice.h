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
//
// T3 승계(#112 — 스펙 §3 ① 발화 수집·③ 접점·⑤ 무음 계약):
//   jk::stt::VoiceLegVerdict                — (main.cpp T1 에서 승계 — 단일
//     진실원 — 런타임 게이트(jk::voice::turn::RunVoiceTurn)와 표면(T4 jkapp_chat
//     — main.cpp 심볼에 접점 없다)이 같은 reason 원문을 먹는다. 2v-a의 어설션
//     원문(4건)은 무변조 — 호출점 위치만 승계된다.)
//   jk::voice::capture::{StartCapture,StopCapture,…} — Win SDL2 push-to-talk
//     캡처(본체 = src/voice/VoiceCaptureSDL.cpp — SDL이므로 헤더는 SDL 형
//     무접촉 선언만 — 이 헤더 계약 유지). 표면(T4)이 소비할 수명주기 계약.
//   jk::voice::turn::{RunVoiceTurn,ChatActionQuery} — STT 전사 문자 →
//     in-process ChatRouteTurn 직호출(스펙 §3 ③ 안 A — "STT 텍스트 == 타이핑
//     텍스트" — 신규 파서 0) + ChatAction→창 서버 도구 질의 양식.

#include <agent/JKLlmEngine.h>  // ChatConfig — turn 접점 배선 인자(cfg 선택형)
#include <apps/ChatRouter.h>   // ChatAction — 접점 도구 지시(라우터 단일 진실원)

#include <cstdint>
#include <cstdio>   // snprintf — JsonEscapeStr 쌍둥이 원문 수형
#include <cstdlib>  // getenv — WHISPER_ROOT 런타임 판정
#include <cstring>  // memcmp — RIFF/WAVE/fmt/data 매직 판별
#include <functional>
#include <string>
#include <vector>

// ---- stt — WHISPER_ROOT fail-closed 판정부 (#112 T1 — 스펙 §3 ②·§2.4·R1) ---
// main.cpp(jkdesktop)의 T1 수형에서 승계(T3 — 런타임 게이트 RunVoiceTurn과
// 표면 T4(jkapp_chat)가 같은 진실원을 먹는다 — reason 원문은 T1 리포트 §10의
// 4행 원문 그대로. 순수 함수 — 입력 3요소(env 유무·whisper-cli 존재·ggml 모델
// 존재)로 판정하고 스폰은 하지 않는다). selftest 2v-a는 이 승계판을 단정한다
// (어설션 원문 무변조 — 호출점 위치 승계만).
namespace jk::stt {
struct VoiceVerdict {
    bool enabled = false;
    const char* reason = "";  // 정직 표기 원문 — 표면이 그대로 인쇄한다
};
inline VoiceVerdict VoiceLegVerdict(
    bool envSet, bool binExists, bool modelExists) {
    if (!envSet) return {false, "구두 끔 (WHISPER_ROOT 미설정)"};
    if (!binExists) return {false, "구두 끔 (whisper-cli 부재)"};
    if (!modelExists) return {false, "구두 끔 (whisper 모델 부재)"};
    return {true, "구두 켬 (whisper-cli)"};
}

// envSet 입력의 런타임 원본 — WHISPER_ROOT env 유무(빈 값 미포함 — env 계약
// "설정"의 문자 그대로). verdict와 RunVoiceTurn 게이트가 같은 진실원.
inline bool VoiceWhisperRootSet() {
    const char* env = std::getenv("WHISPER_ROOT");
    return env != nullptr && *env != '\0';
}
}  // namespace jk::stt

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

// whisper-cli 바이너리 resolve의 표면(정적 수형의 공개 — T3 게이트가 verdict
// 입력(binExists)으로 같은 진실원을 먹는다 — 스폰 leg WhisperTranscribe 내부
// resolve와 복제 금지). found=false여도 bin은 "부재 원문 경로"(표기 소관).
std::string ResolveCliPath(bool* found);

// ggml 모델 리졸브 — exe 형제 dir의 stt-models/*.bin(조달기
// install_stt_models.sh 진실원 — Win build/stt-models·WSL buildwsl/·폰
// buildterm/ 계열이 exe 형제 dir 모양으로 수렴). 축별 파일명 편차(small/
// tiny-ko)는 dir 스캔(문자열 정렬 선두 .bin — 결정론)으로 흡수. STT_MODEL env
// = 지역 오버라이드(WHISPER_BIN과 같은 probe/현장 대응 몫). 부재 = 빈
// 문자열(verdict 입력 modelExists=false — "whisper 모델 부재"). 스폰 0.
std::string ResolveModelPath();

}  // namespace run

// ---- capture — push-to-talk 발화 수집 (#112 T3 — 스펙 §3 ① Win 축·⑤ 무음) ---
// Win SDL2 캡처 — **재생 장치와 별개 SDL 장치**(SDL_OpenAudioDevice iscapture=1
// — vplayer/JKSDLAudioBackend의 재생 장치와 독립 — WASAPI 공유 모드라 음악
// 재생 중 구두 조작 동시 성립). 요청 spec = 16 kHz/모노/s16le (SDL_AudioSpec
// 명시·allowed_changes=0 — SDL 변환 승인 계약; 기기 채널/샘플레이트를 그냥
// 먹고 리샘플 재합성하는 합성 금지 — 2v-⑥/⑦ 거부 규약의 사전 계약). 본체 =
// src/voice/VoiceCaptureSDL.cpp(SDL 형 수용 TU — 캡처 본체는 selftest 밖 —
// 실기기 probe/T6 몫). 실패(마이크 부재·장치 개방 실패·결실 0) = 정직 거부.
//
// 수명주기(push-to-talk) — 표면(T4) 소비 계약:
//   StartCapture(wavPath) → (발화) → StopCapture() → wavPath에 캐논 44B
//   RIFF/WAVE(16 kHz 모노 s16le) 착지 → jk::voice::turn::RunVoiceTurn(wavPath…)
//   소비. 상태 = IsCapturing()/GetCaptureState() — 표면 상태행 표기 원료.
//
// 무음 계약(스펙 §3 ⑤): 재생 계통(vplayer 조그 무음 원존) 무접촉 — 캡처는
// 재생 코드와 접점 0(별 장치·별 callback·별 버퍼).
namespace capture {

struct CaptureState {
    bool capturing = false;
    std::string wavPath;   // StopCapture가 쓰는 목표 경로(캡처 중 = 예약 경로)
    std::string lastError;  // 최근 정직 거부 원문(표면 표기 — 빈 = 무오류)
};

// 캡처 시작 — wavPath = 정지 시 WAV가 착지할 경로(호출측 임시 경로 진실원).
// 이미 진행 중 / SDL 개방 실패 / 얻은 spec이 계약(16k·모노·s16le) 비적합 =
// 정직 거부(false + err).
bool StartCapture(const std::string& wavPath, std::string& err);

// 캡처 정지 — 장치를 닫고 누적 PCM으로 캐논 WAV를 wavPath에 쓴다. 미진행 /
// 결실 0(빈 WAV는 whisper에 무의미) / 파일 쓰기 실패 = 정직 거부.
bool StopCapture(std::string& err);

bool IsCapturing();
CaptureState GetCaptureState();

}  // namespace capture

// ---- turn — STT 전사 문자 → ChatRouteTurn 접점 (#112 T3 — 스펙 §3 ③ 안 A) ----
// "STT 텍스트는 text와 완전 동형"(§2.1) — 전사 문자를 타이핑 텍스트와 같은
// 자리(서명 ChatRouteTurn)로 원문 전달한다. **in-process· IPC 0**(안 A —
// jkweb /talk 재용이 아닌 직호출), 신규 파서 0(정규화는 라우터
// NormalizeText·행동 파싱은 ChatLlmActionParse 소관). chat.json 미구성 =
// 라우터 즉발+stub 폴백 원존 — **"LLM이 아니어도 구두는 조작된다"**(네트워크
// 불요의 최후 방어 — §3 ③). 도구 지시(launch_app/close_window/focus_window/
// list_windows)는 기존 도구 경로 재용 — 지시 JSON 조립만 여기 단일화한다.
namespace turn {

// JSON 문자열 이스케이프 — EscapeJson 쌍둥이 원문 수형(ClientChatApp.cpp:33의
// 바이트 그대로 복제 — "쌍둥이 원문 수형" 반복 선례: MusicModel.h:487·
// MusicDirStore.h:316). 도구 질의 args의 app 인용 소관.
inline std::string JsonEscapeStr(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (const char ch : in) {
        if (ch == '"' || ch == '\\') { out += '\\'; out += ch; }
        else if (static_cast<unsigned char>(ch) < 0x20) {
            char num[8];
            std::snprintf(num, sizeof(num), "\\u%04x", ch);
            out += num;
        } else {
            out += ch;
        }
    }
    return out;
}

// ChatAction → (도구, 인자 JSON) — 창 서버 도구 질의 양식(ClientChatApp
// SendQuery·jkdesktop agentctl이 먹는 기존 질의 JSON과 같은 모양 — 재용).
// Close/Focus의 무앱(app=="") = argless "{}"(서버 포커스 창 판정 위임 —
// 라우터 1n-7 규약). Info = false(도구 지시 없음 — 앱이 질의를 보내지 않는다).
// app 인용 = ClientChatApp::EscapeJson 쌍둥이 원문 수형(순수 std — 복제
// 선례: MusicModel/MusicDirStore 쌍둥이).
bool ChatActionQuery(const ChatAction& a, std::string& tool,
                     std::string& args);

// 도구 지시 소비자(표면 소유 — "기존 도구 경로 재용"): 지시 1건을 표면의
// 질의 통로로 전달하고 성립하면 true. false = 질의 전달 실패(정직 표기 대상).
// 채팅 앱(SendQuery — id 0 = 실패)·jkdesktop(JKAgentClient)이 각자 자기
// 통로를 꽂는다 — 사슬이 소비자의 전송 형태를 강요하지 않는다.
using ToolDispatcher =
    std::function<bool(const std::string& tool, const std::string& args)>;

// 구두 턴 1건의 결과 — 표면(T4)이 기록/상태행에 그대로 쓰는 원문 모음.
struct VoiceTurnResult {
    bool ok = false;        // 전사+접점 전체 성립
    std::string sttText;    // 전사 원문(기록 [나] 항 — 정직 원문 문화 §3 ④)
    std::string guide;      // 접점 안내문([시스템] 항 — ChatRouteTurn 반환)
    std::string err;        // 실패 진단 원문(fail-closed — T2 reason 원문 승계)
    bool usedLlm = false;   // 시도 표지(1n-f14 관측 얼굴 승계 — 진단용)
};

// 사슬 본체 — 파일 기반 전사(push-to-talk 결과 WAV 경로) → 접점.
//   (1) fail-closed 게이트 — jk::stt::VoiceLegVerdict(env·cli·모델 3요소 —
//       리졸브 2종은 run 원존 재용) — !enabled = ok=false+err=reason 원문.
//   (2) wav 헤더 판정부 — wav::Validate16kMono 재용(캡처가 16k 모노 s16le로
//       SDL spec 강제했으므로 통과가 규약 — 비적합은 조용히 재합성하지 않고
//       정직 거부).
//   (3) 전사 — run::WhisperTranscribe(T2 스폰 원문 — 실패 err 원문 그대로
//       승계 — T4 표기 계약).
//   (4) 접점 — ChatRouteTurn(전사 원문, cfg) → ChatAction + guide.
//   (5) 도구 지시 — Info가 아니면 ChatActionQuery로 질의 조립해 dispatch에
//       전달(dispatch는 표면 소유 — 미지정 = 접점 단정까지, 질의 전달 생략).
// 순수 스폰 접점 — SDL 무접촉(캡처는 표면 사이 — §3 ⑤별 장치 계약)·selftest
// 단정은 (4)(5)만(전사 비결정 원칙 — 2v-d).
bool RunVoiceTurn(const std::string& wavPath, const agent::ChatConfig& cfg,
                  const ToolDispatcher& dispatch, VoiceTurnResult& out);

}  // namespace turn

}  // namespace voice
}  // namespace jk

#endif  // STTVOICE_H
