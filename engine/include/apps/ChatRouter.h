#ifndef JK_CHATROUTER_H
#define JK_CHATROUTER_H

// 채팅 명령 라우터 (스펙 2026-10-07-desktop-chat-app §1.3 — stub 턴 백엔드의
// 뇌). 한국어 발화("지뢰찾기 켜줘"/"창 목록" 등)를 도구 지시(launch_app /
// focus_window / …)로 번역하는 pure 룩업 — 창·imgui 무접촉.
//
// 소비자 3좌(앱 라이브러리 single-truth 선례 승계): jkapp_chat stub 백엔드,
// CLI 진단(jktalk --route), 셀프테스트(케이스 1n). 어휘 표는 구현 파일 상단 정적 배열
// 단일 진실원 — 어휘 확장은 표 1행 추가로 끝난다(스펙 "확장 열린 패턴
// 테이블" 계약).
//
// ChatRouterRoute 반환 문자열은 도구 지시가 아니라 **사용자에게 보여질**
// 안내/확인문(한국어 UTF-8) — 채팅 앱이 대화 기록에 그대로 인쇄한다.
// Kind==Info면 도구 지시 없음(앱이 SendAgentQuery를 보내지 않는다).

#include <string>

namespace jk {

struct ChatAction {
    enum Kind { Launch, Close, Focus, ListWindows, Info };
    Kind kind = Info;   // 도구 지시 종류 — Info만 "지시 없음"의미.
    std::string app;    // Launch·Close의 앱 키(라이브러리 appName 규약 —
                        // alias표 해소). Close 미지정 = ""(포커스 창).
                        // Focus·ListWindows·Info는 항상 "".
};

// text(사용자 발화 한 줄)를 해석해 out에 도구 지시를 채우고, 화면 표시용
// 응답문을 돌려준다. 인식 불가·앱어 부재 등은 전부 Info — 항상 안내문 반환
// (오류 전파 경로 없음 — 채팅 앱이 그대로 인쇄하는 계약).
std::string ChatRouterRoute(const std::string& text, ChatAction& out);

} // namespace jk
#endif // JK_CHATROUTER_H
