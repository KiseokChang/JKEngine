# 스크립트 브릿지 (QuickJS-ng) 설계와 단계별 계획

> 2026-09-06. **확정.** 외부 리뷰(JSValue RAII, 핫 리로드, 스택 트레이스 덤프, 에이전트 가드레일 주석)를 반영했고 §8 결정 항목 4건이 확정되었다. docs/README 인덱스 등록 완료.
> 상위 문서: docs/21(.jkx 컨테이너), docs/24(폴더 재구조화), docs/26(터미널 로드맵 — 산출물 예: 설정 파일).
> 핵심 전제: 이 엔진의 1차 "사용자"는 사람이 아니라 **코딩 에이전트**(본 작업을 수행하는 LLM 에이전트)이며, 스크립트의 대부분도 에이전트가 작성한다. 따라서 런타임/도구 선택은 사람의 편의가 아니라 **에이전트의 생산성(작성 유창성, 실행 전 검증 가능성, 컨텍스트 재구성 비용)** 기준으로 판단한다.

---

## TL;DR

- 런타임: **QuickJS-ng** 채택. 결정적 근거는 **`jk.d.ts` 타입 계약**(스크립트용 CLAUDE.md)과 JS 코퍼스 우위.
- 스크립트 앱 = **공용 인터프리터 모듈 `jkapp_script.dll` 1개 + 앱별 `app.js` 데이터**. 컴파일/링크 없이 화면 추가.
- 적용 우선순위: **① 코어 증명 → ② UI 자동화 → ③ 레거시 화면 포팅 → ④ 설정·테마.**
- 스크립트 금지 구역: 서버 컴포지터, VT 파서/그리드, 렌더 핫패스, IPC. 스크립트는 **UI 오케스트레이션 전용**.
- 모든 단계에서 **바인딩 추가 시 `jk.d.ts` 동시 갱신이 의무**(이 습관이 QuickJS 채택 논거의 전제).

---

## 1. 목적과 배경 — 왜 스크립트 브릿지인가

| 이득 | 현재 | 스크립트 도입 후 |
|---|---|---|
| 앱 추가 비용 | `JKAppModule_*.cpp` 작성 → 빌드 → `.jkx` 리팩 | `app.js` 하나 — 재컴파일 없음, 시제품 속도 |
| 레거시 화면 포팅 | 잔/OCC 업무 다이얼로그를 C++로 1:1 포팅 | 다이얼로그 나열형 UI를 스크립트로 저비용 포팅 (볼륨 페이오프) |
| 반복 개발 루프 | taskkill → cmake 빌드 → repack → 재실행 | 스크립트만 수정 → 재실행 (UI 작업 한정 핫리로드 효과) |
| UI 테스트 | 좌표 클릭 PowerShell probe(`engine/tools/probes/`) — 깨지기 쉬움 | 엔진 내부에서 컨트롤 핸들로 구동하는 시나리오 스크립트 |

**경계 정합성**: 스크립트 호스트가 필요로 하는 서비스(리소스 캐시, 캡처, 모달, 입력 윈도우)는 `JKApplicationHost`(`g_jkAppHost`)가 이미 제공하며, 서버/클라 분리 구조에서 스크립트는 클라 쪽 `JKAppModule` 자리만 대체한다. 아키텍처 변경이 거의 없다.

---

## 2. 런타임 선택 — QuickJS-ng (결정)

### 2.1 평가 원칙

브릿지(C++ 쪽)는 **한 번** 짜지만, 스크립트는 **계속** 짠다. 반복 활동에 최적화해야 하며, 에이전트의 반복 활동은 "언어 유창성 + 실행 전 검증 수단 + API 문서 재구성 비용"이 지배적이다.

### 2.2 비교

