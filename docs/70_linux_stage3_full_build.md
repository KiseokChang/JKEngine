# docs/70 — 리눅스 3단계: 전체 엔진 WSL CMake 빌드 (as-built)

- 날짜: 2026-10-05
- 플랜: `docs/superpowers/plans/2026-10-05-linux-stage3-full-build.md` (커밋 9549661)
- 실행: superpowers:subagent-driven-development — 컨트롤러+구현/리뷰 서브에이전트
- 레저: `.superpowers/sdd/2026-10-05-linux-stage3-full-build/progress.md` (판정·룰링 전문)
- 선행: docs/68 (1단계 경계 수술), docs/69 (2단계 posix 어댑터 본체)

## 0. 성과 요약

**전체 엔진(jkcore/jkserver/jkclient/jkdesktop_shell + 앱 20여종 + 도구 4종)이 WSL2
Ubuntu-24.04에서 CMake(ninja) 전체 빌드 에러 0.** 산출물: `jkdesktop` ELF PIE
7,110,736B + 앱 .so 다수 + `jkbridge`/`jktriggers`/`jkctl`/`jkagentd` 4종.
WSLg(DISPLAY=:0)에서 `jkdesktop --server` 기동 확인 — SDL 그래픽 초기화+
unix socket(`/tmp/JKWindowServerPipe.sock`) 생성+`tools/call list_windows`
IPC 왕복까지 실측. Windows 공식 9항목 게이트 ×2 그린 — **Win32 관측 무변화 증명**.

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
   (`unsupported_platform` 즉답). 배선 태스크 후보
2. **posix LLM 턴 셸 접미** — BuildEngineCmd의 `cmd.exe /c` 접두 남아
   dash에서 not found(셸 추상 — jk::process 셸 계약) — docs/69 §4에서
   승계된 잔여
3. **JKWindowServer `dir + "\\state"` 수기 백슬래치 합성** — 리눅스에서
   '\' 문자 성분 경로(런타임 결함, T7 리뷰 발견 — 후속 봉합)
4. **fmt 스탬프 localtime_s 역전 boolean**(win32 잠재 결함 — docs/68
   승계, T2 레저 재확인) + **BuildEngineCmd win32 백슬래시 누락**(별도
   결함 후보 — FmtStamp 선례)
5. docs/69 §4 소비자 측 접봉 잔여 중 미처리: **job=단일 멤버**(posix pgid
   덮어쓰기 — 트리 킬 누락), **자식 stdin 부모 상속**(패리티 원하면
   open("/dev/null")+dup2(0)), **조기 CloseHandleLike 좀비**(init 회수 —
   누수 아님), **전송 phantom 연결**(probe connect가 backlog 슬롯 소비)
6. **vplayer 리눅스 실재 동작**(libav 60.x 링크 확인 — 런타임 재생 실측은
   별도), **Termux 패키징·폰 실기기 도달**(docs/62 §3 흐름)
7. **selftest 강화 후보**(docs/68 승계): 수백 바이트 RecvAll 패턴·Accept
   무한블록 방어

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