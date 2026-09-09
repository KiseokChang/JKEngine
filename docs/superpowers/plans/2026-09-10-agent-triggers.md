# M2b Trigger Scripts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 트리거 스크립트 플랫폼 — 별도 jktriggers.exe가 QuickJS로 `on(topic, filter, handler)`를 실행하고, 서버에 클라이언트→구독자 이벤트 발행 경로(`publish_event`)와 크래시 감지(`app.crashed`)를 추가하며, 첫 번들 3개(빌드 실패 알림 / 30분 idle 레이아웃 저장 / 앱 크래시 배지)가 채팅창에 알림을 표시한다.

**Architecture:** 이벤트 발행은 서버의 새 agent tool `publish_event` (모든 클라이언트가 호출 가능 — "one API many faces") → `PushAgentEventJson` 브로드캐스트. 트리거 호스트는 control-only 클라이언트(jkchat 패턴 복사) + 벤더드 quickjs-ng 영속 런타임. 트리거는 `apps/triggers/*.jkx` (manifest `trigger=` 선언 + SCRI 엔트리)로 패키징 — 서버 런처에는 안 보이는 별도 디렉터리. `desktop.notify()`는 `agent.notify` 이벤트를 발행하고 채팅창이 시스템 줄로 표시한다.

**Tech Stack:** Win32 named pipe (M1 JKAgentClient), quickjs-ng (벤더드, jkcore PUBLIC), JKJkxFile (컨테이너), ConPTY 터미널 앱.

**Spec:** `docs/superpowers/specs/2026-09-09-desktop-agent-platform-design.md` §6.2 (트리거 스크립트), §2 자동화 스크립트 예제, 완료 조건 "트리거 1개가 실빌드 실패를 잡아 알림 센터에 도달".

**User decisions (2026-09-10 AskUserQuestion):** 호스트 = 별도 jktriggers.exe; notify 수신처 = `agent.notify` 이벤트 + 채팅창 표시 (알림 센터는 §7 후속); 첫 번들 = 3개 전부.

## Global Constraints

- 채팅창/트리거 호스트는 JKWindow/SDL 체계 밖 (사용자 제약).
- 에이전트 응답 JSON 규약: `{"ok":true,...}` / `{"ok":false,"error":"<snake_case>"}`.
- 이벤트 봉투: `{"topic":"...","ts":<epoch ms>,...}` — 기존 필드 파괴 금지.
- push 이벤트는 재생되지 않음 (lesson 28) — 프로브는 구독이 트리거보다 먼저 연결돼야 함.
- PS5.1 프로브는 UTF-8 BOM 필수 (한글 리터럴).
- 커밋 단위 = 태스크, `Co-Authored-By: Claude Code <noreply@anthropic.com>`, 완료 후 push.

---

### Task 1: 서버 `publish_event` 도구

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` (HandleAgentQuery ~:1274)

**Interfaces:**
- Consumes: 기존 `PushAgentEventJson` (호출자가 clientsMutex_ 보유 — HandleAgentQuery가 보유).
- Produces: tool `publish_event`, args `{"topic":"<t>","data":<any JSON>}` → 방송 `{"topic":"<t>","data":<data>,"ts":<ms>}` + `{"ok":true}` 응답. 잘못된 topic → `{"ok":false,"error":"bad_request"}`.

- [ ] **Step 1: 도구 브랜치 추가** — `launch_chat` 브랜치 앞에:

```cpp
    } else if (tool == "publish_event") {
        // Client→subscriber event publishing (M2b trigger scripts). Any
        // client may publish; the server stamps the envelope and relays.
        std::string topic, data;
        req.GetStr("topic", topic);
        data = req.GetObjStr("data");  // raw JSON of the data object, or ""
        if (topic.empty() || topic.size() > 96 || data.empty()) {
            reply = "{\"ok\":false,\"error\":\"bad_request\"}";
        } else {
            char ev[1024];
            std::snprintf(ev, sizeof(ev),
                "{\"topic\":\"%s\",\"data\":%s,\"ts\":%lld}",
                JsonEsc(topic).c_str(), data.c_str(),
                static_cast<long long>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count()));
            PushAgentEventJson(ev);
            reply = "{\"ok\":true}";
        }
