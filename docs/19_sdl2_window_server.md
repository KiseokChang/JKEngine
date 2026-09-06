# SDL2 윈도우 서버 (Phase 2) 아키텍처

> `engine`의 프로세스 분리(window server) 구조를 정리한 문서.
> 2026-09 Phase 2 구현(`570c802`, `a6d383e`), 2026-09-05 좌표계 통일 수정과
> 같은 날 추가된 **서버 측 창 프레임(이동/닫기/리사이즈)** 을 반영한다.
> 좌표계/DPI의 근본 규칙은 `14_sdl2_window_dpi.md` 참조.

## TL;DR

- 런처는 **윈도우 서버 프로세스**(`--server`)로, 앱들은 **클라이언트 프로세스**(`--client <app>` / .jkx)로 분리된다. 앱 모듈은 11종: 게임 3종(`minesweeper`/`tetris`/`testwin` — `testwin`은 760×560 컨트롤 쇼케이스), WINDBASE 계열 8종(`jango`/`occ`/`pcx`/`vector`/`iconedit`/`recog`/`vfont`/`vpres`, 2026-09-05 추가). WINDBASE 앱들은 1920×1080 절대 배치 그대로를 surface 설계 크기로 쓰며 서버가 fit 스케일(§7.3)로 표시 축소한다. UI 로직은 단일 프로세스 앱과 공유(`JangoUI`/`OccUI` 등 `include/apps/*UI.h`)하고 모듈 DLL은 `JKAppModule_<name>.cpp` C ABI만 노출한다.
- 클라이언트는 SDL 윈도우를 띄우지 않고 **공유 메모리 RGBA surface**에 그린 뒤 named pipe로 `CommitSurface`를 보낸다. 서버는 이를 SDL 텍스처로 합성(compositing)한다.
- 입력은 서버가 hit-test해서 **surface 로컬 좌표**로 변환해 클라이언트에 전달한다. 오디오도 서버가 유일한 SDL_mixer 인스턴스로 대행 재생한다.
- **좌표 모델(2026-09-05 확정)**: 서버 쪽 모든 그리기·hit-test·마우스를 **물리 픽셀(px)** 하나로 통일한다. 논리 pt ↔ 물리 px 환산은 렌더러 비율(`outputScale = physW/logW`)로만 한다. `GetDpiForWindow/96` 같은 DPI 기반 환산을 섞으면 혼합 배율 모니터에서 어긋난다.
- **빠른 찾기**: 프로세스 구조 §2, 와이어 프로토콜 §3, 서버 구성요소 §4, 클라이언트 §5, 좌표 모델 §6, 생명주기 §7, 검증 §8.

---

## 1. 두 렌더 경로

| 경로 | 실행 방법 | 창 소유 | 사용 파일 |
|------|----------|---------|----------|
| 단일 프로세스 (스레드 분리) | `jkdesktop.exe minesweeper` 등 | `JKRenderThread` (앱 전용 SDL 창) | `JKApplication`, `JKRenderThread` |
| 윈도우 서버 (Phase 2) | `--server` (서버) / `--client minesweeper\|tetris` (클라이언트) | 서버가 유일한 SDL 창 소유 | `server/JKWindowServer`, `client/JKClientApplication` |

단일 프로세스 경로의 좌표계·마우스 규칙은 `14_sdl2_window_dpi.md` §13-§14 참조.

---

## 2. 프로세스·스레드 구조

```
[서버 프로세스: jkdesktop.exe --server]
  main 스레드      : SDL_PollEvent → HandleSDLEvent → Composite 루프 (Run)
  acceptor 스레드  : 파이프 대기 → Hello/CreateSurface 핸드셰이크 → pendingClients_
  read 스레드 x N  : 클라이언트별 파이프 읽기 → QueueMessage
  audio 스레드     : JKAudioThread (서버가 유일한 SDL_mixer 소유)

[클라이언트 프로세스: jkdesktop.exe --client minesweeper]
  main 스레드      : 앱 로직 → offscreen surface 렌더 → CommitSurface
  (SDL 윈도우 없음 — SDL은 이벤트/IME 초기화용으로만 HIDDEN 창 1개)
```

