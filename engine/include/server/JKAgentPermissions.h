#ifndef JKAGENTPERMISSIONS_H
#define JKAGENTPERMISSIONS_H

// [P1 -- docs/108 3단계] 에이전트 권한 매트릭스 -- 허브와 서버 공유
// 단일 진실원(복본 복제 금지).
namespace jk {
namespace server {

struct AgentPermRow { const char* tool; const char* gate; const char* deflt; };
static const AgentPermRow kPermMatrix[] = {
    {"close_window", "server", "deny"},
    {"trust_request", "server", "ask"},
    {"run_console_app", "server", "ask"},
    {"trust_revoke", "server", "ask"},
    {"permission_set", "server(fixed)", "ask"},
    {"read_log", "broker", "allow"},
    {"read_events", "broker", "allow"},
    {"terminal_exec", "broker", "allow"},
    {"list_windows", "none", "allow"},
    {"focus_window", "none", "allow"},
    {"window_fullscreen", "none", "allow"},
    // 창 기하 2종 (스펙 2026-09-21-phone-practical-improvements): fullscreen
    // 과 같은 none/allow 분류 — 화면 상태 변경일 뿐 승인 행위가 아니다.
    {"window_move", "none", "allow"},
    {"window_resize", "none", "allow"},
    // 최소화/복원 (docs/57 §14.16) — 태스크바가 이미 하던 상태 변경의 에이전트
    // 노출이라 같은 none/allow 분류.
    {"window_minimize", "none", "allow"},
    // 창 번호 오버레이 스위치 (#118 T1 — 스펙 2026-10-27-window-overlay-design
    // D2): 10분 뒤 자동 소멸하는 일시 표지(배지)를 켜고 끄는 것뿐 — 창 상태에
    // 무접촉이고 승인 행위가 아니라 window_minimize와 같은 none/allow 분류.
    // HandleToolRegister의 namespace_conflict 가드가 이 표 전 행을 소비하므로
    // 이 행의 존재 자체가 앱측 "overlay_numbers" 등록도 자동 봉쇄한다.
    {"overlay_numbers", "none", "allow"},
    {"launch_app", "none", "allow"},
    {"save_layout", "none", "allow"},
    {"restore_layout", "none", "allow"},
    {"publish_event", "none", "allow"},
    // 캡처 쌍은 gate "server(flip)" (docs/54 §11 opus M2 픽스): 파일값이
    // 서버에서 강제되고, "ask"는 도구 거부(capture_ask) — 승인 파킹이
    // "allow"로 뒤집을 때까지. 기본값(파일 없음)은 allow 유지.
    {"capture_window", "server(flip)", "allow"},
    {"capture_region", "server(flip)", "allow"},
    // 폰 미러 (스펙 2026-09-27-phone-mirror): 캡처 쌍과 동일 server(flip)
    // 분류 — 디스크 기록 없는 JPEG b64 프레임이므로 캡처보다 위험이 낮다.
    // 파일 "ask"는 capture_ask 하드거부(승인 파킹 아님), "deny"는 전 거부.
    {"window_frame", "server(flip)", "allow"},
    {"trigger_toggle", "none", "allow"},
    {"theme_set", "none", "allow"},
    {"open_notify", "none", "allow"},
    {"launch_chat", "none", "allow"},
    {"approve", "none", "allow"},
    {"file_open", "none", "allow"},
    {"file_dialog_params", "none", "allow"},
    {"agent_permissions", "none", "allow"},
    {"installed_list", "none", "allow"},
    // 노트 허브 (스펙 2026-09-18-notes-hub §2.2): 저위험 사용자 데이터 —
    // 스팸 벡터는 receipts 감사 + rate limiter(docs/38).
    {"notes_read", "none", "allow"},
    {"notes_write", "none", "allow"},
    {"read_receipts", "none", "allow"},
    // 설정 허브 (스펙 2026-09-18-settings-hub §2.2): 외관·환경 설정 —
    // theme_set/trigger_toggle 분류. 단 settings_set의 capture_allow 키만
    // 키별 Ask(파킹) — 에이전트가 설정 도구로 권한을 넓히는 경로 봉쇄.
    {"settings_read", "none", "allow"},
    {"settings_set", "none", "allow"},
    // 파일 허브 (스펙 2026-09-18-file-hub §2.2): 최초의 파일 콘텐츠 도구 —
    // 게이트 "server(files)" (소스 분리: window 연결=allow, control-only=ask
    // 파킹 kind files_access; 명시 allow=에이전트 무승인, deny=전 소스 거부).
    // audit은 감사 열람이라 저위험 allow.
    {"files_list", "server(files)", "none"},
    {"files_read", "server(files)", "none"},
    // audit은 감사 열람이라 기본 allow — 단 파일값은 강제한다(opus 리뷰
    // MINOR-3): deny=거부, ask=files 도구와 동일 파킹(kind files_access).
    {"files_audit", "server(audit)", "allow"},
    // 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §4.3/§4.4): app_tool의
    // 실질 게이트는 3단 키(AppToolAllowed) — 이 행은 전역 기본값/매트릭스
    // 표기용. list_app_tools는 카탈로그 조회(none 등급).
    // ⚠ load-bearing(Task 2 리뷰): HandleToolRegister의 namespace_conflict는
    // 이 두 행으로 app="app_tool"/"list_app_tools" 등록을 자동 봉쇄한다.
    {"list_app_tools", "none", "allow"},
    {"app_tool", "server", "allow"},
    // 앱 정복 사다리 (스펙 2026-09-21-conquest-ladder §3.1): 트랙 B 조작
    // 수단 — 대상 창 합성 입력. ask 기본(2026-09-21 사용자 승인): 매 호출이
    // 승인 파킹으로 들어간다. 자동화 편의는 permissions.json에서 allow로.
    {"send_input", "server", "ask"},
    // 의미 커서 (스펙 2026-09-22-semantic-cursor §3): 합성 <app>.move/read/
    // act는 행 없음 — 앱 도구 허브 3단 키(app_tool.*)와 동일 분류. move/read는
    // 플랫폼 구현 무승인(window_move 분류), act는 앱 릴레이(커서 선언 앱 한정
    // 기본 ask — permissions.json의 app_tool.* 키가 이긴다).
};

}  // namespace server
}  // namespace jk

#endif  // JKAGENTPERMISSIONS_H
