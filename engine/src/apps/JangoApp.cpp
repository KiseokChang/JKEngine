#include <apps/JangoApp.h>

#include <apps/JangoUI.h>
#include <JKApplication.h>

#include <SDL.h>
#include <memory>

namespace jk {

// 단일 프로세스 셸: UI는 JangoUI(단일 구현)가 담당하고, 이 클래스는
// SDL 앱 수명과 Exit 버튼의 SDL_QUIT 전달만 처리한다.
JangoApp::JangoApp() = default;

JangoApp::~JangoApp() = default;

void JangoApp::OnInit() {
    ui_ = std::make_unique<JangoUI>([]() {
        SDL_Event quit;
        quit.type = SDL_QUIT;
        SDL_PushEvent(&quit);
    });
    ui_->BuildMainWindow();
    SetMainWindow(ui_->TakeMainWindow());
    ui_->CreateDialogs();
}

} // namespace jk