- **앱 모듈 동적 로딩(Phase B, 2026-09-05)**: 게임 앱은 `jkapp_<이름>.dll` 공유 라이브러리로 분리되어 `--client` 호스트가 `LoadLibraryA`로 로드한다. 계약은 `include/apps/JKAppModule.h`의 C ABI 2개(`jk_app_meta`, `jk_app_run_client`)뿐 — C++ 객체는 모듈 경계를 넘지 않고, 앱 객체 생성·Init·Run·소멸은 전부 DLL 안에서 일어난다. 각 모듈은 `jkcore`(코어 정적 라이브러리) 사본을 정적 포함하며 `-static-libstdc++`로 자기 CRT 상태를 가진다. DLL은 exe 옆에 두고 LoadLibrary 표준 검색 순서로 해석된다. `.jkx` 컨테이너(Phase C)도 같은 ABI를 재사용한다 — `21_jkx_container.md`. **주의: 모듈을 실행 도중 FreeLibrary하면 힙이 오염되므로 금지**(doc 21 §3).

- 서버는 클라이언트 스폰 시 `CreateProcess`로 **자기 exe를 재실행**하며 작업 디렉터리를 exe 위치로 맞춘다(에셋 로딩 보장).
- 클라이언트 크래시 감지: 파이프 끊김(`IsDisconnected`) → surface 정리 + read 스레드 join (`CleanupDisconnectedClients`). join은 `clientsMutex_` **밖에서** 수행(교착 방지).

---

## 3. 와이어 프로토콜 (`ipc/JKWireProtocol.h`)

- 전송: Windows named pipe `\\.\pipe\JKWindowServerPipe` (`JKPipeTransport`).
  - **파이프 핸들은 반드시 `FILE_FLAG_OVERLAPPED`로 생성** (서버 `CreateNamedPipeA`, 클라이언트 `CreateFileA` 공통). 오버랩 플래그가 없으면 동기 핸들이 되어 한 번에 하나의 I/O만 허용 — 읽기 스레드가 블로킹 `ReadFile`로 대기 중이면 같은 핸들에 대한 `WriteFile`이 영구 대기해 서버/클라이언트가 동시에 교착한다(2026-09-05 수정). `Read`/`Write`는 매 연산마다 `OVERLAPPED` 이벤트 + `GetOverlappedResult(TRUE)`로 완료를 기다린다.
- 프레임: `{ magic=0x4A4B0001, type(uint32), length(uint32) } + payload`.
- 메시지 타입:

| MsgType | 방향 | payload | 용도 |
|---------|------|---------|------|
| `Hello` | C→S | `HelloPayload{protocolVersion}` | 연결 첫 메시지 |
| `HelloAck` | S→C | — | 핸드셰이크 완료 |
| `CreateSurface` | C→S | `SurfaceCreatePayload{w,h,title}` | surface 생성 요청 |
| `SurfaceCreated` | S→C | `SurfaceCreatedPayload{surfaceId}` | 할당 결과 (공유메모리 이름 규약 전달) |
| `CommitSurface` | C→S | `CommitSurfaceHeader{surfaceId,dirtyCount}` + `DirtyRect[]` | 프레임 커밋. 픽셀은 공유메모리로 직접 |
| `InputEvent` | S→C | `InputEventPayload` | 키/마우스/휠/IME, surface 로컬 좌표 |
| `TimerEvent` | S→C | — | (예약) |
| `ResizeSurface` | S→C | `SurfaceResizePayload{surfaceId,w,h,shmName[256]}` | 서버 주도 리사이즈(창 테두리 드래그). **새 shm 이름**을 전달 |
| `AudioCommand` | C→S | `AudioCommand` | SFX/BGM 재생 요청 |
| `Close` | C→S | — | 정상 종료 (클라→서버도 동일 타입 사용) |

- 닫기 버튼: 서버가 `Close`를 **S→C로** 보내면 클라 `JKClientSurface::ReadLoop`가 `JKEventType::Quit`로 변환 → 클라가 정상 종료 루프를 타고 C→S `Close` 전송 → 서버 `CleanupDisconnectedClients`가 레이어 제거.

