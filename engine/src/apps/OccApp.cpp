#include <apps/OccApp.h>

#include <JKApplication.h>
#include <JKEvent.h>

#include <SDL.h>
#include <memory>

namespace jk {

// 단일 프로세스 셸: 데이터/ UI는 OccUI(단일 구현)가 담당하고, 이 클래스는
// SDL 앱 수명, Exit 메뉴의 SDL_QUIT 전달, 30초 타이머 펌핑만 처리한다.
OccApp::OccApp() = default;

OccApp::~OccApp() {
    if (ui_) {
        ui_->SaveAll();
    }
}

void OccApp::OnInit() {
    ui_ = std::make_unique<OccUI>([]() {
        SDL_Event quit;
        quit.type = SDL_QUIT;
        SDL_PushEvent(&quit);
    });
    ui_->LoadAll();
    ui_->BuildMainWindow();
    SetMainWindow(ui_->TakeMainWindow());

    // 원본 SetTimer(30000)에 대응하는 1초 틱(상태줄 T+초 카운터).
    SetTimerInterval(1000);
}

void OccApp::OnClose() {
    if (ui_) {
        ui_->SaveAll();
    }
}

bool OccApp::PreProcessMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::Timer && ui_) {
        ui_->OnTimerTick();
    }
    return JKApplication::PreProcessMessage(ev);
}

} // namespace jk