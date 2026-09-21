# 의미 커서(semantic cursor) v1 Implementation Plan — 지뢰찾기 첫 소비자

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 앱 선언형 의미 커서 계약(docs/superpowers/specs/2026-09-22-semantic-cursor-design.md) v1 구현 — 지뢰찾기가 `move/act/read` 3종 도구로 폰에서 조작되고, 승인 대상이 셀 단위 하이라이트된다.

**Architecture:** 서버가 `AgentToolRegister`의 `cursor` 블록(선언)을 해석해 `<app>.move`/`<app>.read`는 플랫폼이 구현하고 `<app>.act`는 기존 app_tool 릴레이로 앱에 전달. 커서 상태는 서버 소유(free-cursor). 하이라이트는 기존 서버 오버레이 경로에 2계층(커서 셀/승인 셀) 추가.

**Tech Stack:** C++17/MinGW, jkserver static lib, 앱 도구 허브(docs/58) 재사용, PS5.1 프로브.

**Spec:** docs/superpowers/specs/2026-09-22-semantic-cursor-design.md (계약의 유일 권위 — 각 태스크는 이 문서의 항을 인수로 삼는다)

## Global Constraints

- **외부 의존 0**, 수기 인코더 관례 유지.
- **앱 도구 허브 기존 배관 최대 재사용** — 신규 와이어 메시지 금지(AgentToolRegister JSON 확장만), 브로커(jkagentd)는 generic 릴레이라 무수정이 지향점(수정 필요하면 스펙 위반 아님을 보고).
- **게이트**: `<app>.move`/`<app>.read` = none→allow(kPermMatrix window_move 선례), `<app>.act` = 기존 app_tool ask 파킹(재실행형+승인 시점 재검증).
- **LLM 인자는 row/col 인덱스** — 좌표계는 client 단일(`coordSpace` 필드 명시), 스크린 좌표 노출 금지.
- **에코 규약**: 모든 위치성 도구 응답에 `{row, col}`(상태 직렬화), 불법 전이는 `bad_state`, act 에코에 `kind/row/col/opened/status`.
- **파킹 시점 셀 rect 고정** + 승인 시점 재선언 재검증(어긋나면 거부).
- **shell/캡처 오버레이 배제**, 하이라이트는 비상호작용(클릭 통과), 매 프레임 실시간 계산.
- **설정 파일 런타임 무접촉** — 프로브 접촉 시 백업+바이트동일 복원+콘솔 고지(§16.1 룰).
- **프로브 ×2 연속 PASS**, `> log 2>&1` 파일 리다이렉트, PS5.1 ASCII-only, 빌드=ninja(라이브 점유 시 정지→빌드→재기동 사이클 기록).
- **커밋 규약** — 한국어 컨벤셔널+`Co-Authored-By: Claude Code <noreply@anthropic.com>`, 파일별 add(절대 -A).
- **문서 = 산출물** — as-built는 docs/64 신설.

---