```

  (GetObjStr의 정확한 시맨틱 — 실패 시 "" 반환 — 확인 후 조정. data가 문자열이면 `"\"...\""` 형태여야 방송 JSON이 유효.)

- [ ] **Step 2: 빌드 + 기존 테스트** — `jkdesktop test` 0 failures 확인.
- [ ] **Step 3: 커밋** `feat(agent): publish_event tool — client→subscriber event relay`.

### Task 2: 서버 크래시 감지 (`app.crashed`)

**Files:**
- Modify: `engine/include/server/JKWindowServer.h`, `engine/src/server/JKWindowServer.cpp` (SpawnProcess ~:2007, CleanupDisconnectedClients ~:1700)

**Interfaces:**
- Consumes: SpawnProcess의 `pi.dwProcessId`/`pi.hProcess`; Hello 자기보고 pid (`client->Pid()`).
- Produces: app 윈도우 클라이언트가 비정상 종료(exit code ≠ 0)로 끊기면 `window.destroyed` 대신 `app.crashed` 이벤트 (동일 봉투 + `"crash":true`? — MVP: 토픽만 다름, 동일 4필드 봉투).

- [ ] **Step 1: 스폰 핸들 테이블** — 헤더에 `std::map<uint32_t, void*> spawnedClients_;` (pid→hProcess, void*로 windows.h 회피 — lesson 23). SpawnProcess에서 jkdesktop.exe 스폰 시 `CloseHandle(pi.hProcess)` 대신 `spawnedClients_[pi.dwProcessId] = pi.hProcess;` (hThread는 계속 닫음; jkchat 등 다른 exe도 담아도 무해).
- [ ] **Step 2: 정리 경로 분기** — CleanupDisconnectedClients의 app-window 브랜치:

```cpp
                if (!client->IsControlOnly() && !client->IsShell()) {
                    // Crash detection (M2b): a spawned client whose process
                    // exited non-zero did not leave gracefully.
                    bool crashed = false;
                    auto sh = spawnedClients_.find(client->Pid());
                    if (sh != spawnedClients_.end()) {
                        DWORD code = 0;
                        if (GetExitCodeProcess(
                                static_cast<HANDLE>(sh->second), &code) &&
                            code != STILL_ACTIVE && code != 0) {
                            crashed = true;
                        }
                        CloseHandle(static_cast<HANDLE>(sh->second));
                        spawnedClients_.erase(sh);
                    }
                    PushAgentEvent(crashed ? "app.crashed" : "window.destroyed",
                                   client->Id(), client->Title(),
                                   client->Pid());
                }
```

  필요한 win32 선언(extern "C" dllimport)이 파일 상단 manual-decl 블록에 있는지 확인 — GetExitCodeProcess/CloseHandle/STILL_ACTIVE 없으면 추가 (lesson 23).
- [ ] **Step 3: 수동 검증** — minesweeper 띄우고 `close_window`로 닫으면 `window.destroyed`, `taskkill //F`로 죽이면 `app.crashed` (probe에서 정식 검증 — Task 8).
- [ ] **Step 4: 빌드 + 테스트 + 커밋** `feat(server): app.crashed event — exit-code crash detection on spawned clients`.

### Task 3: 터미널 앱 `terminal.output` 발행

**Files:**
- Modify: `engine/src/apps/ClientTerminalApp.cpp` (PumpPty ~:101), `engine/include/apps/ClientTerminalApp.h`

