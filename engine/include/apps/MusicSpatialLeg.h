#ifndef MUSICSPATIALLEG_H
#define MUSICSPATIALLEG_H

// Music spatial leg pure parts (spec 2026-10-10-music-spatial-leg-design, T1):
// nothing here opens a device or decodes audio — this is the decision layer
// the T2 module (ClientMusicApp) consumes on top of spatial-player's
// audio_core (mp3/flac/ogg/wav 스트리밍 + POSITIONAL HRTF). The
// spatial-player repository itself is never modified (consumer contract —
// docs/superpowers/specs/2026-10-10 §산전 전제): JKEngine CMake wires it via
// the SPATIAL_PLAYER_ROOT environment variable only, and **the path is never
// written to any source/commit** (IP 레포 규약 — env-only 계약).
//
// Header-inline on purpose — MusicModel.h 쌍둥이 수형: the selftest twins
// (engine/src/main.cpp selftest 2n) assert these parts by direct link with no
// extra TU, and the T2 module consumes the same functions. This header must
// stay free of imgui/SDL/client types **and free of audio_core/OpenAL
// includes** — the leg header must compile in the env-unset build too
// (이 헤더는 fail-closed 배선의 미설정 축에서도 전부 컴파일된다; audio_core
// 소비는 T2 소스 쪽의 JK_MUSIC_SPATIAL_LEG 매크로 가드 아래에만 산다).
//
// Fail-closed contract (스펙 D2): env 미설정 빌드에서 leg는 컴파일 자체가
// 생략된다 — 이 헤더의 판정 부품(LegSupport)은 그래도 정직하게
// 전원 Unsupported 모양의 데이터를 내보낸다(표기만 남는다).
//
// 원존 계약(스펙 D3): 더블클릭=vplayer 위임(music::OpenRequestJson)은 무변조
// 유지 — spatial leg는 행의 [spatial] 버튼이 차별화해 발사하는 제2 채널이다.
// 이 헤더는 그 대상(Track)을 소유하지 않는다(경로 문자열판만 — Track 변환은
// 호출부 몫).

#include <port/JKCrtShim.h>  // Stricmp — Win/posix 공용 ASCII 대소문자 무시 비교

#include <cstdio>  // ToolJson의 snprintf — 2m-g OpenRequestJsonPath 수형 동원
#include <filesystem>
#include <string>

namespace jk {
namespace music {
namespace leg {

// ---- 지원 포맷 판정 (D4 표 — 정직 문화) ----

// audio_core가 디코드하는 포맷의 승계표(스펙 D4): 지원=mp3/flac/ogg/vorbis/wav
// (5종 — Decoder::open 포맷 판별 원문 계열), 미지원=m4a/aac/wma(3종 — 조달/
// 라이선스 비-목표 §5). 지원 행만 [spatial] 버튼이 활성되고, 미지원 행은
// 회색+툴팁 "미지원 포맷"(D4 정직 계약 — 지원 안 하는 걸 지원이라 하지 않는다).
// 판정은 **경로 전체**를 받아 말단 확장자로 접는다 — 호출부가 Track.full을
// 그대로 건네도 된다(도트 포함 확장자 추출은 fs::path::extension() 원문
// 계약 — 음악 스캔 열거(IsAudioExtension)와 같은 형식의 입력).
struct LegSupport {
    enum class State { Unsupported, Supported };

