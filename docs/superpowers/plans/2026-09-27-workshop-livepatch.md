# 워크숍 라이브 패치 (컨텍스트 생존 정의 재평가) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `set_script {source, slot?, live:1}`가 QuickJS 컨텍스트를 죽이지 않고 정의만 재평가하게 한다 — 타이머 카운터·클로저·글로벌 프로퍼티·의미 커서 선언이 패치에 생존한다(React Fast Refresh / Erlang 핫 코드 방식).

**Architecture:** JKScriptHost에 `CompileGate`(JS_Eval + `JS_EVAL_FLAG_COMPILE_ONLY`, 부작용 0의 문법 게이트)와 `PatchEval`(프리패스 → 같은 컨텍스트에서 `JS_EvalFunction` 재평가, 재평가 구간엔 `patching_` 플래그로 생성류 바인딩을 `bad_patch` 에러로 차단)을 신설한다. ClientScriptApp의 set_script에 live 분기 추가 — 실패 이원화(컴파일 실패=파일 미기록+에러 에코 / 런타임 예외=자동 풀 리로드 낙하+에러 에코), 기본 경로(live 부재=풀 리로드) 불변. 서버·와이어·브리지 무수정.

**Tech Stack:** C++20, vendored quickjs-ng (`JS_EVAL_FLAG_COMPILE_ONLY` quickjs.h:457, `JS_EvalFunction` :1278), CMake+msys64 ucrt64, 유닛 프로브=workshop_slot_probe.cpp, e2e 프로브=PowerShell 5.1 (ASCII 전용).

**Spec:** `docs/superpowers/specs/2026-09-27-workshop-livepatch-design.md` (승인 완료 — §4 흐름, §5 구현 지점, §6 게이트가 이 플랜의 근거)

## Global Constraints

- **파일=진실원 (원칙 1)** — 컴파일 게이트 통과 전 파일 기록 금지. 패치 성공 후엔 파일==실행 상태.
- **폐곡선 (원칙 3)** — 양 실패 경로 모두 에러를 도구 응답으로 에코한다. 조용한 눌먹기 금지.
- **기본 경로 불변** — `live` 인자 부재/0 = 기존 풀 리로드 경로 그대로(스냅샷→기록→ReloadNow/SwitchToSlot 순서·응답 필드 무수정).
- **Stop()/Start() 무수정** — Patch는 별도 경로(컨텍스트 재사용), 리로드 계약(docs/67 단 1: pending 1건 ↔ Start 1회)을 건드리지 않는다.
- **`cursorDeclJson_` 보존** — PatchEval은 클리어하지 않는다(Start와 다름). 재선언 시 기존 dedupe/갱신 로직이 그대로 작동.
- **프로브 규율** — PS5.1 ASCII-only, 공식 런 ×2, 슬롯 격리(probe-ws), **permissions.json은 프로브가 절대 건드리지 않는다**(docs/59 §16.1).
- **테스트 미디어** — i:\@keep 영상 절대 사용 금지. 엔진 소유 클립이나 합성 클립만(이 태스크엔 불요).
- **커밋 트레일러** — 모든 커밋에 `Co-Authored-By: Claude Code <noreply@anthropic.com>`.
- **빌드 규약** — `engine/build`에서 `PATH=/c/msys64/ucrt64/bin cmake --build . --target <T>`. JKScriptHost는 libjkcore에 정적 링크 → **jkapp_script.dll 재빌드 + `pack_workshop.ps1` 재팩은 세트**(docs/60 §10.1 레슨).
- **jk.d.ts 규칙** — additive-only, 버전 주석 갱신(v7), 바인딩 추가 커밋에 갱신 동봉.

---

### Task 1: JKScriptHost — CompileGate·PatchEval·패치 게이트 + 유닛 프로브

**Files:**
- Modify: `engine/include/script/JKScriptHost.h` (Reload() 선언 뒤, 멤버는 `cursorDeclJson_` 근처 :187 부근)
- Modify: `engine/src/script/JKScriptHost.cpp` (Reload() 정의 :1262-1267 뒤; 바인딩 9곳 서두)
- Modify: `engine/tools/probes/workshop_slot_probe.cpp` (TestLivePatch 신설 + main() 배선)

**Interfaces:**
- Consumes: 기존 `DumpPendingException(JSContext*)` (cpp 내부 헬퍼), `ThrowTypeError(ctx, what, why)` (:83), `controls_` (public, `std::vector<std::pair<uint16_t, JKControl*>>`), `lastError_`, `Start/Stop/Reload/IsRunning/LastError/DeclaredCursorJson/CaptureWidgetState/DispatchClick`.
- Produces (Task 2·3이 사용):
  - `bool CompileGate(const std::string& source, std::string* error = nullptr)` — 문법만 판정, 상태 무변화.
  - `bool PatchEval(const std::string& source, std::string* error = nullptr)` — 프리패스→재평가. 성공 `true`, 실패 시(문법·런타임 모두) `false` + `error`에 에러 문구. **컨텍스트는 항상 생존** — 낙하 판단은 호출자 몫.
  - `bool IsPatching() const` — 바인딩 차단 게이트가 읽는다.

- [ ] **Step 1: 헤더에 선언 추가**

