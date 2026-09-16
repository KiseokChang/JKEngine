# vplayer 전체화면 + 하단 호버 OSD Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** vplayer에 서버 레이어 전체화면(docs/39 maximize 형제) + 극장 모드 UI + 하단 호버 반투명 OSD 스트립(하단 호버 표시/무동작·이탈 소멸)을 구현한다.

**Architecture:** 서버에 fullscreen 레이어 상태(preFsRects_ + JKCompositorLayer 플래그)를 두고 크롬(버튼/그랩/커서)을 스킵하며, `window_fullscreen` 도구(none gate, focus_window 분류)로 클라·스크립트·프로브가 구동한다. vplayer는 도구 응답으로 극장 모드 플래그를 관리하고, OSD는 극장 모드 전용 ImGui 스트립(하단 22% 밴드 + 2.5s 아이들 + 200ms 페이드)으로 그린다.

**Tech Stack:** C++ (MinGW/ucrt64, CMake), SDL2 컴포지터, ImGui, PS5.1 probes.

**Spec:** `docs/superpowers/specs/2026-09-17-vplayer-fullscreen-osd.md`

## Global Constraints

- 빌드: `cd /i/progwork/JKENGINE/engine/build && PATH=/c/msys64/ucrt64/bin:$PATH cmake --build . --target <T>` — 빌드 출력을 grep으로 줄이지 않는다(레슨 37). 산출물 mtime > 소스 mtime 게이트.
- 프로브 규약: PS5.1, ASCII-only + BOM(lesson 50), 클린바이너리 공식런(lesson 61), SendKeys 대신 도구 경로로 상태 구동(roadmap SendKeys 플레이크 교훈).
- 주석 한국어 설계 사유 위주(기존 관례). 최종리뷰 opus(상임). 완료 후 roadmap/killer-app 메모리 동기화.
- 창 모드 동작은 전혀 변하지 않는다(스펙 §2.3) — 회귀 vpt9/vpt4/vpt5/vpt11로 증명.

---

### Task 1: 서버 전체화면 기계 + window_fullscreen 도구 (+ jkagentd 등록)

**Files:**
- Modify: `engine/include/server/JKCompositorLayer.h` (fullscreen 플래그)
- Modify: `engine/src/server/JKCompositor.cpp` (크롬 스킵)
- Modify: `engine/include/server/JKWindowServer.h` + `engine/src/server/JKWindowServer.cpp` (상태 맵/토글/도구/재발행/정리)
- Modify: `engine/tools/jkagentd/main.cpp` (4곳 등록)

**Interfaces:**
- Consumes: `CommitChromeResize(client, id, surfW, surfH, dispW, dispH)`, `MaxState{x,y,surfW,surfH,dispW,dispH}`(JKWindowServer.h:306), `PushMaximizeEvent(topic, client)`.
- Produces: `ToggleFullscreen(client, layer, bool on)` + `RestoreFullscreen(client, layer)`(서버 private), 도구 `window_fullscreen {id?, on?}` → `{"ok":true,"fullscreen":bool}` / `{"ok":false,"error":"no_window"|"window_not_found"}`; 이벤트 `window.fullscreen`/`window.fullscreen_exit`.

- [ ] **Step 1: JKCompositorLayer에 fullscreen 플래그** — `maximizedFlag` 뒤에:

```cpp
    // 전체화면 상태(스펙 2026-09-17 vplayer-fullscreen-osd §2.1): 서버가
    // 상태 맵(preFsRects_)을 소유하고, 이 플래그는 컴포지터 그리기 경로가
    // 크롬 버튼을 생략하도록 거울이다(maximizeFlag 선례).
    bool IsFullscreen() const { return fullscreenFlag; }
    void SetFullscreen(bool f) { fullscreenFlag = f; }
```
멤버 `bool fullscreenFlag = false;` 추가.

- [ ] **Step 2: 크롬 스킵** — `JKCompositor::Composite`(257행)의 크롬 그리기 조건에 fullscreen 추가:

```cpp
            if (!layer->IsShell() && layer->Title() != kCaptureOverlayTitle &&
                !layer->IsFullscreen()) {
```
`DrawMaximizeButton` 블록은 위 if 안에 이미 있으므로 한 곳의 조건 수정으로
close+maximize 버튼이 모두 스킵된다.

- [ ] **Step 3: 서버 헤더** — `JKWindowServer.h`의 `ToggleMaximize` 선언 뒤:

```cpp
    // vplayer 전체화면(스펙 §2.1): maximize 형제 — preFsRects_가 상태의
    // 단일 소유, SetFullscreen은 그리기 경로 거울. on=false = RestoreFullscreen.
    void ToggleFullscreen(JKClientConnection& client, JKCompositorLayer& layer,
                          bool on);
    bool RestoreFullscreen(JKClientConnection& client, JKCompositorLayer& layer);
```
멤버: `std::map<uint32_t, MaxState> preFsRects_;`(preMaxRects_ 옆 — 별도 맵
룰링: maximize 글리프/재발행 경로와 얽히지 않게).

- [ ] **Step 4: 서버 토글 구현** — `RestoreFromMaximize`(1073행) 뒤에:

```cpp
// vplayer 전체화면(스펙 §2.1): maximize 형제. 최대화 중이면 먼저 복원에서
// 출발(Windows 관례)하고, 전체 출력 크기로 CommitChromeResize — 작업 영역
// 예약 없음(전체화면은 작업표시줄을 덮는다, 표준). 크롬 스킵은
// layer.SetFullscreen 거울 + TryChromeGrab/Composite/UpdateChromeHoverCursor
// 3곳의 플래그 검사.
void JKWindowServer::ToggleFullscreen(JKClientConnection& client,
                                      JKCompositorLayer& layer, bool on) {
    if (!on) {
        RestoreFullscreen(client, layer);
        return;
    }
    if (preFsRects_.count(layer.Id()) != 0) return; // 이미 전체화면
    if (!compositor_ || !window_) return;
    if (preMaxRects_.count(layer.Id()) != 0)
        RestoreFromMaximize(client, layer); // 최대화 → 정상 크기에서 출발
    MaxState saved;
    saved.x = layer.X();
    saved.y = layer.Y();
    saved.surfW = layer.Width();
    saved.surfH = layer.Height();
    saved.dispW = static_cast<int>(std::llround(saved.surfW * layer.ScaleX()));
    saved.dispH = static_cast<int>(std::llround(saved.surfH * layer.ScaleY()));
    int ww = 0, wh = 0;
    SDL_GetWindowSize(window_, &ww, &wh);
    CommitChromeResize(client, layer.Id(), ww, wh, ww, wh);
    preFsRects_[layer.Id()] = saved;
    compositor_->SetLayerPosition(layer.Id(), 0, 0);
    client.SetPosition(0, 0);
    layer.SetFullscreen(true);
    PushMaximizeEvent("window.fullscreen", client);
}

// 전체화면 복원: RestoreFromMaximize의 대응물 — 저장 rect 복원 + 이벤트 1회.
// 데스크탑 리사이즈 재발행과 단절 정리가 이 함수를 경유하지 않는다(각자
// 이유가 다름 — 재발행은 이벤트 없음, 정리는 레이어 소멸).
bool JKWindowServer::RestoreFullscreen(JKClientConnection& client,
                                       JKCompositorLayer& layer) {
    auto it = preFsRects_.find(layer.Id());
    if (it == preFsRects_.end()) return false;
    const MaxState saved = it->second;
    preFsRects_.erase(it);
    CommitChromeResize(client, layer.Id(), saved.surfW, saved.surfH,
                       saved.dispW, saved.dispH);
    compositor_->SetLayerPosition(layer.Id(), saved.x, saved.y);
    client.SetPosition(saved.x, saved.y);
    layer.SetFullscreen(false);
    PushMaximizeEvent("window.fullscreen_exit", client);
    return true;
}
```

- [ ] **Step 5: 크롬 그랩/커서 스킵** — `TryChromeGrab`(839행)의 셸/capture 거부 직후:

```cpp
    // 전체화면 레이어는 크롬이 없다 — 상단 24pt 포함 모든 클릭이 앱에 도달
    // (스펙 §2.1). 클릭 포커스는 1241행 일반 경로라 살아 있다.
    if (layer->IsFullscreen()) return false;
```
`UpdateChromeHoverCursor`(1099행) — 레이어를 찾은 뒤 같은 조건으로 리사이즈
커서를 화살표로 강제:

```cpp
        if (layer && client && layer->IsFullscreen()) {
            SetChromeCursor(CursorShape::Arrow);
            return;
        }
```
(정확한 위치는 기존 조건 순서에 맞춰 배치 — 셸 제외 검사와 같은 앞단.)

- [ ] **Step 6: 데스크탑 리사이즈 재발행** — `UpdateOutputBounds`(1340행)의 재발행 루프 뒤(같은 조건 블록 안), fullscreen 맵 루프 추가:

```cpp
            size_t nFs = 0;
            for (const auto& kv : preFsRects_) {
                JKClientConnection* client = FindClientById(kv.first);
                if (!client || client->IsDisconnected()) continue;
                ++nFs;
                // 전체화면 재발행: 새 출력 전체(예약 없음). 저장 rect는
                // 무효화하지 않는다(복원 대상은 여전히 진짜 원 rect).
                CommitChromeResize(*client, kv.first, logW, logH, logW, logH);
                compositor_->SetLayerPosition(kv.first, 0, 0);
                client->SetPosition(0, 0);
            }
            std::printf("[server] desktop size changed to %dx%d "
                        "(re-maximized %zu, re-fullscreened %zu layer(s))\n",
                        logW, logH, nMax, nFs);
            std::fflush(stdout);
```
(기존 printf의 %zu 포맷에 nFs 인수를 맞춘다 — 정확한 기존 라인을 교체.)

- [ ] **Step 7: 단절 정리** — `CleanupDisconnectedClients`(1586행)의
`preMaxRects_.erase(client->Id());` 뒤에 `preFsRects_.erase(client->Id());`
추가(주석: 죽은 레이어의 저장 rect는 무의미 — maximize 동일).

- [ ] **Step 8: window_fullscreen 도구** — `kPermMatrix`에
`{"window_fullscreen", "none", "allow"},`(focus_window 행 근처) 추가.
`HandleAgentQuery`의 `focus_window` 분기 뒤:

```cpp
    } else if (tool == "window_fullscreen") {
        // vplayer 전체화면(스펙 §2.2): id 생략 = 호출자 자기 창(창 클라이언트
        // — vplayer 경로), 명시 id = 임의 창(스크립트/프로브). control-only
        // 호출자의 생략형은 no_window. on 생략 = 현재 상태 반전. 화면 상태
        // 변경일 뿐 승인 행위가 아니어서 none gate(focus_window 분류).
        int id = 0;
        JKClientConnection* target = nullptr;
        if (req.GetObjInt("args", "id", id)) {
            for (auto& c : clients_) {
                if (c && c->Id() == static_cast<uint32_t>(id)) { target = c.get(); break; }
            }
        } else if (!client.IsControlOnly()) {
            target = &client;
        }
        bool on = true;
        if (target && !target->IsControlOnly() && compositor_) {
            JKCompositorLayer* layer = compositor_->FindLayerById(target->Id());
            if (layer) {
                if (req.GetObjBool && !req.GetObjBool("args", "on", on)) on = !layer->IsFullscreen();
                // AgentJson에 bool 접근자가 없으면 문자열/정수 재판독(실행 시
                // 확인 — 아래 Step 9 주석 참조).
                ToggleFullscreen(*target, *layer, on);
                reply = std::string("{\"ok\":true,\"fullscreen\":") +
                        (layer->IsFullscreen() ? "true" : "false") + "}";
            } else { reply = "{\"ok\":false,\"error\":\"window_not_found\"}"; }
        } else {
            reply = (target ? "{\"ok\":false,\"error\":\"window_not_found\"}"
                            : "{\"ok\":false,\"error\":\"no_window\"}");
        }
    }
```
주의(실행 시): (1) `req.GetObjBool`의 존재 여부를 AgentJson에서 먼저 확인 —
없으면 `on`을 생략형(반전)만 지원하거나 GetObjInt(0/1)로 읽는다. 스펙의
`on` 진위는 프로브 경로에 필요하므로 **GetObjInt 0/1로 읽는 것을 기본으로**
한다(없으면 반전). (2) toggle 중에 최대화/전체화면 상태 경계는
ToggleFullscreen 내부에서 해소된다. (3) reply의 fullscreen 값은 토글 후
실제 플래그 — 클라 플래그의 유일 신뢰원.

- [ ] **Step 9: jkagentd 4곳 등록** (run_console_app/focus_window 선례) —
(1) `kToolsListJson`에 focus_window 뒤:
`{"name":"window_fullscreen","description":"Toggle a window's fullscreen layer state (vplayer theater mode, none gate)","inputSchema":{"type":"object","properties":{"id":{"type":"integer"},"on":{"type":"integer","enum":[0,1]}}}},`
(2) `IsKnownTool` kNames에 `"window_fullscreen"`(3) `LoadPermissions`
kNames에 추가(기본 allow — perms 수정 불필요, 맵 존재만)(4) args-rebuild
분기(focus_window 분기 뒤):
```cpp
        } else if (tool == "window_fullscreen") {
            int id = 0, on = -1;
            std::string parts;
            if (req.GetDeepInt("params", "arguments", "id", id)) {
                parts = "{\"id\":" + std::to_string(id);
                if (req.GetDeepInt("params", "arguments", "on", on))
                    parts += ",\"on\":" + std::to_string(on);
                parts += "}";
                argsJson = parts;
            } else {
                argsJson = "{}";  // 자기 창 생략형 — control-only라 실질 미사용
            }
        }
```
- [ ] **Step 10: 빌드 + selftest** — jkwinserver/jkdesktop/jkagentd 빌드,
`jkdesktop test`(0) + `jkagentd --selftest`(0). agentmgr 스펙의 kPermMatrix
프로브(probe_agentmgr)는 도구 수 관련 단언이 없으므로 기존 21체크로 회귀 확인.

