# vplayer v2 역방향 자동 재생 (Reverse Auto-Play) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 조그 다이얼 영역에 역방향 자동 재생("<<") 토글을 얹는다 — jogRing을 콘텐츠 fps 케이던스로 역주행(무음), 링 소진 시 기존 40ms 디바운스 키프레임 `SeekScrub` 폴백 반복, 종료는 기존 `finishScrub` 정밀 시크 계약. 스펙 §7 v2 + docs/50 §8.2 최종리뷰 경화 2건 동반 반영.

**Architecture:** v1 링 기계(`JogTo`/`JogFrame`/`SeekScrub`/`SetJog`) 위에 UI 전용 케이던스를 얹는다 — PlayerCore 신규 API 0(경화 2건은 기존 결함 픽스). UI는 `reverseActive_` 세션 플래그로 조그 세션(자동 일시정지 + `SetJog(true)`)을 열고, 60Hz 타이머 프레임마다 `jogTarget_`을 1프레임씩 깎아 기존 공유 펌프(링 히트=`JogTo`, 링 미스=디바운스 `SeekScrub`)에 흘린다. 종료(토글 오프/0 도달 자동/드래그·휠 인수)는 전부 기존 `finishScrub` 한 곳으로 수렴한다.

**Tech Stack:** C++17 (MinGW ucrt64), FFmpeg 8.0, ImGui 1.92 (jkapp_vplayer ImGui 클라 앱), SDL 렌더러, PowerShell 5.1 probe 하네스 (vpt9 계승).

**Spec:** `docs/superpowers/specs/2026-09-15-vplayer-jog-framescrub-design.md` §7 (v2) + §3/§4 (v1 기계 — 본 플랜이 전제하는 링/세션/표시 경로의 진실원) + docs/50 §8.2 (레저 + 경화 2건).

## Global Constraints

- 변경 파일: `engine/src/apps/ClientVPlayerApp.cpp` + `engine/include/apps/ClientVPlayerApp.h` (스펙 §5 단일 파일 범위) + probe/docs. PlayerCore 신규 퍼블릭 API 금지 (v2는 v1 기계 위에 얹음).
- GOP 경계 끊김(링 소진 폴백의 키프레임 팝)은 스펙 §7이 명시적으로 수용 — "부드럽게" 고치려 시도하지 말 것.
- 역방향 자동 재생은 **무음**이 구현 계약: 자동 일시정지(SDL 디바이스 pause) + `jogging` 오디오 디코드 스킵의 이중 보장을 깨지 말 것 (스펙 §3.2/§4).
- 빌드: `cd I:/progwork/JKENGINE/engine/build && PATH=/c/msys64/ucrt64/bin:$PATH cmake --build . --target jkapp_vplayer` — **레슨 18: DLL 재빌드 후 반드시 `--target jkx_packages` 재팩** (클라는 .jkx MODL을 추출해 실행). **레슨 37: 빌드 출력을 grep으로 필터하지 말 것 + dll/jkx mtime > 소스 mtime 검증.**
- 회귀 게이트: `vpt9_jogframescrub.ps1` (조그), `vpt4_e2e.ps1` (재생 e2e), `vpt5_e2e.ps1` (게이트), `jkdesktop.exe test` (0 failures). 재생 페이싱 캡 기준(vpt10 S1): 480p 28-31 Hz — 역방향 작업이 일반 재생 페이싱을 훼손하지 않는다.
- probe 규약: PS5.1 **ASCII-only** (레슨 50), Write tool 생성, 좌표는 매 시도 직전 실측(레슨 7), 프레임 번호 판정은 샷을 Read tool으로 에이전트가 직접 판독(레슨 19 — OCR 정규식 금지), 클린바이너리 공식런.
- 커밋 규약: 레포 기존 스타일 (`feat(vplayer): ...` / `fix(vplayer): ...`), 각 태스크 끝에서 커밋.

---

### Task 1: PlayerCore 최종리뷰 경화 2건 (vSeekSeq 제2 bump + audioSkipBelow 슬랙)

docs/50 §8.2 최종리뷰(opus, APPROVE) 1줄 경화 2건 — 조그 영역 재접촉(본 플랜) 시 동반 반영이 전제. 둘 다 기존 결함 픽스이며 v2 기능과 독립적으로 검증 가능.

