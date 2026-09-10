# 알림 센터 앱 (jkapp_notify) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `agent.notify` 이벤트의 단일 수신처인 알림 센터 앱 — 히스토리 + 창 내부 토스트 + 제목 안읽음 배지.

**Architecture:** 팔레트 템플릿 ImGui 클라이언트 앱(`jkapp_notify.dll`) — 이벤트 구독은 팔레트가 쓰는 `SendAgentEventSubscribe`/`DrainAgentEvents` 배관 재사용, 구독 토픽은 `state/notify.json`, 히스토리는 `state/notify_history.json`으로 세션을 넘어 지속. 제목 배지를 위해 새 프로토콜 메시지 `WindowTitle`(C→S) 하나 추가 (docs/28가 예고한 SetTitle C→S).

**Tech Stack:** jkwindow + ImGui (palette/taskmgr 패턴), JKAgentJson (quickjs 리더), CMake custom command (jkx-pack), PowerShell probes.

**Spec:** `docs/superpowers/specs/2026-09-09-desktop-agent-platform-design.md` §7 (알림 센터 행) + docs/32 §8. 사용자 결정사항 (2026-09-11): ImGui 클라 앱 / 구성 가능한 구독 / 토스트+히스토리 / 배지=창 제목 / 화면구석 토스트는 작업표시줄 통합(스펙 단계 4) 후속.

## Global Constraints

- 클라 앱은 JKWindow/SDL 클라 모델 (채팅창 밖-원칙은 jkchat에만 적용됨).
- 서버는 "어리석은 컴포지터" — 알림 파싱/상태는 클라 측 (docs/28 원칙).
- 프로토콜 확장은 v2 규약: trivially-copyable payload, 새 MsgType은 다음 번호(21) 부터.
- 한국어 UI 텍스트는 ImGui 기본 글꼴(ASCII만)에 안 나옴 — Malgun Gothic + `GetGlyphRangesKorean` 필수, 로드 실패 시 영어 폴백.
- 프로브 교훈: PS5.1 한국어 리터럴은 UTF-8 **BOM 단일**; agentctl JSON 안에 **공백 금지**(네이티브 argv 분할); 이벤트 구독은 트리거 쿼리 **이전**; ImGui 앱은 Win32 컨트롤이 아니므로 **크로스프로세스 WM_GETTEXT 불가** — 검증은 `list_windows` 제목 + state 파일로.
- 커밋 규약: 태스크 단위 커밋, `Co-Authored-By: Claude Code <noreply@anthropic.com>`, 완료 즉시 push (main 직접).

---

### Task 1: WindowTitle C→S 프로토콜 (제목 배지 배관)

**Files:**
- Modify: `engine/include/ipc/JKWireProtocol.h` (MsgType, 20 뒤)
- Modify: `engine/include/client/JKClientSurface.h` + `engine/src/client/JKClientSurface.cpp`
- Modify: `engine/src/server/JKWindowServer.cpp` (ProcessClientMessage 브랜치 + 클라 title 필드)

**Interfaces:**
- Consumes: `ipc::WriteMessage`/`ReadMessage` (기존 17-20 패턴).
- Produces: `JKClientSurface::SendWindowTitle(const std::string& utf8)` (C→S, 길이 상한 96 — 초과 시 무시); 서버는 payload를 클라 제목으로 설정 + `PushWindowList()` (taskbar 버튼 텍스트 갱신). Task 4가 이 함수로 배지를 표시한다.

- [ ] **Step 1: MsgType** — `WindowTitle = 21 // C -> S: 클라가 자기 창 제목을 갱신 (알림 센터 안읽음 배지, docs/28 예고)`
- [ ] **Step 2: 클라 송신** — `JKClientSurface::SendWindowTitle`: `ipc::WriteMessage(Transport(), MsgType::WindowTitle, 0, std::vector<uint8_t>(utf8.begin(), utf8.end()))`. 96바이트 초과 시 false.
- [ ] **Step 3: 서버 수신** — ProcessClientMessage에 브랜치: `JKClientSurface::SetTitle(utf8 payload)` (96 cap, 클라 클래스에 setter 없으면 추가) → `PushWindowList()`. ShellRegister/WindowListSubscribe 패턴 그대로.
- [ ] **Step 4: 빌드 + 회귀** — `cmake --build build` (전체 타깃), `jkdesktop test` 0.
- [ ] **Step 5: 커밋** `feat(client): WindowTitle C->S protocol message`.

### Task 2: ClientNotifyApp 스켈레톤 + 모듈 + 패키징

