# docs/70 — 리눅스 3단계: 전체 엔진 WSL CMake 빌드 (as-built)

- 날짜: 2026-10-05
- 플랜: `docs/superpowers/plans/2026-10-05-linux-stage3-full-build.md` (커밋 9549661)
- 실행: superpowers:subagent-driven-development — 컨트롤러+구현/리뷰 서브에이전트
- 레저: `.superpowers/sdd/2026-10-05-linux-stage3-full-build/progress.md` (판정·룰링 전문)
- 선행: docs/68 (1단계 경계 수술), docs/69 (2단계 posix 어댑터 본체)

## 0. 성과 요약

**전체 엔진(jkcore/jkserver/jkclient/jkdesktop_shell + 앱 20여종 + 도구 4종)이 WSL2
Ubuntu-24.04에서 CMake(ninja) 전체 빌드 에러 0.** 산출물: `jkdesktop` ELF PIE
약 7.1MB(최종 재빌드 시점 7,110,640B) + 앱 .so 다수 +
`jkbridge`/`jktriggers`/`jkctl`/`jkagentd` 4종.
WSLg(DISPLAY=:0)에서 `jkdesktop --server` 기동 확인 — SDL 그래픽 초기화+
unix socket(`/tmp/JKWindowServerPipe.sock`) 생성+`tools/call list_windows`
IPC 왕복까지 실측. Windows 공식 9항목 게이트 ×2 그린 — **Win32 관측 무변화 증명**.
(opus 최종리뷰: **APPROVE** — F1 주입 봉합 확장·문서 보강은 본 문서와 동일
라이더에서 즉시 처리)

빌드 환경: g++ 13.3, SDL2 2.30, SDL2_mixer 2.8, FFmpeg libav 60.x, cmake 3.28,
ninja. 빌드 디렉 `engine/buildwsl`(drvfs). `jkdesktop`은 링크 완료(이 플랜 최초의
Linux 링크 성공은 T7 — `ninja jkapp_notify jkapp_shot jkapp_script jkdesktop`).

## 1. 스파이크 인벤토리 — 실패 TU 21개와 흡수처 (2026-10-05, 커밋 전 실측)

```
FAILED: CMakeFiles/jkcore.dir/src/JKImeHook_win32.cpp.o   → T7 (CMake 게이트+posix 스텁 신설)
FAILED: CMakeFiles/jkcore.dir/src/JKHangulUtil.cpp.o       → T4 (jk::text 어댑터)
FAILED: CMakeFiles/jkcore.dir/src/JKButton.cpp.o           → T3 (mouseLog 균일화)
FAILED: CMakeFiles/jkcore.dir/src/JKApplication.cpp.o      → T3
FAILED: CMakeFiles/jkcore.dir/src/script/JKWorkshopStore.cpp.o → T5 (fs→std::filesystem)
FAILED: CMakeFiles/jkcore.dir/src/agent/JKLlmEngine.cpp.o      → T5/T8b (dllimport+주입 봉합)
FAILED: CMakeFiles/jkcore.dir/src/theme/JKThemeConfig.cpp.o    → T5 (mtime)
FAILED: CMakeFiles/jkserver.dir/src/server/JKWindowServer.cpp.o → T5 (fs ops·Toolhelp·posix 접봉)
FAILED: CMakeFiles/jkdesktop.dir/src/main.cpp.o            → T7 (이중 진입+shims)
FAILED: CMakeFiles/jkapp_script.dir/src/apps/JKAppModule_script.cpp.o → T7
FAILED: CMakeFiles/jkapp_notify.dir/src/apps/ClientNotifyApp.cpp.o    → T7
FAILED: CMakeFiles/jkapp_taskmgr.dir/src/apps/ClientTaskmgrApp.cpp.o  → T2 (CRT shim)+T5(스캔 어댑터)
FAILED: CMakeFiles/jkapp_notes.dir/src/apps/ClientNotesApp.cpp.o      → T2
FAILED: CMakeFiles/jkapp_files.dir/src/apps/ClientFilesApp.cpp.o      → T2
FAILED: CMakeFiles/jkapp_shot.dir/src/apps/ClientShotApp.cpp.o        → T7
FAILED: CMakeFiles/cefosr.dir/tools/cefosr/osr_main.c.o       → 제외 v1 (WIN32 게이트, T1)
FAILED: CMakeFiles/jkchat.dir/tools/jkchat/main.cpp.o         → 제외 v1 (T1)
FAILED: CMakeFiles/jkbridge.dir/tools/jkbridge/main.cpp.o     → T8 (콘솔 shim+ws2_32 게이트)
FAILED: CMakeFiles/jkapp_browser.dir/src/apps/ClientBrowserApp.cpp.o → 제외 v1 (T1)
FAILED: CMakeFiles/jktriggers.dir/tools/jktriggers/main.cpp.o → T8
FAILED: CMakeFiles/jkctl.dir/tools/jkctl/main.cpp.o           → T8 (어댑터 흡수+이중 진입)
```

