# SDL2 ConPTY 터미널 클라이언트 (terminal.jkx) 아키텍처

> `engine`의 플래그십 앱 설계 문서. **Phase 1(MVP) 구현 완료 (2026-09-06)** — 파일 목록 §9, 상태 검증 §10.2.
> Windows ConPTY(`CreatePseudoConsole`)를 파워셸/셸에 연결하고, VT/ANSI 스트림을
> 파싱해 클라이언트 offscreen surface에 텍스트 그리드로 렌더링하는
> `jkapp_terminal.dll` / `terminal.jkx` 앱을 설계한다.
> 창 서버 기본 구조는 `19_sdl2_window_server.md`, 앱 모듈 ABI/컨테이너는
> `21_jkx_container.md`, 한글 IME 흐름은 `16_sdl2_jkwindow_ime.md` 참조.

## TL;DR

- 터미널은 **기존 클라이언트 앱 패턴 그대로** 간다: `ClientTerminalApp : JKClientApplication` (`ClientTetrisApp` 미러) + C ABI 모듈 `jkapp_terminal.dll` + `.jkx` repack. 메타는 `{name:"terminal", title:"Terminal", width:800, height:500}` — 1280×720 데스크톱 안이라 **fit 스케일 없이 1:1** 표시된다(doc 19 §7.3).
- 셸 브릿지는 **ConPTY**(Win10 1809+): `CreatePipe`×2 → `CreatePseudoConsole` → `STARTUPINFOEXA` + `UpdateProcThreadAttribute(PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE)` → 셸 `CreateProcess`. MinGW-w64(ucrt64) 헤더는 **선언이 기본 노출**된다(§2.1 검증) — 컴파일 타임 막힘은 없고, 구 환경 대비 `GetProcAddress` 폴백만 유지.
- 읽기는 **전용 리더 스레드**(블로킹 `ReadFile` → 락 걸린 버퍼 적체), 파싱·렌더링은 메인 루프가 프레임마다 배수(drain). UI 루프는 절대 ConPTY I/O에서 블로킹하지 않는다.
- VT 파서는 **손수 만든 서브셋 상태 머신(Phase 1, ~600-900줄)**: ConPTY 출력은 conhost가 **이미 렌더링해 재발행하는 정규화된 스트림**(커서 절대 이동 + 변경 셀 리페인트)이므로 완전한 에뮬레이터 파서가 필요 없다. libtsm 벤더링은 **명시적 폴백 단계**로만 스코프 — Phase 1에 도입하지 않는다.
- 텍스트 렌더링은 **stb_truetype 글리프 아틀라스**(선택 근거 §5.1): `third_party/stb`에 `stb_image.h`만 있는 현재 트리에 `stb_truetype.h`(퍼블릭 도메인 단일 헤더) 하나를 추가하고, 시스템 모노스페이스 폰트(Consolas)를 (코드포인트, fg색, bold) 키로 래스터라이즈해 아틀라스 텍스처에 캐싱 → `JKRenderCommandList::BlitTexture`로 셀 블립. 기존 `JKDC::EngPutCh` 8×16 비트맵 경로는 **픽셀 단위 `SDL_RenderDrawPoint`**라 그리드 전체 재그리기가 불가능하고(§5.1), `JKVectorFont`는 EUC-KR/캐시 없음/모노스페이스 메트릭 부재라 탈락.
- **프레임 스킵 코어 확장 1건**: `JKClientApplication::RenderAndCommit`은 루프마다 무조건 전체 씬 직렬화→리플레이→**전면 `SDL_RenderReadPixels`**→`CommitFull()`이라(JKClientApplication.cpp:501-565), 대기 중 터미널이 이를 매 ~1ms 반복하면 readback 비용이 상시 발생. 기본값 true인 가상 `IsFrameDirty()` 훅을 추가해 터미널만 그리드 변경 시에만 렌더한다(§5.3). 기존 앱 동작 불변.
- Phase 1 MVP: ConPTY 브릿지 + 파서 + 그리드 + 아틀라스 + 키 입력 + 리사이즈 + 수명주기. Phase 2: CJK 전각, IME 조합 표시, 마우스 보고(SGR mouse), 클립보드, 설정 파일, OSC 타이틀 완전화 (스크롤백은 2026-09-06 조기 구현 — §9 표 1행).
- **빠른 찾기**: 코드베이스 확인 §1, ConPTY 브릿지 §2, 프로세스·스레드 §3, VT 파서 §4, 그리드·렌더링 §5, 입력 매핑 표 §6, 리사이즈 수명주기 §7, 생성·종료 수명주기 §8, 구현 단계(파일별) §9, 리스크·검증 §10.

---

## 1. 코드베이스 사실 확인 (기획 전제 대조)

구현 전제가 되는 코드 사실을 전부 실제 파일로 검증했다. 기획안 대비 **정정/확정**은 다음과 같다.

