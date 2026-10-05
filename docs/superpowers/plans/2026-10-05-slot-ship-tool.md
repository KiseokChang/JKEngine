# 슬롯 출하 도구(slot-pack) 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 워크숍 슬롯 .js → .jkx 출하 도구 (`slot-pack`) — 출하=워크숍 모드+파묻힌 SCRI 시딩 결정(스펙 §1), 개별 MANI는 사용량 자동 분석으로 좁게 선언.

**Architecture:** 정적 name→token 표 하나(JKScriptHost 소유, 표↔BoundNames 셀프테스트 핀) → 어휘 경계 분석기 `CapabilityTokensForScript` → `SlotShipManifestText` 조립 → `RunSlotPack`(main.cpp, jkx-pack 옆 배선) → 3-엔트리 팩(MANI+MODL+Scri). 엔진 소수 변경은 시딩 출처 전환 하나(JKAppModule_script 워크숍 분기 — 파묻힌 SCRI 우선).

**Tech Stack:** C++17/MinGW ninja, QuickJS 호스트(JKScriptHost), JKJkxFile 컨테이너, RunAppSelfTest 셀프테스트(main.cpp), PowerShell probe.

**Spec:** docs/superpowers/specs/2026-10-05-slot-ship-tool-design.md (스펙 §1 출하 모드 결정 = 봉인; §3 MANI 캐노니컬 8행; §7 갤러리 아웃 오브 스코프)

## Global Constraints

1. **게이트 문구 계약 불변**: `capability '<tok>' not declared in MANI` — 변경 금지(셀프테스트+d.ts+docs/76 단언).
2. **무조건 허용 3**(log/assert/assertEq)은 정적 표에서 token=nullptr — 선언 생성에 기여하지 않는다.
3. **신규 런타임 바인딩 금지** — 기존 bind 30 호출 불변; d.ts는 만질 필요 없음(BoundNames 정합 테스트가 이를 검증).
4. **오버 선언만 오타낸다(방향성 계약)** — 어휘 경계 미적중 시 언더 선언은 런타임 fail-closed가 정직하게 잡는다(스펙 §3).
5. **probe·도구는 scratch 슬롯만** — 사용자 슬롯(bang-gu/counter/fartcar/myapp) 파일 접촉 금지(probe-ws 격리 패턴). 도구는 슬롯 파일 읽기만.
6. 빌드/테스트 표준 (engine 디렉터리 기준):
   ```bash
   export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"
   ninja.exe -C build -j3; RC=$?; echo "BUILD RC=$RC"
   ./build/jkdesktop.exe test   # → "AppSelfTest: 0 failure(s)" 유지
   ```
   파이프+`tail`로 RC 마스킹 금지. **빌드는 라이브 스택이 DLL을 물면 Permission denied** — 수리 컨트롤러 승인 하에 스택 4프로세스 정지(jkdesktop server/taskbar, jkbridge) 필수.
7. 사내 전용 repo — 푸시는 원격 KiseokChang/JKEngine에만(사전 승인 완료). 외부 공유 금지.
8. 한국어 주석 문화 — 근거 문서 참조(docs/NN·스펙 절)를 주석에 남긴다.

---

### Task 1: 출하 선언 분석기 + 정적 표 (JKScriptHost)

**Files:**
- Modify: `engine/include/script/JKScriptHost.h` (공용 static 헬퍼 2 개)
- Modify: `engine/src/script/JKScriptHost.cpp` (정적 표 + 분석기 구현)
- Test: `engine/src/main.cpp` (셀프테스트 1h 블록 — 1g 블록 뒤)

**Interfaces:**
- Consumes: 없음(신규) — 다만 게이트 토큰 표(docs/76 §2)을 정적 표로 정면화.
- Produces: `jk::JKScriptHost::CapabilityTokensForScript(const std::string& source) → std::vector<std::string>` (표 순서, 중복 없음, token 비었으면 미포함) · `jk::JKScriptHost::HostBindingNames() → std::vector<std::string>` (정적 표의 30 이름 — 표↔런타임 핀용) — Task 3 소비.

