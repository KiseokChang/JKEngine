# 스크린샷 도구 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 에이전트 도구로 클라 창/데스크탑 영역을 PNG 캡처하고, 뷰어 앱과 러버밴드 오버레이로 사용자 얼굴을 제공한다.

**Architecture:** 서버가 클라 shm 표면(RGBA32, w*h*4)을 직접 읽어 stb_image_write로 PNG 저장 (창 캡처, DPI 이슈 없음) / `SDL_RenderReadPixels`로 컴포지트 프레임을 읽어 크롭 (영역 캡처 — 요청자 레이어를 잠깐 숨겨 오버레이 제외). 오버레이(jkapp_snap)와 뷰어(jkapp_shot)는 알림 센터 템플릿의 ImGui 클라 앱.

**Tech Stack:** C++ (MinGW/ucrt64), stb_image_write v1.16 (신규 벤더링, public domain), stb_image (기존) + JKImageLoader, SDL2, ImGui.

**Spec:** docs/superpowers/specs/2026-09-09-desktop-agent-platform-design.md §7 "스크린샷 도구" (영역 캡처 + 에이전트 첨부 — capture 권한의 사용자 얼굴) + 2026-09-11 브레인스토밍 결정 (창 캡처 도구 + 뷰어 앱 + 영역 러버밴드 오버레이 전부).

## Global Constraints

- 커밋은 태스크 단위로 main에 직접, 즉시 push. `Co-Authored-By: Claude Code <noreply@anthropic.com>`.
- 빌드: `cd /i/progwork/JKENGINE/engine && export PATH="/c/msys64/ucrt64/bin:$PATH" && cmake --build build`. 출력을 grep으로 필터하지 말 것 (레슨 37). 빌드 전 살아있는 exe kill (`Get-Process jkdesktop,jkagentd,... | Stop-Process -Force`) — 링크 Permission denied.
- agentctl 프로브 페이로드 공백 금지 (레슨 32). PS는 Write 툴로 .ps1 → `-File`, BOM.
- ImGui 앱은 WM_GETTEXT 불가 — 판정은 agentctl + 파일 + list_windows (레슨 34).
- 클라 shm 표면: `Local\JKSurfaceShm_<id>`, 크기 w*h*4, 바이트 순서 RGBA32 → `stbi_write_png(path, w, h, 4, px, w*4)`로 저장 가능.
- 컴포지터: `JKCompositor::Composite()` (그리기+Present), `FindLayerById(id)` → `Pixels()/Width()/Height()`, `SetLayerVisible`. 서버 캡처는 이들을 쓴다.
- HandleAgentQuery는 clientsMutex_ 보유 중 (레슨 35 — 새 헬퍼는 락 없이).
- 캡처 저장 디렉터리: `state/screenshots/`, 파일명 `shot_<ts>_<id>.png` (ts=epoch 초, id=창 id; region은 id 자리에 `region`).

---

### Task 1: stb_image_write 벤더링 + capture_window 도구

**Files:**
- Create: `engine/third_party/stb/stb_image_write.h` (v1.16, 이미 다운로드 완료 — stb_image/truetype와 같은 public-domain 계열)
- Modify: `engine/src/server/JKWindowServer.cpp` (capture_window 도구, include)

**Interfaces:**
- Produces: 도구 `capture_window {"id":<u32>}` → `{"ok":true,"path":"<exeDir>\state\screenshots\shot_<ts>_<id>.png"}`. Task 3 뷰어가 읽는 디렉터리. `WritePngFromPixels(path, w, h, px)` 서버 정적 헬퍼 — Task 2 capture_region도 재사용.

- [ ] **Step 1: include + 헬퍼.** JKWindowServer.cpp에:

```cpp
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb_image_write.h>
```

(단일 TU에서 구현 — static으로 심볼 충돌 방지. third_party/stb는 CMake include dirs에 이미 있음.) 네임스페이스 내 헬퍼:

```cpp
// PNG write for captures (docs/35). pixels must be RGBA32 w*h*4.
bool WritePng(const std::string& path, int w, int h, const uint8_t* px) {
    return stbi_write_png(path.c_str(), w, h, 4, px, w * 4) != 0;
}
```

- [ ] **Step 2: 도구** (HandleAgentQuery의 trigger_toggle 분기 앞 — 도구 체인):

```cpp
} else if (tool == "capture_window") {
    // docs/35: read the client's shm surface (RGBA32) directly — no screen
    // DPI involvement. Safe tier (launch_app); permissions.json can deny.
    int id = 0;
    req.GetObjInt("args", "id", id);
    JKCompositorLayer* layer =
        compositor_ ? compositor_->FindLayerById(static_cast<uint32_t>(id))
                    : nullptr;
    if (!layer || !layer->Pixels() || layer->Width() <= 0 ||
        layer->Height() <= 0) {
        reply = "{\"ok\":false,\"error\":\"window_not_found\"}";
    } else {
        const std::string dir = StateDir() + "\\screenshots";
        CreateDirectoryA(dir.c_str(), nullptr);
        const long long ts =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        const std::string path = dir + "\\shot_" + std::to_string(ts) + "_" +
                                 std::to_string(id) + ".png";
        // Copy out — the client may commit into the shm while we encode.
        const int w = layer->Width(), h = layer->Height();
        std::vector<uint8_t> px(
            layer->Pixels(), layer->Pixels() + static_cast<size_t>(w) * h * 4);
        if (WritePng(path, w, h, px.data())) {
            reply = "{\"ok\":true,\"path\":\"" + JsonEsc(path) + "\"}";
        } else {
            reply = "{\"ok\":false,\"error\":\"write_failed\"}";
        }
    }
}
```

(필요 include: `<vector>` 확인, JKCompositorLayer 헤더 확인 — 실행 시 확인하고 추가.)

- [ ] **Step 3: 빌드** (mtime 확인).

- [ ] **Step 4: 실측** — 임시 .ps1: 서버 기동 → launch_app minesweeper → list_windows에서 id 추출 → `capture_window {"id":N}` → reply에 path → 파일 존재 + 크기>0 + PNG 매직(`89 50 4E 47`) 확인 → cleanup (screenshots 디렉터리 비우기는 유지 — Task 4 프로브가 정리).

- [ ] **Step 5: 커밋** — `feat(agent): capture_window tool + stb_image_write`

### Task 2: capture_region 도구 + 오버레이 제외

**Files:**
- Modify: `engine/include/server/JKCompositor.h`, `engine/src/server/JKCompositor.cpp` (Composite에 present 플래그)
- Modify: `engine/src/server/JKWindowServer.cpp` (capture_region 도구)

**Interfaces:**
- Consumes: Task 1 `WritePng`.
- Produces: 도구 `capture_region {"x":<i>,"y":<i>,"w":<i>,"h":<i>}` → `{"ok":true,"path":...}`. `JKCompositor::Composite(bool present)` — 기존 `Composite()` 호출부는 기본값으로 무영향.

- [ ] **Step 1: Composite 분해.**

```cpp
// include: void Composite(bool present = true);
// cpp:
void JKCompositor::Composite(bool present) {
    ... 기존 그리기 본체 전체 ...
    if (present) {
        SDL_RenderPresent(renderer_);
    }
}
// Composite() 호출부 호환: 기본 인자라 기존 코드 무수정.
```

실행 시 Composite 내 Present가 마지막 줄인지 확인하고 위 형태로 리팩터.

- [ ] **Step 2: 도구** — 요청자 레이어를 숨겨 셀프 캡처를 제외:

```cpp
} else if (tool == "capture_region") {
    // docs/35: composited frame readback (SDL_RenderReadPixels), then crop.
    // The requester's own layer is hidden for the readback so a rubber-band
    // overlay does not appear in its own screenshot.
    int x = 0, y = 0, w = 0, h = 0;
    req.GetObjInt("args", "x", x);
    req.GetObjInt("args", "y", y);
    req.GetObjInt("args", "w", w);
    req.GetObjInt("args", "h", h);
    uint32_t selfId = client.Id();
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384) {
        reply = "{\"ok\":false,\"error\":\"bad_request\"}";
    } else {
        // clamp into the output bounds
        const int outW = compositor_ ? compositor_->OutputWidth() : 0;
        const int outH = compositor_ ? compositor_->OutputHeight() : 0;
        ... intersect/clamp (x>=0, y>=0, w,h <= out) ...
        if (auto* self = compositor_->FindLayerById(selfId))
            self->SetVisible(false);
        compositor_->Composite(false);          // draw without present
        SDL_Rect rect{x, y, w, h};
        std::vector<uint8_t> buf(static_cast<size_t>(w) * h * 4);
        // full-surface readback then crop (RenderReadPixels needs a target
        // rect within the renderer bounds — read the output rect once):
        ... SDL_SetRenderTarget(renderer_, nullptr);
            SDL_RenderReadPixels(renderer_, &full, SDL_PIXELFORMAT_RGBA32, ...) ...
        // restore
        if (auto* self = compositor_->FindLayerById(selfId))
            self->SetVisible(true);
        compositor_->Composite(false);
        // crop from the full readback into buf, WritePng(...)
    }
}
```

**실행 시 확정 사항 (플랜 작성 시점에 미확정 — 순서):**
1. `JKCompositorOutput`에 크기 접근자가 있는지 보고 `OutputWidth()/OutputHeight()`를 JKCompositor에 추가 (없으면 delegate).
2. `SDL_RenderReadPixels`의 renderer 획득: JKWindowServer가 renderer_를 갖는지 확인 (Compositor 생성자에 넘기는 그 SDL_Renderer*). ReadPixels는 **full output 크기 1회**로 받고 C++에서 크롭 — 부분 rect ReadPixels는 드라이버별로 깨지는 사례가 있어 풀 프레임 안전.
3. `selfId`: `client.Id()` — AgentQuery가 윈도우 클라 연결에서 오면 그 클라의 레이어 id. 컨트롤 전용 연결(에이전트)이면 레이어가 없어 `FindLayerById` null → 그냥 전체 캡처(오버레이가 없으므로 문제 없음).

- [ ] **Step 3: 빌드.**

- [ ] **Step 4: 실측** — 서버+minesweeper → `capture_region {"x":0,"y":0,"w":200,"h":150}` → PNG 존재 + 매직 + 정확한 크기 (PowerShell System.Drawing으로 실측 200×150) → `capture_window`와 비교(같은 대상이어도 파일 2개).

- [ ] **Step 5: 커밋** — `feat(agent): capture_region tool with self-layer exclusion`

### Task 3: 러버밴드 오버레이 jkapp_snap

**Files:**
- Create: `engine/include/apps/ClientSnapApp.h`, `engine/src/apps/ClientSnapApp.cpp`, `engine/src/apps/JKAppModule_snap.cpp`
- Modify: `engine/CMakeLists.txt` (jkapp_snap 타깃 + snap.jkx)

**Interfaces:**
- Consumes: Task 2 도구; `JKClientSurface::SendAgentQuery(queryId, json)` / `PollAgentReply(AgentReply&)` (팔레트 1-deep 패턴).
- Produces: 앱 "snap" — chromeless 클라 창 (큰 rect 요청 → 서버 clamp로 데스크탑 크기), 드래그 사각형 → 마우스 릴리즈 시 `AgentQuery capture_region {자기 창 rect 좌표계}` → reply 수신 → 자기 창 닫기 (앱 종료). ESC 취소.

- [ ] **Step 1: 앱 구현.** 알림 센터 템플릿 (16ms 타이머 + PreProcessMessage + RenderOverlay). ImGui 대신 순수 그리기도 가능하지만 ImGui로 통일:

```cpp
// OnInit: 큰 rect 요청 (서버가 clamp) — SetWindowRect(JKRect{0,0,99999,99999}),
// WA_CHROMELESS, 타이틀 "Region Capture", SetTimerInterval(16)
// 상태: idle / dragging(startX,startY) — 마우스 down에서 dragging 시작,
// 이동 중 rect 갱신, up에서 전송.
// RenderOverlay: ImGui fullscreen 투명 배경 (ImVec4(0,0,0,0.25f)),
// dragging이면 선택 사각형 테두리(앰버) 그림.
// 마우스 up: rect를 자기 창의 GetClientRect() 오프셋으로 서버 좌표 변환 —
// 창이 (0,0)에서 시작하므로 클라 좌표 = 데스크탑 좌표. SendAgentQuery로
// {"tool":"capture_region","args":{"x":..,"y":..,"w":..,"h":..}} 전송
// (queryId_++), 다음 프레임부터 PollAgentReply — ok가 오거나 타임아웃(3s)이면
// 자기 창 Close (앱 종료). 캡처 실패 시에도 종료 (토스트 없음 — 재시도 UX는 후속).
```

전송에 공백 없는 JSON. JKEventType::KeyUp에서 SDLK_ESCAPE → 자기 창 닫기.

- [ ] **Step 2: JKAppModule_snap.cpp** — `static const jk::JKAppMeta meta{ "snap", "Region Capture", 800, 600 };`

- [ ] **Step 3: CMake** — palette/notify 블록 뒤에 동일 패턴으로 jkapp_snap + JKX_ICON_APPS += snap + snap.jx 커스텀 커맨드 + jkx_packages DEPENDS. (아이콘 없이 jkx만 — 런처 폴백 아이콘은 후속; 뷰어 앱이 진입점이므로 snap은 오버레이 전용.)

**주의:** snap.jkx는 `--jkx` 스폰이 아니라 launch_app이면 `SpawnClient("snap")` — 앱 이름 등록이 CMake JKX_ICON_APPS/서버 스캔 어느 쪽인지 실행 시 확인 (notify 패턴 그대로 복제).

- [ ] **Step 4: 빌드 + 실측** — `agentctl launch_app {"app":"snap"}` → list_windows에 "Region Capture" 창 → 캡처 동작은 Task 4에서 도구 직접 호출로 검증 (오버레이 드래그는 사용자 실측 항목).

- [ ] **Step 5: 커밋** — `feat(shot): rubber-band capture overlay app`

### Task 4: 뷰어 앱 jkapp_shot + 팔레트 /shot

**Files:**
- Create: `engine/include/apps/ClientShotApp.h`, `engine/src/apps/ClientShotApp.cpp`, `engine/src/apps/JKAppModule_shot.cpp`
- Create: `engine/tools/probes/gen_shot_icon.ps1`, `engine/assets/icons/launcher_shot@{1x,2x}.png`
- Modify: `engine/CMakeLists.txt`, `engine/src/apps/ClientPaletteApp.cpp` (/shot)

**Interfaces:**
- Consumes: `jk::LoadImageFile(path, LoadedImage&)` (RGBA8), Task 1/2 도구, notify 앱의 SDL 텍스처 뷰 방식.
- Produces: 앱 "shot" — "Screenshots" 창 (480×420): 파일 목록(`state/screenshots/*.png`, 최신순) + 선택 → `LoadImageFile` → SDL 텍스처 → `ImGui::Image` (폭 460에 맞춰 등비 축소). "영역 캡처" 버튼 → `launch_app snap`.

- [ ] **Step 1: 앱 구현** (notify 템플릿):