| 항목 | 검증 결과 | 출처 |
|------|-----------|------|
| 앱 모듈 ABI | `JKAppMeta{name, title, width, height}` + `jk_app_meta()` / `jk_app_run_client(pipeName)` 2개 C 함수 — 기획안과 일치 | `include/apps/JKAppModule.h:39-49` |
| 클라이언트 렌더 경로 | **raw 픽셀 버퍼가 아니라 SDL_Renderer다.** 숨김 SDL 창 + `SDL_RENDERER_ACCELERATED\|SDL_RENDERER_TARGETTEXTURE` 렌더러 → `JKSDLRenderBackend` → 타깃 텍스처에 씬 리플레이 → `SDL_RenderReadPixels`로 `pixelBuffer_` readback → 공유 메모리 memcpy. `JKDC dc_`는 클라이언트 모드에서 `JKSDLRenderBackend`를 감싼다 | `JKClientApplication.cpp:55-65, 87, 501-565` |
| 커밋 dirty rect | **클라이언트는 항상 `CommitFull()`** — 프로토콜의 `DirtyRect[]` 목록은 있지만 커밋 경로는 전면 커밋뿐. 셀 단위 dirty 추적은 IPC가 아니라 **로컬 렌더 비용 절감** 목적으로만 의미가 있다 | `JKClientSurface.cpp:116-123, 125-141`, `JKClientApplication.cpp:563` |
| 키/텍스트 와이어 | `KeyDown`의 `keyCode`=SDL keysym.sym, `detail`=repeat, `option`=SDL 키모드. `Char`=`text[64]` UTF-8. `TextEditing`=`text` + `detail`=editStart, `option`=editLength | `JKWindowServer.cpp:722-751`, `JKClientSurface.cpp:222-229` |
| 마우스 스왑 | 와이어는 keyCode=SDL 버튼/detail=클릭수, 클라 `ReadLoop`가 `ev.detail=버튼, ev.keyCode=클릭수`로 스왑 — 터미널이 마우스를 쓸 때(Phase 2) 이 규약 준수 | `JKClientSurface.cpp:216-218` |
| 리사이즈 이벤트 | `ResizeSurface{surfaceId,w,h,newShmName}` → read 스레드가 최신 1건 coalesce + `SizeChanged` 큐잉 → 메인 루프 `ApplyPendingResize()`(임시 객체 Open 성공 후 교체) → `SetWindowRect` → `Invalidate` | `JKClientSurface.cpp:234-252, 271-299`, `JKClientApplication.cpp:232-251` |
| 서버 크롬 | 닫기 오버레이 → S→C `Close` → 클라 `Quit` 이벤트. 테두리 6px 리사이즈 핫스팟, 최소 clamp **64×48**, 커밋 순서 `BeginResizeSurface → ResizeLayer → Send(ResizeSurface)`. surface가 데스크톱 초과 시에만 fit 스케일 | `JKWindowServer.cpp:533-541, 545-547, 481-482, 582-604, 232-263` |
| 클라이언트 텍스트 경로 | `JKDC::TextOut/EngPutCh`는 **8×8 내장 비트맵 또는 HangulManager `english.fnt` 8×16**을 픽셀 단위 `DrawPixel`로 그린다. `JKSDLRenderBackend::DrawPixel` = `SDL_RenderDrawPoint` 1회/픽셀 | `JKDC.h:59-68`, `JKDC.cpp:124-176`, `JKSDLRenderBackend.cpp:72-75` |
| 텍스처 블립 | `JKRenderBackend::BlitTexture(texture, src, dst, alpha)` — **src 서브렉트 + 알파** 지원. `JKRenderCommandList`도 `BlitTexture` 명령을 직렬화/리플레이하므로 ComposeScene 경로에서 아틀라스 블립이 살아남는다. 단 **RGB per-draw 컬러모드는 없음**(알파만) — 아틀라스에 **선(先)컬러 래스터** 필요 | `JKRenderBackend.h:48-51`, `JKSDLRenderBackend.cpp:120-138`, `JKRenderCommandList.h:39, 67` |
| 텍스처 업로드 | `JKResourceCache::CreateImageFromRGBA(key,w,h,rgba)` + `FlushUploads(backend)`가 앱 스레드에서 즉시 SDL 텍스처 생성 — 아틀라스 업로드에 그대로 사용 가능 | `JKResourceCache.h:35-39`, `JKResourceCache.cpp:130-` |
| stb 벤더 | **`stb_image.h`만 존재. `stb_truetype.h`는 없음** — 신규 벤더 1개 헤더 필요 | `third_party/stb/` |
| TAB 포커스 사이클 | `ProcessOneEvent`가 `PreProcessMessage` **다음에** `SDLK_TAB` KeyDown을 가로채 포커스 사이클 — 앱이 TAB을 소비해도 무조건 실행된다. 터미널은 TAB(셸 완성)이 필요하므로 훅 1줄 추가(§5.4) | `JKClientApplication.cpp:263-281` |
| 타이머 | `SetTimerInterval(ms)` → `JKTimerThread` 레거시 타이머 → `Timer` 이벤트. `ClientTetrisApp::PreProcessMessage`가 처리하는 패턴 재사용 | `JKClientApplication.cpp:372-382`, `ClientTetrisApp.cpp:43-50` |
| 모듈 보일러플레이트 | `jk_app_meta`에서 메타 → `ClientXxxApp` 생성/Init/Run. 호스트는 exe 옆 `jkapp_<name>.dll` 로드, `FreeLibrary` 금지 | `JKAppModule_tetris.cpp:7-19`, `main.cpp:363-392` |
| jkx 아이콘 규약 | `assets/icons/launcher_<이름>@1x.png/@2x.png` — 실측 **@1x=64×64, @2x=128×128 정사각 아트**. `jkx-pack`이 컨테이너에 자동 삽입, 서버는 컨테이너에서 디코드 | `main.cpp:479-502`, `JKWindowServer.cpp:1021-1031`, 아이콘 PNG IHDR 실측 |
| ConPTY 헤더 | MinGW-w64 ucrt64(msys2 현행): `consoleapi.h`에 3개 함수 선언, guard는 `NTDDI_VERSION >= NTDDI_WIN10_RS5`. `_mingw.h` 기본 `_WIN32_WINNT=0xA00` → `NTDDI_VERSION=WDK_NTDDI_VERSION=NTDDI_WIN11_GE` ≥ RS5 → **추가 define 불필요**. `PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE`은 `winbase.h:1768` | `consoleapi.h:119-127`, `_mingw.h:270-271`, `sdkddkver.h:154,178-184`, `winbase.h:1768` |
| windows.h 격리 | 서버 TU는 레거시 typedef 충돌 회피용 최소 선언을 쓰지만(JKWindowServer.cpp:20-24), 터미널은 **자기 TU에서만** `windows.h`를 include하는 별도 PAL 파일로 격리하면 충돌 없음 (`JKPlatform_win32.cpp` 격리 패턴 준용) | `JKWindowServer.cpp:20-24`, `include/JKPlatform.h` |
| IME 이벤트 흐름 | 클라는 `SDL_StartTextInput()` 상태 → 조합 중 `TextEditing`(시각 전용), 확정 `Char`(삽입 전용). `JKEdit`는 UTF-8→KSSM 변환을 하지만 **터미널은 변환이 필요 없다** — UTF-8을 ConPTY stdin으로 그대로 전달 | `JKClientApplication.cpp:114`, doc 16 §2-§3 |

---

## 2. ConPTY 브릿지 구조

### 2.1 파이프·의사 콘솔 생성 순서

