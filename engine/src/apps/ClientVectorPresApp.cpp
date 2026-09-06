#include <apps/ClientVectorPresApp.h>

#include <apps/VectorViews.h>
#include <JKWindow.h>

namespace jk {

ClientVectorPresApp::ClientVectorPresApp() = default;
ClientVectorPresApp::~ClientVectorPresApp() = default;

void ClientVectorPresApp::OnInit() {
    vfont_ = LoadVectorAppFonts("ClientVectorPresApp");

    auto main = std::make_unique<JKWindow>("Presentation");
    main->SetWindowRect(JKRect{ 0, 0, 1920, 1080 });

    // PresentWindow도 ClientTetrisApp처럼 크롬을 벗겨내고 루트 클라이언트
    // 영역에 도크필한다(서버가 타이틀/닫기/리사이즈 크롬을 소유).
    const JKRect clientArea = main->GetClientRect();
    auto presWindow = std::make_unique<PresentWindow>(vfont_.get());
    presWindow->SetAttrFlags(WA_CHROMELESS);
    presWindow->SetWindowRect(JKRect{ 0, 0, clientArea.w, clientArea.h });
    presWindow->SetDock(DOCK_FILL);
    main->AddControl(std::move(presWindow));

    SetMainWindow(std::move(main));

    // 단일 프로세스 VectorPresApp와 동일한 500ms 타이머. Timer 이벤트는 루트
    // 윈도우가 자식들에게 브로드캐스트하므로(ClientTestWindowApp와 동일)
    // PresentWindow::RespondMessage의 JKEventType::Timer 분기가 그대로 동작한다.
    SetTimerInterval(500);
}

} // namespace jk