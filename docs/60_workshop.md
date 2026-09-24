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
- **[소각 2026-09-24] 그리기(캔버스)/키보드/마우스 이벤트 API** — §10 캔버스 API
  v5로 소각 (첫 위젯이 v1으로 충분함이 실측된 후 게임·토이 층 개설)
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

## 10. 그리기(캔버스)·키보드·마우스 이벤트 API — v5 (2026-09-24, 백로그 ① 소각)

**배경**: §5 백로그 ① "그리기(캔버스)/키보드/마우스 이벤트 API — 게임·토이용".
첫 실용 위젯이 v1(버튼/라벨/에디트)으로 충분함이 실측되었으므로(할일 판), 이제
게임·토이 층을 연다. 애니메이션 토이(공 튀기기, 스네이크)는 (1) 픽셀 그리기 +
(2) 타이머(이미 setInterval로 있음) + (3) 키/마우스 입력 — 세 재료 중 앞·뒤가
없었다.

**결정된 설계**:

1. **캔버스 = 유지(retained) 옵 리스트 컨트롤** — `JKScriptCanvas`(jkcore,
   `include/JKScriptCanvas.h`). 스크립트가 그리기 호출을 할 때마다 `Op` 구조체가
   캔버스에 쌓이고 `OnPaintClient`가 매 paint마다 `GetScreenClientRect()` 원점
   오프셋으로 리플레이한다(리테인드 모드 — 즉시 모드가 아닌 이유: 프레임워크의
   그리기는 dirty-rect 무효화 기반이라 리페인트가 자주 재호출되고, 컨트롤이 자기
   장면을 소유해야 재현된다 — JKButton 선례). 옵 상한 4096 — 초과 시 새 옵은
   폐기+한 번 경고. 애니메이션은 `canvasClear()` 후 다시 그리는 주기로 옵 리스트를
   유지한다(문서화).
2. **그리기 바인딩**: `createCanvas(rect)`(id 반환, 포커스 가능) /
   `canvasClear(id, color?)` / `canvasRect(id, x,y,w,h, color, filled?)` /
   `canvasPixel(id, x,y, color)` / `canvasLine(id, x1,y1,x2,y2, color)` /
   `canvasCircle(id, x,y,r, color, filled?)` / `canvasText(id, x,y, text, color)`.
   색은 `0xRRGGBB` 숫자 또는 `"#rrggbb"` 문자열. 좌표는 캔버스 로컬 픽셀.
   canvasText는 위젯 텍스트와 같은 KSSM 경로(Utf8ToKssm) — 한글 안전.
3. **이벤트 콜백(스크립트가 전역 함수로 정의, 정의 없으면 무시 — additive)**:
   `onMouse(type, x, y, canvasId)` — type은 `"down"|"up"|"move"`, 좌표 캔버스
   로컬. `onWheel(dy, x, y)` — dy는 휠 델타(양수=위), 좌표는 마지막 마우스 위치
   (프레임워크의 휠 이벤트는 좌표가 없어 포커스 컨트롤로 간다 — JKWindow::RespondMessage
   실측). `onKey(key, down)` — key는 SDL 키코드, down은 1/0. 키 이벤트는 포커스를
   가진 캔버스로만 간다(에디트 포커스 중이면 에디트가 먹는다 — 정상).
4. **이벤트 경로는 컨트롤 표준을 따른다**: MouseDown에서 SetFocus +
   `g_jkAppHost->SetCapture(this)`(드래그 중 move 계속 수신 — JKButton 선례),
   MouseUp에서 ReleaseCapture. 응답된 이벤트 좌표는 스크린 공간이므로 캔버스가
   `GetScreenClientRect()` 원점으로 로컬 변환 후 호스트에 전달.
5. **호스트 디스패처**: `JKScriptHost::DispatchCanvasMouse(id, kind, x, y)` /
   `DispatchCanvasWheel(id, dy, x, y)` / `DispatchCanvasKey(key, down)` — 전역
   onMouse/onWheel/onKey를 호출. DispatchClick 선례와 같은 예외 정책(예외는
   [script] 로그로 덤프, 스크립트 계속).
6. **계약 동기화**: 바인딩 추가는 jk.d.ts(v5)와 kApiCatalog(§8) 둘 다에 반영 —
   main.cpp §2.4 자기검사(d.ts 선언 ⊆ 런타임 BoundNames)가 한쪽을 지키고,
   카탈로그는 `api` 도구 소비 LLM을 지킨다.

**의도적으로 제외**: Char/TextEditing 콜백(타이핑 게임은 v6로 — 텍스트 조합은
JKEdit가 이미 잘 한다), 다중 버튼 마우스(좌클릭만), 캔버스 리사이즈 재스케일.

