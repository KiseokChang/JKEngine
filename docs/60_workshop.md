# 60. 말로 만드는 앱 워크숍 (Workshop) — 스펙

상태: **구현 완결 (2026-09-20)** — probe_workshop 16체크 ×2 ALL PASS + probe_app_tools
×2 ALL PASS. 자세: **프로토타입 — 보안/트러스트 고려 배제**.
발단: 사용자의 상상 — "스크립트 껍데기 앱을 만들고, **말로 고쳐가며** 앱을 만들기".
/report 체계(docs/57 §12)와 별개의 기능.

## 1. 왜 이것인가 (배경)

- 껍데기 앱 인프라는 이미 존재 (docs/27): `jkapp_script.dll` 공유 모듈 +
  `JKScriptHost`(QuickJS-ng) + `ScriptAppT`(ClientScriptApp.h). 스크립트 API v1:
  log/messageBox/createButton/createLabel/createEdit/setText/getText/
  setInterval/clearInterval, 콜백 onCreate/onClick/onExit.
- 핫 리로드도 존재: `ScriptAppT::OnWatchTick` — mtime 500ms 폴링, 패널 폐쇄 후
  재구축 2단계 (ClientScriptApp.h:129). dev 스위치 `JK_SCRIPT_WATCH=1`.
- **빠진 것**: ① 스크립트의 편집 가능한 안정 경로 (.jkx 경유 시 per-pid 임시
  파일로 추출되어 재기동마다 덮어씀 — engine/src/main.cpp RunClientFromJkx)
  ② 에러가 LLM에게 돌아가는 폐곡선 (LastError는 콘솔에만).

## 2. 결정된 설계 (사용자 답변 3건 반영)

질문→답변: 첫 앱 = **실용 위젯** / 반영 통로 = **도구 푸시+파일 감시 둘 다** /
승인 정책 = **워크숍 무승인** (프로토타입 자세).

### 2.1 워크숍 진실원 — `state/scripts/myapp.js`

- exeDir 기준 `engine/build/state/scripts/myapp.js`. 없으면 앱 기동 시
  템플릿("안녕하세요" 버튼)으로 자동 생성.
- 사람 메모장 편집 = 에이전트 편집 = 같은 파일. 파일이 진실원.

### 2.2 워크숍 앱 — `workshop.jkx`

- 모듈 재사용: `jkapp_script.dll`. MANI 신설 지시 2개:
  - `scriptfile=state/scripts/myapp.js` — 내장 SCRI 대신 exeDir 상대 외부
    파일 사용. **모듈(JKAppModule_script.cpp)이 해석 — main.cpp 무수정**
    (모듈이 manifest copy를 이미 자기 경로 기준으로 읽는 구조 활용).
  - `watch=1` — mtime 핫 리로드 기본 ON. 컨테이너 내장 SCRI 앱은 기존대로
    off (배포 .jkx 행동 불변).
- 창 크기 등은 기존 MANI 필드(name/title/width/height) 그대로.

### 2.3 대화 루프 — 앱 도구 2종 (docs/58 허브, vplayer 패턴)

앱 기동 시 자기 도구 등록 (기존 register 경로 그대로):

- `get_script` — myapp.js 원문 반환 `{ok,source}`.
- `set_script {source}` — 소스 푸시(256KiB 캡, 초과 `too_large`).
  파일에 쓰고 **동기 리로드 → 결과 반환**:
  - 성공 `{ok:true}` / 실패 `{ok:false,error:"<LastError 스택 포함>"}`.
  - **에러가 도구 응답으로 즉시 돌아가 LLM이 스스로 고치는 폐곡선 = 본 기능의 핵심**.
- 리로드 실패 시 UI: Start 실패하면 빈 패널 대신 에러 텍스트 라벨 1개 표시.

### 2.3.1 스레딩 리스크 (구현 첫 단계에서 실측 필수)

동기 리로드가 에이전트 도구 콜 처리 경로에서 안전한지 — QuickJS는 메인/UI
스레드 전용(docs/27 §3.2). 도구 콜이 메인 루프와 동일 스레드에서 오면 직접
ReloadNow(); 아니면 **파일 쓰기→mtime 감시가 다음 틱에 리로드**로 위임하고
응답은 `{ok:true, note:"reload pending"}` (에러는 이후 get_script로 확인).
vplayer OnAgentToolCall 선례상 메인 루프일 가능성이 높으나 실측으로 결정.

