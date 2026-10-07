# docs/81 — 앱 커버리지 as-built (2026-10-07)

스펙(전수 인벤토리)
`.superpowers/app-availability-inventory.md`(2026-10-07 — 28 app module `.so`가
양축 빌드 실측, 갭=포장 단계 Windows 전용이라 판정한 문서)의 as-built 원장.
플랜: docs/superpowers/plans/2026-10-07-app-coverage.md. SDD 세션 원장:
`.superpowers/sdd/2026-10-07-app-coverage/progress.md`(T1-T6 dispatch·review
verdict·fix 라운드·룰링 전부). 태스크 리포트 6건(같은 디렉터리의 task-1·2·2-fix·
3·5·5-fix-report.md — 실측 원문의 진실원). 문체·절 구성 선례: docs/79·80.
BASE=9a2174ecd03f511b4cfb4ecffeb959fe7a5a4ca9(chat 라인 fix r2 — T1 dispatch
기점). 원장 체인: docs/79 앱 라이브러리 → docs/80 데스크톱 채팅 → 인벤토리
(본 라인 스펙) → **앱 커버리지(본 문)**.

본 라인의 성격 1행: 근본 원인이 코드 부재가 아니라 포장 단계라서(인벤토리
headline), 산출 대부분은 코드 신설이 아니라 **플랫폼 균등화** — 진짜 신설 코드는
T3 카탈로그 확장(`cmd_posix`)과 T5 taskmgr posix leg(+brief 밖 Hello pid 보수)뿐.

## 0. 문서 체계

| 문서 | 역할 |
|---|---|
| 인벤토리 `.superpowers/app-availability-inventory.md` | **본 라인 스펙 상당** — 앱별 차단 원인·비용 표+제외 판정(T3 실측 정정 블록 :200-208 포함). 헤더에 as-built 포인터 |
| 플랜 `docs/superpowers/plans/2026-10-07-app-coverage.md` | T1-T6 구현 플랜(제외 확정 상속) |
| SDD 원장 `progress.md` | 판정·룰링 진실원(T1 WSL 기대치 29 확정·T2 8/8 CLEAN·T3 무결·T4 park·T5 CLEAN 6/6) |
| 태스크 리포트 6건 | 실측 영수증 원문(위 표의 상위 진실원) |
| **docs/81(본 문)** | as-built — 배선·3축 영수증·함정·deferred·커밋 원장 |

제외 확정(인벤토리 판정 상속 — 이번 라인 밖, 소유자 결정 없으면 착수 없음):
**jkapp_browser + cefosr**(L — CEF Windows 바이너리 배포+windows.h 링크 모델,
docs/70 §4 명명 제외) · **jkchat**(Win32 전용 — PC 방향 별도 문) ·
**launcher-grid `.jkx` scan**(FindFirstFileA — 스펙 2026-10-06-app-library §7
Q1 룰링: 런처 무접촉 유지) · **체화된 U+F05x trigger 4종 정리**(위생 시 probe
어설션+원장 수치 29→25 동시 갱신 패키지 — §4 표).

## 1. 배선 원장

### T1 — jkx-pack posix leg + repack dual-path (c3c9b79)

