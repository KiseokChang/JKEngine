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

#include <agent/JKLlmEngine.h>  // ChatConfig — 승격 배선(cfg 선택형)의 인자 원천

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

// ── 자연어 승격 배선 (스펙 2026-10-08-chat-llm-promotion 설계 결정 3·4 — T4) ──
// jkweb HandleTalk과 jktalk ProcessTurn의 "뇌호출 자리"(백엔드 슬롯 — 슬롯
// 주석 계약 이행)가 먹는 공통 본체. 복제 금지 계약: 두 소비 측은 이 함수
// 하나만 부르고, 프리앰블(kLlmTurnPreamble — JKLmEngine.cpp의 다중 파서 수리
// 계약 원문)은 엔진이 프롬프트에 한 번만 접두한다(BuildEngineCmd·
// BuildOllamaDirectCmd) — 이 계약 쪽은 프리앰블을 다시 적지 않는다.

// cfg.engine이 승격 배선을 켜는 구성 값인가: "ollama"|"claude"|"ollama-direct"
// (설계 결정 3). "stub"과 미지 값은 **미구성** — LLM 왕복 없이 기존 라우터
// 즉발 폴백(스포른 자체를 안 한다 — 폰·WSL 같이 엔진 없는 기기의 정직 지연 0).
bool ChatLlmEngineConfigured(const agent::ChatConfig& cfg);

// 비매치 발화를 LLM에 보낼 프롬프트 본문 — 트리거 표·앱 별명 표를
// kRouteTable/kAliasTable(ChatRouter.cpp 단일 진실원)에서 조립해 JSON 스키마
// 한 줄과 더한다(어휘 확장은 라우터 표 1행 추가만으로 이 프롬프트에도 자동
// 승계). 프리앰블 미포함 — 엔진 계약의 소관(위 주석).
// 본문 규제(실측 원장 — ChatRouter.cpp ActionSchemaLine): 개행·문자 "
// ·문자 | 를 담지 않는다 — win32 leg의 cmd 접두와 ollama leg의 재인용 층이
// 그 문자를 스스로의 명령 문법으로 먹는다(모델 **출력** JSON은 인용을 갖는다).
std::string ChatLlmTurnPrompt(const std::string& text);

// LLM 응답 텍스트 → ChatAction + 안내문 note 파싱. 방어 원료 = 실측 3회 재현
// (progress.md T1/T3 원장): 모델이 행동 JSON을 마크다운 코드펜스로 감싸거나
// 해설을 섞는다. 복호 절차: (a) 펜스 마커 행 제거 (b) 최초 `{` .. 마지막 `}`
// 절단(해설 헤지) → 완건 파서(AgentJson — quickjs)로 검증 → action/app/키
// 검증. 스키마 밖 값·launch 무app·JSON 부재는 false(정직 폴백 대상).
// note = 모델의 "text" 필드 + JSON 밖 해설 병기(안내문 원료). 성공했을 때
// out은 4종 지시 중 하나, 실패 시 out은 Info(app="") fail-closed.
bool ChatLlmActionParse(const std::string& llmText, ChatAction& out,
                        std::string& note);

// 뇌호출 1건의 전체 배선 — 스펙 설계 결정 4의 순서 계약을 한 곳에서 수행한다:
//   (1) ChatRouterRoute 정확 트리거 매치 → 기존 즉발 경로(LLM 왕복 0)
//   (2) 비매치 + cfg.engine 구성 → TurnSync 동기 턴 → 행동 JSON 파싱 →
//       ChatAction 구성, 모델 텍스트(부가 해설 포함)는 안내문으로 회신
//   (3) TurnSync 실패/파싱 불가 → 기존 stub 안내문 폴백 계약(①의 ChatRouterRoute
//       guide 원문 그대로 — 아래 기존 파이프라인의 결과물과 동일 모양)
// 반환 guide: 사용자에게 보여질 안내문(계약 — ChatRouterRoute 반환 문자열과
// 같은 자리). *usedLlm: LLM 턴이 실제로 수행됐으면 true(진단·셀프테스트
// 단정용 — "라우터 매치가 LLM을 우회한다"의 관측 얼굴).
//
// 엔진 수명 계약: 이 함수는 프로세스 수명의 정적 엔진 1개를 소유한다.
// TurnSync가 타임아웃으로 돌아와도 worker 턴은 돌아가고 busy_를 마주보므로
// 스택 인스턴스는 dangling을 낳는다(TurnJob은 engine의 멤버 atomic을 가리킨다
// — 1n-d16 refcount 계약의 상면). jkweb의 연결당 1스레드는 이 정적 엔진의
// "1턴 1인스턴스" 계약을 내부 락으로 직렬화해 지킨다(jktalk 단일 스레드 무영향).
// 세션 연속성(resumeSessionId)은 미사용 — 채팅 표면 턴은 전부 1회성 발화
 // (기존 jkbridge 세션 계약과 무관하게 유지한다).
std::string ChatRouteTurn(const std::string& text, ChatAction& out,
                          const agent::ChatConfig& cfg,
                          bool* usedLlm = nullptr);

} // namespace jk
#endif // JK_CHATROUTER_H