- [ ] **Step 1: 헤더 — JKScriptHost.h public 절(SetEnabledCapabilities 선언 근처)**

```cpp
// 출하 선언 분석 (스펙 2026-10-05-slot-ship-tool §3 — docs/76 §9의
// "개별 MANI 좁게 선언" 이행). 슬립 소스 어휘 경계 매치로 게이트 토큰
// 사용 집합을 돌려준다 — 표 순서·중복 없음. 주석·문자열 유사 표기는
// 오버 방향 오탐만 낸다(방향성 계약: 언더는 런타임 fail-closed가 잡는다).
static std::vector<std::string> CapabilityTokensForScript(const std::string& source);
// 정적 표의 전체 바인딩 이름(30) — 셀프테스트가 시작 호스트 BoundNames와
// 대조해 표 흔들림을 핀다(표가 bind 호출과 갈라지는 재생산 방지).
static std::vector<std::string> HostBindingNames();
```

- [ ] **Step 2: 구현 — JKScriptHost.cpp, 파일 상단 script_detail 영역 근처 파일 스폰 표**

```cpp
// 게이트 토큰 정적 표 — 능력 토큰 표(docs/76 §2)의 단일 출처 사본.
// token이 nullptr이면 게이트 밖(무조건 허용 3) — 선언 생성에 기여하지
// 않는다(전역 제약 2). bind 호출 본체와 이 표가 갈라지면 셀프테스트
// 1h의 HostBindingNames↔BoundNames 핀이 찬다.
struct BindToken { const char* bind; const char* token; };
static const BindToken kCapabilityBindTokens[] = {
    {"messageBox", "widget"},   {"createButton", "widget"},
    {"createLabel", "widget"},  {"createEdit", "widget"},
    {"setText", "widget"},      {"getText", "widget"},
    {"createDialog", "widget"}, {"dialogAddLabel", "widget"},
    {"dialogAddEdit", "widget"},{"dialogAddButton", "widget"},
    {"dialogShow", "widget"},   {"dialogClose", "widget"},
    {"setInterval", "timer"},   {"clearInterval", "timer"},
    {"declareCursor", "agent"}, {"readConfig", "fs"},
    {"injectMouse", "input"},   {"injectKey", "input"},
    {"click", "input"},         {"findControl", "uiauto"},
    {"createCanvas", "canvas"}, {"canvasClear", "canvas"},
    {"canvasRect", "canvas"},   {"canvasPixel", "canvas"},
    {"canvasLine", "canvas"},   {"canvasCircle", "canvas"},
    {"canvasText", "canvas"},
    {"log", nullptr}, {"assert", nullptr}, {"assertEq", nullptr},
};

namespace {
// JS 식별자 구성 문자 — 어휘 경계 판정 (스펙 §3: [A-Za-z0-9_$])
bool IsIdentCharW(unsigned char c) {
    return std::isalnum(c) || c == '_' || c == '$';
}
bool TokenUsedInSource(const std::string& source, const std::string& name) {
    if (name.empty()) return false;
    size_t pos = source.find(name);
    while (pos != std::string::npos) {
        const size_t end = pos + name.size();
        const bool left = pos == 0 ||
            !IsIdentCharW((unsigned char)source[pos - 1]);
        const bool right = end >= source.size() ||
            !IsIdentCharW((unsigned char)source[end]);
        if (left && right) return true;
        pos = source.find(name, pos + 1);
    }
    return false;
}
} // namespace

std::vector<std::string> JKScriptHost::CapabilityTokensForScript(
    const std::string& source) {
    std::vector<std::string> tokens;   // 표 순서 유지 — 출하 MANI도 표 순
    std::vector<std::string> seen;     // 동일 토큰 1회만 (중복 없음 계약)
    for (const BindToken& entry : kCapabilityBindTokens) {
        if (!entry.token || !entry.token[0]) continue;
        if (!TokenUsedInSource(source, entry.bind)) continue;
        const std::string tok(entry.token);
        if (std::find(seen.begin(), seen.end(), tok) != seen.end()) continue;
        seen.push_back(tok);
        tokens.push_back(tok);
    }
    return tokens;
}

std::vector<std::string> JKScriptHost::HostBindingNames() {
    std::vector<std::string> names;
    for (const BindToken& entry : kCapabilityBindTokens) {
        if (entry.bind) names.push_back(entry.bind);
    }
    return names;
}
```

