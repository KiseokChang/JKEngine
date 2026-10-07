# WSL/Termux 앱 커버리지 확대 구현 플랜 (jkx-pack posix leg + 설치 트윈)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** WSL 라이브러리 7→~30, 폰 4→~29 — 앱 모듈 28종이 이미 양축 빌드되므로 `.jkx` 포장과 카탈로그 게이트만 posix로 열면 앱 수가 폭증한다.

**Architecture:** 근본 원인은 코드 부재가 아니라 포장 단계 — `jkx-pack` 서브커맨드가 `LoadLibraryA`(`_WIN32` 전용, main.cpp:673-791)이고 auto-repack 블록이 `if(WIN32)`(CMakeLists.txt:924-1152 — formal ruling docs/70 §4). posix에서는 `dladdr()`로 jkx-pack에 동일 기능 제공(JKAppModule_script.cpp:47-62 실전 선례, docs/78 TX6) + repack 블록 dual-path 화. 설치류(sampletodo 콘솔 트윈·lf/hx posix)는 카탈로그의 파일 존재 게이트(JKLibraryCatalog.cpp:217-223)가 이미 무접미 posix 경로를 받는다.

**Tech Stack:** C++17, WSL bash, Termux aarch64 ninja, tar-over-ssh.

**Spec:** `.superpowers/app-availability-inventory.md` (2026-10-07 전수 인벤토리 — 앱별 차단 원인+비용 표, 이 플랜의 근거 원장)

## Global Constraints

- selftest 1m 계약 유지 — 카탈로그는 비재귀 스캔, Windows 395 PASS / WSL 374 PASS 불변(1m-1 n==6 실측 유지). Windows count=30 감소 없음(증가만).
- **jkx-pack posix leg는 기능 신설이 아니라 플랫폼 균등화** — Windows 경로의 동작과 포맷을 완전 동일 유지(Windows 회귀 무수정). 룰링 계약: docs/70 §4(WSL 1단계 스코프 판정)은 "browser 제외·launch_app posix 표면 확정"이 본래 내용 — repack 블록의 포장 누락은 환경 결함이 아니라 미실장이므로 이번 라인이 최초 실장이다(룰링 뒤집기 아님).
- **posix pack 시 경로 구분자 전진(/) 필수** — WSL trigger .jkx 역슬래시 이름 부정 이상(U+F05x drvfs 체화, buildwsl/apps/ 최상위) 재발 금지. JKJkxFile가 여는 이름이어야 한다.
- 카탈로그 읽기 전용 계약 유지 — uninstall/자기 진실원 없음, 파일 존재 게이트만.
- 폰 배포는 tar-over-ssh+폰 전량 리컴파일(~9-10분, `-C buildterm -j4`)+probe 영수증 경로(phone_library.sh 기계)만 — 폰에 git 원격 금지.
- probe 품질 원장: 수신=어설션, `|| true` 금지, rc 전파 PIPESTATUS→exit $rc, NOUT 인쇄, pkill/pgrep `-f 'buildwsl/[j]kdesktop'` 브래킷, MSYS_NO_PATHCONV=1 표준 헤더.
- permissions.json 전면 allow 런타임 파일 수정 금지. Windows jkdesktop --server 스폰 금지(서버 소유=probe/사용자 콘솔).

---

### Task 1: jkx-pack posix leg + repack dual-path

**Files:**
- Modify: `engine/src/main.cpp`(jkx-pack 서브커맨드 — posix leg 신설, Windows leg 무변)
- Modify: `engine/CMakeLists.txt`(auto-repack 블록 — if(WIN32)을 dual-path로)

