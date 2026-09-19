# filedlg 음성 내비게이션 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 파일 열기 대화상자(filedlg)를 에이전트/폰이 말로 조작 — navigate/list/choose 3도구를 앱 도구 허브로 등록하고, 모달 다이얼로그의 쿼리-수명(슬롯 소유)을 프로토콜에 구조적으로 반영한다.

**Architecture:** filedlg가 허브(AgentToolRegister)로 자기 도구 등록(등록 시점 = params 수락 직후 → 도구 가시성 == 슬롯 소유 불변식). 서버는 와이어에 `modal` 플래그(선택 필드)를 추가하고 app_tool 중계 경로에서 슬롯 소유자(PendingFileDialog.dialogConnId) 재검증으로 고아 다이얼로그를 tool_gone 봉쇄. 폰 생존성을 위해 file_open에 `wait:"event"` 모드를 추가해 브로커의 600s 블록을 해소하고 해소 결과를 `file.open_result` 이벤트로 방송한다.

**Tech Stack:** C++17 (MinGW/ucrt64), 수기 JSON (AgentJson), PS5.1 프로브, ImGui (filedlg UI)

**Spec:** `docs/superpowers/specs/2026-09-19-filedlg-voice-nav-design.md`

## Global Constraints

- 외부 의존 0 — JSON 직렬화는 기존 수기 경로(JsonEsc/EscapeJson) 재사용, 새 라이브러리 금지.
- 새 락 0 — 서버 신규 코드는 HandleAgentQuery/만료 스캔의 clientsMutex_ 보유 경로 안에서만 상태 접근(레슨 35).
- 클라 도구 핸들러는 프레임 스레드에서 실행 — 블로킹 I/O 금지, ImGui 컨텍스트 밖 ImGui 호출 금지(OnAgentToolCall은 Run 스윕에서 호출, BuildUi/NewFrame 밖이다 — ImGui 상태 조회 필요값은 BuildUi가 캐시).
- 앱 도구 응답 계약: 성공/앱 실패 모두 `{"ok":true,...}` — 앱 실패는 `error` 멤버(docs/58 레슨 f).
- 프로브: PS5.1 ASCII 전용, `> log 2>&1` 파일 리다이렉트, PID-유니크 state 백업/복원, 시작 시 stale 잔여 fail-fast, ×2 연속 ALL PASS, 종료 시 permissions.json 바이트 동일 복원(7키). 한글 문자 매치 금지 — JSON 키 기반 판정(docs/58 레슨 d).
- 빌드: cwd=`engine`, `cmake --build build`; ninja ld exit-67은 재실행 회복.
- 서버 다중 기동 금지 — 프로브 시작 시 기존 jkdesktop 서버 존재 확인 후 정리(2026-09-19 다중 기동 사고 레슨).
- 커밋 메시지: 한국어 관례 유지 + `Co-Authored-By: Claude Code <noreply@anthropic.com>`.

---

### Task 1: 서버 — modal 플래그 + 슬롯 소유자 재검증

**Files:**
- Modify: `engine/include/server/JKWindowServer.h` (AppToolManifest ~:287, PendingFileDialog ~:270)
- Modify: `engine/src/server/JKWindowServer.cpp` (HandleToolRegister ~:4891, file_dialog_params ~:4750, app_tool 중계 ~:2711, list_app_tools ~:2637)

**Interfaces:**
- Consumes: docs/58 §4.1 레지스트리(AppToolManifest/appToolManifest_), docs/48 슬롯(PendingFileDialog)
- Produces: `AppToolManifest.modal`(bool) — Task 2 카탈로그·Task 5 프로브가 소비; `PendingFileDialog.dialogConnId`(uint32) — 모달 가드의 진실원

- [ ] **Step 1: 헤더 필드 추가**

`JKWindowServer.h` AppToolManifest에 `bool modal = false;  // 쿼리-수명 모달(filedlg) — app_tool 중계 시 슬롯 소유 재검증 (스펙 §4)` 추가. PendingFileDialog에 `uint32_t dialogConnId = 0;  // file_dialog_params를 수락한 다이얼로그 연결 — 모달 가드 진실원 (스펙 §5)` 추가(주석에 requesterConnId와의 역할 구분 명기: requester=요청자, dialog=다이얼로그).

