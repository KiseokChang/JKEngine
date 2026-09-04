# SDL2 윈도우 서버 (Phase 2) 아키텍처

> `prototype/sdl2_jkwindow`의 프로세스 분리(window server) 구조를 정리한 문서.
> 2026-09 Phase 2 구현(`570c802`, `a6d383e`)과 2026-09-05 좌표계 통일 수정을 반영한다.
> 좌표계/DPI의 근본 규칙은 `14_sdl2_window_dpi.md` 참조.

## TL;DR

- 런처는 **윈도우 서버 프로세스**(`--server`)로, 게임(지뢰찾기/테트리스)은 **클라이언트 프로세스**(`--client <app>`)로 분리된다.
- 클라이언트는 SDL 윈도우를 띄우지 않고 **공유 메모리 RGBA surface**에 그린 뒤 named pipe로 `CommitSurface`를 보낸다. 서버는 이를 SDL 텍스처로 합성(compositing)한다.
- 입력은 서버가 hit-test해서 **surface 로컬 좌표**로 변환해 클라이언트에 전달한다. 오디오도 서버가 유일한 SDL_mixer 인스턴스로 대행 재생한다.
- **좌표 모델(2026-09-05 확정)**: 서버 쪽 모든 그리기·hit-test·마우스를 **물리 픽셀(px)** 하나로 통일한다. 논리 pt ↔ 물리 px 환산은 렌더러 비율(`outputScale = physW/logW`)로만 한다. `GetDpiForWindow/96` 같은 DPI 기반 환산을 섞으면 혼합 배율 모니터에서 어긋난다.
- **빠른 찾기**: 프로세스 구조 §2, 와이어 프로토콜 §3, 서버 구성요소 §4, 클라이언트 §5, 좌표 모델 §6, 생명주기 §7, 검증 §8.

---

## 1. 두 렌더 경로

| 경로 | 실행 방법 | 창 소유 | 사용 파일 |
|------|----------|---------|----------|
| 단일 프로세스 (스레드 분리) | `jkproto_sdl2_jkwindow.exe minesweeper` 등 | `JKRenderThread` (앱 전용 SDL 창) | `JKApplication`, `JKRenderThread` |
| 윈도우 서버 (Phase 2) | `--server` (서버) / `--client minesweeper\|tetris` (클라이언트) | 서버가 유일한 SDL 창 소유 | `server/JKWindowServer`, `client/JKClientApplication` |

단일 프로세스 경로의 좌표계·마우스 규칙은 `14_sdl2_window_dpi.md` §13-§14 참조.

---

## 2. 프로세스·스레드 구조

```
[서버 프로세스: jkproto_sdl2_jkwindow.exe --server]
  main 스레드      : SDL_PollEvent → HandleSDLEvent → Composite 루프 (Run)
  acceptor 스레드  : 파이프 대기 → Hello/CreateSurface 핸드셰이크 → pendingClients_
  read 스레드 x N  : 클라이언트별 파이프 읽기 → QueueMessage
  audio 스레드     : JKAudioThread (서버가 유일한 SDL_mixer 소유)

[클라이언트 프로세스: jkproto_sdl2_jkwindow.exe --client minesweeper]
  main 스레드      : 앱 로직 → offscreen surface 렌더 → CommitSurface
  (SDL 윈도우 없음 — SDL은 이벤트/IME 초기화용으로만 HIDDEN 창 1개)
```

- 서버는 클라이언트 스폰 시 `CreateProcess`로 **자기 exe를 재실행**하며 작업 디렉터리를 exe 위치로 맞춘다(에셋 로딩 보장).
- 클라이언트 크래시 감지: 파이프 끊김(`IsDisconnected`) → surface 정리 + read 스레드 join (`CleanupDisconnectedClients`). join은 `clientsMutex_` **밖에서** 수행(교착 방지).

---

## 3. 와이어 프로토콜 (`ipc/JKWireProtocol.h`)