- `InputEventType`: `MouseMove/Down/Up`, `MouseWheel`, `KeyDown/Up`, `Char`, `TextEditing`(IME 조합, `editStart/editLength`는 detail/option 필드 사용).
- 공유 메모리: `Local\JKSurfaceShm_<surfaceId>` (리사이즈 세대마다 `Local\JKSurfaceShm_<surfaceId>_<gen>`), 크기 = `w*h*4` (RGBA32). 서버가 생성(`JKClientConnection::CreateSurface`/`BeginResizeSurface`), 클라이언트가 매핑해 쓴다. Windows 파일 매핑은 크기 확장이 불가하므로 리사이즈마다 새 매핑을 만들고, 구 매핑은 `retiredMemories_`에 클라이언트 연결이 끊길 때까지 보관한다(진행 중인 커밋이 구 버퍼를 참조할 수 있음).

---

## 4. 서버 구성요소

### 4.1 `JKWindowServer` (`server/JKWindowServer.cpp`)

- SDL 창(1280×720, `ALLOW_HIGHDPI`) + 렌더러 생성. **DPI 힌트를 `SDL_Init` 전에 설정**하므로 `SDL_GetWindowSize`=논리 pt / `SDL_GetRendererOutputSize`=물리 px이 보장된다. `SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH=1`도 함께 설정 — SDL 기본값은 **비포커스 창의 활성화 클릭을 버리는데**, 창 서버는 "뒤에 깔린 surface의 타이틀바를 클릭 → 포커스 획득과 동시에 이동 그랩"이 되어야 하므로 활성화 클릭을 전달받아야 한다.
- `UpdateOutputBounds()`: `outputScale = physW/logW`를 계산해 컴포지터에 전파. SIZE_CHANGED/MOVED/DISPLAY_CHANGED마다 갱신.
- `HandleSDLEvent()`: **크롬 그랩 → 클라이언트 surface → 런처 아이콘** 순서로 hit-test. 키/텍스트/휠은 포커스 클라이언트로 전달.
- `Composite()`: 런처 배경 → 컴포지터 순. `SDL_RenderSetScale`은 **1.0 유지**(§6 참고).
- 런처: 서버 자체 그리기(사진 배경 + PNG 아이콘 아트). `icon.rect`는 논리 pt 저장, 그리기/hit-test 시 `× outputScale`로 물리 px 환산. 에셋 규약은 `20_png_assets.md`.
- 스폰 스로틀: 동일 앱 500ms 내 중복 스폰 방지.

### 4.2 `JKClientConnection` (`server/JKClientConnection.cpp`)

- 클라이언트 1개 = transport(named pipe) + shared memory + 읽기 스레드 + 수신 큐.
- `CreateSurface(w,h,title)`: 공유 메모리 생성·영설화, dirty 플래그 설정.
- `BeginResizeSurface(w,h,outPayload)`: 새 세대 shm `..._<gen>` 생성·영설화 → 구 매핑을 `retiredMemories_`로 이동 → width/height 갱신 → `SurfaceResizePayload` 채움.
- `Send()`/`QueueMessage()`/`PopMessage()`: 스레드 간 메시지 큐.

### 4.3 `JKCompositor` (`server/JKCompositor.cpp`)

- 레이어 = 클라이언트 surface 1개. 위치/스케일/알파/dirty 보유 (`JKCompositorLayer`).
- `Composite()`: dirty 레이어 텍스처 업로드(`SDL_UpdateTexture`) → 포커스 레이어가 마지막(최상단)이 되도록 stable_sort → `SDL_RenderCopy` → 각 레이어 우상단에 **닫기 버튼 오버레이**(회색 20×20 + 흰 X, `DrawCloseOverlay`) → `SDL_RenderPresent` 1회.
- `ResizeLayer(id,w,h,pixels)`: layersMutex_ 안에서 구 텍스처 destroy → 신규 `SDL_TEXTUREACCESS_STREAMING` RGBA32 생성 → 크기/픽셀 원자적 교체 + 스케일 리셋. **반드시 `BeginResizeSurface` 이후, `Send(ResizeSurface)` 이전에** 호출(§7 리사이즈 수명주기).
- 드래그 중 프리뷰는 `SetLayerScale`(텍스처 늘리기)로 처리한다. 크기를 먼저 바꾸면(`SetSize`) `SDL_UpdateTexture`가 구 버퍼에 신규 pitch로 접근해 OOB read 위험이 있다.
- 레이어 dst rect는 **논리 pt × outputScale = 물리 px**로 환산해 그린다(§6).
- `HitTest(x,y)`: 물리 px 좌표를 받아 `layer->X()×outputScale`과 비교.
- 주의: `AddLayer`는 layers 락 안에서 `FocusLayer`를 재호출하지 않는다(뮤텍스 재귀 교착 방지) — 포커스 적용은 호출자 책임.