**검증 계획**: (1) main.cpp 스크립트 자기검사에 캔버스 케이스 추가 — createCanvas
컨트롤 실존 + 옵 리플레이는 캡처로. (2) 라이브 프로브 probe_workshop_canvas:
set_script로 캔버스+이벤트 스크립트 설치 → send_input 클릭/키 주입 → capture_window
해시 변화로 그려짐 검증, ×2. (3) 회귀 probe_workshop + test-script(§2.4 d.ts
대조) ×2.

### 10.1 검증 (as-built, 2026-09-24)

- **단위**: `jkdesktop test` 292 PASS / 0 실패 — 신설 캔버스 케이스 4건
  (canvas script boots / createCanvas registers a focusable control /
  dispatchcanvasmouse drives script onmouse / dispatchcanvaskey drives script
  onkey) + §2.4 d.ts 대조 (jk.d.ts declared functions all bound).
- **라이브**: probe_workshop_canvas ×2 ALL PASS — c1 스크립트 설치, c2 onCreate
  장면 캡처 해시, c3 injectMouse 이벤트가 그림, c4 injectKey 이벤트가 그림
  (스크립트 자기 주입 경로 = 실 RespondMessage 라우팅, permissions.json 무편집),
  c5 api 카탈로그 캔버스 등재, c6 정리+진실원 복원. 회귀 probe_workshop 17체크
  ×2 ALL PASS.
- **첫 런이 잡은 배포 함정**: jkdesktop.exe만 재빌드해도 workshop.jkx 속
  jkapp_script.dll은 옛날 것 — `createCanvas is not defined`로 스크립트 실패.
  JKScriptHost를 건드리는 바인딩 추가에는 **jkapp_script.dll 재빌드 +
  pack_workshop.ps1 재팩**이 세트다. 레슨 §6.4(jkctl pack이 신필드를 떨구는
  문제)와 짝 — 컨테이너 안 DLL의 진부함은 컴파일 오류가 아니라 런타임
  "is not defined"로만 나타난다.

### 11. 폰 실전 3중 결함 — 라우팅·무음 0·암호 오류 (2026-09-24 오전, 사용자 보고)

사용자 폰 눈확인("> 공튀기는 앱 만들어줘" → "워크숍 앱을 찾을 수 없습니다")에서
출발해 판정하던 중, 프로브 재현이 **3층 결함**을 순차적으로 드러냈다. 전부
커밋 96a0bfc / 6577865로 픽스, probe_workshop_ball ×2 ALL PASS(런 11·12).

**1층 — 라우팅(사용자 보고 그 자체)**: jkagentd의 launch_app 설명이 workshop을
내장 앱 이름 목록에 넣은 실수(워크숍은 .jkx 패키지) + "앱 만들어줘=워크숍"
안내 부재. LLM은 `{"app":"workshop"}`을 쓰고 새로 생긴 스폰 전 검증의
unknown_app에 낙오 → "런처가 인식하지 못합니다"라고 보고한 것. 픽스 3중:
서버 launch_app의 app→jkx 폴백(스키마 설명 드리프트를 앱 차원이 흡수),
설명 교정, 턴 프리앰블에 워크숍 라우팅 지시.

**2층 — 무음 0(프로브 v4 실패에서 역추적)**: LLM이 `createCanvas([10,10,W,H])`
**배열 형태**를 쓰면(문서는 객체 형태만) quickjs가 undefined를 예외 없이 int 0으로
바꾸므로(JS_ToIntegerFree의 JS_TAG_UNDEFINED) RectFromArg가 조용히
rect{0,0,0,0}을 돌려주고 **set_script는 ok:true** — 0×0 보이지 않는 캔버스.
영수증을 뒤져야만 보이는 결함이었고, 폐곡선이 ok:false를 내지 않았으므로
LLM은 자가수선할 근거가 없었다. 픽스: RectFromArg가 배열 형태를 수용하고
누락 컴포넌트는 실패. 교훈 — **"성공했는데 아무것도 없다"는 실패 보고보다
깊다**: 인자 파서의 침묵 폴백은 폐곡선 자체를 무력화한다.

**3층 — 암호 오류**: `setInterval(16, fn)` 순서 착오(정답은 `setInterval(fn,
16)`)가 `thrown value: [uninitialized]`로 보고 — 예외 미설정 JS_EXCEPTION
반환에 QuickJS가 쓰레기를 붙인 것. run 6·8의 LLM은 ok:false를 받고도
메시지를 해석 못 해 같은 스크립트를 재제출했다. 바인딩 가드 전부를
JS_ThrowTypeError로 바꾸고 setInterval 오류는 (fn, ms) 순서를 명시.

**재발한 배포 함정(§10.1 재실측)**: 내가 `jkx-pack workshop`으로 재팩했다가
패커가 MANI를 재생성해 `scriptfile=`/`watch=1`을 떨궜다(§6 레슨 그대로) —
워크숍이 일반 SCRI 모드로 떨어져 에이전트 도구 등록 자체가 사라짐
(unknown_app_tool). 정식 경로는 **tools/pack_workshop.ps1**뿐.