```
(1) CreatePipe(stdinRead, stdinWrite,  inherit=TRUE)   // 터미널 → 셸 입력
(2) CreatePipe(stdoutRead, stdoutWrite, inherit=TRUE)  // 셸 → 터미널 출력
(3) CreatePseudoConsole({cols, rows}, stdinRead, stdoutWrite, 0, &hpc)
(4) SetHandleInformation(stdinRead,  INHERIT, 0)  // ConPTY가 쓰는 끝만 상속,
    SetHandleInformation(stdoutWrite, INHERIT, 0) // 자식이 파이프 전체를 물려받아
                                                  // EOF가 안 오는 MS 문서 함정 회피
(5) InitializeProcThreadAttributeList(1)
    UpdateProcThreadAttribute(PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hpc, sizeof(hpc))
(6) CreateProcess(shell, ..., EXTENDED_STARTUPINFO_PRESENT, ..., &siEx, &pi)
```

- `COORD size`는 초기 cols×rows — §5.2의 그리드 유도식으로 계산한 값을 전달한다(§8 생성 수명주기와 정합).
- 셸 명령행은 **고정 문자열/환경값만** 조합한다: 기본 `powershell.exe`(`%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe` 우선, 실패 시 `powershell.exe`를 PATH 검색), 대안 `cmd.exe`(=`%COMSPEC%`), `wsl.exe`. 사용자 입력 텍스트는 `CreateProcess` 인수에 **절대 들어가지 않는다**(stdin 파이프로만).
- **헤더/런타임 이중 방어**: 컴파일은 현행 msys2 ucrt64에서 define 없이 통과(§1 표). 그래도 (a) 구 버전 툴체인에서 빌드될 경우를 대비해 `CreatePseudoConsole/ResizePseudoConsole/ClosePseudoConsole`을 `GetProcAddress`(kernel32 → KernelBase.dll 순)로 동적 적재하는 폴백, (b) 런타임 Win10 1809 미만 OS에서 기능 비활성 + 오류 표시를 넣는다. 동적 적재면 `_WIN32_WINNT` 정의 문제 자체가 소멸되므로 **폴백을 기본 경로로** 구현하는 것을 권장 — 정적 선언은 쓰지 않고 함수 포인터 타입만 정의한다.

### 2.2 리더 스레드와 입력 쓰기

- **리더 스레드**: `stdoutRead`에 블로킹 `ReadFile`(4-8KB 청크) → 뮤텍스 보호 `std::string`/ring 버퍼에 적체. 파이프 EOF/`ERROR_BROKEN_PIPE` = 셸(conhost) 종료 → 종료 플래그 설정(§8).
- **메인 루프 drain**: 프레임마다 락 짧게 잡고 버퍼를 꺼내 VT 파서에 투입(§4). 버퍼 상한(예: 1MB)을 두고 초과 시 최오래 데이터를 버려 파서 폭주를 방지 — `cat 대용량 파일` 같은 폭발적 출력에서도 UI 루프 생존.
- **입력 쓰기**: 메인 스레드에서 `WriteFile`(동기). VT 시퀀스/UTF-8은 수십 바이트 수준이라 파이프 버퍼(4KB)를 넘기 어렵고, 셸이 읽지 않아 블록할 가능성도 낮다. 그래도 안전을 위해 쓰기 큐(스레드) 도입은 리스크 관찰 후 Phase 2 과제로 남긴다(§10).
- **핸들 수명**: 리사이즈(`ResizePseudoConsole`)는 파이프를 재생성하지 않는다 — 핸들 누수 지점 없음. 모든 핸들(`hpc`, 파이프 4개, 프로세스/스레드)은 RAII 래퍼로 소유자 1곳(`JKConPtyBridge`)에 몰아 close 순서를 강제한다(§8.2).

---

## 3. 프로세스·스레드 구조

```
[서버 프로세스 --server] (변경 없음)
  main 스레드     : SDL 루프 → 크롬/입력 hit-test → Composite
  acceptor/read xN: 기존 그대로

[클라이언트 프로세스 --client terminal (또는 --jkx terminal.jkx)]
  main 스레드     : Run 루프 = DrainTimerChannel → DrainInputChannel → RenderAndCommit(게이트 §5.3)
                    ├─ 입력 이벤트(KeyDown/Char/SizeChanged/Timer) → VT 인코딩 → stdin WriteFile
                    └─ 출력 버퍼 drain → VT 파서 → 그리드 갱신 → 그리드 → 아틀라스 블립
  conpty 리더 스레드: ReadFile(stdoutRead) → 락 버퍼 적체 (셸 종료 감지)
  타이머 스레드    : JKTimerThread (커서 블링크 500ms)
  [자식] powershell.exe + conhost.exe(ConPTY 소유) — 클라이언트 종료 시 정리(§8.2)
```

- 클라이언트는 기존과 동일하게 **SDL 창을 소유하지 않는다**(숨김 창 1개는 IME/이벤트 초기화용). SDL 클라이언트 스레드 규율을 지키기 위해 **모든 ConPTY 호출은 메인/리더 스레드에서만** 수행하고 SDL과 스레드를 공유하지 않는다.
- 멀티 인스턴스: 터미널을 여러 개 스폰해도 각 클라이언트 프로세스가 자기 ConPTY를 소유하므로 충돌 없음(`.jkx` 임시 DLL은 per-pid — doc 21 §3).

---

## 4. VT 파서 설계

### 4.1 ConPTY 출력의 성격 (핵심 전제 — 검증·문서화 필요 사항)

ConPTY는 앱(vim 등)과 터미널 사이에서 conhost를 둔 **렌더링 프록시**다. 앱이 내보내는 원본 시퀀스를 통과시키는 게 아니라, conhost가 자신의 화면 버퍼를 **프레임마다 다시 렌더링해** 터미널에 `커서 절대 이동(CUP) + 변경 셀 리페인트(텍스트+SGR+EL/ED)` 형태로 재발행한다. 실무적 함의:

1. 터미널 파서는 **완전한 에뮬레이터 파서가 아니다** — 스크롤/삽입/삭제 같은 고급 시퀀스는 conhost가 내부에서 처리하고 결과를 정규화된 diff로 보내므로, "현재 셀에 문자 찍기 + 커서 이동 + 지우기 + 색"만 정확히 처리해도 vim/htop 화면이 재현된다.
2. 단, 앱 → ConPTY 방향의 개인 모드 전환(1049 등)과 창 크기 종속 동작은 터미널 쪽에도 **통과/처리**가 필요하다(아래 스코프).
3. 이 전제 때문에 **Phase 1은 손수 만든 서브셋 파서를 권장**한다(~600-900줄, 제어권 완전, 신규 벤더 의존 없음). libtsm 벤더링은 명시적 스코프의 폴백 단계로만 문서화하고 Phase 1에는 넣지 않는다(§9 Phase F).

