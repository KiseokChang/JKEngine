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
| `engine/include/apps/ClientScriptApp.h` | `SetHotWatch()`(MANI watch 강제) + `SyncReload()`(동기 리로드, 에러 반환) + StartScript 실패 시 에러 라벨 + `WorkshopScriptApp`(도구 등록·get_script/set_script 서빙, 256KiB 캡) + FileMtime는 `JKPlatform::FileMtime100ns` 위임(§7 — 수기 GetFileAttributesExA는 세그폴트 원인으로 삭제) |
| `engine/include/JKPlatform.h` + `engine/src/JKPlatform_win32.cpp` | `FileMtime100ns()` 신설 — 100ns FILETIME 뮤테이터, windows.h-clean TU (§7) |
| `engine/src/script/JKScriptHost.cpp` | `ToWidgetText()` — 위젯 경계 UTF-8→KSSM 변환 5곳 + getText 역변 KssmToUtf8 (§7) |
| `engine/include/JKHangulUtil.h` + `engine/src/JKHangulUtil.cpp` | `KssmToUtf8()` 신설 — 역인덱스 지연 구축 + CP949↔UTF-8 |
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

- **[소각 2026-09-21] args 앞단 캡 병목** — 위 §4 표 3행·레슨 6의 "서버 8KiB args
  캡이 앞이라 앱 256KiB 백스톱이 와이어로 도달 불가"는 폰 실전 개선 task-1에서
  서버 앞단 캡을 8KiB→256KiB로 들어 올려 해소. 앱 백스톱과 정렬 — **docs/57 §13 ①**
  참조. probe_workshop c4도 ok 기대로 개정.
- **[소각 2026-09-23] result 16KiB 캡(get_script 대형 스크립트 잘림)** —
  HandleToolResult의 결과 상한을 16KiB→256KiB로 상향(args 캡과 대칭). set_script로
  들어온 스크립트의 JSON 이스케이프 결과는 argsRaw와 동일 형태라 256KiB 안에서
  왕복 보장. 실측: 100KiB 단일행 스크립트 set_script→get_script 왕복 100,205자
  완전 복원. probe_workshop 17체크 + probe_app_tools 64체크 ×2 ALL PASS 회귀.
- 그리기(캔버스)/키보드/마우스 이벤트 API — 게임·토이용 (첫 위젯이 API v1으로
  되는지 확인 후)
- 복수 슬롯 (myapp2.js 등 여러 앱 동시 워크숍)
- **스크립트 앱 declareCursor** — 의미 커서(2026-09-22, docs/64)의 워크숍 재선언
  (`agent.declareCursor()`, 스펙 §6): 말로 만든 앱이 커서 조작을 즉시 획득. v1은
  네이티브 앱(지뢰찾기)만 지원 — 앱 도구 허브 선언 경로에 cursor 블록이 그대로 합류.
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

## 7. 라이브 결함 2건 — 한글 깨짐 + 워크숍 클라 세그폴트 (2026-09-20 오후, 사용자 보고 "글자는 깨지지만")

**증상**: 워크숍 창은 뜨는데 한글이 전부 깨짐(조합형 비트맵 폰트 경로에 UTF-8 원문
직투입). **경로**: JS 문자열은 UTF-8, 위젯(JKStatic/JKButton/JKEdit/JKMessageBox)은
KSSM 조합형(JKDC 비트맵 폰트) — 위젯 경계 변환이 없었다.

**픽스**: `JKScriptHost.cpp`에 `ToWidgetText()` 헬퍼 — 인바운드 5곳(라벨/버튼/에디트/
SetText/messageBox)을 `jk::Utf8ToKssm` 경유로. 아웃바운드 `getText`는 `KssmToUtf8`
역변. `JKHangulUtil`에 `KssmToUtf8` 신설 — wCodeTable/SingleHan/한자 산술 매핑의
역인덱스를 지연 1회 구축(순방향 도메인 순회라 왕복 일치 보장) 후 CP949↔UTF-8.

**2차 결함 (변환 픽스 직후)**: 워크숍 클라가 `StartScript()+976`에서 세그폴트
(`mov %rax,0x150(%rdx)`, rdx=0x216). **이분법**: ToWidgetText 되돌려도 크래시,
FileMtime만 스텁하면 생존 → **원인 = ClientScriptApp.h의 수기
GetFileAttributesExA 선언 경로**. 같은 세그폴트가 FileMtime 호출 시점(StartScript의
lastMtime_ 초기화와 watch 타이머 틱 양쪽)에서 재현.

**픽스**: FileMtime 구현을 windows.h-clean 코어 TU로 이전 — `JKPlatform::
FileMtime100ns(path)` 신설(JKPlatform_win32.cpp, 이 TU는 엔진 헤더 전에
windows.h를 인클루드하므로 SDK의 WIN32_FILE_ATTRIBUTE_DATA를 그대로 쓴다).
ClientScriptApp.h의 수기 JkFileAttrData/GetFileAttributesExA/_WINBASE_ 센티넬
블록 전량 삭제 — 헤더 체인에서 windows.h류가 완전히 사라짐.

**검증**: probe_workshop 16체크 ×2 + probe_app_tools ×2 ALL PASS(신바이너리),
capture_window 실측 — 템플릿 "안녕하세요!" / "눌러 보세요" 정상 렌더링
(state/screenshots 실물 확인).

