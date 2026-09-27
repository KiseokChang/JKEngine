# 플랫폼 의존성 인벤토리 — 리눅스 포팅 1단계 원료 (Gemini 조사, 2026-09-27)

> 지시문 팩: docs/gemini_handoff_linux_port_inventory.md — Gemini CLI가 조사.
> 아래 §1-4는 Gemini 산출물 원문, §5는 Claude 컨트롤러 검증 부록(2026-09-27).
> 10월 첫 세션에서 이 문서를 근거로 어댑터 추출 설계 착수.

## 1. 범주별 플랫폼 의존성 표

### 1. 프로세스/파이프
| 파일:줄 | API | 용도 | 1단계 어댑터 후보 인터페이스 이름 제안 |
|---|---|---|---|
| `engine/src/agent/JKLlmEngine.cpp:289` | `CreateProcessW` | LLM 워커 프로세스 실행 | `jk::process::SpawnProcess` |
| `engine/src/server/JKWindowServer.cpp:97, 8085` | `CreateProcessW` | 클라이언트 프로세스 스폰 | `jk::process::SpawnProcess` |
| `engine/src/terminal/JKConPtyBridge.cpp:119` | `CreateProcessW` | 터미널 셸(cmd 등) 스폰 | `jk::process::SpawnProcess` |
| `engine/src/agent/JKLlmEngine.cpp:259` | `CreatePipe` | 워커 프로세스 stdio 파이프 생성 | `jk::process::CreateStdioPipe` |
| `engine/src/JKCrashHandler_win32.cpp:133` | `CreatePipe` | 크래시 덤프 통신 파이프 생성 | `jk::process::CreateStdioPipe` |
| `engine/src/terminal/JKConPtyBridge.cpp:56` | `CreatePipe` | PTY 입출력 파이프 생성 | `jk::process::CreateStdioPipe` |
| `engine/src/agent/JKLlmEngine.cpp:334, 359` | `PeekNamedPipe` | 워커 stdout/stderr 읽기 가능 여부 확인 | `jk::process::PeekPipeData` |
| `engine/src/agent/JKLlmEngine.cpp:377` | `TerminateJobObject` | 타임아웃 시 LLM 프로세스 강제 종료 | `jk::process::TerminateProcessTree` |
| `engine/src/server/JKWindowServer.cpp:145, 418` | `TerminateProcess` | 자식 프로세스 강제 종료 | `jk::process::KillProcess` |
| `engine/src/terminal/JKConPtyBridge.cpp:218` | `TerminateProcess` | 터미널 셸 강제 종료 | `jk::process::KillProcess` |
| `engine/src/agent/JKLlmEngine.cpp:309` | `CreateJobObjectW` | 자식 프로세스 트래킹용 Job 생성 | `jk::process::CreateProcessGroup` |
| `engine/src/agent/JKLlmEngine.cpp:312, 314` | `SetInformationJobObject`, `Assign...` | 프로세스를 Job에 바인딩 (종료 동기화) | `jk::process::AssignToGroup` |
| `engine/src/main.cpp:2603` | `cmd.exe` | 터미널 기본 셸 하드코딩 | `jk::sys::GetDefaultShell` |
| `engine/src/agent/JKLlmEngine.cpp:90, 167, 273` | `cmd.exe` | LLM 런처용 더미 프로세스 / 셸 커맨드 구성 | `jk::sys::GetDefaultShell` |

### 2. pty/콘솔
| 파일:줄 | API | 용도 | 1단계 어댑터 후보 인터페이스 이름 제안 |
|---|---|---|---|
| `engine/src/terminal/JKConPtyBridge.cpp:22, 47` | `CreatePseudoConsole` | 윈도우 ConPTY(가상 콘솔) 세션 생성 | `jk::pty::CreatePseudoTerminal` |
| `engine/src/terminal/JKConPtyBridge.cpp:27, 197` | `ResizePseudoConsole` | 터미널 리사이즈 이벤트 전달 | `jk::pty::ResizeTerminal` |

### 3. 파이프 서버/클라
| 파일:줄 | API | 용도 | 1단계 어댑터 후보 인터페이스 이름 제안 |
|---|---|---|---|
| `engine/src/ipc/JKPipeTransport_win32.cpp:25` | `CreateNamedPipeA` | IPC용 네임드 파이프 서버 생성 | `jk::ipc::CreateLocalServer` |
| `engine/src/server/JKWindowServer.cpp:127, 486` | `WaitNamedPipeA` | 네임드 파이프 사용 가능 대기 (클라이언트) | `jk::ipc::WaitForLocalServer` |