### Task 1: 서버 선언 파싱 + 커서 상태 + move/read 플랫폼 도구

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` (AgentToolRegister 처리 :2057 근처 — cursor 블록 파싱; app_tool 릴레이 분기 :3040-3125 근처 — move/read 인터셉트), `engine/include/server/JKWindowServer.h` (선언+커서 상태 멤버)
- Create: `engine/src/server/JKSemanticCursor.h` + `engine/src/server/JKSemanticCursor.cpp` (순수 로직 — 스펙 §4 상태 머신: 격자 클램프/스텝 첫 경계 중단/에코 산출; 뷰 무관)
- Test: 빌드 + ninja

**Interfaces:**
- Consumes: AgentToolRegister JSON(이미 type 22로 수신 — cursor 블록 추가 파싱), 앱 도구 허브 레지스트리(docs/58 — 도구 합성 등록 지점)
- Produces: `<app>.move`/`<app>.read` 플랫폼 구현 도구(app_tool 릴레이에서 인터셉트, 앱 미관여) + 커서 상태(초기 0,0) + `SemanticCursorFor(app)` 조회(하이라이트 Task 3이 소비). JKSemanticCursor 순수 API: `MoveTo/MoveRelative/RunSteps → Result{row,col,error}` + `Rows/Cols`

- [ ] **Step 1: JKSemanticCursor 순수 클래스** — 격자+커서+move 규약(절대/상대/다중 스텝 상한 32/첫 경계 중단/도달 에코)만. 에러 코드: `bad_args`(음수/과대), `bad_grid`(선언 격자 밖). 뷰/서버 의존 0.
- [ ] **Step 2: 선언 파싱** — AgentToolRegister의 `cursor` 블록 파싱(type/coordSpace/origin/cellW/cellH/rows/cols/cursorOwner/act.kinds/act.gate). cursorOwner=="app"은 v1 미지원 선언 거부(스펙 §8) — 구현대로 응답. 창 닫힘 시 선언+커서 소멸(허브 자동 제거 룰에 편승).
- [ ] **Step 3: 도구 합성** — 선언 존재 시 레지스트리에 `<app>.move`(none-allow)/`<app>.read`(none-allow)/`<app>.act`(앱 릴레이, ask) 등록 — 이름 규약은 허브 네임스페이스 선례. tools/list 스키마에 act.kinds **enum** 반영.
- [ ] **Step 4: move/read 구현** — move=JKSemanticCursor 상태 갱신+에코. read=앱 `snapshot` app_tool 릴레이 결과에 커서 헤더 조립(앱 snapshot 실패 시 커서만 응답 — 부분 성능 저하 아닌 명시 에러). move는 JKSemanticCursor 프로브 가능 형태로.
- [ ] **Step 5: 빌드+커밋** — `feat(server): 의미 커서 선언 파싱+move/read 플랫폼 도구`

### Task 2: act 릴레이 + 승인 배너 셀 표기

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` (act 릴레이 조립+파킹 kind app_tool 승계; DrawApprovalHighlights 배너 문구 :6140 근처)
- Test: 기존 승인 프로브 하네스 재사용

**Interfaces:**
- Consumes: Task 1의 선언+커서, app_tool 파킹 파이프라인(승인=원 요청 재실행+승인 시점 게이트/선언 재검증)
- Produces: 파킹 시 배너 문구 `<app>.<kind> at (r,c)`; 파킹 레코드에 셀 rect 고정(승인 하이라이트용)

- [ ] **Step 1: act 릴레이** — LLM 호출 `{tool:"minesweeper.act", args:{kind,row,col}}` → 앱 `act` app_tool 호출로 릴레이(kind/row/col 원문). kind가 선언 enum 밖이면 앱 도달 전 `bad_args`.
- [ ] **Step 2: 배너 문구** — app_tool 파킹의 name 구성 확장: 선언 있으면 `<app>.<kind> at (r,c)`, 없으면 기존 `<app>.<tool>`(vplayer류 무선언 앱 무변화 회귀 보장).
- [ ] **Step 3: 셀 rect 고정** — 파킹 시 최신 선언으로 셀 rect 산출·저장. 승인 시점 재선언 검증: rect 재산출→불일치/격자 밖=거부.
- [ ] **Step 4: 빌드+커밋** — `feat(server): 의미 커서 act 릴레이+승인 배너 셀 표기`

