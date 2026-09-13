# P1 engine/desktop 3-lib 분할 — 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `jkdesktop.exe`의 서버/셸/클라 코드를 3-lib(jkcore/jkclient/jkserver)로 분할하고 셸(launcher)을 desktop/ 유닛으로 추출한 뒤 `jkwinserver.exe` thin 호스트를 만든다.

**Architecture:** 점진적 3단계(스펙 §3) — ① jkserver static lib 추출, ② jkclient static lib 추출 + 앱 링크 스왑, ③ desktop/ 셸 유닛 + jkwinserver.exe. 각 단계는 빌드 그린 + 셀프테스트 + 프로브 회귀로 독립 검증. 라이브러리 경계만 움직이고 동작 변경은 없다 (스펙 D4).

**Tech Stack:** CMake (MinGW/Ninja, Windows), SDL2, C++17, 기존 프로브 스크립트 (`engine/tools/probes/*.ps1`).

**Spec:** `docs/superpowers/specs/2026-09-13-engine-desktop-split-design.md` — 결정/직감 장부(D1–D8) 포함. 실행자는 스펙+플랜을 같이 읽는다.

## Global Constraints

- 빌드: `I:/progwork/JKENGINE/engine/build`에서 `cmake --build .` (MinGW/Ninja). 서브셋 빌드는 `--target <name>`.
- 셀프테스트: 빌드 디렉터리에서 `./jkdesktop test` → 출력에 `0 failures` 필수.
- **동작 변경 금지**: 스펙 §1 "구조 이동만". 색/텍스트/로그 문구는 그대로 유지 (`JKWindowServer: ...` 로그 문구 포함 — 스펙 D6).
- **exe 모드 이름 불변**: `jkdesktop --server` 모드 유지 (스펙 §3③), 모든 프로브는 계속 `jkdesktop.exe`를 구동.
- **jkserver는 jkclient를 링크하지 않는다** (스펙 §2 의존 방향). desktop/ 셸 유닛은 jkserver 헤더를 인클루드하지 않는다 (스펙 D7 — 서비스는 `ShellHost` 콜백으로 주입).
- 레슨 37: 빌드 출력을 grep으로 필터하지 말 것 — 실패(살아있는 exe 링크 오류)를 스테일 바이너리로 오판할 수 있음. 빌드 후 exe mtime > 최신 소스 mtime 확인.
- 레슨 18: 클라이언트 모듈 소스가 움직이면 `.jkx` 재팩 필요 — `cmake --build .` (기본 ALL)이 `jkx_packages`를 자동 재실행하므로 풀 빌드를 쓰는 한 신경 쓸 것 없음.
- 커밋은 각 태스크 끝마다. 커밋 메시지 끝에 `Co-Authored-By: Claude Code <noreply@anthropic.com>`.

**회귀 프로브 전체 목록** (풀 게이트, `engine/tools/probes/` 기준 — 각각 서버를 스폰하므로 **순차 실행**, 한 프로브가 끝나 서버를 정리한 뒤 다음 것):

`probe_agent_mcp.ps1, probe_agent_e2e.ps1, probe_agent_palette.ps1, probe_agent_chat.ps1, probe_agent_chat_llm.ps1, probe_agent_triggers.ps1, probe_agent_trust.ps1, probe_agent_ratelimit.ps1, probe_agent_notify.ps1, probe_agent_triggerctl.ps1, probe_agent_shot.ps1, probe_agent_maximize.ps1, probe_desktop_resize.ps1, probe_terminal_mouse.ps1, probe_terminal_select.ps1`

- `probe_agent_ratelimit.ps1`은 60s 로컬 윈도 대기로 ~70초 걸린다 (레슨/스펙 문서화됨) — 실패로 오독하지 말 것.
- 프로브 실행 관례: `powershell -ExecutionPolicy Bypass -File <probe>` — 로그/판정은 각 프로브가 자체 출력 (모두 PASS 체크 포함).

---

## Stage ① — jkserver 추출

### Task 1: 파이프명 상수화

