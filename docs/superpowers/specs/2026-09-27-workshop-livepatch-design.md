# 워크숍 라이브 패치 (컨텍스트 생존 정의 재평가) 설계

> 2026-09-27 설계 합의. docs/67 단 1 리파인 3호 — 사훈 1(말로 고치는 체감) 본체.
> 선행: 단 1(상태 보존 리로드=폴백 경로) 완결(docs/67 §4) — 원장 §6 "게이트 없이
> 착수 금지" 조건 해소. 승인 결정 3건은 §2.

## 1. 한 줄 그림

`set_script {source, slot?, live:1}` → **QuickJS 컨텍스트를 죽이지 않고 정의만
재평가**(React Fast Refresh / Erlang 핫 코드 방식). 지금 리로드=Stop+Start라
JS 상태(타이머 카운터·클로저)가 증발하는 것을 라이브 경로에서 제거 —
폰에서 "버튼 동작 바꿔 줘"가 앱 종료 없이 반영되는 경로.

## 2. 승인 결정 (2026-09-27 AskUserQuestion — 3건 전부 권장안 채택)

1. **패치 중 생성류 = 차단+에러 교육** — 재평가 중 위젯·타이머·다이얼로그
   생성은 `bad_patch` 에러. 규약(§3) 위반을 폐곡선으로 교육.
2. **실패 이원화** — 컴파일 게이트 실패=파일·컨텍스트 무손상+에러 에코 /
   런타임 예외=자동 풀 리로드 낙하+에러 에코. 폐곡선(에러가 LLM에게 돌아온다)
   양쪽 모두 유지.
3. **도구 표면 = set_script live 옵션** — 신도구 없음. 기본 동작(풀 리로드)
   불변. 브리지 generic 릴레이 덕에 폰 무수정 노출.

## 3. 계약 (무엇이 살고, 무엇이 바뀌고, 무엇이 막히나)

### 생존
- **위젯** — native 컨트롤(호스트 소유). 패치는 컨텍스트만 건드린다.
- **타이머+클로저 상태** — 구 정의가 캡처한 변수·setInterval 핸들 전부 생존.
  구 타이머 콜백이 호출하는 top-level 함수는 전역 객체 조회이므로 **새 정의로
  자동 해석**된다.
- **글로벌 프로퍼티** — 스크립트가 globalThis에 둔 상태.
- **의미 커서 선언** — `cursorDeclJson_`은 패치에서 클리어하지 않음(Start와
  다름). 재선언하면 갱신(기존 dedupe 로직), 무선언이면 보존. 패치는 창 수명
  안의 조작 — docs/64 "창 닫힘에만 소멸" 계약과 일치.

### 교체
- **top-level `function` 선언** — 전역 객체 프로퍼티로 재바인딩.
  onCreate/onClick/onTick/onMouse/onAgentAct/onSnapshot/declareCursor 등
  전역 콜백 참조는 모두 전역 조회 경로라 재평가 후 새 정의로 해석.
- **top-level `var`/할당** — 재평가 시 덮어씀(초기화 산술이면 상태 리셋 —
  §7 한계).

### 재호출 금지 (패치에서 안 함)
- `onCreate` — 재호출 시 위젯 중복. 위젯 구조 변경은 풀 리로드의 영역.
- `onExit` — 죽는 컨텍스트가 없다.
- `onSaveState/onRestoreState` — 리로드가 없으므로 상태 수송 불요.

