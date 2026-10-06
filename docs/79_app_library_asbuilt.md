# docs/79 — 앱 라이브러리 as-built (2026-10-07)

스펙 docs/superpowers/specs/2026-10-06-app-library-design.md(결제판 — Q1 A 허브
앱+런처 아이콘 뷰 유지 / Q2 uninstall 스킵 / Q3 미출하 슬롯 제외 / 갤러리→
앱 라이브러리 개명)의 as-built 원장. 플랜:
docs/superpowers/plans/2026-10-07-app-library.md. SDD 세션 원장:
.superpowers/sdd/2026-10-07-app-library/progress.md(T1-T5 dispatch·review·
fix 라운드 전례 기록). 문체·절 구성 선례: docs/77.
BASE=4f4ce5a(스펙 결제판·개명판). 원장 체인: docs/67:144 대전제(로컬 라이브러리만)
→ docs/74 문화 → docs/76 능력 배지 → docs/77 출하 도구 → **라이브러리(본 문)**.

## 0. 스펙 체인 — 결제 → 구현 대응

| 스펙 | 구현 | 비고 |
|---|---|---|
| §0 전제(로컬만·읽기 전용) | T1 카탈로그 — 스캔 함수는 어떤 파일도 생성·수정하지 않는다(try/catch 0) | Q2 스킵과 정합 |
| §2 발견 규약 3원+1 | T1 `jk::LibraryScan` — .jkx/콘솔/built-in, 슬롯(Q3) 제외 | 발견 규약 테스트 = 케이스 1m |
| §3 A 허브 앱 | T3 `jkapp_library` 모듈(스펙 Q1 A) | settings/notes/files 4호 멤버 |
| §4 posix 개방 | T1(std::filesystem — 퍼블릭 경로 try/catch 0, 예외 전파 설비 없음)·T4(WSL 실측)·T5(폰 실측) | 런처 `#ifdef _WIN32` 무접촉 — 스펙 Q1 룰링 준수 |
| §5 스코프 밖(uninstall 없음·런처 무접촉) | 접촉 0 실증 — T1 전 커밋에 JKDesktopShell.cpp 부재, T3 스테이지 검증 `git status --short` | |
| §6 테스트 전략 | T1 셀프테스트 1m + T2 CLI probe + T4·T5 실측 | 갭 없음(플랜 self-review 대응) |
| §7 결제 원장 | Q1 A = T3 · Q2 스킵 = 제거 도구 부재 · Q3 제외 = 카탈로그 슬롯 leg 없음 · 명칭 = jkapp_library·library-list·docs/79 | |

## 1. 배선 — 카탈로그 / 모듈 / CLI / probe

- **카탈로그(jkcore pure, T1)**: `jk::LibraryScan(basePath, out)` — 진실원 1개.
  3원: `apps/*.jkx`(MANI name·title·capabilities·icon) / `apps/<n>/manifest.json`(quickjs
  throwaway 런타임 파싱) / built-in minesweeper·tetris(파일 게이트 없음 — lf/hx는
  apps-bin 존재 게이트). `LibrarySource{Jkx,Console,Builtin}`, `LibraryEntry.source` 필드
  (brief 스니펫 누락 → Produces 블록 진실원 복원, T1 수리 ①). 동명: .jkx가 콘솔·
  런처 폴백을 이긴다(JKDesktopShell.cpp:251-284 hasJkx 규약 동형). ICON wanted 산식 =
  **무조건 2x 우선** `icon2x 비었으면 → icon`(JKLibraryCatalog.cpp — 런처 :400 스케일
  종속 분기 미사용, T3 디코더도 동일 공식으로 정합). engine/CMakeLists.txt jkcore
  STATIC 소스 리스트에 `src/JKLibraryCatalog.cpp` 추가.