**사고 기록(투명 공개)**: 수동 set_script 검증 도중 myapp.js(사용자 진실원,
261바이트)를 백업 없이 덮어써 원본 분실 — 영수증·세션 트랜스크립트 휩쓸기로도
복구 불가, docs/60 §2.1 공식 시드 템플릿으로 재시드(248바이트). 백업 의무
규칙은 프로브뿐 아니라 **수동 ad-hoc 검증에도** 적용된다. 사용자가 갖고 있던
원본은 폰 채팅으로 한마디("워크숍에서 ~ 만들어줘")면 재생성된다.

## 12. 폰 실전 2차 — 성공과 4결함 (2026-09-24 오전, 사용자 폰 대화 원문 분석)

§11의 픽스 후 폰 첫 성공 세션("공튀기는 앱" 완성 → 클릭 스폰 → 동물 얼굴 →
장애물 충돌까지 10턴 이어짐). 워크숍 라인이 처음으로 끝까지 살아있었고,
그 과정에서 다음 결함이 새로 잡혔다. 원본 증거 = claude 세션 트랜스크립트
+ receipts.jsonl (10:23-10:37).

**①launch_app 경로 표기 변주 — unknown_jkx**: LLM이 같은 패키지를
`{"jkx":"workshop"}` / `{"jkx":"workshop.jkx"}` / `{"jkx":"apps/workshop.jkx"}`
로 번갈아 불렀다. §11의 bare-이름 폴백은 첫 형태만 살렸다. 트랜스크립트가
실패 인자를 정확히 증언(apps/workshop.jkx, workshop.jkx). 픽스 = 서버 jkx
해석 정규화: '/'→'\', 끝 .jkx 탈락, 선두 apps\ 탈락 후 (원문 / exeDir 원문 /
exeDir\apps\정규화.jkx / exeDir\정규화.jkx) 4후보 순차 검사. 실측: 세 형태
전부 ok:true. 레슨 — **LLM이 부르는 이름의 변주를 서버가 흡수하는 쪽이
정답이다. "스키마대로 불러라"는 프롬프트로 못 막는다.**

**②onMouse 버튼 구분 API 부재**: 캔버스 onMouse(type,x,y,canvasId)에 좌/우
구분이 없어 LLM이 6턴 소모(추측→테스트 UI→포기하고 모드 버튼 우회). 서버
와이어·단일 프로세스 양쪽 다 ev.detail에 SDL 버튼(1좌/2중/3우)을 실어
왔으므로 캔버스 싱크가 버리고 있던 것. v5.1 = onMouse 5번째 인자 button 추가
(추가 인자라 4-인자 콜백 호환), jk.d.ts+kApiCatalog+셀프테스트 3점 동기.

**③잔상(트레일)의 구조적 함정**: 잔상 효과 = canvasClear 없이 계속 그리기 →
옵 4096 상한에서 **새 옵이 드롭**되어 공이 얼고 옛 장면이 재생. drop-new는
지우지 않는 스타일 자체를 죽인다. 픽스 = 상한 도달 시 **가장 오래된 옵
퇴출(링 동작)** — 잔상은 계속 그려지고 canvasClear-per-frame 장면은 불변.
(유저가 겪은 "잔상이 사라진 것"의 직접 원인은 LLM이 재작성 중 잔상 코드를
떨군 것 — 엔진 함정은 이것이 다시 성립하지 못하게 막던 구조.)

**④CoT 내레이션 누출의 정체**: 폰에 노출된 "일단 ~ 확인해볼게요"류 문장은
thinking 블록이 아니라 **모델(kimi-k2.7-code)이 도구 호출 사이에 text로
출력한 것**(트랜스크립트 TEXT 블록 확인 — thinking_delta는 파서가 이미
필터). 프리앰블 강화("도구 호출 동안에도 진행 안내·중간 보고 금지, 확인해볼
게요 류도 금지"). 모델 습관이라 완전 차단은 불가 — 완화책.

**부수 결정 — 채팅 디폴트 모델 glm**: 사용자 지정으로 ChatConfig 기본 모델
kimi-k2.7-code:cloud → **glm-5.3-flash:cloud** (2026-09-24). state/chat.json
없으면 이 값이 쓰인다.

**프로브 하니스 레슨 2건**: (a) 서버 인수(갱신 14)와 프로브 전용 서버 기동이
충돌 — 프로브의 서버 기동이 **라이브 jkwinserver를 takeover로 죽이고**,
티어다운이 자기 서버까지 정리해 전멸. 회귀 프로브 돌린 직후엔 라이브 스택
재점검이 절차다. (b) WsConnect가 접속 거부를 **예외로** 던지면 s1 체크가
건너뛰어져 **가짜 ALL PASS**가 뜬다(실측) — 접속 실패는 반드시 null로
정규화해 체크가 실패를 세게 한다.
