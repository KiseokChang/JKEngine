// jk.d.ts v1 — host API contract v1 (docs/27_scripting_quickjs_bridge.md §4).
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