**실측 (구현 완료)**: OnAgentToolCall은 `JKClientApplication::Run` 프레임 스윕에서
불리는 것 확인 (vplayer와 동일 경로, app tool hub docs/58 §8.2) → **직접 경로 채택**.
`SyncReload()` = Stop + 패널 폐쇄 + RemoveClosedChildren 인라인(스윕 후행 호출과
멱등) + StartScript — 파산 소스가 응답에 에러를 실어 보내는 것으로 실증(c3).

### 2.4 대화 흐름 (기존 인프라 무수정)

폰 브리지 채팅 → jkagentd → PC 에이전트 세션이 app_tool
get_script/set_script → 자기교정 → **데스크탑 창이 그 순간 변함**.
PC 팔레트/채팅도 동일 도구. 사용자는 한국어로 바람만 말함.

## 3. 파일 변경 목록 (as-built)

| 파일 | 변경 |
|---|---|
| `engine/src/apps/JKAppModule_script.cpp` | MANI `scriptfile=`/`watch=` 해석 — 외부 스크립트 경로를 SetScriptInfo에 전달, watch 강제. 없으면 템플릿 자동 시드(EnsureParentDirs) |
| `engine/include/apps/ClientScriptApp.h` | `SetHotWatch()`(MANI watch 강제) + `SyncReload()`(동기 리로드, 에러 반환) + StartScript 실패 시 에러 라벨 + `WorkshopScriptApp`(도구 등록·get_script/set_script 서빙, 256KiB 캡) + FileMtime를 GetFileAttributesExA 100ns FILETIME으로 교체 |
| `engine/include/JKJkxFile.h` + `engine/src/JKJkxFile.cpp` | JkxManifest에 `scriptfile`/`watch` 필드 신설 (스펙 목록에 없었던 추가 — 모듈이 side manifest를 파싱하려면 컨테이너 파서가 필드를 보존해야 함) |
| `engine/scripts/apps/workshop/manifest.txt` | 워크숍 MANI (`scriptfile=state/scripts/myapp.js`, `watch=1`) |
| `engine/scripts/apps/workshop/app.js` | 템플릿 (SCRI 항목 — 워크숍 모드는 무시, 참고용) |
| `state/scripts/myapp.js` | 진실원 — 앱 기동 시 자동 생성 |
| `engine/tools/pack_workshop.ps1` | 수기 JKX1 패커 — **jkctl pack/jkx-pack은 MANI를 재생성해 `scriptfile=`/`watch=`를 탈락**시키므로 불가 (레슨 §6) |
| `build/apps/workshop.jkx` | MANI+MODL+SCRI 3항목 컨테이너 |
| `engine/tools/probes/probe_workshop.ps1` | 신설 — setup+15체크, permissions.json 미접촉 |

## 4. 검증 (프로브, 2연속 원칙) — **실측 결과 (2026-09-20)**

`probe_workshop.ps1` 16체크 ×2 연속 ALL PASS (r9·r10) + 회귀 `probe_app_tools.ps1`
×2 ALL PASS. 스펙 체크 대응:

| 스펙 체크 | 프로브 | 실측 |
|---|---|---|
| 1 set_script 정상 → ok:true + 디스크 기록 | c2-set-ok/c2-disk-written/c2-roundtrip | PASS (왕복 일치 확인) |
| 2 set_script 파산 → ok:false + error 비어있지 않음 | c3-broken-error | PASS — **동기 폐곡선 실증**: 응답에 `Unexpected end of input\n at ...myapp.js:1:53` 그대로 귀환 |
| 2의 "기존 UI 유지" | — | **설계 변경**: 파산 시 기존 UI 유지 대신 **패널 교체+에러 라벨 1개 표시**. 이유: 기존 UI를 유지하면 "불변"과 "방금 실패"를 구분할 수 없어 사용자가 멈춘 걸로 오인 — 실패가 눈에 보이는 편이 워크숍 자세에 맞음. 성공 리로드 시 정상 복구 |
| 3 256KiB 초과 캡 | — | 앱 측 캡은 구현됐으나 프로브 미직접 시험 — **서버 계층 8KiB args 캡(args_too_large)이 앞에 있어 와이어로는 도달 불가한 백스톱** (c4-args-too-large가 서버 표면을 시험). 계층 문서화: 서버 8KiB → 앱 256KiB |
| 4 파일 감시 경로 (도구 미사용) | c5-watch-hot-reload | PASS — mtime 별도 해결 필요했음 (레슨 §6) |
| 5 get_script 왕복 | c1-get-script-ok | PASS |
| 6 회귀 probe_app_tools ×2 | — | PASS ×2 |

