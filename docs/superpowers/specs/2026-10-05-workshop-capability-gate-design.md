# 워크숍 스크립트 앱 능력 게이트 — 설계 스펙 (docs/74 결정 이행)

날짜: 2026-10-05
원장: docs/74_workshop_trust_decision.md (결정 기록 :9-14 — 구속력 있음)
원료: 탐색 에이전트 인벤토리 (2026-10-05, 이 문서 §부록 인용) + docs/60·67 단 2

## 1. 결정 (docs/74, 사용자 확정 2026-10-05)

1. **모델 = A 능력 선언형** — MANI가 선언한 능력만 스크립트가 쓴다.
2. **fail-closed = 미선언 호출 차단** — 선언 없이 게이트 대상 API를 부르면
   차단 + 정직 에러.
3. **적용 범위 = 워크숍 스크립트 앱만** — `WorkshopScriptApp`
   (MANI `scriptfile=` 비었음 판정이 참인 앱, JKAppModule_script.cpp:154).
   순수 SCRI 스크립트 앱(`ClientScriptApp`)과 내장 네이티브 앱은 게이트 밖.

사용자 근거: "아직은 개인 과제입니다" — 나눔 범위가 좁으므로 진위 보장(서명)
보다 능력 가시성이 먼저다. 서명은 나눔 확대 시 2단계(docs/74 §2 B, 접미
확장은 §3 스케치 말미: 선언을 서명 본문에 넣는 결합).

## 2. 능력 토큰 표 (바인딩 → 토큰)

선언 위치: MANI `capabilities=` 컴마 목록. 미선언 바인딩은 fail-closed.

| 토큰 | 바인딩 (JKScriptHost.cpp) | 비고 |
|---|---|---|
| `widget` | messageBox(:279), createButton(:290), createLabel(:311), createEdit(:331), setText(:351), getText(:365), createDialog(:571), dialogAddLabel(:652), dialogAddEdit(:660), dialogAddButton(:668), dialogShow(:676), dialogClose(:691) | 창 안 UI 생성·모달 |
| `timer` | setInterval(:382), clearInterval(:413) | 앱 타이머 (ScriptTimerWinIdBase 0x7F00) |
| `canvas` | createCanvas(:816), canvasClear(:854), canvasRect(:866), canvasPixel(:884), canvasLine(:900), canvasCircle(:917), canvasText(:934) | 그리기 + 캔버스 입력 수신 |
| `agent` | declareCursor(:960) | 서버에 의미 커서 블록을 봉인("gate":"ask" 고정 :1032-1037) — act/snapshot 유입 경로는 서버 app_tool 게이트(:3765-3862)가 이미 검사하므로 이층 아님 |
| `fs` | readConfig(:712) | 오늘 존재하는 유일한 파일 입출력 (app.js 옆 JSON 1MiB) |
| `input` | injectMouse(:477), injectKey(:501), click(:450) | **최대 특권** — 실 입력 주입·타 창 컨트롤 구조 클릭 |
| `uiauto` | findControl(:429) | depth-first 텍스트 탐색 — **부속 창의 실제 앱 UI까지 닿음**(:206-208), read-only 조회 |
| `network` | (바인딩 없음 — 리저브) | 선언해도 오늘은 아무 API도 안 열림; 미래 확장 자리 |

**무조건 허용 (게이트 밖):** `log`(:270), `assert`/`assertEq`(:520,:543) —
진단·테스트 전용, 환경에 부수효과 없음(stdout 콘솔·참거짓 판정뿐).
선언 문화를 조작으로 만들지 않기 위해 "선언=능력"의 대상에서 제외하되,
정직 원칙상 **모두에게 무해**하다는 점을 d.ts 문서에 명기한다.

`createDialog` 계열을 widget에 넣은 근거: 실 JKDialog 창을 만들지만 그
창은 스크립트 앱 자기 자식 창이며, 닫힘 콜백으로 앱 내부 제어에만 쓰인다
(docs/27). `input`과 `widget`을 분리한 근거: 위젯 생성은 내 자식 UI,
input 주입은 **내 창 밖으로 손을 뻗는** 최대 특권 — docs/74의 능력
가시성 원칙상 이 둘의 위험 등급이 정반대다.

