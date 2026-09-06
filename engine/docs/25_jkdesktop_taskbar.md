# 25. jkdesktop 작업 표시�(Taskbar) 구현 계획 (Phase 1–3)

> **[2026-09-06 결정] 작업 표시줄은 B안(전용 클라이언트 앱)으로 확정 — 현행 스펙은 docs/28.
> 이 문서는 A안(서버 오버레이) 계획의 기록으로 보존하며, §A 레이아웃 수치(하단 바 40pt,
> fit-scale 분모/클램프)와 §C 최소화 분석(`visible_` 플래그 재사용)은 B안이 그대로 계승했다.**

> 서브에이전트가 코드 전체(JKWindowServer.cpp 1221줄, JKCompositor.cpp 298줄,
> JKClientConnection.cpp 147줄, JKWireProtocol.h, JKCompositorLayer.h)를 검토한 뒤
> 작성한 계획. 라인 번호는 작성 시점 기준.

## (A) 레이아웃/배치 결정

**하단 바(Windows식)를 권장.** 근거:
- 기존 런처 아이콘 행은 좌상단 `{50 + i*100, 50, 64, 80}` 논리좌표(JKWindowServer.cpp:1030, 962-971)이며,
  상단 배치 시 이 행과 겹치거나 재배치 비용이 발생. 하단으로 분리하면 Phase 1에서 런처 무변경 가능.
- 레이어 초기 위치는 `ProcessPendingClients`(JKWindowServer.cpp:232-243)가 `wh - dispH`로 클램프하므로
  하단 여백만 확보하면 캐스케이드가 자연히 작업 표시줄 위에 안착.
- 창 이동 그랩의 y 클램프가 `winH - 40`(JKWindowServer.cpp:445)이라 하단 바 아래로 창을 끌어내리는 것도 자연 차단됨.

설계: `kTaskbarHeight = 40` (논리 pt), 전체 폭. 논리→물리 변환은 런처와 동일하게 `outputScale` 곱셈 방식
(DrawLauncherBackground 1062, HitTestLauncherIcon 1123-1137) 재사용. `UpdateOutputBounds`(785-800)에서
물리 크기가 변하므로 작업 표시줄 rect는 매 프레임 계산이 안전.

**1280×720 레이아웃 변화**:
1. fit-scale 계산의 분모를 `wh - kTaskbarHeight`로 변경(232-234) — 전체 바탕화면 크기 레이어가 작업 표시줄을 덮는 것을 방지.
2. 초기 y 클램프를 `wh - dispH - kTaskbarHeight`(243)로.
3. 런처 행은 Phase 1 그대로 상단 유지, Phase 3에서 "시작 영역"으로 편입 검토.

## (B) 작업 표시줄 구성요소

- 버튼당 항목: 레이어 id, 표시 텍스트, 활성 하이라이트, (Phase 2) 최소화 상태.
- **텍스트 소스**: 초기 타이틀은 이미 와이어에 존재 — `SurfaceCreatePayload.title[128]`(JKWireProtocol.h:44-48)
  → `JKClientConnection::CreateSurface`가 `title_` 저장(JKClientConnection.cpp:28) →
  `AddLayer(id, w, h, client->Title(), ...)`로 레이어에 보관(JKWindowServer.cpp:248-253, JKCompositor.cpp:44).
  즉 **Phase 1은 `layer->Title()`만으로 표시 가능**. 없는 것은 "타이틀 변경" S→C/C→S 메시지뿐.
  미니 아이콘은 서버에 surface↔.jkx 매핑이 없으므로(SpawnClient와 접속이 비동기, JKWindowServer.cpp:1139-1218)
  Phase 1은 색상 사각형 폴백(1059-1105의 이름 기반 색 방식 재사용), Phase 3에서 spawn 시 appName 전달.
- 활성 하이라이트: `focusedClientId_`(JKWindowServer.h:106)와 레이어 id 비교.
  정렬은 `SortLayers`(JKCompositor.cpp:182-197)가 이미 focused-last 보장.
- 클릭 동작(Phase 1): `FocusClient(id)`(JKWindowServer.cpp:778-783) → z-order raise 포함.

## (C) 최소화 기능 갭 분석과 권장 단계

갭 확인: 서버에는 close 오버레이+리사이즈만 존재. 그러나 **`JKCompositorLayer`에 `visible_` 플래그와
`SetVisible`이 이미 구현됨**(JKCompositorLayer.h:48-49)이고, `HitTest`(JKCompositor.cpp:282),
`Composite`(218), `TopmostLayerId`(152)가 모두 `!IsVisible()` 레이어를 건너뜀.
즉 **최소화 = 레이어 숨김은 거의 공짜** — JKCompositor에 public 패스스루 `SetLayerVisible(id, bool)`만 추가하면 됨.

권장 단계:
- **Phase 1: 클릭=포커스만.** 최소화 없음. 와이어 변경 0.
- **Phase 2: 서버 전용 최소화.** 작업 표시줄 버튼 재클릭 → 토글. `visible=false` + `capturedClientId_` 정리 +
  포커스된 창이 최소화되면 `CleanupDisconnectedClients`(891-895)의 refocus 로직 복제
  (`TopmostLayerId`는 이미 숨김 레이어를 건너뛰므로 안전). 와이어 변경 0 — 클라이언트는 자신이
  최소화됐는지 몰라도 됨(계속 렌더링, shm 커밋만 무시됨).