### 4.2 상태 머신

```
GROUND ──ESC──▶ ESC ──'['──▶ CSI (파라미터/중간바이트 수집 ──최종바이트▶ 실행→GROUND)
          │        └──']'──▶ OSC (BEL 또는 ST 종료까지 수집 → 실행)
          │        └──'7'/'8'──▶ 커서 저장/복원 (DECSC/DECRC)
          │        └──'c'──▶ RIS(전면 리셋)
          └─ C0: LF/CR/BS/TAB/BEL 즉시 실행, SO/SI 무시
```

UTF-8 디코더를 파서 앞단에 둔다(멀티바이트 완료 시그노스 코드포인트 1개를 GROUND에 전달, 잘못된 시퀀스는 U+FFFD 1개로 치환). 컨트롤 시퀀스 도중 0x18/0x1A(취소) 처리.

### 4.3 Phase 1 처리 스코프

| 분류 | 시퀀스 | 처리 |
|------|--------|------|
| 커서 | `CUU/CUD/CUF/CUB (A/B/C/D)`, `CUP/HVP (H/f)`, `CNL/CPL (E/F)`, `CHA (G)`, `VPA (d)` | 절대/상대 이동, clamp |
| 지우기 | `ED (J: 0/1/2)`, `EL (K: 0/1/2)` | 커서 기준 셀/행/화면 속성 초기화 |
| SGR | `0 복원, 1 bold, 4 밑줄, 7 반전, 22/24/27 해제, 30-37/39 fg, 40-47/49 bg, 90-97/100-107 밝은색, 38;5;n / 38;2;r;g;b, 48 동일` | 셀 속성 렌더 상태 갱신 |
| 화면 전환 | `DECSET/DECRST 1049`(=1047+1048), `1047`, `1048` | 대체 화면 버퍼 스왑(§5.2) + 커서 저장/복원 |
| 스크롤 영역 | `DECSTBM (r)` | 상/하단 마진 기록, 마진 내 LF 시 스크롤업(마진 밖은 이동만) |
| 커서 저장 | `DECSC/DECRC (ESC 7/8, CSI s/u)` | 위치+속성 스택 1단 |
| 리셋 | `RIS (ESC c)` | 그리드/속성 전면 초기화 |
| OSC | `0/2 ; title (BEL\|ST)` | 창 제목 문자열 저장 → 클라 타이틀바 갱신(`JKWindow::SetTitle`) |
| 질의 | `DSR 6 (n)` → 응답 `ESC[{row};{col}R`, `DA (c)` → `ESC[?6c` | ConPTY/conhost가 커서 위치를 확인할 수 있으므로 최소 응답 구현 |
| 개인 모드 기타 | `25`(커서 숨김), `2004`(브래킷 붙여넣기 — 수신만 기록) | 커서 가시성 토글 구현, 2004는 Phase 2 |

**의도적 제외(Phase 1)**: DECDHL/더블라인, DECALN, 문자셋 전환(SCS/G0-G3 — ConPTY가 UTF-8로 정규화하므로), X10/SGR 마우스(Phase 2), OSC 클립보드 52(Phase 2).

---

## 5. 그리드·렌더링

### 5.1 텍스트 경로 결정: stb_truetype 글리프 아틀라스 (선택 (a))

세 후보의 검증 결과:

| 후보 | 판정 | 이유 |
|------|------|------|
| **(a) stb_truetype 아틀라스** ✅ | **채택** | 시스템 TTF(Consolas — Win Vista+ 전량 탑재)를 셀 크기에 맞춰 고품질 래스터. 아틀라스 1회 래스터 → 셀당 `BlitTexture` 1회(3천 셀 ≈ 수 ms). `third_party/stb`에 `stb_image.h` 벤더 선례가 있어 `stb_truetype.h`(퍼블릭 도메인, 단일 헤더) 추가가 집 규약과 일치. 색상은 아래 선(先)컬러 키로 해결 |
| (b) JKVectorFont | 탈락 | `.VFT` 외곽선을 **JKDC 프리미티브(선/채움)로 매번 스캔라인 채움** — 캐시 없음, EUC-KR/KSSM 바이트열 전용(UTF-8 그리드와 불일치), 모노스페이스 메트릭 보장 없음, `JKENGINE_FONT_DIR`(WINDBASE) 에셋 의존 | 
| (c) 내장 비트맵 폰트 | 탈락(폴백용으로만) | `JKDC::EngPutCh`(8×16 `english.fnt`)은 셀 메트릭은 이상적이지만 **픽셀 단위 `SDL_RenderDrawPoint`**라 100×31 그리드 전면 재그리기 시 최대 ~25만 즉시 드로우 콜/프레임 — 대화형 사용 불가. 브링업/디버그 폴백으로만 유지 |

**아틀라스 설계**: `JKRenderBackend`에 per-draw RGB 컬러모드가 없으므로(`JKSDLRenderBackend.cpp:120-138` — 알파만) 글리프를 **fg 색으로 미리 칠해** 아틀라스에 넣는다.

- 캐시 키 = `(codepoint, fgRGB, bold)` — 밑줄은 아틀라스 변형 없이 `HLine` 프리미티브, 반전은 fg/bg 스왑으로 처리해 키 공간 축소.
- 엔트리: 셀 박스(8×16 @ 100% 스케일) 래스터를 `JKResourceCache::CreateImageFromRGBA`로 등록 → `FlushUploads(backend)`로 즉시 업로드(§1 표). 페이지(예: 512×512) 단위 아틀라스 + LRU, 페이지 만료 시 `UnloadImage` → 재업로드.
- 블립은 `OnPaintClient(dc)`에서 `dc.GetBackend()->BlitTexture(atlas, &srcRect, dstRect)` — `JKRenderCommandList`가 `BlitTexture`를 직렬화하므로 ComposeScene 경로를 그대로 통과한다(§1 표).
- 박스 드로잉/블록 문자(U+2500-259F, ▀▄█▌▐░▒▓ 등 — vim/htop 필수): 폰트 글리프 대신 **JKDC 프리미티브(FillRect/DrawLine)로 직접 벡터 드로잉** — 폰트 커버리지 무관, 격자에 픽셀 정렬.
- CJK 글리프는 Phase 2(§9). 폰트 로드 실패 시 폴백: `JKDC::EngPutCh` 8×16 비트맵(기능은 느리지만 동작) + `HangulManager` 폰트 부재 경고와 동일한 패턴.