- [ ] **Step 3: 셀프테스트 — main.cpp 1g 블록 뒤 1h 블록**

```cpp
// 1h — 출하 선언 분석기 (스펙 slot-ship §5.1)
{
    jk::JKScriptHost host;  // 1g와 같은 시작 패턴 재용
    auto toks = jk::JKScriptHost::CapabilityTokensForScript(
        "var t=setInterval(function(){clearInterval(t);},100);"
        "var cv=createCanvas({x:0,y:0,w:10,h:10},'');"
        "readConfig('cfg.json');declareCursor('x');");
    check(toks.size() == 4 && toks[0] == "timer" && toks[1] == "agent" &&
          toks[2] == "fs" && toks[3] == "canvas",
          "1h-a 사용 집합=표 순");       // widget/input/uiauto 미사용 미선언
    check(jk::JKScriptHost::CapabilityTokensForScript(
              "mysetInterval(a,1);xsetIntervalX(b);").empty(),
          "1h-b 어휘 경계 미적중");
    check(jk::JKScriptHost::CapabilityTokensForScript(
              "setInterval(function(){},1);").size() == 1,
          "1h-b2 경계 적중");
    check(jk::JKScriptHost::CapabilityTokensForScript(
              "// setInterval 주석 — 오버 방향 오탐(문서화 계약)").size() == 1,
          "1h-c 주석 폴스포짓=오버 방향");
    check(jk::JKScriptHost::CapabilityTokensForScript(
              "log('hi');assert(true);assertEq(1,1);").empty(),
          "1h-d 무조건 3은 선언 생성 않음");
    // 표↔런타임 핀: 정적 표의 30 이름이 시작 호스트의 실제 바인딩에 전부 존재
    auto bound = host.BoundNames();   // 1g 패턴 호스트를 Start한 상태 재용
    auto table = jk::JKScriptHost::HostBindingNames();
    check(table.size() == 30, "1h-e 정적 표=30");
    bool allBound = true;
    for (const auto& n : table)
        if (std::find(bound.begin(), bound.end(), n) == bound.end())
            allBound = false;
    check(allBound, "1h-f 표↔BoundNames 전수 일치");
}
```
주의: 1g 블록의 호스트 변수 재용(전역 제약 — 1g gateWinIds 공유 ⑦ 유지 판정과 달리 이건 셀프테스트 내부) — 1g 구현 후 실제 변수명에 맞춰 재작성, 새 호스트를 만들어도 됨(변수명 불일치 컴파일러가 잡는다).

- [ ] **Step 4: 빌드(전역 제약 6 — 스택 정지 필요)+테스트**

Run: 표준 2줄 → `0 failure(s)` · "1h-" check 전부 통과
Expected: RC=0, 0 failure(s)

- [ ] **Step 5: 커밋**

```bash
git add engine/include/script/JKScriptHost.h engine/src/script/JKScriptHost.cpp engine/src/main.cpp
git commit -m "feat(slot-ship): 출하 선언 분석기 — 정적 토큰 표+어휘 경계 매치 (스펙 §3, 표↔BoundNames 핀)"
```

---

### Task 2: 시딩 출처 전환 — 파묻힌 SCRI 우선 (스펙 §1-2/§4)

**Files:**
- Create: `engine/include/apps/JKWorkshopSeed.h` (순수 함수 헤더)
- Create: `engine/src/apps/JKWorkshopSeed.cpp` (구현)
- Modify: `engine/src/apps/JKAppModule_script.cpp` (워크숍 분기 연결)
- Test: `engine/src/main.cpp` (셀프테스트 1j — 파일 시나리오)