**Files:**
- Create: `engine/include/apps/ClientNotifyApp.h`, `engine/src/apps/ClientNotifyApp.cpp`
- Create: `engine/src/apps/JKAppModule_notify.cpp`
- Modify: `engine/CMakeLists.txt` (palette 블록 뒤 target; JKX_ICON_APPS + notify; notify.jx custom command + jkx_packages DEPENDS)
- Create: `engine/assets/icons/launcher_notify@1x.png`, `@2x.png` (생성 스크립트로)
- Create: `engine/tools/probes/gen_notify_icon.ps1` (GDI+ 2026-09-07 아이콘 시스템: 다크 슬레이트 타일 #2b303c→#22262f, r14, 앰버 종 글리프 #d8a24a; 64 공간 좌표 + ScaleTransform)

**Interfaces:**
- Consumes: `JKClientApplication` (OnInit/OnClose/PreProcessMessage/IsFrameDirty/OnFrameCommitted/RenderOverlay), `Surface()->SendAgentEventSubscribe(true)`, `DrainAgentEvents` — 팔레트(ClientPaletteApp.cpp) 그대로 복제.
- Produces: 앱 `notify` (제목 "Notifications", 420×560, chromeless, timer 16ms). Task 4가 여기에 UI/상태를 얹는다. 런처에서 `launch_app notify` 스폰 가능.

- [ ] **Step 1: 아이콘 생성 스크립트 + 실행** — gen_notify_icon.ps1 (아래 코드). 실행 후 2개 PNG 확인.
```powershell
# engine/tools/probes/gen_notify_icon.ps1 — bell glyph on the dark slate tile
Add-Type -AssemblyName System.Drawing
function MakeIcon([int]$scale, [string]$path) {
  $s = 64; $w = 64 * $s; $bmp = New-Object System.Drawing.Bitmap($w, $w)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.SmoothingMode = 'AntiAlias'
  $tile = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    (New-Object System.Drawing.Rectangle(0, 0, $w, $w)),
    [System.Drawing.Color]::FromArgb(0x2b,0x30,0x3c),
    [System.Drawing.Color]::FromArgb(0x22,0x26,0x2f), 90)
  $r = 14 * $s
  $gp = New-Object System.Drawing.Drawing2D.GraphicsPath
  $gp.AddArc(0, 0, 2*$r, 2*$r, 180, 90); $gp.AddArc($w-2*$r, 0, 2*$r, 2*$r, 270, 90)
  $gp.AddArc($w-2*$r, $w-2*$r, 2*$r, 2*$r, 0, 90); $gp.AddArc(0, $w-2*$r, 2*$r, 2*$r, 90, 90)
  $gp.CloseFigure(); $g.FillPath($tile, $gp)
  # bell: dome + skirt + clapper, amber
  $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(0xd8,0xa2,0x4a), (4.0*$s))
  $g.DrawArc($pen, 16*$s, 14*$s, 32*$s, 34*$s, 180, 180)          # dome
  $g.DrawLine($pen, 14*$s, 44*$s, 50*$s, 44*$s)                    # skirt
  $g.DrawLine($pen, 32*$s, 12*$s, 32*$s, 16*$s)                    # nub
  $dot = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(0xd8,0xa2,0x4a))
  $g.FillEllipse($dot, 29*$s, 49*$s, 6*$s, 6*$s)                   # clapper
  $g.Dispose(); $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
}
MakeIcon 1 "I:\progwork\JKENGINE\engine\assets\icons\launcher_notify@1x.png"
MakeIcon 2 "I:\progwork\JKENGINE\engine\assets\icons\launcher_notify@2x.png"
```
- [ ] **Step 2: ClientNotifyApp** — 팔레트 복제 후 이름만 바꾼 스켈레톤: `NotifyRoot`(24,24,30 클리어), OnInit: `"Notifications"` 420×560 chromeless + `SetTimerInterval(16)` + `SendAgentEventSubscribe(true)`; RenderOverlay: ImGui init/휴 프레임 루프 + `BuildUi`(임시: 히스토리 벡터 텍스트) + `DrainEvents`(비었음 — Task 4). **한국어 글꼴**: RenderOverlay의 imgui init 전 `ImGuiIO& io = ImGui::GetIO(); io.Fonts->Clear(); io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f, nullptr, io.Fonts->GetGlyphRangesKorean());` — nullptr 반환(파일 없음) 시 Clear 후 기본 폰트로 진행.
- [ ] **Step 3: 모듈** — JKAppModule_notify.cpp: `static const jk::JKAppMeta meta{ "notify", "Notifications", 420, 560 };` (palette 파일 복제).
- [ ] **Step 4: CMake** — `add_library(jkapp_notify SHARED src/apps/JKAppModule_notify.cpp src/apps/ClientNotifyApp.cpp)` + `JKAPP_MODULE_BUILD` + `jkcore imgui` + `PREFIX ""`; JKX_ICON_APPS에 `notify`; imguidemo 패턴 custom command (`jkx-pack notify`, DEPENDS jkdesktop jkapp_notify + 2 icons) + jkx_packages DEPENDS에 `notify.jkx` 추가.
- [ ] **Step 5: 빌드 + 실측** — cmake 재구성 → `jkdesktop test` 0 → 서버 부팅 후 `agentctl`로 `launch_app notify` → `list_windows`에 "Notifications" 보임. (여기까진 빈 화면도 정상.)
- [ ] **Step 6: 커밋** `feat(notify): notification center app skeleton + jkx`.

### Task 3: 구독 설정 + 히스토리 지속

**Files:**
- Modify: `engine/include/apps/ClientNotifyApp.h` + `engine/src/apps/ClientNotifyApp.cpp`

**Interfaces:**
- Consumes: `AgentJson::GetArraySize/GetArrStr/GetArrInt/GetObjStr/GetInt` (`engine/include/agent/JKAgentJson.h`), exeDir 패턴 (`GetModuleFileNameA` → dir — JKWindowServer::StateDir 참고, 클라 버전으로).
- Produces: struct `NotifyEntry { std::string topic, title, body; long long ts; bool read; };` + `std::vector<NotifyEntry> history_;` + `int unread_ = 0;` + `std::vector<std::string> topics_;` — Task 4/5가 사용. 파일 경로 헬퍼 `std::string StatePath(const char* name)`.

- [ ] **Step 1: 설정 로드** — OnInit에서 `state/notify.json`: `{"topics":[{"topic":"agent.notify"}]}` — `AgentJson`으로 `GetArraySize("topics")` + `GetArrStr("topics", i, "topic")`. 파일 없거나 파싱 실패 → `topics_ = ["agent.notify"]` (기본값, 파일 생성 안 함).
- [ ] **Step 2: 히스토리 저장** — `SaveHistory()`: `state/notify_history.json` 전체 재작성 `{"entries":[{"topic":"..","title":"..","body":"..","ts":N,"read":0|1}, ...]}` (EscapeJson 재사용, cap 200 — append 시 초과분은 꼬리부터 drop).
- [ ] **Step 3: 히스토리 로드** — OnInit: `GetArraySize("entries")` + `GetArrStr/GetArrInt` 복원, `unread_` = read==0 개수. 손상 파일 → 비우고 시작.
- [ ] **Step 4: 구독 필터** — `DrainEvents`가 event의 `topic`을 `topics_`에 포함된 것만 히스토리에 push (아직 UI 없음 — 파일에 쌓이는지만). append마다 SaveHistory + frameDirty_.
- [ ] **Step 5: 빌드 + 실측** — 서버+jktriggers 없이도 `agentctl publish_event`로 agent.notify 1회 → `state/notify_history.json`에 1 엔트리; 앱 종료 후 재시작 → 파일에서 복원(로그로 확인할 방법: 두 번째 엔트리 추가 후 파일 줄 수). 
- [ ] **Step 6: 커밋** `feat(notify): configurable topics + persistent history`.

### Task 4: UI — 히스토리 목록 + 토스트 + 배지

**Files:**
- Modify: `engine/include/apps/ClientNotifyApp.h` + `engine/src/apps/ClientNotifyApp.cpp`

**Interfaces:**
- Consumes: Task 1 `SendWindowTitle`, Task 3 `NotifyEntry/history_/unread_`.
- Produces: UI 완성 앱. (probe가 list_windows 제목 + history 파일로 판정.)

- [ ] **Step 1: 표시 라인** — `FormatEntry(const NotifyEntry&, char* buf, size_t n)`: `[HH:MM] <title> — <body>` (본문 없으면 제목만; topic이 agent.notify가 아니면 접두 `<topic>`). 시각은 ts(epoch ms) → localtime.
- [ ] **Step 2: BuildUi** — 상단: `안읽음 N` + 버튼 `모두 읽음`(전체 read=true, unread_=0, SaveHistory, 배지 갱신) / `지우기`(history_ clear, SaveHistory). 히스토리 child(스크롤, 최신이 위 — 역순 순회): 안읽음 항목은 `ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,0.85f,0.5f,1))` 하이라이트.
- [ ] **Step 3: 토스트** — 하단 스트립: `now < toastUntil_` 동안 새 엔트리 title+body, 남은 시간 <2초면 알파 페이드. 새 알림 도착 시 `toastUntil_ = now + 5s`, `toastEntry_ = 마지막 엔트리`.
- [ ] **Step 4: 배지** — `UpdateBadge()`: `main->SetTitle(unread_ ? "Notifications (" + to_string(unread_) + ")" : "Notifications")` + `Surface()->SendWindowTitle(...)`. unread_ 변화 시에만 호출 (SetTitle 스팸 방지).
- [ ] **Step 5: 빌드 + 실측** — publish 2회 → `list_windows` 제목이 `Notifications (2)`; 다시 publish → (3); 앱 재시작 → 배지 유지(파일 복원). 
- [ ] **Step 6: 커밋** `feat(notify): history list, toast strip, unread title badge`.

### Task 5: open_notify 도구 + 팔레트 /notify

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` (TogglePalette → 일반화 + HandleAgentQuery 브랜치)
- Modify: `engine/include/server/JKWindowServer.h`
- Modify: `engine/src/apps/ClientPaletteApp.cpp` (Submit + /help)

**Interfaces:**
- Consumes: SpawnClient/FocusClient (기존).
- Produces: agent 도구 `open_notify` (인수 없음, `{"ok":true}` — 이미 열려 있으면 focus). 팔레트 명령 `/notify`.

- [ ] **Step 1: 일반화** — `void ToggleClientByTitle(const std::string& title, const char* appName);` — TogglePalette 본문을 title/appName 매개변수화, TogglePalette는 `ToggleClientByTitle("Command Palette", "palette")` 호출로 대체.
- [ ] **Step 2: 도구** — HandleAgentQuery에 `open_notify` 브랜치: `ToggleClientByTitle("Notifications", "notify"); reply = "{\"ok\":true}";`
- [ ] **Step 3: 팔레트** — Submit에 `else if (cmd == "notify") { SendTool("open_notify", "{}"); }`, /help 줄에 `/notify` 추가.
- [ ] **Step 4: 빌드 + 실측** — `agentctl` open_notify 2회 → 창 1개 유지(list_windows에서 "Notifications" 카운트 1), 두 번째 호출로 focus 전환.
- [ ] **Step 5: 커밋** `feat(agent): open_notify tool + palette /notify`.

### Task 6: 프로브 + 문서 + 전체 회귀

**Files:**
- Create: `engine/tools/probes/probe_agent_notify.ps1`
- Create: `docs/33_desktop_agent_notify.md`
- Modify: 스펙 §7 상태 기록, docs/32 §8 (알림 센터 완료 표시), memory

- [ ] **Step 1: 프로브** (UTF-8 BOM 단일, 판정은 list_windows/파일 기반 — ImGui는 WM_GETTEXT 불가):

```powershell
# probe_agent_notify.ps1 개요 — 각 체크는 독립 PASS/FAIL, exit code 판정
# 1) 서버 부팅 → agentctl open_notify → list_windows에 "Notifications"
# 2) agentctl publish_event agent.notify 2회 (데이터 공백 없이:
#    {"title":"TestTitle1","body":"body-one"} / {"title":"TestTitle2"})
#    → list_windows 제목 "Notifications (2)" (배지 C->S 실측)
# 3) state/notify_history.json entries >= 2, read==0
# 4) agentctl open_notify 재호출 → "Notifications" 카운트 1 (toggle, 중복 스폰 없음)
# 5) cleanup: 프로세스 kill, notify_history.json/notify.json 삭제, permissions 복구
```
  판정 유틸은 probe_agent_triggers.ps1의 `Invoke-Agentctl`/`Start-Process --server` 패턴 복제. 프로세스 종료 **전에** 파일을 읽을 것 (lesson 33).
- [ ] **Step 2: 전체 회귀** — `jkdesktop test` 0, `jkagentd --selftest` 0, probes: mcp 5/5, e2e 7/7, palette 4/4, chat 7/7, chat_llm 2/2, triggers 7/7, notify 전부 PASS.
- [ ] **Step 3: docs/33** — 아키텍처(이벤트→히스토리/토스트/배지), state 파일 2종 형식, WindowTitle C→S 프로토콜(21), 제한(창 내부 토스트 — 화면구석은 셸 통합 후속, 히스토리 cap 200, 구독 토픽은 시작 시 1회 적용), 다음 단계(트리거 활성/비활성, LLM 스트리밍).
- [ ] **Step 4: 스펙 §7 알림 센터 행에 "구현 완료 (2026-09-11, docs/33)" 표기 + docs/32 §8 갱신 + memory 갱신, 커밋+push** `feat(notify): probe + docs/33 — notification center complete`.

## 완료 조건

1. `open_notify`(또는 런처 아이콘)로 알림 센터 창이 열린다 — 제목 "Notifications".
2. publish_event 2회 → 창 제목이 `Notifications (2)`로 바뀌고(작업표시줄 버튼에도), 히스토리에 2줄 쌓임, 창 하단 토스트.
3. 창을 닫았다 다시 열면 히스토리가 복원되고 안읽음이 유지된다 (`notify_history.json`).
4. `모두 읽음` → 배지 소멸 + 파일 read 플래그. `지우기` → 파일 비움.
5. 기존 회귀 전부 녹색.