## 3. MANI 스키마 + 파싱

### 3.1 구문
```
name=workshop
module=jkapp_script.dll
script=app.js
scriptfile=workshop/<app>/app.js
watch=1
capabilities=timer,canvas
```
- 구분 = 컴마, 토큰 정규화 = trim + 소문자. 빈 토큰 무시. 빈 값(또는 키
  부재) = 선언 없음 = **능력 없음** (fail-closed 그대로).
- **미지 토큰은 파스에서 거부하지 않고 보존하며, 게이트에서 무효 처리** —
  미지 토큰이 선언된 것 자체가 수신자에게 보여야 할 정보다(배지에 그대로
  표시 §5). 게이트는 바인딩이 있는 토큰만 의미한다.

### 3.2 파싱 변경 (유일한 파서)
- `JkxManifest` (engine/include/JKJkxFile.h:37-60)에 신설 멤버:
  ```cpp
  // 능력 선언 (docs/74 결정 — 워크숍 스크립트 앱만 적용). 컴마 목록,
  // trim+소문자. 미지 토큰도 보존한다(수신자 가시성). ""=선언 없음.
  std::string capabilities;
  ```
- `JkxManifest::Parse` (engine/src/JKJkxFile.cpp:39-63)의 if/else 체인에
  `capabilities` 분기 신설 — **정규화하지 않은 원문 행을 그대로 저장**
  (필드 보존 원칙과 일치; 토큰 분해는 소비자 몫).
- **재팩 보존은 이미 완결** — `JkxManifestMerge`(JKJkxFile.cpp:75-146)가
  미지 키 행을 원문 보존하고, 셀프테스트가 `capabilities=agent,timer`
  잔존을 검증 :1656,1671-1672 (b7a76a3). 파서에 필드를 넣으면 이 테스트의
  이중 보장이 된다(merge는 행 보존, Parse는 의미 접근).

### 3.3 런타임 전달 경로
1. `jk_app_meta()` (engine/src/apps/JKAppModule_script.cpp:122-149) —
   `mani.Parse(text)` 후 신설 모듈 static `g_capabilities`(:117-118 자리 옆)
   에 `mani.capabilities` 저장.
2. `jk_app_run_client()` (:151-194) — `WorkshopScriptApp` 분기(:154)에서
   `SetEnabledCapabilities`로 주입.
3. 순수 SCRI 앱(`ClientScriptApp` :188-193)은 **주입하지 않음 → 게이트
   비활성** (결정 3의 워크숍만 적용).

## 4. 게이트 (JKScriptHost)

### 4.1 상태
```cpp
// JKScriptHost.h — 워크숍 능력 게이트 (docs/74 결정)
void EnableCapabilities(std::string list);  // 컴마 목록, ""=능력 없음
bool GateActive() const { return gateActive_; }
```
- `gateActive_` = `EnableCapabilities`가 불렸는지 (워크숍 앱만 부른다).
  불리지 않으면 게이트 완전 비활성 — 기존 SCRI 앱·셀프테스트 무영향.
- 토큰 분해는 여기서 수행(trim+소문자), `caps_` 집합에 저장.

### 4.2 적용점 — 단일 조우
- 모든 호스트 함수의 공용 전주: `JKScriptHost* host = HostOf(ctx)`
  (thunk 선두, 예: :281,:292). 선례 `PatchBlocked`(:89-93)가 있는 위치.
- 신설 헬퍼:
  ```cpp
  // 게이트가 활성인데 토큰이 미선언이면 TypeError를 던진다(fail-closed).
  // 활성 아님(워크숍 앱 아님)이거나 선언됐으면 true.
  bool JKScriptHost::RequireCap(JSContext* ctx, const char* capability);
  ```
  에러 문구 (**문자열 고정, 문서화된 계약**):
  `capability '<tok>' not declared in MANI`
- 게이트 대상 바인딩 각 thunk 선두(HostOf 후)에 1줄 추가:
  ```cpp
  if (!host->RequireCap(ctx, "timer")) return JS_EXCEPTION;
  ```
  표 §2의 토큰-바인딩 매핑 전수 적용. `log`/`assert`/`assertEq`는 제외.
