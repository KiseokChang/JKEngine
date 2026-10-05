# 워크숍 능력 게이트 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 워크숍 스크립트 앱의 호스트 API 호출을 MANI `capabilities=` 선언으로 게이트한다 (fail-closed + 캡션 배지).

**Architecture:** MANI에 `capabilities=` 컴마 목록 신설(원문 보존) → `jk_app_meta`의 모듈 static으로 옮겨 워크숍 앱만 `JKScriptHost::EnableCapabilities` 주입 → 호스트의 단일 조우(각 thunk 선두 `GateCap`)에서 미선언 호출을 TypeError로 차단 → 슬롯 스트립에 배지 라벨. 서버·승인 파이프라인 불변.

**Tech Stack:** C++17 (MinGW ninja), QuickJS-ng, JKWINDOW 위젯 (JKStatic), 셀프테스트 = `jkdesktop.exe test` (RunAppSelfTest).

**Spec:** `docs/superpowers/specs/2026-10-05-workshop-capability-gate-design.md` (docs/74 결정 이행 — 토큰 표 §2, MANI §3, 게이트 §4, 배지 §5, 셀프테스트 §6, 마이그레이션 §8)

## Global Constraints

- 게이트 에러 문구 **고정 계약**: `capability '<tok>' not declared in MANI` — 셀프테스트가 단언하므로 문구를 고치면 테스트와 d.ts 주석도 함께.
- 무조건 허용(게이트 밖): `log`, `assert`, `assertEq` — 이 셋에 GateCap를 넣지 않는다 (스펙 §2).
- 게이트는 **워크숍 앱만 활성** — `EnableCapabilities`가 불렸을 때만 (`ClientScriptApp` 경로는 미호출 → 비활성, 기존 셀프테스트 무영향).
- 미지 토큰: 파스는 **원문 그대로 보존** (`std::string capabilities`, 분해 없음), 토큰 정규화(trim+소문자)는 `EnableCapabilities` 몫, 게이트는 바인딩이 있는 토큰만 의미.
- 신규 바인딩 없음 → `BoundNames()`↔jk.d.ts 대조 테스트(:2704-2742) 녹색 유지. d.ts는 주석만 갱신.
- `JkxManifestMerge` 보존 테스트(main.cpp:1644-1696)와 기존 script 블록 셀프테스트 0-10 전부 녹색 유지 (회귀 금지).
- 코드 주석은 한국어, 기존 문체 따름. 커밋 메시지 끝에 `Co-Authored-By: Claude Code <noreply@anthropic.com>`.
- 빌드/테스트 표준 (MinGW PATH 레슨):
  ```
  export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH" && ninja.exe -C build -j3
  RC=$?; echo "BUILD RC=$RC"
  ./jkdesktop.exe test    # 마지막 줄 "0 failure(s)" 확인
  ```
  `| tail -1 && echo OK` 금지 — RC 마스킹 함정. 빌드는 서버 프로세스(스택) 중단 후(DLL 락) — 마지막 태스크 라이브 검증 전에만 스택 죽이고, 끝나면 스택 복원(`Start-Process jkdesktop.exe` 서버+태스크바, jkbridge, ping GREEN).
- **사내 repo — 절대 외부 공개 금지** (내부 IP 문서 포함). 푸시는 GitHub 원격(KiseokChang/JKEngine)만, 사용자 사전 승인됨.

---

### Task 1: JKScriptHost 능력 게이트 (상태 + GateCap + 전 바인딩 적용)

**Files:**
- Modify: `engine/include/script/JKScriptHost.h` (공용 API + private 상태)
- Modify: `engine/src/script/JKScriptHost.cpp` (구현 + GateCap 헬퍼 + 23 곳 thunk 삽입 + includes)
- Modify: `engine/src/main.cpp` (셀프테스트 2c·2d 신설 — script 블록 내)
- Modify: `engine/scripts/jk.d.ts` (주석만 — 게이트 계약 문서화)

