# docs/68 — 리눅스 포팅 1단계(플랫폼 경계 수술) 실행 플랜 (2026-09-28, 착수 전 문서)

상위 스펙: docs/62 (3단계 로드맵 §3, 착수 전 조사 §8) — 이 문서는 그 **1단계의
작업 목록을 Gemini 인벤토리(docs/gemout_platform_inventory.md §5 검증 부록 포함)와
합쳐 봉합한 것**. 10월 첫 세션에서 이 문서 순서대로 수술한다.

**1단계의 정의(docs/62 §3):** 어댑터 경계를 인터페이스로 뽑고 Win32 구현을 그 자리에
유지. 리눅스 구현은 하지 않는다. 완료 판정 = **기존 Win32 회귀 프로브 전부 GREEN(동작
변화 0 증명)** — 기능 추가·변경 없음이 원칙.

## 1. 작업 목록 (의존 순)

### W1 — 전송 경계 잔여 흠 2건 수선 (docs/62 §8-①②, 최우선) — **AS-BUILT 완료(2026-10-05)**
- `CancelPendingIo()` 인터페이스 승격: `include/ipc/JKWireProtocol.h`의 IWireTransport에
  가상 메서드 추가 → `JKClientConnection.cpp:116`의 구체 클래스 직접 호출 해소.
- `ReadMessage` 페이로드 캡: 길이 상한 검사 후 `assign` (현행 무상한 assign).
- 게이트: probe_app_tools·probe_semantic_cursor 회귀(전송 경로 스모크).
- **실측:** W1a 커밋 502987f(`virtual void CancelPendingIo() {}`를 IWireTransport에,
  JKPipeTransport.h override+holder JKClientConnection.h:27·124·JKClientSurface.h:157
  unique_ptr<ipc::IWireTransport> 확장, 잔여 접촉=팩토리+win32/posix 구현 TU만으로 수렴).
  W1b 커밋 fa46a4a(kMaxWirePayload=64 MiB를 헤더로 승격, ReadMessage는 magic 검사 직후
  assign **전** 거부 fail-closed, 셀프테스트 케이스 12 — cap+1 거부+실데이터 왕복, 우연
  PASS 함정 payloadRead 플래그 봉합). 게이트 probe_app_tools ALL PASS ×2+
  probe_semantic_cursor ALL PASS ×2.

### W2 — 암호 통합 (docs/62 §8-6 + 인벤토리 §1-4) — **AS-BUILT 완료(2026-10-05)**
- BCrypt SHA-256×3(jkctl/jktriggers/JKDesktopShell — jkctl·jktriggers는 tools 소속이라
  Gemini 인벤토리 누락, docs/62 §8-6이 이미 파악했던 것) + jkbridge CSPRNG×1을
  **수기 SHA-256 헬퍼 1개로 흡수**(포맷 "sha256:"+64hex 교차 일치 유지, jkbridge SHA-1
  선례+셀프테스트 확장). JKDesktopShell.cpp:52-120의 BCrypt 4개소가 본체.
