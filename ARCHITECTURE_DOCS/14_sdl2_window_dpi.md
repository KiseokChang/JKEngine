# SDL2 윈도우 배치·DPI·좌표계 아키텍처

> `prototype/sdl2_jkwindow`의 창 생성/배치, DPI 스케일링, 좌표계, 렌더링 파이프라인의 실제 구현을 정리한 문서.
> 2026-08 창 상단 잘림(title bar clipping) 수정 작업에서 확정된 내용을 반영한다.

---

## 1. 문제와 해결 요약

**증상**: Windows DPI 스케일링(예: 125%) 환경에서 창이 화면 상단으로 밀려나 타이틀 바가 잘리고, 창 크기에 따라 앱 내용이 왜곡됨.

**원인**: SDL2 API가 서로 다른 단위(논리 포인트 vs 물리 픽셀)와 기준점(클라이언트 영역 vs 프레임 전체)을 사용하는데, 이를 섞어 계산하면 배치 오차가 발생.

**해결** (`JKApplication`에 구현):

1. **프레임 중앙 배치 + 클라이언트 원점 보정** — 장식(타이틀 바/테두리) 포함 창 전체가 작업 영역 중앙에 오도록 위치를 계산하고, `SDL_SetWindowPosition()`이 클라이언트 원점 기준임을 보정한다.
2. **앱 논리 좌표계 고정 + 등비(레터박스) 스케일링** — 앱 레이아웃은 `Init(width, height)` 요청 크기(예: 1920×1080)로 고정하고, 렌더링/입력만 화면에 맞춰 스케일한다.
3. **좌표 변환 공식 단일화** — DPI 배율(`ptToPhys`), 등비 배율(`fit`), 레터박스 여백(`letterbox`) 세 값만으로 모든 계층 변환을 수행한다.

---

## 2. 좌표계 계층

| 계층 | 단위 | 획득 방법 | 사용처 |
|------|------|-----------|--------|
| 앱 논리 좌표계 | `Init()` 요청 크기 고정 (`logicalWidth_/logicalHeight_`, 예: 1920×1080) | `JKApplication` 멤버 | 컨트롤 레이아웃, hit-test, 드래그, 그리기 |
| 창 클라이언트 좌표 | 논리 포인트(pt) | `SDL_GetWindowSize()` | 창 크기, SDL 마우스 이벤트 좌표 |
| 렌더러 출력 좌표 | 물리 픽셀(px) | `renderBackend_->GetOutputSize()` (`SDL_GetRendererOutputSize()`) | 실제 출력 해상도, DPI 배율 계산 |
| 화면·작업 영역 좌표 | 논리 포인트(pt) | `SDL_GetDisplayUsableBounds()` | 창 배치(작업표시줄 제외 영역) |

변환 관계:

```
물리 px      = 논리 pt × DPI 배율 (ptToPhys = renderW / windowW)
앱 논리 좌표 = (물리 px − 레터박스 여백) / 등비 배율 (fit)
```

---

## 3. SDL2 API 단위/원점 요약 (핵심 함정)

| API | 단위 | 기준점 | 주의 |
|-----|------|--------|------|
| `SDL_GetWindowSize()` | 논리 pt | 클라이언트 영역 크기 | `SDL_HINT_WINDOWS_DPI_SCALING=1` 전제 |
| `SDL_GetRendererOutputSize()` | 물리 px | 렌더러 출력 | 창 크기(논리)와 값이 다름 (HiDPI) |
| `SDL_GetWindowBordersSize()` | 물리 px | top/left/bottom/right 장식 두께 | 창이 표시된 후에만 유효, 실패 시 음수 반환 |
| `SDL_GetDisplayUsableBounds()` | 논리 pt | 데스크톱 전역 작업 영역 | 디스플레이 인덱스 0 사용 |
| `SDL_SetWindowPosition()` | 논리 pt | **클라이언트 영역 원점** | 프레임 좌상단이 아님 → 상단/좌측 장식 보정 필요 |
| `SDL_SetWindowSize()` | 논리 pt | — | pt/px 해석이 일관되지 않아 **생성 시점 크기로만** 조정 |