---

## 5. 클라이언트 구성요소

### 5.1 `JKClientApplication` / `JKClientSurface`

- `Init(title, w, h, pipeName)`: HIDDEN SDL 창 1개(IME/이벤트 초기화용) + 파이프 연결 + 공유 메모리 매핑.
- 앱은 기존 `JKApplication` API 그대로 사용하되, 렌더 결과를 offscreen surface(RGBA32)에 기록한다.
- `CommitSurface`: dirty rect 목록과 함께 전송. 서버는 픽셀을 공유메모리에서 직접 읽으므로 픽셀 복사 IPC는 없다.
- 서버에서 온 `InputEventPayload`(surface 로컬 좌표)를 `JKEvent`로 역직렬화해 기존 컨트롤 트리로 라우팅한다.
- **호스트 서비스 (`include/JKApplicationHost.h`)**: 클라이언트 프로세스는 `JKApplication`이 아니므로 `g_currentJKApp`이 null — 그 결과 메시지 박스가 모달 등록도, 렌더도 되지 않는 버그가 있었다(2026-09-05 수정). 두 애플리케이션 호스트가 공통 추상 인터페이스 `JKApplicationHost`(모달/캡처/입력창/윈도우매니저/리소스캐시/오디오)를 구현하고 전역 `g_jkAppHost`에 등록한다. `JKMessageBox`/`JKMenu`/`JKDialog`/`JKFileDialog`/`JKEdit`(IME)/캡처 등 모든 컨트롤은 `g_jkAppHost`만 바라보므로 단일/서버 모드 양쪽에서 동일하게 동작한다. `test`처럼 호스트 없는 모드에서는 null이고 관련 기능은 조용히 생략된다.

### 5.2 오디오 라우팅

- 클라이언트는 SDL_mixer를 소유하지 않는다. `JKSoundManager`의 `g_commandPoster` 콜백이 `surface_->PostAudioCommand`로 IPC 전송하고, 서버의 `JKAudioThread`+`SDLAudioBackend`가 재생한다.
- 서버 모드가 아닐 때(단일 프로세스)는 기존 로컬 오디오 스레드 경로를 그대로 쓴다.

---

## 6. 좌표 모델 (2026-09-05 확정 — 혼합 배율 모니터 대응)

### 6.1 규칙

서버 쪽 그리기·hit-test·마우스는 **전부 물리 클라이언트 px** 좌표계 하나를 쓴다. 논리 pt로 저장된 값(`icon.rect`, `layer->X()/Width()`, `SDL_GetWindowSize` 결과)은 물리 px로 그리거나 비교할 때 `× outputScale`을 곱한다.

```
outputScale = SDL_GetRendererOutputSize().w / SDL_GetWindowSize().w   (=DPI 배율)
물리 px = 논리 pt × outputScale
마우스(mx,my) = Win32 GetCursorPos+ScreenToClient 결과 그대로 (무변환)
클라이언트 전달 x = round(mx / outputScale) − client->X()   (surface 픽셀 좌표)
```

### 6.2 `SDL_RenderSetScale`을 쓰지 않는 이유

처음에는 렌더러 스케일을 `outputScale`로 걸고 논리 pt로 그리는 방식(단일 프로세스 `JKRenderThread`와 동일)을 시도했다. 그러나 (1) 백엔드별 동작 차이 리스크가 있고 (2) 마우스 변환도 같은 배율로 나눠야 해서 결국 이중 관리가 됐다. **모든 좌표를 물리 px로 통일 + 렌더러 스케일 1.0 고정**이 단순하고 백엔드 무관하다. 단일 프로세스 경로는 반대로 씬 좌표계(=SDL_GetWindowSize 공간)에 마우스를 맞춘다 — `14_sdl2_window_dpi.md` §14 참조.

### 6.3 함정

- `GetDpiForWindow/96`은 렌더러 비율과 **대개** 같지만 혼합 배율 모니터에서 어긋날 수 있다. 마우스/렌더 환산엔 **렌더러 비율만** 쓸 것.
- `JKPlatform::GetLogicalMousePos`(DPI 기반 변환)은 이 원인으로 **삭제**됐다. 대신 `GetPhysicalClientMousePos`(raw 물리 px 반환)를 쓰고 호출자가 자기 렌더 비율로 환산한다.
- 컴포지터의 dst/hit-test 환산 배율과 마우스 환산 배율은 **같은 소스**(`compositor_->OutputScale()`)에서 나와야 한다.

