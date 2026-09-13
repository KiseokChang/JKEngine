# Phase A TUI 흡수 (lf/helix) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** lf(파일 매니저)와 helix(에디터)를 JKENGINE 터미널 위에서 확보하고, 런처에서 바로 띄우며, lf 개조(에이전트 연동)를 1개 녹인다.

**Architecture:** 터미널 앱(`jkdesktop.exe terminal`)에 `--shell`/`--cwd` CLI 오버라이드를 추가해 어떤 콘솔 앱이든 터미널 창 안에서 실행한다. 데스크탑 런처 fallback 아이콘이 `terminal:<cmd>` 형식의 appName을 스폰하도록 서버 SpawnClient에 관례 하나를 추가한다. lf는 `%APPDATA%\lf\lfrc` 설정으로 helix 실행·터미널 열기·에이전트 호출을 연결한다.

**Tech Stack:** C++ (기존 jkdesktop/jkwinserver), ConPTY, PowerShell 설치 스크립트, lf(Windows 바이너리), helix(Windows 바이너리)

**Spec:** `docs/superpowers/specs/2026-09-13-killer-app-absorption-design.md`

## Global Constraints

- Windows 전용 (ConPTY 경로). 비-Windows 빌드는 건드리지 않는다.
- 바이너리(lf/helix)는 git에 넣지 않는다 — `engine/build/`는 `.gitignore`(`build/`)로 이미 무시된다.
- 빌드: `engine/build_sdl2_jkwindow.bat`, 실행 파일 위치 `engine/build/jkdesktop.exe`, `engine/build/jkwinserver.exe`.
- 터미널 스택은 이미 완성되어 있다(docs/40~42: 클립보드/IME, 마우스 SGR, 리플로우) — **터미널 인프라를 수정하지 않는다.**
- 검증 프로브는 `engine/tools/probes/*.ps1` 패턴을 따른다.
- 이 플랜은 Win32/ConPTY 통합 작업이라 기존 셀프테스트(main.cpp) 대상이 아니다. 각 태스크의 검증은 실행 확인 + 프로브로 한다.

---

### Task 1: lf/helix 바이너리 설치 스크립트

**Files:**
- Create: `engine/scripts/install_lf_helix.ps1`
- Create (스크립트가 생성): `engine/build/apps-bin/lf/lf.exe`, `engine/build/apps-bin/helix/hx.exe` (+ `runtime/`)

**Interfaces:**
- Consumes: 없음 (최초 태스크)
- Produces: `engine/build/apps-bin/lf/lf.exe`, `engine/build/apps-bin/helix/hx.exe` — Task 2~5가 이 상대경로를 사용한다 (jkdesktop.exe의 cwd가 `engine/build`이므로 `apps-bin/lf/lf.exe`로 해석됨)

- [ ] **Step 1: 설치 스크립트 작성**

```powershell
# engine/scripts/install_lf_helix.ps1 — Phase A TUI 흡수 (docs/44).
# lf (gokcehan/lf) 최신 Windows 릴리스와 helix 최신 Windows 릴리스를
# engine/build/apps-bin 아래에 풀어둔다. 바이너리는 git에 넣지 않는다
# (build/ 는 이미 ignore). 재실행하면 최신으로 갱신된다.
$ErrorActionPreference = "Stop"

$dest = Join-Path $PSScriptRoot "..\build\apps-bin"
New-Item -ItemType Directory -Force -Path "$dest\lf", "$dest\helix" | Out-Null

function Get-LatestAsset($repo, $pattern) {
    $rel = Invoke-RestMethod "https://api.github.com/repos/$repo/releases/latest"
    $asset = $rel.assets | Where-Object { $_.name -match $pattern } | Select-Object -First 1
    if (-not $asset) { throw "no asset matching '$pattern' in $repo latest ($($rel.tag_name))" }
    Write-Host "$repo $($rel.tag_name) -> $($asset.name)"
    return $asset.browser_download_url
}

# lf: lf-windows-amd64.zip -> lf.exe
$lfZip = Join-Path $env:TEMP "lf-install.zip"
Invoke-WebRequest (Get-LatestAsset "gokcehan/lf" "lf-windows-amd64\.zip$") -OutFile $lfZip
Expand-Archive $lfZip "$dest\lf" -Force
Remove-Item $lfZip

# helix: *x86_64-windows.zip -> hx.exe + runtime/ (runtime은 exe 옆에서 자동 탐색됨)
$hxZip = Join-Path $env:TEMP "helix-install.zip"
Invoke-WebRequest (Get-LatestAsset "helix-editor/helix" "x86_64-windows\.zip$") -OutFile $hxZip
Expand-Archive $hxZip "$dest\helix" -Force
Remove-Item $hxZip

Write-Host "installed:"
Get-ChildItem "$dest\lf", "$dest\helix" | ForEach-Object { Write-Host "  $($_.FullName)" }
```

