#ifndef APPS_ICONEDITVIEWS_H
#define APPS_ICONEDITVIEWS_H

#include <memory>

namespace jk {

class JKWindow;

// Icon Editor UI 빌더(TetrisGameWindow와 동일한 재사용 패턴).
// 단일 프로세스 IconEditApp과 클라이언트 모듈(ClientIconEditApp)이 같은
// 컨트롤 구성 코드와 스프라이트/색상 상태를 공유한다.
//
// Build()는 프리뷰 보드, 파일/이름 입력, Color/Clear/Save/Load 버튼,
// 픽셀 보드({20,120,1900,1060})를 `parent`에 구성하고 픽셀 보드에 포커스를
// 준다. Save/Load/Color 콜백이 Build 이후에도 내부 Sprite 상태를 참조하므로
// IconEditUI는 이를 소유한 앱과 동일한 수명을 가져야 한다(멤버
// unique_ptr로 보관).
class IconEditUI {
public:
    IconEditUI();
    ~IconEditUI();

    void Build(JKWindow* parent);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace jk

#endif // APPS_ICONEDITVIEWS_H