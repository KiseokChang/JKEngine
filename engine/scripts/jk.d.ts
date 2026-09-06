// jk.d.ts v4 — host API contract (docs/27_scripting_quickjs_bridge.md §4).
// v1: 단계 1 최소 증명 세트 / v2: 단계 2 UI 자동화 API (findControl, click,
// injectMouse, injectKey, assert, assertEq — additive) / v3: 단계 3 모달
// 다이얼로그 (createDialog, dialogAdd*, dialogShow, dialogClose — additive) /
// v4: 단계 4 설정 주입 (readConfig).
//
// 이 파일은 실행되지 않는 TypeScript 선언 파일이다. QuickJS-ng가 실행하는
// 것은 app.js(JavaScript)이며, 이 선언은 (1) 스크립트를 작성하는 에이전트가
// 가장 먼저 읽는 API 레퍼런스이자 (2) 에디터 intelliSense와
// `npx tsc --noEmit --checkJs` 실행 전 검증의 기준이다 (node 툴체인이 없어도
// 계약 문서로 단독 유효).
//
// 운영 규칙 (docs/27 §2.4):
//  - 확장(additive)만 허용 — 제거/시그니처 변경 금지. 교체가 필요하면
//    @deprecated 마킹 후 구버전 병행, 제거는 마일스톤에서.
//  - 바인딩 추가 커밋에는 반드시 이 파일의 갱신을 동봉한다.
//  - 셀프테스트가 실제 바인딩 이름(Object.getOwnPropertyNames(globalThis)
//    열람)과 아래 선언 이름을 대조한다 — 존재성 드리프트를 잡는 기계 검증.
//  - 모든 선언에 JSDoc을 쓴다. 이 파일이 곧 API 레퍼런스다.

/**
 * 컨트롤 배치 사각형. 좌표는 앱 패널 클라이언트 영역(타이틀 바 아래) 기준
 * 픽셀이다.
 */
interface JKRect {
    x: number;
    y: number;
    w: number;
    h: number;
}

/**
 * 표준 출력으로 한 줄 로그를 남긴다. `[script] ` 접두어가 붙어 콘솔에
 * 나타난다. 디버깅/자기 보고 용도.
 */
declare function log(text: string): void;

/**
 * 모달 메시지 박스(OK 버튼)를 연다. 호출은 즉시 반환되고 박스는 닫힐 때까지
 * 앱 입력을 가로챈다(비동기 모달 — JS 실행을 막지 않는다).
 */
declare function messageBox(title: string, text: string): void;

/**
 * 버튼을 만들어 controlId를 반환한다. 클릭마다 전역 onClick(controlId)가
 * 호출된다. id를 생략하면 1000부터 자동 배정(명시 id가 이미 있으면
 * 충돌을 피해 증가).
 */
declare function createButton(rect: JKRect, text: string, id?: number): number;

/** 정적 라벨을 만들어 controlId를 반환한다. */
declare function createLabel(rect: JKRect, text: string, id?: number): number;

/** 한 줄 입력란을 만들어 controlId를 반환한다. */
declare function createEdit(rect: JKRect, text: string, id?: number): number;

/** controlId 컨트롤의 텍스트를 바꾼다. */
declare function setText(controlId: number, text: string): void;

/** controlId 컨트롤의 현재 텍스트를 읽는다(없으면 빈 문자열). */
declare function getText(controlId: number): string;

/**
 * `ms`마다 fn을 호출하는 반복 타이머를 걸고 타이머 id를 반환한다.
 * 정확도 보장 없음 — UI 이벤트 루프 뒷편에서 실행된다.
 *
 * @note [AI Agent] 콜백은 UI 메인 스레드에서 실행된다. 수백 ms를 넘기는
 *   작업(연산 루프, 동기 I/O 흉내)을 금지 — 화면과 입력이 멈춘다. UI
 *   오케스트레이션(텍스트 갱신, 상태 폴링 수준)만 수행할 것.
 */
declare function setInterval(ms: number, fn: () => void): number;