### 4. 암호/해시
| 파일:줄 | API | 용도 | 1단계 어댑터 후보 인터페이스 이름 제안 |
|---|---|---|---|
| `engine/src/desktop/JKDesktopShell.cpp:52, 113` | `BCryptOpenAlgorithmProvider` | CNG SHA256 프로바이더 오픈 | `jk::crypto::InitSha256` |
| `engine/src/desktop/JKDesktopShell.cpp:55, 116` | `BCryptCreateHash` | SHA256 해시 컨텍스트 생성 | `jk::crypto::CreateHash` |
| `engine/src/desktop/JKDesktopShell.cpp:59, 118` | `BCryptHashData` | cmd 명령어 텍스트 해싱 | `jk::crypto::HashData` |
| `engine/src/desktop/JKDesktopShell.cpp:61, 120` | `BCryptFinishHash` | SHA256 다이제스트 추출 | `jk::crypto::FinishHash` |

### 5. 네트워크
검색 결과 내 명시적 의존성(`WSAStartup`, `WinHTTP`, `WinINet`) 발견되지 않음.

### 6. 파일시스템
| 파일:줄 | API | 용도 | 1단계 어댑터 후보 인터페이스 이름 제안 |
|---|---|---|---|
| `engine/src/server/JKWindowServer.cpp:118, 553, 8029` | `GetModuleFileNameA/W` | 현재 실행 파일/DLL의 절대 경로 획득 | `jk::fs::GetExecutablePath` |
| `engine/src/desktop/JKDesktopShell.cpp:27, 144` | `GetModuleFileNameA` | 실행 위치 기반 리소스 경로 결정 | `jk::fs::GetExecutablePath` |
| `engine/src/agent/JKLlmEngine.cpp:46` | `GetModuleFileNameA` | LLM 워커 구동 위치 식별 | `jk::fs::GetExecutablePath` |
| `engine/src/apps/ClientBrowserApp.cpp:143, 655` | `GetModuleFileNameA` | 브라우저 서브프로세스 런처 위치 식별 | `jk::fs::GetExecutablePath` |

### 7. 동기화/시간
| 파일:줄 | API | 용도 | 1단계 어댑터 후보 인터페이스 이름 제안 |
|---|---|---|---|
| `engine/src/agent/JKLlmEngine.cpp:324, 376` | `GetTickCount64` | 워커 프로세스 타임아웃 틱 계산 | `jk::time::GetMonotonicTicks` |

### 8. 스레드
| 파일:줄 | API | 용도 | 1단계 어댑터 후보 인터페이스 이름 제안 |
|---|---|---|---|
| `engine/src/agent/JKLlmEngine.cpp:428` | `CreateThread` | LLM 응답 대기/처리용 워커 스레드 생성 | `std::thread` (대체 권장) |
| `engine/src/JKCrashHandler_win32.cpp:157` | `CreateThread` | 크래시 미러 스레드 생성 | `std::thread` (대체 권장) |

### 9. 입력/IME
| 파일:줄 | API | 용도 | 1단계 어댑터 후보 인터페이스 이름 제안 |
|---|---|---|---|
| `engine/src/JKPlatform_win32.cpp:266, 277` | `SendInput` | Agent/Server발 합성 키보드/마우스 입력 전송 | `jk::input::InjectSyntheticEvent` |
| `engine/src/JKPlatform_win32.cpp:298, 316` | `ImmGetContext` | 네이티브 윈도우의 IME 컨텍스트 획득 | `jk::ime::GetContext` |
| `engine/src/JKPlatform_win32.cpp:303, 320` | `ImmGetConversionStatus` | 현재 IME 한/영 변환 상태 조회 | `jk::ime::GetConversionStatus` |
| `engine/src/JKPlatform_win32.cpp:328` | `ImmSetConversionStatus` | IME 한/영 상태 강제 전환 | `jk::ime::SetConversionStatus` |
| `engine/src/JKPlatform_win32.cpp:339` | `ImmNotifyIME` | IME 합성 문자 완료(Complete) 강제 통지 | `jk::ime::ForceCompleteComposition` |

### 10. 창/그래픽
| 파일:줄 | API | 용도 | 1단계 어댑터 후보 인터페이스 이름 제안 |
|---|---|---|---|
| `engine/src/JKPlatform_win32.cpp:49, 64` | `SetProcessDpiAwarenessContext` | 고해상도 DPI 스케일링 설정 활성화 | `jk::window::EnableHighDpiAwareness` |

