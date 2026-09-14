# vplayer 조그 프레임 스크럽 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 조그 다이얼(노브 드래그 + 휠 스크럽)을 무음 프레임 단위 스크럽으로 교체 — 링 히트 구간은 즉시 표시, 링 밖은 기존 키프레임 시크 폴백.

**Architecture:** PlayerCore에 디코드 프레임 접미사 링(`jogRing`, 10초/1.5GB 캡)을 두고, 조그 중엔 다이얼 target(`JogTo`)에 맞춰 디코드 게이트가 전진하고 UI가 링에서 프레임을 골라 표시한다(`JogFrame`). 조그 주 경로의 40ms 디바운스 키프레임 시크는 링 밖 폴백으로만 남는다.

**Tech Stack:** C++ (MinGW ucrt64), FFmpeg 8.0, SDL2, ImGui — 기존 `ClientVPlayerApp.cpp` 내 PlayerCore 확장.

**Spec:** `docs/superpowers/specs/2026-09-15-vplayer-jog-framescrub-design.md`

## Global Constraints

- 모든 새 공유 상태는 기존 락 도메인에 넣는다: `jogRing`/`jogRingBytes`/`jogTargetPts`는 `m` 보호 (videoQ와 동일). **새 뮤텍스 금지.**
- 락 순서 불변: `m -> ringM`, `m -> vPktM`, `m -> vdecM` (기존 문서 주석 유지). 새 경로는 `m` 단독 획득만 허용.
- 상수: `kJogRingMaxSecs = 10.0`, `kJogRingMaxBytes = 1536 * 1024 * 1024` (스펙 §2 결정).
- 조그는 완전 무음: `jogging` 스킵 + paused 디바이스 기존 유지.
- 클라이언트는 .jkx MODL로 실행된다 — **DLL 빌드 후 반드시 `--target jkx_packages` 재팩** (레슨 18). 재팩 후 dll mtime > 소스 mtime 확인 (레슨 37).
- 커밋 메시지 끝: `Co-Authored-By: Claude Code <noreply@anthropic.com>`
- 빌드 cwd: `I:\progwork\JKENGINE\engine\build`

---

### Task 1: jogRing 저장소 + 링 push/trim + 시크 클리어

**Files:**
- Modify: `engine/src/apps/ClientVPlayerApp.cpp` (PlayerCore 멤버 ~196행, `DecodeVideoPacket` ready 루프 1179-1188, `DoSeekStages` stage (a) ~498)

**Interfaces:**
- Produces: `PlayerCore::jogRing` (std::deque<VideoFrame>, m 보호), `PlayerCore::jogRingBytes`, `PlayerCore::kJogRingMaxSecs`, `PlayerCore::kJogRingMaxBytes`, `PlayerCore::JogRingPushLocked(VideoFrame vf)` — Task 2/3이 소비.

- [ ] **Step 1: jogRing 멤버 추가** — `std::deque<VideoFrame> videoQ;` (196행) 직후:

```cpp

    // --- Jog frame-scrub history ring (spec 2026-09-15 §3.1) -------------
    // Retained decoded-frame suffix: the dial's instant-reverse window.
    // Contiguous decoded pixels — keyframe boundaries do NOT clear it (only
    // a seek does, stage (a)); GOP is a FALLBACK-path concept only. Trimmed
    // at push time to kJogRingMaxSecs / kJogRingMaxBytes; no backpressure —
    // the caps are the bound. m-protected (same domain as videoQ).
    static constexpr double kJogRingMaxSecs = 10.0;
    static constexpr size_t kJogRingMaxBytes = (size_t)1536 * 1024 * 1024;
    std::deque<VideoFrame> jogRing;
    size_t jogRingBytes = 0;
```

- [ ] **Step 2: JogRingPushLocked 헬퍼 추가** — `DecodeVideoPacket` 바로 위:

```cpp
    // m held. Retain one decoded frame in the jog ring and trim the caps
    // (time from the newest pts, bytes). The ring has NO backpressure: the
    // trim IS the bound, so the caller never parks on it.
    void JogRingPushLocked(VideoFrame vf) {
        const size_t fb = (size_t)vf.w * (size_t)vf.h * 4;
        jogRing.push_back(std::move(vf));
        jogRingBytes += fb;
        while (!jogRing.empty() &&
               (jogRing.back().pts - jogRing.front().pts > kJogRingMaxSecs ||
                jogRingBytes > kJogRingMaxBytes)) {
            jogRingBytes -= (size_t)jogRing.front().w * (size_t)jogRing.front().h * 4;
            jogRing.pop_front();
        }
    }
```