/** setInterval로 건 타이머를 해제한다. 이미 해제된 id는 무시된다. */
declare function clearInterval(timerId: number): void;

// ---------------------------------------------------------------------------
// v2 — UI 자동화 (docs/27 §4 단계 2). 시나리오 스크립트가 좌표 probe를 대체한다:
// 컨트롤 핸들로 직접 구동하고(assert), 실제 입력 경로도 주입할 수 있다(inject*).
// ---------------------------------------------------------------------------

/**
 * 컨트롤을 찾아 controlId를 반환한다 (못 찾으면 null).
 * - 숫자: controlId로 첨부된 윈도우 트리 전체에서 탐색 — 스크립트가 만든
 *   컨트롤뿐 아니라 호스트 윈도우의 기존 컨트롤도 찾는다.
 * - 문자열: 표시 텍스트(GetText)로 트리를 깊이 우선 탐색 — probe가 화면
 *   좌표로 찾던 컨트롤을 라벨로 찾는 용도.
 */
declare function findControl(idOrText: number | string): number | null;

/**
 * controlId 컨트롤을 클릭한다 — 버튼의 OnClick을 직접 호출(구조적 클릭).
 * 스크립트가 만든 버튼이면 전역 onClick(controlId)로 이어진다. 버튼이 아닌
 * 컨트롤이면 로그를 남기고 무시된다.
 */
declare function click(controlId: number): void;

/**
 * (x, y)에 마우스 다운/업을 주입한다. 패널 클라이언트 픽셀 좌표
 * (createButton rect와 같은 좌표계). RespondMessage 라우팅 — 히트테스트와
 * 컨트롤 핸들러가 실제 입력과 동일하게 동작한다.
 */
declare function injectMouse(x: number, y: number): void;

/**
 * keyCode 키 다운/업을 주입한다. 포커스를 가진 컨트롤로 전달된다.
 * 키 코드 규약은 엔진의 JKEvent.keyCode와 동일.
 */
declare function injectKey(keyCode: number): void;

/**
 * cond가 아니면 실패를 기록한다 — `[script] ASSERT FAIL` 로그 + 실패 카운트
 * 증가. 카운트는 `jkdesktop test-script` 러너가 프로세스 종료 코드로 승화한다
 * (0 = 전부 통과, 1 = 실패 있음).
 *
 * @note [AI Agent] assert는 값을 고치지 않는다 — 기록만 한다. 실패해도
 *   스크립트는 계속 실행되므로, 이후 assert가 연쇄 실패할 수 있다.
 */
declare function assert(cond: boolean, message: string): void;

/**
 * actual과 expected가 다르면 실패를 기록한다. 비교는 JSON 직렬화 기준 —
 * 숫자 1과 문자열 "1"은 다르다고 판정한다.
 */
declare function assertEq(actual: unknown, expected: unknown, message: string): void;

// ---------------------------------------------------------------------------
// v3 — 모달 다이얼로그 (docs/27 §4 단계 3). 레거시 화면(JangoUI/OccUI 빌더)이
// 다이얼로그 중심이므로, C++의 JKDialog 구축 패턴(모달 윈도우 + 컨트롤 + 결과
// 콜백)을 그대로 옮긴 표면이다. 다이얼로그가 만든 컨트롤도 같은 controlId
// 레지스트리에 등록되므로 findControl/click/setText/getText가 그대로 동작한다.
//
// 결과 코드 규약 (JKDialog 상수): 1 = OK, 2 = Cancel, 3 = Yes, 4 = No.
// ---------------------------------------------------------------------------

/**
 * 모달 다이얼로그(숨김 상태)를 만들어 dialogId를 반환한다. title이 타이틀 바
 * 텍스트가 되고 rect는 다이얼로그 크기(이동 가능 — 원본 다이얼로그와 동일한
 * WA_TITLEMOVEABLE). 닫힐 때마다 onClose(result)가 호출된다 — OK/Cancel
 * 버튼에서 dialogClose(dlg, 결과)로 닫거나, ESC/타이틀 닫기는 Cancel로 닫는다.
 *
 * Close 후에도 dialogShow(dlg)로 재열 수 있다(원본 앱의 다이얼로그 재사용
 * 패턴과 동일).
 *
 * @note [AI Agent] 모달 슬롯은 앱당 하나다 — 다이얼로그가 열린 상태에서
 *   messageBox를 호출하면 모달 슬롯을 빼앗는다(원본 C++ 앱과 동일 제약).
 *   onDialogClose 안에서 messageBox를 여는 것은 안전하다(다이얼로그가 먼저
 *   모달 슬롯을 해제한 뒤 콜백이 돈다).
 */