**Interfaces:**
- Consumes: `SideFilePath(".app.js")` (JKAppModule_script.cpp 익명 네임스페이스 — 연결부에서만 씀)
- Produces: `jk::WorkshopSeedScript(externalPath, shippedScriptPath, templateText, &error) → int` (반환 계약: 0=시딩 불요, 1=shipped 시딩, 2=template 시딩, -1=오류) — Task 2 셀프테스트 소비.

- [ ] **Step 1: 헤더 — JKWorkshopSeed.h**

```cpp
// 출하 시딩 원천 판정 (스펙 2026-10-05-slot-ship-tool §1-2 — docs/74 §5
// 단 2 출하 라인). 규약: 외부 파일(workshop 진실원)이 이미 있으면 아무것도
// 하지 않는다(외부가 이긴다 — 수신 기기 진실원 존중, 스펙 §1-3). 부재 시
// 출하 팩의 파묻힌 SCRI 원문으로 시딩하고, 그것도 없으면 템플릿(현행 회귀).
// 반환: 0=시딩 불요, 1=shipped 시딩, 2=template 시딩, -1=오류(error 채움).
int WorkshopSeedScript(const std::string& externalPath,
                       const std::string& shippedScriptPath,
                       const std::string& templateText,
                       std::string& error);
```

- [ ] **Step 2: 구현 — JKWorkshopSeed.cpp (ReadTextFile/WriteTextFile/EnsureParentDirs는 JKAppModule_script.cpp 현 함수들과 동일 로직 — 기존 static들을 이 .cpp로 재조립하지 말고, 파일 접근 3 함수를 로컬 정적으로 복사 구현해도 무방. 중복 최소화 선호 — 기존 헬퍼가 다른 TU 소속이라 그대로 재용이 불가하면 로컬 복사+주석에 "원본 JKAppModule_script.cpp" 명기)**

```cpp
#include "JKWorkshopSeed.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace jk {
// 파일 접근은 3 함수만 — 원본과 동일 로직(읽기 실패=빈, 쓰기 전 디렉터리 생성).
static bool ReadWhole(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss; ss << f.rdbuf();
    out = ss.str();
    return true;
}
static bool WriteWhole(const std::string& path, const std::string& text) {
    std::filesystem::path p(path);
    std::error_code ec;
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path(), ec);
        if (ec) return false;
    }
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(text.data(), (std::streamsize)text.size());
    return (bool)f;
}
} // namespace jk

// (헤더 주석의 계약 그대로)
int jk::WorkshopSeedScript(const std::string& externalPath,
                           const std::string& shippedScriptPath,
                           const std::string& templateText,
                           std::string& error) {
    std::string existing;
    if (ReadWhole(externalPath, existing)) return 0;   // 외부=B진실원 — 불요
    std::string shipped;
    if (!shippedScriptPath.empty() && ReadWhole(shippedScriptPath, shipped)) {
        if (!WriteWhole(externalPath, shipped)) {
            error = "cannot seed shipped script to '" + externalPath + "'";
            return -1;
        }
        return 1;
    }
    if (!WriteWhole(externalPath, templateText)) {
        error = "cannot seed template to '" + externalPath + "'";
        return -1;
    }
    return 2;
}
```

- [ ] **Step 3: 연결 — JKAppModule_script.cpp 워크숍 분기(:166-174 현 "ReadTextFile 실패 → kTemplateScript 시딩" 블록) 교체**

```cpp
        std::string seedError;
        const int seeded = jk::WorkshopSeedScript(   // 스펙 slot-ship §4
            path, SideFilePath(".app.js"), kTemplateScript, seedError);
        if (seeded < 0) {
            std::fprintf(stderr, "[workshop] cannot seed '%s': %s\n",
                         path.c_str(), seedError.c_str());
            return 1;
        }
```
기존 `EnsureParentDirs`/`ReadTextFile` 쌍이 이 블록에서만 쓰였다면 남은 사용처가 있는지 확인하고, 쓰는 곳이 더 없으면 그대로 둔다(다른 분기 사용 가능 — 소거하지 않는다; 사악한 정적 소각 금지).