| 축 | Lua 5.4 | QuickJS-ng (JS) |
|---|---|---|
| 학습 코퍼스 | 게임/Neovim 수준. 핵심은 작음 | **JS는 코퍼스 최대권** — 문법 수준 실수가 가장 적음 |
| 타입 계약 | LuaLS 주석(`---@param`) — 가능 | **`.d.ts`로 호스트 API를 타입 문서화** — tsc 검증 가능, 체커 없어도 문서로 유효 |
| 커스텀 API 학습 | 바인딩 문서를 매번 읽어야 함 | `.d.ts` 파일 하나로 **새 대화의 에이전트가 즉시 정착** |
| 문자열/한글 | 바이트 투과 — UTF-8 그대로 | C API 경계에서 UTF-8 변환. BMP 한글(완성형/호환 자모)은 사실상 무해 |
| JSON/설정 | 별도 라이브러리 | `JSON.parse` 내장 — 단계 4(설정)에 유리 |
| 임베딩 C API | 가장 단순 | JSValue 소유권/GC 실수 여지 — **한 번 패턴화하면 소멸하는 일회성 비용** |
| 유지보수 | 5.4 안정 | quickjs-ng 활발, CMake 지원 |

### 2.3 결정과 근거

1. **`jk.d.ts` = 스크립트용 CLAUDE.md.** 호스트 API(윈도우/컨트롤 생성, 이벤트, 타이머, 자동화)를 `.d.ts`로 정의하면 매 세션의 에이전트가 컨텍스트 재구성 없이 검증된 API 지식을 확보한다. node 툴체인이 없어도 "계약 문서"로 유효하고, 있으면 `tsc --noEmit`으로 실행 전 오타/시그니처 오류를 걸러낸다.
2. **코퍼스 우위는 매 스크립트마다 적립**된다. UI 트리·이벤트 핸들러는 JS 객체 리터럴/클로저에 자연 매핑된다.
3. 브릿지의 JSValue 소유권 실수는 **일회성 비용** — 패턴화되면 재발하지 않으므로 Lua의 "C API가 단순함" 우위가 밀린다.

**Lua가 유리한 경우**: 임베딩 표면 최소화가 절대적이거나, 스크립트가 짧은 설정/매크로 수준에 머무는 경우. 범용 UI 로직 플랫폼에는 JS가 적합.
**기각**: Squirrel/Wren 등은 코퍼스가 작아 에이전트 기준 탈락. V8/Node는 임베딩 무게가 부적절.

### 2.4 `jk.d.ts` 유지 정책 (QuickJS 채택 논거의 전제)

**동기화 3계층** — 프로세스 규칙만으로는 드리프트가 나므로 기계적 검증을 곁들인다:

| 계층 | 수단 | 잡아내는 것 |
|---|---|---|
| 프로세스 | 바인딩 변경 커밋에 **반드시 `jk.d.ts` 갱신 동봉** | 대부분의 누락 |
| 기계 검증 | 셀프테스트가 **실제 바인딩 이름 ↔ `.d.ts` 선언 이름 대조** — JKScriptHost가 QuickJS 컨텍스트 안에서 `Object.getOwnPropertyNames(globalThis)`를 평가해 스크립트에 실제 보이는 이름을 열람(ground truth)하고, `jk.d.ts`에서 선언 이름을 추출해 비교. C++ 병렬 목록 없이 런타임 자기 열람이므로 열람값 = 스크립트 시야와 정확히 일치 | 존재성 드리프트(누락/이름 변경/삭제). 타입 시그니처까지는 못 잡음 |
| 타입 검증(선택) | `tsconfig.json`(checkJs, strict, noEmit) + 예제 스크립트를 `npx tsc`로 검사 — node가 있을 때만 | 시그니처 오류 |