- **모듈(jkapp_library, T3)**: `ClientLibraryApp.h/.cpp`+`JKAppModule_library.cpp`,
  meta name="library", 창 920x640. Begin(`라이브러리`/`Library`)→좌 목록(행 높이 36,
  `AlwaysVerticalScrollbar`)→우 상세(MANI 원문·크기·경로)→실행 버튼
  `launch_app {"app":"<escaped>"}`(SendQuery 래퍼 settings 선례 본사). 능력 배지 문구는
  **복제가 아니라 `ClientScriptApp.h`의 `CapabilityBadgeText` 인라인 직접 include** —
  문구 단일 진실원("능력 없음"도 숨기지 않는다, docs/76 동형). 아이콘 텍스처는
  OnClose에서 해제(JKClientApplication.cpp:260 OnClose가 DestroyHiddenRenderer보다
  먼저 불림 — ClientShotApp 선례 동형). 클라앱이 자기 기기 apps/를 읽는다 — 수신
  기기에서는 폰의 apps/가 진실원(스펙 §3 함정 원장 항목, 자동 성립).
- **CLI(library-list, T2)**: `./jkdesktop library-list [base]` — 서버 불요 순수 stdio,
  `#ifdef` 게이트 없음(TX6 posix 개방 선례). 행 포맷
  `name=<name> title=<t> source=<jkx|console|builtin> caps=<원문> size=<n> path=<p>` —
  caps 빈값도 `caps=`로 인쇄(빈 선언을 숨기지 않는다). 꼬리 `count=<n> base=<base>`.
  기점 생략 = exe dir(끝 경로성분 제거, 뒤 구분자 없음), apps/ 부재 = count=0 정당 상태
  (오류 전파 없음).
- **probe 3종**: `engine/tools/probes/probe_library_list.ps1`(Windows 전용 —
  LIBRARY-BASE/CAPS/ARG 3정판+CLEAN, UTF-8 BOM, PSI raw-Arguments) /
  `wsl_library_boot.sh`(WSL — ninja 빌드→CLI→부팅→ping→launch→list_windows→철수,
  hard-fail `exit 1`+`LIBRARY-BOOT-FAIL:`) / `phone_library.sh`(폰 드라이버 — tar
  over-ssh 재배포+폰 remote script 생성 실행, `LIBRARY-PHONE-OK`).

## 2. 실측 영수증 (전부 리포트 원문 — as-far-verified)

### 2.1 Windows (T1·T2 — 서버 기동 없음, 라이브 스택 무접촉)

- 셀프테스트 케이스 1m: **전체 PASS**(1m-0·1m-0b·1m-1..1m-16·1m-z) — `.jkx 발견·MANI
  title 전승·능력 원문 보존·source=Jkx/Console·ICON 부재 폴백·크기·절대 경로·콘솔
  계약(발견·source·desc 기반 title·빈 능력·절대 경로)·.jkx 우선(동명 콘솔
  스킵)·무효 컨테이너 스킵·내장 항상·lf/hx 파일 부재 제외`.
  `AppSelfTest: 0 failure(s)`.
- `library-list` 실기 base(exe dir=engine/build) **count=29** — 이 기기에는
  minesweeper.jkx 등이 설치돼 있어 `name=minesweeper`는 `source=jkx`가 이긴다
  → **built-in 단정은 격리 가짜 트리에서**(LIBRARY-ARG). workshop.jkx 실측
  `caps=widget,timer,canvas,agent,fs` — 실기 base에서도 능력 원문 전승 부수 영수증.
- probe(probe_library_list.ps1): 본편 런 3정판 OK+POSIX SKIP+CLEAN →
  fix r1(LIBRARY-POSIX 사망 분기 **제거** — ps1은 posix 런을 정직하게 소유할 수
  없고 WSL 검증 소유권은 T4 .sh로 이관, 컨트롤러 룰링) 후 재런이 canonical:

  ```
  LIBRARY-BASE: OK (count=29 lines=29 minesweeper=1)
  LIBRARY-CAPS: OK (caps=timer,canvas verbatim, count=30)
  LIBRARY-ARG: OK (expect=4 got=4 galapp=console jkx=capgauge builtins=2)
  LIBRARY-CLEAN: OK
  done
  ```

  LIBRARY-CAPS는 slot-pack capgauge(표 순서 timer,canvas)로 능력 선언 jkx를 조립해
  원문 일치+count 정확 증가를 단정(docs/77 scratch 선례 재용). LIBRARY-ARG의
  builtins=2가 격리 트리 built-in 단정(terminal: 부재 동시 단정). 재런 2회 모두 동일
  OK — capgauge 잔존 0.