- [ ] **Step 3: ready 루프 재구성** — `DecodeVideoPacket`의 push 루프(1179-1188행, `for (VideoFrame& vf : ready) {` ~ `return true;` 앞 루프 끝)를 아래로 교체. 기존 동작 보존: 스테일 프레임 폐기, 50ms 타임드 백프레셔, 타임아웃 드롭. 신규: 항상 링 push, 조그 중 videoQ push 생략. (기존은 스테일 검사가 wait 뒤였음 — 스테일 프레임이 풀 큐에서 파킹하던 것을 제거하는 부수 정리.)

```cpp
        for (VideoFrame& vf : ready) {
            std::unique_lock<std::mutex> lk(m);
            if (stop || wantSeek) return false;
            if (vf.pts < dropBeforePts) continue; // stale frame from before the seek
            // Jog ring retention (frame-scrub history): always, before any
            // videoQ decision. During a jog the videoQ push below is SKIPPED:
            // nothing pops while paused, so the 3-slot cap would park this
            // thread and stall the dial — the ring is the jog path's sink.
            if (jogging.load(std::memory_order_relaxed)) {
                JogRingPushLocked(std::move(vf));
                continue;
            }
            JogRingPushLocked(vf); // copy — vf still moves to videoQ below
            // Backpressure: hold at most 3 pending frames (~100 ms at 30 fps).
            // Unlike the pre-split worker loop, a park here blocks ONLY video
            // decode — the demuxer keeps feeding the audio ring independently,
            // so no RefillDue() escape is needed. Timed wait (review
            // Important-1) stays: while paused nothing pops, so NOTHING notifies
            // this wait; the 50 ms escape re-checks the predicate and drops the
            // frame on timeout (the display gate drops late frames the same way,
            // and the clock gate upstream stops decode from running ahead of a
            // frozen clock by more than kVideoLead + vDecodeDelay anyway).
            const bool ok = cv.wait_for(lk, std::chrono::milliseconds(50), [&] {
                return stop || wantSeek || videoQ.size() < 3;
            });
            if (stop || wantSeek) return false;
            if (!ok) continue; // queue still full at timeout: drop
            videoQ.push_back(std::move(vf));
        }
```

- [ ] **Step 4: 시크 stage (a) 클리어** — `DoSeekStages`의 `videoQ.clear();` (498행) 직후:

```cpp
        // The jog ring is pre-seek history: drop it so the ring rebuilds
        // from the landing position (dropBeforePts guard keeps stale
        // frames out of both queues at push time).
        jogRing.clear();
        jogRingBytes = 0;
```

- [ ] **Step 5: 빌드 게이트**

Run: `cd I:\progwork\JKENGINE\engine\build && cmake --build . --target jkapp_vplayer`
Expected: BUILD 성공, 링크 에러 0. (이 시점 동작 변화 없음 — 링은 항상 쌓이지만 소비처는 Task 3. 조그 중 videoQ push 생략은 아직 UI가 jogging을 안 쓰는 기존 경로라 영향 0.)

- [ ] **Step 6: Commit**

```bash
git add engine/src/apps/ClientVPlayerApp.cpp
git commit -m "feat(vplayer): jog history ring — retained decoded-frame suffix, push/trim/seek-clear"
```

---

### Task 2: PlayerCore 조그 코어 — JogTo / JogFrame / 게이트 재편

**Files:**
- Modify: `engine/src/apps/ClientVPlayerApp.cpp` (`SnapNow` ~323, `SetJog` ~440, `SeekCommon` ~385, `VideoLoop` 게이트 ~1239, 신규 메서드)

**Interfaces:**
- Consumes: Task 1의 `jogRing`/`JogRingPushLocked`.
- Produces: `PlayerCore::JogTo(double t)`, `PlayerCore::JogFrame(VideoFrame& out) -> bool`, `PlayerCore::jogTargetPts` (m 보호, -1 = 조그 없음), `Snap::jogRingLo` (double, -1 = 링 비었음). Task 3이 소비.

- [ ] **Step 1: jogTargetPts 멤버** — Task 1 블록에 추가:

