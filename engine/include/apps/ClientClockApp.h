#ifndef CLIENTCLOCKAPP_H
#define CLIENTCLOCKAPP_H

// 시계 앱 (2026-11-16 사용자 판정: "트레이 시계를 클릭하면 뜨는 앱").
// 데스크톱 우상단 대형 시계 위젯을 없애고, 시간 표시를 **앱 창 하나**로 모은다.
// 서버 도구 접촉 0(순수 클라이언트): 시간·날짜·경과 초만 그린다.
//
// 표면 계약: 16ms 틱은 배송 채널일 뿐(#89 T1/T2), 실제 재그리기는 1초 경계에서만
// IsFrameDirty를 세운다(정지 화면에서 프레임 낭비 없음).

#include <client/JKClientApplication.h>

namespace jk {

class ClientClockApp : public JKClientApplication {
public:
    ClientClockApp() = default;
    ~ClientClockApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    void OnThemeChanged() override;                 // ImGui 팔레트 재적용
    bool PreProcessMessage(const JKEvent& ev) override;
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;

private:
    void BuildUi(int w, int h);

    bool imguiReady_ = false;
    bool frameDirty_ = true;
    long long lastSecond_ = -1;   // 1초 경계 판정
};

} // namespace jk

#endif  // CLIENTCLOCKAPP_H
