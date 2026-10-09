#ifndef CLIENTCHATAPP_H
#define CLIENTCHATAPP_H

// 채팅 앱 클라 (스펙 2026-10-07-desktop-chat-app — ClientLibraryApp 패턴:
// 요청-응답 폴링, 이벤트 구독 없음). 뇌는 T1의 stub 라우터(jk::ChatRouterRoute,
// pure 룩업) — 앱은 발화 1줄을 받아 도구 지시로 번역해 SendAgentQuery로 보내고,
// 서버 답신 원문을 대화 기록에 그대로 인쇄한다.

#include <client/JKClientApplication.h>

#include <cstdint>
#include <string>
#include <vector>

namespace jk {

// 기록(상) + 입력(하) 2창 ImGui 클라. 기록은 Append-only(요청/응답 전부
// 인쇄 — 스펙 §1.1)라 별도 상태 모델 없이 벡터 1개가 진실원이다.
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