- **코드젠(바인딩 → `.d.ts` 자동 생성)은 하지 않는다.** API가 작은 동안엔 수동 유지가 저렴하고, 선언 파일의 JSDoc 주석이 곧 API 레퍼런스이기 때문에 사람/에이전트가 읽을 문서 품질이 코드젠보다 낫다. API가 수십 개를 넘고 드리프트가 잦아지면 재검토.
- **빌드 단계에 `.d.ts` 생성/갱신은 없다.** 갱신은 커밋 시점의 수동 작업이고, 바인딩은 런타임 등록으로 성립하므로 C++ 빌드 시점엔 API 표면이 존재하지 않는다. 빌드 후 `jkdesktop test`(검증 ⑥)는 갱신 누락을 **탐지만** 한다 — 실패 = 같은 커밋에서 `.d.ts` 갱신을 빼먹었다는 신호.
- **호환 규칙**: `.d.ts`는 **확장(additive)만** 하고 제거/시그니처 변경은 하지 않는다 — `.jkx` 안의 오래된 스크립트가 계속 동작해야 한다. 교체가 필요하면 `@deprecated` 마킹 후 구버전을 병행 유지하고, 제거는 마일스톤에서만.
- **버전**: 파일 헤더에 계약 버전 기록 (예: `// jk.d.ts v1 — host API contract v1`). vN 확장 시 docs/27 §4 표에 한 줄 기록.
- **문서화 스타일**: 모든 선언에 JSDoc 주석 의무 — `.d.ts` 자체가 호스트 API 레퍼런스이며, 에이전트가 스크립트 작성 전 **가장 먼저 읽는 문서**다.
- **에이전트 가드레일 주석**: 오용이 예상되는 API는 JSDoc 안에 `@note [AI Agent] 렌더링 루프 내 호출 금지` 식의 **명시적 금지사항**을 텍스트로 기록한다 — 계약 문서를 읽는 에이전트의 환각/오용을 선제 차단하는 장치다.
- **소비자**: 에이전트(1차), 에디터 intelliSense, `tsc --checkJs`(선택 툴체인 — 파일은 단독으로도 유효한 계약 문서).

---

## 3. 아키텍처

### 3.1 배치

```
[.jkx (스크립트 앱)]
  manifest.txt   name=scriptdemo / module=jkapp_script.dll / script=app.js
  jkapp_script.dll (공용 인터프리터 모듈 — 모든 스크립트 앱이 공유)
  app.js         (SCRI 엔트리 — 신규 TOC 타입)
  launcher@2x.png

클라이언트 호스트(main.cpp RunClientFromJkx)
  → MODL 추출·로드(기존 경로 유지) + SCRI를 DLL 옆 임시 디렉터리에 추출
  → jk_app_run_client() : JKScriptHost 기동 (JSRuntime/JSContext 1:1)
  → app.js 로드 → onCreate() → 이벤트 펌프 → onExit()
       │
       └ host API ↔ JKApplicationHost(g_jkAppHost) 서비스
            (리소스 캐시/모달/캡처/입력 윈도우/타이머)
```

### 3.2 설계 원칙

- **스레딩**: QuickJS 런타임은 스레드 간 공유 금지. 스크립트는 **UI 메인 스레드 전용**. 백그라운드 결과(터미널 출력 등)는 이벤트 큐잉 후 메인 스레드에서 디스패치 — ConPTY 리더 스레드 패턴(docs/22)과 동일.
- **샌드박스**: QuickJS 표준 내장에는 파일/네트워크 접근이 없다. 엔진 접근은 **호스트가 바인딩한 것만** 노출 — 추가 차단 계층 불필요.
- **공용 모듈 모델**: 앱별 DLL을 만들지 않고 `jkapp_script.dll` 하나를 모든 스크립트 앱이 참조한다(§3.1). 스크립트가 수정되면 `jkx-pack`만 재실행하면 되고, DLL은 변하지 않는다. 단일 출처 원칙(docs/21 §3)도 유지: 창 제목/크기는 `jk_app_meta()`가 권위 — 스크립트 모듈의 meta는 manifest를 읽어 채운다.
- **문자열 경계**: `JS_ToCStringLen`은 UTF-8 — 엔진 내부 UTF-8(`JKVtParser`, 컨트롤 텍스트)과 직결.
- **예외 정책(확정)**: 미처리 예외 → 로그 + 정상 종료(exit=1). 셀프테스트 모드에서는 실패 반환. 대화상자 표시는 과도 — 로그로 충분. 이때 `JS_GetException`으로 받은 예외와 **스택 트레이스(`.stack`)를 반드시 로그에 덤프**한다 — 에이전트가 로그만 읽고 스스로 수정(self-healing)하려면 콜스택 텍스트가 필수다.

