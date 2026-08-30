# JKWINDOW → SDL2 매핑 설계

> JKENGINE의 자체 GUI 프레임워크(JKWINDOW)를 SDL2 위에 최소한으로 재현하기 위한 설계 문서.
> 본 문서는 **Prototype 1단계**에서 구현할 핵심 클래스만 다룹니다.

## TL;DR

- **목표**: `JKControl` → `JKWindow`/`JKApplication` 트리, 메시지 루프, 기본 DC 그리기를 SDL2로 재현.
- **핵심 매핑**: `SDL_PollEvent` → `JKEvent` → `JKMessageQue.Push()` → `JKApplication.Run()` → target `RespondMessage()`.
- **Out of scope**: VESA256, 프린터/메타파일 DC, 복잡한 EntryControl/RecordViewBase.
- **빠른 찾기**: 클래스 계층 §2, JKControl 매핑 §3.1, JKWindow 매핑 §3.2, 이벤트 매핑 §4.

---

## 1. 목표와 범위

### 목표

- JKWINDOW의 핵심 클래스 구조(`JKControl` → `JKWindow`/`JKApplication`)를 SDL2로 재현.
- 메시지 루프, 이벤트 라우팅, 기본 그리기(Device Context) 흐름을 동작시킴.
- 추후 SDL3 마이그레이션을 염두에 둔 lean 한 SDL2 사용.

### 1단계 프로토타입 범위 (Out of Scope 명시)

| 포함 | 제외 |
|------|------|
| JKApplication, JKWindow, JKControl, JKMessageQue, JKDC, JKEvent | JKDialog 모달 루프 |
| 마우스/키보드/QUIT 이벤트 라우팅 | TimerManager, ResourceManager |
| 단일 SDL 창 기반 윈도우 그리기 | VESA 256 팔레트 모드 직접 제어 |
| 단순 사각형/텍스트 그리기 | 한글 출력, 프린터, 메타파일 |
| 부모-자식 컨트롤 트리 | 복잡한 EntryControl/RecordViewBase |

---

## 2. 아키텍처 개요

```
JKApplication
    └── owns JKWindow (root)
            └── owns JKControl[] children
                    └── recursive children

Event Loop (SDL_PollEvent)
    └── SDL event → JKEvent 변환
            └── JKMessageQue.Push()
                    └── JKApplication.Run() → Dispatch to target control

Painting
    └── SDL_Renderer Clear
            └── JKWindow.PaintWindow()
                    └── foreach child: JKControl.PaintClient()
            └── SDL_Renderer Present
```

---

## 3. 클래스 매핑

### 3.1 JKControl → 모든 UI의 기반

| JKControl 멤버/함수 | SDL2 Prototype 매핑 |
|-------------------|----------------------|
| `ControlID` | `uint16_t id` |
| `WinID` | `uint32_t id` (UUID 카운터) |
| `Parent*` | `JKControl* parent` |
| `prev/next` (DLLFrame) | `std::vector<std::unique_ptr<JKControl>> children` |
| `WinRect` | `JKRect rect` (부모 기준 좌표) |
| `ClientRect` | `JKRect clientRect` |
| `IsShow` | `bool visible` |
| `WinAttr` | `uint32_t attrFlags` |
| `WinText` | `std::string text` |
| `InitWindow()` | `Init()` 가상 함수 |
| `SetupWindow()` | `Setup()` 가상 함수 |
| `OpenWindow()` | `Open()` |
| `CloseWindow()` | `Close()` |
| `ShowWindow()`/`HideWindow()` | `Show()`/`Hide()` |
| `PaintWindow(dc, rect)` | `PaintWindow(JKDC& dc)` |
| `PaintClient(dc, rect)` | `PaintClient(JKDC& dc)` |
| `RespondMessage(msg)` | `RespondMessage(const JKEvent& ev)` |

### 3.2 JKWindow

