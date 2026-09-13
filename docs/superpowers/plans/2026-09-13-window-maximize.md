# 창 최대화/복원 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 앱 창(합성 레이어)의 서버 크롬에 최대화/복원 버튼 + 제목바 더블클릭 토글을 추가 — 기존 CommitChromeResize 라운드트립 재사용, 클라 프로토콜 변경 0.

**Architecture:** 서버가 레이어별 최대화 직전 rect를 보관(map)하고, 최대화 = 작업영역 크기 리사이즈 + (0,0), 복원 = 저장 rect 리사이즈 + 복귀. 그리기는 DrawCloseOverlay 옆에 최대화 글리프(최대화/복원 상태별), 히트는 TryChromeGrab의 닫기 존 다음.

**Tech Stack:** C++20, SDL2, MinGW UCRT64, CMake(engine/build)

**Spec:** docs/superpowers/specs/2026-09-13-window-maximize-design.md

## Global Constraints

- 클라 프로토콜/클라 코드 변경 없음 — 서버만 (ResizeSurface는 이미 존재).
- 셸(docs/28)과 캡처 오버레이(docs/35, kCaptureOverlayTitle)는 최대화도 면제 —
  DrawCloseOverlay와 동일 가드를 공유.
- 최대화 rect 저장은 **연결 종료/레이어 제거 시 반드시 제거** (댕글링 방지).
- 이벤트: `window.maximized`/`window.restored`, id/title 최상위 필드 (서버 내부
  이벤트 관례) + events_list 카탈로그 2행.
- consume/스레드 규율: 레이어 접근은 compositor_ 뮤텍스 경계를 그대로 따름
  (TryChromeGrab이 이미 하는 방식). 새 락 금지.
- 커밋 관례: `feat(window)`/`test(window)` + Co-Authored-By.

---

### Task 1: 서버 — 최대화/복원 코어 + 크롬 버튼 + 상호작용

**Files:**
- Modify: `engine/include/server/JKCompositor.h:19-23` (상수 추가)
- Modify: `engine/src/server/JKCompositor.cpp` (DrawMaximizeButton + ComposeScene 호출부)
- Modify: `engine/src/server/JKWindowServer.cpp` (TryChromeGrab, UpdateChromeHoverCursor, HandleChromeGrab, 클라이언트 제거 경로)
- Modify: `engine/include/server/JKWindowServer.h` (MaxState 멤버 + ToggleMaximize/Restore 선언)
- Modify: `engine/src/server/JKWindowServer.cpp` events_list 카탈로그 (2행)

**Interfaces:**
- Consumes: CommitChromeResize(4인자), SetLayerPosition, ShellReserveHeight, DrawCloseOverlay 가드, kCaptureOverlayTitle
- Produces: `window.maximized`/`window.restored` 이벤트 — Task 2 프로브가 실측

- [ ] **Step 1: 상수 + 그리기** — JKCompositor.h에 `kChromeMaximizeSize = 20; kChromeMaximizeGap = 2;` 추가. JKCompositor.cpp에 `DrawMaximizeButton(const JKCompositorLayer&, float scale)` — DrawCloseOverlay와 동일 박스 도식(192,192,192 fill + black outline), x0 = `(Width() − kChromeCloseMargin − kChromeCloseSize − kChromeMaximizeGap − kChromeMaximizeSize) * ScaleX()`, 글리프: 최대화 = pad 5 흰 `SDL_RenderDrawRect`, 복원 = pad 7 흰 외곽선 + pad 3 흰 외곽선(겹친 두 사각형). 호출부(299 근처)는 DrawCloseOverlay와 동일 가드에서 병행 호출. **최대화 상태 판별**: JKWindowServer가 상태를 소유하므로 Compositor에 상태를 두지 말고 — `layer->Maximized()` 불리언(서버가 토글 시 SetLayerMaximized) 또는 간단히: 그리기 시 서버에서 bool 전달 불가라면 **JKCompositorLayer에 `bool maximizedFlag` 필드** 추가(SetMaximized)가 가장 단순. 선택 후 코드에 근거 주석.
- [ ] **Step 2: 상태 + 토글** — JKWindowServer.h에:

```cpp
// Maximize/restore (docs/39): pre-maximize rect per layer. Presence in the
// map = currently maximized.
struct MaxState { int x, y, surfW, surfH, dispW, dispH; };
std::map<uint32_t, MaxState> preMaxRects_;
void ToggleMaximize(JKClientConnection& client, JKCompositorLayer& layer);
```

ToggleMaximize: 없으면 저장(layer->X/Y + surf = layer->Width/Height + disp = surf*ScaleX/Y) → `const int ww, wh` (SDL_GetWindowSize), reserve → `CommitChromeResize(client, id, ww, wh - reserve, ww, wh - reserve)` + SetLayerPosition(0,0) + layer SetMaximized(true) + `PushAgentEventJson("{\"topic\":\"window.maximized\",\"id\":N,\"title\":\"…\"}")` (기존 PushAgentEvent 사용처 관례 확인 — id/title은 최상위). 있으면 복원: CommitChromeResize(surfW, surfH, dispW, dispH) + SetLayerPosition(x,y) + 제거 + `window.restored` 발행 + SetMaximized(false).
- [ ] **Step 3: 히트 존** — TryChromeGrab의 닫기 존(759-764) 뒤에 최대화 존:

```cpp
// 1b) Maximize/restore button (left of the close X, server-drawn — docs/39).
const int maxBtnX0 = w - kChromeCloseMargin - kChromeCloseSize - kChromeMaximizeGap - kChromeMaximizeSize;
const bool inMaxX = (lx >= maxBtnX0) && (lx < maxBtnX0 + kChromeMaximizeSize);
if (inMaxX && inCloseY) {
    FocusClient(client->Id());
    PushWindowList();
    ToggleMaximize(*client, *layer);
    return true;
}
```

- [ ] **Step 4: 더블클릭 토글** — HandleChromeGrab(또는 TryChromeGrab)의 제목바 이동 그랩 시작 지점(795)에서 `ev.button.clicks == 2`면 ToggleMaximize 후 return (이동 그랩 안 함).
- [ ] **Step 5: 드래그 복원** — Move 그랩 시작(799 전)과 Resize 그랩 시작(776 전)에서 `preMaxRects_`에 layerId 있으면 **먼저 복원** 후 원래 그랩 로직 (rect/좌표는 복원 후 layer에서 다시 읽기).
- [ ] **Step 6: 호버 커서** — UpdateChromeHoverCursor(874-883): inMaxX&&inCloseY도 close와 동일 면제(화살표 유지).
- [ ] **Step 7: 정리** — 클라 제거/레이어 RemoveLayer 경로에서 `preMaxRects_.erase(id)` (연결 끊김 처리 위치 검색: RemoveLayer 사용처).
- [ ] **Step 8: events_list 카탈로그** — `window.maximized`/`window.restored` 2행 추가 (source "server", fields ["id","title"], desc 한국어).
- [ ] **Step 9: 빌드 + `jkdesktop.exe test` 0** + 수동 스모크는 Task 2 프로브가 대행 — 이 커밋 시점엔 프로브 없음(히트 존은 다음 커밋의 SendInput으로 실측).
- [ ] **Step 10: Commit** `feat(window): chrome maximize/restore button + title double-click toggle (docs/39 Task 1)`

### Task 2: probe_agent_maximize.ps1

**Files:**
- Create: `engine/tools/probes/probe_agent_maximize.ps1`

**Interfaces:**
- Consumes: probe_agent_shot.ps1의 SendInput 합성 관례(레슨 48-49: ClientToScreen, DPI 가상화 1:1, ×OutputScale 금지), probe_agent_trust의 기동/정리 관례, agent-events 구독 선기동(레슨 28)

- [ ] **Step 1: 프로브 (6 체크)**:
  1. 기동 → minesweeper 스폰 → list_windows로 id/제목 실측 (rect는 이벤트로 보강)
  2. agent-events 구독 잡 선기동 → SendInput으로 최대화 버튼 합성 클릭(버튼 화면 좌표 = 창 rect에서 계산 — 서버 창 위치 + layer 위치 + 버튼 offset×outputScale 주의: 버튼이 표면 px이므로 fit 스케일 고려; minesweeper는 fit 1:1 크기라 단순) → `window.maximized` 캡처 + id 일치
  3. capture_window로 전체화면 실측 or list_windows가 rect 미제공이면 compositor 직접 불가 — 대신 `window.maximized` 후 이벤트 + 스크린샷(shot)으로 시각 확인 가능하면 보너스 (필수 아님)
  4. 재클릭 → `window.restored` + 원위치(두 번째 maximized 이벤트 없음)
  5. 제목바 더블클릭(2회 연속 클릭) → maximized → 다시 더블클릭 → restored
  6. cleanup: 프로세스 kill, permission 원복
  각 PASS/FAIL + 실패 exit 1, 성공만 exit 0.
- [ ] **Step 2: 전체 회귀** — mcp 5/5, e2e 7/7, palette 4/4, chat 7/7, chat_llm 2/2, triggers 7/7, trust 7/7, ratelimit 7/7, events 5/5, notify 6/6, shot 7/7, triggerctl 5/5 + jkdesktop test 0
- [ ] **Step 3: Commit** `test(window): probe_agent_maximize — button/double-click/restore via SendInput`

### Task 3: 문서화 — docs/39 + 메모리

**Files:**
- Create: `docs/39_window_maximize.md` (docs/38 관례 — 스펙 압축 + 최종 구현 기준)
- Modify: `docs/28` 관련 표기는 불필요 — docs/23 §11 크롬 교훈(15) 언급 시 참조만
- Modify: MEMORY.md roadmap 라인 + jkengine_roadmap.md 문단 (커밋 제외)

- [ ] **Step 1: docs/39 작성** (§1 모델/크롬, §2 상태+토글, §3 상호작용 4종, §4 이벤트+카탈로그, §5 프로브 6체크, §6 제한 — v1 놓침 항목: 모니터 해상도 변경 시 재최대치 않음, 최소화/스냅 제외)
- [ ] **Step 2: 메모리 갱신**
- [ ] **Step 3: Commit** `docs(39): window maximize/restore — chrome button + title double-click`

## Task 의존성

Task 1 → Task 2 (프로브가 Task 1 실측) → Task 3.