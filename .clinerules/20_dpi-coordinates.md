# DPI / 좌표계 함정 (최다 삽질 지점)

타이틀바가 화면 위로 잘리던 버그(2026-08 수정)의 근본 원인. **SDL2 API끼리 단위·원점이 다르다.**

## 이 개발기 화면 환경 (기준값)

- **3대 모니터·혼합 배율(2026-08 실측, PMv2 Win32 기준)**:
  - 주 모니터: 물리 1920×1080 @ **125%**, 원점 (0,0) → 작업영역 물리 (0,0) 1920×1020 / 논리 1536×816 pt
  - 보조(우측): 물리 **1600×900 @ 100%**, 원점 (1920,0) → 작업영역 물리 (1920,0) 1600×852 (태스크바 48px)
  - 보조(좌측): 물리 1920×1080 @ 100%, 원점 (-1920,0)
- dpiScale = 1.25 (주 모니터, 렌더러 출력 px ÷ 윈도우 pt)
- **좌표 조회 신뢰도 순위**: PMv2/PMv1 Win32(GetMonitorInfo/GetWindowRect) = 물리 px 전부 신뢰 ✓ > SDL_GetDisplayUsableBounds(디스플레이별 ddpi 환산 섞임 — ddpi는 정확) > **.NET Screen.AllScreens(비-주 모니터가 ×1.25 가상화된 값 — 1600×900@100% 보조를 2000×1125@+2400으로 보고함, 절대 신뢰 금지)**

## 다중 모니터·혼합 배율 함정 (2026-08 다중 스케일 검증)

- 드래그 테스트 드라이버(테스트 스크립트)와 앱 모두 **PMv2 이상 인식**이어야 좌표 비교가 성립한다. 시스템 인식 프로세스는 비-주 모니터 좌표가 배율 가상화됨(주 125%/보조 100% 환경에서 드라이버의 GetWindowRect가 ×1.25 부풀려짐 → 캡처 영역이 어긋나 "화면 밖" 오판). 드라이버 템플릿: `tools/app_mouse_test3.ps1` 패턴(`SetProcessDpiAwarenessContext(-4)` + 폴백)
- 앱(mingw 링커 기본 매니페스트가 PMv1 선언 → 런타임 PMv2 승격 실패)에서도 Win32 창/모니터 좌표는 **모두 물리 px**로 반환됨(실측 확인) — PMv1 가상화 가설로 배율 보정을 곱하면 창이 모니터 밖으로 나간다(실제 재해 발생). 보정 금지
- `MapWindowPoints(hwnd, nullptr, ...)` 반환값은 **장식 두께가 아니라 클라이언트 원점의 절대 화면 좌표**다. 두께 = 반환값 − 현재 프레임 원점. 절대값을 그대로 더하면 모니터 이동 시 배치가 매번 에스컬레이션한다(실제로 겪은 함정)
- 모니터 이동 후 배치는 `JKApplication::ReapplyPlacement()`가 담당: Win32 작업영역 중앙에 프레임 배치 → 장식 두께만큼 클라이언트 원점으로 환산 → `GetDpiForWindow()` 실측값으로 pt 환산 → `SDL_SetWindowPosition(pt)`. 멱등해야 하며(재적용 시 드리프트 제로) SDL의 stale dpiScale 대신 GetDpiForWindow를 쓸 것

## SDL2 API 단위·원점 정리 (외우지 말고 이 표를 볼 것)

| API | 단위/원점 |
|---|---|
| `SDL_SetWindowPosition(x,y)` | **클라이언트 영역 원점** (프레임 아님!) |
| `SDL_GetWindowBordersSize()` | **물리 px** |
| `SDL_GetWindowSize()` | 논리 pt |
| `SDL_GetDisplayUsableBounds()` | 논리 pt |
| `SDL_GetRendererOutputSize()` | 물리 px |

- 장식 px ÷ dpiScale = 장식 pt. **px과 pt를 섞어 쓰면 타이틀바 잘림/창 가림이 재발**한다
- dpiScale은 렌더러 생성 후에만 계산 가능 (`SDL_GetRendererOutputSize`)
- 창 배치 올바른 패턴: `JKApplication.cpp` `Init()`의 재배치 블록 참고 — 프레임 전체 크기를 작업영역에 중앙 정렬 계산 후, 클라이언트 원점 보정을 위해 장식 left/top을 더해 SetWindowPosition
- 창 배치 코드는 **렌더러 생성 뒤**에 둘 것

## 앱 내부 좌표계 (렌더링 모델)

- 앱 논리 좌표는 **1920×1080 고정**. `fit = min(clientW/1920, clientH/1080)` 등비 스케일로 중앙 정렬
- 레터박스: `Render()`가 배경을 (192,192,192)로 클리어 후 콘텐츠 blit → 밴드 색은 (192,192,192)
- 기준값(회귀 비교용): 125% 환경에서 클라이언트 1910×976px, 좌우 밴드 87/88px, 스케일 0.9036~0.9046 (fit 0.9037)
- `JKWindow`는 콘텐츠 둘레에 **1px 회색 보더**를 그림 → 픽셀 검증 시 엣지 판정에 영향
- `JKRect`는 **inclusive 좌표** (h=24면 실제 25행 그려짐, Win16 GDI 관습). 픽셀 검증은 ±1 허용 스캔으로
- 앱 타이틀바: kTitle=24 (in-app 타이틀바, 25행)

## 화면 검증(PowerShell) 시 주의

- `CopyFromScreen` 스크립트는 폼 로드 전 반드시 `[Win32]::SetProcessDPIAware()` 호출할 것 → **미호출 시 DPI-unaware 프로세스라 화면 좌상단 물리 1536×864 영역만 캡처**하고 "콘텐츠가 작업표시줄 아래로 넘친다"는 오판을 유발함 (실제로 겪은 함정)
- 검증은 물리 픽셀 1:1 기준. PNG 저장 후 픽셀 스캔
- 템플릿: `tools\verify_fixwin3.ps1` — SetProcessDPIAware 후 지오메트리/타이틀바/레터박스/콘텐츠/스케일 14항목 검사 (방법론: `ARCHITECTURE_DOCS/15_verification_playbook.md`)
- 상세 아키텍처 문서: `ARCHITECTURE_DOCS/14_sdl2_window_dpi.md` (좌표계 계층·재배치 알고리즘·렌더링 파이프라인·회귀 기준값)