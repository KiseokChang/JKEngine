#ifndef APPS_RECOGVIEWS_H
#define APPS_RECOGVIEWS_H

#include <memory>

namespace jk {

class JKWindow;

// Stroke Recognition UI 빌더(TetrisGameWindow와 동일한 재사용 패턴).
// 단일 프로세스 RecogApp과 클라이언트 모듈(ClientRecogApp)이 같은 컨트롤
// 구성 코드와 스트로크 처리 상태를 공유한다.
//
// Build()는 입력 보드({20,320,1880,1060}), Clear/Recognize 버튼, 스트로크/
// 문자 리스트를 `parent`에 구성하고 입력 보드에 포커스를 준다. 콜백이
// Build 이후에도 내부 상태를 참조하므로 RecogUI는 이를 소유한 앱과 동일한
// 수명을 가져야 한다(멤버 unique_ptr로 보관).
class RecogUI {
public:
    RecogUI();
    ~RecogUI();

    void Build(JKWindow* parent);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace jk

#endif // APPS_RECOGVIEWS_H