`engine/include/script/JKScriptHost.h` — `void Reload()` 선언(주석 "Reload support" 블록) 직후:

```cpp
    // 라이브 패치 (docs/67 단 1 리파인 3호, 스펙 §5): 컨텍스트를 죽이지 않고
    // 정의만 재평가. CompileGate는 COMPILE_ONLY로 문법만 판정(부작용·상태
    // 변화 없음), PatchEval은 프리패스→재평가. 실패 이원화(문법=무손상 에코 /
    // 런타임=풀 리로드 낙하)는 호출자(ClientScriptApp)의 몫이다.
    bool CompileGate(const std::string& source, std::string* error = nullptr);
    bool PatchEval(const std::string& source, std::string* error = nullptr);
    // PatchEval의 재평가 구간 표식 — 생성류 바인딩(Bindings::PatchBlocked)이
    // 읽어 bad_patch로 막는다.
    bool IsPatching() const { return patching_; }
```

멤버(`cursorDeclJson_` 선언 근처):

```cpp
    bool patching_ = false;  // PatchEval 재평가 구간 — 생성류 바인딩이 bad_patch로 막는다
```

- [ ] **Step 2: 유닛 프로브에 실험 코드 먼저 (TDD)**

`engine/tools/probes/workshop_slot_probe.cpp` — 기존 `WriteFile(dir, name, content)` 헬퍼와 `Check(name, ok, detail)` 관용구를 그대로 쓴다. `TestHostState`의 셋업 관용구(:172-176) 그대로: `JKWindow win(...)` → `host.Attach(&win)` → `JKScriptTimerServices noop; noop.start = [](uint32_t, uint32_t) -> uint64_t { return 1; }; noop.stop = [](uint64_t) {}; host.SetTimerServices(noop);`

파일 말미(기존 테스트 함수들 뒤, main() 앞)에 추가:

```cpp
// --- (라이브 패치 — 컨텍스트 생존 정의 재평가, 스펙 §6 게이트 ①) ---
// 생존 단위: 위젯(native)·글로벌 프로퍼티·의미 커서 선언. 교체: top-level
// function 정의. 차단: 패치 평가 중 생성류(bad_patch). 미호출: onCreate/onExit.
// 텍스트 판독은 CaptureWidgetState(편집창 텍스트만 수록 — 라벨·버튼은 스크립트
// 소유 파생 출력)로 한다 — 기존 b2/b2b 체크와 같은 판독 경로.
static void TestLivePatch(const fs::path& dir) {
    JKWindow win("livepatch-probe");
    JKScriptHost host;
    host.Attach(&win);
    JKScriptTimerServices noop;
    noop.start = [](uint32_t, uint32_t) -> uint64_t { return 1; };
    noop.stop = [](uint64_t) {};
    host.SetTimerServices(noop);

    WriteFile(dir / "live_a.js",
        "globalThis.S = {count: 1};\n"
        "var btn = createButton({x:0,y:0,w:120,h:24}, 'v1');\n"
        "var ed = createEdit({x:0,y:30,w:160,h:22}, '');\n"
        "function onClick(id){ S.count = S.count + 1; setText(ed, 'hit' + S.count); }\n");
    Check("f1-start", host.Start((dir / "live_a.js").string()),
          host.LastError());

    // 구 정의 클릭 — 상태 시드: count 1→2, 편집창 "hit2".
    const uint16_t btnId = host.controls_.front().first;
    host.DispatchClick(btnId);
    std::string snap;
    Check("f2-seed-click", host.CaptureWidgetState(snap) &&
                               snap.find("hit2") != std::string::npos, snap);

    // 컴파일 게이트: 통과는 무부작용, 실패는 컨텍스트 무손상(스펙 §4 흐름 1).
    Check("f3-gate-ok", host.CompileGate(
        "function onClick(id){ S.count = S.count + 10; "
        "setText(ed, 'patched' + S.count); }\n"));
    Check("f4-gate-noop", host.IsRunning() && host.controls_.size() == 1);
    Check("f5-gate-syntax-fail", !host.CompileGate(
        "function onClick(id { setText(id, 'x'); }\n"),
        host.LastError());
    Check("f6-gate-fail-intact", host.IsRunning() &&
                                     host.controls_.size() == 1);

    // 재평가 성공 — 위젯 수 불변, 새 정의가 전역 조회로 해석된다.
    Check("f7-patch-ok", host.PatchEval(
        "function onClick(id){ S.count = S.count + 10; "
        "setText(ed, 'patched' + S.count); }\n"), host.LastError());
    Check("f8-patch-alive", host.IsRunning() && host.controls_.size() == 1);
    host.DispatchClick(btnId);
    std::string snap2;
    Check("f9-state-survives", host.CaptureWidgetState(snap2) &&
                                   snap2.find("patched12") != std::string::npos,
          snap2);  // 구 상태(2) 생존 + 새 정의(+10)

    // 재평가 중 생성류 차단 — bad_patch 에러가 규약을 교육한다(폐곡선).
    Check("f10-patch-create-blocked", !host.PatchEval(
        "createButton({x:0,y:60,w:20,h:20}, 'dup');\n"), host.LastError());
    Check("f11-block-error-text",
          host.LastError().find("live patch") != std::string::npos,
          host.LastError());
    Check("f12-no-dup-widget", host.controls_.size() == 1);

    // 패치 중 문법 실패 — 컨텍스트 무손상, 구 정의가 그대로 바인딩.
    Check("f13-patch-syntax-fail", !host.PatchEval(
        "function onClick(id { setText(id, 'x'); }\n"), host.LastError());
    Check("f14-syntax-fail-alive", host.IsRunning());
    host.DispatchClick(btnId);
    std::string snap3;
    Check("f15-old-def-still-bound", host.CaptureWidgetState(snap3) &&
                                         snap3.find("patched13") !=
                                             std::string::npos, snap3);

    // top-level const 재선언 — 재평가 시 SyntaxError → 호출자가 낙하 판단.
    // 호스트는 죽지 않고(스펙 §4 유수정) 구 정의가 살아 남는다.
    Check("f16-const-redecl-fails", !host.PatchEval("const S = {count: 0};\n"),
          host.LastError());
    Check("f17-const-fail-alive", host.IsRunning());
    host.DispatchClick(btnId);
    std::string snap4;
    Check("f18-old-def-after-const", host.CaptureWidgetState(snap4) &&
                                         snap4.find("patched14") !=
                                             std::string::npos, snap4);

    // 커서 선언 보존 — 패치는 cursorDeclJson_을 클리어하지 않는다(스펙 §3).
    WriteFile(dir / "live_cursor.js",
        "declareCursor({origin:{x:10,y:10}, cellW:30, cellH:25, rows:8, "
        "cols:10, kinds:['paint']});\n"
        "function onAgentAct(kind,row,col){ return '{\\\"ok\\\":true}'; }\n");
    Check("f19-cursor-start", host.Start((dir / "live_cursor.js").string()),
          host.LastError());
    const std::string decl0 = host.DeclaredCursorJson();
    Check("f20-decl-seeded", !decl0.empty(), decl0);
    Check("f21-patch-act-fn", host.PatchEval(
        "function onAgentAct(kind,row,col){ "
        "return '{\\\"ok\\\":true,\\\"patched\\\":true}'; }\n"),
        host.LastError());
    Check("f22-decl-preserved", host.DeclaredCursorJson() == decl0,
          host.DeclaredCursorJson());
    Check("f23-patch-redeclare", host.PatchEval(
        "declareCursor({origin:{x:10,y:10}, cellW:30, cellH:25, rows:8, "
        "cols:10, kinds:['paint','chord']});\n"), host.LastError());
    Check("f24-decl-updated",
          host.DeclaredCursorJson().find("chord") != std::string::npos,
          host.DeclaredCursorJson());

    // 풀 리로드 회귀 — Stop+Start 경로는 무수정(라이브 경로가 옆길일 뿐).
    Check("f25-full-reload-works", host.Reload());
    Check("f26-reload-fresh-cursor",
          host.DeclaredCursorJson().find("chord") != std::string::npos);
}
```