### 5.2 그리드 모델

```
Cell { uint32 cp;         // 유니코드 코드포인트 (조합 잔여 바이트 없음 — UTF-8 디코더 완결)
       uint32 fgRGB;      // 기본색 센티널(-1) 허용 → 테마 기본색 적용
       uint32 bgRGB;
       uint8  attrs;      // bold | underline | reverse
     }
Grid { cols, rows, cursor{x,y,visible}, fg/bg 현재 SGR 상태,
       std::vector<Cell> cells;  // cols*rows
       bool altScreen;           // 1049: main ↔ alt 그리드 스왑
     }
```

- **셀 메트릭 8×16 고정(Phase 1)**. 초기 surface 800×500에서 루트 프레임(타이틀 24 + 테두리 2, `JKWindow.cpp:37`)을 뺀 클라 영역 ≈ 796×472 → `cols = clientW/8 = 99`, `rows = clientH/16 = 29`. 여백 픽셀은 기본 bg로 칠한다. `CreatePseudoConsole`/`ResizePseudoConsole` 크기는 이 cols×rows를 그대로 쓴다.
- 전면 재그리기 비용: 99×29 = 2,871셀 ≈ bg `FillRect` + 글리프 `BlitTexture` 2회씩 ≈ 6천 SDL 호출/프레임 — GPU로 수 ms 이하, §5.3 게이트와 결합해 대기 중 비용 0.
- **dirty 추적**: 파서가 건드린 행 범위를 row-dirty 비트맵으로 기록 → 다음 프레임 그리기 범위. IPC 커밋은 어차피 전면(§1 표)이므로 dirty는 로컬 최적화 전용. Phase 1 MVP는 "그리드 dirty 플래그 1개"까지 단순화해도 충분하고, row 비트맵은 같은 Phase 1 후반에 추가.
- 스크롤백 버퍼와 CJK 전각 셀(`cp 2칸 차지`)은 Phase 2 — 셀 구조체에 `width` 필드 확장 여지를 남겨둔다.

### 5.3 프레임 게이트 (코어 최소 확장)

- 문제: `Run()` 루프가 `SDL_Delay(1)` 간격으로 `RenderAndCommit()`을 **무조건** 실행하고, 그 안에서 매번 씬 직렬화→리플레이→**전면 `SDL_RenderReadPixels`(800×500 RGBA ≈ 1.6MB)**→memcpy→`CommitFull()` (JKClientApplication.cpp:501-565). 셸 출력이 없을 때도 이 비용이 상시 발생한다(기존 게임 앱은 씬이 작아 관측되지 않았던 것).
- 해결: `JKClientApplication`에 `virtual bool IsFrameDirty() const { return true; }` 추가, `Run()`에서 `RenderAndCommit()` 직전 검사. 기본 true라 **기존 앱 11종 동작 불변**. 터미널은 `grid dirty ∥ 커서 블링크 토글 ∥ 타이틀 변경`일 때만 true.
- 커서 블링크: `SetTimerInterval(500)` → `Timer` 이벤트(`ClientTetrisApp::PreProcessMessage` 패턴) → 블링크 토글 + dirty 마킹.

### 5.4 입력 수집 지점과 TAB 훅

- 모든 입력은 `ClientTerminalApp::PreProcessMessage`(각 이벤트가 라우팅 전에 도달)에서 소비한다 — `ProcessOneEvent`가 `PreProcessMessage`를 TAB 가로채기보다 **먼저** 호출하므로(JKClientApplication.cpp:263-281) 컨트롤 트리/포커스 상태와 무관하게 키를 확실히 받는다. **반환은 항상 true** — false는 루프 종료 의미다(DrainInputChannel 규약).
- TAB만 예외: `PreProcessMessage` 이후에도 base가 무조건 포커스 사이클을 돌린다. `virtual bool WantsTabFocusCycle() const { return true; }` 가드를 TAB 분기에 추가하고 터미널이 false로 재정의한다(§1 표). 기존 앱은 기본 true라 무영향.

---

## 6. 입력 매핑 표 (SDL keysym → VT)

서버가 `keyCode=keysym.sym, option=keysym.mod, detail=repeat`로 전달한다(§1 표). 클라이언트는 `SDL_StartTextInput` 상태이므로 **인쇄 가능 문자는 `Char` 이벤트로만** 들어온다 — KeyDown에서 인쇄 문자를 다시 인코딩하면 이중 입력된다. KeyDown은 (1) 비텍스트 키(화살표/펑션/편집키)와 (2) `KMOD_CTRL`/`KMOD_ALT` 조합만 처리한다.

### 6.1 KeyDown (`ev.keyCode` = keysym.sym, `ev.option` = mod)

| SDL 키 | VT 바이트 | 비고 |
|--------|-----------|------|
| `SDLK_RETURN` | `0x0D` | CR (ConPTY가 LF 변환) |
| `SDLK_BACKSPACE` | `0x7F` | DEL — ConPTY 규약 |
| `SDLK_TAB` | `0x09` | §5.4 훅으로 포커스 사이클 차단 |
| `SDLK_ESCAPE` | `0x1B` | |
| `SDLK_UP/DOWN/RIGHT/LEFT` | `ESC [ A/B/C/D` | |
| `SDLK_HOME/END` | `ESC [ H / ESC [ F` | |
| `SDLK_PAGEUP/PAGEDOWN` | `ESC [ 5~ / ESC [ 6~` | |
| `SDLK_INSERT/DELETE` | `ESC [ 2~ / ESC [ 3~` | |
| `SDLK_F1..F4` | `ESC O P/Q/R/S` | SS3 |
| `SDLK_F5..F12` | `ESC [ 15~ 17~ 18~ 19~ 20~ 21~ 23~ 24~` | 16/22 건너뜀 |
| `Ctrl`+`A..Z` | `0x01..0x1A` (`letter & 0x1F`) | |
| `Ctrl`+`@`/`Space` | `0x00` | |
| `Ctrl`+`[ \ ] ^ _` | `0x1B 0x1C 0x1D 0x1E 0x1F` | |
| `Alt`+<위 시퀀스> | `ESC` 접두 + 동일 시퀀스 | `option & KMOD_ALT` |
| 그 외 인쇄 키 KeyDown | **무시** | `Char`가 담당(이중 입력 방지) |