- [ ] **Step 4: 빌드+셀프테스트 1j (main.cpp — 1h 뒤; temp 디렉터리 3 시나리오)**

```cpp
// 1j — 출하 시딩 원전 전환 (스펙 slot-ship §5.3)
{
    std::string dir = (fs::temp_directory_path() / "jk_seed_test").string();
    fs::remove_all(dir); fs::create_directories(dir);
    const std::string ext = dir + "/ext.js", ship = dir + "/shi.js";
    const std::string tpl = "TEMPLATE";
    // (a) shipped 존재+외부 부재 → 외부 = shipped
    WriteWhole(ship, "SHIPPED");   // 로컬 ReadWhole/WriteWhole 헬퍼 재용
    int rc = jk::WorkshopSeedScript(ext, ship, tpl, err);
    check(rc == 1 && ReadWhole(ext, s) && s == "SHIPPED", "1j-a shipped 시딩");
    // (b) shipped 부재 → 템플릿
    fs::remove(ext); fs::remove(ship);
    rc = jk::WorkshopSeedScript(ext, ship, tpl, err);
    check(rc == 2 && ReadWhole(ext, s) && s == "TEMPLATE", "1j-b 템플릿 회귀");
    // (c) 외부 존재 → 무변 (수신 기기 진실원 존중)
    rc = jk::WorkshopSeedScript(ext, ship, tpl, err);
    check(rc == 0 && ReadWhole(ext, s) && s == "TEMPLATE", "1j-c 외부 우선");
    fs::remove_all(dir);
}
```
(ReadWhole/WriteWhole은 1j 내부에도 로컬 static로 두거나 테스트 파일 I/O 유틸이 이미 있으면 재용 — 구현자가 판단, 중복은 테스트 내부 국소면 무방.)

- [ ] **Step 5: 빌드+"0 failure(s)" → 커밋**

```bash
git add engine/include/apps/JKWorkshopSeed.h engine/src/apps/JKWorkshopSeed.cpp engine/src/apps/JKAppModule_script.cpp engine/src/main.cpp
git commit -m "feat(slot-ship): 시딩 출처 전환 — 파묻힌 SCRI 우선, 템플릿 회귀 유지 (스펙 §1-2/§4)"
```

---

### Task 3: slot-pack 서브커맨드 + 출하 MANI 조립 (스펙 §2/§3)

**Files:**
- Modify: `engine/include/JKJkxFile.h` (조립기 선언)
- Modify: `engine/src/JKJkxFile.cpp` (조립기 구현)
- Modify: `engine/src/main.cpp` (RunSlotPack + 서브커맨드 배선 :3407 근처 + jkx-list MANI 인쇄 확장)
- Test: `engine/src/main.cpp` (셀프테스트 1k — 조립기 원문+Parse)

**Interfaces:**
- Consumes: Task 1 `CapabilityTokensForScript` · `JKJkxFile::Write(path, entries)` (기존 — :117) · JkxManifest `scriptfile/capabilities` 필드(있음 — 616b8d1).
- Produces: `jk::SlotShipManifestText(slot, tokens) → std::string` (스펙 §3 캐노니컬 8행) · `jkdesktop.exe slot-pack <slot> [out]` · jkx-list가 `scriptfile=`/`capabilities=`/`watch=` 행을 인쇄(probe 몫).

- [ ] **Step 1: 조립기 — JKJkxFile.h(JkxManifest 선언 근처, jk 네임스페이스)**

```cpp
// 출하 MANI 조립 (스펙 2026-10-05-slot-ship-tool §3 — docs/76 §9 이행).
// 워크숍 슬롯 출하 팩의 매니페스트 원문 8행 캐노니컬. tokens는 분석기
// (JKScriptHost::CapabilityTokensForScript) 산출 — 컴마 조인, 표 순서
// 유지. 빈 tokens = "capabilities=" 빈값(능력 없음 — 스펙 §3, badge가
// 그대로 보여 준다). name+module 필수 파스 계약(JKJkxFile.h)을 만족.
std::string SlotShipManifestText(const std::string& slot,
                                 const std::vector<std::string>& tokens);
```