- `main.cpp` RunJkxPack un-gate(구 `#ifdef _WIN32` :673-793 해제): manifest 조립·
  컨테이너 레이아웃·entry/manifest 키는 공유 — **플랫폼 분기는 모듈 로드 트리오만**
  win32 `LoadLibraryA`/`GetProcAddress` ↔ posix `dlopen(RTLD_NOW|RTLD_LOCAL)`/
  `dlsym`(플랜 G2 RunClientModule posix leg 쌍대, dlerror NULL 가드). dispatch
  :4068 posix 진입(fix r1 현행 좌표 — "jkx-pack is Windows-only in this
  prototype" 메시지 폐기).
- **모듈 이름/접미 단일 출처**: 디스크 실물 = `jkapp_<app><AppModuleSuffix()>` —
  `JKWindowServer.h` `jk::server::AppModuleSuffix()`(.dll/.so) 1원. MODL 키·
  manifest `module=`은 양축 모두 플랫폼 중립 `jkapp_<app>.dll`(slot-pack 선례 —
  키는 RunClientFromJkx FindEntry 소비뿐, 파일명 아님) → **manifest 텍스트 양축
  바이트 동일**(jkx-list 실측), MODL 페이로드만 다름.
- `CMakeLists.txt`: auto-repack 블록(icon sync :952-980+per-app 컨테이너 규칙
  :981-1165+`jkx_packages ALL` :1166)의 `if(WIN32)` 해제 → dual-path. **browser
  규칙만 내부 `if(WIN32)` 유지**(libcef.dll pack-time 의존, docs/70 §4). CMake는
  네이티브 구분자 산출 → posix 전진 구분자 — U+F05x drvfs 체화 경로 재발 0
  (T1 실측 `ls | cat -A` 바이트 청결).
- Windows 무회귀: build rc=0(repack 28스텝)·selftest baseline↔final **바이트
  동일**(412/412)·count=30 유지·기존 산출 TOC 무변(jkx-list version=1 entries=5
  — MANI/MODL/SCRI/ICON 원형·`scriptfile=` 생존)·수동 pack rc=0.
- WSL 신규: auto-repack **24 launcher 컨테이너**(agentmgr..vpres) + legacy
  trigger 4 + chat builtin = **count=29**(T1 룰링 — 정착 기대치를 29로 확정,
  count 29↔30 소결은 별도 소유 결정). 동명 .jkx가 minesweeper/tetris builtin을
  흡수. 수동 `jkx-pack workshop` → 30(수동 유산 — 영수증 후 원상 복원).
  컨테이너 포맷 패리티: WSL산 workshop/scriptdemo jkx-list가 Windows산과 entry
  배열·매니페스트 바이트 동일.

### T2 — WSL 커버리지 probe (80dbcb3 → fix r1 1f8a783)

- `engine/tools/probes/wsl_apps_count.sh` 신설(본체 154행 → fix r1 193행 → T3
  갱신 현재 254행) — count 어설션+특정 행 단정+바이트 레벨 트립와이어+negative
  path+부팅 launch 2종→list_windows 단정+WSL selftest 편승. 리뷰 1 Important
  (drvfs 투명 디코드 민감도 거짓 주장) → fix r1 **§4b Windows 측 바이트 직검**
  신설(WSL interop powershell + .NET 코드포인트 스캔) → **re-review CLEAN 8/8**
  (리뷰어 독립 재실행 — Git Bash ls ASCII 뷰 vs .NET 진짜 이름 대조 성립).

### T3 — 설치 트윈 + 카탈로그 확장 계약 (63cf062)

- **구현자 정정 기록(예측 → 실측)**: 인벤토리·플랜·brief의 "카탈로그 무수정 —
  무접미 게이트가 이미 계약" 예측은 **절반 틀렸다**.
  - (사실이던 절반) **lf/hx leg** `JKLibraryCatalog.cpp:249-257` 플랫폼 접미
    게이트(win32 `.exe`/posix 무접미 — fix r1 현행 좌표; T3 작성 시 :217-223은
    콘솔 분기 삽입 전 구 좌표)는 실측 유효 — 무수정 그대로.
  - (틀린 절반) **콘솔 스캔**(:167-197)은 manifest `cmd`를 **원문 전승**한다 —
    Windows에서 그 `.cmd` 원문이 작동하는 이유는 win32 런처 spawnConsole
    cwd=앱 폴더 계약 전용이다(JKWindowServer SpawnConsoleApp 주석). posix가
    bare `sampletodo.cmd`를 받으면 `/bin/sh -c sampletodo.cmd`로 죽는다.
  - 근거 관측: posix `SpawnProcess`는 child cwd를 **exe dir로 고정**
    (JKWindowServer.cpp posix leg) — 내장 lf 키 `terminal:apps-bin/lf/lf`가
    basePath 상대인 근거(같은 기점 계약).
- **확장 계약(실제 구현 — JKLibraryCatalog.cpp:163·:186-203)**: manifest
  **`cmd_posix`(옵션 필드)** — posix 한정 (a) 유 → basePath 상대 키
  (`apps/<dir>/<file>`)로 승격 + 파일 존재 게이트 **fail-closed**(트윈 결손 시
  스킵 + stderr 1행), (b) 무 → cmd 원문(기존 계약 1:1 — selftest 1m
  "apps-bin/y" leg 보존). win32는 필드 무시(Windows `terminal:sampletodo.cmd`
  행 원형 실측 — 카탈로그 30행 계약 불변). "manifest `cmd`를 `.sh`로 switch"
  대안은 win32 런처를 깨므로 기각.
- 산출물: `engine/apps/sampletodo/sampletodo.sh` 트윈(.cmd 본문 1:1 셸 이식,
  100755) · manifest.json `"cmd_posix": "apps/sampletodo/sampletodo.sh"` 1행 ·
  `JKLibraryCatalog.cpp` 확장 · selftest **1m-t 신설**(별도 temp 트리 — 승격 키/
  결손 스킵/원문 폴백 3계약 플랫폼별 잠금, 본 1m 트리 n==6 계약 무접촉) ·
  CMake **`console_apps` 타겟**(:935 — engine/apps 콘솔 트윈을 `$(CMAKE_BINARY
  _DIR)/apps/`에 자동 스테이징, 수동 drop 폐기) ·
  `engine/scripts/install_lf_helix_posix.sh`(lf r42/helix 25.07.1 핀 — Windows
  축 패리티 실측 `lf -version`=r42·`hx --version`=25.07.1 a05c151b, GitHub
  release 조달·네트워크 명시·**aarch64 자산 존재 실측** — 폰 T4 재용 포인트).
- **조달 바이너리 커밋 기각**: buildwsl은 무추적(무시 아님 — `??`)이라 커밋 자체는
  가능했으나 서드파티 fat 바이너리 2개(lf 5.7 MiB + hx 19 MiB) 블롭 — docs/44
  Windows 축 "기계만 커밋" 정책 계승, 조달기 스크립트만 커밋(멱등 — 부재 시에만
  네트워크).

### T5 — taskmgr /proc leg + brief 밖 Hello pid 보수 (6f4f29c → fix r1 85661c8)

- `ClientTaskmgrApp.cpp` **+210 insertions · 0 deletions** — 신설 전량
  `#ifndef _WIN32`, win32 hunk·무조건부 줄 byte-identical(diff 증명 = Windows
  leg 무변).
- 행당 최소 3종 procfs: `/proc/<pid>/comm`(빈 창 제목 폴백 + 행 툴팁) ·
  `/proc/<pid>/stat`(comm에 공백·`)`이 있어도 필드가 시프트되지 않게 **마지막
  `)` 이후 토큰화** — 셸 `awk $14`의 어긋남 개선) · `/proc/<pid>/status`
  VmRSS(kB→바이트). CPU는 **샘플 간 Δ**: (dTicks / CLK_TCK) / dWall / cores ×
  100 — 갱신 37 렛슨(수명평균 %CPU 무용)대로, win32 leg 동형 "총 머신 용량 %".
  dWall=steady_clock(단조 — 벽시계 역행 방지), pid 재사용 역행은 Δ 스킵.
- 정직 뷰: 행은 살아있는데 /proc 읽기 전멸(hidepid 등)이면 2연속 샘플 확정 후
  `per-process stats unavailable — /proc reads denied (hidepid mount?)` 표시 —
  침묵하는 "..." 방지(컨트롤러 룰링 이행).
- **brief 밖 보수(측정으로 발견 — 리뷰 승인)**: `JKClientSurface.cpp` +8 —
  Hello pid가 `#ifdef _WIN32`에서만 채워져 **WSL list_windows가 `"pid":0`을
  보냈다**(leg 이전 실측). pid=0이면 ① 샘플러가 모든 행을 건너뛰어 /proc 리더가
  옳아도 표가 영구 "..."(태스크 Produces "프로세스 표 채움"의 구조적 불능) ②
  서버 좀비 판정 스윕 `spawnedClients_.find(client->Pid())`(JKWindowServer
  .cpp:8170)가 영원히 미스. leg = **JKAgentClient.cpp:24-28 Hello getpid
  선례 패리티**(리뷰어 원장 대조 승인 — "보수 없이 태스크 불성립" 판정).
  부수 효과: posix crash-detection 경로 활성. 트립와이어 = probe §5 "pid 0
  아님" 어설션.
- probe `engine/tools/probes/wsl_taskmgr_stats.sh` 신설 — **TASKMGR-OK rc=0**:
  bracketed pre-clean → rebuild → selftest → setsid 부팅 → launch taskmgr →
  **900x620 기하 + pid≠0 트립와이어**(20110 실측) → /proc Δ 셸 검증
  (utime+stime 348~354 ticks / 3s, CLK_TCK=100 → 최종 런 335 ticks·cores=12) →
  VmRSS(157984~158048 kB) → capture_window PNG 900x620 → **표 채움 육안 영수증**
  (pid 19633 12.6% 154.0 MB / pid 20110 9.1% 154.3 MB — 셸 VmRSS와 메모리 열
  동일값 검증).
- **fix r1(85661c8 — 리뷰 I1 수리)**: `/proc/stat` sscanf 억제 10→**11**개
  (`%*u` 1행 추가, ClientTaskmgrApp.cpp:108 — 버그 상태 utime←cmajflt·stime
  ←utime 오염). 수리 검증 = **미니 컴파일 실측**(`SSCANF-SELFTEST: OK` — caseA
  합성 stat 수리판 정확·구판 2형 버그 재현 확정, caseB live /proc 독립 스플리터
  대차) + probe 재실행 TASKMGR-OK + **CROSS-CHECK 영수증 신설**(셸 Δ% 9.3 vs 앱
  표 9.7 — 같은 오더; 창 불정렬 유령 실패 방지로 대역 어설션은 의도 비설치) +
  probe minor 3건(EOF newline·CAPTURE-WH 2중 인쇄 소각·pkill 브래킷) →
  **re-review CLEAN 6/6**(억제 11개 blob 실측 + 미니 컴파일 case 3종 전부 정확).

### T4 — 폰 rollout probe (c89e893 → fix r1 4800b36) — **2026-10-08 sshd 회복 후 dispatch**

- park 해제 근거: 사용자 sshd 회복(`sshd`) 후 컨트롤러 폰 도달 실측 — 그때
  채팅 라인 최종 게이트(jkweb 신판 루프백 재기동·PC LAN refused rc=7 =
  노출 소멸 재단정·웹채팅→launch 체인)도 동시 실측(docs/80 §8 결제 대기
  물려있음).
- probe `phone_apps.sh`(411행) — tar 10파일 재배포(T1/T3 변분 전량) →
  aarch64 리빌드(NINJA-RC=0) → selftest → jkx-pack 수동 실측 → **auto-repack
  도달 판정**(진실 질문) → count → launch 2종 → 종료 서버 UP.
- **폰 실행 이력은 run 4가 최초 green**(run 1 probe selftest 절 누락·run 2
  원격 스크립트 변수 오타(`ST_RC/S_RC` — `set -u`가 silent fail) → run 3
  폰 Wi-Fi 편성 낙하(ping 66% 손실, 자발 회복) — §3 함정 #9).
  **auto-repack 폰 도달 = REACHED** — ninja Repacking 24행 + `apps/*.jkx`
  24/24 실재의 2원 합의(fix r1 EVIDENCE-CONSENSUS — 리뷰 I2 "단일 원채점
  오-REACHED" 지적의 수리).
- **병행 세션 협업 트랩(§3 #11)**: close-fix 세션의 미커밋 WIP가 공유 워킹
  트리(=배포 원천)에 상존해 본 probe tar가 오염 판(rebuild RC=1). 본 fix에서
  **배포 오염 게이트**(더러운 `main.cpp` = HEAD blob 스테이징 — 헤더는
  배포 목록 밖이라, 기타 파일 더러움=hard FAIL)을 probe에 봉합. +
  `MSYS_NO_PATHCONV=1`이 `git.exe -C /i/...`의 MSYS 인수 변환도 차단(rc=128
  실측) → git 전용 Windows형 경로 신설(#10).
- phone_library.sh의 접속 하드코드 유산은 본 probe에서 환경변수 계약 신설
  (PHONE_HOST) — 그 파일 자체의 하드코드는 유예(§4).
- hx 폰 bionic exec 거부 — §2.3·§3 #8(조달 결정은 사용자 원장).
- 리뷰: **Spec PASS / Quality Needs fixing(Important 2) → fix r1 → re-review
  CLEAN 5/5**(신규 Minor 4 = r-M1..r-M4 — §4 park 후보 영장).

## 2. 3축 영수증

### 2.0 캐논 — selftest 기준선 진술(T3 리뷰 M4 요구 = 본 원장 결제)

| 축 | 이 라인 시작 | 종료(현재 캐논) | 증가 내역 — **계약 준수 증가만** |
|---|---|---|---|
| Windows | docs/79 결제판 395 | **420 PASS / 0 FAIL** | docs/79(395) → chat 라인 docs/80(412) → 본 라인 T1 실측 412 → **T3 1m-t 8케이스 신설로 420** |
| WSL | docs/79판 374 | **399 PASS / 0 FAIL rc=0** | 본 라인 T2 실측 391 → **T3 +1m-t 8로 399** |
| 폰 | — | **미실측(§2.3)** | sshd 사망 블록 — T4 park |

**T4 전입(2026-10-08) + 후속 라인 부기**: 폰 축 첫 실측 = **406 PASS / 0
FAIL**(399+1n-t 8에 해당하는 T3 트윈 케이스의 aarch64 첫 렌더). 그 직후
병행 라인 chat-close-fix(0e25762, docs/80 §4b)가 1n-s1..s7 7행 신설 —
3축 동시 갱신: **Windows 420→427 · WSL 399→406 · 폰 399→406**(폰은 리빌드
순서상 T4가 close-fix 코드를 포함해 빌드됐으므로 폰 첫 실측치가 처음부터
406 — 폰 406의 요인 분해는 T4 이행분+7과 병행 이행분의 구분 불가 정직
기록). 캐논 표는 라인 결제 시점 절사본 — 이후 캐논 소유는 각 as-built 원장.

캐논 진술의 실재: 기존 케이스 파손 0 — T1 baseline↔final 바이트 동일(412/412),
T3·T5 재실측 420/0, **리뷰 독립 재검증 일치**(T3 리뷰어: Windows 420/0 +
count=30 행 원형·WSL 399/0 + count=32 트윈 3행 — 무결). 이 라인의 플랜
Global Constraints가 인용한 "395/374"는 작성 시점 수치로 만료된 문언(§4 표).

### 2.1 Windows — 기존 유지(회귀 0)

- `library-list` **count=30 불변**: 26 `.jkx` + 1 console(`terminal:sampletodo
  .cmd`) + chat + lf.exe + hx.exe builtins. T2 M4 원문 영수증 fix r1 재실측
  (30 name= 행 rc=0), T3에서 `cmd_posix` 무시 실측 — `name=terminal:sampletodo
  .cmd ... source=console` 행 원형 그대로.
- jkx-pack Windows leg 무변: 수동 pack rc=0, TOC 무변(T1 §2.1).
- 무접촉: permissions.json 전면 allow·Windows `--server` 스폰 금지 — 전 태스크
  전부 준수.

### 2.2 WSL — count 29→32

- T1: auto-repack = **29**(24 launcher .jkx — 전부 전진 구분자·U+F05x 0건 + 4
  legacy drvfs trigger + chat builtin).
- T3: **count 29→32** — 설치 트윈 3행 신설: 콘솔 트윈 1
  (`name=terminal:apps/sampletodo/sampletodo.sh ... source=console`)+조달
  lf/hx 2(`name=terminal:apps-bin/lf/lf`·`name=terminal:apps-bin/helix/hx`
  source=builtin). Windows 30 불변과 병행 실측.
- **twin-hidden negative path**: 트윈 .sh 잠깐 결손 → count=31(fail-closed
  게이트 — 스폰 키를 안 올림) → 복원 → 32 실측.
- launch 단정: settings 900x620 + notes 760x520 list_windows 오브젝트(T2) ·
  taskmgr 900x620 + pid≠0 + /proc 표 채움 육안(T5). 스폰 진실원 = 서버 로그 행
  (T3 — `JKWindowServer: spawned jkdesktop terminal --shell apps/sampletodo
  /sampletodo.sh`).
- probe 2종 전부 rc=0: `wsl_apps_count.sh` **APPS-COUNT-OK**(하한 어설션 391→
  399 갱신·비ASCII 이중 트립와이어) · `wsl_taskmgr_stats.sh` **TASKMGR-OK**.

### 2.3 폰 — T4 실측 완료(2026-10-08 — sshd 회복 후 dispatch)

- **라인 시작 관측치 폰 count=4 → T4 실측 count=28**(조성: launcher .jkx 24 +
  phoneprobe + 콘솔 트윈 + chat builtin + lf — WSL 32 대비 **−4·조성 비동형**
  = WSL 유산 drvfs trigger .jkx 4 vs 폰 phoneprobe = −3, hx dead = −1.
  T4 리뷰 I2 산술 정정 원장). auto-repack 도달 실측 + 수동 `jkx-pack
  settings` rc=0(TOC MODL jkapp_settings.dll)·negative path dlopen
  fail-closed.
- **auto-repack 폰 도달 = REACHED**(태스크의 진실 질문 답): 폰 CMake 레일이
  ninja 레일에서 도달 — T4 fix r1으로 판정이 2원 합의 강결합(ninja
  "Repacking" 행 24 + `.jkx` 실재 24/24 ==, EVIDENCE-CONSENSUS).
- selftest 첫 실측 **406 PASS / 0 FAIL**(캐논 갱신 — §2.0 전입표).
- launch 2종 단정: settings 900x620 + notes 760x520(`list_windows` 오브젝트).
- **hx(helix) 폰 미조달**: helix 25.07.1 aarch64-linux는 glibc 빌드 — bionic
  exec 거부(표시 메시지 "No such file or directory"는 파일 부재가 아니라
  exec 거부 — §3 함정 #8). lf r42는 Go 단일 바이너리로 폰 실구동 확인.
  dead 바이너리 정제 + `.dead-on-phone` 마커(재실행 멱등 — SKIP 경로 영수증
  `HX-PHONE=SKIPPED-DEAD-MARKER` 실측). Termux pkg 조달 = 엔진 밖 별도 문.
- **taskmgr 폰 /proc 편성 미실측**(T5 concern ④ 이행 후보로 남음 — taskmgr
  .jkx 컨테이너 pack 완료, launch 미측정. §4 표 전입).
- 종료 상태: 서버 UP + Settings·Notes 창 + jkweb ALIVE — 사용자 육안 게이트
  대기(§6). probe = `engine/tools/probes/phone_apps.sh` rc=0(run 4 green
  커밋·run 5 재실험도 rc=0 — IP 정화 후 환경변수 PHONE_HOST 계약).

## 3. 함정 원장 (실측 신규 — 재발 방지 표준)

1. **drvfs 투명 디코드 — U+F05C 층별 진실(T1 I1 공허 → T2 I1 발각 원장).**
   NTFS 기록 바이트는 **U+F05C(EF 81 9C** — Git Bash `cat -A`
   `triggersM-oM-^AM-^\...` 실측), WSL drvfs readdir는 이를 **0x5C로 투명
   디코드**(ASCII 뷰 — `ls | grep -P '[^\x20-\x7E]'` 어설션이 이 이름 패밀리를
   영원히 못 잡는다), **Windows 측 직검만 진짜 이름**을 본다(fix r1 §4b — .NET
   코드포인트 스캔; 리뷰 권장 `cmd dir /b`는 U+F05C를 콘솔 코드페이지로 렌더해
   바이트 판별 불안정이라 기각). 교훈: "어느 뷰의 파일명인가"를 어설션 문언에
   쓰지 않으면 공허 어설션이 된다 — re-review에서 리뷰어가 트립와이어를 독립
   재실행해 U+F05C 코드포인트를 실측하며 봉합.
2. **살아 있는 서버 + WSL 재빌드 = drvfs ld 사망(T5, 2회 재현).**
   `/mnt/i`에서는 unlink 불가 — `ld: cannot open output file jkdesktop: No
   such file or directory`. **probe pre-clean(브래킷 pkill)이 빌드 앞에 온다 —
   이 순서는 계약**(실측으로 2회 사망 후 확정).
3. **XWayland XGetImage 전면 BadMatch(T5).** WSLg에서 루트든 리다이렉트 창든
   XGetImage 캡처 불가 → 시각 관측의 정답 = **서버 자체 capture_window 도구**
   (클라 shm 프레임버퍼 그대로 PNG — 900x620 원형). Win32 측 msrdc
   PrintWindow/CopyFromScreen(데스크톱 뷰, 겹침 이슈)은 보조. PNG 하한 20 KB는
   약한 생존 프록시 — 내용 단정은 사람 육안(§4 표).
4. **powershell interop 출력 CRLF(T2 fix r1, 실측 FAIL → 봉합).** WSL interop
  으로 부른 powershell.exe의 표준출력은 CRLF — `\r` 잔류 시 bash 정수 비교가
   "정수 표현이 아닙니다"로 사망(실측 오판 `28 != 28` — 뷰는 28/28 동일). 봉합 =
   **`tr -d '\r'`**(probe :145·:150 — fix r1 현행 좌표; T2 fix r1 작성 시
   :108·:109·:114는 프로브가 254행으로 성장하기 전 구 좌표) — interop 채널 표준.
5. **/proc stat sscanf 억제 개수 오타(T5 I1 — fix r1 수리).** 10개 억제는
   utime←cmajflt·stime←utime 오염(합=cmajflt+utime, stime 누락). **표 CPU%(앱
   채널)와 셸 Δ(probe 채널)는 독립 구현이라 LIVE 영수증이 이 결함을 반증하지
   못한다** — stime 비중이 작으면 (cmajflt+utime)≈(utime+stime) 수렴(정직
   메모). 실질 방어선 = **합성 케이스 컴파일 실측(caseA — 커널 무거운 값)+포맷
   자체 검증**, CROSS-CHECK 영수증 상주(fix r1 신설)가 미래의 크게 틀린 값을
   잡는 보조 트립와이어.
6. **폰 CMake 재생성 사망 — tar 입자 누락(chat T7 원장 docs/80 §6 #2 승계 —
   T4 전방 경고).** CMakeLists를 배포하되 그 안이 참조하는 신규 소스 누락 시
   폰 cmake 재생성이 `No SOURCES given to target`로 사망(NINJA-RC=1, 1s —
   docs/80 실측). T4 배포 목록은 T1/T3/T5의 신규 소스 전량(JKClientSurface·
   ClientTaskmgrApp·sampletodo 3파일·install 스크립트 참조 등) 동반이 원칙.
7. **조달 바이너리 커밋 기각 + buildwsl 무추적(T3).** lf 5.7 + hx 19 MiB ≈25
   MiB 블롭 — **docs/44 Windows 축 "기계만 커밋" 정책 계승**, 조달기
   `scripts/install_lf_helix_posix.sh`(멱등·핀 버전)만 커밋. 부수: buildwsl은
   무시 아닌 무추적 — 조달 산출물(~25 MiB)이 `??` 노이즈로 지속(§4 표).
8. **glibc 바이너리의 bionic exec 거부 — 표시 메시지가 기만(T4).** 폰
   termux-jkdesktop에서 `exec`하면 "No such file or directory" 인쇄이지만
   파일은 존재 — Android(bionic)엔 glibc 로더가 없기 때문의 exec 거부.
   판별법 = `file`에서 동적 링커 경로 확인. lf r42(Go 정적 바이너리)만 폰
   실구동. helix는 폰 배제 — `.dead-on-phone` 마커로 멱등 판정.
9. **폰 probe run 재공 이력 — 커밋은 최종 green만(T4).** run 1 probe selftest
   절 누락·run 2 원격 스크립트 변수 오타(`ST_RC/S_RC` — `set -u`에서도
   조건부 대입 경로로 silent fail, `bash -n`은 구문만 봐서 미발견)·run 3 폰
   Wi-Fi 편성 낙하(ping 66% 손실 — 수십 분 후 자발 회복, 원인 불명 정직
   기록). 계약: 이력은 보고서에 정직 기록, 커밋 probe = green 원문 전용.
10. **`MSYS_NO_PATHCONV=1`이 `git.exe`의 MSYS형 인수 변환도 차단(T4 fix
    r1).** `git -C /i/...` → `no such repository` (rc=128) — git은 전용
    Windows형 경로(cygwin -w 실측)가 필요. 원격/인터럽트 양쪽 명령체의
    경로 규약은 도구별로 다르다.
11. **병행 SDD 세션 = 공유 워킹 트리 배포 오염(T4 fix r1 사고).** 서로 다른
    라인의 미커밋 WIP가 배포 원천(워킹 트리)에 상존하면 tar 배포가 오염
    판을 배송한다(NINJA-RC=1). 봉합 = probe의 배포 오염 게이트(더러운
    `main.cpp`=HEAD blob 스테이징 — 헤더는 배포 목록 밖, 기타 더러움=hard
    FAIL). 절차 렛슨: 병행 dispatch는 (a) 동시 커밋 인계 레스 절차 or (b)
    게이트 봉합 — 본건은 (b)로 실측 완결.
12. **WSL 서버 SIGTERM 흡수 — pre-clean도 -9 에스컬레이션 필요(chat-close
    fix 세션 wsl_chat_close.sh 봉합 — 이전 라인 유예 항목의 실측화).**

## 4. deferred minors 트라이아지 — 전수

처분 원칙: 수요 없으면 소각 없음(docs/77·79·80 선례) — 다음 probe/문서 손때
후보는 우열 표기. **유예 항목을 유예로 기록한다 — 소각한 것만 소각으로 쓴다.**

| 태스크 | 항목 | 처분 |
|---|---|---|
| T2 M1 | report 수치 미스 2건 — task-2-report.md:7 "154행→187행"(1f8a783 실측 wc=193)·:177 잔존 `>= 374` 문구(fix r1·T3 경과로 옛말) | **소각 — 본 원장 손때 수리**(re-review 룰링 "유예 nit는 T6 docs 손때에 수리" 이행 — 본 태스크) |
| T3 M1 | probe 헤더가 **없는 스크립트명** `wsl_apps_bin_setup.sh` 인용(wsl_apps_count.sh:16 — 실체 = `engine/scripts/install_lf_helix_posix.sh`) | 유예 — 다음 probe 손때 1차 후보(문구 1행) |
| T3 M2 | 플랜 본문(Global Constraints) 낡은 "Windows 395 / WSL 374" 수치 미정정 | 유예 — 플랜은 역사 문서(as-built=진실원, §2.0 캐논 표가 본 원장 내 결제) |
| T3 M3 | probe sleep 2 고정 대기 레이스(honest-FAIL 방향 — 무해) | 유예 |
| T5 유예 | probe :142 8단 pkill 비브래킷(브래킷 통일 후보)·probe 자신의 셸 Δ가 `awk $14+$15` naive(comm 공백 어긋남 — 현실 무해, T5 fix 리뷰 판정) | 유예 — 다음 probe 손때 통일 |
| T3 concern | **트윈 인수 경로(`jkctl ask`) 서버리 검증 불** — 본문(무인수)만 이식 실측, 스폰 로그+본문 rc로 검증 | 유예 — 서버 요구 경로, 폰 T4 편성 시 실측 후보 |
| T1 concern | legacy drvfs trigger 4종 위생(위생 시 probe 어설션+원장 수치 29→25 동시 갱신 패키지)·workshop auto-repack 규칙 신설(현재=수동 유산 유지 소결) | 유예 — 소유자 결정 |
| T3 concern | buildwsl/ `.gitignore` 1행 후보(`??` 노이즈 해소) | 유예 |
| T5 concern | capture PNG 20KB 하한=약한 생존 프록시(내용 단정=사람 육안 — 자동화하려면 화면 문자 판독, 현 단계 과다) | 유예 |
| **이전 라인 이월 합명** (docs/80 §5) | **JKLmEngine 오타 형제 전수** — 올바른 파일명은 `JKLlmEngine.h/.cpp`(Llm — `git ls-files` 실측; "JKLmEngine"이 오타). 잔존 실측(T6 작성 grep → **fix r1 2026-10-07 전수 재측정**): phone_ollama_try.sh 4곳(:12·:245·:257·:295)·posix_selftest main.cpp 2곳(:970·:1010)·**docs/80 4곳(:171·:175·:185·:220 — 구 3곳 표기에 :220 누락, T6 과제 ②백엔드 슬롯 실장 행 실측 정정)**·구문서 전수(docs/68 2·docs/70 2·docs/71 1·docs/72 1·docs/73 2·플랜 6건) = **합계 24곳**. tmp/phone_ollama_try_remote.sh 3곳은 무추적 scratch로 소멸(파일 부재 실측 — 구 표기 개산 26·docs/80 4건 보정 시 27에서 차감). progress 원장의 "22행"은 구측정치로 미정합 — 본 행 24곳이 정합 캐논(tracked `git grep` 전수 25 = 외부 실물 24 + 본 문 자신 1건) | **이번에 소각 없음 — 정직 기록** |
| T6 리뷰 유예 1 | **§5 de2fa28 미기재** — 자기 커밋 SHA를 표에 기재한 커밋은 기재 시점마다 또 이월이 필요한 자기참조 구조적 residual(본 fix r1 커밋도 동일 구조로 §5 미기재) | 유예 — 다음 docs 손때에 §5 본 라인 3커밋(aee21a6·1006601·de2fa28)+fix r1을 통합 원장화 |
| 최종 리뷰 유예 M1 | **jkx-pack appName 경로 이스케이프 잠재** — `../evil`은 rc=1 fail-closed 실측이나 세그먼트 성분(`..`·구분자 뒷마무리)은 정화 부재; 컨테이너 outPath(main.cpp:823-829)·icon 접두(:791-798)가 appName을 그대로 조립(선존재 승계 — 인벤토리 출하 이전부터) | 유예 — 다음 **jkx 도구 손때 코드**(basename/화이트리스트 정화 1함수 후보) |
| 최종 리뷰 유예 M2 | **조달기 sha256 체크섬 핀 부재 + ALREADY-PRESENT 시 버전 무단정** — `install_lf_helix_posix.sh`는 버전 핀 문구만 있고 다운로드 무결성 검증이 없으며, 기조산물 존재 시 버전을 재단정하지 않음 | 유예 — 다음 **조달기 스크립트 손때**(핀 sha256 1행 후보) |
| 최종 리뷰 유예 M3 | **wsl_apps_count.sh twin-hidden trap 부재** — 트윈 결손 스텝(§4 negative path)이 중간 FAIL로 죽으면 staged 트윈이 잔존해 후속 계측이 오염된 상태에서 시작 가능 | 유예 — 다음 **probe 손때**(trap·cleanup 브래킷 통일과 같은 손때 — T3 M1·T5 유예 편승) |
| T4 fix r1 | probe 신규 Minor(r-re-review) 4건 — r-M1 스테이징 경로 실패 방향 영수증 부재(단층 실측만)·r-M2 selftest 라벨 "399·420" 노후 문언(정보 행, 어설션 없음)·r-M5 run 5a/5b 영수증 미보존(문언 인용만)·r-M4 "조용한 폴백" 주석 과다 | 유예 — 다음 **probe 손때**(T4 원리: 배포 게이트의 실패 방향 1회 합성 계측이 r-M1 소각 후보) |
| T4 fix r1 병행 park (구 Minor) | M2 report "LF-PHONE=RUNS" vs receipt "ALREADY" 문구·M5 $TMPDIR 폴백 불일치·M7 JSON 포맷 강결합 | 유예 — 다음 probe 손때 |
| T4 concern | **hx(helix) 폰 조달** — glibc 빌드 exec 거부(§3 #8)·`.dead-on-phone` 마커·Termux pkg(helix·lf 존재)는 **엔진 밖 조달 별도 문** | 유예 — **소유자 결정 대기(사용자)** |
| T4 concern | **taskmgr 폰 /proc 편성 미실측**(T5 concern ④ 이행 후보 — taskmgr.jkx pack 완료, launch 미측정)·phone_library.sh 접속 하드코드 유예(계약 신설 = PHONE_HOST) | 유예 — 다음 폰 probe 손때 |
| T4 concern | 폰 permissions.json 스테이지(`{"close_window":"allow"}` in buildterm) 상시 유지 — chat-close fix 세션 원장(docs/80 §4b) 소유, 철수 = 런타임 파일 rm | 유예 — **사용자 결정 대기**(런타임 파일) |

## 5. 커밋 원장 (전부 rev-parse 실측 값 — 본 원장 작성 시점 재확인)

| 커밋 | 내용 |
|---|---|
| 9a2174ecd03f511b4cfb4ecffeb959fe7a5a4ca9 | BASE — chat fix r2(T1 dispatch 기점) |
| c3c9b792ce5fe9dc026679d0bf736144ebe2cdd3 | T1 — jkx-pack posix leg + repack dual-path(로드 트리오만 분기) |
| 80dbcb3c263360c98277fcf0de4377b6ad073c18 | T2 본체 — WSL 앱 커버리지 probe(count+launch 단정) |
| 1f8a7832c5eaa7c3fc7f023d8de9cf3e81433819 | T2 fix r1 — drvfs 투명 디코드 정정 + Windows 측 바이트 직검(§4b) + Minor 4건 |
| 63cf0625f16e7aa97337cc1e4517853e409990f2 | T3 — sampletodo/lf/hx posix 설치 트윈 + 카탈로그 cmd_posix 확장 계약 |
| 6f4f29cae59e4669b2b86e790e1d576efc16883e | T5 본체 — taskmgr /proc posix leg + JKClientSurface Hello pid 보수(brief 밖) + probe |
| 85661c8efdb7d14818fefb8c070d415d3c160d1a | T5 fix r1 — /proc stat 억제 11개 + CROSS-CHECK + probe minor 3건 |
| (본 커밋) aee21a67d8a141d9a0aca454617fdbe1c4355a73 | docs/81 as-built 원장(체인 마지막 = 본 docs 커밋 aee21a6+§5 행 실체 정정 1006601d17ff3a36830fbf295538669c73eafef2). 인벤토리 헤더 링크+task-2-report nit 2건(T2 M1) 편집은 **원문 그대로 본 커밋 밖** — .superpowers/ 전체가 무추적(T3 선례: 커밋 밖 작업 트리 편집) |
| f7f33ead7bde28e1660c8da430aa48009e051f63 | 최종 docs fix r1 — EOF 개행·deferred 보충·좌표 갱신(최종 리뷰 소화) |
| c89e893eb856b01dd951fa1a5b2fb2e7b2d8e7f1 | **T4 본체** — 폰 rollout probe(aarch64 jkx-pack 실측 — 2026-10-08 sshd 회복 후) |
| 0e25762d7e33bb1170762318279478b79efefffc | (병행 라인 chat-close-fix — 본 라인 체인 밖. close_window/focus_window 계약 확장, docs/80 §4b 소유) |
| 4800b3615942b3c37b0382765c125c4163cc0026 | T4 fix r1 — phone_apps probe AUTO-REPACK 분기 강결합(EVIDENCE-CONSENSUS)+EOF 개행+IP 정화+배포 오염 게이트 |

체인 = 9a2174e..85661c8(T1-T5 — fix 2커밋 T2/T5, T3 fix 0건, T4 park 미dispatch)
+ 본 docs 커밋 2건(aee21a6·1006601 — §5 행 실체 정정). 리뷰 경과: T1 Spec PASS/Quality Approved(I1=방법론 이월 — fix 불요)·
T2 본체 I1 1건 → fix r1 → **re-review CLEAN 8/8**·T3 Critical 0/Important 0
(Minor 4 — M4=본 원장 §2.0 캐논으로 결제)·T5 본체 Spec needs fixing(I1) → fix
r1 → **re-review CLEAN 6/6**. 누적 Critical 0, Important 3건 처분 = **T2·T5
2건만 fix r1 소각, T1 I1 1건은 fix 불요 — tmp 스크립트 방법론을 T2 이월로
소화**(progress 원장 "I1은 tmp 스크립트 방법론으로 T2 이월, fix 불요" —
fix r1 소각으로 집계하던 구 문언 정정).

최종 전체 리뷰(opus, 9a2174e..de2fa28 9커밋): verdict **GO** — Critical 0 /
Important 0 / Minor 4 → 처분: 수리 1(M4 — 본 §5 문구 정정, fix r1)·유예 3
(M1 jkx-pack 경로 정화·M2 조달기 sha256 핀·M3 probe twin-hidden trap —
§4 표 부기 원장화, 손때 소유자 명기). T6 리뷰 Minor 5도 흡수 — 수리 4
(EOF 개행·§4 JKLmEngine :220·§6 문형·현행 좌표)+유예 1(§5 de2fa28
자기참조 residual — §4 표 부기). fix r1 본 커밋 SHA는 자기참조 구조로 §5
미기재(§4 residual 항목).

## 6. 사용자 게이트 상태 — sshd 회복(2026-10-08) → T4 실행 완료 — 육안 결제만 대기

- **폰 sshd 회복 = 사용자 조작으로 확정(2026-10-08)** — 컨트롤러가 폰 도달
  실측(ssh 정상 응답)으로 확인. T4는 회복 당일 dispatch·실행·fix r1·
  **re-review CLEAN** — 위 §1 T4·§2.3 전입.
- **남은 결제(사용자 선언 전까지 기록 없음)**: ① 폰 브라우저 `localhost:8090`
  (jkweb — ALIVE 유지) 채팅 육안 ② 폰 X11 화면에서 Settings·Notes 창
  모양 (T4 종료 상태에서 제공 — T4는 창을 끄지 않고 잠금).
- 진행 중 이웃 라인: chat-close-fix(0e25762 — "닫아줘" 실사용 결함 수리,
  본 원장 §의 T4 조성 수치를 공유 — 리뷰 진행 중 2026-10-08).

- **회복 방법(사용자 측)**: 폰 Termux에서 sshd 재기동(표준 = `sshd`). PC 측
  생존 판정 1행 — 도달=회복, Connection refused=여전히 사망(2026-10-07 실측
  상태):

  ```
  ssh -p 8022 -o BatchMode=yes -o ConnectTimeout=15 -i ~/.ssh/termux_jkengine u0_a4@192.168.219.109 true
  ```

- **회복 후 T4 실행 게이트(plan Task 4 + T3/T5 이월)** — probe
  `engine/tools/probes/phone_apps.sh` **예정** (실물 부재 — T4 미dispatch,
  phone_library.sh 기계·tar-over-ssh로 작성 예정, 신규 입자 전량 동반 —
  §3 #6), 순서:
  1. tar 배포 → 폰 리빌드(`ninja -C buildterm -j4`, ~9-10분 — NINJA-RC=0)
  2. selftest **0 failure**(폰 축 캐논 신설 — 현재 기록 0건, T4가 첫 실측)
  3. **jkx-pack 폰 실측** — auto-repack이 폰 CMake 레일을 도달하는지가 태스크의
     진실 질문; 불가면 수동 pack으로 판정 원장화(plan 조항)
  4. `library-list` 실측(폰 조성 — 새 jkx + 콘솔 트윈 + builtins) → 상승 원장화
  5. launch_app jkx 2종 → list_windows 창 오브젝트 단정
  6. 종료 상태 = 서버 UP + 창 유지 → **사용자 육안 게이트 산출**
  재용 실측: `install_lf_helix_posix.sh`의 **aarch64 자산 존재 실측(T3 완료) →
  폰 apps-bin 배치**(조달기는 /mnt/i 하드코드라 폰 경로는 핀 URL 직접 fetch) ·
  **taskmgr probe 폰 편성**(T5 — /proc 동일 ABI 실측, hidepid 정직 메시지
  경로 — probe rehost 후보).
- **가짜 결제 기록 금지**: probe 출력만으로 "폰 확인됨"이라 쓰지 않는다 —
  결제 기록은 사용자 선언을 받은 뒤에만 본 원장에 기입(docs/80 §8 계약 승계).
  현재 상태 = **전부 대기**.