- `detail`(repeat)은 그대로 통과 — SDL 자동 반복이 VT 반복을 만든다.
- `KeyUp`은 무시한다.

### 6.2 Char / TextEditing

| 이벤트 | Phase 1 | Phase 2 |
|--------|---------|---------|
| `Char` (`text` = UTF-8 확정 문자열) | 바이트열을 그대로 stdin `WriteFile`. **KSSM 변환 없음**(doc 16의 `JKEdit`와 결정적 차이 — ConPTY는 UTF-8을 먹는다) | 동일 |
| `TextEditing` (조합 중 pre-edit) | **무시** — 셸이 에코로 조합 결과를 보여주므로 편집기 수준 표시는 불요 | 그리드 인라인 pre-edit 렌더 + 백스페이스 양보 규약(doc 16 §3.1 준용) |

- 한글: IME 확정 `Char`("한글" UTF-8) → stdin → conhost → 셸 에코 → 터미널 표시. Phase 1에서 **그리드 렌더에 한글 글리프가 없으면** □(치환)로 보일 수 있다 — ASCII 동작 검증 후 Phase 2에서 CJK 렌더를 붙인다(§9).

### 6.3 마우스 (Phase 2 예고)

- 서버가 surface 로컬 px로 전달하는 MouseMove/Down/Up/Wheel(`JKClientSurface.cpp:216-218` 스왑 규약)을 셀 좌표로 역산 → `DECSET 1000/1002 + SGR 확장 1006` 활성 시 `ESC [ < b ; x ; y M/m` 인코딩. 비활성 시 휠은 `ESC [ 5~ / 6~`(대체 화면 앱용 스크롤)로 변환하는 클래식 동작.

---

## 7. 리사이즈 수명주기

서버 크롬 드래그 → 클라 적용까지의 전체 사슬(실명 검증 완료):

```
[서버] 테두리 드래그(MouseMove) → SetLayerScale 프리뷰
  ↓ MouseUp — clamp 64×48 (JKWindowServer.cpp:481-482)
(1) CommitChromeResize → BeginResizeSurface(w,h)   : 신규 shm 세대, 구 매핑 retired
(2) ResizeLayer(id,w,h,pixels)                     : 텍스처 원자적 교체
(3) Send(ResizeSurface {w,h,newShmName})
  ↓
[클라 read 스레드] pendingResize_ = 최신 1건 coalesce + SizeChanged 큐잉
  ↓ (JKClientSurface.cpp:234-252)
[메인 루프 ProcessOneEvent]
  ApplyPendingResize()   : 임시 JKSharedMemory Open 성공 후 교체 (JKClientSurface.cpp:271-299)
  logicalWidth/Height 갱신 → mainWindow_->SetWindowRect → Invalidate
  ↓ (JKClientApplication.cpp:232-251)
[ClientTerminalApp::PreProcessMessage(SizeChanged)]  ← base 처리 "후" 호출됨
  TerminalView 클라 영역 재측정 (루트 타이틀 24px 제외)
  cols = max(1, w/8), rows = max(1, h/16)          ← clamp 64×48 대응: 64px=8열, 48px=3행
  cols×rows 변화 시에만 ResizePseudoConsole(hpc, {cols, rows})
  그리드 리사이즈(내용 보존 근사: 좌상단 기준 절단/여백)
  ↓
[ConPTY] conhost가 재레이아웃 → vim 등에 SIGWINCH 상당 전달
  → 재렌더 diff가 리더 스레드 → 파서 → 그리드 (기존 파이프 재사용, 핸들 재생성 없음)
```

- coalesce가 이미 최신 1건만 남기므로 드래그 중 `ResizePseudoConsole` 폭주는 없다. 그래도 커밋 직후 100ms 타이머 디바운스를 선택적으로 얹을 수 있다(Phase 2).
- **최소 크기**: 서버 clamp 64×48에서 셀 8×16이면 8열×3행 — 0 이하가 되지 않도록 `max(1, …)` 필수.
- fit 스케일은 800×500 설계에서 **발생하지 않는다**(표시 ≤ 1280×720) — 리사이즈로 데스크톱을 초과하는 표시 크기는 서버가 중앙 clamp하므로 여전히 surface==표시 1:1.

---

## 8. 수명주기 (생성·종료)

### 8.1 생성

```
[런처 아이콘 클릭] SpawnClient → "--jkx terminal.jkx" (또는 --client terminal 폴백)
  → RunClientModule → jk_app_meta() {terminal, Terminal, 800, 500}
  → ClientTerminalApp::Init(meta) → JKClientApplication::Init
     (숨김 렌더러 + 파이프 연결 + surface 생성 + OnInit)
  → OnInit: 루트 JKWindow("Terminal") + chromeless TerminalView(DOCK_FILL)
     + 폰트/아틀라스 준비 + JKConPtyBridge::Start(cols, rows)
       → CreatePipe ×2 → CreatePseudoConsole → 셸 CreateProcess → 리더 스레드 기동
  → SetTimerInterval(500) (커서 블링크)
  → Run() 루프
```

- ConPTY 실패(셸 미발견, 1809 미만) 시: 클라이언트는 오류 텍스트를 그리드에 렌더한 뒤 `RequestQuit` — 서버에 좀비 surface를 남기지 않는다.

### 8.2 종료 (서버 닫기 오버레이 클릭 기준)

```
[서버] 닫기 오버레이 클릭 → Send(Close) S→C (JKWindowServer.cpp:533-541)
  ↓
[클라 read 스레드] Close 수신 → Quit 이벤트 큐 → 루프 break (JKClientSurface.cpp:179-185)
  ↓
[Run 루프 종료] → JKClientApplication 소멸자 → OnClose() → Close()
  ↓ ClientTerminalApp::OnClose (순서가 중요)
(1) ClosePseudoConsole(hpc)      : conhost가 셸에 종료 신호
(2) WaitForSingleObject(pi.hProcess, 3000ms)
(3) 타임아웃 시 TerminateProcess(pi.hProcess, 1)
(4) CloseHandle: 프로세스/스레드/파이프 4개 (RAII — 어떤 경로든 1회만)
  ↓
 JKClientApplication::Close → surface Close(C→S Close) → 서버 CleanupDisconnectedClients가 레이어 제거
```

