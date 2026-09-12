# docs/35 — 데스크톱 에이전트: 스크린샷 도구

날짜: 2026-09-11
이전: docs/33 (알림 센터), docs/34 (트리거 on/off)
계획: docs/superpowers/plans/2026-09-11-agent-shot.md
스펙: Desktop Agent API §7 (스크린샷)

## 1. 요약

윈도우 캡처(capture_window), 영역 캡처(capture_region), 러버밴드 오버레이
앱(jkapp_snap), 스크린샷 뷰어(jkapp_shot + 팔레트 /shot)의 4층 구성.
브레인스토밍 결정: 스크린샷은 "영역 러버밴드 오버레이까지" 전체 스코프,
뷰어가 진입점 (오버레이는 뷰어의 영역 캡처 버튼과 /shot에서 도달).

## 2. 서버 도구

### 2.1 `capture_window {"id":<surfaceId>}`

- 컴포지터 레이어 픽셀을 `state\screenshots\shot_<epoch-ms>_<id>.png`로
  저장 (stb_image_write v1.16 vendored, 단일 TU에
  STB_IMAGE_WRITE_IMPLEMENTATION + STATIC).
- 클라이언트 표면은 RGBA32 바이트 순서라 `stbi_write_png(path, w, h, 4,
  px, w*4)` 직행.

### 2.2 `capture_region {"x","y","w","h"}`

- 인자는 **논리 데스크탑 좌표**; 프레임버퍼는 물리 픽셀이므로
  OutputScale(125% DPI에서 1.25) 곱셈. 출력 경계와 먼저 교차시켜 클램프.
- **요청자 레이어 은닉**: 오버레이 앱이 자기 좌표로 자기장면을 찍을 때 디임과
  선택 사각형이 샷에 들어가지 않게, 읽기 전에 요청자 레이어를 숨기고
  `Composite(false)` → `SDL_RenderReadPixels` → 복원. 이를 위해
  `JKWindowServer::Composite(bool)` / `JKCompositor::Composite(bool)` 분리
  (present=false면 SDL_RenderPresent 생략).
- 출력은 전체 프레임 1회 readback 후 행 단위 memcpy 크롭
  (`shot_<epoch-ms>_region.png`).

## 3. 러버밴드 오버레이 (jkapp_snap)

### 3.1 서버 스폰 훅

클라이언트 표면 크기는 호스트가 Init에서 메타로 고정한다 — 클라는 런타임에
자기 크기를 바꿀 수 없다. 그래서 **서버가 데스크탑 크기를 지시**한다:
스폰 배치에서 `client->Title() == kCaptureOverlayTitle("Region Capture")`면
`CommitChromeResize(ww, wh)` + 위치 (0,0) — DockShellClient가 태스크바를
풀데스크폭으로 도킹할 때 쓰는 것과 같은 ResizeSurface 왕복. 메타 크기
(800×600)는 플레이스홀더일 뿐. 실측: 800×600 placeholder → (0,0)
1280×720으로 리사이즈 스폰 확인.

크롬리스 서버 처리도 동반:
- 컴포지터가 닫기 버튼 오버레이를 그리지 않음 (`!IsShell &&
  Title() != kCaptureOverlayTitle`) — 데스크탑 구석의 회색 X 방지.
- TryChromeGrab에서 제외 — 상단 24px가 타이틀 드래그로 씹히지 않음.

### 3.2 투명 표면

- `JKClientApplication::WantsTransparentSurface()` — 기본 false,
  snap만 true. ComposeScene의 클리어가 (192,192,192,255) → (0,0,0,0).
  `SDL_RenderClear`는 블렌드 모드를 무시하고 raw 색상+알파를 쓰므로 진짜
  투명이 보장된다.
- 스냅 루트는 OnPaintClient를 비움 (기본 페인트가 서피스를 채움).
- ImGui: 전면 (0,0,0,70) 디임 + 드래그 중 앰버 rect + 크기 라벨. 실측 캡처:
  중앙 픽셀 정확히 (0,0,0,70) — 데스크탑이 비친다.

### 3.3 상호작용

- 16ms 타이머, ImGui io.MouseDown으로 드래그, 마우스업에 정규화된 rect가
  4×4 이상이면 `SendAgentQuery(capture_region)` — 오버레이 레이어가 (0,0)
  스케일 1이므로 **클라 좌표 = 데스크탑 논리 좌표**.
- reply 도착 or 3초 타임아웃에 스스로 Close (팔레트의 1-deep 패턴). ESC 취소.
- 서버가 캡처 중 요청자 레이어를 숨기므로 디임/선택 rect는 샷에 안 들어감.

## 4. 뷰어 (jkapp_shot) + /shot