- [ ] **Step 2: HandleToolRegister 파싱**

`HandleToolRegister`(`JKWindowServer.cpp:4891`)의 매니페스트 조립에 `req.GetBool("modal", ...)` 계열로 modal 선택 필드 파싱(부재=false). AgentJson에 bool 리더가 있으면 사용, 없으면 int 0/1로 수용하고 주석으로 계약 명시(기존 파서 계약 — bool은 0/1 int, jkagentd settings_set 선례).

- [ ] **Step 3: file_dialog_params에서 dialogConnId 기록**

`file_dialog_params` 성공 경로(`:4758-4769`)의 `paramsTaken = true` 직전에 `pendingFileDialog_.dialogConnId = client.Id();` 추가.

- [ ] **Step 4: app_tool 중계 모달 가드**

`app_tool` 중계(`:2712` `const AppToolManifest* m = cands.front();` 직후, conn 조회 전):

```cpp
if (m->modal && pendingFileDialog_.dialogConnId != m->connId) {
    // 모달 가드 (스펙 §5): 도구는 "다이얼로그가 슬롯을 소유한 동안만"
    // 존재한다. 슬롯 만료/해소/재사용 후 살아 있는 고아 다이얼로그의
    // 도구 호출을 중계 전에 차단한다 (docs/48 MAJOR-1 교차결함 동류 —
    // 판정은 앱이 아니라 슬롯 진실원이 한다).
    reply = "{\"ok\":false,\"error\":\"tool_gone\"}";
} else { /* 기존 conn 조회 + 게이트 스위치 */ }
```

- [ ] **Step 5: list_app_tools modal 표기**

`list_app_tools` 행(`:2649-2655`)에 `,"modal":` + (m.modal ? "true" : "false") 추가 — false도 명시(스펙 §4, 소비자 파싱 단순화).

- [ ] **Step 6: 빌드 + 자가 검증**

`cmake --build build` 성공. 서버 기동 후 기존 vplayer가 등록한 매니페스트의 list_app_tools 행에 `"modal":false` 확인.

- [ ] **Step 7: 커밋**

```bash
git add -A && git commit -m "feat(server): 모달 플래그+슬롯 소유자 재검증 — app_tool 고아 가드"
```

---

### Task 2: 서버 — file_open wait 모드 + file.open_result 이벤트

**Files:**
- Modify: `engine/include/server/JKWindowServer.h` (PendingFileDialog)
- Modify: `engine/src/server/JKWindowServer.cpp` (file_open ~:4680, file_open_result ~:4770, 만료 스캔 ~:1595, kReservedTopicPrefixes ~:2549, events_list ~:4047)

**Interfaces:**
- Consumes: PushAgentEventJson(서버 이벤트 방송), kReservedTopicPrefixes(파일스코프)
- Produces: `file.open_result` 이벤트 `{topic, ok, path?|error?}` — Task 5 프로브가 read_events로 수취; `PendingFileDialog.waitAsync`

- [ ] **Step 1: wait 인자 + 슬롯 필드**

`PendingFileDialog`에 `bool waitAsync = false;` 추가. `file_open` 핸들러(`:4687`)에서 `std::string wait; req.GetObjStr("args", "wait", wait);` — 기본 "reply"; "event"가 아니고 비어있지 않으면 `bad_args` 즉답. 슬롯 채움 경로(`:4741-4746`)에 `pendingFileDialog_.waitAsync = (wait == "event");` 추가.

- [ ] **Step 2: waitAsync 즉시 회답**

`replied = false;`(`:4746`)를 분기:

```cpp
if (pendingFileDialog_.waitAsync) {
    replied = true;
    reply = "{\"ok\":true,\"parked\":true}";
} else {
    replied = false;  // file_open_result(또는 만료)가 응답한다
}
```

- [ ] **Step 3: 해소 경로 이벤트 발행**

`file_open_result`(`:4791-4819`): resolved 블록 안에서 waitAsync면 요청자 WriteAgentJson 대신 `PushAgentEventJson`으로 방송:

```cpp
std::string ev = ok
    ? (path.empty() ? "{\"topic\":\"file.open_result\",\"ok\":true}"
                    : "{\"topic\":\"file.open_result\",\"ok\":true,\"path\":\"" +
                          JsonEsc(path) + "\"}")
    : "{\"topic\":\"file.open_result\",\"ok\":false}";
```

waitAsync=false면 기존 AgentReply 그대로(계약 불변). 슬롯 소진/상관 검증은 양쪽 공유.

- [ ] **Step 4: 만료 스캔 이벤트**

만료 스캔(`:1595-1598`)의 file_open 회수 시 waitAsync면(스캔 시점에 `pendingFileDialog_.requestId == it->requestId`로 슬롯 대조 가능 — 소진되어 슬롯이 비었으면 waitAsync 여부 판독 불가이므로 **슬롯 대조 성공 시에만** 발행) `{"topic":"file.open_result","ok":false,"error":"expired"}` 방송 추가. reply 모드 현행 불변(dialog_timeout AgentReply 유지).

- [ ] **Step 5: 예약 접두 + 카탈로그**

`kReservedTopicPrefixes`(`:2549`)에 `"file."` 추가(주석: file.open_result 스푸핑 봉쇄 — docs/54 NIT-3 선례). events_list 카탈로그(`:4047` 부근)에 `{"file.open_result", "server", ...}` 행 추가 — 기존 행 형식 그대로.

- [ ] **Step 6: 빌드 + agentctl 수동 검증**

빌드 후 서버 기동, `agentctl '{"tool":"file_open","args":{"wait":"event"}}'` → 즉시 `{"ok":true,"parked":true}` + 다이얼로그 스폰 확인 → `publish_event`로 `"file.open_result"` 시도 → reserved_topic 거부 확인. 검증 후 다이얼로그 취소.

- [ ] **Step 7: 커밋**

```bash
git add -A && git commit -m "feat(server): file_open wait:event 모드 + file.open_result 이벤트 + file. 예약 접두"
```

---

### Task 3: filedlg — 도구 3종 등록 + 핸들러

**Files:**
- Modify: `engine/include/apps/ClientFileDialogApp.h` (훅 오버라이드 + 멤버)
- Modify: `engine/src/apps/ClientFileDialogApp.cpp` (PumpReplies 등록, 핸들러 ~150줄, BuildUi 스크롤)

**Interfaces:**
- Consumes: docs/58 §5.1 `JKClientSurface::AgentToolDecl`/`SendAgentToolRegister`/`OnAgentToolCall` 훅(vplayer 선례 — `ClientVPlayerApp.cpp:1665-1687` 등록, `:2142` 핸들러), 코어 결과 전송 보장(`JKClientApplication.cpp:286→:287`, `!running_` 체크 `:292` 이전)
- Produces: filedlg 도구 3종(navigate/list/choose) — 서버 스펙 §3 계약 그대로

- [ ] **Step 1: 헤더**

`OnAgentToolCall` 오버라이드(vplayer 시그니처 동일) + 멤버: `bool toolsRegistered_ = false;`(params 수락 후 1회 등록), `bool scrollToSelection_ = false;`, `int visibleRows_ = 10;`(pgup/pgdn 페이지 크기 캐시).

- [ ] **Step 2: 등록 — params 수락 직후**

`PumpReplies` params 성공 경로 말미(`:537` title 적용 후):

```cpp
if (!toolsRegistered_) {
    toolsRegistered_ = true;
    if (jk::client::JKClientSurface* s = Surface()) {
        using Decl = jk::client::JKClientSurface::AgentToolDecl;
        const std::vector<Decl> tools = {
            {"navigate", "Move the file dialog selection (key: up/down/pgup/pgdn/parent/select) or jump to index; select acts like Enter (folder descends, file resolves)",
             "{\"type\":\"object\",\"properties\":{\"key\":{\"type\":\"string\",\"enum\":[\"up\",\"down\",\"pgup\",\"pgdn\",\"parent\",\"select\"]},\"index\":{\"type\":\"integer\"}}}"},
            {"list", "Page the dialog's current directory listing (offset 0-based, limit <= 200, default 50)",
             "{\"type\":\"object\",\"properties\":{\"offset\":{\"type\":\"integer\"},\"limit\":{\"type\":\"integer\"}}}"},
            {"choose", "Resolve the dialog: optional entry name (folder descends, file resolves); omitted = current selection/file box",
             "{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"}}}"},
        };
        s->SendAgentToolRegister("filedlg", tools);
    }
}
```