main()에서 `TestHostState` 호출 뒤에 배선:

```cpp
    {
        const fs::path lp = dir / "livepatch";
        fs::create_directories(lp);
        TestLivePatch(lp);
    }
```

(`fs::path` 별칭은 파일 기존 관용구를 따른다 — `TestHostState`가 이미 `fs::path` 인자를 받으므로 별칭 존재.)

- [ ] **Step 3: 프로브 빌드 → 컴파일 실패 확인(f-시리즈가 CompileGate/PatchEval 미정의로 FAIL)**

아드혹 빌드 레시피(docs/61:701-716 관용구 — 수정 .o를 libjkcore.a **앞에** 링크):

```bash
cd engine/build
g++ -std=c++20 -c -I../../include -I../../legacy/wancode \
  -I../../third_party/quickjs-ng -I/c/msys64/ucrt64/include/SDL2 \
  ../tools/probes/workshop_slot_probe.cpp -o /tmp/workshop_slot_probe.o
g++ -o /tmp/workshop_slot_probe.exe /tmp/workshop_slot_probe.o \
  ../../build/libjkcore.a -lSDL2 -lSDL2_mixer -limm32 -lwinmm \
  -lole32 -luuid -loleaut32 -lws2_32 -lshlwapi -ldbghelp
```

Run: `/tmp/workshop_slot_probe.exe`
Expected: **빌드 자체가 실패** — `'CompileGate' is not a member of 'jk::JKScriptHost'`. (링크가 통과하면 Step 5 이후 재확인.)

- [ ] **Step 4: 최소 구현 — CompileGate·PatchEval·바인딩 차단**

`engine/src/script/JKScriptHost.cpp` — `Reload()` 정의(:1262-1267) 직후:

```cpp
// ---------------------------------------------------------------------------
// 라이브 패치 (스펙 §4-§5): 컨텍스트 생존 정의 재평가. 리로드(Stop+Start)와
// 별도 경로 — Stop()/Start()는 무수정. 실패 이원화는 호출자 몫: 문법 실패는
// 컨텍스트 무접촉, 런타임 예외는 컨텍스트가 살아 있되 호출자가 풀 리로드로
// 회수한다.
// ---------------------------------------------------------------------------

// 컴파일 게이트 — COMPILE_ONLY(quickjs.h:457)로 문법만 판정. 컴파일 산출물은
// 즉시 폐기(무부작용), 전역 오염 없다. 실패 시 pending exception은 소비해
// 마른다(이후 Dispatch*가 오염되지 않게).
bool JKScriptHost::CompileGate(const std::string& source,
                               std::string* error) {
    lastError_.clear();
    if (!ctx_) {
        lastError_ = "JKScriptHost::CompileGate: not running";
        if (error) *error = lastError_;
        return false;
    }
    JSValue fn = JS_Eval(static_cast<JSContext*>(ctx_), source.c_str(),
                         source.size(), "<patch-gate>",
                         JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(fn)) {
        if (error) *error = DumpPendingException(ctx_);
        lastError_ = *error;
        return false;
    }
    JS_FreeValue(fn);
    return true;
}

// 재평가 — 프리패스 바이트코드를 같은 컨텍스트에서 JS_EvalFunction
// (quickjs.h:1278)으로 실행. onCreate/onExit는 호출하지 않고 cursorDeclJson_은
// 보존한다(Start와 다름 — 패치는 창 수명 안의 조작, docs/64 "창 닫힘에만
// 소멸"). 재평가 구간엔 patching_을 세워 생성류 바인딩을 막는다(스펙 §3).
bool JKScriptHost::PatchEval(const std::string& source, std::string* error) {
    lastError_.clear();
    if (!ctx_) {
        lastError_ = "JKScriptHost::PatchEval: not running";
        if (error) *error = lastError_;
        return false;
    }
    JSContext* ctx = static_cast<JSContext*>(ctx_);
    JSValue fn = JS_Eval(ctx, source.c_str(), source.size(), "<patch>",
                         JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(fn)) {
        if (error) *error = DumpPendingException(ctx);
        lastError_ = *error;
        return false;
    }
    // JS_EvalFunction은 fn 소유를 넘겨받는다(quickjs.h:1278) — 자유점수 이전.
    patching_ = true;
    JSValue rv = JS_EvalFunction(ctx, fn);
    patching_ = false;
    if (JS_IsException(rv)) {
        // 예외는 소비 — 컨텍스트는 살아 있다(낙하 판단은 호출자 몫).
        if (error) *error = DumpPendingException(ctx);
        lastError_ = *error;
        return false;
    }
    JS_FreeValue(rv);
    return true;
}
```

(`DumpPendingException`이 내부 네임스페이스 헬퍼면 같은 TU 안이므로 그대로 접근 — Reload() 뒤 배치는 그보다 뒤여도 된다. 정적 헬퍼가 cpp 앞부분(:83 근처)에 있으므로 접근 가능.)

**차단 게이트 헬퍼** — `ThrowTypeError`(:83) 옆에:

```cpp
// 라이브 패치 재평가 중 생성류 차단 (스펙 §3 bad_patch). 에러 문구가 규약을
// 교육한다(폐곡선) — 어디로 옮길지를 말해 준다.
JSValue PatchBlocked(JSContext* ctx, const char* what) {
    return ThrowTypeError(ctx, what,
        "not allowed during a live patch - move creation into onCreate, "
        "or use a full reload (live:0)");
}
```

9개 생성류 바인딩 서두(HostOf 직후, 실 파라미터 파싱 전)에 2줄씩 삽입 — 대상: `CreateButton`, `CreateLabel`, `CreateEdit`, `CreateCanvas`, `CreateDialog`, `DialogAddLabel`, `DialogAddEdit`, `DialogAddButton`, `SetInterval`:

```cpp
        if (host->IsPatching()) return PatchBlocked(ctx, "createButton");
```

(각 바인딩 이름으로 두 번째 인자 교체. **차단하지 않는 것**: setText/getText/click/injectMouse/injectKey/declareCursor/clearInterval/messageBox/dialogShow/dialogClose — 패치 중 조작·조회·재선언은 허용이 계약이다.)

각 바인딩 위치는 `grep -n "static JSValue CreateButton" engine/src/script/JKScriptHost.cpp` 류로 찾는다 — CreateButton :282 부근, CreateLabel :306, CreateEdit :325, SetInterval :370-390, 다이얼로그 4종 :500-630, CreateCanvas :832 부근.

- [ ] **Step 5: 프로브 재빌드 → 전체 PASS 확인**

Step 3과 같은 빌드 레시피(이번엔 `JKScriptHost.cpp`도 먼저 수정 .o로 컴파일해 libjkcore 앞에 링크 — docs/61:701-716):

```bash
cd engine/build
g++ -std=c++20 -c -I../../include -I../../legacy/wancode \
  -I../../third_party/quickjs-ng -I/c/msys64/ucrt64/include/SDL2 \
  ../src/script/JKScriptHost.cpp -o /tmp/JKScriptHost_patched.o
g++ -std=c++20 -c -I../../include -I../../legacy/wancode \
  -I../../third_party/quickjs-ng -I/c/msys64/ucrt64/include/SDL2 \
  ../tools/probes/workshop_slot_probe.cpp -o /tmp/workshop_slot_probe.o
g++ -o /tmp/workshop_slot_probe.exe /tmp/JKScriptHost_patched.o \
  /tmp/workshop_slot_probe.o ../../build/libjkcore.a \
  -lSDL2 -lSDL2_mixer -limm32 -lwinmm -lole32 -luuid -loleaut32 \
  -lws2_32 -lshlwapi -ldbghelp
```