### 2.2 WSL (T4 — LIBRARY-BOOT-OK, probe rc=0)

```
=== 1. ninja rebuild (buildwsl) ===        ← jkapp_library.so 신규 빌드(사전 부재 실증)
WSL-BUILD-RC=0
=== 2. library-list === rc=0 — trigger jkx 4(trig_build·trig_idle·trig_crash·rate_probe) + built-in 2
count=6 base=/mnt/i/progwork/JKENGINE/engine/buildwsl
=== 3. boot (setsid nohup, WSLg :0) === server pids: 15270 15303
ping reply: {"ok":true,"pong":true}
=== 4. launch_app {app:library} === launch reply: {"ok":true}
=== 5. list_windows === {"ok":true,"windows":[{"id":4,"title":"Library","pid":0,"x":200,"y":40,"w":920,"h":640,"dw":920,"dh":640,"focused":true,"minimized":false}]}
=== 6. cleanup (pkill) === LIBRARY-BOOT-OK
```

- 서버 로그 부수 영수증: `JKClientSurface: connected surfaceId=4 size=920x640` /
  `JKWindowServer: client surface 4 created (920x640)` / **`[library] apps=6`** —
  스폰된 클라가 CLI와 동일 `jk::LibraryScan` 진실원을 먹고 같은 6을 센다(카탈로그
  단일 진실원의 런타임 증명). list_windows title=Library는 T3 Critical의 안티 토큰
  (폴백 결함이었으면 title=Debug로 어긋난다).
- fix r1(MSYS 헤더 전면화+EOF) 후 재런 — 전부 green, `PIPE_RC=0`(PIPESTATUS 직접
  수령). WSL 환경 사실: NotoSansCJK ttc 부재 → 폰트 비트맵 글리프 폴백(기존 환경
  조건 — 목록 창 타이포 영향, 결함 아님).

### 2.3 폰 (T5 — LIBRARY-PHONE-OK, aarch64 실측)

- tar-over-ssh 재배포 7종(카탈로그 헤더/.cpp·모듈 헤더/.cpp·JKAppModule_library.cpp·
  main.cpp·CMakeLists.txt) **296960 bytes**, 폰 측 stat 7행 전부 도달.
- 폰 리빌드(첫 배포라 사실상 전량 170-171 target, `ninja -C buildterm -j4`, aarch64)
  **rc=0** — `jkapp_library.so` 16,267,552 bytes 부재→성립 확인.
- 폰 selftest **rc=0, `AppSelfTest: 0 failure(s)`**(aarch64 포함 3축 중 폰 축).
- 폰 `library-list` **count=3**(run 2 원문):

  ```
  name=phoneprobe title=phoneprobe source=jkx caps=widget,timer size=14849578 path=/data/data/com.termux/files/home/JKENGINE/engine/buildterm/apps/phoneprobe.jkx
  name=minesweeper title=Minesweeper source=builtin caps= size=0 path=
  name=tetris title=Tetris source=builtin caps= size=0 path=
  count=3 base=/data/data/com.termux/files/home/JKENGINE/engine/buildterm
  ```

  요구 두 진실원(phoneprobe jkx+minesweeper builtin) 전부 — posix 스캔 개방(스펙 §4)
  의 폰 도달 영수증. path는 폰에서 `/data/...` 슬래시 원문(WSL의 혼합 구분자와 대비).
- 부팅(tx4_boot, DISPLAY=:1 표준)→ping ok→launch ok→ **list_windows id 4 Library
  920x640 focused** → srvx.log `JKWindowServer: client surface 4 created (920x640)`
  + **`[library] apps=3`** → `LIBRARY-PHONE-OK`.