- 480×420 "Screenshots". `<exeDir>\state\screenshots\*.png`를
  FindFirstFileA로 스캔, **이름 역순 정렬이 곧 최신순** (shot_<epoch-ms>_ 접두).
- 선택 → `LoadImageFile` (stb_image) → SDL RGBA32 STREAMING 텍스처 업로드 →
  ImGui::Image 폭 맞춤. 텍스처는 선택/새로고침/종료마다 파괴(1개만 유지),
  업로드는 RenderOverlay에서만 (렌더러 접근 가능 시점).
- 새로고침 / 영역 캡처(launch_app snap, fire-and-forget) 버튼, 한글 UI는
  Malgun Gothic (notify 센터 관례).
- 팔레트 `/shot` → `launch_app {"app":"shot"}`. 런처 아이콘은 앰버 카메라
  글리프 (gen_shot_icon.ps1, notify 아이콘 관례 클론).

## 5. 검증

`tools/probes/probe_agent_shot.ps1` — 6 체크 (전부 agentctl/파일 판정,
레슨 34):

1. capture-window: minesweeper 스폰 → capture_window → ok + path + PNG 매직.
2. capture-region: 120×90 논리 요청 → ok + path.
3. region-size: 물리 크기 150×112 (논리×1.25, 4:3 종횡비 보존).
4. overlay-spawn: launch_app snap → "Region Capture"가 (0,0) 1280×720
   (서버가 데스크탑 크기로 리사이즈한 실측).
5. viewer-spawn: launch_app shot → "Screenshots" 존재.
6. viewer-count: 창 1개 (중복 스폰 없음).

전체 회귀: jkagentd selftest 0 실패, mcp/e2e/palette/chat/chat_llm/
triggers/notify/triggerctl/shot 전부 PASS.

## 6. 제한 / 후속

- 오버레이 드래그/ESC는 실입력이 필요해 프로브가 못 잡는다 — 사용자 실측 항목
  (스폰/리사이즈/투명/캡처 경로는 프로브가 검증). — **2026-09-12 실사에서
  외부 합성 클릭/드래그(SendInput)로 자동 검증 달성, §6.1 참조.**
- 다중 모니터는 단일 출력 가정 (OutputWidth/Height).
- 오버레이 중복 스폰 시 마지막 것이 위에 쌓임 — 실사용 시나리오에서 문제되면
  토글로 승격.

### 6.1 실사 피드백 수정 (2026-09-12)

사용자 실측에서 세 건이 나와 수정했다. 전부 프로브 맹점이었다.

1. **"영역 캡처" 버튼 무반응** — 뷰어의 버튼들이 ImGui 기본 패딩으로 y≈8에
   깔렸고, 서버는 모든 일반 클라 표면의 상단 24px(`kChromeTitleBar`)을
   타이틀바 드래그 존으로 쓰므로(`TryChromeGrab` zone 3) 마우스 DOWN이 클라에
   도달하지 않았다. 레슨 8("첫 행은 SetCursorPosY(30) 이상")의 서버측 근거가
   여기서 확정됐다 — 첫 UI 행을 크롬 존 아래로 내리는 한 줄 수정. 프로브는
   spawn을 agentctl로 하고 클릭하지 않아 못 잡았다.
2. **오버레이가 반투명이 아니라 불투명 검정** — 오버레이 레이어 픽셀은
   (0,0,0,70)으로 완벽했지만 컴포지터가 화면에 그릴 때 알파를 무시했다
   (실측: 오버레이 떠 있는 상태의 region readback = (0,0,0,255)). 이 SDL
   빌드의 STREAMING 텍스처는 기본 블렌드가 BLEND가 아니어서
   `SDL_SetTextureBlendMode(BLEND)`를 AddLayer/ResizeLayer에 명시. 런처
   아이콘 로드 경로는 이미 명시하고 있었다(선례 누락). 프로브는 오버레이
   없이 region을 캡처해 못 잡았다 → **dim-blend 체크 신설**(오버레이 떠 있는
   상태에서 지뢰찾기 회색 영역 readback이 검정이면 FAIL).
3. **캡처 결과에 뷰어가 보임** — 오버레이(요청자)만 숨기고 그것을 연 뷰어는
   숨기지 않았다. launch_app snap 시 요청자 연결 id를 기억했다가 오버레이
   연결 등록에서 페어링(`overlaySpawner_`)하고, capture_region readback에서
   요청자와 스포너 레이어를 함께 숨긴다. agentctl에서 스폰하면 스포너가
   컨트롤 전용이라 숨길 레이어가 없다(자연 노오프).
4. **외부 합성 클릭으로 e2e 달성** — SendInput으로 뷰어 버튼 클릭 → 오버레이
   스폰 → 러버밴드 드래그 → 캡처까지 자동 검증했다. 좌표계 교훈은 레슨 8번.

## 7. 레슨