### 3.3 스크립트 금지 구역 (스코프 한정)

| 금지 | 이유 |
|---|---|
| 서버 컴포지터/창 관리 | 성능 + 서버는 텍스트 렌더러가 없고 IPC 설계가 별도 필요 — 원천 배제, 추후 별도 설계 |
| VT 파서/그리드, 렌더 핫패스 | 프레임당 경로 — 해석기 오버헤드 허용 불가 |
| IPC 전송 계층 | 신뢰성 핵심 — 바인딩 노출 자체를 하지 않음 |

스크립트는 **UI 오케스트레이션 전용**으로 한정해야 API 표면이 관리 가능하게 유지된다.

---

## 4. 호스트 API 표면과 `jk.d.ts` 계약

**언어 구분**: 실행되는 스크립트는 평범한 **JavaScript**(`app.js`)이며 QuickJS-ng가 실행한다. `jk.d.ts`는 TypeScript의 *선언 파일*로, 호스트 API의 타입 시그니처만 담은 **계약 문서**이다 — 런타임에 로드되지 않고 QuickJS가 실행하지도 않는다. 에디터/`tsc --checkJs`가 이 선언을 기준으로 `.js` 코드를 실행 전에 검증한다(존재하지 않는 API, 잘못된 인자). tsc가 없으면 계약 문서로만 기능한다.

단계가 올라갈수록 확장한다. vN 확장 시 `jk.d.ts` 버전을 올리고 docs/27에 한 줄 기록한다.

| 버전 | 노출 | 비고 |
|---|---|---|
| **v1 (단계 1)** | `log`, `messageBox`, 창/다이얼로그 생성, `button`/`label`/`edit` 팩토리, `onClick`/`onTimer`, `setInterval`/`clearInterval` | 최소 증명 세트 |
| **v2 (단계 2)** | `findControl(id/name)`, `click()`, `setText`/`getText`, 키/마우스 이벤트 주입, `assert*` 헬퍼 | 자동화. 컨트롤 **직접 호출 우선**(구조적), SDL 이벤트 주입은 보조 |
| **v3 (단계 3)** | 모달 다이얼로그: `createDialog`(title/rect/onClose), `dialogAddLabel`/`dialogAddEdit`/`dialogAddButton`, `dialogShow`, `dialogClose(result)` | 레거시 포팅에 필요한 만큼만 — PasswordDialog 파일럿이 유발한 것만 추가 (combobox/목록/loadImagePNG는 필요해질 때) |
| **v4 (단계 4)** | `readConfig(file)` — JSON 파일을 `JS_ParseJSON`으로 읽어 객체 반환 | terminal.ini 등 |

`.d.ts` 운영은 §2.4 규칙을 따른다.

---

## 5. 단계별 구현 계획

### 단계 0 — 설계 확정 (이 문서)

✅ 2026-09-06 완료 — 검토·리뷰 반영, 결정 항목 확정(§8), docs/README 인덱스 등록.

### 단계 1 — 코어 임베딩 + 첫 스크립트 앱 (엔드투엔드 증명)