- fix r1(ninja rc 전파 수리) 후 재실행 — 전 영수증 재취득, DRIVER-EXIT=0(진짜), rc
  경로는 로컬 동형 시뮬(rc3)로 음성 루트 별도 실증. 폰 최종 상태: 서버 UP+Library
  창 920x640 focused 유지.

### 2.4 사용자 눈확인 — **대기 중**

폰 화면에 Library 창(id 4, 920x640 focused)이 떠 있는 상태로 T5가 종료했다 —
**사용자 눈확인 결제 기록 없음. 본 원장 기준 눈확인 대기**(서버를 끄지 말 것 — T5
지시 준수). Windows 라이브 스택에서의 허브 앱 GUI 육안도 미결 — Windows 서버
스폰 금지 원장으로 probe가 소유할 수 없어 WSL probe가 부팅 수준까지만 커버한다.
list_windows는 픽셀을 커버하지 않는다 — 목록·배지·상세·스크롤의 실제 렌더는
사용자 눈확인 목표(WSLg 창 또는 폰 화면).

### 2.5 final review 수리 런 (2026-10-07 — 리뷰 판정 "With fixes" 단일 수리 커밋)

- **콘솔 런치 계약 복원**: 카탈로그 콘솔 엔트리 `appName = "terminal:" + cmd`
  (launch_app 접두 면제 경로 — manifest 이름 원문이던 결함 상태 폐기. 이름이
  그대로 가면 서버 존재 검증에서 unknown_app). library-list 실기 검증:
  `name=terminal:sampletodo.cmd title=SDK 샘플 — 할 일 뷰어 + jkctl ask
  source=console`. selftest 1m-9/1m-18이 `terminal:` 접두+cmd 포함 계약을 잠근다.
- **MANI 원문(스펙 §3 구현)**: `LibraryEntry.manifestRaw` 신설 — jkx=패키지 내
  첫 MANI형 엔트리 bytes / 콘솔=manifest.json bytes / 내장=""(블록 숨김). 상세
  창 하단 "MANI 원문" 블록이 원문 그대로 인쇄(TextUnformatted, 가공 없음).
- **셀프테스트 1m 확장**: 1m-17(jkx MANI 원문 비공백+매치)·1m-18(terminal: 접두)
  ·1m-19(콘솔 manifest.json 원문) 추가 — Windows 전체 **395 PASS** (수리 전 392
  +3).
- **Windows 축**: `ninja -C engine/build -j3` rc=0 → `AppSelfTest: 0 failure(s)`
  → `library-list` rc=0 **count=29**(계약 불변).
- **WSL 축**: `ninja -C buildwsl -j3` rc=0 → `AppSelfTest: 0 failure(s)`(PASS
  374 — Windows와 케이스 절 대 수 다른 것은 플랫폼 조건부 케이스 때문) →
  `wsl_library_boot.sh` 전체 green. 어설션 강화 통과: ping
  `{"ok":true,"pong":true}`에 `"ok":true` 잠금, list_windows의 Library 오브젝트
  절단 후 `{"id":4,"title":"Library",...,"w":920,"h":640,...,"focused":true}`
  단정 → `LIBRARY-BOOT-OK`. (§2.2의 T4 원 런 영수증은 어설션 강화 전 값이다.)
- **폰 축**: `phone_library.sh` 재실행 전체 green — NINJA-RC=0, selftest rc=0
  `AppSelfTest: 0 failure(s)`, library-list rc=0 count=3, launch ok, list_windows
  id 4 Library 920x640 focused(신설 id·기하·focused 어설션 통과), srvx.log
  `created (920x640)`+`[library] apps=3` → `LIBRARY-PHONE-OK`. 종료 상태=서버
  UP+Library 창 유지(사용자 눈확인 대기 재산출).
- **프로브 진단 보존(T5 잔여 ⑤ 소각)**: phone_library.sh ninja NOUT을 FAIL
  판정보다 먼저 인쇄하는 순서로 교정 — 실패 경로에도 `tail -12` 진단이 도달한다.
  wsl_library_boot.sh는 무해(ninja tail이 파이프로 직접 인쇄 — 실패 분기 유실
  없음)여서 무수정.

