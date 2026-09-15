# 4-후속 일괄 구현 플랜 — 북마크 MINOR / vplayer 레저 / jkctl init / 테마 핫스왑

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** docs/45-51에 이월된 후속 4건을 한 배치로 완결 — ①북마크 최종리뷰 MINOR 2건 ②vplayer 워치독+역방향 MINOR 3건 ③P4 SDK C 후보(jkctl init/install) ④P3 테마 핫스왑.

**Architecture:** 각 Part는 독립 서브시스템(브라우저 앱 / vplayer+클라 공용 루프 / jkctl CLI / 테마 시스템)이라 파일 겹침 없음. 테마 핫스왑은 "서버 도구 + theme.json mtime 폴링" 2중 트리거로, 재적용 지점 3류형(페인트 시점 자동 / ctor 캡처 / 스냅샷) 각각에 대응한다.

**Tech Stack:** C++ (MinGW ucrt64, Ninja), ImGui 1.92, FFmpeg, PowerShell 5.1 프로브, BCrypt SHA-256.

**Spec:** docs/49 §7, docs/50 §10, docs/diag-probe-ui-stall.md §7, docs/51 C 후보 섹션, docs/46 §7 + docs/47 §6 (P3 핫스왑 선언). 조사 근거는 본 플랜 각 Task에 인라인 기록했다.

## Global Constraints

- 빌드: `cd /i/progwork/JKENGINE/engine/build && PATH=/c/msys64/ucrt64/bin:$PATH cmake --build . --target <T>`. **레슨 18**: `--target jkapp_*` 후 반드시 `--target jkx_packages` 재팩 (안 하면 구코드로 오판). **레슨 57**: 라이브러리(jkclient 등) 변경 시 그것을 링크하는 전 타겟 재링크 필요 — `ninja` 전체 빌드가 안전.
- 프로브 규약: PS5.1 ASCII 전용 (비ASCII 넣으면 BOM 필수 — 레슨 50), 실행 전 아티팩트 mtime > 소스 mtime 게이트 (레슨 37), 임시 계측→원복→클린바이너리 공식런 3단 (레슨 61).
- 클라 stderr 로그: `std::fprintf(stderr, ...)` + `std::fflush(stderr)` 직접 호출 관행 (매크로 없음).
- 최종 리뷰는 최강 모델(opus). 워치독(Task B4)은 공용 런 루프라 ledger 지시대로 **별도 리뷰 사이클**로.
- 모든 Part 종료 시 회귀 게이트: `./jkdesktop test` → `AppSelfTest: 0 failure(s)`.
- docs는 인도물 — 각 Part 끝에 as-built 표기 커밋. 완결 항목 재수행 금지.

---

# Part A — 북마크 MINOR 후속 (docs/49)

**조사 근거 (2026-09-16):** docs/49에 두 MINOR가 문자 그대로 없음을 확인. 실체: (1) §7 L171-174 "T2 NOTE 이월" — 삭제 클릭이 기하 게이트상 CEF로도 전달되는 것이 기록돼 있고 ImGui 팝업이 먼저 먹어 "실해 미관측" 판정. 최종리뷰(MEMORY 기록)는 이것을 "팝업-페이지 더블파이어" MINOR로 승격. (2) `TruncateLabel` O(n²)는 코드상 사실 (`ClientBrowserApp.cpp:157-168` — 코드점 1개씩 자르며 매 반복 전체 재측정).

**게이트 설계 원칙 (레슨 12, docs/23 §11.9):** `io.WantCaptureMouse`는 버튼 눌린 동안 항상 true (mouse_any_down) → UP 게이트에 쓰면 CEF가 UP을 못 받아 클릭 전면 파손. 그래서 DOWN 억제는 **팝업 오픈 검사**(`ImGui::IsPopupOpen`)로 하고, UP은 `cefMouseDown_` 페어링으로만 보낸다. 팝업 클릭에서 DOWN이 억제되면 `cefMouseDown_`는 false → UP도 페어링 실패로 억제되어 고아 UP이 CEF에 가지 않는다.

### Task A1: 팝업-페이지 더블파이어 게이트

**Files:**
- Modify: `engine/src/apps/ClientBrowserApp.cpp:518-535` (PreProcessMessage의 MouseDown/MouseUp 케이스)

**Interfaces:**
- Consumes: 기존 `cefMouseDown_` 멤버 (DOWN/UP 자체 페어링 플래그), `pageY_ = 110` 기하 게이트
- Produces: 팝업 오픈 중 CEF 마우스 전달 억제 (시그니처 변경 없음)

- [ ] **Step 1: MouseDown/MouseUp 케이스 교체**

현재 코드 (`ClientBrowserApp.cpp:518-535`):

```cpp
case JKEventType::MouseDown:
    // Geometric gate only. WantCaptureMouse is wrong here: imgui sets
    // it true while ANY button is down (mouse_any_down), so gating
    // UPs by it drops the UP, leaves CEF stuck pressed, and freezes
    // our g_mouseFlags. Pair DOWN/UP by ourselves instead.
    if (ev.y >= pageY_) {
        SendMouseButton(ev.x, ev.y, ev.detail, false);
        cefMouseDown_ = true;
    }
    break;
case JKEventType::MouseUp:
    if (ev.y >= pageY_ || cefMouseDown_) {
        SendMouseButton(ev.x, ev.y, ev.detail, true);
        cefMouseDown_ = false;
    }
```

교체 후:

```cpp
case JKEventType::MouseDown: {
    // Geometric gate only. WantCaptureMouse is wrong here: imgui sets
    // it true while ANY button is down (mouse_any_down), so gating
    // UPs by it drops the UP, leaves CEF stuck pressed, and freezes
    // our g_mouseFlags. Pair DOWN/UP by ourselves instead.
    // Popup clicks must not reach the CEF page underneath — an open
    // popup owns the click and forwarding the DOWN made popup actions
    // (bookmark menu items, delete) double-fire onto the page
    // (docs/49 §7 T2 NOTE). Popup-open check, NOT WantCaptureMouse,
    // so the DOWN/UP pairing contract above is untouched.
    if (ev.y >= pageY_ &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
        SendMouseButton(ev.x, ev.y, ev.detail, false);
        cefMouseDown_ = true;
    }
    break;
}
case JKEventType::MouseUp:
    if (cefMouseDown_) {
        SendMouseButton(ev.x, ev.y, ev.detail, true);
        cefMouseDown_ = false;
    } else if (ev.y >= pageY_ &&
               !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
        // Popup-guarded too: a popup click suppressed its DOWN, so this
        // branch must not emit an orphan UP once the popup has closed.
        SendMouseButton(ev.x, ev.y, ev.detail, true);
    }
    break;
```