```cpp
    // Live jog dial target (UI time). -1 = no jog in flight. Written by
    // JogTo and by the fallback scrub seek (SeekCommon scrub branch) under
    // m; read by the video thread's clock gate and JogFrame (UI thread).
    double jogTargetPts = -1;
```

- [ ] **Step 2: JogTo + JogFrame 메서드** — `SetJog` 위에 추가:

```cpp
    // Jog dial target update (frame-scrub): zero-blocking. The UI thread
    // calls this every frame the dial moves; the video thread's clock gate
    // re-evaluates against it on its 10 ms poll — no notify needed.
    void JogTo(double t) {
        if (duration > 0) t = std::clamp(t, 0.0, duration);
        std::lock_guard<std::mutex> lk(m);
        jogTargetPts = t;
    }

    // UI thread: the frame to display for the live jog target — the newest
    // decoded frame at or before jogTargetPts. Nothing is popped: dialing
    // back re-displays history straight from the ring (the ring holds every
    // decoded frame, videoQ included — videoQ pushes also ring-push).
    bool JogFrame(VideoFrame& out) {
        std::lock_guard<std::mutex> lk(m);
        if (jogTargetPts < 0) return false;
        // Half-frame tolerance: the dial target is continuous time, decoded
        // pts are quantized — pick the frame the target lands on.
        const double eps = fps > 0.0 ? 0.5 / fps : 0.005;
        for (auto it = jogRing.rbegin(); it != jogRing.rend(); ++it) {
            if (it->pts <= jogTargetPts + eps) {
                out = *it; // shared_ptr copy — the frame stays for re-display
                return true;
            }
        }
        return false;
    }
```

- [ ] **Step 3: SetJog 하이지ーン** — 기존 `SetJog` 교체:

```cpp
    void SetJog(bool j) {
        jogging.store(j, std::memory_order_relaxed);
        if (!j) {
            std::lock_guard<std::mutex> lk(m);
            jogTargetPts = -1; // stale gate input must not outlive the session
        }
    }
```

- [ ] **Step 4: SeekCommon 폴백 표시 연결** — `jogSeek = scrub;` (390행) 직후:

```cpp
        // The fallback scrub seek (ring start crossed) ALSO feeds the jog
        // display: JogFrame shows each decoded frame as decode creeps toward
        // the target, so the fallback is frame-smooth too, not a keyframe pop.
        if (scrub) jogTargetPts = t;
```

- [ ] **Step 5: VideoLoop 클록 게이트 재편** — 게이트 홀드 루프(1239-1250행)의 m 블록과 판정을 교체:

```cpp
                bool seekAbort = false;
                while (!stop) {
                    double gateClock;
                    {
                        std::lock_guard<std::mutex> lk(m);
                        if (wantSeek) { seekAbort = true; break; }
                        // Jog frame-scrub: decode runs to the DIAL target, not
                        // the (pinned) clock — max() keeps non-jog playback
                        // identical (jogTargetPts is -1 outside a session).
                        gateClock = std::max(ClockNow(), jogTargetPts);
                    }
                    const double pts = mine->pts == AV_NOPTS_VALUE
                                           ? -1.0
                                           : mine->pts * av_q2d(videoTb) - ptsOrigin;
                    if (pts < 0 || pts <= gateClock + kVideoLead + vPipeDelay)
                        break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
```

- [ ] **Step 6: Snap 확장** — `Snap` 구조체에 필드 추가:

```cpp
        double jogRingLo = -1;                // oldest retained jog-ring pts (-1 = empty)
```

`SnapNow`의 `s.audioDeviceFailed = ...` 직전에 추가:

```cpp
        s.jogRingLo = jogRing.empty() ? -1.0 : jogRing.front().pts;
```

- [ ] **Step 7: 빌드 게이트**

Run: `cd I:\progwork\JKENGINE\engine\build && cmake --build . --target jkapp_vplayer`
Expected: BUILD 성공. (비조그 경로 불변: `jogTargetPts=-1` → `max(clock,-1)=clock`.)

- [ ] **Step 8: Commit**

```bash
git add engine/src/apps/ClientVPlayerApp.cpp
git commit -m "feat(vplayer): jog core — JogTo/JogFrame, dial-target decode gate, ring snapshot"
```

---

### Task 3: UI 전환 — 조그 펌프 교체 + 표시 경로 분기

