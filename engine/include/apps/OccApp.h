#ifndef OCCAPP_H
#define OCCAPP_H

// 원본 WINDBASE/2CAOCC의 C2 앱(OCCMAIN.CPP/OCCWIN.CPP)을 SDL2
// JKWindow 프레임워크로 단계적으로 포팅하기 위한 진입점.
// 데이터 모델과 UI는 OccUI(OccUI.h)의 단일 구현이 담당하고, 이 클래스는
// 단일 프로세스(SDL 앱) 셸이다. 서버 모드는 ClientOccApp 모듈을 쓴다.

#include <JKApplication.h>
#include <apps/OccUI.h>
#include <memory>

namespace jk {

// 2CAOCC 진입 앱: `jkdesktop.exe occ`로 실행.
class OccApp : public JKApplication {
public:
    OccApp();
    ~OccApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    bool PreProcessMessage(const JKEvent& ev) override;

private:
    std::unique_ptr<OccUI> ui_;
};

} // namespace jk

#endif // OCCAPP_H