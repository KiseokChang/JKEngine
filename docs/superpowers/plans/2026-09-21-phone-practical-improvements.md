# 폰 실전 개선 잔여 4건 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** docs/57 §12.4/§12.5 폰 실전 세션에서 도출된 개선 4건 소각 — args 캡 상향, 창 move/resize 도구, 채팅 LLM 시스템 프롬프트, window.focused 디바운스.

**Architecture:** 서버(JKWindowServer) 도구 2종 신설은 window_fullscreen 선례(docs/50 §11)의 조합 재사용 — 새 메커니즘 없음. JKLlmEngine 프롬프트 주입은 BuildEngineCmd 한 지점. 디바운스는 FocusClient 조건부 push.

**Tech Stack:** C++17/MinGW, jkserver static lib, jkagentd 브로커, PS5.1 프로브.

**Spec:** docs/57_jkbridge.md §12.4-§12.5(도출 배경) + 로드맵 메모리 갱신 8(합의 우선순위+측정 포인터).

## Global Constraints

- **외부 의존 0** — 수기 Win32/수기 인코더 관례 유지.
- **게이트 = kPermMatrix(kToolMatrix) 선례 준수** — 새 도구는 `{"window_move","none","allow"}` 류 행 추가(focus_window/window_fullscreen 분류: 상태 변경이지 승인 행위가 아님).
- **shell(태스크바)·캡처 오버레이 레이어는 도구 대상 배제** — window_fullscreen의 shell 거절(IsShell)과 capture 오버레이 특수 타이틀 예외(kCaptureOverlayTitle)를 그대로 따른다.
- **설정 파일 런타임 무접촉** — 프로브가 permissions.json/state를 다루면 백업+바이트동일 복원+콘솔 고지(§16.1 사건 규칙).
- **프로브 ×2 연속 PASS**, `> log 2>&1` 파일 리다이렉트, PS5.1 ASCII-only(BOM 유의), 빌드는 `PATH=/c/msys64/ucrt64/bin` ninja(라이브 데스크탑이 exe/dll 점유 시 링크 보류 → 정지→빌드→재기동 사이클).
- **커밋 규약** — 한국어 컨벤셔널 커밋, `Co-Authored-By: Claude Code <noreply@anthropic.com>` 트레일러, 파일별 add(절대 -A).
- **문서 = 산출물** — 각 태스크 판정과 why를 docs/57 §13에 기록.

---

### Task 1: args 캡 상향 + window.focused 디바운스 (소형 배치)

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp:3031` (args 캡), `:1751-1764` (FocusClient)
- Test: `engine/tools/probes/probe_app_tools.ps1`, 이벤트 프로브(기존 harness 재사용)

**Interfaces:**
- Consumes: 없음(독립 소형 수정)
- Produces: app_tool args 캡 256KiB(앱 측 256KiB 백스톱과 정렬 — set_script >8KiB 소스 통과), FocusClient는 **바뀐 포커스 id만** window.focused push

- [ ] **Step 1: args 캡** — `JKWindowServer.cpp:3031`의 `argsRaw.size() > 8 * 1024`를 `256 * 1024`로. 주석에 근거 기록(워크숍 set_script 256KiB 백스톱과 정렬 — 8KiB가 앞단 병목이었음, docs/60 §5 백로그). result 16KiB 캡(HandleToolResult)은 **건드리지 않는다**(별도 백로그).
- [ ] **Step 2: 디바운스** — `FocusClient`(1751): 대입 전 `prev = focusedClientId_` 캡처 후 `if (surfaceId != prev)`일 때만 PushAgentEvent. 주석: 무변화 재push가 WS 초당 수회 스팸의 근원(docs/57 §12.4 ⑦). 첫 포커스(prev 미지정)는 push 유지 — spawn 포커스 이벤트 계약 불변.
- [ ] **Step 3: 프로브** — ①probe_app_tools.ps1: args ~10KiB + **존재하지 않는 도구명** app_tool 호출 → 기대 `unknown_app_tool`(구 캡이면 `args_too_large` — 캡 통과의 간접 단정, 무해 호출). ②기존 이벤트 프로브에서 window.focused 단정 유지(스폰 포커스는 prev≠id라 push됨) + 같은 창 재 focus 시 이벤트 증가 0 단정 추가(harness가 있으면; 없으면 probe_agent_events 계열에 신설).
- [ ] **Step 4: 빌드+프로브 ×2** — ninja(라이브 점유 시 정지/재기동 사이클), 프로브 ×2 PASS.
- [ ] **Step 5: Commit** — `feat(agent): app_tool args 캡 256KiB + window.focused 무변화 디바운스`

### Task 2: window_move / window_resize 서버 도구

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp`(kToolMatrix :2121 근처 + HandleAgentQuery window_fullscreen 분기 :2957 근처), `engine/tools/jkagentd/main.cpp`(등록 4곳 — tools/list JSON :57 선례, IsKnownTool :90, 기본 allow :135, 릴레이 분기 :734 선례)
- Test: `engine/tools/probes/probe_window_geom.ps1` (신설)

**Interfaces:**
- Consumes: `JKCompositor::SetLayerPosition`(id,x,y), `CommitChromeResize(client, layerId, w, h, dispW, dispH)`(정규 리사이즈 경로 — BeginResizeSurface→ResizeLayer→Send(ResizeSurface) 순서 보장), window_fullscreen의 대상 선정/거절 뼈대
- Produces: 서버 도구 2종 — `window_move {"id"?:int, "x":int, "y":int}`, `window_resize {"id"?:int, "w":int, "h":int}`. reply `{"ok":true}` / 오류 `window_not_found`|`no_window`|`window_maximized`|`window_fullscreen_state`|`bad_args`