**Interfaces:**
- Consumes: 기존 thunk 선두 패턴 `JKScriptHost* host = HostOf(ctx)` + 선례 `PatchBlocked`(JKScriptHost.cpp:89-93) + 셀프테스트 헬퍼 `writeScript`/`makeTimerServices`(main.cpp:2577-2595).
- Produces: `void JKScriptHost::EnableCapabilities(std::string)`, `bool JKScriptHost::GateActive() const`, `bool JKScriptHost::HasCapability(const std::string&) const` — Task 3이 소비. 파일 내부 헬퍼 `bool GateCap(JKScriptHost*, JSContext*, const char*)`(anonymous namespace) — 이후 태스크 소비자 없음.

- [ ] **Step 1: 실패하는 셀프테스트를 먼저 쓴다** — main.cpp script 블록 안, 기존 1) boot 테스트(:2597-2623) **직전**에 신설:

```cpp
        // 1g) 워크숍 능력 게이트 (docs/74 결정, 스펙 §4/§6): fail-closed —
        //     미선언 호출은 Start 실패 + 고정 문구. 선언되면 통과. log/assert
        //     는 무조건 허용. 각 시나리오는 새 창을 소유한다(블록 원칙).
        {
            // (a) 미선언 차단 + 문구 단언
            writeScript("test_script_gate.js",
                "var tick = setInterval(function(){}, 16);\n");
            jk::JKWindow gwin("ScriptGateTest");
            gwin.SetWindowRect(jk::JKRect{ 0, 0, 320, 240 });
            jk::JKScriptHost ghost;
            ghost.Attach(&gwin);
            std::vector<uint32_t> gateWinIds;
            uint64_t gateSeq = 0;
            ghost.SetTimerServices(makeTimerServices(gateWinIds, gateSeq));
            ghost.EnableCapabilities("");  // 선언 없음 = 능력 없음
            check(!ghost.Start("test_script_gate.js"),
                  "capability gate blocks undeclared setInterval");
            check(ghost.LastError().find(
                      "capability 'timer' not declared in MANI") !=
                      std::string::npos,
                  "capability gate error names the token");
            check(ghost.GateActive(),
                  "gate active after EnableCapabilities");
            ghost.Stop();

            // (b) 정규화(대문자·공백) + 선언 통과 + 미지 토큰 보존
            jk::JKWindow gwin2("ScriptGateNorm");
            gwin2.SetWindowRect(jk::JKRect{ 0, 0, 320, 240 });
            jk::JKScriptHost ghost2;
            ghost2.Attach(&gwin2);
            ghost2.SetTimerServices(makeTimerServices(gateWinIds, gateSeq));
            ghost2.EnableCapabilities("Timer, input,network,weird ");
            check(ghost2.HasCapability("timer") && ghost2.HasCapability("input") &&
                      ghost2.HasCapability("network") && ghost2.HasCapability("weird") &&
                      !ghost2.HasCapability("widget"),
                  "capability list normalizes (trim+lowercase, unknown kept)");
            check(ghost2.Start("test_script_gate.js"),
                  "declared capability passes the gate");
            check(gateWinIds.size() >= 1,
                  "declared timer claims a timer winId");
            ghost2.Stop();

            // (c) 무조건 허용: 게이트 활성 상태에서 log/assert 통과
            writeScript("test_script_free.js",
                "log(\"gate-ok\");\n"
                "assert(true, \"assert stays free\");\n");
            jk::JKWindow fwin("ScriptGateFree");
            fwin.SetWindowRect(jk::JKRect{ 0, 0, 320, 240 });
            jk::JKScriptHost fhost;
            fhost.Attach(&fwin);
            fhost.EnableCapabilities("");
            check(fhost.Start("test_script_free.js"),
                  "log/assert stay free under an active gate");
            fhost.Stop();

            // (d) 게이트 비활성(EnableCapabilities 미호출)은 기존 셀프테스트
            //     1·2가 회귀 검증으로 겸함 — 여기에 단언 없음(스펙 §6-5).
        }
```

- [ ] **Step 2: 빌드가 실패함을 확인한다** (`EnableCapabilities` 미존재):

```
export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH" && ninja.exe -C build -j3
RC=$?; echo "BUILD RC=$RC"
```
Expected: FAIL — `class jk::JKScriptHost has no member EnableCapabilities` (RC=1).

- [ ] **Step 3: 헤더에 공용 API + 상태를 추가한다** — JKScriptHost.h. 공용은 `SetTimerServices`(:62) 뒤:

```cpp
    // 워크숍 능력 게이트 (docs/74 결정 — 스펙
    // 2026-10-05-workshop-capability-gate §4): 컴마 목록 원문을 받아
    // trim+소문자로 정규화해 보관. ""=능력 없음. 워크숍 앱만 Start 전 1회
    // 부른다(SCRI 앱·셀프테스트는 미호출 → 게이트 비활성). 런타임 변경
    // API 없음 — 선언은 MANI가 유일 원천(라이브 패치도 같은 선언 상속).
    void EnableCapabilities(std::string list);
    // 게이트 활성 여부 = EnableCapabilities가 불렸는지.
    bool GateActive() const { return gateActive_; }
    // 정규화된 토큰 보유 여부.
    bool HasCapability(const std::string& token) const;
```

private 멤버는 `bool patching_ = false;`(:199) 뒤:

```cpp
    // 능력 게이트 상태 (docs/74): 워크숍 앱만 활성. capabilities_는 정규화
    // (trim+소문자)된 토큰 집합 — MANI 원문은 앱 쪽 배지가 보여 준다.
    bool gateActive_ = false;
    std::vector<std::string> capabilities_;
```

- [ ] **Step 4: 게이트를 구현한다** — JKScriptHost.cpp.

4-1) includes 갱신 (`#include <cstring>` 뒤 — 없으면 추가):

```cpp
#include <algorithm>
#include <cctype>
```

4-2) anonymous namespace, `PatchBlocked`(:89-93) 뒤에 헬퍼:

```cpp
// 워크숍 능력 게이트 (docs/74 결정 — fail-closed). 게이트가 활성(워크숍 앱)
// 이고 토큰이 미선언이면 TypeError("capability '<tok>' not declared in
// MANI")를 던진다. 문구는 고정 계약(스펙 §4.2) — 셀프테스트가 단언하고
// jk.d.ts 주석이 문서화한다. 활성 아님(워크숍 앱 아님)이면 무조건 허용.
bool GateCap(JKScriptHost* host, JSContext* ctx, const char* capability) {
    if (!host || !host->GateActive() || host->HasCapability(capability)) {
        return true;
    }
    JS_ThrowTypeError(ctx, "capability '%s' not declared in MANI",
                      capability);
    return false;
}
```

4-3) 멤버 구현 — `SetCursorDeclChanged` 등 public 인라인이 아닌 파일 구현부 아무 위치(관습: Stop/Reload 근처. `void JKScriptHost::Stop()` 앞 권장):

```cpp
void JKScriptHost::EnableCapabilities(std::string list) {
    gateActive_ = true;
    capabilities_.clear();
    size_t pos = 0;
    while (pos <= list.size()) {
        size_t comma = list.find(',', pos);
        if (comma == std::string::npos) comma = list.size();
        const std::string raw = list.substr(pos, comma - pos);
        const size_t b = raw.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) {
            pos = comma + 1;
            continue;  // 빈 토큰 무시 (스펙 §3.1)
        }
        const size_t e = raw.find_last_not_of(" \t\r\n");
        std::string tok = raw.substr(b, e - b + 1);
        for (char& c : tok) {
            c = static_cast<char>(
                std::tolower(static_cast<unsigned char>(c)));
        }
        capabilities_.push_back(std::move(tok));
        pos = comma + 1;
    }
}

bool JKScriptHost::HasCapability(const std::string& token) const {
    return std::find(capabilities_.begin(), capabilities_.end(), token) !=
           capabilities_.end();
}
```

4-4) thunk 삽입 — `bind` 목록(:1145-1174) 전수가 소스이다. 각 thunk 첫 줄(`JKScriptHost* host = HostOf(ctx);`) **바로 다음**에 1줄 추가. 토큰 배정은 **정확히** 이 표대로:

| thunk | 삽입 줄 |
|---|---|
| MessageBox(:281), CreateButton(:292), CreateLabel(:313), CreateEdit(:334), SetText(:353), GetText(:368), CreateDialog, DialogAddLabel, DialogAddEdit, DialogAddButton, DialogShow, DialogClose | `if (!GateCap(host, ctx, "widget")) return JS_EXCEPTION;` |
| SetInterval(:384), ClearInterval | `if (!GateCap(host, ctx, "timer")) return JS_EXCEPTION;` |
| FindControlBinding | `if (!GateCap(host, ctx, "uiauto")) return JS_EXCEPTION;` |
| Click, InjectMouse, InjectKey | `if (!GateCap(host, ctx, "input")) return JS_EXCEPTION;` |
| ReadConfig | `if (!GateCap(host, ctx, "fs")) return JS_EXCEPTION;` |
| CreateCanvas, CanvasClear, CanvasRect, CanvasPixel, CanvasLine, CanvasCircle, CanvasText | `if (!GateCap(host, ctx, "canvas")) return JS_EXCEPTION;` |
| DeclareCursor | `if (!GateCap(host, ctx, "agent")) return JS_EXCEPTION;` |
| **Log, Assert, AssertEq** | **삽입 금지 (무조건 허용 — Global Constraints)** |

게이트 줄은 **IsPatching 줄보다 먼저** 놓는다(능력이 기본, 라이브 패치는 그 위의 재평가 구간 — 둘 다 막는 순서가 정의상 자연). 배치 예 (SetInterval :382-385):

```cpp
        JKScriptHost* host = HostOf(ctx);
        if (!GateCap(host, ctx, "timer")) return JS_EXCEPTION;
        if (host && host->IsPatching()) return PatchBlocked(ctx, "setInterval");
```

예외: HostOf 선두가 없는 thunk가 있으면(예: 일부 dialog thunk가 DialogAddControl 프롤로그로 회단) 그 프롤로그 안의 `host` 확득 직후에 넣는다 — **HostOf(ctx)가 없는 thunk는 없다**가 실제 코드에서 사실인지 반드시 눈으로 확인하고, 확인 결과와 상충하면 각 thunk의 host 획득 직후에 맞춘다.

- [ ] **Step 5: 빌드 + 셀프테스트 녹색**

```
export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH" && ninja.exe -C build -j3
RC=$?; echo "BUILD RC=$RC"
./jkdesktop.exe test
```
Expected: BUILD RC=0, 마지막 줄 `0 failure(s)`. `1g`의 6 check PASS 확인.

- [ ] **Step 6: jk.d.ts 주석 갱신** — 헤더 주석의 버전 주석 블록(:1-10)에 한 줄 추가:

```
// v8: 능력 게이트 (docs/74 결정 — 워크숍 스크립트 앱만, fail-closed): MANI
// capabilities=에 선언된 토큰만 해당 API가 열린다. 토큰 = widget(위젯·다이얼로그)
// / timer(setInterval·clearInterval) / canvas(canvas*) / agent(declareCursor)
// / fs(readConfig) / input(injectMouse·injectKey·click) / uiauto(findControl)
// / network(리저브 — 오늘은 대응 API 없음). **무조건 허용(선언 불요)**: log,
// assert, assertEq. 미선언 호출은 "capability '<tok>' not declared in MANI"
// TypeError로 막는다.
```

(운영 규칙 "바인딩 추가 커밋에는 이 파일 갱신 동봉" 위배 아님 — 바인딩 추가는 없다.)

- [ ] **Step 7: 재빌드 + 테스트 녹색 확인 (Step 5 명령 재실행) 후 커밋:**

```bash
git add engine/include/script/JKScriptHost.h engine/src/script/JKScriptHost.cpp engine/src/main.cpp engine/scripts/jk.d.ts
git commit -m "feat(capability): 워크숍 능력 게이트 — JKScriptHost EnableCapabilities+GateCap fail-closed (docs/74 결정, 스펙 §4)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 2: MANI `capabilities=` 필드 + 파스

**Files:**
- Modify: `engine/include/JKJkxFile.h:48-55` 근방 (멤버 신설)
- Modify: `engine/src/JKJkxFile.cpp:59-60` (Parse 분기)
- Modify: `engine/src/main.cpp` (I2 merge 블록 :1644-1696 직후 파스 테스트)

**Interfaces:**
- Consumes: `JkxManifest::Parse`의 if/else 체인(JKJkxFile.cpp:51-60).
- Produces: `std::string JkxManifest::capabilities` (원문 그대로, ""=선언 없음) — Task 3이 `mani.capabilities`로 소비. Task 1의 `EnableCapabilities`가 토큰 분해를 맡는다(접합점 계약: "원문 보존은 파스, 정규화는 호스트").

- [ ] **Step 1: 실패하는 테스트 먼저** — main.cpp I2 블록(:1647-1696) **직후**에 신설:

```cpp
    // docs/74 능력 게이트 — MANI capabilities= 파스 (스펙 §3.2 — 원문 보존,
    // 토큰 분해는 소비자 JKScriptHost::EnableCapabilities 몫).
    {
        jk::JkxManifest m;
        check(m.Parse("name=x\nmodule=jkapp_script.dll\n"
                      "capabilities=Timer, input,network,weird\n"),
              "mani parse with capabilities succeeds");
        check(m.capabilities == "Timer, input,network,weird",
              "mani capabilities stored verbatim");
        jk::JkxManifest n;
        check(n.Parse("name=x\nmodule=jkapp_script.dll\ncapabilities=\n") &&
                  n.capabilities.empty(),
              "mani empty capabilities = no declaration");
        jk::JkxManifest o;
        check(o.Parse("name=x\nmodule=jkapp_script.dll\n") &&
                  o.capabilities.empty() && !o.scriptfile.empty() == false,
              "mani without capabilities parses as before");
    }