---

## 7. 창 프레임(크롬) 모델 — 서버 측 이동/닫기/리사이즈

클라이언트 surface 안의 타이틀바는 **클라이언트가 그린다**(서버엔 텍스트 렌더러가 없음). 서버는 (1) 닫기 버튼만 SDL 오버레이로 그리고 (2) 타이틀/테두리/닫기 영역의 입력을 **클라이언트에 전달하지 않고 가로채** 레이어를 조작한다.

**내부 게임 창은 크롬리스**(`WA_CHROMELESS`, `JKControl.h`): 서버 모드에서 창 관리(이동/닫기/리사이즈)는 전부 서버 크롬의 역할이므로, 클라 앱은 루트 윈도우(타이틀 그림)와 그 안에 고정된 게임 창 2계층으로 구성한다. 클라 앱이 `SetAttrFlags(WA_CHROMELESS)`로 게임 창의 플로팅 속성(`WA_TITLEMOVEABLE|WA_BORDERRESIZABLE`)을 대체한다 — 타이틀바·테두리·닫기 없이 클라이언트 영역 전체를 내용물이 채우고 위치 고정. 단일 모드는 같은 게임 창 클래스를 플로팅 윈도우로 그대로 쓴다.

### 7.1 크롬 영역 (surface 로컬 논리 px, 단일 모드 `JKWindow` 프레임과 동일 상수)

| 영역 | rect | 동작 |
|------|------|------|
| 닫기 | `x∈[W-22, W-2), y∈[2, 22)` | `Close` S→C 전송 → 클라 정상 종료 → disconnect → 레이어 제거 |
| 리사이즈 | 좌/우/하 6px inset (`kResizeHotspot`) | 드래그 중 `SetLayerScale` 늘리기 프리뷰 → MouseUp에 커밋 |
| 타이틀 | `y∈[0, 24)` (위 영역 제외) | 드래그 이동. 이동 중 입력은 클라에 전달되지 않음 |

- 판정 순서: 닫기 → 리사이즈 → 타이틀. 왼쪽 버튼만 크롬으로 처리한다.
- 최소 크기 clamp 64×48 (단일 모드 `JKWindow`와 동일).
- 이동 clamp: 창 밖으로 완전히 나가지 않도록 제한.
- 상단 엣지는 타이틀 드래그 전용(상단 리사이즈 없음) — 클라이언트가 그린 프레임과 정합.
- 크롬 상수는 `JKCompositor.h` (`kChrome*`), 단일 모드는 `JKWindow.cpp` — **동기화 필요**(주석 참조).
- **좌표 변환(2026-09-05)**: `TryChromeGrab`은 마우스(물리 px)→`mx/outputScale`→레이어 원점 차감→**`/ScaleX, /ScaleY`로 surface px**까지 변환한 뒤 판정한다(1:1 레이어는 Scale=1이라 기존과 동일). 닫기 오버레이 **그리기**(`DrawCloseOverlay`)도 같은 surface px 기준으로 레이어 스케일을 곱해 그린다 — 그리기와 히트존이 어긋나면 fit 스케일 레이어에서 닫기가 안 눌린다.
- **스폰 위치 clamp**: fit 레이어가 데스크톱을 정확히 채우면(jango 등) cascade 오프셋(+20/클라) 때문에 닫기 버튼 모서리가 창 밖으로 밀려나 클릭 불가가 된다. `ProcessPendingClients`는 `x,y ≤ 데스크톱−표시크기`로 clamp한다.

### 7.2 리사이즈 커밋 수명주기 (순서가 중요)