**Files:**
- Modify: `engine/src/apps/ClientVPlayerApp.cpp` (DoSeekStages 내 2곳)

**Interfaces:**
- Consumes: 기존 `vSeekSeq` 세대 기계 (VideoLoop 게이트 루프가 dequeue 시점 세대를 캡처해 폴링마다 재비교), 기존 `audioSkipBelow` 게이트 (워커 리드 루프).
- Produces: 동작 변경 없음 (레이스 봉쇄). 이후 태스크가 전제하는 시크/조그 기계의 정합성 보장.

- [ ] **Step 1: vSeekSeq 제2 bump 삽입**

`DoSeekStages` stage (a)의 vPktQ drain 블록(약 648-657행, `while (!vPktQ.empty())` drain 뒤)을 찾아 drain 루프와 `vPktCv.notify_all()` 사이에 bump를 넣는다:

```cpp
        {
            std::lock_guard<std::mutex> lkV(vPktM);
            while (!vPktQ.empty()) {
                av_packet_unref(vPktQ.front());
                av_packet_free(&vPktQ.front());
                vPktQ.pop_front();
            }
            vPktQBytes = 0;
            // Second generation bump (final-review hardening, docs/50 §8.2):
            // the bump earlier in stage (a) only invalidates packets dequeued
            // BEFORE it. A packet dequeued in the window between that bump
            // and this drain captured the NEW generation and would sail
            // through the gate loop (wantSeek is already consumed by then) —
            // a pre-seek frame entering the just-cleared videoQ. Bump again
            // under vPktM after the drain: every packet still in flight was
            // dequeued before this bump, so its captured generation is now
            // stale and the next gate poll aborts the hold. Nothing dequeues
            // after this point until post-seek packets arrive (the worker is
            // the only enqueuer and it is inside this seek).
            vSeekSeq.fetch_add(1, std::memory_order_relaxed);
            vPktCv.notify_all();
        }
```

(락 순서 m -> vPktM은 SeekCommon이 이미 쓰는 문서화된 순서 — 신규 inversion 없음.)

- [ ] **Step 2: audioSkipBelow -0.05 슬랙**

`DoSeekStages` Ok 경로 말미의 `audioSkipBelow = t;` (약 782행)를 다음으로 교체. 기존 주석(781-783행)은 유지하고 아래 근거를 덧붙인다:

```cpp
        audioSkipBelow = t - 0.05;
        // -0.05 slack = dropBeforePts' landing tolerance (final-review
        // hardening, docs/50 §8.2): the demuxer lands on a keyframe <= t, so
        // the first audio packet can sit just below t; a bare `t` gate would
        // shave it. avDelay is deliberately NOT folded in — it is a
        // display-gate shift only, the audio clock domain stays pure.
```

- [ ] **Step 3: 빌드 + mtime 게이트**

```bash
cd I:/progwork/JKENGINE/engine/build
PATH=/c/msys64/ucrt64/bin:$PATH cmake --build . --target jkapp_vplayer
PATH=/c/msys64/ucrt64/bin:$PATH cmake --build . --target jkx_packages
ls --full-time ../src/apps/ClientVPlayerApp.cpp build/jkapp_vplayer.dll apps/vplayer.jkx 2>/dev/null || ls --full-time jkapp_vplayer.dll
```

Expected: 컴파일 에러 0, `jkapp_vplayer.dll`/`vplayer.jkx` mtime > `ClientVPlayerApp.cpp` mtime. (build 트리 내 상대경로는 실제 레이아웃에 맞춰 확인 — jkx는 `build/apps/` 아래.)

- [ ] **Step 4: 시크 회귀 (vpt9 S3이 폴백 시크를 실측)**

```bash
powershell -ExecutionPolicy Bypass -File I:/progwork/JKENGINE/engine/tools/probes/vpt9_jogframescrub.ps1
```

Expected: `RESULT: ALL PASS`. 샷 판독(vpt9 S3 링 경계 폴백 + S1/S2)에서 이상 발견 시 즉시 원인 규명 — 경화가 시크 기계를 깬 신호.

- [ ] **Step 5: Commit**