Run: `/tmp/workshop_slot_probe.exe`
Expected: `f1`~`f26` 전부 ok, 기존 체크(a/b/c/d/e 계열) 회귀 0. 실패 시 리턴 전에 원인 정착(스냅샷 detail이 붙어 있다).

- [ ] **Step 6: 전체 유닛 프로브 재빌드·기존 체크 회귀**

CMake 타깃 빌드도 통과 확인(정적 링크 본빌드):

```bash
cd engine/build
PATH=/c/msys64/ucrt64/bin cmake --build . --target workshop_slot_probe
./workshop_slot_probe.exe
```

Expected: 전체 ALL PASS(기존 50체크+신설 f계열).

- [ ] **Step 7: 커밋**

```bash
git add engine/include/script/JKScriptHost.h engine/src/script/JKScriptHost.cpp engine/tools/probes/workshop_slot_probe.cpp
git commit -m "feat(script): 라이브 패치 — CompileGate·PatchEval·생성류 게이트 (docs/67 단 1 리파인 3호, 스펙 §5)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 2: set_script live 분기 + 교육 표면(카탈로그·프리앰블·jk.d.ts v7)

**Files:**
- Modify: `engine/include/apps/ClientScriptApp.h` (set_script 도구 등록 :354-363, set_script 분기 :518-570, kApiCatalog :776-820)
- Modify: `engine/src/agent/JKLlmEngine.cpp:105-119` (kLlmTurnPreamble에 라우팅 1문장)
- Modify: `engine/scripts/jk.d.ts` (헤더 v7 + 패치 안전 형태 주석)

**Interfaces:**
- Consumes: Task 1의 `host_->CompileGate(source, &err)` / `host_->PatchEval(source, &err)` / `host_->IsRunning()`.
- Produces: set_script 응답 계약(폰 브리지 generic 릴레이가 그대로 흘린다 — 와이어 무수정):
  - 성공 `{"ok":true,"live":true,"slot":"<s>","gen":N}`
  - 게이트 실패 `{"ok":false,"live":true,"slot":"<s>","error":"<문법 에러>","hint":"..."}`
  - 런타임 예외 낙하 `{"ok":true,"live":false,"slot":"<s>","gen":N,"error":"<에러>","note":"recovered by full reload"}`
  - 라이브 불가 조건(다른 슬롯·호스트 미러닝)은 기존 경로로 자동 낙하 — 응답에 `"live":false` 표기.
  - live 부재/0 = 기존 응답 불변(`{"ok":true,"slot":...,"gen":N}` — live 필드 신설이므로 추가는 additive).

- [ ] **Step 1: set_script 도구 등록 갱신 (live 인자 노출)**

`ClientScriptApp.h` `SendToolRegister()` 내 set_script 항목(:354-363)을:

```cpp
            {"set_script",
             "Replace the workshop script and reload it synchronously — "
             "a script error comes back in the same response. slot optional: "
             "writes another slot and auto-switches; the pre-write source is "
             "snapshotted to the version ribbon (.history/<slot>/NNNN.js). "
             "live:1 = patch the running context instead of a full reload "
             "(widget/timer state survives) — same slot only; use it for "
             "small edits of callback bodies",
             "{\"type\":\"object\",\"properties\":{\"source\":{\"type\":"
             "\"string\"},\"slot\":{\"type\":\"string\"},\"live\":{\"type\":"
             "\"number\"}},\"required\":[\"source\"]}"},
```

- [ ] **Step 2: set_script 분기 — 라이브 경로 (스펙 §4 흐름 그대로)**

`if (tool == "set_script")` 블록(:518) — `live` 파싱을 args 파싱 바로 뒤에 추가:

```cpp
            int live = 0;
            (void)args.GetInt("live", live);
```

`ResolveSlot` 성공 뒤, 리본 스냅샷 직전에 라이브 분기 삽입(스냅샷·기록 순서 재배치 — 게이트 통과 전엔 파일을 건드리지 않는다):

```cpp
            // 라이브 패치 (스펙 §4): 같은 슬롯+호스트 러닝 조건에서만. 그 외는
            // live 요청 무시하고 기존 경로(응답에 live:false 표기). 게이트 실패
            // = 스냅샷·기록 모두 스킵(파일·컨텍스트 무손상 — 원칙 1).
            const bool liveCapable =
                live != 0 && scriptPath_ == path && host_->IsRunning();
            if (liveCapable) {
                std::string gateErr;
                if (!host_->CompileGate(source, &gateErr)) {
                    out = "{\"ok\":false,\"live\":true,\"slot\":\"" +
                          JsonEsc(slot) + "\",\"error\":\"" +
                          JsonEsc(gateErr) +
                          "\",\"hint\":\"fix the syntax and retry live:1, or "
                          "drop live for a full reload\"}";
                    return false;
                }
            }