추가 체크: s3 템플릿 자동 시드, c3-truth-source(파산 소스가 진실원에 반영 —
에이전트가 고치는 설계), c3b-recovered(자기교정 복구), c6 close_window → 카탈로그
소멸, c7 진실원 템플릿 복원(눈확인용 클린 상태).

**get_script 결과 상한**: 서버 result 16KiB — 템플릿(~357B)은 여유. 대형 스크립트는
16KiB 넘으면 결과 잘림 위험 → 백로그.

1. set_script 정상 소스 → `{ok:true}` + 파일에 실제 기록됨
2. set_script 파산 소스 → `{ok:false,error:비어있지 않음}` + 기존 UI 유지
3. set_script 256KiB 초과 → 캡 에러
4. 파일 감시 경로: 디스크에서 myapp.js mtime 변경 → 리로드 (도구 미사용 경로)
5. get_script → 방금 쓴 내용 왕복 일치
6. 회귀: probe_app_tools ×2 (앱 도구 허브 무손상)

첫 실용 위젯 = **할일 판** (에디트+추가 버튼+목록 라벨, API v1 범위) —
폰에서 "할일 판 만들어줘" 한마디로 만들어지는 것까지 눈확인.

## 5. 백로그 (범위 밖)

- 그리기(캔버스)/키보드/마우스 이벤트 API — 게임·토이용 (첫 위젯이 API v1으로
  되는지 확인 후)
- 복수 슬롯 (myapp2.js 등 여러 앱 동시 워크숍)
- 폰 미러 (창 상태를 폰에서 보기)
- 보안/트러스트: 프로토타입 완성 후 재검토 — 배포(.jkx 설치) 트러스트는
  기존 모델 유지, 워크숍 디렉터리 무승인의 경계 명문화

## 6. 레슨 (구현 세션 실측, 2026-09-20)

1. **jkctl pack/jkx-pack은 MANI를 재생성한다** — 신설 필드(scriptfile/watch)가
   탈락. 신필드를 담은 .jkx는 수기 패커(pack_workshop.ps1)로. 패커의 진짜 결함
   발견: jkx-pack 버그가 아니라 "pack 도구가 MANI 화이트리스트"라는 설계 자체.
2. **exFAT(FAT계열) LastWriteTime 에일리어스**: 서브초 간격 연속 쓰기는 이전
   스탬프에 가려져 mtime 폴링이 놓친다. 실측 — 600ms 간격 플레이크, 2s 간격
   3/3 성공. ① 감시자 쪽은 `_stat64`(1초 해상도)가 아니라
   GetFileAttributesExA 100ns FILETIME으로 읽고 ② 프로브는 2s 간격 최대 3회
   재푸시(사람의 "다시 편집"과 동일 패턴). 드라이브 제약류는 주기 재실측의
   연장선 — 파일시스템 타임스탬프 해상도도 제약류.
3. **PS5.1 CRT 인용기 이중 이스케이프 함정 (docs/55 레슨 3의 재발)** — JS 소스에
   `"` 있으면 JSON `\"` → agentctl 인용 이스케이프 `\\"` → CRT가 닫는 quote 직전
   백슬래시 런을 절반으로 깎아 인자가 조기 종료 → JSON 절단 → 서버 bad_request.
   프로브는 JS 소스를 작은따옴표 문자열로 써서 `"` 원문을 원천 배제.
4. **Start-Process cmd.exe /c 인용 지옥** — 임시 .cmd 배치 파일로 스폰하면
   전부 소멸 (stdout 리다이렉트 포함).
5. **c3 오탐 방어** — `unknown_app_tool`도 `"error":"[^"]+"`에 매치되므로
   negative lookahead + notmatch 병행. 클라 미등록과 리로드 실패는 다른 결함.
6. **캡 계층 명문화** — 서버 8KiB args(args_too_large)가 앞, 앱 256KiB(too_large)는
   와이어로 도달 불가한 백스톱. get_script 결과는 서버 16KiB result 캡 — 대형
   스크립트 원문 반환은 잘림 위험 (백로그).
7. **프로브 스폰은 .cmd 배치**: probe가 GUI 클라이언트를 띄우는 유일한 패턴 —
   `Start-Process -FilePath $bat -WindowStyle Hidden` + stdout 리다이렉트로
   클라 콘솔 로그(핫 리로드/스크립트 실패 문구)를 판정재로 쓴다.