- [ ] **Step 2: 스크립트 실행**

Run: `powershell -ExecutionPolicy Bypass -File engine/scripts/install_lf_helix.ps1`
Expected: `installed:` 뒤에 `lf.exe`와 `hx.exe`(포함) 경로가 출력됨. GitHub API 실패 시 레이트리밋 — 몇 분 후 재실행.

- [ ] **Step 3: 버전 확인 (실행 검증)**

Run: `engine/build/apps-bin/lf/lf.exe -version` → `rXX` 형태 버전 출력
Run: `engine/build/apps-bin/helix/hx.exe --version` → `helix 25.x` 형태 버전 출력

- [ ] **Step 4: Commit**

```bash
git add engine/scripts/install_lf_helix.ps1
git commit -m "feat(apps): lf/helix installer script (Phase A TUI absorption)"
```

---

### Task 2: 터미널 `--shell` / `--cwd` CLI 오버라이드

**Files:**
- Modify: `engine/src/main.cpp:2693` 부근 (`runTerminal` 선언) 및 `:2776-2782` 부근 (`runTerminal` 분기)
- Modify: `engine/include/apps/TerminalApp.h` (public 멤버 추가)
- Modify: `engine/src/apps/TerminalApp.cpp:37` 부근 (`shell_ = cfg.shell;`)

**Interfaces:**
- Consumes: Task 1의 `apps-bin/lf/lf.exe` (검증에 사용)
- Produces: `jkdesktop.exe terminal [--shell <cmdline>] [--cwd <dir>]` — Task 3(런처)와 Task 4/5(lfrc)가 이 CLI를 호출한다. `TerminalApp::SetShellOverride(const std::string&)` — Task 3의 SpawnClient 관례가 CLI만 쓰므로 C++ API로는 이 태스크 내부에서만 사용.

- [ ] **Step 1: TerminalApp에 셸 오버라이드 추가**

`engine/include/apps/TerminalApp.h` — public 섹션에 추가:

```cpp
    // --shell 오버라이드 (Phase A): terminal.json의 shell 대신 이 명령줄을
    // ConPTY에 띄운다. 빈 문자열이면 config 기본값을 쓴다.
    void SetShellOverride(const std::string& shell) { shellOverride_ = shell; }

private:
    std::string shellOverride_;
```

(기존 private 멤버 블록이 있으면 그 안에 `shellOverride_`를 합친다. 헤더의 실제 배치는 기존 스타일을 따른다.)

`engine/src/apps/TerminalApp.cpp` — `OnInit()`의 `shell_ = cfg.shell;` (37행)을:

```cpp
    // --shell 오버라이드가 terminal.json 설정보다 우선 (Phase A 흡수 경로).
    shell_ = !shellOverride_.empty() ? shellOverride_ : cfg.shell;
```

- [ ] **Step 2: main.cpp에서 인자 파싱**

`engine/src/main.cpp` `runTerminal` 분기(`if (runTerminal) { ... }`)를:

```cpp
    if (runTerminal) {
        // Phase A: terminal [--shell <cmdline>] [--cwd <dir>]
        // --cwd는 PTY 스폰 전 프로세스 작업 디렉토리를 바꾼다 (lf 시작 폴더).
        // --shell은 terminal.json shell 대신 띄울 명령줄.
        std::string shellOverride;
        for (int i = 2; i < argc; ++i) {
            if (std::strcmp(argv[i], "--shell") == 0 && i + 1 < argc) {
                shellOverride = argv[++i];
            } else if (std::strcmp(argv[i], "--cwd") == 0 && i + 1 < argc) {
#ifdef _WIN32
                SetCurrentDirectoryA(argv[++i]);
#endif
            }
        }
        jk::TerminalApp app;
        if (!shellOverride.empty()) app.SetShellOverride(shellOverride);
        if (!app.Init("Terminal", 800, 500)) {
            return 1;
        }
        return app.Run();
    }
```

- [ ] **Step 3: 빌드**

Run: `cd engine && ./build_sdl2_jkwindow.bat`
Expected: 빌드 성공, 기존 경고 수와 동등.

- [ ] **Step 4: 실행 검증 (lf를 터미널에 띄우기)**

Run (bash): `cd engine/build && ./jkdesktop.exe terminal --shell apps-bin/lf/lf.exe`
Expected: 800x500 터미널 창에 lf 파일 브라우저가 뜬다. lf 내부에서 키 입력/마우스 동작 확인(스크롤, `q` 종료 → 창이 닫힘).
Run: `cd engine/build && ./jkdesktop.exe terminal --cwd apps-bin --shell apps-bin/lf/lf.exe`
Expected: lf가 `apps-bin`에서 시작.

- [ ] **Step 5: Commit**

```bash
git add engine/src/main.cpp engine/include/apps/TerminalApp.h engine/src/apps/TerminalApp.cpp
git commit -m "feat(terminal): --shell/--cwd overrides for TUI app absorption (Phase A)"
```

---

### Task 3: 런처 등록 — lf/helix 셀 + `terminal:` 스폰 관례

**Files:**
- Modify: `engine/src/desktop/JKDesktopShell.cpp:80-90` 부근 (fallback 아이콘 블록)
- Modify: `engine/src/server/JKWindowServer.cpp:2742-2760` 부근 (`SpawnClient`)

**Interfaces:**
- Consumes: Task 2의 `terminal --shell <cmdline>` CLI
- Produces: appName 관례 `terminal:<cmdline>` — 런처 fallback 셀이 이 이름을 `host_.launch`로 넘기면 서버가 `jkdesktop.exe terminal --shell <cmdline>`을 스폰한다. Task 5에서도 참조하는 관례.

- [ ] **Step 1: 런처 fallback 셀 추가**

`JKDesktopShell.cpp` `Init()`의 tetris fallback 블록(89행 근처) 뒤에:

```cpp
    if (!hasJkx("lf")) {
        LauncherIcon icon;
        icon.appName = "terminal:apps-bin/lf/lf.exe";
        launcherIcons_.push_back(icon);
    }
    if (!hasJkx("helix")) {
        LauncherIcon icon;
        icon.appName = "terminal:apps-bin/helix/hx.exe";
        launcherIcons_.push_back(icon);
    }
```

아이콘 아트는 minesweeper가 아닌 fallback 셀에 tetris 아트가 임시로 쓰인다(기존 폴백 동작). 전용 아이콘은 이후 폴리싱 — 지금 하지 않는다(YAGNI).

- [ ] **Step 2: SpawnClient에 `terminal:` 관례 추가**

`JKWindowServer::SpawnClient`의 비-jkx 분기(2755행 근처)를:

```cpp
    } else {
        // Phase A: appName "terminal:<cmdline>" — 콘솔 TUI 앱을 터미널 위에
        // 띄운다 (docs/44). 런처 fallback 셀이 이 관례를 쓴다.
        std::string name(appName);
        constexpr const char* kTermPrefix = "terminal:";
        if (name.rfind(kTermPrefix, 0) == 0) {
            SpawnProcess(clientHostExe_.c_str(),
                         std::string("terminal --shell ") + name.c_str() + strlen(kTermPrefix),
                         appName);
        } else {
            SpawnProcess(clientHostExe_.c_str(),
                         std::string("--client ") + appName, appName);
        }
    }
```

(상대경로 `apps-bin/...`는 jkdesktop.exe의 cwd가 `engine/build`라 해석된다. 써 있는 그대로: `name.c_str() + strlen(kTermPrefix)`는 접두사 뒤의 포인터를 이어붙인다.)

