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
// Kind==AppTool면 도구 지시 = app_tool 릴레이 질의 1건(#115b T1 — D2 재생
// 도메인: kRouteTable 행의 toolApp/toolName으로만 조립된다 — 새 파서 0) —
// 다만 발송 표면은 **구두(voice turn)뿐**이다(fix r1 — ChatAppToolTypingGuide).

#include <agent/JKLlmEngine.h>  // ChatConfig — 승격 배선(cfg 선택형)의 인자 원천

#include <string>

namespace jk {

struct ChatAction {
    // #118 T2 (스펙 2026-10-27-window-overlay-design D3): OverlayNumbers =
    // 서버 도구 overlay_numbers(창 번호 배지 켬 — 서버 원존 재용 — 새 도구 0)
    // 릴레이. **Info 뒤에 추가**(기존 kind 수치의 계보 보존 — 전사 표면 probe
    // 의 committed 원문 행이 kind 수치를 등호 판정한다 — AppTool=4·Info=5
    // 원존 유지, 2y-⑲).
    enum Kind { Launch, Close, Focus, ListWindows, AppTool, Info,
                OverlayNumbers };
    Kind kind = Info;   // 도구 지시 종류 — Info만 "지시 없음"의미.
    std::string app;    // Launch·Close의 앱 키(라이브러리 appName 규약 —
                        // alias표 해소). Close 미지정 = ""(포커스 창).
                        // Focus·ListWindows·Info는 항상 "".
    // #118 T2 (스펙 D3) — Focus 번호 화법("창 3"·"3번 창")의 배지 숫자:
    // 1기반(화면 배지 1..N = OverlayBadgeOrder 등호 — list_windows z등호 —
    // T1 원존). 0 = 미지정(기존 Focus 원존 — app 지명·argless 형) — 파싱 부품
    // ParseWindowNumberSpeech가 0을 반환하면 라우터가 Info(stub)로 떨어뜨린다
    // (D1 무인자 강제 정합 — "창 0"류 = 배지 부정 = 지시 불성립).
    int winNum = 0;
    std::string tool;   // #115b T1 — AppTool 전용: app_tool 릴레이 도구 이름
                        // (app도 AppTool에서는 앱 키가 아니라 **릴레이 대상
                        // 네임스페이스** — 도구 단일 진실원 kRouteTable 행의
                        // toolApp). 구두 경로 질의 조립 = ChatActionQuery
                        // (voice/SttVoice turn) 소관 — 도구 부재 앱·도구는
                        // 서버 unknown_app_tool로 정직 회신한다(fail-open).
                        // **소비자 소비 축(fix r1)**: 구두 경로 = 질의 전송.
                        // 타이핑·웹 경로(Submit·jkweb·jktalk) = 전송 없이
                        // ChatAppToolTypingGuide 정직 안내문만 — 전송 배선은
                        // 후속 태스크(부재 — 원장행).
};

// text(사용자 발화 한 줄)를 해석해 out에 도구 지시를 채우고, 화면 표시용
// 응답문을 돌려준다. 인식 불가·앱어 부재 등은 전부 Info — 항상 안내문 반환
// (오류 전파 경로 없음 — 채팅 앱이 그대로 인쇄하는 계약).
std::string ChatRouterRoute(const std::string& text, ChatAction& out);

// ── AppTool의 타이핑·웹 경로 소비 계약 (#115b T1 fix r1) ───────────────
// AppTool 지시를 소비할 표면이 구두(voice turn — ChatActionQuery→app_tool
// 릴레이)만 전송권을 갖는 동안, 타이핑·웹 경로(ClientChatApp::Submit·jkweb
// HandleTalk·jktalk ProcessTurn)는 **전송 없이** 이 공유 정직 문언을
// 회신한다. 복제 금지 계약 — 3 소비자가 같은 진실원을 먹는다(2y-⑰).
// 전송 배선(타이핑 표면 app_tool 송신)은 후속 태스크 미존재 — 원장행.
std::string ChatAppToolTypingGuide(const ChatAction& a);

// ── 자연어 승격 배선 (스펙 2026-10-08-chat-llm-promotion 설계 결정 3·4 — T4) ──
// jkweb HandleTalk과 jktalk ProcessTurn의 "뇌호출 자리"(백엔드 슬롯 — 슬롯
// 주석 계약 이행)가 먹는 공통 본체. 복제 금지 계약: 두 소비 측은 이 함수
// 하나만 부르고, 프리앰블(kLlmTurnPreamble — JKLlmEngine.cpp의 다중 파서 수리
// 계약 원문)은 엔진이 프롬프트에 한 번만 접두한다(BuildEngineCmd·
// BuildOllamaDirectCmd) — 이 계약 쪽은 프리앰블을 다시 적지 않는다.

// cfg가 승격 배선을 켜는 구성 값인가 (설계 결정 3 + T4 fix r1 NR4-1 룰링):
// **chat.json 파일이 실제 존재하고 engine 키가 명시**된 cfg(LoadChatConfig의
// fileKnown 표지)이면서, 그 engine 값이 "ollama"|"claude"|"ollama-direct"
// 중 하나. 파일 부재·파스 실패·engine 키 누락·"stub"·미지 값은 전부
// **미구성** — LLM 왕복 없이 기존 라우터 즉발 폴백(스폰 자체를 안 한다 —
// 승격은 opt-in 가법이고 폰·WSL 같이 chat.json이 없는 기기의 정직 지연 0).
// 기본값만으로 구성을 쳐주지 않는 이유: ChatConfig.engine의 구조 기본값은
// jkbridge/jkchat 원계약이 그대로기 때문(그 소비자는 이 게이트를 읽지 않는다).
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