**Files:**
- Create: `engine/include/ipc/JKWireEndpoints.h`
- Modify: `engine/src/main.cpp:2789`, `:2795`, `:2889`, `engine/include/agent/JKAgentClient.h:38`

**Interfaces:**
- Produces: `jk::ipc::kWindowServerPipeName` (`const char[]` 상수) — Task 5의 jkwinserver main이 소비.

- [ ] **Step 1: 상수 헤더 작성**

`engine/include/ipc/JKWireEndpoints.h` (신규):

```cpp
#ifndef JK_WIRE_ENDPOINTS_H
#define JK_WIRE_ENDPOINTS_H

// Wire endpoint names shared by every process on the bus (server host,
// client host, agent clients). The server's acceptor listens on the same
// name all clients connect to — one constant, no per-file literals.
namespace jk {
namespace ipc {

inline constexpr char kWindowServerPipeName[] = "\\\\.\\pipe\\JKWindowServerPipe";

} // namespace ipc
} // namespace jk

#endif // JK_WIRE_ENDPOINTS_H
```

- [ ] **Step 2: main.cpp 3곳 교체**

main.cpp 상단 include에 `#include <ipc/JKWireEndpoints.h>` 추가 (기존 `#include <ipc/...>` 블록 옆). 그리고:

- :2789 `server.StartAcceptor("\\\\.\\pipe\\JKWindowServerPipe");` → `server.StartAcceptor(jk::ipc::kWindowServerPipeName);`
- :2795 `constexpr const char* kPipe = "\\\\.\\pipe\\JKWindowServerPipe";` → `constexpr const char* kPipe = jk::ipc::kWindowServerPipeName;`
- :2889 동일 교체.

- [ ] **Step 3: JKAgentClient 기본 인자 교체**

`include/agent/JKAgentClient.h:38`:
```cpp
bool Connect(const std::string& pipeName = "\\\\.\\pipe\\JKWindowServerPipe");
```
→
```cpp
bool Connect(const std::string& pipeName = jk::ipc::kWindowServerPipeName);
```
(`#include <ipc/JKWireEndpoints.h>` 추가. 헤더의 기본 인자가 상수를 참조하므로 인클루드 필수.)

- [ ] **Step 4: 빌드 + 리터럴 잔존 확인**

```bash
cd I:/progwork/JKENGINE/engine/build && cmake --build . 2>&1 | tail -5
grep -rn "JKWindowServerPipe" ../src ../tools ../include | grep -v "JKWireEndpoints"
```
Expected: 빌드 성공, grep 결과 없음 (상수 헤더 외 리터럴 0).

- [ ] **Step 5: 셀프테스트**

```bash
./jkdesktop test 2>&1 | tail -3
```
Expected: `0 failures`.

- [ ] **Step 6: Commit**

```bash
cd I:/progwork/JKENGINE && git add engine/include/ipc/JKWireEndpoints.h engine/src/main.cpp engine/include/agent/JKAgentClient.h
git commit -m "refactor(ipc): unify pipe name into JKWireEndpoints constant (P1 ①)"
```

### Task 2: jkserver static lib 추출

**Files:**
- Modify: `engine/CMakeLists.txt:170-173` (jkcore 소스 목록), `:230` (jkdesktop 링크), 신규 블록 jkcore 뒤(≈:200)

**Interfaces:**
- Produces: `jkserver` static target — Task 5의 jkwinserver가 링크. 앱 DLL·jkclient·agent 프로세스는 이 타깃을 절대 링크하지 않는다.

- [ ] **Step 1: jkcore 소스 목록에서 서버 4개 제거**

CMakeLists.txt jkcore 블록(:121-180)에서 다음 4행 삭제:

```cmake
    src/server/JKClientConnection.cpp
    src/server/JKWindowServer.cpp
    src/server/JKCompositor.cpp
    src/server/JKCompositorOutput.cpp
```

(JKCompositorLayer는 헤더 전용 — 소스 목록에 없음.)

- [ ] **Step 2: jkserver 타깃 신설**

jkcore의 `target_link_libraries(jkcore PUBLIC ...)`/`imm32` 블록(:195-199) 바로 뒤에:

```cmake
# ---------------------------------------------------------------------------
# jkserver (P1 split ①, spec §3①): the compositor/window-server side. Clients
# reach the server only over wire IPC, so it never links the client stack.
# jkdesktop links it next to jkcore — main.cpp hosts server + client modes
# in one exe until the jkwinserver split (stage ③).
# ---------------------------------------------------------------------------
add_library(jkserver STATIC
    src/server/JKClientConnection.cpp
    src/server/JKWindowServer.cpp
    src/server/JKCompositor.cpp
    src/server/JKCompositorOutput.cpp
)
target_link_libraries(jkserver PUBLIC jkcore)
```

- [ ] **Step 3: jkdesktop에 링크**

:230 `target_link_libraries(jkdesktop PRIVATE jkcore)` → `target_link_libraries(jkdesktop PRIVATE jkcore jkserver)`

- [ ] **Step 4: 풀 빌드**

```bash
cd I:/progwork/JKENGINE/engine/build && cmake --build . 2>&1 | tail -5
```
Expected: 성공 (libjkserver.a 생성, jkdesktop 리링크). 서버 헤더 소비자는 main.cpp뿐임이 이미 확인돼 있어 (grep `server/JK*.h` → src/main.cpp만) 컴파일 오류가 나면 main.cpp의 include만 점검.

- [ ] **Step 5: 셀프테스트**

```bash
./jkdesktop test 2>&1 | tail -3
```
Expected: `0 failures` (test 모드의 서버 경로 커버리지 포함).

- [ ] **Step 6: 풀 게이트 — 프로브 15종 순차 실행**

위 "회귀 프로브 전체 목록" 순서대로 `powershell -ExecutionPolicy Bypass -File engine/tools/probes/probe_<name>.ps1`. 모두 PASS. 하나라도 FAIL이면 원인 규명 후 재실행 — 스테일 exe(mtime 확인)부터 의심.

- [ ] **Step 7: Commit**

```bash
cd I:/progwork/JKENGINE && git add engine/CMakeLists.txt
git commit -m "build: extract jkserver static lib from jkcore (P1 ①)"
```

---

## Stage ② — jkclient 경계 정리

### Task 3: jkclient static lib + 앱 링크 스왑

**Files:**
- Modify: `engine/CMakeLists.txt` (jkcore 목록, 신규 jkclient 타깃, jkapp_* 링크 스왑, jkdesktop 링크)

**Interfaces:**
- Produces: `jkclient` static target — 모든 `jkapp_*` 앱 모듈 DLL이 링크 (jkcore superset). jkserver는 링크하지 않음 (의존 방향 불변).

- [ ] **Step 1: jkcore 목록에서 클라/터미널 제거**

CMakeLists.txt jkcore 블록에서 삭제:

```cmake
    src/client/JKClientSurface.cpp
    src/client/JKClientApplication.cpp
    src/terminal/JKVtParser.cpp
    src/terminal/JKTerminalGrid.cpp
    src/terminal/JKGlyphAtlas.cpp
    src/terminal/JKConPtyBridge.cpp
    src/apps/JKTerminalConfig.cpp
```

- [ ] **Step 2: jkclient 타깃 신설** (jkserver 블록 뒤):

```cmake
# ---------------------------------------------------------------------------
# jkclient (P1 split ②, spec §3②): the client-application stack every app
# module links — surfaces, the client app loop, the terminal stack and the
# terminal config. Statically embedded into every module DLL like jkcore
# (the C ABI in JKAppModule.h still isolates module boundaries).
# ---------------------------------------------------------------------------
add_library(jkclient STATIC
    src/client/JKClientSurface.cpp
    src/client/JKClientApplication.cpp
    src/terminal/JKVtParser.cpp
    src/terminal/JKTerminalGrid.cpp
    src/terminal/JKGlyphAtlas.cpp
    src/terminal/JKConPtyBridge.cpp
    src/apps/JKTerminalConfig.cpp
)
target_link_libraries(jkclient PUBLIC jkcore)
```