```

이후 기존 스냅샷→`WriteTextFile`은 그대로 두고, 리로드 호출부만 분기 교체:

```cpp
            // 같은 슬롯: live 요청이면 라이브 재평가, 아니면 동기 리로드(폐곡선).
            // 라이브 성공엔 리본 gen을 그대로 실는다(스냅샷·기록은 게이트 통과
            // 후에도 했다 — 이후 낙하해도 파일=진실원 회복).
            bool reloadOk = true;
            std::string liveErr;
            if (liveCapable) {
                std::string patchErr;
                if (host_->PatchEval(source, &patchErr)) {
                    std::printf("[script] live patch: gen %d\n", gen);
                    std::fflush(stdout);
                    out = "{\"ok\":true,\"live\":true,\"slot\":\"" +
                          JsonEsc(slot) + "\",\"gen\":" + std::to_string(gen) +
                          "}";
                    return true;
                }
                // 런타임 예외 → 자동 풀 리로드 낙하(스펙 §4 흐름 4). 파일엔
                // 이미 새 원문이 기록됐다(파일=진실원 회복 경로). 폐곡선:
                // 에러를 응답에 실어 돌려보낸다.
                liveErr = patchErr;
                reloadOk = ReloadNow();
            } else {
                reloadOk = (scriptPath_ == path) ? ReloadNow()
                                                 : SwitchToSlot(slot);
            }
            if (!reloadOk) {
                // 기존 경로와 동일한 에러 응답(live 필드만 추가 — additive).
                out = "{\"ok\":false,\"live\":" +
                      std::string(liveCapable ? "true" : "false") +
                      ",\"slot\":\"" + JsonEsc(slot) +
                      "\",\"gen\":" + std::to_string(gen) + ",\"error\":\"" +
                      JsonEsc(host_->LastError()) +
                      "\",\"hint\":\"call the api tool for the function list\"}";
                return false;
            }
            // 라이브 재평가 실패 후 낙하 성공 — 앱은 살아 있고(위젯 스냅샷+
            // onSaveState 수송) 에러는 폐곡선으로 노출된다(스펙 §4 흐름 4).
            if (liveCapable) {
                out = "{\"ok\":true,\"live\":false,\"slot\":\"" +
                      JsonEsc(slot) + "\",\"gen\":" + std::to_string(gen) +
                      ",\"error\":\"" + JsonEsc(liveErr) +
                      "\",\"note\":\"recovered by full reload\"}";
                return true;
            }
            out = "{\"ok\":true,\"live\":false,\"slot\":\"" + JsonEsc(slot) +
                  "\",\"gen\":" + std::to_string(gen) + "}";
            return true;
```

기존 블록의 중간부(`const bool reloadOk = ...`부터 마지막 `return true;`)를 위 코드로 교체한다 — 성공 응답에 `live:false`가 추가되고 나머지 필드는 불변.

- [ ] **Step 3: kApiCatalog에 패치 계약 교육 문장**

`kApiCatalog`(:776) — `"state"` 필드 뒤에 신규 필드 삽입(쉼표 연결 주의):

```cpp
        "\"patch\":\"작은 수정(콜백 몸통 교체)은 도구 set_script live:1 — 컨텍스트가 살아 있고 위젯·타이머·글로벌 상태·의미 커서 선언이 보존된다. 패치 안전 형태: top-level은 function 정의만 (top-level const/let 재선언은 에러 → 풀 리로드 낙하); 위젯·타이머 생성은 onCreate에서 — 패치 평가 중 생성은 bad_patch 에러. 타이머 간격·위젯 구조 변경은 live 불가 → live:0 풀 리로드\","
```

- [ ] **Step 4: 폰 프리앰블 라우팅 1문장**

`JKLlmEngine.cpp` `kLlmTurnPreamble`(:105-119) — "set_script로 스크립트를 쓴다" 문장 뒤에 추가:

```cpp
    "이미 만든 워크숍 앱의 작은 수정은 set_script에 live:1로 "
    "라이브 패치한다(앱이 계속 살아 있다); 위젯 추가·삭제 같은 구조 변경은 "
    "live 없이 다시 쓴다. "