```bash
cd I:/progwork/JKENGINE && git add engine/src/apps/ClientVPlayerApp.cpp
git commit -m "fix(vplayer): final-review hardening - vSeekSeq 2nd bump after vPktQ drain, audioSkipBelow -0.05 slack"
```

---

### Task 2: 역방향 세션 상태 기계 (header 필드 + finishScrub 확장 + "<<" 토글 버튼 + 인수)

세션의 열기/닫기만 — 케이던스는 Task 3. 종료 경로를 `finishScrub` 한 곳으로 수렴시키는 게 이 태스크의 핵심 설계.

**Files:**
- Modify: `engine/include/apps/ClientVPlayerApp.h` (jog 필드 뒤, 92행 근처)
- Modify: `engine/src/apps/ClientVPlayerApp.cpp` (BuildUi jog 블록: finishScrub 람다 2053행 근처, 스텝 버튼 2034-2044행, 드래그 진입 2061-2077행, 휠 진입 2110-2126행)

**Interfaces:**
- Consumes: `p->SetJog(bool)`, `p->SetPaused(bool)`, `p->Seek(double)` (PlayerCore, 기존), `jogTarget_`/`jogLastSent_`/`jogLastSeek_`/`jogWasPlaying_` (기존 UI 세션 상태).
- Produces (Task 3이 소비): `bool reverseActive_` (역방향 세션 활성), `double reverseAcc_` (케이던스 소수 프레임 캐리, 초), `std::chrono::steady_clock::time_point reverseLastTick_`. 계약: `reverseActive_` 동안 조그 세션(`SetJog(true)`+자동 일시정지)이 열려 있고 `jogTarget_`이 유효하며, 종료는 반드시 `finishScrub()`을 경유한다.

- [ ] **Step 1: header 필드 추가**

`ClientVPlayerApp.h`의 `wheelLastTick_` 선언 뒤:

```cpp
    // Reverse auto-play (spec 2026-09-15 section 7 v2): the jog ring walked
    // backward at content fps by the UI cadence (Task 3); ring exhaustion
    // falls back to the debounced keyframe SeekScrub (GOP-boundary stutter
    // accepted by spec). The session opens/closes exactly like a drag/wheel
    // scrub (auto-pause + SetJog(true) = silent), so the same finishScrub
    // precision-seek contract applies on every exit.
    bool reverseActive_ = false;
    double reverseAcc_ = 0.0; // fractional-frame cadence carry (seconds)
    std::chrono::steady_clock::time_point reverseLastTick_{};
```

- [ ] **Step 2: finishScrub 확장 — 모든 종료의 수렴점**

BuildUi의 `finishScrub` 람다(약 2053행):

```cpp
        auto finishScrub = [&]() {
            const double t = fps > 0.0
                                 ? std::round(std::clamp(jogTarget_, 0.0, dur) * fps) / fps
                                 : jogTarget_;
            p->Seek(t);
            p->SetJog(false);
            if (jogWasPlaying_) p->SetPaused(false);
            // Reverse auto-play converges here too: any scrub release ends a
            // reverse session (its own toggle-off / 0-reach auto-finish, or a
            // drag/wheel takeover), so there is exactly one exit contract.
            reverseActive_ = false;
            reverseAcc_ = 0.0;
        };
```

- [ ] **Step 3: "<<" 토글 버튼 (스텝 버튼 왼쪽)**

스텝 버튼 블록(약 2034-2044행)을 교체. 현재 `SetCursorScreenPos(kmin.x - 78, c.y - 11)`에서 시작하는 "<"/">"를 왼쪽으로 38px 밀고 그 자리에 "<<"를 SameLine 체인으로 넣는다 — "<"의 화면 x좌표(793, vpt9이 클릭하는)는 불변:

```cpp
        // Translucent step buttons + reverse auto-play toggle, left of the
        // knob. "<<" (spec section 7 v2) walks the jog ring backward at
        // content fps; the shared pump below turns each new target into
        // JogTo (ring hit) or the debounced SeekScrub fallback.
        ImGui::SetCursorScreenPos(ImVec2(kmin.x - 116.0f, c.y - 11.0f));
        // 의도적 잔존 — 의미색 (P2 테마 스왑 제외): 비디오 위 반투명 오버레이
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.20f, 0.55f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.32f, 0.80f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.32f, 0.42f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.95f, 0.75f));
        if (reverseActive_)
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(0.12f, 0.24f, 0.43f, 0.85f)); // active tint
        if (ImGui::Button("<<", ImVec2(30, 22))) {
            if (reverseActive_) {
                finishScrub(); // toggle-off = the one exit contract
            } else {
                // Toggle-on: open the shared jog session. A still-open wheel
                // session (target-only, no release yet) is superseded —
                // letting both run would double-finish (two seeks).
                wheelScrubbing_ = false;
                reverseActive_ = true;
                reverseAcc_ = 0.0;
                reverseLastTick_ = std::chrono::steady_clock::now();
                jogWasPlaying_ = !st.paused && !st.ended;
                if (jogWasPlaying_) p->SetPaused(true);
                p->SetJog(true);
                jogTarget_ = st.pos;
                jogLastSent_ = -1;
                jogLastSeek_ = std::chrono::steady_clock::now();
            }
        }
        if (reverseActive_) ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("<", ImVec2(30, 22))) stepFrame(-1);
        ImGui::SameLine();
        if (ImGui::Button(">", ImVec2(30, 22))) stepFrame(+1);
        ImGui::PopStyleColor(4);
```

- [ ] **Step 4: 드래그/휠 진입의 인수 (takeover)**

드래그 진입 블록(`const bool activated = ImGui::IsItemActivated();` 이후, 약 2062-2077행)을 교체 — 세션 플래그를 먼저 정리하고, 기존 세션이 있던 인수면 `jogTarget_`을 재시드하지 않는다(세션 타깃 보존), `jogWasPlaying_`도 최초 세션이 캡처한 값 유지:

```cpp
        if (activated) {
            // Drag start: freeze the clock (auto-pause) and skip audio decode
            // while scrubbing. A drag takes over any open session (wheel
            // scrub or reverse auto-play) — its release below becomes the
            // single finish (no stale flags, no double seek). On takeover the
            // in-flight dial target and the pre-scrub pause verdict are
            // KEPT: the first session to pause captured the truth, and the
            // target is the user's accumulated dial position.
            const bool takeover = wheelScrubbing_ || reverseActive_;
            wheelScrubbing_ = false;
            reverseActive_ = false;
            reverseAcc_ = 0.0;
            jogActive_ = true;
            if (!takeover) jogWasPlaying_ = !st.paused && !st.ended;
            if (jogWasPlaying_ && st.paused == false) p->SetPaused(true);
            p->SetJog(true);
            if (!takeover) jogTarget_ = st.pos;
            jogLastSent_ = -1;
            knobCX_ = c.x; knobCY_ = c.y;
            jogMouseX_ = io.MousePos.x; jogMouseY_ = io.MousePos.y;
            jogLastSeek_ = std::chrono::steady_clock::now();
        }
```

휠 진입 블록(약 2110-2126행)의 진입 가드도 동일 규칙 — 역방향 세션 도중 휠 틱이 오면 휠 세션이 인수한다(리버스 종료, 타깃 보존):

```cpp
        if (hot && io.MouseWheel != 0.0f) {
            if (!jogActive_ && !wheelScrubbing_) {
                // Entry mirrors the drag start: auto-pause if playing, skip
                // audio decode, seed the target from the live position. A
                // reverse session is taken over the same way (its target and
                // pause verdict are kept).
                const bool takeover = reverseActive_;
                reverseActive_ = false;
                reverseAcc_ = 0.0;
                wheelScrubbing_ = true;
                if (!takeover) jogWasPlaying_ = !st.paused && !st.ended;
                if (jogWasPlaying_ && st.paused == false) p->SetPaused(true);
                p->SetJog(true);
                if (!takeover) jogTarget_ = st.pos;
                jogLastSent_ = -1;
                jogLastSeek_ = std::chrono::steady_clock::now();
            }
            jogTarget_ = std::clamp(
                jogTarget_ + (double)io.MouseWheel * sPerRev / 16.0, 0.0, dur);
            if (!jogActive_)
                wheelLastTick_ = std::chrono::steady_clock::now();
        }
```