- [ ] **Step 3: 모든 앱 모듈 링크 스왑**

21개 타깃의 `PRIVATE jkcore` → `PRIVATE jkclient` (부가 deps 보존):

`jkapp_minesweeper, jkapp_tetris, jkapp_testwin, jkapp_jango, jkapp_occ, jkapp_pcx, jkapp_vector, jkapp_iconedit, jkapp_recog, jkapp_vfont, jkapp_vpres, jkapp_terminal, jkapp_script, jkapp_taskbar, jkapp_imguidemo, jkapp_taskmgr, jkapp_palette, jkapp_notify, jkapp_snap, jkapp_shot, jkapp_vplayer, jkapp_browser`

예: `target_link_libraries(jkapp_imguidemo PRIVATE jkclient imgui)`, `target_link_libraries(jkapp_taskmgr PRIVATE jkclient imgui implot)`, `target_link_libraries(jkapp_vplayer PRIVATE jkclient imgui PkgConfig::FFMPEG)`, `jkapp_browser`는 CEF 링크 유지.

- **jkagentd / jkchat / jktriggers는 jkcore 유지** — 이들은 클라 스택을 쓰지 않는다 (JKAgentClient/JKAgentJson은 jkcore에 잔존).
- `jkdesktop`: `PRIVATE jkcore jkserver` → `PRIVATE jkcore jkserver jkclient` (main.cpp의 test 모드가 터미널 그리드/파서/뷰어 셀프테스트를 직접 구동).

- [ ] **Step 4: 풀 빌드 + 셀프테스트**

```bash
cd I:/progwork/JKENGINE/engine/build && cmake --build . 2>&1 | tail -5
./jkdesktop test 2>&1 | tail -3
```
Expected: 빌드 성공(모든 jkapp DLL 재링크 + .jkx 재팩), `0 failures`. **빌드 후 jkdesktop.exe/jkapp_*.dll mtime > 소스 mtime 확인 (레슨 37/18)** — 살아있는 exe로 링크 실패가 가려지면 프로세스 kill 후 재빌드.

- [ ] **Step 5: 풀 게이트 — 프로브 15종**

Expected: 모두 PASS. 앱 DLL 링크가 전부 바뀌었으므로 특히 `probe_agent_e2e`(minesweeper 스폰), `probe_agent_triggers`, `probe_terminal_*`에 신경 쓸 것.

- [ ] **Step 6: Commit**

```bash
cd I:/progwork/JKENGINE && git add engine/CMakeLists.txt
git commit -m "build: extract jkclient static lib; app modules link jkclient (P1 ②)"
```

---

## Stage ③ — desktop/ 셸 추출 + jkwinserver.exe

### Task 4: JKDesktopShell 유닛 추출

**Files:**
- Create: `engine/include/desktop/JKDesktopShell.h`, `engine/src/desktop/JKDesktopShell.cpp`
- Modify: `engine/include/server/JKWindowServer.h`, `engine/src/server/JKWindowServer.cpp`, `engine/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 2의 jkserver.
- Produces: `jk::desktop::JKDesktopShell` (`Init(ShellHost)` / `Draw(SDL_Renderer*)` / `Destroy()` / `HitTest(x,y)`), `jk::desktop::JKDesktopShell::ShellHost{renderer, outputScale, makeTexture, launch}`. Task 5의 jkwinserver는 jkserver 링크로 자동 획득.

- [ ] **Step 1: 셸 헤더 작성**

`engine/include/desktop/JKDesktopShell.h`:

```cpp
#ifndef JKDESKTOPSHELL_H
#define JKDESKTOPSHELL_H

#include <JKTypes.h>
#include <SDL.h>

#include <functional>
#include <string>
#include <vector>

namespace jk {
struct LoadedImage;
}