```
[테두리 드래그] MouseMove마다 SetLayerScale 프리뷰 (width_ = 텍스처 pitch 유지)
  ↓ MouseUp
(1) BeginResizeSurface(w,h)   : 신규 shm 세대 생성, 구 매핑 retired
(2) ResizeLayer(id,w,h,pixels): 구 텍스처 파괴 → 신규 텍스처 생성 → 원자적 교체
(3) Send(ResizeSurface)       : 클라에 {w,h,신규 shm 이름} 통지
  ↓
[클라] ReadLoop가 ResizeSurface 수신 → pendingResize_ 저장(최신만, coalesce)
  + SizeChanged 이벤트 큐잉
  ↓ 메인 루프 (DrainInputChannel → ProcessOneEvent)
ApplyPendingResize(): 임시 JKSharedMemory에 Open **성공 후** 교체
  (같은 객체 Close→Open은 실패 시 stale 상태가 되므로 금지)
  → logicalWidth/Height 갱신 → mainWindow SetWindowRect → 도크 레이아웃 재계산
  ↓ 이후 커밋은 신규 shm으로
```

- (2)를 (1)보다 먼저 하거나 (3)을 (2)보다 먼저 하면 `SDL_UpdateTexture`가 구 픽셀 버퍼에 신규 pitch로 접근해 OOB read/crash.
- 인정된 artifact: 커밋 직후 클라가 remap하기 전까지 1프레임 빈 화면(구 shm에 커밋 → 신규 shm은 영).
- 클라가 remap 전에 계속 구 shm에 커밋해도 안전 — 구 매핑은 `retiredMemories_`가 살려두고, 컴포지터는 이미 신규 텍스처(영 버퍼)를 가리킨다.

### 7.3 과대 surface fit 스케일 (2026-09-05 추가)

FHD(1920×1080) 레이아웃의 앱(jango/occ/pcx/vector/iconedit/recog/vfont/vpres)은 절대 좌표 배치라 surface 크기를 줄일 수 없다. 대신 **서버가 표시 크기만 축소**한다 — 클라는 설계 크기 그대로 렌더링.

- `ProcessPendingClients`: surface가 데스크톱(논리 1280×720)보다 크면 균일 fit 스케일 `min(ww/W, wh/H)`를 `SetLayerScale`로 적용하고, 표시 크기 기준으로 중앙 배치.
- 입력 매핑: surface 로컬 좌표 = `((mx/outputScale) − X) / layerScale` — 레이어 스케일을 반영해 클라 surface 픽셀 공간으로 역변환. MouseMove의 dx/dy도 스케일로 나눈다.
- 크롬 영역은 surface 로컬 px 기준이라 비례 축소된다(타이틀 24 → 16 논리 px 등) — 판정식은 그대로 동작.
- 이동 그랩 오프셋(`chromeGrabDX_/DY_`)은 **논리 pt**로 저장(`lx × ScaleX`) — surface 좌표와 논리 좌표가 어긋나는 것을 방지.
- 리사이즈: 프리뷰/커밋 판정은 **표시(논리) 크기** 기준(`chromeResizeW_/H_` = grab 시점 표시 크기). 커밋 surface 크기 = 표시 목표 ÷ grab 시점 스케일 — fit 스케일 앱은 레이아웃이 잘리지 않고 표시 배율이 유지되며, 1:1 레이어는 기존과 동일(surface == 표시). `CommitChromeResize`가 `ResizeLayer`(스케일 1 리셋) 후 표시 크기에 맞는 스케일을 재적용한다.

---

## 8. 생명주기

```
[아이콘 클릭] → SpawnClient(appName): CreateProcess(exe, "--client <app>")
  ↓
[클라이언트] Hello → CreateSurface(w,h,title)
  ↓
[서버 acceptor] HelloAck → SurfaceCreated{surfaceId} → 공유메모리 생성
  ↓
[서버 Run 루프] pendingClients_ 인입 → 중앙 배치(기존 클라이언트 수×20 오프셋)
  → compositor AddLayer/SetLayerPosition/FocusLayer
  ↓
[실행 중] CommitSurface → dirty 텍스처 업로드 → Composite / InputEvent ←→ AudioCommand
  ↓
[종료] Close 메시지 또는 파이프 끊김 → RemoveLayer → read 스레드 join → clientsMutex_ 밖 정리
```