(주의: `if (jogWasPlaying_ && st.paused == false) p->SetPaused(true);`는 기존 `if (jogWasPlaying_) p->SetPaused(true);`와 동치이되 인수 시 이미 일시정지 상태에서의 재호출을 무해하게 만든다 — SetPaused는 자기 값이면 no-op이므로 기존 형태 유지도 허용. 어느 쪽이든 일관되게.)

- [ ] **Step 5: 빌드 + 재팩 + mtime 게이트**

Task 1 Step 3과 동일 명령. Expected: 컴파일 에러 0, mtime 역전 없음.

- [ ] **Step 6: 조그 회귀 (vpt9) — "<" 클릭 좌표 793 불변 확인 포함**

```bash
powershell -ExecutionPolicy Bypass -File I:/progwork/JKENGINE/engine/tools/probes/vpt9_jogframescrub.ps1
```

Expected: `RESULT: ALL PASS` — 특히 S5의 "<"(793,582)/">"(824,582) 클릭이 여전히 ±1F로 동작해야 한다(레이아웃 시프트 회귀). ">" 좌표가 SameLine 시프트로 어긋나면 vpt9의 ">" 클릭 x를 재측정한 값으로 갱신(레슨 7 — 샷에서 직접 측정)하고 커밋에 포함.

- [ ] **Step 7: Commit**

```bash
cd I:/progwork/JKENGINE && git add engine/include/apps/ClientVPlayerApp.h engine/src/apps/ClientVPlayerApp.cpp engine/tools/probes/vpt9_jogframescrub.ps1
git commit -m "feat(vplayer): reverse auto-play session - << toggle button + shared jog session entry/exit"
```

---

### Task 3: 역방향 케이던스 + 공유 펌프 결선 + 입력 가드

**Files:**
- Modify: `engine/src/apps/ClientVPlayerApp.cpp` (BuildUi jog 블록: 케이던스 신설, 공유 펌프 조건 2134행, knob arc 2179행, 상태행 1941행, stepFrame 가드 2028행, Space 2210행, 좌우 화살표 2218행)

**Interfaces:**
- Consumes: Task 2의 `reverseActive_`/`reverseAcc_`/`reverseLastTick_` + `finishScrub()` 수렴 계약, 기존 공유 펌프(`ringHit` → `JogTo` / 링 미스 → 40ms 디바운스 `SeekScrub`), `p->fps`.
- Produces: 없음 (기능 완결 태스크).

- [ ] **Step 1: 케이던스 블록 삽입**

휠 블록(Step 4에서 교체된 블록) 직후, 공유 펌프(`if ((jogActive_ && !activated) || ...)`) 직전에:

```cpp
        // Reverse auto-play cadence (spec section 7 v2): walk jogTarget_
        // backward one frame per content-fps period, in real time. The UI
        // frame gate is the 16 ms Timer (OnInit SetTimerInterval), so the
        // accumulator carries sub-frame remainders. fps unknown: fall back
        // to 30 fps pacing (spec section 4: pts-based dial is time-accumulated
        // either way). Each new target flows through the shared pump below —
        // ring hit = JogTo, exhaustion = debounced SeekScrub (the accepted
        // GOP-boundary stutter).
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
            if (jogTarget_ <= 0.0) {
                // Ring walked to the file start: auto-finish with the same
                // precision-seek contract as a manual toggle-off.
                finishScrub();
            }
        }
```

- [ ] **Step 2: 공유 펌프 조건에 reverse 결선**

```cpp
        if ((jogActive_ && !activated) || (wheelScrubbing_ && !jogActive_) ||
            reverseActive_) {
```

(블록 내부 무변경 — `ringHit` 판정은 `st.jogRingLo`(SnapNow 매 프레임 갱신)를 쓰므로 리버스 케이던스 타깃에 그대로 적용된다.)

- [ ] **Step 3: 표시 경로 2곳에 reverse 반영**

knob arc의 frac 선택(약 2179행):

```cpp
        const double frac = std::clamp(
            ((jogActive_ || wheelScrubbing_ || reverseActive_) ? jogTarget_
                                                               : (double)st.pos) /
                dur,
            0.0, 1.0);
```

상태행 진행 표시(약 1941행)도 동일하게 `reverseActive_`를 세션 플래그 나열에 추가:

```cpp
               (jogActive_ || wheelScrubbing_ || reverseActive_) ? jogTarget_
                                                                 : st.pos);
```

- [ ] **Step 4: 입력 가드 (역방향 세션이 transport를 소유하는 동안)**

stepFrame 람다 가드(약 2028행):

```cpp
            if (fps <= 0.0 || jogActive_ || wheelScrubbing_ || reverseActive_)
                return;
```

Space 토글(약 2210행) — 기존 조그 세션에도 Space가 통과하던 퀴크를 함께 봉쇄(자동 일시정지 중 SetPaused(false)로 세션을 깨는 결함):

```cpp
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space, false) &&
        st.dur > 0 && !st.ended && !jogActive_ && !wheelScrubbing_ &&
        !reverseActive_) {
        p->SetPaused(!st.paused);
    }
```

좌우 화살표(약 2218행):

```cpp
    if (!io.WantTextInput && st.dur > 0 && p->fps > 0.0 && !jogActive_ &&
        !wheelScrubbing_ && !reverseActive_) {
```

- [ ] **Step 5: 빌드 + 재팩 + mtime 게이트**

Task 1 Step 3 동일 명령. Expected: 컴파일 에러 0, mtime 역전 없음.

- [ ] **Step 6: 수동 스모크 최소 확인 (probe 전 셀프 점검)**

```bash
powershell -ExecutionPolicy Bypass -File I:/progwork/JKENGINE/engine/tools/probes/vpt9_jogframescrub.ps1
```

Expected: `RESULT: ALL PASS` (조그 v1 회귀 — 인수/가드 변경이 기존 세션을 깨지 않음).

- [ ] **Step 7: Commit**

```bash
cd I:/progwork/JKENGINE && git add engine/src/apps/ClientVPlayerApp.cpp
git commit -m "feat(vplayer): reverse auto-play cadence - ring walk at content fps + shared pump + input guards"
```

---

### Task 4: probe vpt11_reverse.ps1 + 전체 회귀

**Files:**
- Create: `engine/tools/probes/vpt11_reverse.ps1`
- Test: 위 전부

**Interfaces:**
- Consumes: Task 2/3가 만든 UI ("<<" 버튼, 케이던스, 인수), vpt9 하네스 레시피 (DPI-aware, layer crop, agentctl).
- Produces: vpt11 회귀 게이트 (이후 vplayer 작업의 회귀 목록에 추가).

- [ ] **Step 1: probe 작성**

vpt9 하네스를 그대로 계승(동일 Wt9 인터셉트/Refresh-Geom/Save-ServerShotFast/Click-App/Send-Wheel/Seek-Frac/Ping-Ms — vpt9에서 복사). 시나리오:

- **S0 캘리브레이션**: 기본 샷 + "<<" 버튼 존재 확인. 좌표는 새 레이아웃에서 재측정(레슨 7): knob center (888,582) 기준 `kmin.x-116` → "<<" 중심 ≈ (755,582), "<" ≈ (793,582) — 샷에서 반드시 재확인 후 클릭 좌표 확정.
- **S1 역방향 진입 + 케이던스**: 재생 중 "<<" 클릭 → 자동 일시정지(UI 클럭 동결) + 프레임 번호 감소. 500ms 간격 3샷 — 콘텐츠 30fps 기준 샷 간 ~15프레임 감소를 Read tool 판독으로 확인(레슨 19).
- **S2 링 소진 폴백**: `Seek-Frac`으로 시크(링 클리어) → 재생 2초(링 ~2초) → 일시정지 → "<<" → 5초 이상 역주행. 링(~2초) 소진 후에도 프레임 번호가 **계속 감소**(폴백 키프레임 시크 착지 후 케이던스 재개) — 스톨 없음. 팝 한 번(GOP 경계)은 스펙 수용 사항.
- **S3 0 도달 자동 종료**: 재생 중 "<<" → 0 근처까지 역주행 → 자동 종료: 클럭 ~0, 버튼 하이라이트 해제, 진입 시 재생 중이었다면 재생 복원(이후 샷에서 클럭 전진).
- **S4 토글 오프 정밀 시크**: 일시정지 상태에서 역주행 2초 → "<<" 재클릭 → 클럭이 마지막 표시 프레임에 정밀 착지(스냅 일치), 일시정지 유지.
- **S5 인수 + 가드 회귀**: 역방향 활성 중 knob 드래그 → 리버스 종료 + 드래그 세션 동작, 릴리즈 정밀 시크. 스텝 버튼 "<"/">"(793/824) 회귀.