**Interfaces:**
- Consumes: `JKClientApplication::Surface()` → `JKClientSurface::SendAgentQuery(id, json)` (팔레트 패턴, ClientPaletteApp.cpp:198) + `PollAgentReply` 프레임 루프 드레인.
- Produces: 이벤트 `{"topic":"terminal.output","text":"<이스케이프 제거된 출력 청크>", ...}` — 서버가 발행(terminal은 publish_event를 호출).

- [ ] **Step 1: 이스케이프 제거기** — 앱 로컬 헬퍼: ESC(0x1B) 시퀀스 제거(CSI `ESC [ ... @-~`, OSC `ESC ] ... BEL|ESC\`, 기타 `ESC <1byte>`), 제어문자 제거(\r 유지). ~30줄.
- [ ] **Step 2: PumpPty 발행** — `parser_->Feed(out)` 후: 정제 텍스트가 비어있지 않으면 200ms coalesce 버퍼에 누적; 버퍼가 250ms 이상 묵었거나 4KiB 초과 시 `SendAgentQuery`로 `{"tool":"publish_event","args":{"topic":"terminal.output","data":{"text":"<JsonEsc>"}}}` 발사. 쿼리ID는 무시(발사 후 즉시 링에서 poll해 버림 — 팔레트 PollAgentReply 패턴).
- [ ] **Step 3: 빌드 + jkx 리팩** — 터미널 앱이므로 `terminal.jkx` 재포장 필요 (auto-repack 타깃이 처리). `jkdesktop test` 0 failures.
- [ ] **Step 4: 커밋** `feat(terminal): publish terminal.output agent events (M2b trigger feed)`.

### Task 4: jktriggers.exe — 호스트 골격 + QuickJS

**Files:**
- Create: `engine/tools/jktriggers/main.cpp`
- Modify: `engine/CMakeLists.txt` (jkchat 블록 뒤)

**Interfaces:**
- Consumes: `JKAgentClient` (jkchat 패턴: Connect → SubscribeEvents(true) → 400ms 루프 — 콘솔이므로 Sleep 루프), quickjs (jkcore PUBLIC).
- Produces: 스크립트 API `on(topic, filter, handler)`, `desktop.notify(title, body)`, `desktop.publish(topic, dataObj)`, `desktop.saveLayout(name)`, `desktop.log(s)`, `setTimeout/setInterval/clearTimeout/clearInterval`. 이벤트 객체: `{topic, id, title, pid, ts, text?, data?}`.

- [ ] **Step 1: 골격** — 콘솔 앱 (`add_executable(jktriggers tools/jktriggers/main.cpp)`, `target_link_libraries(jktriggers PRIVATE jkcore)` + `-static-libstdc++ -static-libgcc`). 루프: 재접속→구독, ping 펌프, PollReply 드레인, PollEvents → `DispatchEvent`.
- [ ] **Step 2: QuickJS 호스트** — 영속 `JSRuntime/JSContext`. 바인딩:
  - `on(topic, filter, handler)`: 전역 `__triggers` 배열 push `{topic, matchRegExp(JS RegExp 객체, filter.match), handler}`.
  - 이벤트 봉투 JSON → `JS_ParseJSON` → topic 매칭(정확 일치 또는 `prefix.*` 글롭) → RegExp test(event json 텍스트) → `JS_Call` handler(eventObj) → `JS_ExecutePendingJob` 루프로 프로미스 정산. 예외는 stderr 로그 + 런타임 생존.
  - `desktop.notify(title, body)` / `desktop.publish(topic, dataObj)`: C 함수 → `SendQuery("publish_event", ...)` (dataObj는 `JS_JSON.stringify`).
  - `desktop.saveLayout(name)`: `SendQuery("save_layout", ...)` — 응답은 PollReply에서 로그.
  - 타이머: 호스트 측 틱 루프 (100ms Sleep, `GetTickCount64` 만료 검사 → JS_Call 콜백) — JKScriptHost의 host-timer 관례 동일.
- [ ] **Step 3: 커밋** `feat(triggers): jktriggers.exe — QuickJS on(topic,filter,handler) host`.

### Task 5: 트리거 로딩 (.jkx) + 패커

**Files:**
- Modify: `engine/tools/jktriggers/main.cpp`
- Modify: `engine/CMakeLists.txt` (패킹 custom command)

**Interfaces:**
- Consumes: `JKJkxFile::Open/FindEntry/ReadEntry` (엔트리 타입 무관 이름 조회), `JKJkxFile::Write`.
- Produces: `jktriggers --pack <srcDir> <outDir>` — `<srcDir>/<name>/manifest.txt` + 스크립트 → `<outDir>/<name>.jkx`. 로더: `<exeDir>\apps\triggers\*.jkx` 스캔 → 매니페스트 `trigger=` (쉼표 목록) → 각 SCRI 엔트리 읽어 eval. 매니페스트 파서는 jktriggers 로컬 (name/trigger만 — 서버 JkxManifest는 건드리지 않음, 런처 오염 방지).

- [x] **Step 1: 로더** — `LoadTriggerContainers()`: FindFirstFile `apps/triggers/*.jkx` → Open → manifest.txt 엔트리 찾아 로컬 파싱 → `trigger=` 목록의 각 이름 FindEntry+ReadEntry → `JS_Eval`(file명으로). 로드 실패는 로그하고 계속.
- [x] **Step 2: 패커** — `--pack`: 디렉터 순회, `manifest.txt` + `*.js`를 JKJkxFile::Write로 패킹 (TypeForName이 .js→SCRI 자동).
- [x] **Step 3: CMake** — POST_BUILD식: `$<TARGET_FILE:jktriggers> --pack ${CMAKE_SOURCE_DIR}/tools/triggers "${CMAKE_BINARY_DIR}/apps/triggers"`. 소스 트리 `engine/tools/triggers/<name>/{manifest.txt,*.js}`.
- [x] **Step 4: 커밋** `feat(triggers): .jkx trigger containers + packer`.

### Task 6: 첫 번들 3개

**Files:**
- Create: `engine/tools/triggers/trig_build/manifest.txt`, `trig_build/trig_build.js`
- Create: `engine/tools/triggers/trig_idle/manifest.txt`, `trig_idle/trig_idle.js`
- Create: `engine/tools/triggers/trig_crash/manifest.txt`, `trig_crash/trig_crash.js`

**Interfaces:**
- Consumes: Task 4 API, Task 3 terminal.output, Task 2 app.crashed.
- Produces: 알림 3종 → 채팅창 `[알림]` 줄.

- [x] **Step 1: trig_build.js** — 빌드 실패 감지 (스펙 예제 충실):

```js
var lastNotify = 0;
on("terminal.output", { match: /error C\d+|fatal error|error:/i }, function (e) {
  var now = Date.now();
  if (now - lastNotify < 60000) return;   // debounce 60s
  lastNotify = now;
  var snippet = (e.text || "").slice(0, 200);
  desktop.notify("빌드 실패 감지", snippet);
  desktop.log("trigger: build fail -> notify");
});
```

- [x] **Step 2: trig_idle.js** — 30분 idle 레이아웃 저장. 활동 = `window.focused|window.created|window.destroyed` 수신 시각. `state/idle_minutes` 파일로 임계값 오버라이드 (기본 30 — 프로브가 0으로 세팅해 즉시 발화 검증 가능).

```js
var thresholdMin = 30;
try { var f = desktop.readFile("state/idle_minutes"); parseInt(desktop.readFile(...)) !== NaN 체크 — 0도 유효 임계값
var lastActivity = Date.now(), saved = false;
on("window.focused", {}, function () { lastActivity = Date.now(); saved = false; });
on("window.created", {}, function () { lastActivity = Date.now(); saved = false; });
on("window.destroyed", {}, function () { lastActivity = Date.now(); saved = false; });
setInterval(function () {
  var idleMs = Date.now() - lastActivity;
  if (!saved && idleMs >= thresholdMin * 60000) {
    saved = true;
    desktop.saveLayout("auto_idle");
    desktop.notify("자동 저장", "idle " + thresholdMin + "분 — 레이아웃 auto_idle 저장");
  }
}, 10000);
```

  `desktop.readFile(relPath)` — jktriggers 호스트 API에 추가 (exeDir 기준 상대경로, 크기 상한 64KiB).
- [x] **Step 3: trig_crash.js** — 크래시 배지:

```js
on("app.crashed", {}, function (e) {
  desktop.notify("앱 비정상 종료", (e.title || "?") + " (pid " + e.pid + ")");
});
```

- [x] **Step 4: 각 manifest.txt** — `name=trig_build` / `title=Build-failure trigger` / `trigger=trig_build.js` (icon/module 없음).
- [x] **Step 5: 커밋** `feat(triggers): first bundle — build fail, idle save, crash badge`.

### Task 7: 채팅창 `agent.notify` 표시

**Files:**
- Modify: `engine/tools/jkchat/main.cpp` (HandleEvent ~:396)

- [x] **Step 1:** HandleEvent에 토픽 브랜치:

```cpp
    } else if (ev.topic == "agent.notify") {
        jk::agent::AgentJson j(ev.json);
        std::string title, body;
        j.GetDeepStr("data.title", title);
        j.GetDeepStr("data.body", body);
        Log(std::string("[알림] ") + (title.empty() ? "(무제)" : title) +
            (body.empty() ? "" : " — " + body));
    }
```

  (봉투 필드 배치는 실제 방송 JSON에 맞춰 조정 — `data.title` 경로 확인.)
- [x] **Step 2: 빌드 + 커밋** `feat(chat): display agent.notify as system lines`.

### Task 8: 프로브 + 문서

**Files:**
- Create: `engine/tools/probes/probe_agent_triggers.ps1`
- Create: `docs/32_desktop_agent_triggers.md`
- Modify: 스펙 상태줄, docs/31 §8, memory

- [x] **Step 1: 프로브** — 서버+jktriggers+jkchat 구동, 채팅 구독 먼저(lesson 28):
  1. `agentctl publish_event terminal.output {"text":"... error C2084 ..."}` → 채팅 트랜스크립트에 "빌드 실패 감지" (WmGetText로 판정)
  2. minesweeper 띄우고 `taskkill //F` (비정상) → "앱 비정상 종료" 표시
  3. minesweeper 다시 띄우고 `close_window`(allow)로 닫기 → "비정상" 추가 줄 없음 + `window.destroyed` 정상 경로 회귀 확인
  4. `state/idle_minutes=0` 쓰고 jktriggers 재시작 → 10초 내 "자동 저장" 표시 + `save_layout` 응답 ok
  5. 정리: idle_minutes 삭제, 프로세스 종료
- [x] **Step 2: 전체 회귀** — jkdesktop test 0, jkagentd --selftest 0, mcp 5/5, e2e 7/7, palette 4/4, chat 7/7, chat_llm 2/2.
- [x] **Step 3: docs/32** — 아키텍처(발행/구독/트리거 호스트), 스크립트 API, .jkx 선언 형식, 번들 3개 설명, 프로브, 제한(rate limit/dead-letter·알림 센터·async 이벤트 루프·스트리밍은 후속), 다음 단계.
- [x] **Step 4: 스펙 상태줄 + docs/31 §8 + memory 갱신, 커밋+push.**

## 완료 조건

1. [x] 터미널에서 실제 빌드 실패(또는 동합성 패턴) → trig_build가 notify → 채팅창 `[알림] 빌드 실패 감지` 도달.
2. [x] taskkill로 죽인 앱 → `[알림] 앱 비정상 종료`, 정상 닫기는 `window.destroyed` 그대로.
3. [x] idle 0분 오버라이드 → `auto_idle` 저장 + 알림.
4. [x] 기존 회귀 전부 녹색.