    // 말단 확장자와 D4 표의 대조. 확장자는 대소문자 무시(ASCII fold —
    // Stricmp 원문: 한글·멀티바이트는 이진 비교로 촉점 없음). 무확장자·
    // 숨김 파일(".mp3" 도트포함 항목 — extension()는 공문자를 돌려준다)·
    // 빈 문자열·표 밖 확장자는 전부 Unsupported(보수 파 — 미지원 표기가
    // 정직이고, 지원으로 놓치는 실수는 버튼이 사는 결함이 된다).
    static State OfExt(const std::string& path) {
        const std::string ext =
            std::filesystem::path(path).extension().string();
        return jk::crt::Stricmp(ext.c_str(), ".mp3") == 0 ||
                       jk::crt::Stricmp(ext.c_str(), ".flac") == 0 ||
                       jk::crt::Stricmp(ext.c_str(), ".ogg") == 0 ||
                       jk::crt::Stricmp(ext.c_str(), ".vorbis") == 0 ||
                       jk::crt::Stricmp(ext.c_str(), ".wav") == 0
                   ? State::Supported
                   : State::Unsupported;
    }
};

// ---- leg 상태 머신 (스펙 §2 Idles→Playing→Stopped의 순수 전이판) ----

// leg에 투입되는 사건 4종 — T2 모듈이 디바이스/도구 경로에서 관측한 사실만
// 이 열거로 번역해 Apply에 넣는다(전이는 이 헤더가 단독 소유 — 디바이스 호출
// 금지 계약).
enum class LegEvent { Start, StopRequested, DeviceFailed, Eos };

// leg의 관측 상태(T2 UI의 유일 진실원 — 위 필드별 소비 계약은 리포트 §3):
//   active     - 재생 중 플래그(진행 표기 갱신 더티의 유일 근거 — idle 계약
//                #89 준수의 전제. false면 pump 폴링도 무더티)
//   positional - POSITIONAL(모노 다운믹스+HRTF 위치)/STEREO 토글(스펙 §2 —
//                기본 true=POSITIONAL)
//   deviceOk   - ALC 디바이스 성립 플래그(D5 fail-closed 표기의 근거 —
//                false면 상태 행에 안내를 그린다)
//   err        - 직전 고장 원문(DeviceFailed가 쓰는 안내 원문 — UI 라벨 소비)
//   path       - 재생 중/직후 트랙 경로(상태 행 표기)
//   posSec     - 현재 재생 위치(초 — pump가 갱신하는 수치의 자리)
//   durSec     - 트랙 전체 길이(초 — seek/Eos 판정의 자리)
struct LegState {
    bool active = false;
    bool positional = true;
    bool deviceOk = false;
    std::string err;
    std::string path;
    double posSec = 0;
    double durSec = 0;
};

// DeviceFailed가 기록하고 상태 행이 그리는 폴백 안내 원문(스펙 §2 "ALC 실패
// 시 폴백 안내" + §5 EYES 전의 정직 표기 계약). vplayer 위임은 양쪽 재생
// 오디오가 OS 믹서에서 합산된다(스펙 §2 충돌 원장 — 결함 아님)라 leg 실패
// 사용자의 정상 종착지는 vplayer 위임 채널이다.
inline constexpr char kDelegationHint[] =
    "spatial leg 불가 — vplayer 위임 이용";

// 순수 전이 (selftest 2n-b): 인자 상태를 고치지 않고(값 반환이라 원본 보존 —
// purity 단정 축) 사건 1건만 적용한 다음 상태를 돌려준다. 디바이스/스레드/
// 파일 I/O 접촉 0건 — T2가 사건의 사실원이다(이 함수가 관측도 주장도 않는다).
// 전이 명세(리포트 §3에 봉합):
//   Start         - 재생 개시: active=true·deviceOk=true로 고정(호출부가
//                   디바이스 열기에 성공한 뒤에만 Start를 넣는다 — 전제),
//                   posSec=0·err 공문자(이전 고장 표기 소멸)·path 무변조
//                   (호출부가 미리 채워 넣는다). duration 반영은 Eos/pump 몫.
//   StopRequested - 정지 성공 종착: active=false만 — deviceOk·posSec(최후
//                   위치 표기)·path·err 보존(정지는 고장이 아니다).
//   DeviceFailed  - 고장 종착: active=false·deviceOk=false·err=kDelegationHint
//                   (UI는 이 문자열을 그대로 라벨로 소비 — vplayer 위임 안내).
//                   pos/dur/path 보존(고장 지점의 기록 — 다음 Start가 지운다).
//   Eos           - 자연 종료 종착: active=false·posSec=durSec(진행 표기를
//                   끝까지 채운다)·deviceOk 보존(디바이스는 살아 있다 —
//                   다음 Start가 다시 쓴다)·err 보존.
inline LegState Apply(const LegState& state, LegEvent event) {
    LegState out = state;  // 값반환 — 원본 무변조(purity 계약)
    switch (event) {
        case LegEvent::Start:
            out.active = true;
            out.deviceOk = true;  // Start의 전제 — 디바이스 연 뒤에만 발사
            out.posSec = 0;       // 새 재생 — 이전 재생 위치 소멸
            out.err.clear();      // 이전 고장 표기 소멸
            break;
        case LegEvent::StopRequested:
            out.active = false;   // 정지 = 성공 종착(deviceOk·pos 보존)
            break;
        case LegEvent::DeviceFailed:
            out.active = false;
            out.deviceOk = false;
            out.err = kDelegationHint;
            break;
        case LegEvent::Eos:
            out.active = false;
            out.posSec = out.durSec;  // 진행 표기 끝까지(자연 종료 원문)
            break;
    }
    return out;
}

// ---- app 도구 허브 요청 원문 (2n-c — 2m-g OpenRequestJson 쌍둥이) ----

// spatial leg가 등록하는 app 도구 3종의 이름 원문(스펙 §2 — probe/구두 조작
// 표면). T2의 SendAgentToolRegister와 ToolJson 소비가 같은 상수를 쓴다.
inline constexpr char kToolPlay[] = "spatial_play";
inline constexpr char kToolStop[] = "spatial_stop";
inline constexpr char kToolStatus[] = "spatial_status";

// app 도구 허브 요청 전문 조립(selftest 2n-c): music 앱으로 향하므로
// {"app":"music", ...} — 2m-g OpenRequestJson의 {"app":"vplayer", ...}와
// 쌍둥이 형식. spatial_play는 args에 {"path":"<path>"}, stop/status는
// args {} (무인자 — path 인자 무시·기본 ""). path 이스케이프는 2m-g 원문
// 수형 전문: '\'→'/' 정규화(Windows 수형 경로가 JSON 이스케이프로 쌓이지
// 않고 폰/WSL '/' 표기와 동형)·따옴표 \·제어 문자 \uXXXX·공백 원문 수용.
inline std::string ToolJson(const std::string& tool,
                            const std::string& path = "") {
    std::string out;
    out.reserve(40 + tool.size() + path.size());
    out += "{\"app\":\"music\",\"tool\":\"";
    out += tool;
    out += "\",\"args\":";
    if (path.empty()) {
        out += "{}";                       // 무인자(stop/status) 원문
    } else {
        out += "{\"path\":\"";
        for (const char ch : path) {
            const unsigned char c = static_cast<unsigned char>(ch);
            if (ch == '\\') {
                out += '/';                    // 슬래시 정규화(2m-g 원문 수형)
            } else if (ch == '"') {
                out += "\\\"";
            } else if (c < 0x20) {
                char num[8];
                std::snprintf(num, sizeof(num), "\\u%04x",
                              static_cast<int>(c));
                out += num;
            } else {
                out += ch;                     // 공백 포함 원문 수용
            }
        }
        out += "\"}";                          // path+args 닫기
    }
    out += "}";            // 전문 닫기(2m-g 쌍둥이 — 괄호 쌍 소각)
    return out;
}

} // namespace leg
} // namespace music
} // namespace jk

#endif // MUSICSPATIALLEG_H