- [ ] **Step 3: 빌드**

Run: `cd engine && ./build_sdl2_jkwindow.bat`
Expected: 빌드 성공.

- [ ] **Step 4: 실행 검증**

Run: 서버 실행 (`cd engine/build && ./jkwinserver.exe`) 후 데스크탑에서 새 셀 2개 확인 → lf 셀 클릭 → lf 터미널 창 스폰, helix 셀 클릭 → helix 편집기 창 스폰.
Expected: 셀 2개 추가, 각각 정상 스폰. (셀 위치는 기존 그리드 랩 규칙을 따른다 — `RelayoutLauncherIcons`가 처리.)

- [ ] **Step 5: Commit**

```bash
git add engine/src/desktop/JKDesktopShell.cpp engine/src/server/JKWindowServer.cpp
git commit -m "feat(launcher): lf/helix cells via terminal:<cmdline> spawn convention (Phase A)"
```

---

### Task 4: lf 설정 — helix 실행 + 터미널 열기

**Files:**
- Create: `%APPDATA%\lf\lfrc` (사용자 머신 설정 — git 밖)
- Create: `engine/tools/probes/probe_lf_ops.ps1` (검증 프로브, docs/15 패턴)

**Interfaces:**
- Consumes: Task 2 CLI (`--shell`/`--cwd`), Task 1 바이너리 경로
- Produces: lf에서 파일 열기→helix, `T`→터미널. Task 5가 같은 파일에 `agent` 명령을 추가한다.

- [ ] **Step 1: lfrc 작성**

lf의 설정 경로는 Windows에서 `%APPDATA%\lf\lfrc`이다. 파일이 이미 있으면 아래 내용을 뒤에 덧붙인다(없으면 새로 만든다):

```
# JKENGINE Phase A — docs/44. 파일 열기 → helix, 디렉토리 열기 → 새 엔진 터미널.
set shell cmd

cmd open &{{
    if exist "%fx%\*" (
        start "" "I:\progwork\JKENGINE\engine\build\jkdesktop.exe" terminal --cwd "%fx%"
    ) else (
        "I:\progwork\JKENGINE\engine\build\apps-bin\helix\hx.exe" "%fx%"
    )
}}
map o open
cmd here &{{
    start "" "I:\progwork\JKENGINE\engine\build\jkdesktop.exe" terminal --cwd "%CD%"
}}
map T here
```

(`%fx%`는 lf가 내보내는 선택 항목 경로 환경변수, `%CD%`는 cmd의 현재 디렉토리. lf 내부에서 `cd`하면 실제 프로세스 cwd도 따라온다.)

- [ ] **Step 2: 검증 프로브 작성**

`engine/tools/probes/probe_lf_ops.ps1`:

```powershell
# probe_lf_ops.ps1 — lf 설정 존재 확인 (docs/44). 설정은 %APPDATA%\lf\lfrc에 산다.
$rc = Join-Path $env:APPDATA "lf\lfrc"
if (-not (Test-Path $rc)) { Write-Host "FAIL: $rc missing"; exit 1 }
$lines = Get-Content $rc
foreach ($want in @("cmd open", "cmd here", "map o open", "map T here")) {
    if (-not ($lines | Where-Object { $_ -match [regex]::Escape($want) })) {
        Write-Host "FAIL: lfrc missing '$want'"; exit 1
    }
}
Write-Host "PASS: lfrc has open/here commands"
```

- [ ] **Step 3: 프로브 + 수동 검증**

Run: `powershell -ExecutionPolicy Bypass -File engine/tools/probes/probe_lf_ops.ps1`
Expected: `PASS: lfrc has open/here commands`

수동: lf에서 파일 위 `o` → helix가 파일을 열어줌. 디렉토리 위 `o` → 새 엔진 터미널이 그 폴더에서 시작. `T` → 현재 폴더에 터미널.

- [ ] **Step 4: Commit**

```bash
git add engine/tools/probes/probe_lf_ops.ps1
git commit -m "feat(apps): lf config wiring — hx open, engine terminal spawn (Phase A)"
```

---

### Task 5: 개조 실험 — lf `agent` 명령 + docs/44 as-built 문서

