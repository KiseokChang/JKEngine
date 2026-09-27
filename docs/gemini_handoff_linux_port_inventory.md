# Gemini 작업 팩 — 리눅스 포팅 1단계 플랫폼 의존성 전수 조사 (2026-09-27)

> 이 파일은 Claude Code 세션에서 만든 Gemini CLI용 지시문이다. 사용자가 Gemini에
> 이 파일 경로를 전달하고 조사를 시킨 뒤, 산출물을 10월 첫 Claude 세션에서 검토한다.
> 읽기 전용 작업 — 어떤 소스 파일도 수정하지 않는다.

## 배경 (필요한 것만)

- JKEngine은 Windows 전용 C++ 데스크톱 엔진(SDL2 + QuickJS). 2026-10에 리눅스 포팅 착수 예정.
- 설계 문서: `docs/62_linux_port_and_dex.md` — 이미 "OS 창 하나만 띄우면 셸 전체 이식"
  발견이 기록돼 있고 3단계(①플랫폼 어댑터 추출 → ②Termux → ③DeX APK)로 나뉘어 있다.
- 이 조사는 **1단계(플랫폼 어댑터 추출)의 원료**를 만드는 것: 어떤 파일이 어떤
  Windows 전용 API를 어디서 쓰는지의 지도. 설계 결정은 하지 않는다.

## 작업: 플랫폼 의존성 인벤토리

대상: `engine/src/**`, `engine/include/**` (벤더 디렉토리 제외 — `vendor/`, `quickjs*`,
`imgui*` 등 서드파티는 제외. `engine/tools/**`와 `engine/build/**`도 제외).

다음 범주별로 **파일:줄 + 용도 한 줄** 표를 만든다:

1. **프로세스/파이프** — `CreateProcess`, `CreatePipe`, `PeekNamedPipe`,
   `TerminateProcess`, `JobObject`(KILL_ON_JOB_CLOSE), `cmd.exe` 호출
2. **pty/콘솔** — `ConPTY`, `CreatePseudoConsole`, `ResizePseudoConsole`
3. **파이프 서버/클라** — `CreateNamedPipe`, `WaitNamedPipe` (JKWindowServer 파이프)
4. **암호/해시** — `BCrypt*` (SHA-256 등), `CryptProtectData`
5. **네트워크** — `Winsock`(`WSAStartup`), `WinHTTP`, `WinINet`
6. **파일시스템** — `GetModuleFileName`, `SHGetKnownFolderPath`(APPDATA 등),
   `CopyFile`, `MoveFileEx`, 경로 하드코딩(`I:\`, `C:\`)
7. **동기화/시간** — `CRITICAL_SECTION`, `SRWLOCK`, `GetTickCount64`, `QueryPerformanceCounter`
8. **스레드** — `CreateThread`, `TLS`
9. **입력/IME** — `SendInput`, `IMM*`(`ImmGetContext` 등), `AttachThreadInput`
10. **창/그래픽** — `SDL2` 밖의 `GDI`/`DWM`(`PrintWindow`, `CopyFromScreen`류 캡처,
    `SetProcessDPIAware`), `GetDC`
11. **레지스트리/시스템** — `RegOpenKey`, `GetSystemMetrics`
12. **기타 전용** — 위에 없는 windows.h 의존(리스트에서 눈에 띄는 것)

## 산출물 형식

`docs/gemout_platform_inventory.md` 로 저장:

1. 범주별 표(파일:줄 | API | 용도 | 1단계 어댑터 후보 인터페이스 이름 제안)
2. 상위 10개 "치환 난이도" 랭킹 — 난이도 근거 한 줄 (예: named pipe → Unix socket은
   기계적, SendInput → uinput은 설계 요구)
3. **이미 플랫폼 중립인 모듈 목록** (windows.h 인클루드 없는 src 파일) — 어댑터
   작업에서 손 안 대도 되는 안전 지대
4. 조사 중 발견한 이상(빌드 시스템 종속, 경로 하드코딩 등) 별도 섹션

규칙: 추측 금지 — 표의 모든 행은 실제 파일:줄 근거를 가져야 한다. 수량이 많아도
정확도 우선. 이 파일과 `docs/62_linux_port_and_dex.md` 외의 문서를 읽을 필요는 없다.