### 패치 평가 중 차단 (bad_patch)
`createButton/createLabel/createEdit/createCanvas/createDialog/dialogAdd*/
setInterval` → 에러 응답(어느 함수가 왜 막혔는지 문구 포함 — "move into
onCreate, or full reload"). 근거: 패치 재평가에서 위젯 생성은 중복 난입,
타이머 재무장은 구 타이머와 이중 구동. 차단 에러가 LLM에게 규약을 교육한다
(폐곡선). `setText/getText/click` 등 조작·조회는 허용.

### 패치 안전 형태 (카탈로그·프리앰블 교육 문장)
- top-level = `function` 정의만 권장 — top-level `const/let`은 재선언 시
  SyntaxError → 풀 리로드 낙하(§2 결정 2). 가변 상태는 클로저(onCreate) 또는
  globalThis 프로퍼티로.
- 위젯/타이머 생성은 onCreate에서.
- 타이머 간격·위젯 구조 변경 = 라이브 불가 → live 미사용 풀 리로드.

## 4. 흐름 — set_script {source, slot?, live:1}

1. **컴파일 게이트**: 살아있는 컨텍스트에서 `JS_Eval(COMPILE_ONLY)`
   (벤더 quickjs-ng quickjs.h:457 실측). 문법 실패 → **파일 미기록, 컨텍스트
   무손상**, `{"ok":false,"error":...,"live":true}` 에코 → LLM이 고쳐 재시도
   하거나 live:0(풀 리로드) 선택.
2. 통과 → **.history 스냅샷 → 파일 기록**(기존 스토어 경로 재사용 — 이후
   낙하해도 파일=진실원 회복, 원칙 1) → 컨텍스트에서 정의 재평가(생성류
   게이트 on).
3. **성공** → `{"ok":true,"live":true,"gen":N}` — 앱 생존, 상태 생존.
4. **런타임 예외** → 자동 풀 리로드 낙하(기존 ReloadNow — 위젯 스냅샷+
   onSaveState/onRestoreState 수송) + `{"ok":true,"live":false,"error":...,
   "gen":N}` — 폐곡선 유지(에러 노출).

- `live` 옵션 부재/0 = 기존 풀 리로드 경로 **불변**.
- 다른 슬롯+live:1 = 라이브 불가 — 기존 스위치 경로로 자동 낙하, 응답에
  `live:false` 표기.
- top-level `const/let` 재선언은 재평가 시 SyntaxError → 낙하 경로(문법
  에러 문구가 규약을 교육). v1 절제: SyntaxError 서브분류로 무손상 에코
  최적화는 후속.

## 5. 구현 지점 (3곳 + 계약 문서, 서버·와이어·브리지 무수정)

- `engine/src/script/JKScriptHost.cpp/.h` — `Patch(source)` 신설:
  ① `patching_` 플래그(C 바인딩이 context opaque로 판정 — 생성류
  createButton/createLabel/createEdit/createCanvas/createDialog/dialogAdd*/
  SetInterval만 차단, setText/getText/click/declareCursor 허용)
  ② COMPILE_ONLY 프리패스(quickjs.h:457) → 실패 시 컨텍스트 무접촉
  ③ 재평가 = 프리패스 바이트코드를 `JS_EvalFunction`(quickjs.h:1278)으로
  실행 — onCreate/onExit 미호출, cursorDeclJson_ 보존.
  `Stop()/Start()` 무수정 — Patch는 별도 경로.
- `engine/include/apps/ClientScriptApp.h` — set_script 분기: live 인자 →
  §4 흐름. 기존 스냅샷/기록 순서를 성공 후 기록으로 재배치(live 경로만).
  같은 슬롯+호스트 러닝 조건에서만 라이브, 그 외 기존 경로.
- `jk.d.ts v7 + kApiCatalog + kLlmTurnPreamble` — set_script live 인자 +
  패치 안전 형태 교육 1문장 + "작은 수정은 live:1" 라우팅 문장.
- 패치 성공 시 stderr 로그 1줄(`[script] live patch: gen N`) — 프로브 단정
  지점.

## 6. 게이트

- 신설 `engine/tools/probes/probe_workshop_livepatch.ps1` ×2:
  ①라이브 패치 성공 — 타이머 카운터(패치 전 상태) 생존+새 동작 반영 실측
  ②문법 실패 — 컨텍스트 무손상(기존 라벨 유지)+에러 에코 ③위젯 생성 위반 —
  bad_patch+위젯 수 불변 ④런타임 예외 — 풀 리로드 낙하+에러 에코+live:false
  ⑤폰 경로(브리지 WS) 회귀.
- 회귀: probe_workshop / canvas / cursor / conquest_workshop / app_tools ×2.
- 워크숍 재팩 규율: JKScriptHost 변경은 jkapp_script.dll 재빌드+
  pack_workshop.ps1 재팩 세트(docs/60 §10.1).

## 7. 문서화 한계 (명시 — v1 수용)

- 수기(메모장) 편집은 여전 풀 리로드(watch mtime 경로 — 라이브 패치는 도구
  쓰기 경로 전용).
- 타이머 간격/위젯 구조 변경 = 라이브 불가(풀 리로드 필요 — 협약 문서화).
- top-level `const/let` 재선언 = 재평가 에러 → 풀 리로드 낙하. 패치 안전
  형태(top-level function 정의만)는 에러 교육+카탈로그로 정착.
- 구 타이머가 클로저로 캡처한 **지역 상태는 구 코드 그대로**(Fast Refresh의
  stale closure 수용 — 호출하는 함수는 새 정의로 해석되는 게 이 설계의
  생존 단위).
- 컴파일 게이트는 문법만 — top-level 순수성(위젯 생성 부재)은 런타임
  차단+에러로 교육한다(정적 판정 불가).

## 8. 다음 수

스펙 승인 → writing-plans(구현 플랜) → SDD 실행(태스크별 커밋) → 최종
리뷰(opus) → 라이브 반영(서버+워크숍 재팩·재기동, 토큰 불변) → 사용자
눈확인(폰 "버튼 색 바꿔 줘"류 실전 — 맨 뒤).