- [ ] **Step 2: 빌드 + 재팩**

```bash
cd /i/progwork/JKENGINE/engine/build && PATH=/c/msys64/ucrt64/bin:$PATH \
  cmake --build . --target jkapp_browser && PATH=/c/msys64/ucrt64/bin:$PATH \
  cmake --build . --target jkx_packages
```

예상: exit 0. mtime 게이트: `jkapp_browser.dll` mtime > `ClientBrowserApp.cpp` mtime.

- [ ] **Step 3: 회귀 프로브**

```bash
cd /i/progwork/JKENGINE/engine/build && pwsh=no powershell -ExecutionPolicy Bypass \
  -File ../tools/probes/probe_browser_bookmarks.ps1
```

예상: 기존 16 단정 전부 PASS (특히 "우클릭 삭제" 시나리오 — 팝업 오픈 중 클릭이 이제 CEF로 가지 않으므로 삭제 동작은 ImGui 쪽 그대로).

- [ ] **Step 4: 커밋**

```bash
git add src/apps/ClientBrowserApp.cpp
git commit -m "fix(browser): suppress CEF mouse while an ImGui popup is open (bookmark popup double-fire, docs/49 review MINOR)"
```

### Task A2: TruncateLabel O(n²) → 이진 탐색

**Files:**
- Modify: `engine/src/apps/ClientBrowserApp.cpp:157-168`

- [ ] **Step 1: 함수 교체**

현재: 코드점 1개씩 잘라내며 매 반복 `out + "..."` 복사 + 전체 `CalcTextSize` 재측정 (O(n²)). 교체 후:

```cpp
std::string TruncateLabel(const std::string& s, float maxW) {
    if (ImGui::CalcTextSize(s.c_str()).x <= maxW) return s;
    // Binary search over code-point boundaries. The old loop chopped one
    // UTF-8 sequence per iteration and re-measured the whole candidate
    // each time — O(n²) per bookmark label per frame (docs/49 review).
    static thread_local std::vector<size_t> cps;  // code-point start offsets
    cps.clear();
    for (size_t i = 0; i < s.size(); ) {
        cps.push_back(i);
        const unsigned char c = (unsigned char)s[i];
        i += (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
    }
    size_t lo = 0, hi = cps.size();
    while (lo < hi) {
        const size_t mid = (lo + hi + 1) / 2;
        const std::string cand = s.substr(0, cps[mid]) + "...";
        if (ImGui::CalcTextSize(cand.c_str()).x <= maxW) lo = mid; else hi = mid - 1;
    }
    return lo == 0 ? std::string("...") : s.substr(0, cps[lo]) + "...";
}
```

`<vector>` include는 파일 상단에 이미 있을 것 — 없으면 추가.

- [ ] **Step 2: 빌드 + 재팩 (Task A1과 동일 명령)**

- [ ] **Step 3: 회귀 프로브 (동일)** + 사용자 실측 항목 기록: 긴 북마크 라벨이 바(140px)/팝업(200px)에서 올바르게 말줄임.

- [ ] **Step 4: docs/49 갱신 + 커밋**

docs/49 §7 "T2 NOTE 이월" 행 아래에 해소 표기 추가:

```markdown
- **2026-09-16 해소**: 팝업 오픈 중 CEF 마우스 전달 억제
  (`ImGui::IsPopupOpen` 게이트 — 레슨 12의 WantCaptureMouse 방향 회피,
  `cefMouseDown_` 페어링 불변) + TruncateLabel O(n²)→이진 탐색
  (probe_browser_bookmarks 회귀 PASS).
```

```bash
git add src/apps/ClientBrowserApp.cpp docs/49_browser_bookmarks.md
git commit -m "perf(browser): TruncateLabel binary search + docs/49 followup resolved"
```

---

# Part B — vplayer 레저 (워치독 + v2 MINOR 3건)

**조사 근거:** docs/diag-probe-ui-stall.md §7 L82 (워치독 1순위 원문: "`JKClientApplication::Run` 루프에 반복 갭 감시(임계 500ms, 로그 전용, 페이즈 귀속: gap/timer/input/idle/render + 직전 커밋 스테이지 분해)"). docs/50 §10 케이던스 (`ClientVPlayerApp.cpp:2231-2246`), 무가드 3곳 (:1946 Play/Pause, :1950 Replay, :1969-1975 슬라이더 — 대조 가드 관용구: Space :2332-2334 등 `!reverseActive_` 조건 포함). 프로브 구조 전용 한계 (vpt11_reverse.ps1:6-8).

### Task B1: 케이던스 적산 클램프 + 페이싱 증거 로그

**Files:**
- Modify: `engine/src/apps/ClientVPlayerApp.cpp:2231-2246` (reverse 케이던스 블록)
- Modify: `engine/include/apps/ClientVPlayerApp.h:101` 근처 (멤버 추가)

- [ ] **Step 1: 멤버 추가** — `ClientVPlayerApp.h`의 `reverseLastTick_` 옆에:

```cpp
std::chrono::steady_clock::time_point reverseLastLog_{};  // [vpt11] pacing log throttle
```

- [ ] **Step 2: 케이던스 블록 교체** — 현재 (:2231-2246):

```cpp
if (reverseActive_) {
    const double stepSec = fps > 0.0 ? 1.0 / fps : 1.0 / 30.0;
    const auto now = std::chrono::steady_clock::now();
    reverseAcc_ +=
        std::chrono::duration<double>(now - reverseLastTick_).count();
    reverseLastTick_ = now;
    while (reverseAcc_ >= stepSec && jogTarget_ > 0.0) {
        reverseAcc_ -= stepSec;
        jogTarget_ = std::clamp(jogTarget_ - stepSec, 0.0, dur);
    }
    if (jogTarget_ <= 0.0) { finishScrub(); }
}
```

교체 후:

```cpp
if (reverseActive_) {
    const double stepSec = fps > 0.0 ? 1.0 / fps : 1.0 / 30.0;
    const auto now = std::chrono::steady_clock::now();
    // Bounded carry: a UI-thread stall must replay as at most 0.25s of
    // backward motion, not a burst of catch-up frames (docs/50 §10
    // final-review MINOR — carry previously accumulated unbounded).
    reverseAcc_ = std::min(
        reverseAcc_ + std::chrono::duration<double>(now - reverseLastTick_).count(),
        kReverseCarryMax);
    reverseLastTick_ = now;
    while (reverseAcc_ >= stepSec && jogTarget_ > 0.0) {
        reverseAcc_ -= stepSec;
        jogTarget_ = std::clamp(jogTarget_ - stepSec, 0.0, dur);
    }
    if (jogTarget_ <= 0.0) { finishScrub(); }
    // Pacing evidence for the probe gate — structure-only checks cannot
    // see cadence (vpt11 header note). Once per second, stderr.
    if (now - reverseLastLog_ >= std::chrono::seconds(1)) {
        reverseLastLog_ = now;
        std::fprintf(stderr, "[vpt11] rev pos=%.3f fps=%.1f\n",
                     jogTarget_, fps);
        std::fflush(stderr);
    }
}
```