## 3. 함정 원장 (전부 실측 — 재발 방지 표준)

1. **toybox `pgrep -x` 폰 전면 거짓음성.** 폰에서 13시간 살아 있는 서버+클라 3개가
   있는데도 `pgrep -x jkdesktop` rc=1(toybox는 argv[0] basename 비교 — `./buildterm/
   jkdesktop`; comm은 정상). 컨트롤러 pre-flight도 동일 함정으로 SRV-DOWN 오판.
   **폰 프로세스 측정 표준 = `pkill/pgrep -f 'buildterm/[j]kdesktop'` 브래킷 계열**
   (자기 cmdline에 리터럴이 없어 자기매칭 무해) — phone_library.sh 헤더·실행부가
   이 형태로 봉합. WSL에서의 `pgrep -x` 2-pid(서버 합성)는 별개 관측 — 부팅 직후
   서버는 같은 이름 프로세스 2개(렌더/태스크바 계열)를 띈다.
2. **Git Bash MSYS path conversion.** `wsl.exe -- bash /mnt/i/...` 실행 시 MSYS가
   /mnt/i를 `C:/Program Files/Git/mnt/i/...`로 파손해 파일 실행 자체가 사망.
   **`MSYS_NO_PATHCONV=1` 접두 필수** — wsl_library_boot.sh 헤더의 표준 호출 줄에
   이유 라인까지 기록(T4 fix r1 전면화; phone_library.sh 헤더도 동일 계약 문서화).
3. **폰 재배포 = 사실상 전량 리컴파일 ~170 target ≈9-10분.** 배포 tar가 윈도 쪽
   mtime(00:32-00:50)을 실어 오면 폰 산출물 mtime 순서 문제로 스큐 의심 — 관찰
   권고(원장 concern). rc 계약은 만족; 재실행 속도·긴 빌드 중 절사 리스크는
   termux-wake-lock으로 완화된 상태. tar 자체의 mtime 미래 경고(0.75-0.86s)도
   무영향 실측.
4. **호출측 `| tail` 파이프가 종료코드를 마스킹.** 초기 진단의 `... | tail` 뒤 `$?`는
   tail의 0 — probe 도입부의 FAIL(exit 1)이 보이지 않는다. **probe 진짜 rc는
   PIPESTATUS 직접 수령**(T4 `PIPE_RC`, T5 `DRIVER-EXIT=$?` — 파이프 없이).
5. **tar extract `-C ~/JKENGINE` 중첩 트랩.** repo 루트에서 만든 tar를
   `-C ~/JKENGINE`으로 풀어야 engine/engine 중첩이 안 생긴다(docs/78 §4.5 계약 —
   phone_library.sh가 그대로 준수).
6. **ImGui Begin 후 즉시 End() — implicit "Debug" 폴백 창.** Begin/End 사이 본문이
   비면 위젯은 imgui.cpp:7521 폴백 창에 그려진다(settings의 바깥 창 Begin/End
   스니펫을 몸통 창에 오이식한 것이 원인). **빌드·셀프테스트·링크는 전부 green으로
   통과 — review만이 잡은 결함(T3 Critical).** GUI 형태 결함은 빌드 3축으로
   검증 불가 → 육안/영수증 게이트로 승격한 근거. fix = settings `##settingsbody`
   형태(`if (!Begin(...)) { End(); return; } 본문 End()`) 회수.
7. 소액(참고): PS 5.1에서 `$IsWindows`=$null — `major -ge 6` 게이트를 먼저(T2).
   probe .ps1은 UTF-8 BOM 필수(docs/77 §5 승계). wsl.exe 인라인 인용 파열 → WSL
   스크립트는 파일 실행 표준(docs/78 승계). jkbridge.exe relink Permission denied =
   살아 있는 게이트웨이 점유 — 타깃 단위 링크로 회피, 무해(전 빌드 공통 관측).
   폰 텐푸는 probe 임시 파일만 소각하고 서버·창은 건드리지 않는다(측정 노트 계약).

## 4. deferred minors 트라이아지 — 전부 유예 유지

