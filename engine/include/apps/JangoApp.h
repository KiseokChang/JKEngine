#ifndef JANGOAPP_H_SDL2
#define JANGOAPP_H_SDL2

#include <JKApplication.h>
#include <memory>
#include <string>

namespace jk {

class JangoUI;

// 단일 프로세스 JANGO 런처 셸. UI 로직은 JangoUI(JangoUI.h)가 단일 구현으로
// 담당하고, 서버 모드 클라이언트 모듈(ClientJangoApp)도 같은 클래스를 쓴다.
class JangoApp : public JKApplication {
public:
    JangoApp();
    ~JangoApp() override;

protected:
    void OnInit() override;

private:
    std::unique_ptr<JangoUI> ui_;
};

} // namespace jk

#endif // JANGOAPP_H_SDL2