```

- [ ] **Step 2: 빌드 실패 확인** (`capabilities` 미멤버) — Task 1 Step 2 명령 재실행. Expected: RC=1, `struct jk::JkxManifest has no member capabilities`.

- [ ] **Step 3: 구현** — JKJkxFile.h 멤버(`int watch = 0;` :55 뒤):

```cpp
    // 능력 선언 (docs/74 결정 — 스펙 2026-10-05-workshop-capability-gate
    // §3.2, 워크숍 스크립트 앱만 적용). 컴마 목록 원문 그대로 보존 — 토큰
    // 분해(trim+소문자)는 소비자 JKScriptHost::EnableCapabilities 몫.
    // ""=선언 없음.
    std::string capabilities;
```

JKJkxFile.cpp Parse 체인(:59-60 사이, `scriptfile`과 `watch` 사이):

```cpp
        else if (key == "scriptfile") scriptfile = value;  // docs/60 §2.2
        else if (key == "capabilities") capabilities = value;  // docs/74 —
            // 원문 보존: 정규화는 소비자 몫(스펙 §3.2)
        else if (key == "watch") watch = std::atoi(value.c_str());
```

- [ ] **Step 4: 빌드 + 테스트 녹색** — Task 1 Step 5 명령 재실행. Expected: RC=0, `0 failure(s)` (신설 3 check PASS + 기존 I2 13 check 유지).

- [ ] **Step 5: 커밋**

```bash
git add engine/include/JKJkxFile.h engine/src/JKJkxFile.cpp engine/src/main.cpp
git commit -m "feat(capability): MANI capabilities= 필드 — 원문 보존 파스, 정규화는 호스트 몫 (스펙 §3)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 3: 워크숍 앱 배선 + 슬롯 스트립 능력 배지

**Files:**
- Modify: `engine/src/apps/JKAppModule_script.cpp:117-118` (g_capabilities), `:143-144` (meta 캡처), `:181` (주입)
- Modify: `engine/include/apps/ClientScriptApp.h` (WorkshopScriptApp: SetEnabledCapabilities + OnInit 주입 + BuildStrip 배지 + 헬퍼 함수)
- Modify: `engine/src/main.cpp` (배지 문구 셀프테스트)

**Interfaces:**
- Consumes: Task 1 `EnableCapabilities/GateActive/HasCapability`; Task 2 `JkxManifest::capabilities`.
- Produces: `void WorkshopScriptApp::SetEnabledCapabilities(std::string)` — JKAppModule_script.cpp:181이 소비(이 태스크 안); `inline std::string jk::CapabilityBadgeText(const std::string& rawCaps)` — 셀프테스트가 소비; 멤버 `JKStatic* capBadge_` — OnScriptStarted 리레이즈가 소비.

- [ ] **Step 1: 실패하는 배지 문구 테스트 먼저** — main.cpp, Task 1의 1g 블록 직후:

```cpp
        // 1c2) 능력 배지 문구 (docs/74 — 빈 선언도 숨기지 않는다, 스펙 §5).
        check(jk::CapabilityBadgeText("agent,timer") == "능력: agent,timer",
              "capability badge text with declaration");
        check(jk::CapabilityBadgeText("") == "능력 없음",
              "capability badge text without declaration");
```