채택 처분 = **이 라인에서 전부 deferred 유지**(수요 없으면 소각 없음 — docs/77 §7
선례). 최종 전체 리뷰(분기 완결 시)가 승격 여부를 다시 판정한다.

| 태스크 | 항목 |
|---|---|
| T1 | ① 신규 파일 trailing newline(후속 fix 라운드들이 각자 자기 신규 파일의 EOF를 보강 — T3 3파일·T4·T5) ② 1m-14 badFound 불가능 카운터(중복 안전망 — n==5가 실제 보증) ③ 콘솔 title desc-else-name = 브리프 명시 요구라 준수(결함 아님) |
| T2 | ReadToEnd 이중 파이프 데드락 패턴(선례 형태 — 현 물량 무해) · LIBRARY-ARG가 .jkx 부재 시 오인 소비하는 fail 문구 · caps scratch가 실제 build/apps에 쓴다(선례+CLEAN 수령으로 방어됨) |
| T3 | 16ms 타이머 → 항시 ~60fps 렌더(settings 가족 선례 계약 — 허브 가족은 docs/78 더티프리젠트·활동 게이트 백로그 소속) · 빈 벡터 reserve 관용 호출. OOO: EscapeJson 3호 복제(per-app 익명 헬퍼 — 공용 헤더 호이스트 후보) · SourceLabel↔CLI source= 토큰 정합 1행 검산 |
| T4 | ping/list_windows 어설션 성숙도(`{"ok":false`도 ping 통과·920x640+focused 미어설션·서버 로그 교차 검산 행 부재) · teardown 잔존이 WARN 인쇄 후 exit 0(소프트패스 — 채택 판정=방어 가능, 승격 후보) |
| T5 | 기하 어설션(w:920/h:640/focused 미가드) · 누적 로그 grep 상류성(srvx.log rotation 미특정) · `pkill -f` 협피해 클래스(path 문자열을 포함한 cmdline이 pre-clean에 사망 — 단일 세션 잔여) · 고정 sleep 15 → 폴링 루프 · **실패 경로 진단 소각 — ninja 출력이 NOUT에 캡처된 뒤 `\|\| FAIL`가 exit 1로 빠져 `tail -12` 미인쇄(T5 fix 자체의 잔여 — fail-loud는 유지)** |

## 5. 커밋 원장 (전부 rev-parse 실측 값)

| 커밋 | 내용 |
|---|---|
| 4f4ce5a3d5…1535d | BASE — 스펙 결제판+개명판(사전 준비, 플랜 대상 밖) |
| 9aba84c89ac7…91767f | T1 — 카탈로그 3원 스캔 jkcore+셀프테스트 1m |
| e432e0e2408f…866ec | T2 본체 — library-list CLI+probe(posix 개방) |
| c152b14d590c…54d71 | T2 fix r1 — probe posix 분기 제거(WSL 소유권 → T4 이관) |
| 118194197568…254a1 | T3 본체 — 허브 앱 모듈 jkapp_library(스펙 Q1 A) |
| 9e87a15a6639…fa6d8 | T3 fix r1 — Begin/End 안 회수(Critical)+좌 창 스크롤바+EOF |
| b25391b2bb72…92f48 | T4 본체 — WSL 부팅·런치 probe(LIBRARY-BOOT-OK) |
| 619017e280e4…3ad3e1 | T4 fix r1 — 헤더 표준 줄 MSYS_NO_PATHCONV=1 전면화+EOF |
| 3cd2cc82e0e4…65d86 | T5 본체 — 폰 실측 probe(posix 스캔 폰 도달) |
| 9fbd713e6a37…3beab41 | T5 fix r1 — ninja rc 전파 부활+pkill 코멘트 정합+\|\|true 해소+EOF |
| (본 커밋) | docs/79 as-built 원장+스펙 상태 주석 as-built 링크 1행 |

전체 체인 = 4f4ce5a..9fbd713(T1-T5, 수리 라운드 4건 — T2·T3·T4·T5 각 1회)+
본 커밋. 기억 갱신 40은 repo 밖(커밋 미포함).
