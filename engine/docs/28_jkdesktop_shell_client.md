# 28. jkdesktop 셸 클라이언트 (작업 표시줄, B안)

> 작업 표시줄을 **전용 클라이언트 앱**(jkwindow 기반)으로 구현하는 B안 스펙.
> 서버(JKWindowServer/JKCompositor)는 "어리석은 컴포지터"로 남는다 — 텍스트/UI 렌더링 없이
> 셸 레이어를 최상위에 유지하고 자리를 비켜줄 뿐. UI는 모두 클라이언트가 그린다.
> 결정: 2026-09-06 B안 확정 (A안 기록은 docs/25 — §A/§C 수치를 이 문서가 계승).

## 셸 역할 (Shell role)

- 클라가 접속 직후 `ShellRegister`(C→S)를 보낸다. **활성 셸은 1개**(첫 등록자 승인,
  이후 등록은 `ShellRegisterAck{accepted=0}`로 거부). 로컬 파이프 신뢰 모델이라 인증 없음.
- 셸 레이어 특권/제약 (서버가 기계적으로 적용, 셸이 무엇을 그리는지는 모른다):
  - **항상 최상위**: `JKCompositor::SortLayers` 비교자 — 비셸 < 셸. focused 창도 셸을 못 덮음.
  - **크롬 면제**: `DrawCloseOverlay` 스킵, `TryChromeGrab` 스킵(닫기 X/타이틀 드래그/리사이즈 없음).
  - **포커스 미인가**: 스폰 시·클릭 시 `FocusClient` 스킵. 키 입력은 앱 창으로 간다.
    `TopmostLayerId` 폴백 refocus도 셸 스킵.
  - **창 목록 제외**: 셸 자신은 WindowList 스냅샷에 안 나옴.
- 클라 UI는 `WA_CHROMELESS` 메인 윈도우. 셸은 런처 그리드에 안 뜨는 **셸이지 앱이 아님** →
  **.jkx로 패키징하지 않음**(ScanJkxApps 오염 방지).

## 프로토콜 (`include/ipc/JKWireProtocol.h`)

| MsgType | 방향 | payload | 용도 |
|---|---|---|---|
| `ShellRegister = 11` | C→S | `{protocolVersion, dockEdge, barHeight}` | 셸 역할 등록 + 도킹 정보(0=하단, 40pt) |
| `WindowList = 12` | S→C | `{count, ShellWindowEntry windows[32]}`, entry = `{surfaceId, flags, title[128]}` | **스냅샷 전체 교체**(델타 아님) — idempotent, taskbar 재시작 후 자가 복구. flags: bit0=active(`kShellWindowActive`), bit1=minimized(`kShellWindowMinimized`) |
| `WindowActivate = 13` | C→S | `{surfaceId}` | 버튼 클릭 → 포커스(+최소화 상태면 복귀, 단계 4) |
| `ShellRegisterAck = 14` | S→C | `{accepted}` | 승인(1)/거부(0) — 거부된 taskbar는 WindowList를 받지 못해 inert |
| `WindowMinimizeToggle = 15` | C→S | `{surfaceId}` | (단계 4) active 버튼 재클릭 → 표시/숨김 토글 |

- **WindowList 발행 시점(push-on-change)**: 클라 접속(ProcessPendingClients), 클라
  disconnect(CleanupDisconnectedClients), WindowActivate 처리 후, 셸 등록 시. 폴링 없음.
- 타이틀 소스: `layer->Title()`(= `SurfaceCreatePayload.title[128]`). OSC 0 실시간 타이틀은
  `SetTitle` C→S로 별도 확장.
- 클라 수신 경로: `JKClientSurface` ReadLoop가 최신 스냅샷을 coalesce(`pendingWindowList_`,
  `pendingResize_` 패턴) + `JKEventType::WindowListChanged` 큐잉 → 앱은 이벤트마다
  `GetWindowList()`로 사본을 뽑아 버튼 재구성.

## 도킹 / 작업 영역

- 셸 표면 = 바 그 자체. **바 높이 = 셸 표면 높이**(클라가 정한다, v1=40), 폭 = 데스크톱 논리 폭.
- `DockShellClient`(서버): 표면 폭 ≠ 데스크톱 폭이면 `CommitChromeResize` 3단계
  (BeginResizeSurface → ResizeLayer → ResizeSurface 송신)로 리사이즈 후 `(0, wh-h)` 배치.
  데스크톱 크기 변경(SIZE_CHANGED/MOVED/DISPLAY_CHANGED → `UpdateOutputBounds`)마다 재도킹.