- **셸이 먼저 끝나는 방향**(`exit` 입력, powershell 창 닫기 명령): 리더 스레드 `ReadFile`이 EOF/`ERROR_BROKEN_PIPE` → 종료 플래그. Phase 1은 종료 플래그 감지 시 `RequestQuit`(창이 함께 닫힘). Phase 2는 "[프로세스 종료 — 아무 키나 누르면 닫습니다]"를 그리드에 표시 후 키 대기로 업그레이드.
- 순서 규칙: **ClosePseudoConsole이 파이프 close보다 먼저**여야 리더 스레드가 EOF를 정상 인지한다. 리더 스레드는 OnClose 전에 조인하지 않고 EOF로 자연 종료시킨 뒤 소멸자에서 join — `StopReadThread`의 Cancel-then-join 교훈(JKClientSurface.cpp:162-169)과 동일한 디시플린.
- 서버 강제 종료(`--server` quit) 시 클라는 기존 규약대로 파이프 끊김 감지 → 정상 종료 → ConPTY도 같은 경로로 정리. 셸이 클라보다 오래 살 경우는 (3) 폴백이 흡수.

---

## 9. 구현 단계 (파일 레벨)

엔진(parser/grid/atlas/bridge)은 `jkcore` 정적 라이브러리에 넣고 **앱 UI만 모듈 DLL에** 둔다 — exe의 `test` 셀프테스트에서 파서/그리드를 직접 단위 검증할 수 있고(§10), 향후 Dear ImGui 호스트(백로그)도 같은 ConPTY 브릿지를 재사용한다. windows.h 의존은 bridge TU 하나로 격리(§1 표).

### Phase 1 — MVP (동작 터미널 1개)

| 구분 | 파일 | 내용 |
|------|------|------|
| 신규 | `include/terminal/JKVtParser.h`, `src/terminal/JKVtParser.cpp` | §4 상태 머신 + UTF-8 디코더. 순수 C++(Windows 헤더 없음) — 셀프테스트 대상 |
| 신규 | `include/terminal/JKTerminalGrid.h`, `src/terminal/JKTerminalGrid.cpp` | §5.2 Cell/Grid, alt screen, DECSTBM 마진, dirty 기록 |
| 신규 | `include/terminal/JKGlyphAtlas.h`, `src/terminal/JKGlyphAtlas.cpp` | stb_truetype 래스터 + (cp,fg,bold) LRU 페이지 + `JKResourceCache` 업로드 |
| 신규 | `third_party/stb/stb_truetype.h` | 벤더 (퍼블릭 도메인, 단일 헤더) |
| 신규 | `include/terminal/JKConPtyBridge.h`, `src/terminal/JKConPtyBridge.cpp` | §2 전부. windows.h는 이 TU만. GetProcAddress 동적 적재 + 폴백 |
| 신규 | `include/apps/ClientTerminalApp.h`, `src/apps/ClientTerminalApp.cpp` | `JKClientApplication` 파생: OnInit(루트+view 구성), PreProcessMessage(입력 캡처/리사이즈/타이머), OnClose(§8.2 순서) |
| 신규 | `include/apps/TerminalView.h`, `src/apps/TerminalView.cpp` | `JKWindow` 파생 OnPaintClient: bg FillRect → row-dirty 셀 bg/글리프 블립 → 커서 |
| 신규 | `src/apps/JKAppModule_terminal.cpp` | C ABI (`JKAppModule_tetris.cpp` 미러): `{name:"terminal", title:"Terminal", width:800, height:500}` |
| 수정 | `CMakeLists.txt` | (1) `jkcore` 소스에 `src/terminal/*.cpp` 4개 추가 — `third_party/stb` include는 이미 jkcore에 있음(CMakeLists.txt:102-107). (2) `jkapp_terminal SHARED` 타깃(JKAppModule_terminal + ClientTerminalApp + TerminalView) — CMakeLists.txt:151-173 패턴. (3) `-static-libstdc++` foreach 목록에 추가(261-269). (4) `jkx-pack terminal` 커스텀 커맨드 + `jkx_packages` 의존 추가(286-352 패턴) |
| 신규 | `assets/icons/launcher_terminal@1x.png`(64×64), `@2x.png`(128×128) | 정사각 아트 규약(§1 표 실측). `jkx-pack`이 컨테이너에 자동 삽입 |
| 수정 | `include/client/JKClientApplication.h`, `src/client/JKClientApplication.cpp` | `IsFrameDirty()` 가상 추가 + `Run()` 게이트(§5.3), `WantsTabFocusCycle()` 가드(§5.4) — 합계 ~10줄, 기본 동작 불변 |

**Phase 1 종료 기준**: powershell 프롬프트가 컬러로 보이고, `dir`/`cls`/커서 이동/`Ctrl+C`가 동작하며, 테두리 리사이즈가 vim 없이 프롬프트 재출력으로 확인되고, 닫기로 셸이 정리된다.

### Phase 2 — 완성도

| 순서 | 파일 | 내용 |
|------|------|------|
| 1. 스크롤백 ✅ (2026-09-06 조기 구현) | `JKTerminalGrid.cpp`, `TerminalView.cpp`, `ClientTerminalApp.cpp` | 메인 스크롤(top-margin 스크롤) 때 이탈 행을 `scrollback_` deque 스냅샷(1000줄 상한, alt 화면·마진 스크롤 제외, RIS로 해제). 뷰는 히스토리+그리드 결합 뷰포트 렌더, 휠 1 노치 = 3행, 키 입력 시 live 복귀. 리플로우 없음(스냅샷 폭 고정). alt 화면 휠 → §6.3 키 변환은 미구현 |
| 2. CJK 전각 | `JKTerminalGrid.cpp`, `JKGlyphAtlas.cpp` | 유니코드 East Asian Width로 cp 2칸 차지(빈 셀 팔로우), 한글 글리프 — HangulManager `HANGUL.FNT` 16×16 폴백 또는 맑은 고딕 래스터. doc 16의 KSSM 계층은 **불필요**(터미널은 UTF-8 직통) |
| 3. IME 조합 표시 | `ClientTerminalApp.cpp`, `TerminalView.cpp` | `TextEditing`을 그리드 인라인 pre-edit로 렌더, 확정 `Char`에서 clear(doc 16 §3.1 책임 분리 준용). 조합 중 백스페이스/화살표는 IME 양보 |
| 4. 마우스 보고 | `ClientTerminalApp.cpp`, `JKVtParser.cpp` | `DECSET 1000/1002/1006` 추적, 마우스 이벤트 → SGR 시퀀스 인코딩(§6.3) |
| 5. 클립보드 | `ClientTerminalApp.cpp` | 선택(마우스 드래그) + `Ctrl+Shift+C/V` 또는 Win32 클립보드 PAL(`JKPlatform` 확장). 브래킷 모드(2004) 응답 |
| 6. 설정 파일 | `include/terminal/JKTerminalConfig.h/.cpp` | `terminal.ini`(exe 옆): shell(powershell/cmd/wsl/커스텀), 폰트/크기, 컬러 테마, 스크롤백 크기. 브릿지/아틀라스에 주입 |
| 7. OSC 타이틀 완전화 | `JKWindowServer`(와이어), `JKClientConnection` | `SetTitle` S→C 메시지 신설(서버 레이어 타이틀 갱신) — Phase 1은 클라 타이틀바만 갱신(`JKWindow::SetTitle`, JKWindow.h:17) |