- [ ] **Step 11: 커밋** — `feat(server): window fullscreen layer state + window_fullscreen tool (vplayer spec 2.1-2.2)`

### Task 2: vplayer 극장 모드 + 하단 호버 OSD

**Files:**
- Modify: `engine/include/apps/ClientVPlayerApp.h` + `engine/src/apps/ClientVPlayerApp.cpp`

**Interfaces:**
- Consumes: Task 1의 도구(자기 창 생략형), `PlayerCore::SnapNow()`, 기존 시크 커밋-on-release 슬라이더 패턴(BuildUi 1971-1986).
- Produces: 극장 모드 렌더(비헤이비어 계약은 창 모드 무변경), OSD 가시 규칙(하단 22% 밴드 + 2.5s 아이들 + 200ms 페이드).

- [ ] **Step 1: 헤더 멤버 추가**:

```cpp
    // 전체화면/극장 모드(스펙 §2.2-2.3): 도구 응답이 실제 상태를 실어 오므로
    // 클라 플래그는 응답으로만 갱신된다(외부 토글 포함 최신).
    bool fullscreenUi_ = false;
    uint32_t fsQueryId_ = 0; // window_fullscreen 1-in-flight
    void RequestFullscreen(bool on);
    // OSD: 하단 밴드 + 아이들 + 페이드(스펙 §2.3). ms 단위 steady_clock.
    float osdAlpha_ = 0.f; // 0..1 (페이드 보간)
    bool osdShown_ = false;
    std::chrono::steady_clock::time_point osdLastActivity_{};
```

- [ ] **Step 2: RequestFullscreen 구현** — RequestOpenDialog(1792행) 옆에
동일 1-in-flight 패턴: `SendAgentQuery(id, "{\"tool\":\"window_fullscreen\","
"{... on 0/1 ...}")` (args `{"on":true|false}` — 생략형 반전 대신 명시로
클라 의도 고정). PumpAgentReplies에서 qid 매치 → 응답 JSON의
`"fullscreen":true` 문자열 스캔 → fullscreenUi_ 갱신 + `fsQueryId_ = 0`.
pipe_error/`window_not_found`면 fsQueryId_만 리셋(상태 불변 — 다음 토글 재시도).

- [ ] **Step 3: F11 + 더블클릭** — `PreProcessMessage`(1688행):
`JKEventType::Key`에서 F11 keyCode(virtual-key 0x7A)면
`RequestFullscreen(!fullscreenUi_)` — st.opening 여부 무관(창 상태 토글은
플레이어와 무관). 더블클릭은 BuildUi 내 비디오 이미지 항목에서
`payload.detail` 전달 확인본 대신 ImGui 판정:
`ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)`
(Image 위 — 창 모드에서도 토글, 스펙 §2.2 규칙).

- [ ] **Step 4: BuildUi 극장 분기** — `Begin` 직후(1901행의 SetCursorPosY(30)
이전에 분기): `if (fullscreenUi_) { TheaterUi(w, h, p, st); ImGui::End(); return; }`
— 별도 `TheaterUi(int w, int h, PlayerCore* p, const PlayerCore::Snap& st)`
개인 메서드로 뽑는다(창 모드 본문 무손상).

- [ ] **Step 5: TheaterUi 본문**:

```cpp
void ClientVPlayerApp::TheaterUi(int w, int h, PlayerCore* p,
                                 const PlayerCore::Snap& st) {
    ImGuiIO& io = ImGui::GetIO();
    // 비디오: 표면 전체 aspect-fit 중앙(창 모드와 동일 코드, avail = 전체).
    // 오류/오프닝 문구는 중앙 텍스트로.
    ...
    // --- OSD 스트립 -------------------------------------------------------
    const auto now = std::chrono::steady_clock::now();
    const float bandY = (float)h * 0.78f;
    const bool inBand = io.MousePos.y >= bandY &&
                        io.MousePos.x >= 0 && io.MousePos.x < (float)w &&
                        io.MousePos.y < (float)h;
    const bool active = io.MouseDelta.x != 0 || io.MouseDelta.y != 0 ||
                        io.MouseWheel != 0 || io.MouseClicked[0];
    if (active) osdLastActivity_ = now;
    osdShown_ = inBand && std::chrono::duration<double>(now - osdLastActivity_).count() < 2.5;
    const float target = osdShown_ ? 1.f : 0.f;
    const float dt = (float)std::chrono::duration<double>(now - lastOsdTick_).count();
    lastOsdTick_ = now;
    osdAlpha_ += (target - osdAlpha_) * std::min(1.f, dt / 0.2f); // 200ms 선형
    if (osdAlpha_ <= 0.02f) return; // 히든 — 입력 흡수 없음
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, osdAlpha_);
    ImGui::SetNextWindowPos(ImVec2(0, (float)h - 64.f));
    ImGui::SetNextWindowSize(ImVec2((float)w, 64.f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 0.82f));
    ImGui::Begin("##osd", nullptr, ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings);
    // Play/Pause(기존 reverseActive_ 가드 유지) | 시크 슬라이더(커밋-on-
    // release, reverseActive_ 가드) | 시간/전체 | vol | 해제 버튼
    ...
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    // 상태 문구(seekError/audioDeviceFailed)는 OSD 히든과 무관히
    // 좌상단에 항상 렌더(에러는 사라지면 안 된다).
}
```
주의: (1) seekError/오디오 실패 문구를 TheaterUi 초반(스트립 밖)에 렌더.
(2) osdLastActivity_ 초기화 — OpenPath/Init에서 now로 리셋(스테일 타이머
방지, OpenPath의 wheelScrubbing_ 컷 패턴 참조). (3) `lastOsdTick_` 멤버는
Step 1에 추가. (4) 극장 모드 진입/이탈 시 osdAlpha_/osdShown_ 리셋.

- [ ] **Step 6: mtime 게이트 + 빌드** — `--target jkapp_vplayer` 1회 빌드
(레슨 57) 후 jkx_packages 재팩(레슨 18).

- [ ] **Step 7: 커밋** — `feat(vplayer): theater mode + bottom-hover OSD (vplayer spec 2.3)`

### Task 3: vpt12 전체화면/OSD 프로브

**Files:**
- Create: `engine/tools/probes/vpt12_fullscreen.ps1`

- [ ] **Step 1: 프로브 작성** (PS5.1 ASCII + BOM, 도구 경로 구동 — SendKeys 없음):

구성(스펙 §5):
1. 클린바이너리 가드 + vplayer 스폰(기존 vpt 프로브의 spawn 패턴) → media 오픈.
2. `agentctl {"tool":"window_fullscreen","args":{"id":<id>,"on":1}}` →
   `{"ok":true,"fullscreen":true}` + list_windows에서 0,0 + 데스크탑 크기.
3. 서버 로그에 `window.fullscreen` 이벤트(read_log match).
4. 스크린샷: capture_window → 상단 행 소멸(극장 모드) 판정(경로 행 픽셀 유무).
5. 커서를 vplayer 하단으로 SendInput 이동 → OSD 등장 픽셀 판정(하단 64pt 밴드
   변화) → 2.5s 방치 → 소멸 판정(타이밍 마진 3.2s 관찰 — 플레이크 여유).
6. `on:0` → list_windows 원 rect + `window.fullscreen_exit` 이벤트.
7. teardown: 프로세스 종료.

- [ ] **Step 2: 공식런** — 클린빌드 후 vpt12 ALL PASS ×2 연속(타이밍 플레이크
  문서화 관례).

- [ ] **Step 3: 회귀** — vpt9/vpt4/vpt5/vpt11 + selftest + jkdesktop test +
  probe_agentmgr 21/21 + probe_approve_self 7/7.

- [ ] **Step 4: 커밋** — `test(vplayer): vpt12 fullscreen + OSD probe`

### Task 4: docs + 카탈로그 + 메모리

- [ ] **Step 1: docs/50 §11** as-built(결정·커밋·프로브 수·레슨) + 스펙 §6 커밋 기입.
- [ ] **Step 2: docs/32 §7.5 이벤트 카탈로그**에 window.fullscreen /
  window.fullscreen_exit 2행 추가.
- [ ] **Step 3: 메모리 동기화** — killer_app_absorption(실행 지시 갱신) +
  roadmap(다음 세션 후보) + MEMORY.md 훅.

### Task 5: opus 최종리뷰 (상임)

전체 커밋 diff + vpt12 결과 전달 → 픽스 반영 커밋 → docs/50 §11 재판정 기입.