```

- [ ] **Step 5: jk.d.ts v7 — 패치 안전 형태 계약**

헤더(:1-8) 버전 주석에 추가:

```
// v7: 라이브 패치 계약 (set_script live:1, docs/67 단 1 리파인 3호 — 이 파일은
// 도구 표면을 다루지 않으므로 작성 규약만): top-level은 function 정의만 권장.
```

파일 말미(라이프사이클 주석 블록, :312 부근) 뒤에 섹션 추가:

```
// ---------------------------------------------------------------------------
// 라이브 패치 작성 규약 (jk.d.ts v7 — set_script live:1)
// 라이브 패치는 컨텍스트를 죽이지 않고 top-level 정의만 재평가한다.
//  - top-level에는 function 정의만 권장 — top-level const/let 재선언은
//    재평가 시 SyntaxError로 풀 리로드 낙하한다.
//  - 위젯(create*)·타이머(setInterval) 생성은 onCreate에서 — 패치 평가 중
//    생성류 호출은 TypeError(bad_patch)로 막힌다.
//  - 타이머가 호출하는 전역 콜백은 패치 후 새 정의로 해석된다(전역 객체
//    조회). 클로저가 캡처한 지역 상태는 구 코드 그대로다(stale closure 수용).
//  - 타이머 간격·위젯 구조 변경은 라이브 불가 — live 없이 set_script(풀
//    리로드).
```

(additive-only — 기존 선언 무수정.)

- [ ] **Step 6: 빌드 + 유닛 프로브 회귀**

```bash
cd engine/build
PATH=/c/msys64/ucrt64/bin cmake --build . --target jkapp_script jkserver workshop_slot_probe
./workshop_slot_probe.exe
```

Expected: 빌드 0 에러, 유닛 프로브 ALL PASS(Host 변경 없음 — ClientScriptApp이 jkapp_script에 있으므로 이 태스크만으로 jkapp_script.dll 재빌드 필요가 채워진다. **워크숍 재팩은 Task 3에서 세트로**).

- [ ] **Step 7: 커밋**

```bash
git add engine/include/apps/ClientScriptApp.h engine/src/agent/JKLlmEngine.cpp engine/scripts/jk.d.ts
git commit -m "feat(workshop): set_script live:1 라이브 패치 분기 + 폐곡선 교육 표면 (카탈로그·프리앰블·jk.d.ts v7)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 3: e2e 프로브 + 공식 런 ×2 + 회귀 + 라이브 배포(재빌드·재팩·재기동)

**Files:**
- Create: `engine/tools/probes/probe_workshop_livepatch.ps1` (probe_workshop.ps1 하니스 복제 기반)
- Rebuild+Repack: jkapp_script.dll → `pack_workshop.ps1` (docs/60 §10.1 세트)

**Interfaces:**
- Consumes: Task 2의 set_script live 응답 계약(위), 슬롯 격리 관용구(probe-ws), agentctl 도구 루프(probe_workshop.ps1 기존 관용구).
- Produces: 공식 런 기록(문서에 런 아이디·체크 수 기록) + 라이브 배포 완료 상태.

- [ ] **Step 1: e2e 프로브 작성**

`probe_workshop.ps1`을 복제해 `probe_workshop_livepatch.ps1`로 시작 — 기존 하니스(클라이언트 기동·agentctl 도구 호출·로그 단정·USER FILE 가드·**probe-ws 슬롯 격리**·finally 포인터 원복)를 그대로 쓰고 시나리오만 교체한다. PS5.1 ASCII-only 규칙 유지(스크립트 원문의 한글은 이 프로브에서 쓰지 않는다 — 위젯 텍스트는 ASCII).

시나리오(스펙 §6 게이트 ①-④, 각 체크는 agentctl 응답 JSON + 클라이언트 로그 단정):

1. **라이브 패치 성공(타이머 카운터 생존)** — set_script(live:1 아님)로 시드 스크립트:

```js
globalThis.n = 0;
setInterval(function(){ n = n + 1; log('n=' + n); }, 250);
function onClick(id){ setText(id, 'count=' + n); }
var b = createButton({x:10,y:10,w:140,h:26}, 'c0');
```

   타이머 구동 확인(로그 `n=1` 이상) → set_script(live:1)로:

```js
function onClick(id){ setText(id, 'patched:' + n); }
```

   → 응답 단정: `"ok":true,"live":true` + 클라이언트 로그 `[script] live patch: gen` 단정 → 로그에서 `n=` 카운터가 **리셋 없이 계속 상승**하는지 단정(패치 전 값 > 패치 후 최초 관측값 — 상태 생존 실측, 스펙 §6 ①).
2. **문법 실패 무손상** — set_script(live:1) `function onClick(id { ... }` → `"ok":false,"live":true` + 에러 에코 + 화면 기존 라벨 유지(로그/다음 클릭으로 구 동작 확인). 스펙 §6 ②.
3. **위젯 생성 위반** — set_script(live:1) `createButton(...);` → 에러 응답에 `live patch` 문구 + 이후 클릭이 여전히 구 동작(위젯 수 불변). 스펙 §6 ③.
4. **런타임 예외 낙하** — set_script(live:1) 정의가 패치 시점에 예외를 던지는 스크립트:

```js
function onClick(id){ setText(id, 'ok'); }
throw new Error('patch-time boom');
```

   → 응답 단정: `"ok":true,"live":false` + `error` 노출(에러 에코) + 이후 상태 읽기로 앱이 풀 리로드로 살아났는지 확인. 스펙 §6 ④.
5. **폰 경로 회귀** — 브리지 WS 릴레이로 set_script(live:1) 1회 전송 성공(브리지는 generic 릴레이라 무수정 통과 — probe_workshop.ps1의 WS 릴레이 프레임 관용구 참조). 스펙 §6 ⑤.

- [ ] **Step 2: 로컬 빌드·재팩·재기동 (라이브 배포 세트)**

```bash
cd engine/build
PATH=/c/msys64/ucrt64/bin cmake --build . --target jkapp_script jkserver
```

워크숍 재팩(docs/60 §10.1 — JKScriptHost 변경은 jkapp_script.dll 재빌드+재팩이 세트):

```bash
powershell -NoProfile -ExecutionPolicy Bypass -File engine/scripts/apps/workshop/pack_workshop.ps1
```