- 게이트: trust/permissions 관련 프로브(permissions.json 소유 프로브)+jkctl 스모크.
- **실측:** 커밋 81cf020(include/crypto/JKSha256.h+.cpp — jk::crypto::Sha256Hex(접두사
  없는 64hex)/RandomBytes(win32 dllimport BCryptGenRandom flag 2, non-Win
  fail-closed), FIPS 180-4 수기, 셀프테스트 케이스 13: 빈·abc·55바이트 FIPS/실측 3벡터)
  +0acbfaa(4개소 흡수: jkctl·jktriggers·JKDesktopShell 래퍼는 "sha256:"+Sha256Hex
  결합으로 형식 불변, jkbridge RandU32 실패 무시 동행 보존, dllimport BCrypt
  extern decl 제거합계 12건(JKDesktopShell 6+jkctl 6), bcrypt.h include 2건
  제거). **2단계 예방 노트(최종리뷰 Minor ①): jkcore는 bcrypt를 링크하지 않는다
  — RandomBytes의 __imp_BCryptGenRandom이 소비자 4곳의 잔존 -lbcrypt로 우연
  해소 중. 다음 CMakeLists 접촉 시 `if(WIN32) target_link_libraries(jkcore
  PUBLIC bcrypt) endif()` 추가 후 잔존 4곳 주석 갱신 — 안 하면 jkcore만
  링크하는 신규 타깃에서 링크 파단.** **리뷰 레슨: 라이브 BCrypt 호출은 JKSha256.cpp
  단일 소유로 수렴 — 전 엔진 grep 재확인. TDD RED가 플랜 원문 코드 결함 2건
  (니블 루프 b<4, 워드 로드 p[i] 슬라이딩)을 포획 — 플랜은 16efef2로 정정**
  (hashlib 독립 참조 x55=d5e28568…4072). 게이트 probe_jkbridge PASS ×2
  (CSPRNG→토큰·지문 경로 스모크)+jkctl 샌드박스 install 실측 지문=hashlib
  바이트 동일. 비-Windows 관찰 폭 widening 1건(ConsoleAppFingerprint ifdef 제거)
  — 1단계 "동작 변화 0 예외 1건"으로 기록.

### W3 — pty 파일 분할 (docs/62 §8-4) — **AS-BUILT 완료(2026-10-05)**
- `JKConPtyBridge`(Start/DrainOutput/WriteInput/Resize/Stop 바이트 스트림 인터페이스
  유지)를 플랫폼 TU로 분리 — 윈도우 ConPTY 구현 유지, 비윈도우 스텁 기존 것 승계.
- 게이트: terminal_hangul_probe 33/33 + 터미널 스모크.
- **실측:** 커밋 9098d9c(JKConPtyBridge.cpp → JKConPtyBridge_win32.cpp+posix.cpp,
  win32 본문 = 원본과 단 `#else !_WIN32` 스텁 블록 13행 삭제만 — 원시 diff 확정,
  CMakeLists 1행→2행 무조건 리스트). posix 스텁은 헤더 공개 멤버 8개 전수
  커버 — **원본 잠복 결함(비윈도우 public ProcessExited·ReaderThread 미정의) 봉합**.
  리눅스 ABI 실컴파일은 2단계 몫 — 합성 TU(`#ifndef _WIN32`→`#if 1`) ucrt64 g++
  컴파일+nm 심볼 8개 전부 정의·미정의 0으로 대체 실측(리뷰어 독립 재현 확인).
  게이트 terminal_hangul_probe **44/44 ×2**(플랜 기재 33/33은 후속 세션에서 체크
  증가 — 실측값 기준) + jkdesktop 셀프테스트 0 failure.

### W4 — 프로세스 스폰 어댑터 신설 (인벤토리 §1-1 본체) — **AS-BUILT 완료(2026-10-05)**
- `jk::process` 인터페이스 신설: SpawnProcess·CreateStdioPipe·PeekPipeData·
  KillProcess·TerminateProcessTree(=Win32 JobObject에 대응하는 프로세스 그룹 추상).
- 접촉점 3TU: JKLlmEngine.cpp(CreateProcess/Pipe/Peek/Job 전부)·JKWindowServer.cpp
  (스폰+Terminate)·JKConPtyBridge.cpp(스폰+Terminate). **Win32 JobObject는 1단계에선
  윈도우 구현으로 유지** — 리눅스의 pdeathsig/cgroups 대응은 2단계.
- LLM 턴의 cmd.exe 하드코딩(JKLlmEngine.cpp:90,167,273)은 2단계에서 셸 추상으로
  치환 예정 — 1단계는 인벤토리 §1-1의 main.cpp:2603 행처럼 **테스트 리터럴 오인 없도록
  접촉점만 마킹**.