구조 체크(프로브가 자체 판정): 서버/vplayer 생존, `Ping-Ms` < 500ms 각 시나리오 후. 프레임 번호 연속성 판정은 실행 에이전트가 샷을 Read tool으로 판독해 기록 — 프로브는 구조만 assert.

전체 스크립트 골격(초반 헬퍼는 vpt9 복사, 시나리오부):

```powershell
# vpt11 e2e: reverse auto-play (docs/50 sec 10, spec 2026-09-15 section 7 v2).
# Harness recipe = vpt9_jogframescrub.ps1 (helpers copied verbatim).
# Scenarios: S0 button calibration, S1 reverse cadence (frame numbers
# DECREASE ~15/500ms at 30fps, clock frozen by auto-pause), S2 ring
# exhaustion fallback (decrease continues past the ring start, no stall),
# S3 auto-finish at 0 (clock ~0, playback restored), S4 toggle-off precision
# seek (clock snaps to the displayed frame), S5 drag takeover + step buttons.
# Frame-number verdicts are read OFF THE SHOTS by the executing agent with
# the Read tool (lesson 19); the probe itself only checks structure.
param()
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
# ... [Wt9 type + helpers: vpt9 39-269행 그대로 복사 — Find-Server /
# Find-VPlayerLayer / Refresh-Geom / Set-LayerCursor / Send-Click /
# Click-App / Type-IntoPath / Click-Open / Save-ServerShotFast /
# Save-ServerShot / Ping-Ms / Seek-Frac / Send-Wheel / Hover-Knob] ...
$mp4     = "I:/progwork/JKENGINE/tmp/vpt2_test.mp4"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-15-vplayer-reverse-autoplay\shots"
# setup: server + vplayer + media open (vpt9 setup 블록 그대로)
# S0: Save-ServerShot "vpt11-s0-base.png" -> agent reads "<<" position
# S1: Click-App 28 68 (Play ensure) -> Click-App 755 582 ("<<") ->
#     Save-ServerShotFast s1-a; 500ms; s1-b; 500ms; s1-c  (decreasing)
# S2: Click-App 28 68 (Pause) -> Seek-Frac 0.4 -> sleep 1500 ->
#     Click-App 28 68 (Play) -> sleep 2000 (ring ~2s rebuild) ->
#     Click-App 28 68 (Pause) -> Hover-Knob 불필요("<<"는 호버 불요) ->
#     Click-App 755 582 -> s2-a .. 4 x (sleep 500 + shot) (계속 감소)
# S3: 위 세션 이어서 (재생 상태로 진입한 S1과 별도 진입) 재생 중 "<<" ->
#     sleep until target ~0 (S2 위치*fps/30 초 가산) -> s3-a (clock ~0) ->
#     sleep 1500 -> s3-b (재생 복원: 클럭 전진)
# S4: Pause -> Click-App 755 582 -> sleep 2000 -> s4-a ->
#     Click-App 755 582 (toggle-off) -> sleep 800 -> s4-b (snap 일치)
# S5: 재생 -> "<<" 진입 -> knob 드래그(mouse down on knob 888,582, move, up)
#     -> s5-a (리버스 종료, 드래그 표시) -> release 후 s5-b ->
#     Click-App 793 582 / 824 582 (step 회귀) -> shots
# teardown + RESULT (vpt9 그대로)
```

주의: 드래그는 `mouse_event(2)` down → `SetCursorPos` 곡선 이동(수 회) → `mouse_event(4)` up, vpt9의 knob 드래그 관례(S0 hover 좌표에서 시작). PS5.1 ASCII-only(레슨 50) — 한글 주석/문자열 금지, `Write tool`으로 생성.

- [ ] **Step 2: 클린바이너리 공식런 (Heisenberg 3단 — 임시 계측 없는 기능이므로 1런)**