**Files:**
- Modify: `%APPDATA%\lf\lfrc` (Task 4에서 만든 파일 끝에 추가)
- Create: `docs/44_phase_a_tui.md`
- Modify: `engine/tools/probes/probe_lf_ops.ps1` (agent 항목 추가)

**Interfaces:**
- Consumes: Task 4의 lfrc, jkchat이 쓰는 LLM CLI 관례 (`ollama launch claude --model <m> -- <args>`, jkchat main.cpp §claude_wrapper)
- Produces: lf 내 `A` 키 → 선택 파일을 LLM CLI에 넘겨 요약. docs/44의 "C 후보 적립"/"P4 갭" 섹션 — 이후 작업들이 이곳에 적립한다.

- [ ] **Step 1: lfrc에 agent 명령 추가**

lfrc 끝에:

```
# 개조 실험: 선택 항목을 LLM CLI에 넘긴다 (docs/44 §개조).
# 모델은 사용자 백엔드 기본(glm-5.3-flash:cloud) — 바꾸려면 이 줄만 고친다.
cmd agent &{{
    ollama launch claude --model "glm-5.3-flash:cloud" -- -p "다음 파일/디렉토리를 검토하고 한국어로 요약해줘: %fx%"
}}
map A agent
```

- [ ] **Step 2: 프로브 갱신**

`probe_lf_ops.ps1`의 `@(...)` 목록에 `"cmd agent"`, `"map A agent"`를 추가.

- [ ] **Step 3: 검증**

Run: `powershell -ExecutionPolicy Bypass -File engine/tools/probes/probe_lf_ops.ps1`
Expected: `PASS` (agent 항목 포함)
수동: lf에서 파일 선택 후 `A` → 터미널에 응답 스트리밍. 실패 시 `ollama` 래퍼 에러 확인 — `-- -p` 인자 전달 형식은 jkchat main.cpp와 동일한 패턴이므로 그쪽 동작과 비교.

- [ ] **Step 4: docs/44 as-built 작성**

`docs/44_phase_a_tui.md` — 스펙(docs/superpowers/specs/2026-09-13-killer-app-absorption-design.md)의 §5.3 순서대로 as-built 기록 + 필수 섹션 2개:

```markdown
# 44 — Phase A: TUI 흡수 as-built (lf/helix)

- 날짜: 2026-09-13
- 스펙: docs/superpowers/specs/2026-09-13-killer-app-absorption-design.md

## As-built
<!-- 태스크별 실제 결과, 계획과 다른 점을 적는다. -->

## C 후보 적립
<!-- 터미널 촉감으로 불가능한 것, 코드 깊숙이 개조가 필요한 것을 적립.
     예: 전용 런처 아이콘 아트, GUI 다이얼로그 필요성 등. 증거와 함께. -->

## P4 갭
<!-- 흡수 과정에서 막힌 지점: 창 모델, 입력/IME, 클립보드, 경로, 프로세스
     접근. P4 SDK 계약의 요구사항 입력이 된다. -->
```

- [ ] **Step 5: Commit**

```bash
git add docs/44_phase_a_tui.md
git commit -m "docs(44): Phase A TUI absorption as-built + C candidate/P4 gap ledger"
```

---

## Self-Review 결과

- **스펙 커버리지**: §5.3 순서(lf→launcher→helix→연결→개조) = Task 1→3→2→4→5 실행 순서(2가 3보다 먼저 선행 — 3이 2의 CLI를 소비). 성공 기준 ① Task 2~4, ② Task 5 개조 커밋, ③ docs/44 Task 5. §6/§7 = docs/44 필수 섹션. 커버됨.
- **플레이스홀더**: 없음. lfrc/스크립트/코드 전부 실제 내용 포함.
- **타입 일관성**: `terminal:<cmdline>` 관례 표기가 Task 3 정의와 Task 5 문서 참조에서 동일. `apps-bin/lf/lf.exe` 경로가 Task 1 Produce와 Task 2/3 Consume에서 동일.
- **실행 순서 주의**: 태스크 번호와 달리 **Task 2를 Task 3보다 먼저** 실행할 것 (3이 2의 CLI를 검증에 사용).