- **작업 영역 예약** (`ShellReserveHeight()` = 셸 레이어 표시 높이, 없으면 0):
  - 새 창 배치(ProcessPendingClients): fit-scale 분모 `wh - reserve`, y 클램프 `wh - reserve - dispH`.
  - 타이틀 드래그 y 클램프: `winH - reserve - 40`.

## 자동 스폰

- `StartAcceptor` 말미: acceptor의 첫 파이프 인스턴스 생성 대기(WaitNamedPipe, 상한 ~200ms —
  클라 Connect에 재시도가 없어서 파이프가 반드시 먼저 존재해야 함) → exe 옆 `jkapp_taskbar.dll`
  존재 확인 → `SpawnClient("taskbar")` (= `jkdesktop.exe --client taskbar`).
- DLL이 없으면 로그만 남기고 셸 없이 운영. v1 재스폰은 생략(셸이 죽으면 로그만).

## UI (`ClientTaskbarApp` — 단계 3, 구현 완료)

- `TaskbarWindow`(`JKWindow` 서브클래스): `OnPaintClient`를 어두운 배경(24,26,32)으로
  대체 — `JKWindow`는 밝은 회색(240)으로 하드코딩되어 있어서.
- **버튼 풀**: `TaskbarButton : JKButton` 32개를 OnInit에서 미리 생성(= WindowListPayload
  상한). 스냅샷마다 in-place 재바인딩(`Bind`) — 컨트롤 해체/재생성이 없어 플리커 없음,
  프레임워크에 remove-control API 추가 불필요. count를 넘는 버튼은 Hide.
- 버튼 페인트: 타이틀 FNV-1a 해시색 칩(10px) + 제목 텍스트(`Utf8ToKssm` → `dc.TextOutX`,
  Y센터 정렬, 넘치면 클립). 상태: **active** = 밝은 면 + 흰 테두리 / 보통 = 중간 회색 /
  **minimized** = 어두운 면 + 흐린 텍스트(단계 4).
- 클릭(`JKButton::OnClick`) → `SendWindowActivate(surfaceId)`.
- **오버플로 규칙(최소 너비 보장형 동적 축소)**: 가용 폭 ÷ 창 수로 버튼 폭 분배, 상한 180px,
  하한 40px 아래로는 축소 중단. 스크롤/그룹화는 v2.
- 도킹 리사이즈(`SizeChanged`)마다 `Relayout()`으로 재배치.
- 셸이므로 타이머/편집컨트롤/IME 불필요 — 이벤트 구동 only.

## 최소화 (단계 4, 구현 완료)

- `WindowMinimizeToggle`(15) → 서버가 레이어 가시성 토글(`SetLayerVisible(id, !visible)`,
  docs/25 §C의 `visible_` 재사용 — 클라는 계속 렌더+커밋하고 서버가 그리기만 스킵).
- 숨김으로 포커스를 잃으면 `FocusClient(TopmostLayerId())`(disconnect 폴백과 동일 경로).
- `WindowActivate`는 숨겨진 창을 먼저 보이게 한 뒤 포커스(restore-on-activate).
- `PushWindowList`가 minimized 플래그(bit1)를 레이어 가시성에서 채움 — 버튼 딤 표시.
- UI: **active 버튼 재클릭** → `WindowMinimizeToggle`, 그 외 클릭 → `WindowActivate`.

## 구현 상태

- [x] 단계 1 — 프로토콜 레이어 (와이어 11-14 + 클라 전송/수신 경로 + 서버 핸들러 + taskbar 골격)
- [x] 단계 2 — 컴포지터/도킹 (SortLayers/크롬·포커스 면제 + 작업영역 예약 + DockShellClient + 자동 스폰)
- [x] 단계 3 — UI 레이어 (TaskbarButton 풀 + 버튼 페인트 + 오버플로 규칙)
- [x] 단계 4 — 최소화 (MsgType 15 + SetLayerVisible 토글 + restore-on-activate)
- [ ] 단계 5 — 문서 정리 (docs/19 메시지 표)