- [ ] **Step 2: 구현 — JKJkxFile.cpp (Parse/JkxManifestMerge 근처)**

```cpp
std::string SlotShipManifestText(const std::string& slot,
                                 const std::vector<std::string>& tokens) {
    std::string caps;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i) caps += ",";
        caps += tokens[i];
    }
    std::string text;
    text += "name=" + slot + "\n";
    text += "title=" + slot + "\n";
    text += "width=360\n";          // 워크숍 벤치 관습 (스펙 §3)
    text += "height=280\n";
    text += "module=jkapp_script.dll\n";
    text += "script=app.js\n";
    text += "scriptfile=state/scripts/" + slot + ".js\n";
    text += "capabilities=" + caps + "\n";
    return text;
}
```
(끝 행 뒤 개행 1개 유지 — Parse의 last-key 계약과 무충돌, workshop MANI의 "마지막 행 개행 없음" 패턴과 달리 신작 원문이므로 자유. 셀프테스트가 원문 전체를 단언.)

- [ ] **Step 3: RunSlotPack — main.cpp (RunJkxPack :677 뒤)**

```cpp
// slot-pack <slot> [out] — 워크숍 슬롯 → .jkx 출하 (스펙 slot-ship §2).
// 출하=워크숍 모드(scriptfile=)+파묻힌 SCRI 시딩(§1) — 수신 기기에서
// 게이트·배지·진실원 문화가 산다. jkctl pack(콘솔앱 zip)과는 다른 도구.
// 슬립 파일은 읽기만 한다(쓰기·이력 접촉 금지 — 스펙 §2).
static int RunSlotPack(const char* slotName, const char* outOverride) {
    std::string base;
    if (char* p = SDL_GetBasePath()) { base = p; SDL_free(p); }

    std::vector<uint8_t> script;
    const std::string slotPath =
        base + "state\\scripts\\" + slotName + ".js";
    if (!ReadWholeFile(slotPath, script)) {
        std::fprintf(stderr, "slot-pack: no slot source '%s'\n",
                     slotPath.c_str());
        return 1;
    }
    std::vector<uint8_t> dll;
    if (!ReadWholeFile(base + "jkapp_script.dll", dll)) {
        std::fprintf(stderr, "slot-pack: cannot read 'jkapp_script.dll'\n");
        return 1;
    }

    // 능력 선언 = 사용량 자동 분석 — 표 단일 출처 (스펙 §3)
    const std::string source(script.begin(), script.end());
    const std::vector<std::string> tokens =
        jk::JKScriptHost::CapabilityTokensForScript(source);
    const std::string manifest =
        jk::SlotShipManifestText(slotName, tokens);

    std::vector<std::pair<std::string, std::vector<uint8_t>>> entries;
    entries.emplace_back("manifest.txt",
                         std::vector<uint8_t>(manifest.begin(), manifest.end()));
    entries.emplace_back("jkapp_script.dll", std::move(dll));
    entries.emplace_back("app.js", std::move(script));

    CreateDirectoryA((base + "apps").c_str(), nullptr);
    const std::string outPath = (outOverride && outOverride[0])
        ? std::string(outOverride) : base + "apps\\" + slotName + ".jkx";
    if (!jk::JKJkxFile::Write(outPath, entries)) return 1;
    std::fprintf(stderr,
        "[slot-pack] 빌드 규율: 팩 직전 jkapp_script.dll이 현재 소스인지 — "
        "stale DLL은 런타임 'is not defined'로만 발현 (docs/60:334)\n");
    std::printf("packed %s (caps=%s)\n", outPath.c_str(),
                jk::JKScriptHost::CapabilityTokensForScript(
                    std::string()).empty() ? "" : "analyzed");
    return 0;
}
```
(마지막 printf는 조립 시각의 실제 선언을 인쇄한다 — 위 스케치의 자리표시는 구현자가 tokens를 써서 `caps=<joined>`로 인쇄할 것. 스케치 오류를 복사하지 않는다 — "caps=" 뒤에 실제 컴마 조인 문자열.)