- [ ] **Step 2: 빌드 실패 확인** (`CapabilityBadgeText` 미존사) — RC=1.

- [ ] **Step 3: 배선 구현** — JKAppModule_script.cpp:

3-1) statics (:117-118):

```cpp
std::string g_scriptfile;
bool g_watch = false;
// 능력 선언 원문 (docs/74 능력 게이트 — 배지 문구와 게이트 주입의 유일 원천).
std::string g_capabilities;
```

3-2) meta 캡처 (:143-144 뒤):

```cpp
                g_scriptfile = mani.scriptfile;
                g_watch = (mani.watch != 0);
                g_capabilities = mani.capabilities;  // docs/74 능력 게이트
```

3-3) workshop 분기 주입 (:181 `app.SetAgentAppName(meta->name);` 뒤):

```cpp
        app.SetAgentAppName(meta->name);
        // 능력 게이트 (docs/74 — 워크숍만): 빈값도 주입한다(선언 없음 =
        // 능력 없음, 배지가 그대로 보여 준다). ClientScriptApp 분기는
        // 주입하지 않는다 — 게이트 비활성 (결정 3).
        app.SetEnabledCapabilities(g_capabilities);
```

- [ ] **Step 4: 배지 구현** — ClientScriptApp.h.

4-1) 헬퍼 — `class WorkshopScriptApp` **직전**(namespace 스코프):

```cpp
// 능력 배지 문구 (docs/74 — 수신자 가시성, 스펙 §5). MANI capabilities=
// 원문을 그대로 보여 준다(미지 토큰 포함 — 선언 자체가 문서화). 빈 선언도
// 숨기지 않는다: 능력 없음이 표시되는 극단을 견뎌야 선언 문화가 성립.
inline std::string CapabilityBadgeText(const std::string& rawCaps) {
    if (rawCaps.empty()) return "능력 없음";
    return "능력: " + rawCaps;
}
```

4-2) `SetAgentAppName`(:295) 뒤 공용:

```cpp
    // 능력 게이트 (docs/74 — 워크숍 앱만): MANI capabilities= 원문을 전달.
    // 주입 자체는 OnInit에서(Start 전 1회 — 리로드·패치도 같은 호스트
    // 인스턴스라 선언 상속). 빈값도 유의미(능력 없음 배지).
    void SetEnabledCapabilities(std::string list) {
        capabilitiesRaw_ = std::move(list);
    }
```

4-3) `OnInit` 주입 — `ScriptAppT<JKClientApplication>::OnInit();`(:311) **직전**:

```cpp
        // 능력 게이트 주입 (docs/74) — StartScript 전 1회. 빈 선언도
        // EnableCapabilities를 부르므로 게이트는 활성(fail-closed).
        host_->EnableCapabilities(capabilitiesRaw_);
```

4-4) `BuildStrip` 배지 — `main->AddControl(std::move(combo));`(:454) 뒤:

```cpp
        // 능력 배지 (docs/74): 콤보 끝(클라 x 210)+6 오른쪽. X 버튼 침범
        // 금지(오른쪽 여백 30)·최소 폭 60 — 어두운 창에서는 생략(문구 계약
        // 은 셀프테스트가 단독 검증한다).
        const JKRect cr = main->GetClientRect();
        constexpr int kBadgeX = 216;
        if (cr.w - kBadgeX - 30 >= 60) {
            auto badge = std::make_unique<JKStatic>(
                JKRect{ kBadgeX, -18, cr.w - kBadgeX - 30, 18 }, 0);
            badge->SetText(jk::Utf8ToKssm(
                CapabilityBadgeText(capabilitiesRaw_).c_str()));
            capBadge_ = badge.get();
            main->AddControl(std::move(badge));
        }
```

4-5) `OnScriptStarted` 리레이즈 — `main->MoveChildToTop(slotCombo_);`(:433) 뒤:

```cpp
            if (capBadge_) main->MoveChildToTop(capBadge_);
```

4-6) 멤버 — `JKStatic* slotLabel_ = nullptr;`(:491) 뒤:

```cpp
    JKStatic* capBadge_ = nullptr;
    // MANI capabilities= 원문 — 배지 문구와 게이트 주입의 유일 원천.
    std::string capabilitiesRaw_;
```

