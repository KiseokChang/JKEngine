# 39. 창 최대화/복원 — 서버 크롬 최대화 버튼 + 제목바 더블클릭 + 드래그 복원

- 날짜: 2026-09-13
- 상태: 구현 완료. 스펙 `docs/superpowers/specs/2026-09-13-window-maximize-design.md`
- 선행: docs/28 (크롬/작업영역), docs/35 §6.1 (오버레이 크롬 면제 선례 + SendInput 좌표 선례)

사용자 질문 "SDL창 최대화 가능한지"에서 기원 — 앱 창은 SDL 창이 아니라 합성
레이어라 OS 최대화가 없고, 크롬에는 닫기 X만 있었다. 최소화는 이미 taskbar
WindowMinimizeToggle(docs/28)이 담당하므로 이 문서는 최대화/복원만 다룬다
(최소화 버튼 추가 없음, 스냅-반쪽도 스코프 밖).

## 1. 모델

앱 창(합성 레이어)의 최대화 = **서버가 레이어를 작업영역 크기로 리사이즈 + (0,0) 배치,
scale 1**. 기존 `CommitChromeResize`(드래그 리사이즈/셸 도킹/오버레이 전체화면이
쓰는 서버-지시 라운드트립)와 `SetLayerPosition`의 재조합 — **클라 프로토콜 변경 0**
(ResizeSurface는 이미 존재, 클라 앱은 드래그 리사이즈로 같은 경로를 이미 지원).

- **최대화**: 현재 rect 저장 → `CommitChromeResize(id, ww, wh−reserve, ww, wh−reserve)`
  (reserve = `ShellReserveHeight()`, taskbar 미덮음) + `SetLayerPosition(id, 0, 0)` +
  `client.SetPosition(0, 0)`. dispW/dispH = 새 표면 크기이므로 fit 스케일이 해제되어
  scale 1 — 창이 작업영역에 1:1.
- **복원**: `CommitChromeResize(id, surfW, surfH, dispW, dispH)`(저장 dispW/dispH가
  기존 fit을 복원 — ResizeLayer가 scale을 1로 리셋하는 기존 동작) + `SetLayerPosition(x, y)`
  + `client.SetPosition(x, y)` + 상태 제거.
- 클라이언트 측 코드 변경 없음 (docs/31 §5 jkchat도, 앱도 무변경).

## 2. 크롬

- `kChromeMaximizeSize = 20`, `kChromeMaximizeGap = 2` (JKCompositor.h — 닫기 상수 옆).
  닫기 X 왼쪽에 20×20 버튼, x0 = `w − margin − closeSize − gap − maxSize`.
- 그리기는 `DrawMaximizeButton` — `DrawCloseOverlay`와 동일 박스(192,192,192 fill +
  black outline), 표면 px 공간이라 fit 스케일에서 닫기 X와 똑같이 비례 축소.
  글리프(흰선, SDL_RenderDrawRect): **최대화** = pad 5 내부 외곽선 사각형,
  **복원** = 겹친 사각형 두 개 (pad 7 큰 사각형 + pad 3 작은 사각형 — 창과 그 그림자).
- `DrawCloseOverlay`의 **가드 공유**: `Composite()`에서 `!IsShell() && Title() !=
  kCaptureOverlayTitle`일 때만 그림 — 셸 레이어와 캡처 오버레이(docs/35)는 버튼이
  아예 없고, 히트 존도 같은 이유로 크롬 경로 자체가 조기 반환.
- **maximizedFlag는 JKCompositorLayer의 bool** (`SetMaximized`/`Maximized`): 컴포지터
  그리기 경로가 JKWindowServer에 접근할 수 없어 글리프 선택용으로만 미러 —
  진실원은 서버의 `preMaxRects_` map.

## 3. 상태/토글

- **`std::map<uint32_t layerId, MaxState> preMaxRects_`** (`MaxState{x, y, surfW,
  surfH, dispW, dispH}` = 최대화 직전 rect). **존재 = 최대화 중** — 별도 bool 상태
  없음. 서버 루프 스레드 전용 접근(인접 chrome-grab 상태와 같은 레짐), 자체 락 없음.
- `ToggleMaximize`: map에 없으면 rect 저장 후 최대화 + `window.maximized`; 있으면
  `RestoreFromMaximize` 위임.
- `RestoreFromMaximize` = **공유 복원 코어** (토글/더블클릭/드래그 복원 3곳 사용):
  복원 + map erase + 플래그 해제 + `window.restored` 1회. 최대화가 아니면 false —
  그랩 시작 시 조건 없이 호출해도 안전(그랩당 restored 이벤트 최대 1회).