전부 유틸리티 계층 잔여였다 — 렌더/컴포지트/클라 코어(jkclient·jkdesktop_shell·
JKDC·터미널 렌더 층)는 스파이크 시점에 이미 POSIX 컴파일 통과(2단계 성과).

## 2. 태스크 실측 기록 (T1→T8b, 레저 요약)

| 태스크 | 커밋 | 내용·판정 |
|---|---|---|
| T1 CMake 조건화 | 0e19a92 | PAL 3TU genex·jkchat/cefosr/RC WIN32 게이트·-municode 게이트. 리뷰어가 임시 reconfigure로 Windows build.ninja 402=402 엣지·FLAGS md5 동일 실증. 실패 엣지 21→17 |
| T2 CRT shim | 0003e79 | `include/port/JKCrtShim.h` — FopenS/LocaltimeS/Stricmp(+FormatBytes 중립화). _WIN32 위임=원문 리터럴이라 무편차 구조적 성립 |
| T3 mouseLog 균일화 | 96aaf7f | 멤버+게터 무조건, 개방부만 _WIN32. Windows 토큰 스트림 바이트 동일(게이트 이동뿐) |
| T4 jk::text 어댑터 | 7dc65ea | JKTextConv.h+_win32(MBTW/WCTB 위임)+_posix(iconv CP949, 부분변환 fail-closed). ComputeKssmCodepoint 검역 해제. posix_selftest 케이스 7 신설. terminal_hangul_probe 44/44 |
| T5 서버 잔여 | a6c593e+edcba75 | JKWindowServer fs ops 15개 사용처→`std::error_code` ec 오버로드(리뷰 1판에서 throwing overload 9곳 FIX → r1 교체), Toolhelp→JKProcess_win32 이동+posix `/proc` 스캔, flock 가드 실물화, pipe 이름→unix socket 경로 fold 매핑, JKProcess 확장(SpawnResult.errorCode·kStillActiveExit 소유·케이스 8/9 신설). 리뷰 opus APPROVE |
| T7 데스크톱+진입 | 4956299 | **최초 Linux 링크 성공**. 링크 봉합 4건: PIC no-WIN32 전역(R_X86_64_TPOFF32 실측), jkdesktop_shell bcrypt 게이트, PAL 3TU 무조건+JKImeHook posix 스텁(훅 없음 계약), 보류 게이트 컨트롤러 소관 |
| T8 도구 3종 | b978864+627d992+62ebece | jkctl CreateProcessW→jk::process 흡수+wmain/main 이중 진입+`tools/ConsoleShim.h`(ANSI posix leg). posix ask 셸 주입 봉합(삼중 이스케이프, 주입 무해 실측). jkbridge/jktriggers도 포팅(Spawn 소비처 0 — 감사 실증) |
| T8b 미니봉합 | 73819fd | JKLmEngine posix 프롬프트 경로 주입(라이브 모델 결합 텍스트가 `/bin/sh -c` 도달) — BuildEngineCmd posix leg 삼중 이스케이프. 리뷰어 실 sh(dash/bash) 재현 실증 |

리뷰 정본: T2 sonnet APPROVE / T3 sonnet APPROVE / T4 opus APPROVE / T5 opus
FIX REQUIRED→r1 APPROVE / T7 opus APPROVE / T8 opus FIX REQUIRED→r1 APPROVE /
T8b sonnet APPROVE. T6는 방지(vacated) — 범위 3 TU가 전부 T5에서 소각(code-is-law:
`ninja jkserver`가 jkcore 전이 빌드 — 게이트 도달 위해 불가피).

## 3. 게이트 (2026-10-05)

**(1) Windows 공식 세트 ×2 연속 전부 GREEN** (공식 런 기준 — 스토탑 후):
① 전체 빌드 에러 0 — 런1: 누적 96/96(코드 최종판 73819fd) RC=0, 런2·런3: 무변경
exit 0. ② `jkdesktop.exe test` → `AppSelfTest: 0 failure(s)` ×2 ③
terminal_hangul_probe **44/44** ×2(소스 재컴파일 빌드 — T4 변환 경로 반영) ④
probe_app_tools **ALL PASS** ×2 ⑤ probe_jkbridge **PASS** ×2 ⑥
probe_agent_events **8/8** ×2 ⑦ probe_semantic_cursor **ALL PASS** ×2 ⑧
jkagentd stdio `tools/call list_windows` → `{"ok":true,"windows":[]}` ×2 ⑨
jkbridge HTTP 스모크 root=200(토큰/채팅 페이지)+health=200 ×2.
보조: probe_workshop **ALL PASS** ×2(T6 잔여 게이트의 프로브 스모크 편입
룰링), theme 스모크=every 재팩 `[theme] preset 'dark'` 로드 관측+AppSelfTest.