**레슨**:
1. **수기 WIN32 선언(관례)에도 한계가 있다** — 선언 자체가 맞아도 스택 레이아웃
   등 미묘한 불일치가 세그폴트로 온다(docs/56의 WIN32_FIND_DATAA 팩 사례의
   반대쪽 교훈). windows.h가 필요하면 **windows.h를 이미 안전하게 포함하는 TU로
   함수를 옮기는 편이 수기 선언보다 낫다** — 헤더 체인 오염 억제와 SDK 정확성을
   동시에 얻는다.
2. **이분법은 한 번에 하나의 변수** — ToWidgetText 되돌림은 무효였고 FileMtime
   스텁만이 생존. 두 픽스(KSSM 변환+FileMtime 이전)를 동시 넣고 프로브로 회귀
   확인한 뒤 capture_window로 최종 판정.
3. **한글 깨짐은 파이프라인 언어 경계** — JKDC 비트맵 폰트 파이프라인은 KSSM,
   JS/ImGui는 UTF-8. 위젯 경계에서 변환하는 게 정답이고, 역변환은 순방향 매핑의
   역인덱스로 자동 구성해 왕복 일치를 보장.
## 8. API 카탈로그 도구 (`api`) — LLM-facing 계약 (2026-09-20 오후)

**동기**: 폰 세션 실측 — 폰 LLM이 `createListBox`를 추측했다(헛다리 3턴 소모).
계약 원문이 `engine/scripts/jk.d.ts`에만 있고 에이전트는 그것을 절대 못 본다.

**픽스**: 워크숍 앱에 3번째 에이전트 도구 `api` 신설 — `get_script`/`set_script`
옆에 등록(`WorkshopScriptApp::OnInit`, probe `c1b-api-catalog`). 응답은
`kApiCatalog`(ClientScriptApp.h): 함수 시그니처 22종 + **문자집합 계약 명문화**
("위젯 텍스트는 ASCII+한글만 안전 — 기호(■□●◆)·이모지는 ?로 렌더됨", §7의
Utf8ToKssm 도메인 한계) + `onClick(id)` 이벤트 규약 + "목록 위젯은 없다 —
라벨+버튼 조합" 안내. 유지보수 룰 = jk.d.ts와 동일(신규 바인딩 시 둘 다 갱신,
additive-only라 드리프트 드묾).

## 9. 서버 크래시 증거 설비 — MirrorLogToFiles 자기먹이 루프 픽스 (2026-09-20)

**배경**: docs/57 §13 — 서버 무음 사망(2026-09-20 16:12 실측, WER 기록·콘솔 로그
소실) 대비 `InstallCrashHandler`(미니덤프+abort 마커)와 `MirrorLogToFiles`
(stdout/stderr→익명 파이프→데몬 스레드→원 콘솔+타임스탬프 로그 파일, 행 플러시)
을 `jkwinserver_main.cpp`+`main.cpp --server` 양 경로 최전선에 부착.

**라이브 결함**: 미러 부착 직후 로그가 초당 ~150MB 폭주 — 내용은 스타트업 출력
3KB가 무한 리플레이. **이분법 실측**(미러 OFF→플러드 0 / 콘솔 쓰기만 OFF→3KB
멈춤 / 파일 쓰기만 OFF→콘솔 관측 불가)로 콘솔 쓰기가 방아쇠임을 확정하고
핸들 각인+프로브 문자열 실측으로 봉합: **`_dup2(wfd, 1)`가 fd1의 옛 핸들을
닫고, 해방된 핸들테이블 슬롯이 파이프 쓰기단 복제 핸들로 재할당된다** —
dup2 전 저장해 둔 "원래 콘솔" 핸들값이 파이프 쓰기단을 가리키게 되고, 미러가
그 핸들에 쓰면 자기 파이프를 먹이는 무한 루프(프로브 문자열이 파이프로 에코
되돌아옴을 직접 관측).

**픽스**: dup2 전에 `DuplicateHandle`로 stdout의 사본을 확보하고 미러는 그
사본에만 쓴다 — 사본은 자기 슬롯을 소유하므로 원본 슬롯 재할당과 무관.

**검증**: 픽스 후 4s/5s 실행 2연속 로그 3,154바이트(픽스 전 3s에 427MB),
`crash_probe`(null 역참조) ×2 → `state/logs/crashprobe_*.dmp` 39KB 생성 2/2.
probe_workshop 17체크 ×2 ALL PASS(api-catalog 포함, 미러 부착 상태 서버).

**레슨**:
1. **Win32 핸들 값은 슬롯 재할당으로 별개 객체를 가리킬 수 있다** — 저장해 둔
   핸들을 "그 객체"로 신뢰하면 안 된다. fd를 닫는 CRT 함수(`_dup2`)와 핸들
   저장 조합은 특히 위험. 사본 확보(DuplicateHandle)가 정석.
2. **플러드 내용 판독이 최단 루트** — 리플레이 형태(같은 스타트업 문단 반복)
   자체가 "생산자 무한 루프가 아니라 소비-재기입 루프"라는 뜻이었다.
3. **이분법 env 스위치 한 빌드**(JK_MIRROR_NOCON/NOFILE)로 세 갈래를 한 번에.