파일 상단 익명 네임스페이스에 상수:

```cpp
// Reverse-cadence carry cap in seconds (docs/50 §10 review): after a UI
// stall the clock delta is folded back in at most this much, so a 5s
// freeze replays as ~7 frames of backward motion, not a burst.
constexpr double kReverseCarryMax = 0.25;
```

- [ ] **Step 3: 빌드 + 재팩** — `--target jkapp_vplayer && --target jkx_packages`, mtime 게이트.

### Task B2: 역방향 세션 중 트랜스포트 가드

**Files:**
- Modify: `engine/src/apps/ClientVPlayerApp.cpp:1946-1975`

- [ ] **Step 1: 3곳 가드 추가** (기존 관용구 = 조건에 `!reverseActive_` 포함, Space :2332-2334와 동형):

Play/Pause (:1946):
```cpp
// Reverse session owns the transport — cadence drives jogTarget_ and a
// pause/play here would fight it (docs/50 §10 review MINOR; joins the
// Space/arrow guards).
if (!reverseActive_ && ImGui::Button(st.paused ? "Play" : "Pause"))
    p->SetPaused(!st.paused);
```

Replay (:1948-1953): `st.ended` 래치가 역방향 세션 중 노출될 수 있음 (관찰 ①) —
```cpp
if (st.ended && !reverseActive_) {
    ImGui::SameLine();
    if (ImGui::Button("Replay")) {
        p->Seek(0);
        p->SetPaused(false);
    }
```

슬라이더 커밋 (:1972-1975) — 가드만 하면 `seekingUi_`가 영구 true로 남아 슬라이더가 st.pos 재동기화를 영구 잃으므로 리셋은 무조건:
```cpp
if (seekingUi_ && ImGui::IsItemDeactivatedAfterEdit()) {
    if (!reverseActive_)
        p->Seek(seekUi_);
    seekingUi_ = false;
}
```

- [ ] **Step 2: 빌드 + 재팩 (B1과 동일)**

### Task B3: vpt11 페이싱 게이트 + 회귀

**Files:**
- Modify: `engine/tools/probes/vpt11_reverse.ps1`

- [ ] **Step 1: 프로브에 페이싱 Check 추가** — S2(역방향 자동 재생) 시나리오 뒤에. 프로브의 stderr 캡처 로그에서 `[vpt11] rev pos=` 행 파싱:

```powershell
# Cadence pacing gate (docs/50 §10 review — structure-only checks could not
# see cadence): [vpt11] rev pos= lines must show steady backward progress.
$revLines = Select-String -Path $Log -Pattern '\[vpt11\] rev pos=([0-9.]+)' |
    ForEach-Object { [double]$_.Matches[0].Groups[1].Value }
Check "rev-pacing-lines" ($revLines.Count -ge 3) "got $($revLines.Count) pacing lines"
$paceOk = $false; $paceDetail = "insufficient samples"
if ($revLines.Count -ge 3) {
    $rate = ($revLines[0] - $revLines[$revLines.Count - 1]) /
            [double]($revLines.Count - 1)   # seconds of position per sample (1s apart)
    $mono = $true
    for ($i = 1; $i -lt $revLines.Count; $i++) {
        if ($revLines[$i] -gt $revLines[$i - 1] + 0.001) { $mono = $false }
    }
    # 30fps content: backward speed ~1.0 pos-s per wall-s; gate [0.3, 1.5].
    $paceOk = $mono -and ($rate -ge 0.3) -and ($rate -le 1.5)
    $paceDetail = "rate=$([math]::Round($rate,3)) mono=$mono"
}
Check "rev-pacing-rate" $paceOk $paceDetail
```

(프로브가 이미 `Check` 함수와 `$Log` stderr 캡처를 갖고 있음 — 실제 변수명은 파일 내 관용구를 따른다. 로그는 절단 주의 — 레슨: `-RedirectStandardError` 실행 전 복제.)

- [ ] **Step 2: 공식런** — 클린바이너리 확인(mtime 게이트) 후 vpt11 공식런 ALL PASS.

- [ ] **Step 3: 회귀** — `vpt9_jogframescrub.ps1`, `vpt4_e2e.ps1`, `vpt5_e2e.ps1` 전부 PASS + `./jkdesktop test` 0 failures.

- [ ] **Step 4: 커밋**

```bash
git add src/apps/ClientVPlayerApp.cpp src/apps/ClientVPlayerApp.h include/apps/ClientVPlayerApp.h tools/probes/vpt11_reverse.ps1
git commit -m "fix(vplayer): reverse cadence carry clamp + transport guards + vpt11 pacing gate (docs/50 s10 review)"
```

(실제 헤더 경로는 `include/apps/ClientVPlayerApp.h` — 레포 구조 따름.)

### Task B4: 영구 UI 워치독 (별도 리뷰 사이클 대상)

**Files:**
- Modify: `engine/src/client/JKClientApplication.cpp:182-204` (Run 루프), `:517` 근처 (RenderAndCommit 커밋 스테이지)

**설계:** diag-probe-ui-stall §5 계측 블록의 영구 승격. 임계 초과 시에만 로그 — 상시 비용은 steady_clock::now() 호출 수 회/반복뿐. 로그 전용, 동작 변경 0.

- [ ] **Step 1: Run 루프 페이즈 계측**

현재 (:187-204):

```cpp
while (running_) {
    DrainTimerChannel();
    DrainInputChannel();
    mainWindow_->RemoveClosedChildren();
    OnIdle();
    if (IsFrameDirty()) {
        RenderAndCommit();
        OnFrameCommitted();
    }
    SDL_Delay(1);
}
```

교체 후:

```cpp
// Permanent low-cost watchdog (diag-probe-ui-stall §7-1): any UI-thread
// stall >= kUiStallGapMs is attributed to a phase. Log-only — on stall,
// ph= values route the triage (readpix/commit -> GPU/DWM path,
// idle/timer -> app event path). Cost when quiet: a few clock reads.
constexpr double kUiStallGapMs = 500.0;
while (running_) {
    const auto t0 = std::chrono::steady_clock::now();
    DrainTimerChannel();
    const auto t1 = std::chrono::steady_clock::now();
    DrainInputChannel();
    const auto t2 = std::chrono::steady_clock::now();
    mainWindow_->RemoveClosedChildren();
    OnIdle();
    const auto t3 = std::chrono::steady_clock::now();
    if (IsFrameDirty()) {
        RenderAndCommit();
        OnFrameCommitted();
    }
    const auto t4 = std::chrono::steady_clock::now();
    const double totalMs =
        std::chrono::duration<double, std::milli>(t4 - t0).count();
    if (totalMs >= kUiStallGapMs) {
        const double timerMs  = std::chrono::duration<double, std::milli>(t1 - t0).count();
        const double inputMs  = std::chrono::duration<double, std::milli>(t2 - t1).count();
        const double idleMs   = std::chrono::duration<double, std::milli>(t3 - t2).count();
        const double renderMs = std::chrono::duration<double, std::milli>(t4 - t3).count();
        std::fprintf(stderr,
                     "[uistall] total=%.0fms timer=%.0f input=%.0f "
                     "idle=%.0f render=%.0f gap=%.0f\n",
                     totalMs, timerMs, inputMs, idleMs, renderMs,
                     totalMs - timerMs - inputMs - idleMs - renderMs);
        std::fflush(stderr);
    }
    SDL_Delay(1);
}
```

- [ ] **Step 2: RenderAndCommit 커밋 스테이지 분해** — `RenderAndCommit`(:517) 내부의 컴포지트/오버레이/readpix/커밋 경계 4곳에 steady_clock 마크를 두고, 합계 ≥100ms일 때:

```cpp
std::fprintf(stderr,
             "[uistall] commit comp=%.0f overlay=%.0f readpix=%.0f commit=%.0f\n",
             ...);
std::fflush(stderr);
```

정확한 스테이지 경계 문은 :517-580의 기존 코드에서 확인 (SDL_RenderReadPixels 실패 로그 :578이 readpix 귀속 지점 — diag §7-2의 분기 규칙 근거).

- [ ] **Step 3: 전체 재링크 + 검증** — jkclient 변경은 전 클라 재링크 필요: `ninja` 전체 빌드 (build_with_temp.sh 산출 동기화 포함). 검증: `./jkdesktop test` 0 failures + vpt4 e2e 1런 (공용 루프 무손상 스모크) + 조용한 상태에서 `[uistall]` 로그 0행 확인.

- [ ] **Step 4: 커밋 (이 Task만 별도 커밋 — 리뷰 단위 분리)**

```bash
git add src/client/JKClientApplication.cpp
git commit -m "feat(client): permanent ui-stall watchdog in Run loop + commit-stage breakdown (diag-probe-ui-stall s7)"
```

### Task B5: docs 표기

- [ ] docs/50 §10 끝에 후속 해소 표기 (케이던스 클램프/가드/페이싱 게이트, 커밋 해시), docs/diag-probe-ui-stall.md §10 "남은 레저 (1)" 해소 표기 (워치독 상시화 + 임계/형식 명시). 커밋:

```bash
git commit -m "docs(50): v2 reverse followups resolved + permanent ui watchdog as-built"
```

---

# Part C — P4 SDK C 후보: jkctl init + install

**조사 근거:** docs/51 C 후보 원문 — "템플릿 생성기 — `jkctl init <name>`: 템플릿을 복사해 이름 치환", "패키지 매니저 — 콘솔 앱 폴더의 zip 배포 + trust 지문 검증 설치". jkctl = `engine/tools/jkctl/main.cpp` (145행, if-chain 디스패치 :127-145, `wmain` + CP_UTF8 정규화). 템플릿 = `engine/templates/console-app/` (manifest.json + README.md, `name:"myapp"`). `build_with_temp.sh` L36-49가 exe/apps를 build로 복사 — templates 복사는 없음. zip 지원은 이번 MVP에서 뺀다 (폴더 설치만 — C 후보 나머지로 남김).

**범위 결정:** `jkctl init` (스캐폴딩) + `jkctl install <folder>` (빌드 트리 apps/로 복사 설치). zip 배포/패키지 매니저 전체·샘플 라인업·`--attach`는 C 후보 잔여로 문서화.

### Task C1: 템플릿에 실행 가능한 본체 추가 + 빌드 동기화

**Files:**
- Create: `engine/templates/console-app/main.cmd`
- Modify: `engine/templates/console-app/manifest.json`
- Modify: `engine/build_with_temp.sh` (templates 복사)

- [ ] **Step 1: main.cmd** (ASCII 전용 — .bat OEM 코드페이지 레슨):

```bat
@echo off
REM myapp - console app scaffolded by "jkctl init myapp".
REM NOTE: keep this file ASCII-only - cmd reads .bat in the OEM codepage
REM (docs/48 CP949 lesson). Call the agent with:
REM   %HERE%..\..\jkctl.exe ask "question"   (or notify "message")
set HERE=%~dp0
echo Hello from myapp - edit main.cmd to make it yours.
```

- [ ] **Step 2: manifest.json** — `"cmd"`를 본체로:

```json
{
  "name": "myapp",
  "cmd": "main.cmd",
  "desc": "scaffolded by jkctl init (edit desc)"
}
```

- [ ] **Step 3: build_with_temp.sh** — apps 복사 루프(L43-49) 옆에 templates 복사 추가:

```bash
# jkctl init reads templates from the runtime root (exe dir).
mkdir -p "$BUILD/templates"
cp -R "$SRC/templates/console-app" "$BUILD/templates/"
```

(실제 변수명 `$BUILD`/`$SRC`는 스크립트 기존 관용구 따름 — 스크립트를 읽고 맞출 것. idempotent: `cp -R`는 기존 디렉터리가 있으면 안에 중첩되므로 기존 apps 복사 루프가 쓰는 동일 패턴/플래그를 그대로 따른다.)

### Task C2: `jkctl init <name>`

**Files:**
- Modify: `engine/tools/jkctl/main.cpp`