```bash
cd I:/progwork/JKENGINE/engine/build
PATH=/c/msys64/ucrt64/bin:$PATH cmake --build . --target jkapp_vplayer
PATH=/c/msys64/ucrt64/bin:$PATH cmake --build . --target jkx_packages
powershell -ExecutionPolicy Bypass -File I:/progwork/JKENGINE/engine/tools/probes/vpt11_reverse.ps1
```

Expected: `RESULT: ALL PASS` + 샷 판독(에이전트가 Read로 vpt11-*.png 열어 프레임 번호 감소/자동 종료/스냅 일치 판정, 판정 결과를 태스크 리포트에 기록).

- [ ] **Step 3: 전체 회귀**

```bash
powershell -ExecutionPolicy Bypass -File I:/progwork/JKENGINE/engine/tools/probes/vpt9_jogframescrub.ps1
powershell -ExecutionPolicy Bypass -File I:/progwork/JKENGINE/engine/tools/probes/vpt4_e2e.ps1
powershell -ExecutionPolicy Bypass -File I:/progwork/JKENGINE/engine/tools/probes/vpt5_e2e.ps1
cd I:/progwork/JKENGINE/engine/build && ./jkdesktop.exe test
```

Expected: vpt9/vpt4/vpt5 전부 ALL PASS, `jkdesktop.exe test` 0 failures.

- [ ] **Step 4: Commit**

```bash
cd I:/progwork/JKENGINE && git add engine/tools/probes/vpt11_reverse.ps1
git commit -m "test(vplayer): vpt11 reverse auto-play e2e probe"
```

---

### Task 5: 문서 (as-built)

**Files:**
- Modify: `docs/50_vplayer_stability.md` (§8.2 레저 갱신 + §10 신설 as-built)
- Modify: `docs/superpowers/specs/2026-09-15-vplayer-jog-framescrub-design.md` (헤더 상태 + §7 구현 표기)

**Interfaces:** 없음 (문서).

- [ ] **Step 1: docs/50 §8.2 갱신** — ② v2 항목을 "해소 → §10 as-built" 로 표기, 경화 2건이 Task 1로 반영되었음을 기록.
- [ ] **Step 2: docs/50 §10 as-built 신설** — 커밋 목록, 메커니즘(케이던스/수렴점/인수 규칙), 검증 실측(vpt11 시나리오별 판정 + 회귀 결과), 실행 직감(새로 배운 것 — 예: 무입력 세션의 프레임 게이트 전제=Timer 16ms, 인수 시 세션 타깃 보존). 기존 § 스타일(§7-§9) 그대로.
- [ ] **Step 3: 스펙 상태 갱신** — 헤더 `상태: 승인 대기 → ...`를 실제 이력으로 정리, §7에 "→ 구현됨, docs/50 §10" 표기.
- [ ] **Step 4: Commit**

```bash
cd I:/progwork/JKENGINE && git add docs/50_vplayer_stability.md docs/superpowers/specs/2026-09-15-vplayer-jog-framescrub-design.md
git commit -m "docs(vplayer): v2 reverse auto-play as-built (docs/50 sec 10, spec sec 7 status)"
```

---

## Self-Review 기록

- **스펙 커버리지**: §7 역방향 케이던스 자동 진행=Task 3 Step 1, 링 소진 시 이전 키프레임 시크 반복=기존 공유 펌프 결선(Task 3 Step 2), GOP 경계 끊김 수용=Global Constraints 명시, v1 기계 위 얹음(신규 core API 0)=Architecture, 경화 2건=Task 1, 무음 계약=Global Constraints. 스펙 §3/§4는 v1 as-built(이미 구현)이므로 본 플랜의 전제로만 참조 — 갭 없음.
- **플레이스홀더 스캔**: Task 4 probe 골격의 헬퍼는 "vpt9 39-269행 복사" 참조 방식 — vpt9이 실존 파일이고 복사 대상이 정확히 특정되므로 placeholder 아님(실행자가 읽을 실제 코드의 위치를 지정). 그 외 코드 스텝 전부 실코드 포함.
- **타입 일관성**: `reverseActive_`/`reverseAcc_`/`reverseLastTick_` — Task 2 선언 = Task 3 소비 이름 일치. `finishScrub` 시그니처 불변(람다 캡처 확장만). PlayerCore API 변경 0.