- [ ] **Step 4: 배선 — main.cpp 서브커맨드 체인(jkx-pack 분기 :3407 뒤) + usage 행(:3376 근처)**

```cpp
    if (argc > 1 && std::strcmp(argv[1], "slot-pack") == 0) {
#ifdef _WIN32
        if (argc < 3 || argc > 4) {
            std::fprintf(stderr, "Usage: slot-pack <slot> [out]  (bundles state/scripts/<slot>.js + jkapp_script.dll into a workshop-mode .jkx)\n");
            return 1;
        }
        return RunSlotPack(argv[2], argc > 3 ? argv[3] : nullptr);
#else
        std::fprintf(stderr, "slot-pack is Windows-only in this prototype\n");
        return 1;
#endif
    }
```
usage: `std::printf("  slot-pack SLOT [OUT]  Ship a workshop slot as a .jkx app (capability-declared MANI)\n");`

- [ ] **Step 5: jkx-list 인쇄 확장 — RunJkxList(:789 근처, MANI 행 뒤)**

```cpp
        if (!m.scriptfile.empty()) std::printf(" scriptfile=%s", m.scriptfile.c_str());
        if (!m.capabilities.empty()) std::printf(" capabilities=%s", m.capabilities.c_str());
```
(m.scriptfile 멤버명은 구현자가 JKJkxFile.h에서 확인 — JkxManifest에 script=과 scriptfile=이 별개 멤버로 있다(Parse 분기 체인 실측). 이름이 다르면 실제 멤버명을 쓴다.)

- [ ] **Step 6: 셀프테스트 1k (main.cpp — 조립기 원문+Parse)**

```cpp
// 1k — 출하 MANI 조립기 (스펙 slot-ship §5.2)
{
    const std::string m = jk::SlotShipManifestText("bang-gu", {"timer", "canvas"});
    check(m == "name=bang-gu\ntitle=bang-gu\nwidth=360\nheight=280\n"
               "module=jkapp_script.dll\nscript=app.js\n"
               "scriptfile=state/scripts/bang-gu.js\n"
               "capabilities=timer,canvas\n", "1k-a 캐노니컬 원문");
    jk::JkxManifest mm;
    check(mm.Parse(m) && mm.scriptfile == "state/scripts/bang-gu.js" &&
          mm.capabilities == "timer,canvas", "1k-b Parse 통과+원문 재검");
    jk::JkxManifest m0;
    const std::string mZero = jk::SlotShipManifestText("x", {});
    check(m0.Parse(mZero) && m0.capabilities.empty(),
          "1k-c 빈 tokens=능력 없음(fail-closed)");
}
```

- [ ] **Step 7: 빌드+"0 failure(s)" → 커밋**

```bash
git add engine/include/JKJkxFile.h engine/src/JKJkxFile.cpp engine/src/main.cpp
git commit -m "feat(slot-ship): slot-pack 서브커맨드 — 출하 MANI 조립기+jkx-list 인쇄 확장 (스펙 §2/§3)"
```

---

### Task 4: 워크숍 템플릿 capabilities 예시 (스펙 §8 — M-2 봉합)

**Files:**
- Modify: `engine/src/apps/JKAppModule_script.cpp:100-114` (kTemplateScript — **주석만**)

**Interfaces:**
- Consumes: 없음. Produces: 시딩된 슬롯 파일의 첫 선언 경험 안내 문장.

- [ ] **Step 1: kTemplateScript 헤더 주석에 안내 행 추가 (JS 문자열 리터럴이므로 `\"` 이스케이프 유의; 코드 행은 추가하지 않는다 — BoundNames 회귀 불변)**