- **연결 종료 시 erase**: `CleanupDisconnectedClients`의 RemoveLayer 경로 (호출부가
  전체에서 1곳)에서 `preMaxRects_.erase(client->Id())` — 재활용된 표면 id가
  옛 pre-max rect를 상속하지 않게 레이어 제거 직전에.

## 4. 상호작용 4종 (TryChromeGrab 존 순서: close → 최대화 버튼 → 더블클릭 → 드래그 복원 → 리사이즈 엣지 → 제목바 이동)

1. **버튼 클릭 토글**: 닫기 존 다음(리사이즈 엣지 앞) 최대화 존 — `inMaxX = lx ∈
   [maxBtnX0, maxBtnX0+20)` + `inCloseY` 동일. 히트 → FocusClient + PushWindowList +
   토글. 시그니처는 `int clicks` 인자를 얻음 (더블클릭용).
2. **제목바 더블클릭 토글**: `ev.button.clicks == 2`면 이동 그랩 대신 토글 (Windows
   관례). `ly ∈ [kResizeHotspot, kChromeTitleBar)`로 한정 — 상단 리사이즈 스트립은
   여전히 리사이즈 존. 버튼 존 위 더블클릭은 버튼 로직이 먼저 먹는다.
3. **드래그 복원 (deferred + 모션 스레숄드 — Fix 1 + final review)**: 최대화 레이어에서
   Move/Resize 그랩 시작 시 **복원하지 않고 `chromeRestorePendingId_`만 무장** — 복원은
   그랩 시작점에서 변위(Chebyshev 거리)가 **`kResizeHotspot`(6px)을 넘는 첫 MOUSEMOTION** 시점에
   발생 (mousedown 아님, 임계 이하 모션은 소비만 하고 무시). **아주 미세한 손떨림은
   복원 트리거가 아님 — 실제 드래그만 복원**: 실제 손의 더블클릭은 클릭 사이 ~1px
   떨림이 있는데 임계 없이는 그 떨림이 복원을 발화해 click 2(clicks==2)가 이제
   일반 상태인 창을 재최대화했다(최대화 유지 + restored/maximized 페어). 6px 임계로
   무모션·떨림 더블클릭 모두 존 1c에 그대로 도달해 정확히 한 번 토글. Move는
   `chromeGrabFX_/FY_`(그랩 점의 표면 내 분수 좌표)를 저장해 복원 후
   `fx·dispW/fy·dispH` 재앵커로 창을 커서 아래에 재배치(Windows와 동일 체감);
   Resize는 복원 rect에서 엣지 핫스팟을 재평가하고 클릭이 더 이상 엣지가 아니면
   **그랩 취소**(복원된 채 grab-free). MOUSEBUTTONUP은 pending을 클리어 —
   **움직임 없는 클릭은 복원하지 않는다** (Windows 패리티).
   이유: 초기 구현(mousedown 복원)에서는 최대화 상태 더블클릭이 click 1 = 드래그
   복원 → click 2 = 재최대화로 **두 이벤트/제자리 토글 없음**이 되었는데, deferred로
   바꾸면 click 1은 무장만 하고 click 2(clicks==2)는 여전히 최대화 상태에서 존 1c에
   도달해 정확히 한 번 복원 — 문제가 구조적으로 사라진다. 최대화가 아닌 그랩은
   임계 없이 기존 그대로(첫 모션이 곧 이동/리사이즈).
4. **호버 커서**: 버튼 존(`inMaxX && inCloseY`)은 닫기 존과 동일하게 화살표 유지
   (UpdateChromeHoverCursor에 동일 면제 조건 추가).

**최대화 상태에서 리사이즈 엣지 그랩**: 첫 이동에 복원 후 **복원 rect 기준으로
리사이즈** (v1 동작). Windows는 최대화 창에 엣지 리사이즈가 없어 비교 대상 자체가
없음 — 이 차이는 §7에 기록.

## 5. 이벤트/카탈로그

- 토글 시 `window.maximized` / `window.restored` 발행 — `{"topic","id","title","ts"}`
  (id/title 최상위 — 서버 내부 이벤트 관례, `window.created/destroyed`와 같은 봉투,
  pid 없음). PushMaximizeEvent는 크롬 경로(서버 루프 스레드, clientsMutex_ 미보유)에서
  호출 — FocusClient→PushAgentEvent와 같은 레짐.