namespace jk {
namespace desktop {

// In-process privileged shell (P1 ③, spec D7): the launcher desktop. Owns
// the icon grid, the background photo and the .jkx scan. Every service it
// needs from the server arrives through the injected ShellHost callbacks —
// this unit never includes jkserver headers, so a future out-of-process
// shell can reuse it as-is.
class JKDesktopShell {
public:
    struct ShellHost {
        SDL_Renderer* renderer = nullptr;
        std::function<float()> outputScale;
        std::function<SDL_Texture*(const jk::LoadedImage&, const char*)> makeTexture;
        std::function<void(const char*, bool)> launch;  // (appName, fromJkx)
    };

    // Scan apps/*.jkx, add built-in fallbacks, load the background photo,
    // lay the grid out and draw once (the per-frame draw happens via Draw
    // from Composite).
    void Init(const ShellHost& host);

    // Frame background painter — the server's Composite() calls this before
    // compositing layers. No-op with no icons (legacy empty-desktop behavior).
    void Draw(SDL_Renderer* renderer);

    void Destroy();

    // Physical-pixel hit test → launcher icon index, or -1.
    int HitTest(int x, int y) const;

private:
    struct LauncherIcon {
        JKRect rect;
        std::string appName;   // spawn key / display name
        std::string jkxPath;   // non-empty → spawn "--jkx <path>"
        SDL_Texture* texture = nullptr;
    };

    void ScanJkxApps();
    void RelayoutLauncherIcons();
    SDL_Texture* LoadTextureScaled(const char* assetBase);

    ShellHost host_;
    std::vector<LauncherIcon> launcherIcons_;
    SDL_Texture* backgroundTexture_ = nullptr;
};

} // namespace desktop
} // namespace jk

#endif // JKDESKTOPSHELL_H
```

- [ ] **Step 2: 셸 구현 이동**

`engine/src/desktop/JKDesktopShell.cpp` (신규) — JKWindowServer.cpp의 `InitLauncher`(:2676), `RelayoutLauncherIcons`(:2733), `ScanJkxApps`(:2749), `DrawLauncher`+:`DrawLauncherBackground`(:2802, :2806) 및 `LoadTextureScaled`(:2660) 본문을 `namespace jk::desktop` 안으로 이동. 변환 규칙:

- `JKWindowServer::` 접두 제거, `renderer_` → `host_.renderer`, `compositor_ ? compositor_->OutputScale() : 1.0f` → `host_.outputScale ? host_.outputScale() : 1.0f`, `TextureFromRGBA(...)` → `host_.makeTexture(...)`, `launcherIcons_`/`backgroundTexture_`는 멤버 유지.
- `InitLauncher` → `Init(host_)`: `if (!renderer_) return;` → `if (!host_.renderer) return;`, `InitLauncher` 서두의 `launcherIcons_.clear()`는 유지(재초기화 안전), 끝의 `DrawLauncher();`는 `Draw(host_.renderer);`로. `ScanJkxApps()`/`RelayoutLauncherIcons()` 호출은 그대로.
- `DrawLauncherBackground` → `Draw(SDL_Renderer*)`: 시그니처만 바꾸고 본문 유지 (`if (!renderer_ || launcherIcons_.empty()) return;`의 `renderer_`는 파라미터 사용).
- `DrawLauncher()`(2행 래퍼)는 병합되어 소멸.
- 로그 문구(`JKWindowServer: launcher icon ...`) 그대로 유지 — 스펙 D6(동작 변경 금지).
- 필요 include: `<desktop/JKDesktopShell.h>`, `<JKImageLoader.h>`, `<JKJkxFile.h>`, SDL, Windows(ScanJkxApps의 FindFirstFileA 등 — 기존 전제 유지, `#ifdef _WIN32` 유지), `<cstdio>`/`<cstring>`.

- [ ] **Step 3: 서버 헤더 정리** (`include/server/JKWindowServer.h`)