- 게이트: probe_jkbridge(LLM 턴 경로)+probe_agent_events.
- **실측(플랜 B, 커밋 4c90a6e·21907f2·6db73eb·e373339):** 어댑터는
  `engine/include/process/JKProcess.h`(56행)+`JKProcess_win32.cpp`(195행)+posix
  스텁 — ConPTY 계열은 3번째 스폰 패밀리로 **배제**(JKConPtyBridge 보존, inventory
  §3 실측 승계). 스폰 2계열만 흡수: stdio 상속형(SpawnOptions.inheritedStdioPipes)
  +GUI 무stdio형. 계약 명문화: (a) 상속형 부모는 READ 끝만 보유, 스폰 직후 write 끝
  폐기(자식 stdout EOF 보장) (b) Job 핸들 close==트리 사망
  (JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE). PeekPipeAvail은 brokenError로
  ERROR_BROKEN_PIPE(109)·ERROR_NO_DATA(232) 보고(상수는 WinSDK 값 하드코딩 —
  어댑터 TU가 windows.h 소유, 소비 TU는 windows.h-clean 유지: JKLmEngine 직접
  include 소각, JKWindowServer는 미 include 관례+수기 dllimport ~60행 소각).
  셀프테스트 케이스 14 신설(echo 스폰 왕복+Job 스모크+GUI형+"양 파이프 EOF 관측"
  단언 — 계약 (a) 위반이 ~20s 지연으로만 관측되는 것을 감별력 있게 잡음). 흡수는
  원문과 미세 차이 3건이 의도적 개선: PeekPipeAvail SetLastError(0) 프리셋(stale
  ERROR_BROKEN_PIPE 오판 고착 레이스 1건 소거 — 즉, "동일 관측"이 아니라 "동일
  관측+레이스 소거")·ReadFile 부분 바이트 업스루프·부분 실패 파이프의 데이터
  보존(opus 최종리뷰 finding 7).
- **★인자 스왑 결함 픽스 e373339**(동작 변화 0의 결함 수선 예외 2번째 — 플랜 A
  ConsoleAppFingerprint 다음): JKLmEngine.cpp:311의
  `AssignProcessToJobObject(pi.hProcess, jobTree)` 인자 스왑 — 항상 FALSE(err=6),
  kill-on-close 계약이 무효 운용됐음(idle 킬·grandchild 정리가 사실상 미작동).
  어댑터 AssignToJob(job, proc) 계약으로 수선. probe_jkbridge가 LLM 턴 경로에서
  실재 검증.
- 마킹 이행: BuildEngineCmd·stub 리터럴·cmd.exe 접두 :273에 "stage-1 marking:
  shell literal, docs/68 W4" 주석 — 2단계 셸 추상 대상 명시(1단계 원문 유지).

### W5 — 파일시스템·경로 정리 (인벤토리 §6+§4 본체) — **AS-BUILT 완료(2026-10-05, 잔여 2건=2단계 결정 대기)**
- `jk::fs::GetExecutablePath` 신설 → GetModuleFileName 4TU 흡수(JKWindowServer/
  JKDesktopShell/JKLlmEngine/ClientBrowserApp — readlink("/proc/self/exe")는 2단계).
- **폰트 경로 추상화**: `C:\Windows\Fonts\malgun.ttf`·`consola.ttf` 하드코딩(클라 앱
  6파일+JKTextAtlas)을 탐색 체계로 — **docs/63 데스크톱 벡터 폰트의 폴백 체인과 통합
  설계**(desktextf_ 폴백이 이미 있으므로 "폰트 후보 경로 리스트" 어댑터 1개 추가).
- `C:\` 하드코딩 소각: JKApplication.cpp:38(검증 로그), ClientFilesApp.cpp:94·
  ClientFileDialogApp.cpp:138(기본 경로 → exe-dir/config 주도).
- 게이트: 런처·노트·파일 앱 스모크+probe_filedlg 계열.
- **실측(플랜 B 태스크 1, 커밋 a6f4f67):** `engine/include/fs/JKFs.h`+win32/posix
  신설 — 동적 재시도(1024 시작→배증, 최대 8회)로 **MAX_PATH 절단 취약점 동시 소거**,
  실패/비윈도우는 빈 문자열(기존 폴백 유지). 흡수 콜사이트 **실측 26건/11TU**(본
  섹션 원문 "4TU"는 정정 — 소비자 전원 A형 경로 소비, :8029 W형→A형 균일화 1건
  포함, 경로 파생 규약(뒤 "\\" 유무·파일명) 원문 유지). JKApplication.cpp:38 검증
  로그를 exe-dir/state/mouse_verify.log로 소각(fopen 실패 조용히 무시 동일).
  **ClientFilesApp `C:\` 기본 경로는 스펙 §2.3 충돌로 마킹만, 폰트 ImGui 앱 10파일
  흡수는 제외** — 2단계 결정 대기 2건(3번째는 W4의 cmd.exe 셸 리터럴 마킹)

### W6 — 시간·스레드 표준화 (인벤토리 §7-8) — **AS-BUILT 완료(2026-10-05)**
- `GetTickCount64`→`std::chrono::steady_clock`(JKLlmEngine 2곳),
  `CreateThread`→`std::thread`(JKLlmEngine/JKCrashHandler 2곳 — JKCrashHandler는
  windows.h-clean TU라 인클루드 최소 침범).
- 게이트: 빌드+probe_jkbridge.
- **실측(플랜 B 태스크 5, 커밋 b737788·c46f59e):** GetTickCount64는 steady_clock
  **차분만** 소비(절대시각 미소비 — monotonic 동등), kTurnIdleKillMs=
  `std::chrono::minutes{10}`로 원문 600000ms 정확 캐리. CreateThread→
  `std::thread(LlmTurnThread, TurnJob*).detach()` — 시그니처 `unsigned long
  __stdcall(void*)`→TurnJob* 직접형 전환(반환값 무소비 원문 유지), busy 롤백은
  try/catch(std::system_error)로 "no thread, no turn" 계약 보존. JKCrashHandler
  `std::thread(MirrorThread, ctx).detach()` — 원문이 CreateThread 실패도 무조건
  return true였으므로 catch 후 동일 리크+return true 유지(throw가 크래시 미러
  설치 중 프로세스 사망으로 에스컬레이션 방지 — 동일 관측 보존, windows.h-first
  규약 유지). JKCrashHandler 실제 경로는 `engine/src/JKCrashHandler_win32.cpp`
  (플랜 문서의 src/crash/ 오기 — 실측은 실제 경로로 수행). JKLlmEngine 잔여
  Win32 접촉=WaitForSingleObject dllimport 1건(spawn reap — 2단계 마이그레이션
  몫으로 명시 주석).

### W7 — IME·입력·DPI 경계 확인 (인벤토리 §9-10)
- SendInput·Imm*·SetProcessDpiAwarenessContext는 이미 `JKPlatform_win32.cpp` 단일 TU에
  있고 `JKPlatform.h`에 OS별 분기 설계 존재(docs/62 §8-7) — **신설 불요, 인터페이스
  이름만 확정**(jk::input::InjectSyntheticEvent·jk::ime::·jk::window::EnableHighDpiAwareness).
- 리눅스 IME는 §18 단일 소유 원칙상 OS IME 미개입이 정답 — 어댑터는 스텁만.
- 게이트: 불요(수술 없음).

### W8 — tools 5종 보완 조사+정리 (인벤토리 §5-3 교정, 지시문 스코프 누락 해소)
- jkagentd/jkchat/jktriggers/jkctl/jkbridge의 windows.h 접촉점 전수 조사 — 본 문서가
  이미 W2(jkctl·jktriggers 해시)·W4(스폰류)로 흡수한 것 외 잔여 목록화.
- **jkbridge WSAStartup(main.cpp:2285, 8899 HTTP+WS 서버)**은 네트워크 어댑터의 실질
  대상 — `jk::net` 인터페이스(서버 리슨·accept·recv/send) 정의만 1단계에서,
  구현은 Winsock 유지·Unix socket은 2단계.
- 게이트: probe_jkbridge·jkagentd 스모크.

### W9 — 완료 게이트 (docs/62 §3·§5)
- **Win32 회귀 프로브 전부 GREEN ×2 연속**(라이브 스택 정지 후 공식 런) — 이것이
  1단계 완료 판정 전부. .ps1 하네스는 Win32 전용 명문화 유지.
- jkdesktop 셀프테스트(지뢰찾기 논리층 16건 포함) 1회.

## 2. 순서와 근거

W1→W2→W3→W4→W5→W6→W7→W8→W9. 난이도 랭킹(인벤토리 §2)은 **2단계 구현의 난이도**지
1단계 수술 순서가 아니므로 역으로 참조만: 1단계는 난이도 높은 것(ConPTY·SendInput·IME)
부터 건드리지 않고, 인터페이스 존재 확인(W3·W7)까지만 하고 리눅스 구현은 남긴다.
기계적 치환(GetModuleFileName·GetTickCount64·CreateThread)은 1단계에서 미리 끝내
2단계의 실질 작업을 posix 전송·pty·flock 가드·폰트 탐색 4가지로 좁힌다(docs/62 §8 결론 승계).

## 3. 10월 세션 착수 순서

1. 본 문서 검토(스코프 확정 — tools W8 포함) → 2. W1 수술(흠 2건) → 3. W2-W6 순차
수술+각 게이트 → 4. W9 전체 회귀 ×2 → 5. 2단계(Termux/리눅스 머신) 착수 판단.
토큰 배분상 SDD 플랜 분할 권장: W1-W3(경계 수선)와 W4-W6(어댑터 신설)로 2 플랜.

**→ 1단계 플랜 A(W1-W3) 착수·완료 실측(2026-10-05)** — SDD 플랜
docs/superpowers/plans/2026-10-05-linux-stage1-surgery-a.md로 실행. 커밋
502987f(W1a)·fa46a4a(W1b)·81cf020+0acbfaa(W2)·9098d9c(W3, 상세는 위 각 W의
AS-BUILT 블록). 게이트: probe_app_tools ALL PASS ×2·probe_semantic_cursor
ALL PASS ×2·probe_jkbridge PASS ×2·terminal_hangul_probe **44/44 ×2**(33/33은
후속 세션에서 체크 수 증가)·jkdesktop 셀프테스트 0 failure·전체 빌드 에러 0.

**→ 플랜 B(W4-W6) 착수·완료 실측(2026-10-05)** — SDD 플랜
docs/superpowers/plans/2026-10-05-linux-stage1-surgery-b.md로 실행. 커밋
a6f4f67(W5 fs)·4c90a6e(W4 어댑터)·e373339(인자 스왑 결함 픽스)·21907f2(W4
LlmEngine 흡수)·6db73eb(W4 WindowServer 흡수)·b737788(W6)·c46f59e(스타일).
게이트(라이브 스택 정지 후 공식 런 ×2): probe_app_tools ALL PASS ×2·
probe_jkbridge PASS ×2·probe_agent_events 8/8 ×2·terminal_hangul_probe
44/44·jkdesktop 셀프테스트 0 failure(케이스 14 스폰/잡 스모크 포함)·전체 빌드
64타깃 에러 0. 셀프테스트가 포획한 실재 결함 1건(e373339 인자 스왑) 수선.
**W7-W9 대기** — W7은 어댑터 인터페이스 이름 확정만, W9=전체 회귀 ×2(플랜 B와
동일 게이트 세트면 재검증으로 대용 가능 검토 필요).

## 4. 리스크 보강 (docs/62 §6 외)

- **W5 폰트 추상화가 가장 "설계 스며"가 크다** — 클라 앱 6파일+아틀라스에 박힌 경로는
  docs/63 폴백 체인과 겹치므로, 두 체계를 한 어댑터로 흡수해야 이중 관리를 피한다.
- W4의 JobObject 추상화를 섣불리 "프로세스 그룹"으로 일반화하면 리눅스 구현 시
  권한·상속 문제로 곱절 낭비 — 1단계는 **윈도우 전용 구현 유지+인터페이스만**.
- CreateThread→std::thread는 JKLlmEngine의 kill-on-close Job 트리와 상호작용 —
  스레드 교체 시 JobObject 소유 구조를 바꾸지 말 것(동작 변화 0 원칙).