(패커 실제 경로/인자는 `engine/tools/`의 기존 재팩 커밋 로그·docs/60 §10.1을 따른다 — 실행 중인 workshop.jkx를 물고 있으면 IOException이니 **재팩 전 워크숍 창 닫기**.)

데스크톱 서버 재기동 → 워크숍 재기동(토큰 불변 — 서버 재기동 후 jkagentd·브리지 자동 재접속 실측 완료, docs/59 §11).

- [ ] **Step 3: 공식 런 ×2**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File engine\tools\probes\probe_workshop_livepatch.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File engine\tools\probes\probe_workshop_livepatch.ps1
```

Expected: ×2 ALL PASS. 실패 시 systematic-debugging으로 원인 봉합 후 재런(재런 자체가 공식 기록 — 로그 스태일 규칙 주의: [[jkwindow_server_testing]] 런타임 전 로그 새로움 확인).

- [ ] **Step 4: 회귀 전수 ×2**

probe_workshop / probe_workshop_cursor / probe_workshop_canvas / probe_conquest_workshop / probe_app_tools — 기존 ×2 원칙 그대로.

Expected: ALL PASS. probe_workshop은 7 rows 도구 목록에서 set_script 스키마 변화(live 인자 추가)가 additive인지 함께 눈확인.

- [ ] **Step 5: 커밋**

```bash
git add engine/tools/probes/probe_workshop_livepatch.ps1
git commit -m "test(workshop): 라이브 패치 e2e 프로브 — 상태 생존·무손상·bad_patch·낙하 실측 (docs/67 단 1 리파인 3호)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

(재빌드 산출물·repack 결과물은 레포 추적 파일이 아니면 커밋하지 않는다 — 빌드 디렉토리는 .gitignore 관용구.)

---

### Task 4: 문서 as-built + 스펙 상태 갱신 + 커밋

**Files:**
- Modify: `docs/67_workshop_vision.md` (§8 다음 수 — 라이브 패치 소각 기록, 단 1 리파인 3호 as-built)
- Modify: `docs/60_workshop.md` (§5 잔여 목록 갱신 — 라이브 패치 완결)
- Modify: `docs/superpowers/specs/2026-09-27-workshop-livepatch-design.md` (상태 헤더에 as-built 기록)
- Modify: 기억창고 `jkengine_next_backlog.md`·`jkengine_roadmap.md` (갱신 19)

- [ ] **Step 1: docs/67 §8 갱신** — 라이브 패치 as-built 항목 추가: 완결 날짜(2026-09-27), 스펙+플랜 포인터, 게이트 결과(체크 수·×2 ALL PASS), 커밋 해시 목록, 스펙 §7 문서화 한계 5건 그대로 이월. "잔여"를 "단 2 본편 트러스트 문"으로 갱신.
- [ ] **Step 2: docs/60 §5 잔여 소각** — "라이브 패치(사훈 1 본체)" 완결 표기 + 원장 §6 게이트 해소 문구와 스펙 포인터.
- [ ] **Step 3: 스펙 헤더에 as-built 표기** — 상태: 구현 완료(YYYY-MM-DD), 플랜 포인터, 최종 리뷰(opus) 결과, 라이브 배포 커밋.
- [ ] **Step 4: 기억창고 갱신** — `jkengine_next_backlog.md`의 "다음 후보"에서 라이브 패치 제거(완결로 이동), `jkengine_roadmap.md` 갱신 19 추가(라이브 패치 as-built + 커밋 해시), MEMORY.md 훅 문장 갱신.
- [ ] **Step 5: 최종 리뷰(opus) → 라이브 반영 확인 → 커밋**

```bash
git add docs/67_workshop_vision.md docs/60_workshop.md docs/superpowers/specs/2026-09-27-workshop-livepatch-design.md
git commit -m "docs(67,60): 라이브 패치 as-built — 단 1 리파인 3호 소각, 잔여=단 2 트러스트 문

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

잔여: 사용자 눈확인(폰 실기기 "버튼 색 바꿔 줘"류 실전 — 맨 뒤, 다른 눈확인 묶음과 함께).

---

## Self-Review 기록 (계획 작성자 — 실행 전 완료 확인)

1. **스펙 커버리지**: §3 계약(생존/교체/재호출 금지/차단/안전 형태) → T1(게이트·재평가·차단)+T2-Step3(교육). §4 흐름 1-4 → T1 Step4+T2 Step2. §5 구현 지점 3곳+계약 문서 → T1/T2. §6 게이트(①-⑤+회귀+재팩) → T3. §7 문서화 한계 → T2-Step3(카탈로그 문장)·T4-Step1. §8 다음 수 → T4. 빈칸 없음.
2. **플레이스홀더 스캔**: 없음 — 모든 코드 스텝에 실제 코드. T3-Step1의 시나리오 5는 기존 probe_workshop.ps1 관용구 참조로 구체화(관용구 자체가 레시피).
3. **타입 일관성**: `CompileGate(source, error*)`·`PatchEval(source, error*)`·`IsPatching()` — T1 정의, T2 소비 이름 일치. 유닛 체크 f1-f26 이름·예상값 상호 일치. set_script 응답 필드(ok/live/slot/gen/error/note) T2-Step2와 T3-Step1 시나리오 단정 일치.