```cpp
// 목록: FindFirstFileA(stateDir + "\\screenshots\\*.png"), 파일시간 내림차순.
// 선택: LoadImageFile → LoadedImage.rgba (w*h*4 RGBA8) →
//   SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, STREAMING, w, h)
//   + SDL_UpdateTexture → ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(scaledW, scaledH))
//   (renderer는 RenderOverlay 인자 — notify가 ImGui 백엔드 init에 쓰는 것과 동일)
// 텍스처 수명: 선택 바뀔 때 이전 텍스처 Destroy (누수 방지).
// 한글 폰트: Malgun Gothic (notify와 동일).
// 새로고침 버튼: 목록 재스캔 (다른 창에서 새 캡처가 생긴 뒤 누름).
```

- [ ] **Step 2: 아이콘** — gen_notify_icon.ps1 복제해 카메라 글리프(앰버 #d8a24a 바디 사각형 + 렌즈 원)로 generator 작성, @1x/@2x 생성, CMake JKX_ICON_APPS 연결.

- [ ] **Step 3: CMake + 팔레트** — jkapp_shot 타깃 + shot.jkx + DEPENDS 체인 (notify 패턴 복제). Submit에:

```cpp
} else if (cmd == "shot") {
    // Open the screenshot viewer (docs/35).
    SendTool("launch_app", "{\"app\":\"shot\"}");
}
```
/help에 `/shot` 추가.

- [ ] **Step 4: 빌드 + 실측** — capture_window로 PNG 생성 → launch_app shot → list_windows "Screenshots" → (WM_GETTEXT 불가 — 렌더는 사용자 실측).

- [ ] **Step 5: 커밋** — `feat(shot): screenshot viewer app + /shot + icon`

### Task 4: 프로브 + docs/35 + 회귀

**Files:**
- Create: `engine/tools/probes/probe_agent_shot.ps1` (BOM)
- Create: `docs/35_desktop_agent_shot.md`
- Modify: docs/32는 건드리지 않음, spec §7 스크린샷 도구 행, memory roadmap

- [ ] **Step 1: probe** — 체크 6종:
  1. capture-window: 서버+minesweeper → capture_window → ok+path, PNG 매직 + 크기>0.
  2. capture-region: `{"x":10,"y":10,"w":120,"h":90}` → ok + PNG 매직.
  3. png-size: System.Drawing으로 region PNG 실측 120×90.
  4. overlay-spawn: launch_app snap → list_windows에 "Region Capture".
  5. viewer-spawn: launch_app shot → list_windows에 "Screenshots".
  6. viewer-reachable: 다시 launch_app shot → 창 수 변화 없음 (launch_app은 토글 아님 — 중복 스폰이면 FAIL... **실행 시 확인**: SpawnClient는 매번 스폰하는가? notify에서 launch_app 2회 경험 기준: 서버가 app 단위 쓰로틀(spawn throttle) 있음 — 2회째는 throttle로 거부/지연. 프로브는 창 수 1 유지만 확인).
  - cleanup: state\screenshots 삭제 + 프로세스 kill.
- [ ] **Step 2: probe PASS** + 전체 회귀 (기존 8종).
- [ ] **Step 3: docs/35 + spec §7 + memory + 커밋푸시** `feat(shot): probe + docs/35 — screenshot capture complete`.

## Self-Review

- 스펙 커버리지: capture_window(1)/capture_region+오버레이(2,3)/뷰어(4)/검증·문서(4) — 커버.
- 플레이스홀더: Task 2 Step 2의 Composite present 플래그 리팩터와 OutputWidth/Height 추가는 "실행 시 확인" 지시가 붙어 있고 대안 없음 — 기존 코드 확인 후 그대로 구현하면 됨 (정확한 코드는 Composite 본체 리팩터이므로 사전 확정 불가 — 명시 표기).
- 타입 일관성: state/screenshots, shot_<ts>_<id>.png, WritePng(path,w,h,px) — 1↔2 일치. snap은 자기 rect를 데스크탑 좌표로 전송(창 (0,0) 기준) — 2↔3 일치.