**Files:**
- Modify: `engine/src/apps/ClientVPlayerApp.cpp` (`SyncVideoTexture` 1544-1550, 조그 펌프 1848-1859)
- Modify: `engine/include/apps/ClientVPlayerApp.h` (주석 2곳 — `jogLastSent_`/`wheelScrubbing_` 블록)

**Interfaces:**
- Consumes: Task 2의 `JogTo`/`JogFrame`/`Snap::jogRingLo`.

- [ ] **Step 1: SyncVideoTexture 분기** — `VideoFrame vf;`부터 `if (!p->PopVideoFrame(gate, vf)) return;`까지 교체:

```cpp
    VideoFrame vf;
    if (p->jogging.load(std::memory_order_relaxed)) {
        // Frame-scrub: display the frame at the live dial target (ring hit)
        // or the decode creep toward it (fallback seek) — the clock gate
        // does not apply, the dial owns the picture. avDelay is scrub-domain
        // pure (spec: display-gate shift only for playback) and not applied.
        if (!p->JogFrame(vf)) return;
    } else {
        // The user A/V offset lives here — a display-gate shift only. Positive
        // avDelay shows frames earlier relative to the audio clock (= audio
        // later), without touching the clock/seek domain (no stepping drift).
        const double gate = p->ClockNow() +
                            (double)p->avDelay.load(std::memory_order_relaxed);
        if (!p->PopVideoFrame(gate, vf)) return;
    }
```

- [ ] **Step 2: 조그 펌프 교체** — 기존 "Debounced latest-wins scrub seek" 블록(1848-1859행) 교체:

```cpp
        // Frame-scrub pump: inside the retained ring the dial is zero-blocking
        // (JogTo per moved target — decode runs to it, JogFrame displays it).
        // Crossing the ring start falls back to the keyframe scrub seek
        // (blocking I/O — keeps the 40 ms latest-wins debounce; SeekCommon's
        // scrub branch feeds jogTargetPts so the fallback display is
        // frame-smooth too). The single jogTargetPts slot coalesces both.
        if ((jogActive_ && !activated) || (wheelScrubbing_ && !jogActive_)) {
            const double halfFrame = fps > 0.0 ? 0.5 / fps : 0.0;
            const bool ringHit = st.jogRingLo >= 0.0 &&
                                 jogTarget_ >= st.jogRingLo - halfFrame;
            if (ringHit) {
                if (jogTarget_ != jogLastSent_) {
                    p->JogTo(jogTarget_);
                    jogLastSent_ = jogTarget_;
                    // Arm the fallback debounce too: a later ring-miss must
                    // not burst-seek through every missed frame.
                    jogLastSeek_ = std::chrono::steady_clock::now();
                }
            } else {
                const auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration<double>(now - jogLastSeek_).count() >= 0.040 &&
                    jogTarget_ != jogLastSent_) {
                    p->SeekScrub(jogTarget_);
                    jogLastSent_ = jogTarget_;
                    jogLastSeek_ = now;
                }
            }
        }
```

- [ ] **Step 3: 헤더 주석 정리** — `ClientVPlayerApp.h`:

74-75행의 `jogTarget_`/`jogLastSent_` 주석을 교체:

```cpp
    double jogTarget_ = 0;      // scrub target while dragging (UI time)
    double jogLastSent_ = -1;   // last target sent to JogTo/SeekScrub
```

(83-84행 `wheelScrubbing_` 주석의 "drive the same jogTarget_/debounce" → "drive the same jogTarget_ frame-scrub pump (ring hit = JogTo, ring-miss = debounced SeekScrub fallback)")

- [ ] **Step 4: 빌드 + 재팩**

Run: `cd I:\progwork\JKENGINE\engine\build && cmake --build . --target jkapp_vplayer && cmake --build . --target jkx_packages`
Expected: BUILD 성공. **레슨 37**: `ls -la apps/jkapp_vplayer.dll apps/vplayer.jkx`의 dll mtime이 최신 소스보다 새로운지 확인.

- [ ] **Step 5: 수동 스모크 (서버 기동 상태에서)**

vplayer 스폰 → 재생 → 일시정지 → 노브 휠 위/아래 몇 틱: 그림이 틱마다 즉시 앞/뒤로 움직이는지, 종료 후 정상 재생 복원. (정밀 판정은 Task 4 프로브.)

- [ ] **Step 6: Commit**