**Interfaces:**
- Consumes: `JKAppModule_script.cpp:47-62`의 dladdr 실전 선례(docs/78 TX6) — 자기 .so 경로 해결 패턴.
- Produces: posix `jkx-pack <app>` 동작(Windows와 동일 컨테이너 포맷·이름 규약, 전진 구분자) + WSL 빌드 시 auto-repack이 apps/*.jkx 생성 → 카탈로그가 jkx 어휘를 library-list에 올림.

- [ ] **Step 1:** posix leg 구현 — LoadLibraryA 대응은 dlopen/GetModuleHandle형 자기 해결(dladdr 선례 준수). 앱 모듈 등록 표 기반 패킹은 Windows leg와 코드 공유(플랫폼 분기 최소 — 로드 함수만 분기). 리팩터 수순 최소.
- [ ] **Step 2:** CMake repack dual-path — posix에서도 빌드 후 repack이 돌도록; 브래킷/전진 구분자. 기존 if(WIN32) 내 로직 재용+posix 레그 신설.
- [ ] **Step 3:** Windows 회귀 무수정 실측 — 표준 빌드 rc=0+셀프테스트 전부 PASS+`library-list` count=30 유지+(기존 apps/ jkx 재포장 안 깨짐 실측 — repack이 기존 산출을 건드리며 이름/포맷 무변 확인) → 커밋 `feat(apps): jkx-pack posix leg + repack dual-path — WSL/폰 앱 커버리지 확대 1단`.

### Task 2: WSL 실측 probe (count 게이트)

**Files:**
- Create: `engine/tools/probes/wsl_apps_count.sh`(wsl_library_boot.sh 원준)

**Interfaces:**
- Consumes: T1 산출(buildwsl/apps/*.jkx 전진 구분자 명명).
- Produces: WSL `library-list` count 어설션(실측 목표 ~29±1 — 내장 4+console 트윈 제외 기준 상시 보고; 정확 수치는 T1 완료 후 실측으로 확정)+신규 jkx 앱 2종 launch_app 성공 단정.

- [ ] **Step 1:** WSL 리빌드 → library-list 실측 count 기록(기대치는 report에 실측 보고 — 숫자 먼저 못 정확히 정하는 태스크라 잠긴 어설션 아님) → jkx 앱 2종(minesweeper 외 기존 미제공이었던 것) launch_app → list_windows 창 생성 단정 → pkill 브래킷 철수. rc 전파 원장 준수.
- [ ] **Step 2:** 커밋 `test(apps): WSL 앱 커버리지 probe — count+launch 단정`.

### Task 3: 설치 트윈 (sampletodo .sh + lf/hx posix)

**Files:**
- Create: `engine/apps/sampletodo` 런쳐 .sh(트윈 — 기존 .cmd 동형, 셸스크립트), manifest 갱신(커밋 원장 동선)
- Modify: `engine/src/JKLibraryCatalog.cpp`(필요시 — 이미 무접미 경로 게이트면 무수정, 실측으로 판정)

**Interfaces:**
- Consumes: 카탈로그 파일 존재 게이트(JKLibraryCatalog.cpp:217-223) — .sh/.cmd 무접미 처리 이미 계약.
- Produces: sampletodo(콘솔 트윈)·lf·hx가 WSL/폰(library-list)에 등장.

- [x] **Step 1:** sampletodo .sh 트윈 작성(.cmd 본문 1:1 셸 이식)+lf/hx posix 빌드 산출 경로 확인(빌드 설비에 이미 있는지 실측 — buildwsl/apps) → Windows 회귀 없음 실측(selftest 395 count) → 커밋 `feat(apps): sampletodo/lf/hx posix 설치 트윈 — 카탈로그 무접미 경로 실측`.
  (as-built: 카탈로그는 "무접미 경로 게이트 이미 계약"이 아니었다 — lf/hx leg(:217-223)만 무접미고 콘솔 스캔은 cmd 원문 전승. 확장 계약 = manifest `cmd_posix`(옵션, basePath 상대 키)+posix 존재 게이트 fail-closed+staging CMake `console_apps` 타겟. WSL count 실측 29→32, selftest 391→399; Windows count=30 불변, selftest 0 failure 유지. lf/hx linux 조달 = `engine/scripts/install_lf_helix_posix.sh`(lf r42/helix 25.07.1 핀 — Windows 패리티), 바이너리 git 밖.)

### Task 4: 폰 rollout probe

**Files:**
- Create: `engine/tools/probes/phone_apps.sh`(phone_library.sh 기계)

**Interfaces:**
- Consumes: T1 posix pack leg — 폰에서 libjkx-pack 동형 동작(폰은 auto-repack까지는 과연 도달할지 실측 — 태스크의 진실 질문).
- Produces: 폰 count 실측(~29 목표)+jkx launch 2종 단정+사용자 육안 게이트 산출.

- [ ] **Step 1:** tar 배포(T1/T3 변분+신규 probe) → 폰 리빌드(~9-10분 NOUT/rc 전파) → 셀프테스트 0 failure → jkx-pack 폰 실측(수동 pack 1회 — auto-repack이 폰 CMake 레일에서 안 돌면 수동 pack로 판정 원장화) → library-list 실측 → launch_app jkx 2종 단정 → 종료 상태 서버 UP+창 유지(육안 게이트) → 커밋 `test(apps): 폰 앱 커버리지 probe — aarch64 jkx-pack 실측`.

### Task 5: taskmgr posix leg (/proc 기반 — 유일한 실 코드 포팅)

**Files:**
- Modify: `engine/src/apps/ClientTaskmgrApp.cpp`(per-process stats — `#else` posix leg 신설, `/proc/<pid>` 샘플링)

**Interfaces:**
- Consumes: 없음(단일 파일 내부 — 인벤토리 판정: psapi OpenProcess/GetProcessTimes/GetProcessMemoryInfo에 #else 없음).
- Produces: taskmgr가 WSL/폰에서 프로세스 표 채움(값 정합은 육안).

- [ ] **Step 1:** posix leg — /proc에서 pid·이름·CPU(샘플 간 delta)·RSS 읽기. Windows leg 무변.
- [ ] **Step 2:** 빌드+셀프테스트(Windows 395 무변)+커밋 `feat(apps): taskmgr posix leg — /proc 기반 per-process stats`.

### Task 6: as-built docs/81

- [ ] **Step 1:** 배선·3축 영수증·앱 수 증가 원장(Windows 30 / WSL 실측 / 폰 실측)·인벤토리 상속(차단 원인·남은 제외: browser·jkchat·launcher-grid scan)·커밋 원장. 스펙 링크는 인벤토리 문서.
- [ ] **Step 2:** 커밋 `docs(apps): as-built 원장 docs/81 + push(origin main 직행 관행).`

## 제외 확정 (이번 라인 밖 — 인벤토리 판정 상속)

- jkapp_browser+cefosr(L — CEF Windows 바이너리 배포+windows.h 링크 모델; docs/70 §4 명시 제외)
- jkchat(Win32 전용 — PC 방향, 별도 문)
- launcher-grid `.jkx` scan(FindFirstFileA — Q1 룰링: launcher 무접촉 유지)
- WSL trigger .jkx 역슬래시 이름 부정 — T1에서 전진 구분자로 재발 방지하되, 이미 체화된 U+F05x 파일 정리는 별도 소모 태스크(보고서에 후보 기록)