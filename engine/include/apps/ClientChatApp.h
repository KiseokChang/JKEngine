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
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace jk {

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
