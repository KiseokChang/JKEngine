# 창 최대화/복원 — 서버 크롬 최대화 버튼 + 제목바 더블클릭 토글

- 날짜: 2026-09-13
- 상태: 설계 확정 (사용자 위임 진행 — "좋아요 주욱 가줘요". 사용자 질문 "SDL창 최대화
  가능한지"에서 기원 — 앱 창은 SDL 창이 아니라 합성 레이어, 크롬에 닫기 X만 있어
  최대화 부재. 결정은 컨트롤러 재량)
- 선행: docs/28 (크롬/작업영역), docs/35 (오버레이 크롬 면제 선례)
- 후속 문서: 구현 완료 시 `docs/39_window_maximize.md`

## 1. 모델

앱 창(합성 레이어)의 최대화 = **서버가 레이어를 작업영역 크기로 리사이즈 + (0,0) 배치**.
기존 `CommitChromeResize`(드래그 리사이즈/오버레이/taskbar 도킹이 쓰는 서버-지시
라운드트립)와 `SetLayerPosition`의 재조합 — 클라 프로토콜 변경 없음 (ResizeSurface는
이미 존재). 클라 앱은 리사이즈에 이미 대응(드래그 리사이즈가 같은 경로).

**크롬 구성 (추가)**: 닫기 X 왼쪽에 최대화/복원 버튼 20×20 (gap 2px) —
`kChromeMaximizeSize=20`, `kChromeMaximizeGap=2`. 버튼 x0 = `w − margin − closeSize −
gap − maxSize`. 그리기는 `DrawCloseOverlay` 옆 `DrawMaximizeButton`: 동일 박스
(192,192,192 fill + black outline), 글리프는 흰선 — 최대화 = 내부 흰 외곽선 사각형
(RenderDrawRect pad 5), 복원 = 겹친 사각형 두 개 (앞 사각형 + 뒤 사각형 윤곽).
DrawCloseOverlay의 셸/오버레이 면제 가드를 공유 (docs/28/35).

## 2. 상태와 토글

- **서버 상태**: `std::map<uint32_t layerId, MaxState>` — `MaxState{x, y, surfW, surfH,
  dispW, dispH}` (최대화 직전 rect). 존재 = 최대화 중.
- **최대화**: 현재 rect 저장 → 작업영역 크기로 `CommitChromeResize(id, ww, wh−reserve,
  ww, wh−reserve)` + `SetLayerPosition(0, 0)`. fit 스케일 해제 (surface == 표시 크기,
  scale 1).
- **복원**: `CommitChromeResize(id, surfW, surfH, dispW, dispH)` + `SetLayerPosition(x, y)`
  + 상태 제거. (ResizeLayer가 scale을 1로 리셋하므로 dispW != surfW면
  CommitChromeResize가 스스로 스케일 복원 — 기존 동작.)
- **연결 종료 시 상태 제거** (레이어 제거 경로와 함께) — 댕글링 방지.
- 데스크탑 크기가 바뀌면(모니터 해상도 변경) 최대화 중인 창은 다음 SIZE_CHANGED에서
  재최대화 — v1은 놓침 허용(복원 rect는 정확, 크기만 옛값; 문서로 명시).

## 3. 상호작용

- **버튼 클릭**: TryChromeGrab에 닫기 존 다음(리사이즈 엣지 앞) 최대화 존 추가 —
  `inMaxX = lx ∈ [w − margin − closeSize − gap − maxsize, w − margin − closeSize − gap)`
  + inCloseY 동일. 히트 → FocusClient + PushWindowList + 토글.
- **호버 커서**: 버튼 존은 닫기와 동일하게 화살표 (UpdateChromeHoverCursor에 동일
  면제 조건 추가).
- **제목바 더블클릭**: 제목바 클릭에서 `ev.button.clicks == 2`면 이동 그랩 대신 토글
  (Windows 관례). 버튼 존 위 더블클릭은 버튼 로직이 먼저 먹는다.
- **드래그로 복원**: 최대화 중 Move/Resize 그랩 시작 시 자동 복원 후 그랩 (Windows
  관례). Move는 복원 rect 기준으로 시작, Resize는 복원 크기에서 드래그.
- **최소화는 제외** (taskbar WindowMinimizeToggle이 이미 담당), 스냅-반쪽도 스코프 밖.

## 4. 이벤트 + taskbar

- 토글 시 `window.maximized` / `window.restored` 발행 (id/title 최상위 — 서버 내부
  이벤트 관례). events_list 정적 카탈로그에 2행 추가 (source server).
- WindowList 페이로드 변경 없음 (V1 — taskbar 버튼 텍스트에 최대화 표시는 후속 여지).

## 5. 테스트

- **`probe_agent_maximize.ps1`** (신규): 서버+채팅 기동 → minesweeper 스폰 →
  list_windows로 rect 추정 → SendInput으로 최대화 버튼 합성 클릭
  (docs/35 레슨: DPI 가상화 공간 1:1, ClientToScreen) → `window.maximized` 실측 +
  layer rect가 작업영역(ww×(wh−reserve)) 확인 → 재클릭 → `window.restored` +
  원 rect 복원 → 제목바 더블클릭 토글 실측 → cleanup.
- 회귀: jkdesktop test 0, 기존 프로브 전부 (mcp/e2e/palette/chat/chat_llm/triggers/
  trust/ratelimit/events/notify/shot/triggerctl).

## 6. 제한

- 최대화 중 서버 창 리사이즈 대응은 v1에서 놓침(다음 배치) — 복원 rect는 유효.
- 최대화 중 앱이 고정 표면이면? — 표면이 작업영역으로 늘어나므로 앱 레이아웃이
  리사이즈를 이미 지원해야 함 (드래그 리사이즈와 동일 요구 — 신규 리스크 없음).
- 크롬은 표면 px 기준이라 fit 스케일에서 버튼 히트/그리기가 비례 축소 — 기존 닫기 X와
  동일 특성.