---

## 4. 창 생성·배치 흐름 (`JKApplication::Init`)

```
① DPI 힌트 설정 (SDL_Init 호출 전)
② 앱 논리 좌표계 고정 (logicalWidth_/logicalHeight_)
③ 생성 크기 클램핑 (작업 영역 − 추정 장식)
④ SDL_CreateWindow (CENTERED, ALLOW_HIGHDPI)
⑤ SDL_CreateRenderer (ACCELERATED | PRESENTVSYNC)
⑥ 재배치 블록 (프레임 중앙 배치 + 클라이언트 원점 보정)
⑦ DC/리소스캐시/한글매니저/메인 윈도우 초기화
⑧ UpdateScale() → back buffer → mainWindow Init/Setup/Open
```

### 4.1 DPI 힌트 (SDL_Init 전에 설정)

```cpp
SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
```

- per-monitor V2 DPI 인식 선언 + 논리 포인트 좌표 사용.
- OS 비트맵 스케일링(흐림) 비활성화, `SDL_GetWindowSize()`=논리 pt / `SDL_GetRendererOutputSize()`=물리 px이 보장됨.

### 4.2 생성 크기 클램핑

창 생성 전에는 실제 장식 크기를 알 수 없으므로 추정치로 맞춘다: 좌/우 테두리 각 4pt, 상단(타이틀 바+테두리) 31pt, 하단 4pt. 클램프 하한 320×240pt.

### 4.3 재배치 블록 (렌더러 생성 후에만 가능)

렌더러가 있어야 물리 픽셀 크기를 알 수 있으므로 재배치는 렌더러 생성 후에 실행한다:

```cpp
int top, left, bottom, right;
if (SDL_GetWindowBordersSize(window_, &top, &left, &bottom, &right) != 0 || /* 음수 */ ...) {
    top = 31; left = 4; bottom = 4; right = 4;   // 폴백 장식값
}
int w, h;            SDL_GetWindowSize(window_, &w, &h);          // 논리 pt
int renderW, renderH; renderBackend_->GetOutputSize(renderW, renderH); // 물리 px
const float dpiScale = renderW / static_cast<float>(w);           // DPI 배율

// 프레임 전체 크기(논리 pt) = 창 크기 + 장식(물리 px) / dpiScale
const int frameW = w + static_cast<int>((left + right) / dpiScale + 0.5f);
const int frameH = h + static_cast<int>((top + bottom) / dpiScale + 0.5f);
const int outerX = usable.x + (usable.w - frameW) / 2;
const int outerY = usable.y + (usable.h - frameH) / 2;

// SDL_SetWindowPosition은 클라이언트 원점 기준 → 좌상단 장식 두께만큼 더한다
SDL_SetWindowPosition(window_,
    outerX + static_cast<int>(left / dpiScale + 0.5f),
    outerY + static_cast<int>(top  / dpiScale + 0.5f));
```

---

## 5. 스케일·레터박스 계산 (`UpdateScale`)

호출 시점: `Init()` 완료 시, 그리고 `SDL_WINDOWEVENT_SIZE_CHANGED`마다.

```
appW×appH   = logicalWidth_×logicalHeight_      (Init 요청값, 고정)
fit         = min(renderW/appW, renderH/appH)   (등비 배율)
scaleX_ = scaleY_ = fit
letterboxX  = round((renderW − appW·fit) / 2)    (좌측 여백)
letterboxY  = round((renderH − appH·fit) / 2)    (상단 여백)
ptToPhysX   = renderW / windowW                 (DPI 배율, 마우스 변환용)
mainWindow_->SetWindowRect({0, 0, appW, appH})
→ CreateOrResizeBackBuffer() (출력 크기 텍스처 재생성)
```

**설계 의도**: 창이 화면 작업 영역보다 작아져도 앱 좌표계·레이아웃은 고정하고 렌더링/입력만 스케일한다. 앱 코드(레이아웃, hit-test, 드래그)는 화면 크기와 무관하게 1920×1080 등 고정 좌표로 작성된다.

---