- 포커스: MouseDown으로 hit된 클라이언트를 `FocusClient` → 컴포지터가 최상단으로 정렬. 휠/키/텍스트는 포커스 클라이언트로만 전달.
- 마우스 캡처(2026-09-05 추가): MouseDown이 클라이언트 안에서 발생하면 서버가 `capturedClientId_`를 기록하고, MouseUp까지는 **커서가 surface 밖에 있어도** MouseMove/MouseUp을 그 클라이언트로 계속 전달한다(Win32 SetCapture 상당). 영역 밖 좌표는 음수/초과 값으로 그대로 간다 — 클라이언트는 단일 프로세스 경로와 같은 캡처 로직(JKButton/JKEdit)을 돌리므로 자체 처리한다. 캡처 중 클라이언트가 끊기면 `CleanupDisconnectedClients`가 `capturedClientId_`를 해제한다.

---

## 9. 검증 방법

1. 서버: `jkdesktop.exe --server` → 런처 데스크톱 표시.
2. 아이콘 클릭 → 클라이언트 프로세스 스폰 → 서버 창 안에 지뢰찾기 surface 합성 확인.
3. 지뢰찾기 셀 클릭/드래그가 보이는 위치와 정확히 일치하는지 확인(혼합 배율 모니터 포함).
4. 두 번째 클라이언트(tetris) 스폰 → z-order/포커스 전환 확인.
5. 클라이언트를 작업 관리자로 강제 종료 → 서버 surface 정리·크래시 없음 확인.
6. 셀프테스트: `jkdesktop.exe test` → `AppSelfTest: 0 failure(s)`.
7. **크롬 스모크(2026-09-05 통과)**: 지뢰찾기+테트리스+지뢰찾기 3개 동시 스폰(cascade 배치) → 타이틀 드래그 3회(grabDX/DY·최종 위치 정확) → 우측 엣지 리사이즈(320→410) → clamp(64×48 하한) → 복귀 → BR 코너(460×410) → 바닥 엣지 → 좌측 엣지 → 닫기 버튼(레이어 제거·클라 정상 종료) → 재스폰(acceptor 재무장, 총 4연결·shm 세대 6 churn) — crash/오류 없음. 스크립트: `tmp/jk_click.ps1`(커서 검증 합성 입력), `tmp/jk_under.ps1`(WindowFromPoint 확인).
8. **대량 동시 강제종료(2026-09-05 수정 후 통과)**: 클라이언트 3개를 `taskkill /F`로 **동시에** 죽이면 서버가 접근위반으로 크래시했다. 원인 2개 — (1) `JKCompositor::RemoveLayer`의 `remove_if`+`(*it)->Texture()` 오용: remove_if가 unique_ptr을 앞으로 move시켜 꼬리에 널을 남기는데, 제거 대상이 벡터 마지막 요소가 아니면(=포커스되지 않은 클라이언트) 그 널을 역참조했다. 지우기-루프로 교체. (2) `StopReadThread`가 read 스레드가 in-flight overlapped I/O를 가진 채 핸들을 닫았다(UB) — `CancelIoEx`로 깨운 뒤 join하고 나서 Close하도록 순서 변경(서버/클라이언트 양쪽), Read/Write 실패 경로는 핸들을 닫지 않고 `connected_=false`만 설정. 5라운드 × 3클라 동시 종료 재현 스크립트(`tmp/repro_loop.ps1`)로 서버 생존 확인.
9. **fit 스케일 앱 스모크(2026-09-05 통과)**: `--client jango/occ/vector` 스폰 → 1920×1080 surface가 1280×720 데스크톱에 0.667 fit 스케일로 중앙(clamp) 배치 → 축소 좌표에서 Personnel 클릭이 surface (175,85)로 정확 매핑(비밀번호 다이얼로그 오픈) → 닫기 오버레이 클릭(논리 1272,8) → 클라 정상 종료. 테트리스 화살표 키: 서버 키 전달 → root focusChild_ 체인(ClientTetrisApp의 gameWin SetFocus) 확인.

## 10. 알려진 제약 / Phase 3 남은 작업

- 출력 1개(서버 SDL 창)만 지원 — 멀티 디스플레이/워크스페이스는 미구현.
- `TimerEvent` 메시지는 예약만 된 상태. 클라이언트 타이머는 자체 스레드 사용.
- `spanDisplays`, `SurfaceHints`, `DisplayChanged` 등은 계획 문서(Phase 3) 단계.
- 크롬: 이동/리사이즈 중 커서 모양 변경 없음. 상단 엣지 리사이즈 없음(타이틀 드래그 전용). 닫기 오버레이는 텍스트 없는 X 아이콘.
- 리사이즈 커밋 시 1프레임 빈 화면 (§8 인정 artifact).