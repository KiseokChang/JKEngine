#include <apps/ClientVectorFontApp.h>

#include <apps/VectorViews.h>
#include <JKWindow.h>

namespace jk {

ClientVectorFontApp::ClientVectorFontApp() = default;
ClientVectorFontApp::~ClientVectorFontApp() = default;

void ClientVectorFontApp::OnInit() {
    vfont_ = LoadVectorAppFonts("ClientVectorFontApp");

    auto main = std::make_unique<JKWindow>("Vector Font");
    main->SetWindowRect(JKRect{ 0, 0, 1920, 1080 });

    // VectorFontWindow는 단일 프로세스에서 떠 있는 메인 윈도우(자체 타이틀바 +
    // 이동/리사이즈 속성)로 태어난다. 서버 모드에서는 크롬이 서버 소유이므로
    // ClientTetrisApp처럼 벗겨내고 루트 클라이언트 영역에 도크필한다.
    // SetAttrFlags 이후 SetWindowRect를 다시 불러야 클라이언트 영역이 전체
    // rect로 재계산된다.
    const JKRect clientArea = main->GetClientRect();
    auto fontWindow = std::make_unique<VectorFontWindow>(vfont_.get());
    fontWindow->SetAttrFlags(WA_CHROMELESS);
    fontWindow->SetWindowRect(JKRect{ 0, 0, clientArea.w, clientArea.h });
    fontWindow->SetDock(DOCK_FILL);

    VectorFontWindow* fontWin = fontWindow.get();
    main->AddControl(std::move(fontWindow));

    // 루트 윈도우는 키 입력을 focusChild_로만 전달하므로 포커스를 미리 준다.
    // 이것이 없으면 첫 클릭 전까지 방향키(글리프 크기 조절)가 먹지 않는다.
    // SetFocus는 최상위 조상 윈도우(루트)의 focusChild_를 설정한다.
    fontWin->SetFocus();

    SetMainWindow(std::move(main));

    // 타이머 없음: 단일 프로세스 VectorFontApp도 Timer 이벤트를 쓰지 않는다.
}

} // namespace jk