| 작업 | 위치 | 비고 |
|---|---|---|
| quickjs-ng vendoring | `engine/third_party/quickjs-ng/` | stb 선례. CMake 타깃 추가, 버전 고정 |
| `JKScriptHost` | `include/script/JKScriptHost.h`, `src/script/JKScriptHost.cpp` | JSRuntime/JSContext 래퍼, **JSValue RAII 홀더 의무 확립**(value-type 래퍼 — 소멸자에서 `JS_FreeValue`, `release()` 제공. JSValue는 16바이트 POD이라 힙 `unique_ptr<JSValue>` 래핑보다 스택값 홀더가 정석), UTF-8 경계 헬퍼, 예외 시 스택 트레이스 덤프(§3.2) |
| 스크립트 모듈 | `src/apps/JKAppModule_script.cpp` | 공용 모듈. meta는 manifest 기반, `run_client`에서 app.js 로드 |
| `.jkx` 확장 | `JKJkxFile` | TOC 타입 신설(예: `SCRI`), `TypeForName`에 `*.js` 매핑, manifest `script=` 키, `jkx-pack`이 app.js 포함 |
| host API v1 | `JKScriptHost` 내 | §4 v1 세트 |
| `jk.d.ts` v1 | `engine/scripts/jk.d.ts` + 첫 `app.js` 예제 | 계약 문서 시작 |
| 첫 소비자 | `scriptdemo.jkx` | 버튼 1 + 라벨(클릭 카운트) + 타이머 시계 — 단일/클라 모드 공통 |
| 개발용 핫 리로드 | `JKAppModule_script` | app.js mtime 감지 → JSContext 재생성 + `onCreate` 재실행. 개발 전용 스위치(`--watch` 또는 개발 키) — "수정 → 재실행" 루프를 "수정 → 즉시 갱신"으로 단축. 배포 경로와 무관, 상태는 버려짐(시제품 용도) |

**검증**: ① 셀프테스트에 스크립트 앱 부트 시나리오(js 로드 → onCreate → 이벤트 1회 → 종료) 추가. ② `scriptdemo` 단일 프로세스 실행. ③ 서버 모드에서 .jkx 스폰 → surface 렌더 → 닫기. ④ 의도적 예외 스크립트 → 로그 + 정상 종료 확인. ⑤ JSValue 누수 점검(JS_SetMemoryLimit + 런타임 해제 후 잔여 보고). ⑥ 바인딩 등록 이름 ↔ `jk.d.ts` 선언 이름 대조 통과(§2.4 기계 검증 — 이 시점부터 상시). ⑦ 핫 리로드: app.js 저장 → 앱 재실행 없이 UI 갱신 확인(개발 모드).

**상태 (2026-09-06)**: 단계 1 구현 완료. 셀프테스트 139 checks / 0 failures — 부트·인트로스펙션(⑥)·클릭 디스패치·타이머 winId·타이머 디스패치·예외 정책(④: LastError에 메시지+스택 트레이스)·SCRI 컨테이너 라운드트립 전부 통과. 구현 중 발견한 두 결함 수정: (1) `Start` 실패 경로가 `Stop()`을 자체 `JsValue` 홀더 소멸 **전에** 호출해 `JS_FreeRuntime`의 gc_obj_list assert를 트리거 — 플래그 + 스코프 재구성으로 홀더를 런타임 해제 전에 release(§5 단계 1 검증 ⑤의 실증). (2) TOC 타입은 4cc인데 "SCRPT"는 5자 — `SCRI`로 확정. 잔여: ②③⑦ 수동 확인, `JK_SCRIPT_WATCH=1` 핫 리로드 검증.

### 단계 2 — UI 자동화 API (테스트 레버)

- `findControl`(id/name 탐색 — `JKWindow` 트리 순회), `click`/`setText`/`getText`(컨트롤 가상 메서드 직접 호출), `injectKey`/`injectMouse`(보조), `assert`/`assertEq` 헬퍼(로그 + 실패 카운트).
- 셀프테스트 드라이버에 시나리오 스크립트 실행기 연결(`jkdesktop test-script <file>`), **기존 probe 1–2개를 스크립트로 대체**하여 회귀 확인.
- 한계 명시: 서버 모드 크롬 드래그 등 **프로세스 밖 상호작용은 probe가 여전히 담당**(docs/19 §7).

**검증**: 대체한 시나리오가 기존 probe와 동일 결함 검출력을 갖는지(의도적 버그 주입 1건), 셀프테스트 전체 통과 유지.