- [ ] **Step 5: 빌드 + 테스트 녹색** — RC=0, `0 failure(s)` (1c2 배지 2 check 포함).

- [ ] **Step 6: 커밋**

```bash
git add engine/src/apps/JKAppModule_script.cpp engine/include/apps/ClientScriptApp.h engine/src/main.cpp
git commit -m "feat(capability): 워크숍 배선(MANI→host 주입)+슬롯 스트립 능력 배지 (스펙 §3.3/§5)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 4: 워크숍 MANI 선언 + 라이브 게이트 실측 (end-to-end)

**Files:**
- Modify: `engine/scripts/apps/workshop/manifest.txt` (capabilities= 선언 — 주의: 현재 마지막 행 watch=1 뒤에 개행이 없다)
- Create: `engine/tools/probes/diag_capgate.ps1` (라이브 게이트 실측 — 기존 probe 패턴 상속)

**Interfaces:**
- Consumes: Task 1-3의 완성 게이트. probe는 **외부 스택** 패턴(서버는 bash가 띄우고 probe는 측정만 — docs/75 §3b 원장 4건 숙지).
- Produces: 실측 증적 = set_script 토크 응답의 고정 에러 문구 캡처.

- [ ] **Step 1: MANI 선언** — `engine/scripts/apps/workshop/manifest.txt`. 현재 내용(마지막 행 개행 없음):

```
name=workshop
title=Workshop
width=360
height=280
module=jkapp_script.dll
scriptfile=state/scripts/myapp.js
watch=1
```

파일 전체를 아래로 교체 (docs/74 — 선언은 합유합이 아니라 문서: 워크숍 벤치 런타임의 능력 원문):

```
name=workshop
title=Workshop
width=360
height=280
module=jkapp_script.dll
# 능력 선언 (docs/74 fail-closed): 워크숍 벤치 슬롯 런타임의 합 유능력.
# 슬롯 출하(.jkx) MANI는 각자 좁게 선언한다 — 이 원본은 벤치 원문이다.
# 미선언 토큰(input/uiauto/network)은 차단 — 슬롯이 쓰려면 이 파일에
# 토큰을 추가하라(정직 에러가 어디를 고칠지 말해 준다: "not declared in MANI").
capabilities=widget,timer,canvas,agent,fs
scriptfile=state/scripts/myapp.js
watch=1
```

- [ ] **Step 2: 빌드 + 셀프테스트 녹색** (코드 불변이지만 MANI가 셀프테스트 경로 아닌지 RC 확인) — Task 1 Step 5 명령.

- [ ] **Step 3: 스택 복원 + probe 실행** — 스택이 살아 있으면 죽이고 빌드한 서버로 복원 후:

```powershell
# diag_capgate.ps1 — 능력 게이트 end-to-end: set_script에 미선언 API를 심어
# 토크 응답의 고정 에러 문구를 확인한다 (probe-ws 격리 — scratch 슬롯 사용,
# 사용자 슬롯 파일 불변).
$build = "I:\progwork\JKENGINE\engine\build"
$mk = '{"tool":"launch_app","args":{"app":"workshop"}}'.Replace('"', '\"')
$null = (& "$build\jkdesktop.exe" agentctl $mk)
Start-Sleep -Seconds 3
# 1) 미선언 토큰(input) — 기대: capability 'input' not declared in MANI
$src = '{"tool":"app_tool","args":{"app":"workshop","tool":"set_script","args":' +
       '{"slot":"gatescratch","source":"var probe = injectMouse(10, 10);"}}}' -replace '"', '\"'
$out = & "$build\jkdesktop.exe" agentctl $src | Out-String
Write-Output ("GATE-BLOCK: " + (& {
    if ($out -match "capability 'input' not declared in MANI") { "OK" } else { "MISS: " + $out }
}))
# 2) 선언 토큰(widget) — 기대: ok:true (게이트 통과 + 슬래시 정상 재평가)
$src2 = '{"tool":"app_tool","args":{"app":"workshop","tool":"set_script","args":' +
        '{"slot":"gatescratch","source":"var l = createLabel({x:10,y:10,w:100,h:24}, \"gate-ok\");"}}}' -replace '"', '\"'