- **d.ts 갱신 불요** — 신규 바인딩 없음(BoundNames 정합 테스트 :2704-2742
  통과 유지). 단 jk.d.ts에 게이트·무조건 허용 집합을 주석으로 문서화
  (계약 문서가 코드와 함께 산다 — docs-as-deliverables).

### 4.3 라이브 패치와의 관계
라이브 패치(QuickJS 정의 재평가, docs/67)는 **같은 호스트 인스턴스**를
재평가하므로 게이트도 같은 선언을 상속한다 — 재 패치로 능력이 늘어나는
경로가 없음. `EnableCapabilities`는 Start 전 1회만 호출(워크숍 앱 부팅
경로 3.3-2), 런타임 변경 API 없음(선언은 MANI가 유일 원천).

## 5. 수신자 UI — 캡션/스트립 능력 배지

- 위치: 슬롯 스트립(타이틀바 내장, kStripComboRect{50,-21,160,22} :
  ClientScriptApp.h:495) 왼편 — `slotLabel_`("슬롯:", :445) 자리 옆 신설
  `JKStatic` 배지.
- 문구: 선언 있음 = `능력: timer,canvas`(원문 그대로, 미지 토큰 포함);
  선언 없음 = `능력 없음`.
- 갱신 시점: `BuildStrip`(:439-455)에서 1회 + `OnScriptStarted`(:424-435)
  재 레이아웃에서 살아남게(스트립 리레이즈 :432-433).
- 원칙(docs/74): 선언 없는 앱이 능력 없음으로 표시되는 극단을 견뎌야
  선언 문화가 성립한다 — 빈 선언을 숨기지 않는다.

## 6. 셀프테스트

패턴: RunAppSelfTest의 script 블록(main.cpp:2571-2582 `writeScript` +
`check`)과 headless 시나리오(engine/scripts/tests/)를 따른다.

1. **게이트 fail-closed**: 워크숍 경로 구성(`EnableCapabilities("")`) 후
   `setInterval` 호출 → 스크립트 예외 + 문구 정확 일치
   `capability 'timer' not declared in MANI`. (RunScriptTestFile 시나리오
   신설: 게이트 차단을 에러로 단언)
2. **선언 통과**: `EnableCapabilities("timer")` 후 동일 호출 → 정상 등록
   (기존 케이스 2 setInterval/winId :2665-2688 재사용 형태).
3. **무조건 허용 확인**: 게이트 활성 상태에서 log·assert 정상 동작.
4. **MANI 파스**: `capabilities=Timer,input` → 보존·정규화(trim/소문자)·
   미지 토큰 보존 3검.
5. **워크숍만 게이트**: `EnableCapabilities` 미호출(ClientScriptApp 경로)
   에서 setInterval 정상 — 기존 케이스 2가 회귀 검증으로 겸함.
6. **배지 문구**: 선언/빈 선언 2가지 문구 단언(문자열 계약).

## 7. 아웃 오브 스코프 (이 스펙이 하지 않는 것)

- 서버 측 승인 파이프라인(PendingApproval/trust_request) 변경 없음 —
  능력 게이트는 클라 프로세스의 정적 계약이고, 유입 에이전트 도구는
  app_tool 3단 게이트가 이미 검사한다(두 층의 직책 분리).
- 서명·검수 도입 없음 (docs/74 §2 B/C — 나눔 확대 시 2단계).
- `network` 토큰에 대응하는 신규 호스트 API 개발 없음 (리저브만).
- 2KB 스키마 한도·스크롤백 등 기존 보류 정책 불변.

## 8. 마이그레이션

- 기존 워크숍 슬롯 앱: MANI에 capabilities= 없음 → 능력 없음으로 게이트.
  재선언 필요(caps 추가 후 재팩 — JkxManifestMerge가 보존, 실측 완료).
- 워크숍 템플릿(kTemplateScript, JKAppModule_script.cpp:100-112) 갱신:
  템플릿 매니페스트/안내에 capabilities 선언 예시를 심어 첫 선언 경험을
  낮춘다.
- 워크숍 개인용 실앱(deploy 대상)에 capabilities 도입은 사용자 배포 시
  각자 — 코드는 선언 없음=차단을 정직하게 표현한다.