- events_list 정적 카탈로그에 2행 추가 (source "server", fields `["id","title"]`) —
  카탈로그 총 11행.
- WindowList 페이로드 변경 없음 (taskbar 버튼에 최대화 표시는 후속 여지).

## 6. 테스트

- **`engine/tools/probes/probe_agent_maximize.ps1` (5체크, 6단계 플로우, exit 1 on FAIL)**
  1. 스폰+list_windows rect 추정 (minesweeper, MCP 파이프)
  2. **버튼 최대화**: SendInput 클릭 → `window.maximized` 정확 1회, id 일치,
     rect가 (0,0) 작업영역(실측 1280×680 = 720−40 taskbar)으로 성장
  3. **버튼 복원**: 재클릭(복원 글리프) → `window.restored` 1회 + 원 rect 복원
     (정확 값 비교)
  4. **더블클릭 최대화**: 제목바 중앙 빠른 2클릭 → `window.maximized` 1회 +
     `window.restored` 0회 (드래그 복원이 deferred라 페어의 일반 클릭이 복원 안 함)
  5. **더블클릭 복원**: 재더블클릭 → `window.restored` 1회 + rect 복원
  (정리는 spawned `--client` PID kill + jkdesktop stop — chrome 입력은 서버 로컬이라
  permissions.json 승인 게이트 없음)
- **SendInput 좌표계 교훈 (docs/35 6.1-4 이후 처음 정식 정리)**:
  - **`MOUSEEVENTF_VIRTUALDESK` 필수 (다중 모니터)** — ABSOLUTE만으로는 프라이머리
    모니터로 정규화되어 가상 데스크톱 x<0 영역에서 ~61px 드리프트(이 리그 가상
    데스크톱 x=-1920 시작)로 버튼 존을 빗나감. `MOVE|ABSOLUTE|VIRTUALDESK`(0xC001)
    + `SM_XVIRTUALSCREEN..CYVIRTUALSCREEN` 메트릭으로 해결.
  - **`-WindowStyle Hidden` 서버는 FindWindow+SW_SHOWNOACTIVATE+TOPMOST 필요** —
    STARTUPINFO SW_HIDE가 SDL 창까지 숨겨 MainWindowHandle=0이 되고, 숨은 창은
    합성 클릭을 받지 못함. `FindWindow("SDL_app", "JKENGINE Window Server")` →
    `ShowWindow(SW_SHOWNOACTIVATE)` → `SetWindowPos(HWND_TOPMOST, SWP_NOACTIVATE)`
    (핀 없으면 인터랙티브 데스크톱의 사용자 창이 클릭을 먹음).
  - move+click은 한 SendInput 배치(3-input)로 원자화 — 그 사이 사용자 손이 커서를
    움직이는 실측 레이스 회피. 더블클릭 = 배치 + 70ms 후 커서 무이동 down/up.
  - scale = clientW/1280 (렌더러 비율, docs/35 레슨 8 — 모니터 OutputScale 추가 곱셈 금지).
  - 무BOM .ps1 비ASCII는 PS 5.1에서 cp949로 파싱 파손 (docs/15 레슨) — 프로브는 ASCII-only.
- **전체 회귀 13종**: `jkdesktop.exe test` 0 failure(s) + probe mcp 5/5, e2e 7/7,
  palette 4/4, chat 7/7, chat_llm 2/2, triggers 7/7, trust 7/7, ratelimit, events 5/5
  (11행 카탈로그), notify 6/6, shot 7/7, triggerctl 5/5 — 전부 exit 0.

## 7. 제한

- **최대화 중 서버 창 리사이즈(모니터 해상도 변경) 시 재최대화 없음** — 창은 새
  크기에 맞춰 늘어나지 않는다 (복원 rect는 유효, 크기만 옛값). 다음 배치.
  → **§8에서 해소** (SIZE_CHANGED → UpdateOutputBounds 재최대화 재발행).
- 최소화/스냅-반쪽 제외 (머리말 전제).
- 크롬(버튼 히트/그리기)은 표면 px 기준이라 fit 스케일에서 닫기 X와 똑같이 비례 축소.
- 최대화 중 리사이즈 엣지 그랩은 복원 후 복원 rect 기준으로 진행 — Windows에는
  최대화 창의 엣지 리사이즈가 없으므로 이 v1 동작은 의도적 차이.
- 더블클릭 직후 아주 짧은 시간에 그랩이 시작되면 pending id가 남을 수 있으나 MOUSEUP/
  그랩 종료에서 모두 클리어 — 스테일 pending이 이후 무관한 그랩에서 발화하지 않음.