**(2) WSL 전체 빌드 에러 0 ×2 연속:** cmake 재구성 RC=0 → `ninja -j4` 런1
RC=0/FAILED 0(129 엣지), 런2 무변경 RC=0/FAILED 0. **posix_selftest**:
`sh engine/tools/posix_selftest/build.sh && ./engine/build/posix_selftest` →
`PosixSelfTest: 0 failure(s)`(케이스 7 charset 왕복·8 `/proc` 자기 PID·
9 socket fold 포함).

**(3) CMake 라이더 후 Windows 무변동 실측 (커밋 65a4c3a):** WSL 전체 빌드에서
유일 실패가 jkx 재팩 엣지 3종("jkx-pack is Windows-only in this prototype")
임을 실측 → jkx 자동 재팩 블록(아이콘 동기+전 컨테이너+jkx_packages ALL)을
`if(WIN32)` 게이트. 게이트 후 Windows 재구성+ninja RC=0, jkx-pack 25 엣지
유지(repack 블록은 Windows에서 바뀌지 않음 — configure 시간 조건만).
게이트 세트는 라이더 전 코드(T73819fd)에서 2연속 확정, 라이더는 WIN32
동작 변경 0(CMake 조건만)이므로 유효.

**(4) WSLg 스모크:** `env DISPLAY=:0 ./jkdesktop --server`(X11 X0 존재) —
SDL 그래픽 초기화·런처 아이콘 로드 로그 실측, `/tmp/JKWindowServerPipe.sock`
생성(`\\.\pipe\JKWindowServerPipe`의 posix fold 매핑 정상 적용),
`./jkagentd` unix socket 경유 `list_windows` → `ok:true` 왕복(POSIX↔POSIX
IPC 실측). terminal_exec는 `unsupported_platform` 즉답 — jkagentd의
RunTerminalExec pty 배선이 win32 전용(#else stub, :371); JKConPtyBridge
posix 어댑터 자체는 2단계 실측되어 있으나 이 도구 배선은 잔여(§6).

## 4. Linux 제외 v1 (명문)

- **jkchat** — Win32 GUI(docs/68 W8 마킹, 비이식 확정 유지)
- **jkapp_browser+cefosr** — third_party/cef는 Windows 바이너리
- **jkx 재팩 엣지** — jkx-pack 호스트가 Windows 전용 서브커맨드
  (프로토타입). 65a4c3a로 WIN32 게이트 — Linux 빌드는 런처 컨테이너
  없이 바이너리만 산출
- **.ps1 하네스** — 엔진 소유가 아니라 진단 장비

## 5. 환경 교훈 (레저 → 기억)

- **Git Bash PATH에 `C:/msys64/ucrt64/bin` 미포함 시 MinGW cc1plus의 DLL
  로드가 무음 RC=1 사망**(드라이버가 에러 출력을 삼킴 — ninja FAILED만
  보이고 원인이 안 보임). Windows 빌드 표준 두 줄:
  `export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH" && ninja -C build -j3`
- `$?`는 파이프 뒤에 있으면 tail의 RC를 반영한다 — exit-code 측정은 파이프
  없이 재측정(런 2 재측정 사례)
- 시스템 메모리 부족(15.9GB 머신) 시 백그라운드 태스크가 강제 종료됨 —
  Windows 빌드는 `-j3`
- WSL `Failed to translate 'H:\tizen-ext\...'` 경고는 WSLENV 잡음 — 무해

## 6. 잔여 (레저 — 후속 판정 대상)

1. **jkagentd terminal_exec posix 배선** — JKConPtyBridge posix 어댑터가
   있으나 jkagentd RunTerminalExec의 pty 구동부가 win32 전용
   (`unsupported_platform` 즉답, tools/jkagentd/main.cpp:371). 배선 태스크 후보
2. **posix LLM 턴 셸 접두** — `cmd.exe /c` 접두는 stub 분기에만 존재;
   claude/ollama 분기는 접두 없이 sh에서 실행 가능(WSL에 ollama가 있으면
   유효 실행 경로). 즉 잔여의 위상은 "not found"가 아니라 **미봉쇄 실행
   경로**(셸 추상 — jk::process 셸 계약 + argv-vector exec가 후속).
   F1 라이더(동일 커밋)로 프롬프트·모델명·세션 id 삼중 이스케이프 완료 —
   남은 것은 접두 추상만
3. **수기 백슬래치 경로 합성 (F2 전수 열거)** — 리눅스에서 '\'가 파일명
   성분 경로: ①JKWindowServer `dir + "\\state"` ②JKWorkshopStore
   HistoryDir `scriptsDir + "\\.history\\" + slot` + AppendSnapshot
   `dir + "\\" + name` ③jktriggers PackMode `outDir + "\\" + name +
   ".jkx"`(런타임 pack 서브커맨드 — **관측급 파손**: pack→load 핸드셰이크
   끊김). 후속 봉합 태스크 소관 — trust.json 합성 3처는 '\' 성분 포함
   소비처 간 일치로 왕복 성립(오히려 일치 상태 유지 필요)
4. **fmt 스탬프 localtime_s 역전 boolean**(win32 잠재 결함 — docs/68
   승계, T2 레저 재확인) + **BuildEngineCmd win32 백슬래시 누락**(별도
   결함 후보 — FmtStamp 선례)
5. docs/69 §4 소비자 측 접봉 잔여 중 미처리: **job=단일 멤버**(posix pgid
   덮어쓰기 — 트리 킬 누락), **자식 stdin 부모 상속**(패리티 원하면
   open("/dev/null")+dup2(0)), **조기 CloseHandleLike 좀비**(init 회수 —
   누수 아님), **전송 phantom 연결**(probe connect가 backlog 슬롯 소비)
6. **vplayer 리눅스 실재 동작** — **§8로 결제(2026-10-05): 재생+EOF 수령
   실측 완료**. 남은 것: **Termux 패키징·폰 실기기 도달**(docs/62 §3 흐름),
   §8.4 부수 관측 2건 판정
7. **selftest 강화 후보**(docs/68 승계): 수백 바이트 RecvAll 패턴·Accept
   무한블록 방어
8. **셸리스 리눅스 서버(F7)** — posix 서버 SpawnClient는 스텁
   ("Windows-only in this prototype")이라 WSLg 서버가 taskbar 자동 스폰을
   하지 않음(스모크에서 windows=[]로 관측된 이유). 클라 수동 기동/스폰
   개통이 후속
9. **관측 편차 기록(F4·F5)** — jktriggers GlobTail 8.3 단명 매칭 드롭
   (FindFirstFile 와일드카드→directory_iterator, 코드 내 문서화된 의도
   편차 — win32 관측 변화이나 도달면 좁아 승인), JKWindowServer
   layout_*.json 스캔이 대소문자 구분으로 전환(와일드카드는 무시 —
   실운용 파일명 일치로 무해, 주석 한 줄 권고)

## 7. 커밋 원장 (이 플랜, BASE 9549661→)

| 커밋 | 내용 |
|---|---|
| 0e19a92 | T1 CMake 조건화 |
| 0003e79 | T2 CRT shim |
| 96aaf7f | T3 mouseLog |
| 7dc65ea | T4 jk::text 어댑터 |
| a6c593e + edcba75 | T5 서버 잔여(ec 오버로드 r1) |
| 4956299 | T7 데스크톱+진입(최초 Linux 링크) |
| b978864 + 627d992 | T8 도구 3종+라이더 |
| 62ebece | T8 r1 픽스(주입 봉합) |
| 73819fd | T8b JKLmEngine 미니봉합 |
| 65a4c3a | T9 라이더: jkx 재팩 WIN32 게이트 |
| (본 문서) | docs/70 as-built |

## 8. vplayer WSL 재생 실측 + 폰트 assert 봉합 (2026-10-05 후속, §6 #6 전반부 결제)

### 8.1 폰트 assert 사망 봉합 (10-site)

WSL 부팅 실측에서 폰트 assert 사망 확정: 클라이언트 ImGui 앱들이
`C:\Windows\Fonts\malgun.ttf`를 하드코딩 — 리눅스에서
`io.Fonts->AddFontFromFileTTF`가 **assert로 즉사**
(`imgui_draw.cpp:3251 "Could not load font file!"`, 클라이언트 측
null-check 가드는 assert가 imgui 내부에서 먼저 터져 무의미). docs/68 W5
deferred 항목의 실재 사망. 봉합: **10 ImGui 앱 전부**
`jk::text::ResolveDesktopFontPath()` 승계(docs/63 §4.1 계약 — settings.json
`text.font_path` → Windows malgun → Linux Noto CJK 후보 배열) + **빈 해석
시 커스텀 폰트 스킵**(내장 기본 글리프로 열화, 사망 아님).

- 승계 10곳: agentmgr·browser·filedlg·files·notes·notify·settings·shot·
  snap·vplayer (vplayer가 최초 사망 수사 발원지)
- 커스텀 글리프 레인지 보존: browser의 `koreanWithStar`
- if-가드 패턴(`koreanFont_ = true` 세팅 곳)은 반환값 검사로 확장:
  `if (!fontPath.empty() && io.Fonts->AddFontFromFileTTF(...))`
- Windows 회귀: BUILD RC=0 + `AppSelfTest: 0 failure(s)` (GREEN)

### 8.2 재생 실측 영수증 (libav 60.x Linux 첫 실측)

통제 사이클(`engine/tools/probes/wsl_vp_cycle.sh`)로 부팅→launch_app→
open→폴링까지 한 세션에서 수행:

- boot(G 표준) → `launch_app vplayer` ok:true → 서버+taskbar+vplayer
  3프로세스 → `app_tool open` (`/mnt/i/progwork/JKENGINE/tmp/
  vpt2_test.mp4`, 엔진 소유클립) → `{"ok":true,"windowId":3,
  "accepted":true}`
- get_status 폴링: pos 실시간 진행(2.97→6.08→…→29.954), `dur:30.000`,
  `error:""`, **`ended:true` 수령** — 디코딩·디먹스·프리젠테이션·EOF
  전 구간 정상. **리눅스에서의 첫 재생 성공.**
- 종료 후 60초+ 폴링 전부 생존 — 재생 종료 자체는 어떤 사망도 유발하지
  않음

### 8.3 사망 트리아지 — 클립 종료가 아니라 wsl.exe 세션 detach 소각

"클립 종료 시 전멸"은 오판이었다. 판별 실험
(`engine/tools/probes/wsl_vp_soak.sh` — 부착 세션에서 240초 폴링):

- 부착 세션 내내 3프로세스 생존(240초, 사망 무)
- **소생 세션이 exit하자 7초 후 점검 시 전멸(0개)** — 재현 3회 동일
- 커널 기록(dmesg)에 사망 신호 기록 **전무** — abort/crash가 아니라
  WSL이 소스 세션 소멸 시 프로세스 트리를 소각하는 것이다(문서화된
  `(env DISPLAY=:0 ./jkdesktop --server … &)` 부팅은 **부착 세션 수명에
  종속** — docs/70 §5 표준 항목 교정)
- dmesg의 signal 6 기록 2건(28999/29039)은 8.1의 폰트 assert 사망
  (폰트 설치 전) — 리듀서 혼동 방지 명기

**신규 표준 부팅 패턴(setsid 신세션 분리 — detach 소각 회피,
`wsl_vp_setsid_boot.sh` 검증, +30초 생존 확인):**

```sh
pkill -9 -x jkdesktop; rm -f /tmp/JKWindowServerPipe.sock
(env DISPLAY=:0 setsid ./jkdesktop --server >/tmp/srv.log 2>&1 &)
sleep 6
```

### 8.4 부수 관측 (결함 확정 아님, 후속 판정)

- **`ended:true` 조기 플립** — pos 24.4에서 flag 조기 세팅되고 pos는
  29.954까지 계속 진행 → 29.954에서 정지(dur 30.000에 미달). 디먹스
  EOF(마지막 비디오 패킷~24.4s)와 프리젠테이션 시간의 괴리 추정.
  Windows 동일 클립 관측 비교 후 판정할 것
- **서버측 벡터 폰트가 NotoSansCJK-Regular.ttc에서 init 실패** —
  `Warning: vector font init failed (…/NotoSansCJK-Regular.ttc); staying
  on bitmap glyphs`. .ttc 컬렉션 파싱 미지원 추정(Windows malgun.ttf는
  정상). ImGui 클라이언트 폰트는 동일 .ttc에서 정상 로드 — 서버 아틀라스
  경로만 열화(docs/63 계열 후속, 셧다운성 결함 아님)
- **wsl.exe 인라인 따옴표 소각** — `bash -lc '…"{\"k\":…}"'`의 이스케이프
  따옴표와 `$VAR` 확장이 wsl.exe Windows 인자 파서에서 유실됨 → 복잡한
  WSL 진단은 반드시 **스크립트 파일**(probe)로 실행
  (`bash /mnt/.../probe.sh`, Git Bash 호출엔 `MSYS_NO_PATHCONV=1`)