| JKWindow 기능 | SDL2 Prototype 매핑 |
|---------------|----------------------|
| 전체 화면/창 좌표 | SDL_Window (1개) 기준, 논리적 좌표는 `JKRect` |
| 타이틀/테두리 | `SDL_SetWindowTitle` + `PaintWindow()`에서 직접 그림 |
| 배경색 | `JKDC::FillRect()` |
| 자식 컨트롤 추가 | `AddControl(JKControl*)` |
| 모달 루프 | 1단계에서 제외 |

### 3.3 JKApplication

| JKApplication 기능 | SDL2 Prototype 매핑 |
|--------------------|----------------------|
| 전체 화면 크기 컨트롤 | `JKWindow* mainWindow` |
| `InitApplication()` | `Init()` |
| `Run()` 메시지 루프 | `while(running_) { PollEvent; Update; Render; Delay; }` |
| `CloseApplication()` | `Close()` |

### 3.4 MessageQue / EventHandler

| JKWINDOW 구성 | SDL2 Prototype 매핑 |
|---------------|----------------------|
| `JKMSG` (WORD main, DWORD sub...) | `JKEvent` enum + union |
| `MessageQue` | `std::deque<JKEvent>` |
| `EventHandler::Run()` | `JKApplication::Run()` |
| `PatchMessage()` | `SDL_PollEvent` 직접 수집 |
| `TranslateMessage()` | SDL 좌표 → JKControl 로컬 좌표 변환 |
| `PreProcessMessage()` | APPEXIT 등 전역 메시지 처리 |

### 3.5 JKDC (Device Context)

| JKDC 기능 | SDL2 Prototype 매핑 |
|-----------|----------------------|
| 그리기 대상 추상화 | `SDL_Renderer*` 래퍼 |
| `MoveTo`/`LineTo` | `SDL_RenderDrawLine` |
| `Rectangle` | `SDL_RenderDrawRect` / `FillRect` |
| `SetColor` | `SDL_SetRenderDrawColor` |
| `TextOut` | SDL2_ttf (향후) / 1단계는 TTF 없이 생략 |
| 화면 DC / 메모리 DC | 1단계에서는 모두 동일 `SDL_Renderer` 사용 |

---

## 4. JKEvent 설계

```cpp
enum class JKEventType : uint16_t {
    None,
    Quit,
    MouseMove,
    MouseDown,
    MouseUp,
    KeyDown,
    KeyUp,
    Char,
    Paint,
    Timer,
    Command,
    User
};

struct JKEvent {
    JKEventType type;
    uint32_t    targetId; // 대상 JKControl WinID
    int32_t     x, y;     // 로컬 좌표 (Mouse)
    int32_t     dx, dy;
    uint32_t    keyCode;  // SDL_Keycode 또는 char
    uint32_t    detail;
    uint32_t    option;
};
```

---

## 5. 메시지 루프 흐름

```cpp
bool JKApplication::Run() {
    SDL_Event sdlEvent;
    while (running_) {
        // 1. SDL 이벤트 수집 → JKEvent 변환 → MessageQue
        while (SDL_PollEvent(&sdlEvent)) {
            JKEvent ev = TranslateSDLEvent(sdlEvent);
            if (ev.type != JKEventType::None) {
                msgQue_.Push(ev);
            }
        }

        // 2. 메시지 처리
        while (msgQue_.Pop(ev)) {
            if (!PreProcessMessage(ev)) { running_ = false; break; }
            RouteMessage(ev);
        }

        // 3. 전체 그리기
        Render();

        // 4. ~60 FPS
        SDL_Delay(16);
    }
    return true;
}
```

### 이벤트 라우팅

```cpp
void JKApplication::RouteMessage(const JKEvent& ev) {
    JKControl* target = FindControlById(ev.targetId);
    if (!target) target = mainWindow_;
    target->RespondMessage(ev);
}
```

마우스 이벤트의 경우, SDL 스크린 좌표 → 루트 윈도우 좌표 → 컨트롤 트리 hit-test로 targetId 결정.

---