- [ ] **Step 3: 핸들러 — navigate/list/choose**

`OnAgentToolCall` 오버라이드(파일 말미, EscapeJson 재사용). 계약: 앱 실패도 `{"ok":true,"error":...}`; ImGui 호출 금지(코어 Run 스윕은 NewFrame 밖 — 페이지 크기는 `visibleRows_` 캐시 사용).

- navigate: `index` 있으면(0≤index<count) selectedIdx_=index + scrollToSelection_=true; 없으면 key별 처리 — up/down: selectedIdx_±1 클램프(빈 리스트면 error empty_list); pgup/pgdn: ±max(visibleRows_,1) 클램프; parent: NavigateUp(); select: OnOk()와 동일 — **OnOk가 Finish(true,path)로 이어질 수 있으므로 해소 응답 먼저 out에 세팅 후 Finish**. select가 하강이면 descended:true. key/index 모두 없으면 bad_args, 둘 다 있으면 index 승리(스펙 §3.1).
- list: offset/limit 클램프(limit≤200, 기본 50; offset 0 미만/범위 밖은 빈 배열) — entries_ 페이지만 직렬화.
- choose: name 있으면 entries_에서 탐색(대소문자 구분 없음 IEquals 재사용) — 미발견 no_such_entry; 발견 시 fileBuf 세팅+selectedIdx_ 갱신 후 폴더=하강(descended) / 파일=해소(resolved). name 없으면 OnOk() 동등(빈 대상 nothing_selected).
- 성공 응답 공통 필드: `dir`(currentDir_), `count`(entries_.size()), `selected`(인덱스, 없으면 -1), `file`(fileBuf_). navigate은 추가로 `selectedName`/`selectedIsDir`(선택 엔트리 스냅샷 — 스펙 §3.1). 해소 응답 `{"ok":true,"resolved":true,"path":...}` — path는 EscapeJson.
- 미등록 도구명 도달 불가 방어선(unknown_tool) 유지 — vplayer 선례.

- [ ] **Step 4: BuildUi 스크롤 인뷰**

`BuildUi` 클리퍼 루프(`:264-280`): `visibleRows_ = max(1, DisplayEnd-DisplayStart)` 갱신(클리퍼가 보는 실측 행수). `if (i == selectedIdx_ && scrollToSelection_) { ImGui::SetScrollHereY(0.5f); scrollToSelection_ = false; }` — Selectable 직후.

- [ ] **Step 5: 빌드 + 수동 e2e**

빌드 → 서버 기동 → `agentctl '{"tool":"file_open","args":{"wait":"event"}}'` → 다이얼로그 기동 → `agentctl '{"tool":"app_tool","args":{"app":"filedlg","tool":"list","args":{}}}'` → entries 응답 → navigate index/parent → choose(파일) → `read_events`에 file.open_result + 다이얼로그 종료 확인.

- [ ] **Step 6: 커밋**

```bash
git add -A && git commit -m "feat(filedlg): 음성 내비게이션 도구 3종 등록+핸들러 (navigate/list/choose)"
```

---

### Task 4: 브로커 — file_open 재조립 + wait:event 주입

**Files:**
- Modify: `engine/tools/jkagentd/main.cpp` (args-rebuild 체인 ~:724-754, kCoreToolsListJson :~68 file_open 행)

**Interfaces:**
- Consumes: 서버 file_open args 계약(스펙 §6 — filter/start/title/wait)
- Produces: MCP file_open이 parked 즉답을 받는 계약(스펙 §7)

- [ ] **Step 1: file_open 재조립 분기**

args-rebuild 체인(`:754` app_tool 분기 앞)에 추가 — **부수 픽스: 현재 file_open은 분기 부재로 기본 `"{}"` 폴백을 타서 MCP 경로의 filter/start/title이 유실된다(잠복 결함, 본 분기로 치유)**:

```cpp
} else if (tool == "file_open") {
    // filedlg 음성 내비게이션 (스펙 §7): MCP 경로는 wait:event 주입 —
    // 파킹 응답이 해소 때까지 안 오면 브로커가 600s 블록해 음성 흐름이
    // 죽는다(스펙 §0 결정 5). 해소는 file.open_result 이벤트로 온다.
    std::string body = "{\"wait\":\"event\"";
    std::string f, s2, t;
    if (req.GetDeepStr("params", "arguments", "filter", f))
        body += ",\"filter\":\"" + JsonEsc(f) + "\"";
    if (req.GetDeepStr("params", "arguments", "start", s2))
        body += ",\"start\":\"" + JsonEsc(s2) + "\"";
    if (req.GetDeepStr("params", "arguments", "title", t))
        body += ",\"title\":\"" + JsonEsc(t) + "\"";
    body += "}";
    argsJson = body;
}
```

- [ ] **Step 2: description 갱신**

kCoreToolsListJson의 file_open 행(존재 확인 후) description에 "; returns parked immediately — the resolution arrives as the file.open_result desktop event (poll read_events)" 추가. (file_open이 정적 목록에 없으면 — 선례 확인 — IsKnownTool/LoadPermissions kNames에 존재하는지와 함께 확인하고, 정적부에 행 추가.)

- [ ] **Step 3: 빌드 + selftest**

`cmake --build build` + `jkagentd` selftest(기존 회귀 명령 준수) 통과.

- [ ] **Step 4: 커밋**

```bash
git add -A && git commit -m "feat(broker): file_open 재조립+wait:event 주입 — MCP 경로 파라미터 유실 부수 픽스"
```

---

### Task 5: 프로브 probe_filedlg_voice.ps1 + 회귀

**Files:**
- Create: `engine/tools/probes/probe_filedlg_voice.ps1`
- Modify: 없음(회귀는 기존 프로브 실행)

**Interfaces:**
- Consumes: probe 관례(probe_app_tools.ps1 구조 — 서버 스폰/정리, state 백업, ×2 판정), Task 1-4의 표면

- [ ] **Step 1: 프로브 작성**

스펙 §9의 12체크 구현. 골격: probe_app_tools.ps1 복제 후 수정 — 서버 스폰(jkdesktop --server, 로그 파일 리다이렉트), permissions.json PID-유니크 백업/복원, agentctl face(`& $jkdesktop agentctl '<json>'` — 값-공백 인용 함정 회피: 값에 공백 없는 인자만), file_open→다이얼로그 대기(list_app_tools에 filedlg 행 폴링), app_tool 3종 호출, wait:event e2e, 가드 2종(프로세스 kill→도구 소멸, 슬롯 없는 수동 filedlg→미등록), reserved_topic 거부. 한글 판정 금지 — JSON 키/불리언 기반. 종료 시 다이얼로그/서버 kill + state 복원 + 잔여 fail-fast.

- [ ] **Step 2: ×2 실행**

`powershell -File engine/tools/probes/probe_filedlg_voice.ps1 > log 2>&1` ×2 연속 ALL PASS.

- [ ] **Step 3: 회귀 스윕**

probe_app_tools.ps1(61체크) ×2, probe_files.ps1, probe_agent_chat.ps1, jkdesktop test, jkagentd selftest — 전부 exit 0. 서버 다중 기동 없음 확인(시작 전 tasklist 점검).

- [ ] **Step 4: 커밋**

```bash
git add -A && git commit -m "test(probe): probe_filedlg_voice — 음성 내비게이션 e2e 12체크"
```

---

### Task 6: as-built 문서 + 원장 갱신

**Files:**
- Create: `docs/59_filedlg_voice_nav.md`
- Modify: `docs/58_app_tool_hub.md` §11(후속 항목 완결 표기), 스펙 헤더(as-built 링크)

- [ ] **Step 1: docs/59 작성** — 스펙 §0-8 as-built(커밋별), 오류 표 실측 정합, 프로브 결과, 레슨(실패한 것 포함).
- [ ] **Step 2: docs/58 §11 갱신** — filedlg 음성 내비게이션 항목을 완결 링크로.
- [ ] **Step 3: 커밋**

```bash
git add -A && git commit -m "docs(59): filedlg 음성 내비게이션 as-built"
```