### Task 3: 하이라이트 렌더 (커서 셀 + 승인 대상 셀)

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` (렌더 경로 :250 호출 지점, DrawApprovalHighlights :6140 근처에 셀 렌더 추가 또는 신설 함수)
- Test: 렌더는 Task 5 프로브의 파킹 하이라이트 경유 실측(기존 승인 링 프로브 선례)

**Interfaces:**
- Consumes: Task 1 `SemanticCursorFor`, Task 2 파킹 셀 rect
- Produces: 커서 셀 지속 표시(액센트색 테두리, 선언 앱+레이어 보일 때만), 승인 대상 셀 호박 링(기존 창 링 격침)

- [ ] **Step 1: 커서 셀 렌더** — 선언 앱의 커서 위치를 매 프레임 client→layer(×Scale)→물리(×outputScale) 변환으로 테두리 셀 표시. 창 이동 추종(실시간 계산), 비상호작용. 커서 미이동 시에도 (0,0) 표시(상태 존재=표시).
- [ ] **Step 2: 승인 셀 렌더** — 파킹된 act의 고정 rect에 기존 호박 3중 링 산식 재사용. 배너는 Task 2 문구.
- [ ] **Step 3: 빌드+커밋** — `feat(server): 의미 커서 셀 하이라이트 — 커서/승인 2계층`

### Task 4: 지뢰찾기 클라 — 선언 발행 + act/snapshot 핸들러

**Files:**
- Modify: `engine/src/apps/ClientMineSweeperApp.cpp` (스폰 직후 SendAgentToolRegister에 cursor 블록+도구 선언 — ClientVPlayerApp.cpp:1687 선례), `engine/src/apps/MineSweeperApp.cpp` (act/snapshot 로직 — 플러드 필 :302 기존)
- Test: Task 5 프로브

**Interfaces:**
- Consumes: 앱 도구 호출 수신 경로(vplayer/워크숍 선례), JKWireProtocol WriteAgentToolRegister
- Produces: app_tool `act {kind,row,col}` → `Result{opened, status, error}`; app_tool `snapshot` → 보드 텍스트(열린 숫자/깃발/status, 패배 시 지뢰 전체 공개) + `new_game` 리셋 수용

- [ ] **Step 1: 선언 발행** — cursor 블록(spec 예시 값, 좌표는 구현 실측 — minesweeper 격자 원점/칸 크기는 소스에서 산출)+tools [act, snapshot]. act.gate="ask", kinds에 reset 포함.
- [ ] **Step 2: act 구현** — reveal(플러드 필 내포, opened 반환)/flag/question(3상 마크 토글)/clear/reset. 유효 전이 검사(열린 칸 재개방=bad_state, 폭발=boom+status lost, 완개방=won). 커서는 플랫폼 소유라 앱은 좌표 인자만 소비.
- [ ] **Step 3: snapshot 구현** — 9줄 텍스트(닫힘=#/숫자/깃발=F/물음표=?)+status. 패배 시 지뢰 전체 공개 직렬화.
- [ ] **Step 4: 빌드+커밋** — `feat(apps): 지뢰찾기 의미 커서 선언+act/snapshot 핸들러`

### Task 5: probe_semantic_cursor 신설 + 회귀

**Files:**
- Create: `engine/tools/probes/probe_semantic_cursor.ps1` (probe_app_tools harness 재사용)
- Test: ×2 + 기존 회귀

- [ ] **Step 1: 시나리오** — ①선언→tools/list에 minesweeper.move/read/act 노출 ②move 절대/상대/스텝(첫 경계 중단+도달 에코)/클램프 ③read 직렬화(커서+보드) ④act 파킹→배너 셀 문구→승인→opened 에코+판 변화 ⑤bad_state(열린 칸 재개방) ⑥boom(폭발 칸 지정)→lost+지뢰 공개 ⑦reset→커서 (0,0) ⑧생략/과대 인자 bad_args. ×2 연속 PASS.
- [ ] **Step 2: 회귀** — probe_app_tools, probe_workshop, probe_agent_events, probe_window_geom, jkdesktop test ×2.
- [ ] **Step 3: 커밋** — `test(probes): probe_semantic_cursor 신설+회귀`

### Task 6: docs/64 as-built + 라이브 반영 + 메모리

**Files:**
- Create: `docs/64_semantic_cursor.md` (as-built — 스펙과의 편차+실측+레슨)
- Modify: `docs/60_jkworkshop.md`(§5 백로그에 스크립트 앱 declareCursor 후보 표기 — 선택)

- [ ] **Step 1: 회귀 ×2 → 라이브 반영** — 데스크탑 정지→ninja 풀빌드(링크 0)→재기동(서버+taskbar)→jkbridge 재기동(pong/http 200, 토큰 불변)→프로브 ×2.
- [ ] **Step 2: docs/64 작성+커밋** — `docs(64): 의미 커서 v1 as-built`
- [ ] **Step 3: 사용자 눈확인 항목 기록** — 폰 실전 "지뢰찾기 켜 줘 → 지금 어디? → 가운데 열어 줘 → 거기 깃발"(맨 뒤 대기열에 편입)