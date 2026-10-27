#ifndef CLIENTCHATAPP_H
#define CLIENTCHATAPP_H

// 채팅 앱 클라 (스펙 2026-10-07-desktop-chat-app — ClientLibraryApp 패턴:
// 요청-응답 폴링, 이벤트 구독 없음). 뇌는 T1의 stub 라우터(jk::ChatRouterRoute,
// pure 룩업) — 앱은 발화 1줄을 받아 도구 지시로 번역해 SendAgentQuery로 보내고,
// 서버 답신 원문을 대화 기록에 그대로 인쇄한다.

#include <client/JKClientApplication.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>  // getenv — VOICE_TTS opt-out 런타임 원본(D2)
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace jk {

// ---- tts — 구두 응답 TTS 결정 부품 (#117 T1 — 스펙
// 2026-10-27-tts-response-design D2·D3·D4·D6) -------------------------------
// 헤더 인라인 순수 부품(GalleryModel.h/MusicModel.h 선례 — selftest 2z가 같은
// 진실원을 직소비 — 심볼 링크 없음·imgui/SDL 무접점). 스폰 본체(D5 실패 무시)는
// ClientChatApp.cpp의 스폰 leg TU — 폰 축 = __ANDROID__(#117 T1)·Win 축 =
// _WIN32(#120 T1 — D1 원장행 "Win/WSL 2축 바이너리에 스폰 leg 미포함"의 Win 몫
// 소각) — **WSL/posix 2축은 leg 부재 유지(끔 — 원존)**: WSLg 축은 폰(승인)
// 계약도 Win(내장 SAPI) 계약도 없어 v1에서 침묵이 정직한 유지 원존.
// Win 조달 = 조달 0(Windows 내장 powershell.exe+System.Speech — 시스템 음성
// SAPI. 리서치 원존 = powershell Add-Type System.Speech SpeechSynthesizer —
// 널리 문서화된 원존 조달, 신규 라이브러리 0).
namespace tts {

// D4 — [시스템] 답신 reply 문언 상한(1행 규약 — "~120자").
inline constexpr size_t kTtsReplyLimit = 120;
inline const char kTtsEllipsis[] = "…";

// D4 — 답신 절단 부품. 상한 초과 = 첫 120 **문자** 절취+말미 "…" 1자 접미.
// 문자 단위 = UTF-8 코드포인트 경계 보전(연속 바이트 0b10xxxxxx는 선행 바이트에
// 흡수 — 바이트 절단의 중간 글리프 파괴 없음). 상한 등호·이내 = 원문 그대로.
inline std::string TtsReplyClip(const std::string& reply) {
    size_t chars = 0;   // 소비한 코드포인트 수(정의상 상한을 넘지 않는다)
    size_t i = 0;
    bool over = false;  // 121번째 문자가 실제로 존재 = 절단 개입
    while (i < reply.size()) {
        if (chars == kTtsReplyLimit) {
            over = true;   // i부터 121번째 문자 — 여기서 끊는다
            break;
        }
        ++chars;
        ++i;
        while (i < reply.size() &&
               (static_cast<unsigned char>(reply[i]) & 0xC0) == 0x80)
            ++i;   // 연속 바이트 — 선행 문자 몫(중간 글리프 절단 없음)
    }
    if (!over) return reply;   // 121번째 문자 부재 — 원문 그대로
    return reply.substr(0, i) + kTtsEllipsis;
}

// D2·D3 — TTS 스폰 판정 부품(스폰 결정의 유일 관문 — 3진실원 합산). ①구두
// 출발 표지(turn.originVoice — StartVoice에서 새긴 턴 표지) ②답신 원문 재원
// (빈 답신 = 스킵) ③opt-out(VoiceTtsOptOutRuntime의 켬/끔). 타이핑 발화의
// 답신은 표지 0이라 구조적으로 부정된다(D3 무음 존중).
inline bool TtsSpeakWanted(bool turnOriginVoice, bool replyEmpty,
                           bool optOut) {
    return turnOriginVoice && !replyEmpty && !optOut;
}

// D2 — VOICE_TTS opt-out 판정부(2z ③ — 표의 유일 진실원). 기본 켬(답을 듣는다
// — UX 원칙)·설정+값 "0" 문자 1바이트 등호만 끔. 런타임 원본과 판정부가 같은
// 표를 먹는다(VoiceLegVerdict+VoiceWhisperRootSet 원존 수형).
inline bool VoiceTtsEnabled(bool envSet, const char* value) {
    return !(envSet && value != nullptr &&
             std::string(value) == "0");
}

// D2 — 런타임 원본 — getenv("VOICE_TTS") 1뒤. 스폰 leg만 소비(판정부 어셜션은
// 2z의 명시 입력판 — env 중독 없음). 스폰 leg = __ANDROID__·_WIN32 TU(#120 T1
// — Win 축도 같은 D2 게이트(기본 켬·VOICE_TTS=0 opt-out — 계약 승계·개판 0)).
inline bool VoiceTtsOptOutRuntime() {
    const char* v = std::getenv("VOICE_TTS");
    return !VoiceTtsEnabled(v != nullptr, v);
}

// ---- #120 T1 — Win 스폰 leg 명령 조립 부품(헤더 인라인 순수 — 2z-④ 직소비,
// 스폰 어셜션 금지 원존 승계: 조립식 등호만 단정하고 실 스폰은 어셜션 밖). ----

// PowerShell 단일 따옴표 문언 보전 — 단일 따옴표 1개 = '' 2개로 복각화(PS
// 인용 규약). 그 외 문자($·백틱·"·\)는 원문 그대로 — PS 단일 따옴표 문자열은
// 이스케이프 문언이 없다(복각화 1종뿐). 스폰이 sh를 거치지 않아 phone leg의
// $·백틱 백슬래시 이스케이프(TtsQuoted)는 Win 축 무접점(계열 계약만 수혀).
inline std::string TtsPsLiteral(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (const char ch : s) {
        if (ch == '\'') { out += "''"; }
        else { out += ch; }
    }
    return out;
}

// CreateProcessW 인자 이중 따옴표 인용 — CommandLineToArgvW 규약 수형(윈 원존
// 계약): 인자를 "…"로 감싸고, 인용 안 따옴표 = \"·직전 백슬래시 n개 = 2n+1개
// 복제·말미 백슬래시 n개(감는 따옴표 직전) = 2n개 복제. Win leg의 조달이
// cmd.exe를 거치지 않아 % 등 환경 확장 표면 0(Sh -c 래핑 밖 계약).
inline std::string TtsWinQuoted(const std::string& s) {
    std::string out = "\"";
    size_t bs = 0;   // 연속 백슬래시 개수(문자 경계에서 정산)
    for (const char ch : s) {
        if (ch == '\\') { ++bs; out += ch; }
        else if (ch == '"') {
            out.append(bs + 1, '\\');  // 직전 백슬래시 2배+인용 이스케이프 1개
            bs = 0;
            out += ch;
        } else {
            bs = 0;
            out += ch;
        }
    }
    if (bs) {  // 말미 백슬래시 — 감는 따옴표가 이스케이프되지 않게 2배
        out.resize(out.size() - bs);
        out.append(bs * 2, '\\');
    }
    out += '"';
    return out;
}

// #120 T1 — Win 조달 명령 1개 조립(스폰 leg의 유일 명령 원문). powershell.exe
// = Windows 내장(조달 0)·System.Speech = 시스템 음성 SAPI. cmd 조립식이 곧
// leg 원문 — 2z-④가 등호 단정(실 스폰 단정 = 어셜션 밖 — D6 승계).
inline std::string TtsWinSpeakCommand(const std::string& speakText) {
    const std::string ps =
        "Add-Type -AssemblyName System.Speech; "
        "$s=New-Object System.Speech.Synthesis.SpeechSynthesizer; "
        "$s.Speak('" + TtsPsLiteral(speakText) + "')";
    return "powershell.exe -NoProfile -Command " + TtsWinQuoted(ps);
}

}  // namespace tts

// 기록(상) + 입력(하) 2창 ImGui 클라. 기록은 Append-only(요청/응답 전부
// 인쇄 — 스펙 §1.1)라 별도 상태 모델 없이 벡터 1개가 진실원이다.
// #112 T4 — [구두] 표면: push-to-talk 수명주기(버튼 토글+핫키 hold)·전사 턴
// 은 비동기 워커 스레드(ClientMusicApp 스캔 워커 수형 — 파괴 join)로 돌고
// 결과는 OnIdle 드레인(UI 축 독점 계약 — Surface/SendQuery를 워커가 만지지
// 않는다).
class ClientChatApp : public JKClientApplication {
public:
    ClientChatApp() = default;
    ~ClientChatApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    void OnThemeChanged() override;   // ImGui 팔레트 재적용 (docs/52)
    bool PreProcessMessage(const JKEvent& ev) override;
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;
    void OnIdle() override;  // #89 T2 — 응답 폴백의 렌더 분리(수령 시 더티)

private:
    struct Turn {                     // 대화 기록 1행 — mine=사용자 발화
        bool mine = false;
        std::string text;
        // #117 T1 — D3: 구두 출발 발화 턴 표지(StartVoice 세션의 placeholder에
        // 새긴다 — FinishVoice push 접점 1곳). 이 표지 턴의 [시스템] 답신 도착
        // = TTS 스폰(DrainVoice 1곳). 타이핑 발화 턴은 기본값 false.
        bool originVoice = false;
    };

    void BuildUi(int w, int h);
    void Submit(const std::string& text);   // 발화 → 라우팅 → 도구 송신
    void PollReplies();                     // AgentQueryReply drain → 기록 반영
    uint32_t SendQuery(const char* tool, const std::string& args);

    // ---- 구두 (T4 — 스펙 §3 ④·R2·R5·⑧3/5) ---------------------------------
    // 표면 수명주기: [구두] 버튼(토글)+핫키 F6(hold — down=시작·up=마침).
    // StartVoice는 fail-closed 사전 판정(verdict 원문 — 마이크 낭비 전 정직
    // 거부)+StartCapture; FinishVoice는 StopCapture→"(전사 중…)" placeholder
    // 턴+워커 큐 적재 → 전사 도착 시 턴 치환(R5). 워커는 단일 상시 스레드
    // (ClientMusicApp ScanWorker 수형 — 우편함 1검·파괴 join·quit 경련).
    struct VoiceDone {                // 워커 → UI 축 수령 상자
        bool ok = false;
        std::string sttText;          // 전사 원문 — [나] 항(§3 ④ 정직 기록)
        std::string guide;            // 접점 안내문 — [시스템] 항
        std::string err;              // 실패 진단 원문(fail-closed 승계)
        bool launched = false;        // launch_app 지시 전달 — 앱 실행 토스트
        size_t slot = 0;              // turns_ 치환 슬롯(append-only — 안정)
    };
    void StartVoice();                // 사전판정+캡처 시작(정직 거부=상태행)
    void FinishVoice();               // 캡처 마침→placeholder+턴 큐(R5 치환)
    void DrainVoice();                // 워커 결과 기록+도구 질의 송신(OnIdle)
    void PublishNotify(const std::string& body);  // 토스트(publish_event 재용)
    void VoiceWorker();               // RunVoiceTurn 비동기 사슬 본체
    void StopVoiceWorker();           // 파괴 join 수형(quit+기상+join)

    std::thread voice_;                          // 단일 상시 구두 워커
    std::atomic<bool> voiceQuit_{ false };
    std::mutex voiceMtx_;
    std::condition_variable voiceCv_;
    struct VoiceJob { std::string wav; size_t slot = 0; };
    std::deque<VoiceJob> voiceJobs_;             // FIFO — 발화 순서 보존
    std::deque<VoiceDone> voiceDone_;            // 워커 결과(수령 대기)
    std::deque<std::pair<std::string, std::string>> voiceOutbox_;  // 도구 질의
    size_t voiceWavSeq_ = 0;                     // 임시 wav 결정론 접미
    std::string voiceStatus_;                    // 구두 상태행(R5 "전사 중…")
    bool voiceOriginPending_ = false;            // #117 D3 — 캡처 세션 표지
                                                 // (StartVoice 새김 →
                                                 // FinishVoice placeholder에
                                                 // 수납 — 1회 소모)
    uint32_t voiceToastId_ = 0x40000000;         // 토스트 id(기록 미반영 체 밖)

    std::vector<Turn> turns_;          // append-only 대화 진실원
    char input_[256] = {};             // 입력 버퍼(InputText 1행 — MVP 계약)
    std::string status_;               // 하단 상태 행(연결/송신 실패)
    bool frameDirty_ = true;
    bool imguiReady_ = false;
    bool koreanFont_ = false;
    bool scrollBottom_ = false;        // 기록 추가 프레임 — 맨 아래로 앵커
    uint32_t nextQueryId_ = 1;
    std::vector<uint32_t> pending_;    // AgentQueryReply 라운드트립 체
};

} // namespace jk
#endif // CLIENTCHATAPP_H