## 6. 렌더링 파이프라인 (`Render`)

2단계 구성:

1. **백버퍼에 앱 논리 좌표로 그리기** — 렌더 타깃을 백버퍼(출력 크기 텍스처)로 지정하고 `SetScale(fit)`을 적용한 뒤, 데스크톱 배경 `(192,192,192)` clear → `mainWindow_` PaintWindow/PaintClient → `modalWindow_`가 있으면 추가로 그린다.
2. **기본 타깃에 blit** — scale을 1.0으로 되돌리고 전체 화면을 192 회색으로 clear(레터박스 여백)한 뒤, 백버퍼의 `(0, 0, appW·fit, appH·fit)` 영역을 `(letterboxX, letterboxY)` 위치에 blit → `Present()`.

그리기 코드는 앱 논리 좌표를 그대로 사용하고, 물리 픽셀 정확도는 blit 단계가 담당한다.

---

## 7. 입력 좌표 변환 (`TranslateSDLEvent`)

SDL 마우스 좌표는 창 좌표(논리 pt)로 들어온다(`DPI_SCALING=1` 전제). 앱 논리 좌표로의 변환:

```
physX = ev.x × ptToPhysX − letterboxX     // 창 논리 pt → 물리 px − 레터박스 여백
appX  = round(physX / fit)                 // → 앱 논리 좌표
(dx, dy)도 dx × ptToPhys / fit 로 변환      // 창 이동/리사이즈용 상대 이동량
```

- 마우스 캡처 중에는 `MouseMove`/`MouseUp`을 캡처한 컨트롤(`captureControl_`)에 그대로 전달한다.
- hit-test는 앱 논리 좌표로 수행하며, 대상 윈도우는 `modalWindow_ ? modalWindow_ : mainWindow_`.
- `Tab`/`Shift+Tab` 포커스 내비게이션은 `Run()` 메시지 루프에서 처리(modal → inputWindow → main 순 우선).

---

## 8. JKWindow 비클라이언트 영역과 페인팅

### 8.1 비클라이언트 영역

- 클라이언트 시작 오프셋: 테두리 **2pt**(kBorder), 타이틀 바 **24pt**(kTitle) — 논리 pt 고정, `SDL_RenderSetScale()`이 물리 px로 확대.
- 클라이언트 영역 = `{2, 24, w−4, h−26}` — `OnRectChanged()`에서 `JKControl`의 padding과 무관하게 직접 계산한다.
- **PerformLayout 무한 재귀 회피**: JKWindow가 자기 자신을 `PerformLayout()` 대상에 포함하면 `SetRect → PerformLayout → SetRect` 무한 재귀에 빠지므로, 직계 자식만 재배치한다.
- `ResizeWindow()` 하한 크기 64×48.

### 8.2 페인팅 (`PaintWindow` / `OnPaintClient`)

- 타이틀 바: `(0,0,128)` 파란 FillRect, 높이 24pt. 타이틀 텍스트는 흰색 비트맵 폰트(내부 여백 4px).
- **프레임 테두리 선**: `DrawRect` 1px 회색 `(192,192,192)` — 클라이언트 오프셋(2pt)과 그려지는 선(1px)은 서로 다른 개념이니 혼동하지 말 것.
- 클라이언트 배경: `(240,240,240)`.
- 닫기 버튼: 부모가 있는 떠 있는 윈도우에만 표시. 우상단 20×20(margin 2), 회색 배경 + 흰색 × 표시.
- **픽셀 검증 시 주의**: DrawRect/DrawLine 기반 그리기는 `x+w−1`까지 그리는 inclusive 관습(Win16 GDI 유래)이라 h=24 사각형의 하단 테두리는 y+23에 나타난다 → 픽셀 스캔은 ±1px 허용.

### 8.3 드래그/리사이즈 (`RespondMessage`)