- 전방선언 추가 (기존 `class JKMessageBus;`들 곁): 
```cpp
namespace desktop { class JKDesktopShell; }
```
(jk 네임스페이스 안, :24 근처)
- 프라이빗 메서드 8개 삭제: `InitLauncher, ScanJkxApps, RelayoutLauncherIcons, DrawLauncher, DrawLauncherBackground, DestroyLauncher, HitTestLauncherIcon, LoadTextureScaled` (:138-154 영역 — `TextureFromRGBA`는 **유지**, ShellHost.makeTexture로 노출됨).
- 멤버 삭제: `LauncherIcon` 구조체+`launcherIcons_`(:291-297), `backgroundTexture_`(:300). `lastSpawnTimes_`/`spawnedClients_`는 유지 (프로세스 생애주기 = 서버 소유).
- 멤버 추가:
```cpp
    // In-process privileged shell (P1 ③): owns the launcher grid + desktop
    // background. Wired in Init, torn down in the destructor.
    std::unique_ptr<desktop::JKDesktopShell> shell_;
```

- [ ] **Step 4: 서버 구현 재배선** (`src/server/JKWindowServer.cpp`)

- 상단 include: `#include <desktop/JKDesktopShell.h>` 추가.
- `Init()`의 `InitLauncher();`(:170) → 셸 호스트 와이어링:
```cpp
    // P1 ③: the launcher is the in-process privileged shell (spec D7) — the
    // shell owns the grid + background; the server only supplies host
    // services through ShellHost (renderer, scale, texture factory, spawn).
    jk::desktop::JKDesktopShell::ShellHost shellHost;
    shellHost.renderer = renderer_;
    shellHost.outputScale = [this]() {
        return compositor_ ? compositor_->OutputScale() : 1.0f;
    };
    shellHost.makeTexture = [this](const jk::LoadedImage& img, const char* label) {
        return TextureFromRGBA(img, label);
    };
    shellHost.launch = [this](const char* app, bool fromJkx) {
        SpawnClient(app, fromJkx);
    };
    shell_ = std::make_unique<jk::desktop::JKDesktopShell>();
    shell_->Init(shellHost);
```
- 소멸자의 `DestroyLauncher();`(:513) → `if (shell_) { shell_->Destroy(); shell_.reset(); }`
- `Composite(bool)`의 `DrawLauncherBackground();`(:2562) → `shell_->Draw(renderer_);`
- `HandleSDLEvent`의 `HitTestLauncherIcon(x, y)` 호출부 → `shell_ ? shell_->HitTest(x, y) : -1` (반환 사용부 그대로).
- 함수 본문 7개 삭제: `InitLauncher/RelayoutLauncherIcons/ScanJkxApps/DrawLauncher/DrawLauncherBackground/DestroyLauncher/HitTestLauncherIcon/LoadTextureScaled` (`TextureFromRGBA` 유지).
- `JkxFindData`/`FindFirstFileA`류가 ScanJkxApps 전용이면 그 선언도 이동(컴파일이 알려줌).

- [ ] **Step 5: CMake** — jkserver 블록 뒤:

```cmake
# ---------------------------------------------------------------------------
# jkdesktop_shell (P1 ③, spec D7): the in-process privileged shell — launcher
# grid, desktop background, .jkx scan. Compiles against jkcore+SDL only; the
# server links it and wires services via ShellHost. jkserver PUBLIC-links it
# so the exe targets get the shell transitively.
# ---------------------------------------------------------------------------
add_library(jkdesktop_shell STATIC
    src/desktop/JKDesktopShell.cpp
)
target_include_directories(jkdesktop_shell PUBLIC
    include
    third_party/stb
)
target_link_libraries(jkdesktop_shell PUBLIC jkcore)
target_link_libraries(jkserver PUBLIC jkdesktop_shell)
```

- [ ] **Step 6: 풀 빌드 + 셀프테스트**

```bash
cd I:/progwork/JKENGINE/engine/build && cmake --build . 2>&1 | tail -5
./jkdesktop test 2>&1 | tail -3
```
Expected: `0 failures`.

- [ ] **Step 7: 퀵 게이트** — `probe_desktop_resize.ps1` + `probe_agent_e2e.ps1` + `probe_agent_shot.ps1` (런처 셀 스폰·배경 드로잉이 바뀌는 표면이므로 e2e의 스폰 경로 + shot의 배경 readback이 최소 검증). Expected: PASS.

- [ ] **Step 8: Commit**