### 11. 기타 (windows.h 종속성)
다음 파일들은 OS 추상화 모듈이 아님에도 `#include <windows.h>`를 직접 호출함.
*   `engine/src/theme/JKThemeConfig.cpp:10`
*   `engine/src/JKTextAtlas.cpp:16`
*   `engine/src/apps/ClientTaskmgrApp.cpp:16`
*   `engine/src/client/JKClientSurface.cpp:8`
*   `engine/src/apps/ClientShotApp.cpp:15`
*   `engine/src/apps/JKTerminalConfig.cpp:10`
*   `engine/src/apps/ClientNotifyApp.cpp:15`
*   `engine/src/apps/ClientBrowserApp.cpp:15`
*   `engine/src/apps/JKAppModule_script.cpp:17`
*   `engine/src/agent/JKAgentClient.cpp:6`
*   `engine/src/script/JKWorkshopStore.cpp:5`

## 2. 치환 난이도 Top 10 랭킹 (리눅스 포팅 기준)

1.  **`CreatePseudoConsole` (ConPTY)**: 리눅스의 `forkpty`/`openpty`는 프로세스 모델(POSIX)이 완전히 달라 핸들(Handle) 기반의 비동기 I/O 구조를 전면 재설계해야 함.
2.  **`SendInput`**: 리눅스는 통합된 GUI 자동화 API가 없고 X11(XTest)과 Wayland(uinput, wtype) 환경에 따라 주입 방식이 완전히 파편화되어 있음.
3.  **`Imm*` (IME 제어)**: 윈도우 IMM 구조를 리눅스의 ibus/fcitx DBus 통신 모델로 매핑하는 것은 1:1 치환이 불가능하며 별도의 이벤트 루프 통합이 필요.
4.  **`CreateJobObjectW` (프로세스 그룹)**: 부모 사망 시 자식 프로세스를 일괄 종료하는 윈도우 Job 기능을 리눅스의 `cgroups`나 `prctl(PR_SET_PDEATHSIG)`로 구현하려면 권한 및 상속 처리가 까다로움.
5.  **`CreateNamedPipeA` (서버 IPC)**: `\\.\pipe\` 네임스페이스 대신 Unix Domain Socket(`AF_UNIX`)을 사용해야 하며, 소켓 파일의 생성 경로와 권한(퍼미션) 관리를 별도로 추가해야 함.
6.  **`BCrypt*` (해시)**: 리눅스 커널 API만으로는 유저스페이스 해싱이 어려워 OpenSSL(`libcrypto`) 등 외부 의존성을 추가하거나 자체 SHA256 구현체를 내장해야 함.
7.  **`PeekNamedPipe`**: 파이프에 데이터가 있는지 논블로킹으로 엿보는 윈도우 특화 기능. 리눅스에서는 `poll`/`select` 후 버퍼를 직접 관리하는 방식으로 I/O 흐름을 수정해야 함.
8.  **`CreateProcessW`**: 리눅스의 `fork` + `execve` (또는 `posix_spawn`)로 교체 가능하나, 윈도우 파이프 상속 플래그와 리눅스 `dup2` 표준 입출력 리다이렉션 로직이 판이하게 다름.
9.  **`GetModuleFileNameA`**: 기계적으로 `readlink("/proc/self/exe")`로 치환 가능하나, `/proc` 파일시스템이 없는 특수 환경을 고려한 폴백이 필요함.
10.  **`SetProcessDpiAwarenessContext`**: Wayland/X11은 OS 차원의 API가 아닌 디스플레이 서버와 툴킷(SDL2/Wayland 프로토콜) 차원에서 핸들링하므로 빈 함수로 더미화해야 함.

## 3. 이미 플랫폼 중립인 모듈 목록 (안전 지대)

다음 헤더/소스 파일들은 주석 및 인클루드 내역을 근거로 볼 때 `windows.h` 의존성을 완벽히 배제하여 플랫폼 어댑터 작업 시 수정 없이 재사용 가능합니다.

*   `engine/include/script/JKWorkshopStore.h` (주석: *windows.h-clean 헤더*)
*   `engine/include/apps/ClientScriptApp.h` (주석: *header must not touch windows.h/fileapi.h*)
*   `engine/include/JKPlatform.h` (주석: *Headers stay clean of windows.h macros*)
*   `engine/src/JKHangulUtil.cpp` (주석: *Avoid pulling in <windows.h> in this translation unit*)
*   `engine/src/JKCrashHandler_win32.cpp` (주석: *windows.h-clean TU* — 윈도우 전용 구현체임에도 헤더 오염을 방지하기 위해 격리된 상태)

## 4. 조사 중 발견한 이상 현상 (설계 부채 / 하드코딩)

조사 과정에서 포팅 전 반드시 정리해야 할 "경로 하드코딩" 및 "환경 종속"이 다수 발견되었습니다.

1.  **시스템 폰트 경로 하드코딩 (심각)**:
    *   다수의 클라이언트 앱(`ClientSnapApp.cpp`, `ClientNotesApp.cpp`, `ClientFilesApp.cpp`, `ClientBrowserApp.cpp` 등)과 `JKTextAtlas.cpp`에서 **`C:\Windows\Fonts\malgun.ttf`** 및 **`C:\Windows\Fonts\consola.ttf`** 경로가 소스 코드에 그대로 박혀 있습니다.
    *   리눅스(`~/.fonts`, `/usr/share/fonts`) 및 DeX 환경을 위한 폰트 탐색/로딩 추상화 계층이 필수적입니다. — 참조: docs/63 데스크톱 벡터 폰트(폴백 체인)가 이 지점의 설계 재료가 된다.
2.  **드라이브/경로 하드코딩**:
    *   `C:\temp_jkwin_verify\mouse.log` (`JKApplication.cpp:38`): 개발 검증용 로그 경로 하드코딩.
    *   `I:\progwork\JKENGINE` (`JKLlmEngine.h:30`): LLM 워커의 CWD(현재 작업 디렉토리)가 특정 개발자 환경 경로로 고정되어 있습니다.
    *   `C:\` 루트 폴더 의존 (`ClientFilesApp.cpp:94`, `ClientFileDialogApp.cpp:138`): 기본 탐색 경로가 Windows의 C 드라이브로 고정되어 Unix 파일시스템(`/`)에서 오동작을 유발합니다.
3.  **수동 DLL 프로시저 로드**:
    *   `JKConPtyBridge.cpp`와 `JKPlatform_win32.cpp`에서 `GetProcAddress`를 사용하여 `CreatePseudoConsole`, `SetProcessDpiAwarenessContext`를 런타임에 동적 로드하고 있습니다. 빌드 시스템 차원(CMake 등)의 라이브러리 링크 대신 수기 코드로 처리되어 있어, 리눅스의 `dlsym` 등과는 다른 윈도우 특화 매크로 패턴입니다.

---

## 5. 컨트롤러 검증 부록 (Claude, 2026-09-27)

표본 검증 4건 + 스코프 감사 1건:

1. **적중 확인** — `JKLlmEngine.h:30`의 `I:\\progwork\\JKENGINE` 하드코딩(§4-2),
   malgun.ttf 하드코딩(§4-1, ClientSnap/Shot/AgentMgr/Notify/Notes/Files 6개 파일에서
   직접 확인), JKLlmEngine.cpp의 CreateProcess/Pipe/JobObject 계열(§1-1) 전부 실제 소스와
   일치. §3 안전 지대의 주석 근거들도 실재.
2. **오인 1건 정정** — §1-1의 `main.cpp:2603 cmd.exe`는 **터미널 기본 셸 하드코딩이
   아니다**: 셀프테스트 리터럴이다. 실제 셸은 `JKTerminalConfig`(exe-dir terminal.json,
   docs/27 단계 4)의 `shell` 필드로 설정 주도 — 치환 설계 시 이 행은 무시할 것.
   `cmd.exe` 진짜 하드코딩은 JKLlmEngine.cpp 쪽(§1-1 마지막 행)만 유효.
3. **§5 네트워크 "발견되지 않음"의 교정** — 엔진 코어(engine/src) 기준으로는 옳지만
   **전체 제품 기준으로는 누락**: 폰 게이트웨이 웹 서버가 `engine/tools/jkbridge/main.cpp:2285`
   `WSAStartup`(8899 HTTP+WS 서버)에 있고, tools 5종(jkagentd/jkchat/jktriggers/jkctl/
   jkbridge)이 windows.h 직접 인클루드. 원인은 지시문 팩의 스코프(engine/tools/** 제외).
   → **1단계 어댑터 설계에 tools 5종의 조사를 포함시킬 것**(jkbridge 네트워크 계층은
   Unix socket 포팅의 실질 대상).
4. **난이도 랭킹 채택** — Top 10 순서는 설계 우선순위로 그대로 채용 가능. 다만 1위
   ConPTY의 "전면 재설계"는 1단계(어댑터 추출)가 아니라 리눅스 구현 단계의 난이도다 —
   1단계는 `jk::pty` 인터페이스 정의까지만 하고 구현은 윈도우 것만 남긴다.

**10월 세션 사용법**: 이 문서가 1단계 설계의 입력. 설계 착수 시 (a) 어댑터 인터페이스
집합 확정(`jk::process/pty/ipc/crypto/ime/input/fs/time` — §1 표의 제안 이름 기반),
(b) tools 5종 보완 조사 병행, (c) 폰트 추상화는 docs/63 폴백 체인과 통합 설계.