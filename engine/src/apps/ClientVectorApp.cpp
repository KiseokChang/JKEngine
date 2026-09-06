#include <apps/ClientVectorApp.h>

#include <apps/VectorViews.h>
#include <JKWindow.h>

namespace jk {

ClientVectorApp::ClientVectorApp() = default;
ClientVectorApp::~ClientVectorApp() = default;

void ClientVectorApp::OnInit() {
    auto main = std::make_unique<JKWindow>("Vector Editor");
    main->SetWindowRect(JKRect{ 0, 0, 1920, 1080 });

    // VectorView와 버튼 3개는 JKControl이므로 크롬 제거 대상이 아니다.
    // 단일 프로세스 VectorApp::Init와 동일한 빌더를 루트 클라이언트 영역에
    // 그대로 구성한다(루트가 프레임 크롬을 그리고 서버가 닫기 버튼을 오버레이).
    BuildVectorEditorUi(main.get());

    SetMainWindow(std::move(main));
}

} // namespace jk