- **Phase 3: `SetTitle` 메시지(새 MsgType=11, C→S, surfaceId+char[128])** + 클라이언트
  `JKWindow::SetTitle`(src/JKWindow.cpp:25)에서 전송. 그룹핑은 appName 매핑과 함께.

## (D) 단계별 구현 매핑

### Phase 1 (파일 2개 + 헤더)
1. `JKCompositor`: 버튼 목록 조회 API — `std::vector<LayerInfo> GetLayerInfos()`
   (layersMutex_ 하드, JKCompositor.cpp 159-171 패턴). z순서는 정렬된 layers_ 벡터 순서 그대로.
2. `JKWindowServer::Composite`(845-863) 재구성: `compositor_->Composite()`가 `SDL_RenderPresent`(JKCompositor.cpp:243)를
   호출하므로 **작업 표시줄은 레이어 위에 그려야 함** → (권장) `JKCompositor::Composite()`에서 Present를 제거하고
   `JKWindowServer::Composite`에서 `DrawLauncherBackground → CompositeLayers() → DrawTaskbar() → SDL_RenderPresent` 순으로.
3. `DrawTaskbar()`/`HitTestTaskbar(mx,my)` 신규(JKWindowServer.cpp, DrawLauncherBackground 1056 / HitTestLauncherIcon 1123 옆).
   **서버에는 텍스트 렌더러가 없음(1086 주석)** — 권장: Phase 1은 색 블록+활성 테두리만, 텍스트는 Phase 3(8x8 비트맵 폰트 또는 SDL_ttf).
4. `HandleSDLEvent`(609): MouseDown에서 `HandleChromeGrab`(642) 통과 직후, `TryChromeGrab`(645) **앞에**
   `if (HitTestTaskbar(...)) { 포커스/복귀 처리; return; }` 삽입. 작업 표시줄이 시각적으로 최상위이므로 클라이언트 히트테스트(664)보다 우선.
5. `CleanupDisconnectedClients`(865-903): 레이어 제거 시 버튼은 GetLayerInfos가 레이어 기반이라 **자동 동기화**
   (별도 버튼 목록 유지 불필요 — 이 설계가 핵심).
6. `ProcessPendingClients`(232-243): fit/y 클램프에 kTaskbarHeight 반영.

### Phase 2
`JKCompositor::SetLayerVisible` + 작업 표시줄 클릭 토글 + `focusedClientId_` refocus 복사. `TopmostLayerId` 무변경.

### Phase 3
`include/ipc/JKWireProtocol.h`(MsgType 11 + payload), `src/JKWindow.cpp` 전송,
`ProcessClientMessage`(JKWindowServer.cpp:814-843)에 `SetTitle` 분기 → `layer->Title()` 갱신
(JKCompositor에 SetLayerTitle 추가). 런처→작업 표시줄 통합, 그룹핑.

## (E) 리스크

1. **입력 우선순위**: 작업 표시줄 체크를 TryChromeGrab보다 늦게 넣으면 레이어가 작업 표시줄을 덮을 때 chrome이 먼저 먹음
   → 반드시 645 앞에 삽입. 단 HandleChromeGrab(642)은 드래그 중이므로 그 뒤에 유지(드래그 중 작업 표시줄 통과 허용).
2. **리사이즈 64x48 클램프**(JKWindowServer.cpp:457-458, 483-484, 496-499): 초소형 창도 버튼은 유지, 버튼 최소폭 확보 필요.
3. **fit-scale 레이어가 작업 표시줄 덮음**: (A)의 fit 분모 수정으로 해결. 기존에 이미 뜬 창은 드래그 클램프(445)만으로는 부족 —
   Phase 1에서 신규 창만 적용해도 실용적.
4. **outputScale/다중 모니터**: 모든 rect를 논리 pt 저장 후 `*s` 변환(런처 패턴 준수).
   `SDL_WINDOWEVENT_DISPLAY_CHANGED`(610-615)에서 UpdateOutputBounds가 이미 호출되므로 자동 재계산.
   물리 px 혼용 금지(852-856 주석의 D3D/OpenGL 드리프트 회귀 재발 위험).
5. **Present 순서 변경**: JKCompositor::Composite에서 Present 제거 시 다른 호출부 유무 확인
   (현재 유일한 호출은 JKWindowServer.cpp:862).
6. **최소화 중 disconnect**: `RemoveLayer`(JKCompositor.cpp:55-74)는 focusedId_를 0으로 만들고 refocus가
   TopmostLayerId(숨김 제외)로 복구 — Phase 2에서 이 경로 테스트 필수.

## (F) 검증 방법

기존 smoke 스크립트 패턴 준수:
1. 서버 모드에서 minesweeper+tetris 동시 스폰 → 버튼 2개, 클릭 포커스 전환, z-order/키 입력 전환(테트리스 방향키).
2. 활성 버튼 테두리가 focusedClientId_ 추적 확인.
3. 클라이언트를 작업 관리자로 종료 → 버튼 즉시 소멸(레이어 기반 자동 동기화).
4. 1920x1080 표면 스폰 → 작업 표시줄 미덮힘.
5. 작업 표시줄 위에서 클라이언트 드래그 → 드래그 유지, 클릭은 작업 표시줄 승리.
6. 150% DPI 모니터에서 히트테스트 일치.
7. Phase 2: 최소화→복귀→포커스 복구, 최소화 상태로 프로세스 kill.