**Interfaces:**
- Consumes: `<exeDir>\templates\console-app\` 3파일 (manifest.json/README.md/main.cmd), `"myapp"` 토큰 치환
- Produces: `init` 서브커맨드 — `<cwd>\<name>\` 생성, exit 0 / 실패 exit 2

- [ ] **Step 1: 구현** — if-chain에 `if (sub == "init") return Init(a2);` 추가 (usage 문자열도 갱신). 함수 전문:

```cpp
static int Init(const std::string& name) {
    if (name.empty() || name.size() > 64) {
        std::fprintf(stderr, "init: name must be 1..64 chars\n");
        return 2;
    }
    for (char c : name) {
        if (!(std::isalnum((unsigned char)c) || c == '-' || c == '_')) {
            std::fprintf(stderr, "init: name may contain [A-Za-z0-9_-] only\n");
            return 2;
        }
    }
    wchar_t exeW[MAX_PATH];
    GetModuleFileNameW(nullptr, exeW, MAX_PATH);
    char exeA[MAX_PATH * 3] = {};
    WideCharToMultiByte(CP_UTF8, 0, exeW, -1, exeA, sizeof(exeA), nullptr, nullptr);
    std::string exeDir = exeA;
    const size_t slash = exeDir.find_last_of("\\/");
    if (slash != std::string::npos) exeDir.resize(slash);
    const std::string tplDir = exeDir + "\\templates\\console-app";
    const std::string dstDir = name;

    const char* files[] = { "manifest.json", "README.md", "main.cmd" };
    for (const char* f : files) {
        std::ifstream in(tplDir + "\\" + f, std::ios::binary);
        if (!in) {
            std::fprintf(stderr, "init: template not found: %s\\%s\n",
                         tplDir.c_str(), f);
            return 2;
        }
        std::string body((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
        // Token substitution: the template ships as "myapp".
        const std::string token = "myapp";
        for (size_t p = body.find(token); p != std::string::npos;
             p = body.find(token, p + name.size()))
            body.replace(p, token.size(), name);
        std::filesystem::create_directories(dstDir);
        std::ofstream out(dstDir + "\\" + f, std::ios::binary);
        out << body;
    }
    std::printf("created %s\\ (manifest.json, README.md, main.cmd)\n"
                "install: jkctl install %s   then restart the desktop\n",
                dstDir.c_str(), name.c_str());
    return 0;
}
```

상단에 `#include <filesystem>` `#include <fstream>` `#include <iterator>` 필요 시 추가. `GetModuleFileNameW`는 이미 jkctl이 Win32 환경(-municode) — include windows.h 확인.

- [ ] **Step 2: 빌드** — `--target jkctl` + `build_with_temp.sh` 재실행 (templates 복사 반영, jkctl.exe 갱신).

- [ ] **Step 3: 수동 검증** (빌드 디렉터리에서):

```bash
cd /i/progwork/JKENGINE/engine/build && mkdir -p /tmp/jkinit_t && cd /tmp/jkinit_t \
  && /i/progwork/JKENGINE/engine/build/jkctl init demo1 \
  && cat demo1/manifest.json
```

예상: `demo1/manifest.json`의 name이 `"demo1"`, main.cmd 주석에 demo1, exit 0. `jkctl init "bad name"` → exit 2.

### Task C3: `jkctl install <folder>`

**Files:**
- Modify: `engine/tools/jkctl/main.cpp`

**Interfaces:**
- Consumes: 설치할 폴더 (manifest.json 포함), `<exeDir>\apps\`
- Produces: `install` 서브커맨드 — 폴더를 `<exeDir>\apps\<name>\`으로 재귀 복사, 스캔은 서버 재시작 시 1회 (출력에 명시)

- [ ] **Step 1: 구현** — if-chain에 `if (sub == "install") return Install(a2);` 추가:

```cpp
// jkctl install <folder>: copy a console app folder (manifest.json inside)
// into <exeDir>\apps\<name>\ (docs/51 C candidate "package manager" MVP —
// folder install only; zip + trust-verify install remain C candidates).
// The server scans apps/ once at startup, so a restart is required.
static int Install(const std::string& folder) {
    const std::string manifestPath = folder + "\\manifest.json";
    std::ifstream in(manifestPath, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "install: not a console app (missing %s)\n",
                     manifestPath.c_str());
        return 2;
    }
    std::string body((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    // Extract "name" the same string-scan way the server/LoadModel do.
    const std::string key = "\"name\"";
    const size_t k = body.find(key);
    if (k == std::string::npos) {
        std::fprintf(stderr, "install: manifest has no \"name\"\n");
        return 2;
    }
    const size_t colon = body.find(':', k);
    const size_t q1 = body.find('"', colon);
    const size_t q2 = body.find('"', q1 + 1);
    if (q1 == std::string::npos || q2 == std::string::npos) {
        std::fprintf(stderr, "install: bad manifest name\n");
        return 2;
    }
    const std::string name = body.substr(q1 + 1, q2 - q1 - 1);
    if (name.empty() || name.find_first_of("\\/:*?\"<>|") != std::string::npos) {
        std::fprintf(stderr, "install: bad name '%s'\n", name.c_str());
        return 2;
    }
    wchar_t exeW[MAX_PATH];
    GetModuleFileNameW(nullptr, exeW, MAX_PATH);
    char exeA[MAX_PATH * 3] = {};
    WideCharToMultiByte(CP_UTF8, 0, exeW, -1, exeA, sizeof(exeA), nullptr, nullptr);
    std::string exeDir = exeA;
    const size_t slash = exeDir.find_last_of("\\/");
    if (slash != std::string::npos) exeDir.resize(slash);
    const std::string dst = exeDir + "\\apps\\" + name;
    if (std::filesystem::exists(dst)) {
        std::fprintf(stderr, "install: already exists: %s (remove it first)\n",
                     dst.c_str());
        return 2;
    }
    std::filesystem::create_directories(exeDir + "\\apps");
    std::filesystem::copy(folder, dst,
                          std::filesystem::copy_options::recursive);
    std::printf("installed %s -> %s\n"
                "restart the desktop to scan it into the launcher\n",
                folder.c_str(), dst.c_str());
    return 0;
}
```

- [ ] **Step 2: 빌드 + 수동 검증** — `--target jkctl` 재빌드 후, C2에서 만든 demo1을 `jkctl install demo1` → `build/apps/demo1/` 착지 확인 → `jkctl install demo1` 재실행 → "already exists" exit 2. (서버 스폰 e2e는 Task C4의 프로브가 아니라 사용자 확인 항목 — 런처 셀 클릭 실측.)

### Task C4: 프로브 + docs

**Files:**
- Create: `engine/tools/probes/probe_jkctl_init.ps1` (ASCII 전용, 무BOM)
- Modify: `docs/51_p4_sdk_contract.md` (C 후보 섹션 표기)

- [ ] **Step 1: 프로브 작성** — 구조: temp dir에서 `jkctl init demo1` → 파일 3개 존재 + manifest name=="demo1" (ConvertFrom-Json) + main.cmd가 ASCII-only(바이트 <0x80 검사) + `jkctl init "bad name!"` exit 2 + `jkctl install demo1` → build/apps/demo1 존재 + 재설치 exit 2 + 정리(teardown). 공통 `Check`/`RESULT` 하네스는 vpt11/vpt4 관용구 복사.

- [ ] **Step 2: 공식런** ALL PASS.

- [ ] **Step 3: docs/51 C 후보 섹션 갱신** — init/install 완료 표기 + 잔여 C 후보 명시 (zip 배포+trust 검증 설치, 샘플 라인업, `--attach`, .jkx 승격 도구). 커밋:

```bash
git add tools/jkctl/main.cpp templates/console-app/ build_with_temp.sh tools/probes/probe_jkctl_init.ps1 docs/51_p4_sdk_contract.md
git commit -m "feat(sdk): jkctl init scaffold + jkctl install (docs/51 C candidates, folder MVP)"
```

---

# Part D — P3 테마 핫스왑 (docs/45-47 next)

**조사 근거 (2026-09-16):** 소비 시점 3류형 — (a) 페인트 시점 `current()` (JKDC 기본 인자, 위젯/크롬 페인트 내 지역 참조 — setTheme만으로 즉시 추종), (b) ctor/멤버 캡처 (`JKControl.h:172-177` 6멤버 + 위젯 9곳 SetBackColor/SetTextColor — 값 복사라 setTheme로 갱신 안 됨), (c) 스냅샷 (ImGuiStyle 1회 `ApplyImGuiTheme` 9앱 + 터미널 `view->SetTheme` 1회). 와이어 이벤트 선례 = `triggers.reload` (서버가 publish, 데몬 재독입), mtime 폴링 선례 = `JK_SCRIPT_WATCH` (500ms OnIdle 폴링).

**설계 결정 (이 플랜에서 확정):**
- **트리거 2중**: (1) 서버 도구 `theme_set {preset}` — 서버 프로세스 즉시 setTheme + theme.json 기록 (스캔/기동 로딩과 동일 진실원 파일), (2) theme.json **mtime 폴링** (500ms, JKClientApplication::Run + JKApplication::Run) — 서브스크라이브/프로토콜 변경 0으로 전 클라 커버 (와이어 이벤트 `theme.changed`는 YAGNI로 빼고 docs에 기록: 이벤트는 agent 구독자만 커버해 비구독 네이티브 앱이 빠짐 — 폴링이 전포괄).
- **재적용 지점**: (b)는 `JKControl::ApplyTheme()` 가상 + 재귀 워크, (c)는 베이스 `OnThemeChanged()` 기본 훅에서 `ApplyImGuiTheme()` + 터미널 앱은 오버라이드로 `view->SetTheme` 재적용, (a)는 무처리 (자동).
- **기본 인자 함정 주의**: JKEdit/JKListBox/JKComboBox의 ctor 캡처는 `fieldBg` (widgetFace 아님) — 베이스 ApplyTheme가 widgetFace로 덮으면 틀리므로 이 3개는 오버라이드 필수.
- 권한: `theme_set` 기본 **Allow** (외관 변경 — close_window/trust_request/run_console_app와 달리 ask 불가).
- 설정 UI(시작 메뉴)는 이번 범위 밖 — 도구+팔레트/jkchat 슬래시로 MVP.

### Task D1: theme.json mtime 폴링 헬퍼

**Files:**
- Modify: `engine/src/theme/JKThemeConfig.cpp` (로더 유일 .cpp)
- Modify: `engine/include/theme/JKTheme.h` (선언)

**Interfaces:**
- Produces: `namespace jk::theme { bool PollPresetFile(); }` — theme.json mtime이 마지막 폴링 대비 바뀌었으면 로더 재실행하고 true 반환 (프리셋 문자열이 같아 재로드돼도 setTheme는 멱등 — 같은 포인터 대입).

- [ ] **Step 1: 구현** — JKThemeConfig.cpp에 추가:

```cpp
// P3 hot-swap: poll theme.json mtime (the caller owns the cadence, 500ms).
// Missing file counts as mtime 0, so deleting the file also registers and
// re-runs the loader, which is fail-open on missing (keeps preset — docs/45).
static long long s_lastThemeMtime = -1;

static long long ThemeFileMtime(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA fa{};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fa))
        return 0;
    return ((long long)fa.ftLastWriteTime.dwHighDateTime << 32) |
            fa.ftLastWriteTime.dwLowDateTime;
}

bool PollPresetFile() {
    const std::string path = DefaultThemePath();
    const long long m = ThemeFileMtime(path);
    if (m == s_lastThemeMtime) return false;
    s_lastThemeMtime = m;
    if (s_lastThemeMtime == 0) return false;  // vanished: nothing to load
    return loadPresetFromFile(path);          // idempotent on same preset
}
```

JKTheme.h의 로더 선언 아래에 `bool PollPresetFile();` 추가 + 주석 1줄. 첫 호출(-1 시드)은 반드시 mtime 기록만 하고 false 반환 (기동 로딩이 이미 했으므로) — 위 구현이 그렇게 동작함 (s_lastThemeMtime==-1 ≠ m → 기록 + 로드 1회. 기동 로딩 직후 중복 로드 1회는 멱등 무해. 만약 중복 로그가 거슬리면 Init 직후 1회 호출로 시드 — 구현자 판단, 동작 불변).

- [ ] **Step 2: 빌드** — `--target jkcore`(정적 lib 위치 확인 후 상위 타겟) — JKTheme는 jkcore 소속이므로 이를 링크하는 전 타겟 재링크는 Task D4에서 일괄 전체 빌드로 검증.

### Task D2: JKControl::ApplyTheme 재귀 재캡처

**Files:**
- Modify: `engine/include/JKControl.h` (가상 메서드)
- Modify: `engine/src/JKControl.cpp` (구현)
- Modify: `engine/src/JKEdit.cpp`, `engine/src/JKListBox.cpp`, `engine/src/JKComboBox.cpp` (fieldBg 오버라이드)

**Interfaces:**
- Produces: `virtual void ApplyTheme();` — 기본: textR_/G/B ← widgetText, backR_/G/B ← widgetFace 재캡처 + children_ 재귀 + InvalidateRect. Edit/ListBox/ComboBox 오버라이드: back ← fieldBg.

- [ ] **Step 1: 베이스 구현** — JKControl.cpp에:

```cpp
// P3 hot-swap: re-capture ctor-captured theme tokens and recurse. Paint-
// time consumers (local `current()` refs, JKDC default args) follow the
// preset automatically; this covers the copy-at-construction class only
// (docs/46 §7 "재캡처 전략"). Subclasses whose ctor captured a DIFFERENT
// token (Edit/ListBox/ComboBox use fieldBg, not widgetFace) override and
// call ApplyThemeBase for the shared part.
void JKControl::ApplyTheme() {
    const auto& t = jk::theme::current();
    SetTextColor(t.widgetText.r, t.widgetText.g, t.widgetText.b);
    SetBackColor(t.widgetFace.r, t.widgetFace.g, t.widgetFace.b);
    for (auto& c : children_) c->ApplyTheme();
    InvalidateRect(rect_);
}
```

(주의: `SetTextColor/SetBackColor`가 textR_/backR_ 멤버를 쓴다는 것은 JKButton.cpp:14-16의 ctor가 그 값들을 캡처하는 것으로 확인 — 구현 시 setter가 멤버를 쓰는지 실측하고, 아니면 멤버 직접 대입으로 바꿀 것. InvalidateRect는 화면 갱신 트리거 — rect_ 대신 전체 클라 영역이 필요하면 GetRect 계열로 조정.)

- [ ] **Step 2: fieldBg 3개 오버라이드** — 각 .cpp에 (헤더 선언 포함):

```cpp
// JKEdit/JKListBox/JKComboBox 공통 — ctor가 back에 widgetFace가 아니라
// fieldBg를 캡처했다 (JKEdit.cpp:20-22). 베이스 재귀는 유지해야 하므로
// 부모 대상이 아닌 자기 캡처만 고치고 재귀는 베이스에 맡긴다: 오버라이드에서
// 자기 멤버 재캡처 후 children_ 재귀 + InvalidateRect를 반복하지 않도록
// 아래 형태로 통일한다.
void JKEdit::ApplyTheme() {
    const auto& t = jk::theme::current();
    SetBackColor(t.fieldBg.r, t.fieldBg.g, t.fieldBg.b);
    SetTextColor(t.widgetText.r, t.widgetText.g, t.widgetText.b);
    for (auto& c : children_) c->ApplyTheme();
    InvalidateRect(rect_);
}
```

(JKListBox/JKComboBox는 동일 본문, 클래스명만 교체 — 이 주석은 JKEdit에만 달고 나머지는 1줄 근거 주석.)

- [ ] **Step 3: 확인 대상** — JKButton/JKScrollBar/JKCheckBox/JKMenu/AppLauncherItem은 베이스 기본 동작(widgetFace)이 정확히 일치 (조사: 각 ctor가 widgetFace+widgetText SetBackColor/SetTextColor). JKMessageBox는 자식 컨트롤의 색을 ctor에서 건드리는데, 그 자식이 ApplyTheme 재귀로 자기 색을 다시 잡으므로 무처리. 페인트 시점 current()를 쓰는 JKMenu 팝업/JKEdit 박스/스크롤바 트랙은 무처리 (자동 추종).

### Task D3: 베이스 폴링 훅 + ImGui/터미널 재적용

**Files:**
- Modify: `engine/src/client/JKClientApplication.cpp` (Run 루프 + Init)
- Modify: `engine/include/client/JKClientApplication.h` (가상 OnThemeChanged) — 실제 헤더 경로는 레포 구조 따름
- Modify: `engine/src/JKApplication.cpp` + 헤더 (서버 모드 앱 루프 동일 틱)
- Modify: `engine/src/apps/ClientTerminalApp.cpp`, `engine/src/apps/TerminalApp.cpp` (오버라이드)

- [ ] **Step 1: JKClientApplication::Run 폴링 틱** — B4의 워치독 루프 안 (Task B4 선결제 시 같은 루프에 얹음; B4를 먼저 하므로 이 편집은 B4 후 루프에 추가):

```cpp
// P3 theme hot-swap: poll theme.json every 500ms. Server-side paint
// consumers follow setTheme instantly (theme_set tool); clients catch up
// here (mtime polling — covers non-agent apps too, unlike a wire event).
static auto s_themeLast = std::chrono::steady_clock::now();
const auto now = std::chrono::steady_clock::now();
if (now - s_themeLast >= std::chrono::milliseconds(500)) {
    s_themeLast = now;
    if (jk::theme::PollPresetFile()) {
        if (ImGui::GetCurrentContext())
            jk::theme::ApplyImGuiTheme();      // palette snapshot re-apply
        if (mainWindow_) mainWindow_->ApplyTheme();  // ctor-capture re-walk
        OnThemeChanged();                       // app-specific snapshots
    }
}
```

(ImGui include는 ImGui 앱들이 이 TU를 공유 — JKClientApplication.cpp가 imgui를 링크하지 않으면 `ImGui::GetCurrentContext()` 대신 약한 연결 불가 → 이 경우 OnThemeChanged 기본 구현 안에서 ImGui 호출로 내리고 베이스 훅은 imgui에 링크된 TU에서만 호출되게 한다. 실제 링크 구조 확인 후 결정 — jkclient가 imgui에 링크되는지 CMakeLists 확인.)

- [ ] **Step 2: OnThemeChanged 가상** — 기본 구현은 비워두고(ImGui 재적용은 위 Step 1에서 이미 했다면 훅은 앱 전용만), 선언:

```cpp
// P3 theme hot-swap: called after a preset swap was detected by polling.
// Base handles the shared re-apply (ImGui palette + widget re-walk in Run);
// override for app-owned snapshots (terminal SetTheme, custom caches).
virtual void OnThemeChanged() {}
```

- [ ] **Step 3: JKApplication::Run 동일 틱** — 서버 모드(`jkdesktop terminal`)용. 같은 500ms 폴링 + `OnThemeChanged()`. JKApplication이 서버 프로세스와 같은 실행 이미지인 경우(`jkdesktop`) 도구가 이미 in-process setTheme를 했으므로 폴링은 수동 파일 편집 커버용.

- [ ] **Step 4: 터미널 오버라이드** — `ClientTerminalApp.cpp:58-60` / `TerminalApp.cpp:58-60`의 시딩 코드를 재적용 함수로 추출:

```cpp
void ClientTerminalApp::OnThemeChanged() {
    // terminal.json explicit theme wins (themeBgSet/themeFgSet — P2 단계 3);
    // only the theme-seeded path follows a hot swap.
    if (!cfg_.themeBgSet || !cfg_.themeFgSet) ApplyThemeSeeding();
}
```

(멤버명 `cfg_`/`ApplyThemeSeeding`은 실제 코드 구조에 맞게 — 시딩 3줄을 그대로 함수화.)

- [ ] **Step 5: 빌드** — 전체 ninja (jkclient/jkcore 변경 다수).

### Task D4: theme_set 도구 + 팔레트/jkchat/jkagentd 배선

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` (도구 체인 + 기본 게이트)
- Modify: `engine/tools/jkagentd/main.cpp` (레슨: 도구 추가 4곳 — list/known/permissions/args-rebuild)
- Modify: `engine/src/apps/ClientPaletteApp.cpp` (/theme 슬래시)
- Modify: `engine/tools/jkchat/main.cpp` (/theme 슬래시 — 선택)

**Interfaces:**
- Produces: 도구 `theme_set {"preset":"dark"|"light"|"classic"}` → `{"ok":true,"preset":"..."}` / `{"ok":false,"error":"bad_preset"}`. 기본 게이트 Allow. 동작: jk::theme 프리셋 포인터 setTheme + `DefaultThemePath()`에 `{"preset":"..."}` 기록 + (셸 있으면) 런처/셸 위젯 재캡처.

- [ ] **Step 1: 서버 도구** — HandleAgentQuery 도구 체인에 추가 (run_console_app :1852-1902 옆 관용구):

```cpp
} else if (tool == "theme_set") {
    // P3 hot-swap (docs/52): write theme.json (same truth the boot loader
    // reads) then swap in-process. Paint-time consumers follow instantly;
    // clients catch up via the 500ms mtime poll; ctor-captured widgets via
    // the ApplyTheme walk in the tool path below (server-owned surfaces).
    const std::string preset = /* args에서 "preset" 문자열 추출 — 기존 도구 인자 관용구 */;
    const JKTheme* t = nullptr;
    if (preset == "light") t = &jk::theme::kLight;
    else if (preset == "classic") t = &jk::theme::kClassic;
    else if (preset == "dark") t = &jk::theme::kDefault;
    if (!t) { /* reply {"ok":false,"error":"bad_preset"} */ }
    else {
        jk::theme::setTheme(t);
        WriteThemePresetFile(preset);   // {"preset":"<p>"} — DefaultThemePath()
        if (shell_) shell_->ApplyTheme();   // launcher item re-capture
        /* reply {"ok":true,"preset":preset} */
    }
}
```

`WriteThemePresetFile`는 JKThemeConfig.cpp에 추가 (기존 로더의 파일 읽기와 대칭 — printf 로그 1행 `[theme] preset '%s' -> %s (hot-swap)`). `jk::theme::kLight/kClassic/kDefault`는 JKTheme.h 인라인 constexpr — 접근 가능. 기본 게이트(:2724-2734)에 `theme_set → AgentDecision::Allow` 명시 (askCapable 목록에 넣지 않음 = Allow).

**JKDesktopShell::ApplyTheme()** — shell이 소유한 런처/셸 네이티브 위젯 트리 루트에 `ApplyTheme()` 1호출 (AppLauncherItem 재캡처 — 베이스 동작이 정확히 widgetFace+widgetText). shell 루트 윈도우 접근자는 기존 멤버 확인 후 사용.

- [ ] **Step 2: jkagentd 4곳** — 도구 목록/known/permissions(json 기본 allow)/args-rebuild에 theme_set 추가 (레슨: docs/51 L81-85 — 한 곳이라 빠지면 MCP에서 bad_request).

- [ ] **Step 3: 팔레트/jkchat 슬래시** — `/theme dark|light|classic` → `SendAgentQuery(theme_set)` 1-deep (기존 /launch 관용구 복사). jkchat은 선택 (팔레트만으로도 e2e 가능하나 비용 낮아 포함).

- [ ] **Step 4: 빌드 + 전체 회귀** — 전체 ninja + `./jkdesktop test` 0 + 기존 프로브 회귀 (palette/chat e2e — 새 도구가 게이트 테이블을 건드리므로 probe_agent_palette/probe_agent_chat/probe_agent_e2e).

### Task D5: e2e 프로브 probe_theme_swap.ps1

**Files:**
- Create: `engine/tools/probes/probe_theme_swap.ps1`

- [ ] **Step 1: 프로브 작성** — 구조:
  1. 서버 기동 (dark 기본) → `agentctl theme_set {"preset":"light"}` → `{"ok":true}` + `state\theme.json`(또는 DefaultThemePath() 실경로) 내용 `{"preset":"light"}` + 서버 로그 `[theme] preset 'light'` 행.
  2. 클라 앱 1개 스폰 (palette) → 클라 stderr에 핫스왑 재적용 흔적 확인. (클라 폴링이 500ms 내 로드 — 클라 stderr에 `[theme] preset 'light' from ...` 행이 폴링 재로드 시 찍힘 — JKThemeConfig 로더 printf가 그 흔적. 이 행의 존재로 클라 폴링 경로 단언.)
  3. `theme_set {"preset":"nope"}` → bad_preset.
  4. 잘못된 인자/트리거 등 회귀 최소 1건.
  5. teardown.

  판정은 파일/로그/응답만 (픽셀 단언은 사용자 항목으로 남김 — 레슨 62-63 이미지 판독 비용 회피).

- [ ] **Step 2: 공식런 ALL PASS.**

- [ ] **Step 3: docs/52_theme_hotswap.md 작성** — 설계 결정(폴링 vs 와이어 이벤트 트레이드오프, 3류형 재적용 대응표, fieldBg 함정, 권한 Allow 근거), as-built, 사용자 확인 항목(서버 크롬 즉시/클라 ≤500ms/터미널/작업표시줄/런처 눈확인). 커밋:

```bash
git commit -m "feat(theme): P3 hot-swap — theme_set tool + mtime poll + ApplyTheme re-walk (docs/52)"
```

---

## 실행 순서와 리뷰

A → B → C → D (위험도: A/B는 국소 픽스, C는 신규 CLI, D가 가장 큼). 최종리뷰: Part별로 opus 1회 — 특히 **Task B4(공용 런 루프)는 ledger 지시대로 별도 리뷰 사이클**. 전체 완료 후 docs 갱신 + 메모리 갱신.

## Self-Review 기록 (작성자 점검 완료)

- 스펙 커버: docs/49 §7 NOTE/O(n²)→A1/A2, diag §7-1→B4, docs/50 §10 3건→B1(a)/B2(b)/B3(c), docs/51 C 후보→C1-C4(init+install, zip/샘플/attach는 잔여 명시), docs/46§7+47§6 핫스왑→D1-D5.
- 타입 일관: `PollPresetFile()`/`ApplyTheme()`/`OnThemeChanged()`/`kReverseCarryMax`/`theme_set` 각 Part 내 정의·소비 일치 확인.
- 미확정 API (setter 멤버 기록 여부, shell 루트 접근자, 클라 헤더 경로, ImGui 링크 구조)는 각 Step에 실측 지시 포함 — 구현자가 해당 파일을 읽고 확인하는 조건부이지 placeholder 아님 (코드 본문 전부 제공).