```bash
git add engine/src/apps/ClientVPlayerApp.cpp engine/include/apps/ClientVPlayerApp.h
git commit -m "feat(vplayer): jog dial = silent frame-scrub (ring hit instant, ring-miss keyframe fallback)"
```

---

### Task 4: 프로브 + 문서 + 회귀

**Files:**
- Create: `engine/tools/probes/vpt9_jogframescrub.ps1`
- Modify: `docs/50_vplayer_stability.md` (§7.4 레저 갱신 + as-built §8)
- Modify: `docs/23_jkdesktop_engine.md` §11.8.1 (조그 v2 보강 1줄)

**Interfaces:**
- Consumes: Task 1-3 전체. 프롬프트 하네스 = `vpt4_e2e.ps1` 레시피(DPI-aware, 측정 지오메트리, `-STA`).

- [ ] **Step 1: 프로브 작성** — `vpt9_jogframescrub.ps1`. 헤더 하네스는 vpt4_e2e.ps1 복사(초반 90행: DPI-aware Wt4 선언, Find-Server, Find-VPlayerLayer, Check/Invoke-Agentctl). 테스트 미디어: `tmp/vpt2_test.mp4`(480x270@30, 30s, 타임코드+프레임번호 번인 — 프레임 연속성을 샷에서 직독). 시나리오:

```powershell
# S0 캘리브레이션: 노브 호버 → 림 하이라이트 (vpt4 동일, kD=64/노브 중심 측정값 재사용)
# S1 전진 스크럽 (PAUSED에서 휠 위 8틱 × 3회, 틱마다 150ms 간격):
#    중간 샷 3장에서 번인 프레임번호가 소폭 연속 증가(틱당 ~7프레임 @sPerRev=3.75) —
#    키프레임 점프(수십~수백 프레임 랜덤 도약) 아님을 프레임번호 차이로 판정.
#    시계 텍스트 = jogTarget 추적.
# S2 링 내 역방향 (즉시): 휠 아래 5틱 → 샷 즉시 촬영 — 시계가 뒤로 가고
#    프레임번호 감소. (폴백이면 착지 지연이 있으므로 "틱 후 즉시 샷"이 판정.)
# S3 링 경계 폴백: S1에서 10초+ 전진 후 휠 아래 대폭(15틱) → 시계가 목표 근처로
#    착지(폴백 시크), 이후 재생 상태 유지.
# S4 릴리스: 휠 세션 400ms 경과 → 정밀 시크 착지(시계=스냅 프레임), 재생 복원.
# S5 회귀: 재생(Play/Pause), ±1F(좌/우 화살표), vpt4_e2e.ps1 재실행 PASS.
```

판정 규칙: 샷의 번인 텍스트는 **실행자가 Read 도구로 직독**한다(레슨 19 — 글리프 오독 방지, 카운터 대조). 실행자가 샷 2장의 프레임번호 차이를 계산해 Check에 넣는다.
오디오 무음(스펙 §6 항목 4)은 외부 관측 불가 — 기존 `jogging` 디코드 스킵 경로 재사용이 보장이며 프로브 헤더 주석에 명시하고 사용자 실측 항목으로 문서에 남긴다.

- [ ] **Step 2: 프로브 실행**

Run: `powershell -ExecutionPolicy Bypass -File I:\progwork\JKENGINE\engine\tools\probes\vpt9_jogframescrub.ps1`
Expected: 모든 Check PASS. 실패 시 systematic-debugging — 스테일 dll 여부(레슨 37) 먼저 확인.

- [ ] **Step 3: 전체 회귀**

Run: vpt4_e2e.ps1, vpt5_e2e.ps1 재실행
Expected: PASS (휠 스크럽/오디오 동기 회귀).

- [ ] **Step 4: 문서** — docs/50에 as-built 섹션 추가(메커니즘 4줄 + 프로브 결과 + 레저: ①NV12 업로드 경로 유지, ②jogTargetPts가 ringClosed/EOF 경계에서 프론티어 클램프 단순화 여부, ③v2 역방향 재생). docs/23 §11.8.1 말미에 "조그 프레임 스크럽(2026-09-15, docs/50 §8)" 1줄. 커밋.

```bash
git add engine/tools/probes/vpt9_jogframescrub.ps1 docs/50_vplayer_stability.md docs/23_jkdesktop_engine.md
git commit -m "docs(vplayer): jog frame-scrub as-built — probe vpt9 + docs/50 ledger"
```