- 전송: Windows named pipe `\\.\pipe\JKWindowServerPipe` (`JKPipeTransport`).
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
| `AudioCommand` | C→S | `AudioCommand` | SFX/BGM 재생 요청 |
| `Close` | C→S | — | 정상 종료 |

- `InputEventType`: `MouseMove/Down/Up`, `MouseWheel`, `KeyDown/Up`, `Char`, `TextEditing`(IME 조합, `editStart/editLength`는 detail/option 필드 사용).
- 공유 메모리: `Local\JKSurfaceShm_<surfaceId>`, 크기 = `w*h*4` (RGBA32). 서버가 생성(`JKClientConnection::CreateSurface`), 클라이언트가 매핑해 쓴다.

---

## 4. 서버 구성요소

### 4.1 `JKWindowServer` (`server/JKWindowServer.cpp`)

- SDL 창(1280×720, `ALLOW_HIGHDPI`) + 렌더러 생성. **DPI 힌트를 `SDL_Init` 전에 설정**하므로 `SDL_GetWindowSize`=논리 pt / `SDL_GetRendererOutputSize`=물리 px이 보장된다.
- `UpdateOutputBounds()`: `outputScale = physW/logW`를 계산해 컴포지터에 전파. SIZE_CHANGED/MOVED/DISPLAY_CHANGED마다 갱신.
- `HandleSDLEvent()`: 마우스 hit-test(클라이언트 surface 우선 → 런처 아이콘), 키/텍스트는 포커스 클라이언트로 전달.
- `Composite()`: 런처 배경 → 컴포지터 순. `SDL_RenderSetScale`은 **1.0 유지**(§6 참고).
- 런처: 서버 자체 그리기(회색 데스크톱 + 아이콘 rect). `icon.rect`는 논리 pt 저장, 그리기/hit-test 시 `× outputScale`로 물리 px 환산.
- 스폰 스로틀: 동일 앱 500ms 내 중복 스폰 방지.

### 4.2 `JKClientConnection` (`server/JKClientConnection.cpp`)

- 클라이언트 1개 = transport(named pipe) + shared memory + 읽기 스레드 + 수신 큐.
- `CreateSurface(w,h,title)`: 공유 메모리 생성·영설화, dirty 플래그 설정.
- `Send()`/`QueueMessage()`/`PopMessage()`: 스레드 간 메시지 큐.

### 4.3 `JKCompositor` (`server/JKCompositor.cpp`)

- 레이어 = 클라이언트 surface 1개. 위치/스케일/알파/dirty 보유 (`JKCompositorLayer`).
- `Composite()`: dirty 레이어 텍스처 업로드(`SDL_UpdateTexture`) → 포커스 레이어가 마지막(최상단)이 되도록 stable_sort → `SDL_RenderCopy` → `SDL_RenderPresent` 1회.
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

## 7. 생명주기

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

---

## 8. 검증 방법

1. 서버: `jkproto_sdl2_jkwindow.exe --server` → 런처 데스크톱 표시.
2. 아이콘 클릭 → 클라이언트 프로세스 스폰 → 서버 창 안에 지뢰찾기 surface 합성 확인.
3. 지뢰찾기 셀 클릭/드래그가 보이는 위치와 정확히 일치하는지 확인(혼합 배율 모니터 포함).
4. 두 번째 클라이언트(tetris) 스폰 → z-order/포커스 전환 확인.
5. 클라이언트를 작업 관리자로 강제 종료 → 서버 surface 정리·크래시 없음 확인.
6. 셀프테스트: `jkproto_sdl2_jkwindow.exe test` → `AppSelfTest: 0 failure(s)`.

## 9. 알려진 제약 / Phase 3 남은 작업

- 출력 1개(서버 SDL 창)만 지원 — 멀티 디스플레이/워크스페이스는 미구현.
- `TimerEvent` 메시지는 예약만 된 상태. 클라이언트 타이머는 자체 스레드 사용.
- `spanDisplays`, `SurfaceHints`, `DisplayChanged` 등은 계획 문서(Phase 3) 단계.
- 클라이언트 surface 크기와 서버 배율이 다를 때의 재협상(`SurfacePlacement`) 미구현.