- `HitTestRegion()`으로 TitleBar/Border/Client 영역을 구분. 타이틀 바 드래그는 `WA_TITLEMOVEABLE`, 테두리 리사이즈는 `WA_BORDERRESIZABLE` attr이 필요.
- 리사이즈 핫스팟은 우하단 10×10px로 단순화됨.
- 드래그/리사이즈 중 `MouseMove`는 `MoveWindow(dx,dy)`/`ResizeWindow(dx,dy)`로 처리되며 자식 컨트롤에는 전달되지 않는다(dx/dy는 §7 변환식을 거친 값).

---

## 9. 검증 방법과 회귀 기준값

### 절차

1. 빌드: `prototype\sdl2_jkwindow\build_sdl2_jkwindow.bat`
2. 실행: `prototype\sdl2_jkwindow\run_sdl2_jkwindow.bat [mode]` (없음=메인 데모 / `jango` / `occ` / `test`)
3. 필요시 DPI-aware 스크린샷 캡처 후 픽셀 분석 — PowerShell 기본 캡처는 DPI 가상화가 적용되므로 `SetProcessDPIAware()`를 호출한 뒤 찍어야 물리 픽셀과 정합한다.
4. 자동 검증: `tools\verify_fixwin3.ps1` (모드당 14항목) — 방법론 전체는 `15_verification_playbook.md` 참조.

### 회귀 기준값 (125% DPI, 1920×1080 물리 화면, 앱 논리 1920×1080 요청 — 2026-08 측정/계산)

| 항목 | 값 |
|------|-----|
| 작업 영역(논리) | 1536×816 pt |
| 생성 클램프 크기 | 1528×781 pt |
| 클라이언트 영역(물리) | 1910×976 px |
| 등비 배율 fit | 0.9036~0.9046 |
| 레터박스 밴드 | 좌·우 약 87/88 px (상·하 0 px — 세로 제한) |

> 모니터/작업표시줄 설정에 따라 ±수 px 변동 가능. 창이 화면 위로 밀려 타이틀 바가 잘리면 §4.3의 클라이언트 원점 보정이 깨진 것이므로 가장 먼저 확인할 것.

---

## 10. 관련 파일/문서

| 파일 | 내용 |
|------|------|
| `prototype/sdl2_jkwindow/src/JKApplication.cpp` | Init(생성/재배치), UpdateScale, Render, TranslateSDLEvent |
| `prototype/sdl2_jkwindow/include/JKApplication.h` | 스케일 관련 멤버(scaleX/Y, logicalW/H, letterboxX/Y, ptToPhysX/Y) |
| `prototype/sdl2_jkwindow/src/JKWindow.cpp` | 비클라이언트 영역 계산, 레이아웃 재귀 회피 |
| `10_sdl2_windows_setup.md` | 빌드 환경 |
| `11_jkwindow_sdl_mapping.md` | JKWINDOW → SDL2 클래스 매핑 (§7 좌표 체계) |
| `12_sdl2_prototype_roadmap.md` | 프로토타입 로드맵 (Phase 5) |
| `.clinerules/20_dpi-coordinates.md` | DPI/좌표계 작업 규칙(세션 자동 로드) |
| `15_verification_playbook.md` | 화면 검증 방법론·자동 검증 스크립트 사용법 |
---

## 11. 다중 모니터·혼합 배율 (2026-08 실험·확정)

### 11.1 테스트 환경 (실측, PMv2 Win32 기준)

| 모니터 | 물리 해상도 | 배율 | 원점 | 작업영역 (물리) |
|--------|------------|------|------|----------|
| 좌측 보조 | 1920×1080 | 100% | (-1920, 0) | — |
| 주 | 1920×1080 | **125%** | (0, 0) | 1920×1020 (논리 1536×816 pt) |
| 우측 보조 | **1600×900** | 100% | (1920, 0) | 1600×852 (태스크바 48px) |

### 11.2 좌표 조회별 신뢰도 (혼합 배율 실측 결론)