1. **클라 표면 크기는 Init 시 고정** — SurfaceCreatePayload는 호스트가 meta에서
   넘긴 크기로 고정되고 런타임 변경 경로가 없다. "클라가 데스크탑 크기를
   알아내 표면을 만드는" 설계는 불가능 — 서버가 ResizeSurface 왕복으로
   지시하는 게 정답 (셸 도킹이 이미 그렇게 동작).
2. **SDL_RenderClear는 블렌드를 무시한다** — 덕분에 (0,0,0,0) 클리어가
   진짜 투명 서피스를 만든다. 반투명 오버레이는 "투명 클리어 + ImGui가
   알파 블렌딩으로 그리기" 두 층으로 분리. — **단, 이 사실은 클라 자기
   표면까지다. 서버가 다른 레이어 위에 얹을 때는 텍스처 블렌드 모드가
   별개다 (레슨 7번).**
3. **close_window 기본 deny가 프로브 정리를 막는다** — 창 닫기가 권한 게이트에
   걸리면 `Win32_Process WHERE CommandLine LIKE '--client <app>'`로 클라
   프로세스를 직접 찾아 Stop-Process (서버와 같은 이미지명이라 PID 매칭 필수).
4. **find -newer 앞의 -o 우선순위** — `find src -name "*.cpp" -o -name "*.h"
   -newer exe`는 이름 조건과 -newer가 OR로 묶여 전수 나열된다. 괄호로 묶어야
   stale 체크가 된다 (레슨 37의 mtime 확인 절차 자체가 틀리기 쉽다).
5. **상단 24px 크롬 존은 서버가 클라에서 뺏는다** — 레슨 8(vplayer)의 근거
   확정: `TryChromeGrab` zone 3이 `ly < kChromeTitleBar`의 마우스 DOWN을
   타이틀바 이동 그랩으로 소비한다. 첫 UI 행은 반드시 y≥30 — "버튼이 그려져
   보인다"와 "클릭이 닿는다"는 다르다.
6. **이 SDL 빌드의 STREAMING 텍스처는 기본 블렌드가 BLEND가 아니다** —
   per-pixel alpha를 지닌 레이어는 `SDL_SetTextureBlendMode(BLEND)` 명시가
   필수. 미설정 시 알파 무시 + RGB 대입 → (0,0,0,70) 디임이 불투명 검정
   화면이 된다. "레이어 픽셀은 옳은데 화면이 다르다"면 텍스처 블렌드 모드부터
   의심한다. 코드베이스에 선례(런처 아이콘 경로의 명시 설정)가 이미 있었다 —
   선례 검색이 이 버그를 1시간 먼저 잡았을 것이다.
7. **캡처 UI의 스폰자도 숨긴다** — 셀프 레이어만 숨기면 "캡처를 연 앱"이
   결과에 남는다. launch_app 요청자를 기억해 오버레이 연결에서 페어링하는
   서버측 소량 상태로 해결 (컨트롤 전용 스폰은 자연 노오프).
8. **외부 합성 입력의 좌표계는 대상 프로세스의 DPI 가상화를 따른다** —
   DPI 가상화된 윈도우에 대해 PowerShell의 ClientToScreen/GetClientRect는
   가상화(논리) 좌표를 돌려주고, 같은 프로세스의 SetCursorPos도 같은 공간을
   쓴다. 물리 배율(×OutputScale 1.25)을 곱하면 과조준으로 빗나간다 — 서버
   내부 마우스 변환(레슨 6의 renderer ratio)과 외부 합성 입력은 좌표계가
   다르다. 먼저 캡처 도구로 실제 UI 좌표를 재측정하고 1:1로 클릭한다.

## 8. 파일

- `src/server/JKWindowServer.cpp` — capture_window/capture_region,
  스폰 배치 훅, TryChromeGrab 제외, 오버레이 스포너 페어링(§6.1).
- `src/server/JKCompositor.cpp` + `include/server/JKCompositor.h` —
  Composite(bool), OutputWidth/Height, kCaptureOverlayTitle, 닫기 오버레이 제외.
- `include/client/JKClientApplication.h` + `src/client/JKClientApplication.cpp`
  — WantsTransparentSurface + 조건부 클리어.
- `include/apps/ClientSnapApp.h` / `src/apps/ClientSnapApp.cpp` /
  `src/apps/JKAppModule_snap.cpp` — 러버밴드 오버레이.
- `include/apps/ClientShotApp.h` / `src/apps/ClientShotApp.cpp` /
  `src/apps/JKAppModule_shot.cpp` — 뷰어.
- `src/apps/ClientPaletteApp.cpp` — /shot, /help.
- `tools/probes/gen_shot_icon.ps1`, `tools/probes/probe_agent_shot.ps1`.
- `third_party/stb/stb_image_write.h` — v1.16 vendored.