### 명시적 폴백 단계 (착수 조건부, Phase 1에 포함하지 않음)

- **libtsm 벤더링**: Phase 1 파서로 vim/htop/far 매니저 스모크에서 **재현 불가한 화면 깨짐**이 2종류 이상 남을 때만 착수. 스코프: `third_party/libtsm` + `JKVtParser` 인터페이스 뒤 구현체 교체 — 그리드/렌더/입력 계층은 그대로. libtsm은 화면 모델까지 갖고 있어 `JKTerminalGrid`를 대체한다는 점을 비용으로 명시한다.

---

## 10. 리스크·검증 방법

### 10.1 리스크

| 리스크 | 등급 | 대응 |
|--------|------|------|
| ConPTY 정규화 스트림 가정 위배(특정 앱이 고급 시퀀스에 의존) | 중 | §4.1 전제를 문서로 고정 + §10.2 스모크 앱 선정(vim/htop/far). 실패 시 libtsm 폴백(§9) |
| 전면 `SDL_RenderReadPixels` 상시 비용 | 중 | `IsFrameDirty` 게이트(§5.3) — 검증: 대기 중 클라 CPU ≈ 0 확인 |
| 리더 스레드-메인 락 경합(폭발적 출력) | 중 | 버퍼 상한 + 프레임당 drain 상한(예: 64KB) — `cat` 대용량에서 UI 프레임 유지 확인 |
| WriteFile 블로킹(셸이 stdin을 안 읽음) | 저 | VT 쓰기는 수십 바이트 — 관찰 후 Phase 2 쓰기 스레드/overlapped |
| MinGW/OS 가용성(1809 미만) | 저 | GetProcAddress 동적 적재 기본 경로 + 런타임 체크(§2.1) |
| resize clamp 64×48 → 그리드 0행/0열 | 저 | `max(1, …)` clamp(§7) + cols/rows 불변 시 `ResizePseudoConsole` 스킵 |
| 셸 잔존(좀비 powershell) | 중 | §8.2 순서 강제 + 검증 항목 7에서 `tasklist` 확인 |
| 모듈 FreeLibrary/힙 오염 | — | 기존 규약 준수(모듈 상주, doc 21 §3) — 터미널 특이사항 없음 |
| 키 이중 입력(Char vs KeyDown) | 중 | §6.1 규칙(인쇄 키는 Char 전담, Ctrl/Alt만 KeyDown) + 검증 항목 4 |

### 10.2 검증 방법

1. **셀프테스트 확장**: `test` 모드(`RunAppSelfTest`)에 파서 골든 테스트 추가 — `ESC[2J`, `ESC[10;20H`+문자, SGR 256/truecolor, `ESC[?1049h/l` 스왑, UTF-8 3바이트/불완전 시퀀스, DECSTBM 스크롤. 엔진이 jkcore에 있으므로 exe에서 직접 검증 가능(§9).
2. **단독 기동**: `--server` + 런처에서 terminal 클릭(or `--client terminal`) → powershell 프롬프트 1:1 표시(fit 스케일 로그 없음) 확인.
3. **타이핑/에코**: ASCII 타이핑이 1회씩만 에코(§6.1 이중 입력 체크), `Tab` 완성 동작(포커스 사이클 발생 안 함), 화살표로 히스토리 조회, `Ctrl+C` 인터럽트.
4. **컬러**: powershell 기본 프롬프트(모듈명 컬러), `$PSStyle` 또는 `Write-Host -ForegroundColor` 16색, truecolor 스크립트 1개.
5. **TUI 스모크**: `vim` 파일 열기/편집/`:q`, `wsl htop` — 커서/색/전체 재출력 정합. 깨짐이 남으면 §4.1 전제 재검토 → 폴백 판정.
6. **리사이즈**: 우측/하단 엣지 드래그 → vim 열어 둔 상태에서 재레이아웃 확인, 64×48까지 줄여 8×3행 clamp 확인, 빠른 드래그(coalesce)로 크래시 없음. `tmp/*.ps1` 합성 입력 프로브 재활용(메모리의 서버 스모크 하네스).
7. **수명주기**: 닫기 오버레이 → 클라 exit=0 + `tasklist`에서 powershell/conhost 잔존 0. 셸에서 `exit` → 창 자동 종료. 서버 재기동 후 재스폰(파이프/shm 재무장). 터미널 2개 동시 스폰 → 독립 ConPTY.
8. **성능**: 대기 중(출력 없음) 클라 CPU ≈ 0(게이트 검증), `Get-ChildItem -Recurse C:\` 같은 폭발 출력 중에도 서버 컴포지션 떨림 없음.
9. **Phase 2 회귀**: 스크롤백 휠, 한글 입력+전각 렌더, IME 조합 표시, 마우스 보고(htop 클릭) — 각 단계마다 1-8 회귀.

## 11. 알려진 제약 / 이후 과제

- Phase 1은 커서 블링크 외 텍스트 애니메이션(633 커서 스타일 등) 미지원. 커서 스타일 DECSET 12/25만 반영.
- 서버 와이어에 타이틀/아이콘 갱신 메시지 없음 — Phase 2 항목 7.
- 멀티 탭/분할은 이 문서 스코프 밖(플래그십 단일 터미널 완성 후 별도 기획).
- Dear ImGui 호스트 백로그(#101)와 ConPTY 브릿지 재사용 관계는 해당 기획 문서에서 정의.