declare function createDialog(title: string, rect: JKRect, onClose: (result: number) => void): number;

/**
 * 다이얼로그에 정적 라벨을 추가하고 controlId를 반환한다. rect는 다이얼로그
 * 클라이언트 영역 기준이다.
 */
declare function dialogAddLabel(dialogId: number, rect: JKRect, text: string, id?: number): number;

/** 다이얼로그에 한 줄 입력란을 추가하고 controlId를 반환한다. */
declare function dialogAddEdit(dialogId: number, rect: JKRect, text: string, id?: number): number;

/**
 * 다이얼로그에 버튼을 추가하고 controlId를 반환한다. 클릭은 전역
 * onClick(controlId)로 전달되므로, 스크립트가 controlId로 결과를 구분해
 * dialogClose를 호출한다(원본의 SetOnClick → Close(result) 패턴).
 */
declare function dialogAddButton(dialogId: number, rect: JKRect, text: string, id?: number): number;

/**
 * 다이얼로그를 모달로 연다. 이전 포커스 컨트롤이 저장되고 다이얼로그의 첫
 * 자식에게 포커스가 간다 — 닫히면 저장된 포커스로 복원된다(모달 포커스 복원).
 */
declare function dialogShow(dialogId: number): void;

/**
 * 다이얼로그를 코드에서 닫는다 (result는 결과 코드 규약 참고). onClose(result)로
 * 이어진다. ESC/타이틀 닫기 버튼은 Cancel(2)로 닫는다.
 */
declare function dialogClose(dialogId: number, result: number): void;

// ---------------------------------------------------------------------------
// v4 — 설정 주입 (docs/27 §4 단계 4). 터미널 terminal.json 같은 반복 데이터를
// JSON으로 두고 스크립트가 읽어 반영한다.
// ---------------------------------------------------------------------------

/**
 * app.js와 **같은 디렉터리**의 JSON 설정 파일을 읽어 파싱한 객체를 반환한다
 * (런타임 자체 파서 사용). 파일이 없거나 JSON이 아니면 null을 반환하고 이유를
 * 로그에 남긴다. 파일 단위 실패만 호스트가 알려주고, 각 키의 기본값 폴백은
 * 스크립트가 담당한다.
 *
 * @example
 *   const cfg = readConfig("config.json") || {};
 *   const scrollback = cfg.scrollback ?? 1000;   // 기본값 폴백
 *
 * @note [AI Agent] 경로는 app.js 옆의 파일 이름만 허용된다 — 절대 경로와
 *   `..` 탐색은 거부된다(샌드박스 유지). 스크립트의 파일 접근은 이 바인딩이
 *   유일하며, 임의 경로 읽기 요구는 설계상 거절 대상이다.
 */
declare function readConfig(fileName: string): any;

// ---------------------------------------------------------------------------
// 스크립트 콜백 (전역 함수로 정의하면 호스트가 호출한다 — 선언 충돌을 피하려고
// .d.ts ambient var로 선언하지 않는다; 아래 주석이 계약이다)
//
//   function onCreate()          — 선택. 스크립트 적재 직후 1회.
//   function onClick(controlId)  — 선택. 버튼 클릭마다.
//   function onExit()            — 선택. 컨텍스트 해체 직전(종료/리로드).
//
// 예외 정책 (docs/27 §3.2): 미처리 예외는 메시지 + JS 스택 트레이스가 로그에
// 덤프되고 앱은 정상 종료한다. 로그만 읽고 스스로 고칠 수 있게 쓸 것.
// ---------------------------------------------------------------------------