- [ ] **Step 1: 서버 도구 본체** — window_fullscreen 분기 옆에 2분기 추가. 대상 선정/거절 규칙은 window_fullscreen 복제: id 생략 = 호출자 자기 창(윈도 클라, control-only 생략형 = no_window), 명시 id = 임의 창, shell/오버레이/IsControlOnly 배제(window_not_found). 추가 거절: preMaxRects_에 있으면 `window_maximized`(최대화 중 기하 조작은 preMaxRects_ 진실원과 충돌), IsFullscreen 레이어는 `window_fullscreen_state`(전체화면 상태 전용 — 복원 후 조작).
- [ ] **Step 2: move 구현** — `compositor_->SetLayerPosition(id, x, y)` + `PushWindowListUnsafe()`(focus_window 선례 — 태스크바 동기). 좌표 클램프 없음(Windows 동작 — 화면 밖 허용), int 범위만 검사(±32768 상한 초과 = bad_args).
- [ ] **Step 3: resize 구현** — 인자 클램프 w/h ∈ [80, 8192](미달/초과 = bad_args). 현재 레이어 픽셀 크기와 동일하면 no-op ok. 다르면 `CommitChromeResize(client, layerId, w, h, w, h)`(dispW/H=자기 — fit-scaled 레이어 아님. **픽셀 크기≠표시 크기인 fit-scaled 레이어**는 dispW/H를 기존 ScaleX 반영 산출로 — 구현 시 ResizeLayer가 scale을 리셋한다는 주석(1210-1216) 재확인). 후 `PushWindowListUnsafe()`.
- [ ] **Step 4: 브로커 등록** — jkagentd 4곳(send_input/window_fullscreen 선례 그대로): tools/list 설명+inputSchema, IsKnownTool, 기본 allow 배열, 릴레이 분기(734의 window_fullscreen 분기가 특수 shaping인지 generic인지 읽고 따른다).
- [ ] **Step 5: probe_window_geom.ps1 신설** — harness: probe_agent_e2e류 agentctl 경로. 시나리오: minesweeper 스폰 → list_windows에서 id/rect 획득(**list_windows 응답에 rect 필드 있는지 먼저 실측 — 없으면 대체 검증: capture_window 픽셀 위치 또는 서버 로그**) → window_move (200,150) → rect 변조 단정 → window_resize 500×400 → 500×400 단정 → close_window 정리. 생략형 1체크(control-only 호출자 생략형 = no_window). ×2.
- [ ] **Step 6: 빌드+프로브 ×2 → Commit** — `feat(agent): window_move/window_resize 도구 — 창 기하 서버 조작(폰 실전 ④)`

### Task 3: 채팅 LLM 시스템 프롬프트 (CoT/마크다운 누출 방지)

**Files:**
- Modify: `engine/src/agent/JKLlmEngine.cpp` (BuildEngineCmd :99-143)
- Test: 기존 smoke_llm_stream.ps1 + stub probe 회귀

**Interfaces:**
- Consumes: BuildEngineCmd의 -p payload 조립부(quote escape 루프 **앞**에서 원문 프롬프트에 접두)
- Produces: 모든 엔진(ollama/claude) 턴의 프롬프트 앞에 고정 지시문 — stub 엔진은 프롬프트 무시라 무영향

- [ ] **Step 1: 프리앰블 상수** — `kLlmTurnPreamble`(한국어, 플레인 텍스트 규약): ①최종 답변만 출력 — 사고 과정/계획/진행 내레이션("thinking" 블록, "먼저 ~를 확인하겠습니다"류) 금지 ②마크다운 문법 금지(코드펜스 ```/헤딩 #/굵게 **/인라인 백틱) — 출력은 플레인 텍스트로 전달된다 ③도구 사용은 조용히 실행하고 결과만 간결 보고. 접미 `[사용자] ` 후 원문 프롬프트. BuildEngineCmd에서 `prompt = kLlmTurnPreamble + prompt` (escape 루프 전).
- [ ] **Step 2: 검증** — stub smoke_llm_stream 3/3 + probe_agent_chat 7/7(프롬프트 경로 무손상 — stub은 echo). 라이브 실측(실엔진 1턴, CoT 누출 부재)은 재량 수행(네트워크 가능하면) — 불가 시 사용자 폰 눈확인으로 이월 기록.
- [ ] **Step 3: Commit** — `feat(agent): 채팅 LLM 턴 프리앰블 — CoT/마크다운 누출 방지(폰 실전 ⑥)`

### Task 4: 회귀 스윕 + docs/57 §13 as-built + 라이브 반영

**Files:**
- Modify: `docs/57_jkbridge.md` (§13 신설)

- [ ] **Step 1: 회귀 ×2** — probe_app_tools, probe_agent_events(또는 focused 단정 프로브), probe_window_geom(신설), smoke_llm_stream, probe_agent_chat, probe_workshop, probe_send_input ×2 — 전부 ×2 GREEN. jkdesktop test 0.
- [ ] **Step 2: docs/57 §13** — 4건 요약+실측+레슨(있으면). docs/60 §5 백로그의 args 항목 소각 표기.
- [ ] **Step 3: 라이브 반영** — 데스크탑 정지→ninja 풀빌드 링크 0→재기동(서버+taskbar), jkbridge 재기동(pong), jkagentd 재기동 — 폰에서 새 도구/프롬프트 즉시 유효화.
- [ ] **Step 4: Commit** — `docs(57): 폰 실전 개선 4건 as-built (§13)`