```bash
cd I:/progwork/JKENGINE && git add engine/include/desktop engine/src/desktop engine/include/server/JKWindowServer.h engine/src/server/JKWindowServer.cpp engine/CMakeLists.txt
git commit -m "refactor(server): extract desktop shell unit behind ShellHost (P1 ③a)"
```

### Task 5: jkwinserver.exe thin 호스트

**Files:**
- Create: `engine/src/jkwinserver_main.cpp`
- Modify: `engine/include/server/JKWindowServer.h` (SetClientHostExe), `engine/src/server/JKWindowServer.cpp` (SpawnClient), `engine/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 2의 jkserver, Task 1의 `jk::ipc::kWindowServerPipeName`, Task 4의 셸(전이 링크).
- Produces: `jkwinserver.exe` (빌드 디렉터리, 콘솔 서브시스템 — stderr 로그를 프로브가 읽음), `JKWindowServer::SetClientHostExe(const char*)`.

- [ ] **Step 1: 호스트 exe명 주입** — 헤더 public 섹션에:

```cpp
    // P1 ③: which exe hosts client modules (SpawnClient builds its command
    // line from this). The server exe name and the client host name are
    // different concerns — jkwinserver.exe spawns jkdesktop.exe clients.
    void SetClientHostExe(const std::string& exeName) { clientHostExe_ = exeName; }
```
멤버 (launcher 상태 옆): `std::string clientHostExe_ = "jkdesktop.exe";` — `#include <string>`은 이미 있음.
`SpawnClient`의 `SpawnProcess("jkdesktop.exe", ...)` 2곳 → `SpawnProcess(clientHostExe_.c_str(), ...)`.

- [ ] **Step 2: thin main 작성** — `engine/src/jkwinserver_main.cpp`:

```cpp
// jkwinserver.exe — the thin window-server host (P1 ③, spec §2). The desktop
// shell stays in-process (privileged shell, spec D7) and the client host is
// jkdesktop.exe, declared here explicitly via SetClientHostExe. Window title
// must stay "JKENGINE Window Server" — probes find the server with it
// (maximize-probe lesson: FindWindow("SDL_app", title)).
#include <server/JKWindowServer.h>
#include <ipc/JKWireEndpoints.h>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    jk::server::JKWindowServer server;
    if (!server.Init("JKENGINE Window Server", 1280, 720)) {
        return 1;
    }
    server.SetClientHostExe("jkdesktop.exe");
    server.StartAcceptor(jk::ipc::kWindowServerPipeName);
    server.Run();
    return 0;
}
```

- [ ] **Step 3: CMake** — jkagentd 블록 근처(툴 exe들 사이)에:

```cmake
# ---------------------------------------------------------------------------
# jkwinserver.exe (P1 ③, spec §2): thin server host. The client host exe is
# jkdesktop.exe (SetClientHostExe) — smoke scripts keep driving jkdesktop,
# which keeps its --server mode unchanged.
# ---------------------------------------------------------------------------
add_executable(jkwinserver src/jkwinserver_main.cpp)
target_link_libraries(jkwinserver PRIVATE jkserver)
if(WIN32)
    target_link_options(jkwinserver PRIVATE -static-libstdc++ -static-libgcc)
endif()
```
POST_BUILD 자산 복사(jkdesktop과 동일 명령, :563 블록 복제):
```cmake
add_custom_command(TARGET jkwinserver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:jkwinserver>/assets"
)
```

- [ ] **Step 4: 빌드 + 셀프테스트** (`cmake --build .` → jkwinserver.exe 산출, `./jkdesktop test` → `0 failures`)

- [ ] **Step 5: jkwinserver 수동 스모크** (PS1 파일로 작성해서 실행 — 레슨 27/50: 인라인 인자 인용 지양, 비ASCII 없음):