$out2 = & "$build\jkdesktop.exe" agentctl $src2 | Out-String
Write-Output ("GATE-PASS: " + (& {
    if ($out2 -match '"ok"\s*:\s*true') { "OK" } else { "MISS: " + $out2 }
}))
Write-Output "done"
```

주의: JSON 인자는 PowerShell 이중 이스케이프 `{\"k\":..}` 표기만 (memory 렛슨 — 사용자 bad_request 실측). probe 후 scratch 슬롯 파일 삭제: `rm state/scripts/gatescratch.js` (없으면 무시).

Expected: `GATE-BLOCK: OK`, `GATE-PASS: OK`. MISS면 서버 로그(ch_server.out = stdout)로 원인 판명 후 probe 정정 — 코드를 함부로 고치지 않는다(게이트는 Task 1-3에서 셀프테스트로 검증 완료; 여기는 배선·문구의 생태계 실측).

- [ ] **Step 4: 스택 정리 + 커밋**

```bash
git add engine/scripts/apps/workshop/manifest.txt engine/tools/probes/diag_capgate.ps1
git commit -m "feat(capability): 워크숍 MANI 선언 widget/timer/canvas/agent/fs + 라이브 게이트 실측 probe (docs/74 fail-closed end-to-end)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 5: as-built 문서 + 원장 갱신

**Files:**
- Create: `docs/76_workshop_capability_gate_asbuilt.md`
- Modify: `docs/74_workshop_trust_decision.md` (결정 이행 상태 갱신 — 헤더 결정 기록 아래 1줄)

**Interfaces:** 문서 태스크 — 코드 소비자 없음. 렛슨/트랩은 실측에서 온 것만 원장화.

- [ ] **Step 1: as-built** — docs/76에 기록: 결정 3항 + 토큰 표 최종 + 배선 경로(MANI→g_capabilities→SetEnabledCapabilities→EnableCapabilities→GateCap) + 게이트 문구 계약 + 배지 문구 계약 + 마이그레이션(기존 슬롯·폰 실기기: 미선언 API 쓰던 슬롯은 MANI 토큰 추가 필요 — 슬롯 출하 도구가 개별 MANI를 쓰는 단 2 라인의 다음 문) + 실측 receipt(diag_capgate BLOCK/PASS 캡처 라인).

- [ ] **Step 2: docs/74 갱신** — 결정 기록 블록 뒤 한 줄:

```markdown
> **이행:** 스펙 docs/superpowers/specs/2026-10-05-workshop-capability-gate-design.md
> + 플랜 docs/superpowers/plans/2026-10-05-workshop-capability-gate.md → 구현
> 완결 (as-built: docs/76).
```

- [ ] **Step 3: 커밋 + 푸시 + 메모리 원장 갱신 (jkengine_next_backlog 갱신 31)**

```bash
git add docs/76_workshop_capability_gate_asbuilt.md docs/74_workshop_trust_decision.md
git commit -m "docs: 워크숍 능력 게이트 as-built (docs/76) + docs/74 이행 기록

Co-Authored-By: Claude Code <noreply@anthropic.com>"
git push
```

---

## 셀프리뷰 (작성자 — 플랜 대 스펙)

- 토큰 표 적용: widget 12·timer 2·canvas 7·agent 1·fs 1·input 3·uiauto 1 = 27 바인딩 중 26 곳 삽입 + 무조건 허용 3(log/assert/assertEq — Log·Assert·AssertEq thunk, 바인딩 29 중 미삽입 3) — 스펙 §2 전체 커버. `network`는 리저브(삽입 대상 없음) — OK.
- 스펙 §6 셀프테스트: 1(fail-closed+문구)=Task 1 (a) 2(선언 통과)=Task 1 (b) 3(무조건 허용)=Task 1 (c) 4(MANI)=Task 2 5(워크숍만)=기존 케이스 1-2 회귀+Task 1 (d) 명시 6(배지 문구)=Task 3 — 전 커버.
- 스펙 §5 배지 위치·문구·빈 선언 = Task 3; §8 마이그레이션 = Task 4 MANI 주석+docs/76.
- 타입 일치: `EnableCapabilities(std::string)` — Task 1 정의=Task 3 소비 동일; `capabilitiesRaw_`/`capBadge_`/`g_capabilities` 명칭 태스크 간 일치 확인 완료.
```