| 조회 경로 | 결과 | 신뢰도 |
|-----------|------|--------|
| Win32 `GetMonitorInfo` / `GetWindowRect` / `MapWindowPoints` | **항상 물리 px** (PMv1 선언 앱에서도 동일 — 실측 확인) | ✅ 이것을 쓴다 |
| `SDL_GetDisplayUsableBounds()` | 디스플레이별 ddpi 환산이 섞인 좌표. 각 디스플레이의 ddpi 값 자체는 정확 | ⚠️ 단일 모니터 환경에서만 사용 |
| `.NET Screen.AllScreens` 등 GDI+ 계열 | 비-주 모니터가 주 모니터 배율로 가상화 (예: 1600×900@100% 보조를 2000×1125, 원점+2400으로 보고) | ❌ 절대 신뢰 금지 |

> **실제 재해 사례(2026-08)**: 우측 보조(1600×900@100%)를 `Screen.AllScreens` 값 기준으로
> 배율 보정(monScale)한 `ReapplyPlacement`를 적용하자 창이 (2633,127) — 모니터 오른쪽 바깥으로 밀려났다.
> PMv2 테스트 드라이버의 `GetMonitorInfo` 덤프로 Win32 값이 물리 px임이 교차 검증되어 보정 배율은 폐기됨.
> 앱 고유 배치(1953,21, 1534×810)가 이미 1600×900 모니터 작업영역의 정중앙이었다.

### 11.3 모니터 이동 후 재배치 — `JKApplication::ReapplyPlacement()`

창이 다른 모니터로 옮겨지면 다시 배치한다:

1. `MonitorFromWindow` → `GetMonitorInfo`로 작업영역 **물리 px** 확보
2. 프레임 전체 크기를 작업영역 중앙에 배치 (§4.3과 동일한 중앙 정렬 계산)
3. 장식 두께 산출: `MapWindowPoints(hwnd, nullptr, ...)` 반환값 − **현재 프레임 원점**.
   반환값 자체는 클라이언트 원점의 절대 화면 좌표이니 그대로 두께로 쓰지 말 것 — 그대로 더하면
   모니터 이동 시 배치가 매번 에스컬레이션한다(실제 재해 사례)
4. `GetDpiForWindow()` 실측값으로 pt 환산 (렌더러에서 온 stale dpiScale 대신)
5. `SDL_SetWindowPosition(pt)` — **클라이언트 원점 기준**

멱등성 필수: 같은 자리에서 재적용하면 좌표가 불변이어야 한다(드리프트 제로).

### 11.4 검증 기준값 (다중 모니터 드래그 테스트)

| 단계 | 기대 프레임 rect (물리 px) |
|------|---------------------------|
| 주 모니터 초기 | (2, 4) 1534×810 외곽 프레임 |
| 우측 보조(1600×900@100%) 착지 | (1953, 21) 1534×810 — 작업영역(1920,0,1600×852) 정중앙 |
| 주 모니터 복귀 | (2, 4) 완전 복원 (드리프트 0) |

### 11.5 테스트 드라이버의 DPI 인식 (`tools/app_mouse_test3.ps1`)

- 드라이버와 앱 모두 **동일 DPI 인식(PMv2)**이어야 좌표 비교가 성립한다.
- 시스템 인식(기본) 프로세스는 **비-주 모니터 좌표가 가상화**됨 — 주 125%/보조 100% 환경에서
  드라이버의 `GetWindowRect`/`CopyFromScreen`이 ×1.25 부풀려져 캡처 영역이 어긋나고
  "창이 화면 밖"이라는 오판을 만든다.
- 해결: 드라이버 시작 시 `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)` (+ `SetProcessDPIAware()`/`SetProcessDpiAwareness(2)` 폴백).
- PowerShell 함정: `Write-Output`은 `$var = Func-Call` 형태 호출 시 변수에 흡수되어 콘솔에 안 찍힌다 → 로그용은 `Write-Host`.

### 11.6 앱의 DPI 인식 선언 현실

- mingw 링커 기본 매니페스트는 PMv1을 선언하고 런타임 PMv2 승격은 실패하지만(`GetThreadDpiAwarenessContext`로 확인 가능),
  **Win32 창/모니터 좌표는 어차피 모두 물리 px로 반환**되므로 `ReapplyPlacement`는 정상 동작한다(실측).
- "PMv1이라 보정이 필요하다"는 가설로 배율을 곱하면 창이 모니터 밖으로 나간다. 보정 금지.