```powershell
# tmp/smoke_jkwinserver.ps1 — boot the thin host, verify title + spawn path.
$proc = Start-Process -FilePath "I:\progwork\JKENGINE\engine\build\jkwinserver.exe" -PassThru
Start-Sleep -Seconds 4
$found = Get-Process | Where-Object { $_.MainWindowTitle -eq "JKENGINE Window Server" }
if (-not $found) { Write-Output "FAIL: server window not found"; Stop-Process -Id $proc.Id -Force; exit 1 }
# Client spawn through the injected host exe: focus a launcher cell via the
# agent API (agentctl is on PATH from the build dir).
$resp = & "I:\progwork\JKENGINE\engine\build\jkdesktop.exe" agentctl launch_app '{"app":"minesweeper"}'
Write-Output "launch: $resp"
Start-Sleep -Seconds 3
$win = & "I:\progwork\JKENGINE\engine\build\jkdesktop.exe" agentctl list_windows '{}'
if ($win -notmatch 'minesweeper') { Write-Output "FAIL: minesweeper not spawned"; Stop-Process -Id $proc.Id -Force; exit 1 }
Write-Output "PASS"
Get-Process | Where-Object { $_.MainWindowTitle -like '*minesweeper*' -or $_.Path -like '*jkdesktop*' } | Stop-Process -Force -ErrorAction SilentlyContinue
Stop-Process -Id $proc.Id -Force
```
(agentctl 서브커맨드/JSON 형식은 실행 전 `engine/src/main.cpp`의 `agentctl` 분기(:2616)와 기존 프로브의 호출부를 확인해 정확히 맞춘다 — 이 스모크의 판정은 "서버 창 존재 + minesweeper 윈도우 스폰" 2건이면 충분.)

- [ ] **Step 6: Commit**

```bash
cd I:/progwork/JKENGINE && git add engine/src/jkwinserver_main.cpp engine/include/server/JKWindowServer.h engine/src/server/JKWindowServer.cpp engine/CMakeLists.txt
git commit -m "feat(build): add jkwinserver.exe thin server host (P1 ③b)"
```

### Task 6: 최종 풀 게이트

**Files:** 없음 (검증 전용)

- [ ] **Step 1: 풀 빌드 + `./jkdesktop test`** → `0 failures`, 산출물 mtime 확인.
- [ ] **Step 2: 프로브 15종 순차 실행** — 전부 PASS. (`probe_agent_ratelimit` ~70s 각주).
- [ ] **Step 3: 스폰 경로 스모크** — Task 5의 `smoke_jkwinserver.ps1` 재실행 + `jkdesktop --server` 경로도 한 번 (둘 다 서버 부팅 + 클라 스폰 성공).

### Task 7: 문서 산출물

**Files:**
- Create: `docs/42_engine_desktop_split.md`
- Modify: `docs/superpowers/specs/2026-09-13-engine-desktop-split-design.md` (§8 장부에 실행 직감 추가)

- [ ] **Step 1: docs/42 작성** — 실측 결과 중심: 단계별 커밋 해시, 프로브 결과, 예상 밖 커플링(발견 시), lib 경계 표(스펙 §2 재사용), "결정/직감" 섹션 (실행 중 판단 사항 기록 — 예: `jkserver PUBLIC jkdesktop_shell` 링크 방향이 스펙 §2 표기와 다른 점과 그 이유).
- [ ] **Step 2: 스펙 §8 장부에 실행 직감 추가** (D9~: 실행 중 생긴 결정).
- [ ] **Step 3: 커밋** `docs: record P1 split execution — docs/42 + spec ledger`

---

## Self-Review 결과

- **스펙 커버리지**: §3①→Task 1-2, §3②→Task 3, §3③→Task 4-5, §4 검증→각 태스크 게이트+Task 6, §5 리스크(파이프명→Task 1, 스모크 의존→Global Constraints+Task 5, font def/MineSweeper→P4 명시적 제외), §8 D4/D5/D6/D7은 제외/보존 규칙으로 각 태스크에 반영. 갭 없음.
- **타입 일관성**: `ShellHost` 필드 4종은 Task 4 정의 → Task 5 미사용(셸은 Task 4에서 와이어), `kWindowServerPipeName`은 Task 1 정의 → Task 5 소비. 일치.
- **플레이스홀더**: 없음 — 모든 코드 단계에 실제 코드 포함.