**상태 (2026-09-06)**: 단계 2 구현 완료. 바인딩 v2 — `findControl`(숫자=controlId 트리 탐색, 문자열=표시 텍스트 깊이 우선 탐색), `click`(`JKButton::OnClick` 직접 호출), `injectMouse`/`injectKey`(윈도우 `RespondMessage` 라우팅 경유 — 히트테스트가 스크린 좌표계라 바인딩에서 패널 클라이언트 픽셀→스크린 변환), `assert`/`assertEq`(호스트 카운터 + `ASSERT FAIL` 로그, JSON 직렬화 비교로 숫자/문자열 구분) — `jk.d.ts` v2 갱신 동봉(§2.4 규칙, 기계 검증 자동 적용). `jkdesktop test-script <file>` 헤드리스 러너가 어설션 카운터를 종료 코드로 승화. 셀프테스트가 (1) `scripts/tests/uiauto.js` 시나리오 통과 — 구조적 클릭과 주입 클릭이 모두 onClick에 도달 — (2) `uiauto_broken.js` 의도적 결함 검출(종료 코드 1)을 상시 검증(§5 단계 2 검증의 버그 주입 1건 충족). 기존 probe의 스크립트 포팅은 수행하지 않음 — 프로세스 밖 상호작용(서버 크롬 드래그 등)은 계속 probe가 담당하고, 실제 앱 UI 대상 시나리오는 필요 시 진행.

### 단계 3 — 레거시 화면 포팅 파일럿 (볼륨 페이오프)

- **선정 기준**: 컨트롤 종류 ≤ 3, 모달 1개 이하, 표/그리드 없는 화면 — `JangoUI`/`OccUI` 추출 빌더(docs/27 기준 커밋 `dc74212`)에서 가장 단순한 다이얼로그 1개. 후보 확정은 단계 2 완료 시점에.
- 포팅하며 필요해진 API만 v3에 추가한다(**선제 설계 금지** — API 표면 폭발 방지).

**검증**: 원본 C++ 화면과 나란히 실행해 레이아웃/동작 등가 확인, 리사이즈/모달 포커스 복원 포함.

**상태 (2026-09-06)**: 단계 3 구현 완료. 파일럿은 **JangoUI의 `PasswordDialog`**(정적+입력란+버튼 2 = 컨트롤 3종, 모달 1, 그리드 없음)로 확정 — 진입 흐름(Personnel 버튼 → 모달 → 결과 처리)까지 포팅한 `scripts/apps/passworddemo/`. 바인딩 v3 — `createDialog`(title/rect/onClose, WA_TITLEMOVEABLE), `dialogAddLabel`/`dialogAddEdit`/`dialogAddButton`(다이얼로그 전용 추가 함수 — v1 create* 시그니처 동결 유지, 컨트롤은 공용 레지스트리에 등록되어 `findControl`/`click`/`setText`가 그대로 동작), `dialogShow`(모달 진입 + 이전 포커스 저장), `dialogClose(result)` — `jk.d.ts` v3 갱신 동봉. 설계 결정: (1) 다이얼로그 윈도우는 JangoUI의 검증된 **재사용 모델**(Close는 숨김, 재Show로 재open — `JKControl::Open`이 closeRequested 해제) — UAF 없이 콜백 체인(버튼 OnClick → onClick → dialogClose → onClose)이 재진입해도 안전; (2) onClose JS 레퍼런스는 Impl이 소유하고 Stop()에서 JS_FreeRuntime **전에** 해제(단계 1 교훈 재적용), 핫 리로드 시 열린 다이얼로그는 모달 슬롯 리셋과 함께 폐기. `findControl` 문자열 탐색도 다이얼로그 트리로 확장(타이틀 매치는 제외). 셀프테스트가 `scripts/tests/dialog.js` 시나리오(생성/추가/show/close/재open, 결과 코드 전달, 다이얼로그 컨트롤 대상 v1/v2 바인딩)를 상시 검증 — 전체 0 failures. `jkx-pack passworddemo` + CMake 자동 리팩 등록. **잔여(수동)**: `jkdesktop jango`와 나란히 실행해 다이얼로그 레이아웃 등가 확인, 모달 포커스 복원(다이얼로그 닫힘 후 Personnel 버튼 포커스 복귀) 눈 확인, 서버 모드 .jkx 스폰 확인.

