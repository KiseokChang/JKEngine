# 60. 말로 만드는 앱 워크숍 (Workshop) — 스펙

상태: **설계 승인 (2026-09-20), 구현 전**. 자세: **프로토타입 — 보안/트러스트 고려 배제**.
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

### 2.4 대화 흐름 (기존 인프라 무수정)

폰 브리지 채팅 → jkagentd → PC 에이전트 세션이 app_tool
get_script/set_script → 자기교정 → **데스크탑 창이 그 순간 변함**.
PC 팔레트/채팅도 동일 도구. 사용자는 한국어로 바람만 말함.

## 3. 파일 변경 목록

| 파일 | 변경 |
|---|---|
| `engine/src/apps/JKAppModule_script.cpp` | MANI `scriptfile=`/`watch=` 해석 — 외부 스크립트 경로를 SetScriptInfo에 전달, watch 강제 |
| `engine/include/apps/ClientScriptApp.h` | 외부 스크립트 지원 + ReloadNow()(동기 리로드, 에러 반환) + 실패 시 에러 라벨 |
| `engine/src/apps/JKScriptHost.cpp` (필요 시) | 리로드 에러 상세 전달 확인 (LastError 이미 존재) |
| `state/scripts/myapp.js` | 템플릿 (앱 기동 시 자동 생성 — 소스 트리에는 샘플 고정) |
| `workshop.jkx` | jkctl pack 또는 수기 MANI+MODL+아이콘 |
| `engine/tools/probes/probe_workshop.ps1` (또는 probe_app_tools 확장) | 신설 |

## 4. 검증 (프로브, 2연속 원칙)

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

## 6. 레슨 예약 (구현 후 채울 것)

- (비워둠 — 구현 세션에서 실측 레슨 기록)