## 6. 그리기 흐름

```cpp
void JKApplication::Render() {
    dc_.SetColor(192, 192, 192, 255); // desktop gray
    dc_.Clear();

    mainWindow_->PaintWindow(dc_);
    mainWindow_->PaintClient(dc_);

    dc_.Present();
}

void JKWindow::PaintWindow(JKDC& dc) {
    // 타이틀 바, 테두리 등 비클라이언트 영역
    dc.SetColor(0, 0, 128, 255); // classic blue title bar
    dc.FillRect(GetWindowRect());
}

void JKWindow::PaintClient(JKDC& dc) {
    dc.SetColor(240, 240, 240, 255); // client background
    dc.FillRect(GetClientRect());

    for (auto& child : children_) {
        if (child->IsVisible()) {
            child->PaintClient(dc);
        }
    }
}
```

---

## 7. 좌표 체계

| 개념 | 설명 |
|------|------|
| Screen Coord | SDL_Window 기준 전체 좌표 |
| Window Coord | JKWindow 클라이언트 기준 좌표 |
| Control Coord | 부모 JKControl 클라이언트 기준 좌표 |
| `JKRect` | `{ int32_t x, y, w, h; }` |

hit-test 함수:

```cpp
JKControl* JKWindow::HitTest(int32_t x, int32_t y) {
    // 마지막에 추가된 자식이 최상위 (z-order)
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        if ((*it)->IsVisible() && (*it)->GetRect().Contains(x, y)) {
            return it->get();
        }
    }
    return this;
}
```

> DPI 스케일링(논리 pt ↔ 물리 px), 레터박스, 마우스 좌표 변환의 실제 구현은 `14_sdl2_window_dpi.md`를 참고한다.

---

## 8. JKRect / JKPoint 타입

```cpp
struct JKPoint {
    int32_t x = 0, y = 0;
};

struct JKRect {
    int32_t x = 0, y = 0, w = 0, h = 0;

    bool Contains(int32_t px, int32_t py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }

    SDL_Rect ToSDL() const { return SDL_Rect{ x, y, w, h }; }
};
```

---

## 9. Prototype 파일 구조

```
prototype/sdl2_jkwindow/
├── CMakeLists.txt
├── include/
│   ├── JKTypes.h
│   ├── JKEvent.h
│   ├── JKMessageQue.h
│   ├── JKDC.h
│   ├── JKControl.h
│   ├── JKWindow.h
│   └── JKApplication.h
└── src/
    ├── JKEvent.cpp
    ├── JKMessageQue.cpp
    ├── JKDC.cpp
    ├── JKControl.cpp
    ├── JKWindow.cpp
    ├── JKApplication.cpp
    └── main.cpp
```

---

## 10. 향후 확장 시 SDL2 → SDL3 마이그레이션 포인트

| SDL2 | SDL3 |
|------|------|
| `SDL_bool` | `bool` |
| `SDL_CreateWindow(title, x, y, w, h, flags)` | `SDL_CreateWindow(title, w, h, flags)` |
| `SDL_WINDOWPOS_CENTERED` | 제거, 플래그로 대체 |
| `SDL_Keycode`/`SDL_Scancode` | 동일 개념, 일부 이름 변경 |
| `SDL_Renderer` API | 거의 동일 |
| `SDL_PollEvent` | 거의 동일 |
| `SDL_INIT_VIDEO` | 거의 동일 |

핵심 GUI 클래스(JKControl/JKWindow/JKApplication/JKDC)는 그대로 유지될 예정.

---

## 11. 확인 체크리스트 (Step 2 → Step 3 전)

- [ ] JKControl/JKWindow/JKApplication 계층 설계 완료
- [ ] JKEvent ↔ SDL_Event 매핑 확정
- [ ] JKDC → SDL_Renderer 매핑 확정
- [ ] 메시지 루프 흐름 확정
- [ ] 좌표 체계(hit-test, local) 확정
- [ ] Prototype 파일 구조 확정