### 단계 4 — 설정·테마 데이터

- docs/26 단계 5의 `terminal.ini`를 JSON으로 대체(`JSON.parse` 활용, host API v4 `readConfig`).
- 테마/아이콘 매핑 등 반복 데이터를 스크립트/JSON으로 이동.

**검증**: 터미널이 JSON 설정(shell/폰트/스크롤 크기)으로 기동, 잘못된 키는 기본값 폴백.

---

## 6. 코드 위치 (계획)

| 요소 | 파일 |
|---|---|
| 런타임 래퍼 | `include/script/JKScriptHost.h`, `src/script/JKScriptHost.cpp` |
| 공용 스크립트 모듈 | `src/apps/JKAppModule_script.cpp` |
| `.jkx` 확장 | `include/JKJkxFile.h`, `src/JKJkxFile.cpp`, `main.cpp`(RunClientFromJkx) |
| 계약 문서 | `engine/scripts/jk.d.ts` |
| 자동화 시나리오 | `engine/scripts/tests/*.js` |
| 빌드 | `engine/CMakeLists.txt` (+ `jkx_packages` repack 자동화 유지, docs/21 §2) |

---

## 7. 리스크 / 비고

- **JSValue 소유권/GC 누수**: 최대 리스크. 단계 1에서 RAII 헬퍼 + 주석 패턴을 확립하고, 셀프테스트에 메모리 상한 점검을 넣는다(§5 단계 1 검증 ⑤).
- **API 버저닝**: `.d.ts` 파기적 변경 금지(§2.4). 바인딩 노출 폭을 통제하기 위해 "필요해진 것만 추가" 원칙(단계 3)을 유지한다.
- **임시 파일**: DLL 옆 app.js 추출은 docs/21 §6의 %TEMP% 잔존 제약과 같은 트레이드오프 — per-pid 파일명으로 누적 방지(기존 패턴 재사용).
- **quickjs-ng 버전 고정**: vendoring 시점 버전을 문서/CMake 주석에 기록해 재현성 확보.
- **회수 경로**: 런타임 교체(Lua 등) 가능성에 대비해 `JKScriptHost` 아래 바인딩 레이어를 얇게 유지 — host API 함수 시그니처는 런타임 중립적으로 설계.
- **의존성 순서**: 단계 2 이후의 모든 단계는 `jk.d.ts` 갱신을 동반한다. 브릿지 산출물 중 장기 가치는 코드보다 계약 문서 쪽이 클 수 있다.

---

## 8. 결정 항목 (2026-09-06 확정)

1. **스크립트 전달 메커니즘** — 클라 호스트가 SCRI를 DLL 옆 %TEMP%에 추출, 모듈이 `GetModuleFileName(self)` 디렉터리에서 app.js 탐색. ABI 무변경으로 엔드투엔드 증명. **장기 후보**: `.jkx` 경로를 C++에서 직접 받아 메모리에서 로드하는 VFS형 전달(I/O 정리) — 런타임 안정화 후 재검토.
2. **예외 정책** — 로그 + 정상 종료(exit=1), **JS 스택 트레이스 덤프 필수**(§3.2). 셀프테스트에서는 실패 반환.
3. **파일럿 화면** — JangoUI/OccUI의 최단 다이얼로그 1개(단계 2 완료 시점 선정, §5 단계 3 기준).
4. **`.d.ts` 배치** — `engine/scripts/jk.d.ts`. 스크립트와 계약을 같은 디렉터리에 두어 에이전트의 첫 스캔 위치를 일치.