```
// 능력 선언: 이 슬롯이 createButton/setText 등을 쓰면 슬롯 .jkx 출하 시
// 사용량이 자동 분석돼 MANI capabilities= 로 선언된다(docs/74 —
// "capability '<tok>' not declared in MANI"가 나오면 미선언 API 호출).
```

- [ ] **Step 2: 빌드+"0 failure(s)" (kTemplateScript 셀프테스트 — 존재하면 그 어설션도 녹색 유지) → 커밋**

```bash
git add engine/src/apps/JKAppModule_script.cpp
git commit -m "feat(slot-ship): 워크숍 템플릿 능력 선언 안내 — 최종리뷰 M-2 추적 봉합 ( 스펙 §8)"
```

---

### Task 5: 라이브 probe + as-built docs/77 + 원장 갱신 + 푸시

**Files:**
- Create: `engine/tools/probes/probe_slot_ship.ps1`
- Create: `docs/77_slot_ship_tool_asbuilt.md`
- Modify: `docs/superpowers/specs/2026-10-05-slot-ship-tool-design.md`(이행 기록 1행) · 기억장치

- [ ] **Step 1: probe — probe-ws 격리 패턴 재용(PSI raw-Arguments+백슬래시 선이스케이프 — docs/55 lesson 3·docs/76 §8; scratch 슬롯만)**

1. `shipscratch.js` 원천 생성(`setInterval`+`createCanvas`+`injectMouse` 1회씩 → 선언 예상 `timer,canvas,input`)
2. `slot-pack shipscratch` → RC=0+`packed` 1행
3. `jkdesktop.exe jkx-list build/apps/shipscratch.jkx` → `entries=3`+`name=shipscratch`+`capabilities=timer,canvas,input`+`scriptfile=state/scripts/shipscratch.jx` — grep 매치를 PASS/FAIL로 인쇄
   (scriptfile 기댓값은 `shipscratch.js`다 — 위 기댓값 오타를 복사하지 않는다.)
4. 팩 파일+scratch 슬롯 원천 소각

- [ ] **Step 2: 실행 → 출력에 GATE-류 판정 4행(SHIP-PACK/SHIP-LIST/SHIP-CAPS/SHIP-CLEAN) 전부 OK 실측**

- [ ] **Step 3: as-built docs/77 — 결정(스펙 §1 루링)·도구 조립·MANI 원문·셀프테스트 맵·probe receipt·트레이드오크(§1-3 수신 기기 우선)·docs/74 §5 마지막 고리 완결 기록·잔여(갤러리 다음 문)**

- [ ] **Step 4: 기억장치 갱신 32 (jkengine_next_backlog+MEMORY.md 인덱스) + 커밋+푸시 + 최종리뷰 준비(review-package, MERGE_BASE=본 플랜 커밋)**

## Self-Review 기록 (작성자 — 플랜 자체 검증)

- 스펙 커버리지: §1(결정)=T2, §2(도구)=T3, §3(MANI)=T1+T3, §4(엔진)=T2, §5(셀프테스트 1-4)=T1(1h)/T2(1j)/T3(1k)+회귀, §6(probe)=T5, §8(템플릿)=T4. §7은 아웃 오브 스코프 — 플랜에 없음이 정확.
- 알려진 위험: ① 1g 호스트 변수 재용(Task 1 Step 3 주석) — 구현자가 실제 변수명에 맞춘다 ② RunSlotPack 스케치의 caps 인쇄 자리표시 — 주석으로 교정 지시 ③ jkx-list의 `m.scriptfile` 멤버명 실측 요구(Step 5 주석) ④ probe scriptfile 기댓값 오타(`.jx`→`.js`) — Step 1-3 교정 주석.
- 유형 정합: `CapabilityTokensForScript`(T1 선언=T3 소비) · `HostBindingNames`(T1=T1 소비) · `SlotShipManifestText`(T3=T3 소비) · `WorkshopSeedScript`(T2=T2 소비) — 전부 시그니처 일치 확인.