- **드래그 복원 임계는 6px (kResizeHotspot)** — 그랩 시작점에서의 변위(가로/세로 중
  큰 값)가 6px를 넘어야 복원. 아주 미세한 손떨림은 복원 트리거가 아님(§4-3) — 실제
  드래그만 복원.
  임계 이하 모션은 그랩에 의해 소비되며 창을 움직이지 않는다.
- PushMaximizeEvent 봉투 버퍼 640 — PushAgentEvent와 동일 크기. 타이틀 상한 + JSON
  이스케이프(\uXXXX) 확장이 320을 초과할 수 있어 잘림 → malformed JSON 방지.
- 커밋: fb45dc5 (코어 + 크롬 버튼 + 상호작용), ce2bf44 (Fix 1 — deferred 드래그
  복원), a1368b1 (프로브), 7f7dbcc (프로브 폴리시), 본 문서, 드래그 복원 모션 임계 +
  이벤트 버퍼 640 (final review).
## 8. 데스크탑 리사이즈/최대화 (2026-09-13)

사용자 요청: jkdesktop SDL 서버 창 자체에 OS 최대화/리사이즈가 없었다. 서버 창
생성 플래그에 `SDL_WINDOW_RESIZABLE` 추가(JKWindowServer.cpp Init)로 해결.

- **플로우**: OS 최대화/리사이즈 → `SDL_WINDOWEVENT_SIZE_CHANGED` → 기존
  `UpdateOutputBounds()` (논리 크기 + 렌더러 물리 크기 + OutputScale 재계산 →
  `DockShellClient(nullptr)` 태스크바 재도킹) — 이 경로는 이미 존재했고 창만
  고정이라 잠겨 있었을 뿐이다.
- **최대화 레이어 재발행**: `UpdateOutputBounds()`가 멤버 `lastDesktopW_/H_`로
  논리 SIZE 변경을 감지(MOVED/DISPLAY_CHANGED는 무시, Init 첫 호출은 시딩만).
  SIZE 변경 시 `preMaxRects_`의 모든 id에 대해 새 작업 영역 기준 최대화 재발행:
  `CommitChromeResize(client, id, ww, wh-reserve, ww, wh-reserve)` +
  `SetLayerPosition(id, 0, 0)` + `client->SetPosition(0, 0)` (connection 측
  동기화 — DockShellClient 주석 참조). 클라이언트 죽은 엔트리는 skip.
- **이벤트 없음**: 데스크탑 크기 조정은 토글이 아니므로 `window.maximized`/
  `window.restored`를 발행하지 않는다. 대신 서버 로그 1행
  `[server] desktop size changed to %dx%d (re-maximized %zu layer(s))`
  (stdout = run_test 리다이렉트 로그). 저장된 pre-max rect는 그대로 유지.
- **프로브**: `engine/tools/probes/probe_desktop_resize.ps1` 5/5 —
  (1) spawn+list, (2) `ShowWindow(SW_MAXIMIZE)` → 로그 "1536x793" + 비최대화
  레이어 유지, (3) 큰 데스크탑에서 2차 앱 spawn이 새 영역 안에 배치, (4) 2차 앱
  크롬 버튼 최대화 → `window.maximized` rect == 1536x753(로그 유도, 1280x680 초과),
  (5) `ShowWindow(SW_RESTORE)` → 이벤트 없음(의도) + 로그 "1280x720 n=1" +
  레이어가 1280x680@0,0으로 재최대화. 회귀: probe_agent_maximize 5/5,
  probe_agent_e2e 7/7, `jkdesktop.exe test` 0 failure(s).
- **프로브 교훈**: 서버 창이 움직이므로 ClientToScreen 원점을 매 단계 재판독.
  scale은 clientW/논리폭 — 최대화 후 논리폭이 1536이 되므로
  probe_agent_maximize의 고정 /1280을 쓰면 1.5x가 되어 클릭이 저우측으로 빗나감
  (논리폭은 로그 라인에서 도출). 서버 stdout은 `Start-Process
  -RedirectStandardOutput`으로 캡처(printf+fflush라 파일이 라이브로 읽힘).
- **v1 미스**: 데스크탑 축소 시 비최대화 창은 재배치/재핏하지 않음(화면 밖으로
  나갈 수 있음); 저장된 복원 rect는 유효. 이전 §7 제한 1행(재최대화 없음)은 해소.
