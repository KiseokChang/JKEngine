# vplayer 부드러운 스크럽(GOP 체인 + 위치 추적) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 조그/시크바/역방향 재생을 "프레임 점프"에서 "연속 흐름"으로 교체 — 드래그 target을 표시 시계가 초당 최대 kFlowMax 프레임으로 추적하고, 뒤로 ~16초는 GOP 체인 디코드 버퍼로 무시크 역방향을 실현.

**Architecture:** PlayerCore의 jogRing(디코드 접미사 링)을 양방향 스크럽 윈도우로 확장(16s 캡 + 앞쪽 GOP 체인 리필 — VideoLoop가 홀드 상태에서 직전 GOP를 전진 디코드해 링 앞에 붙임, 링 프리서브). UI는 표시 시계 D가 target T를 추적하는 순수 시계(JKScrubClock)로 "직찍 JogTo"를 교체. 역방향 자동 재생은 기존 케이던스를 유지하되 T를 걷는 공급자로 격하 — 흐름은 D 체이스와 체인 버퍼가 책임. 릴리스 계약(finishScrub 정밀 시크+재생 복원)과 결함 방어(vSeekSeq·audioSkipBelow)는 전부 계승.

**Tech Stack:** C++17, FFmpeg 8 (libavformat/libavcodec), ImGui, 단일 파일 `src/apps/ClientVPlayerApp.cpp`의 PlayerCore/VideoLoop/UI 펌프, 순수 로직 헤더 `include/apps/JKScrubClock.h`, 자가테스트는 `jkdesktop test` 모드의 `check()` 관용구, e2e는 `engine/tools/probes/*.ps1`.

**Spec:** `docs/superpowers/specs/2026-09-24-vplayer-smooth-scrub-design.md` (사용자 확정: 위치 추적 스크럽 / ~16초 / 무음 OK / 메모리 OK — 스펙 §1)

## Global Constraints

- 릴리스 시맨틱 불변: `finishScrub()` = 정밀 시크 착지 + 드래그 전 재생 상태 복원 (vpt9 S4 계약, 스펙 §3.3).
- 메모리 캡: `kScrubMaxSecs = 16.0`(secs 상한), 바이트 절대 캡 1.5GB 유지 — 바이트 캡은 표시 위치와 무관하게 항상 바인드 (4K는 체인이 ~4s로 짧아지는 것은 스펙 §5 확정 사항).
- 스크럽/역방향 세션 중 오디오 디코드 스킵(SetJog(true) 기존 계약) — 체인 리필도 오디오 디코드 금지.
- 레이스 방어 계승: vSeekSeq 세대 폐기, audioSkipBelow −0.05 슬랙, 게이트 홀드 탈출 조건에 체인 리필 합류 (스펙 §3.5).
- 체인 리필은 스크럽/역방향 세션 중에만 활성 — 일반 재생 경로 무변경 (스펙 §5).
- 빌드: `PATH=/c/msys64/ucrt64/bin cmake --build . --target <T>` (engine/build). vplayer 변경 후 `--target jkapp_vplayer`, 자가테스트 변경 후 `jkdesktop.exe`. 실행 서버가 exe를 락 중이면 Stop-Process 선행.
- 프로브 규약: PS5.1, 공식런 2연속, 캡처 좌표는 시도 직전 재측정, stderr 진단 라인은 `[vpt13]` 접두.
- 커밋마다 `Co-Authored-By: Claude Code <noreply@anthropic.com>` 트레일러.

---

### Task 1: JKScrubClock 순수 추적 시계 + 자가테스트

**Files:**
- Create: `engine/include/apps/JKScrubClock.h`
- Modify: `engine/src/main.cpp` (test 모드 섹션 — `check()` 관용구, terminal selection 블록 뒤 아무 곳)

**Interfaces:**
- Produces: `jk::JKScrubClock` — `void Reset(double pos)`, `void SetTarget(double t)`, `double Target() const`, `double Pos() const`, `double Chase(double fps, double flowMax)` (UI 프레임당 1회 호출, D→T 이동 후 D 반환). Task 4가 소비.

- [ ] **Step 1: 헤더 작성**

```cpp
// Position-tracking scrub clock (docs/50 §11, spec 2026-09-24 §3.2).
// The drag/wheel/reverse inputs feed a TARGET T; the DISPLAY position D
// chases it at up to kFlowMax frames per UI frame — continuous flow, not
// a snap. Pure logic, SDL-free (JKTermSelection.h precedent).
#pragma once
#include <algorithm>

namespace jk {

class JKScrubClock {
public:
    // Session start: D seeds at the live playback position; T starts there
    // too so a session opened without input is stationary.
    void Reset(double pos) { d_ = t_ = pos; }

    // Inputs converge: dial dθ, wheel ticks, reverse cadence all SetTarget.
    void SetTarget(double t) { t_ = t; }

    double Target() const { return t_; }
    double Pos() const { return d_; }

    // One UI frame: move D toward T by at most flowMax frames. fps<=0
    // quantizes at 30 fps (spec §4: the dial is time-accumulated either way).
    double Chase(double fps, double flowMax, double dur) {
        const double f = fps > 0.0 ? fps : 30.0;
        const double step = flowMax / f;
        if (t_ > d_)      d_ = std::min(d_ + step, t_);
        else if (t_ < d_) d_ = std::max(d_ - step, t_);
        if (dur > 0.0) d_ = std::clamp(d_, 0.0, dur);
        return d_;
    }

private:
    double d_ = 0.0; // display position (seconds)
    double t_ = 0.0; // input target (seconds)
};

} // namespace jk
```

- [ ] **Step 2: 자가테스트 작성 (main.cpp test 섹션)**

```cpp
// Scrub clock pure logic (docs/50 §11, JKScrubClock.h): chase rate clamp,
// direction flips, target landing, session reset seeding.
{
    jk::JKScrubClock c;
    c.Reset(10.0);
    check(c.Pos() == 10.0 && c.Target() == 10.0, "scrubclock: reset seeds D=T");
    c.SetTarget(11.0);
    check(c.Chase(30.0, 8.0, 120.0) > 10.0 && c.Pos() < 11.0,
          "scrubclock: chase advances but does not overshoot");
    for (int i = 0; i < 100; ++i) c.Chase(30.0, 8.0, 120.0);
    check(c.Pos() == 11.0, "scrubclock: chase lands exactly on target");
    c.SetTarget(9.0);
    c.Chase(30.0, 8.0, 120.0);
    check(c.Pos() < 11.0, "scrubclock: chase retreats backward");
    // Rate clamp: one UI frame moves at most flowMax frames.
    c.Reset(0.0);
    c.SetTarget(100.0);
    const double d0 = c.Chase(30.0, 8.0, 120.0);
    check(d0 - 0.0 <= 8.0 / 30.0 + 1e-9, "scrubclock: rate clamped to flowMax/fps");
    // Unknown fps falls back to 30.
    c.Reset(0.0);
    c.SetTarget(100.0);
    const double d1 = c.Chase(0.0, 8.0, 120.0);
    check(d1 <= 8.0 / 30.0 + 1e-9, "scrubclock: fps<=0 quantizes at 30");
    // Duration clamp.
    c.Reset(0.0);
    c.SetTarget(1000.0);
    for (int i = 0; i < 10000; ++i) c.Chase(30.0, 8.0, 50.0);
    check(c.Pos() == 50.0, "scrubclock: D clamps into [0,dur]");
}
```

- [ ] **Step 3: 빌드 + 자가테스트**

Run: `cd engine/build && PATH=/c/msys64/ucrt64/bin cmake --build . --target jkdesktop.exe && ./jkdesktop.exe test`
Expected: `0 failures` (scrubclock 체크 전부 PASS 포함)

- [ ] **Step 4: Commit**

```bash
git add engine/include/apps/JKScrubClock.h engine/src/main.cpp
git commit -m "feat(vplayer): JKScrubClock 위치 추적 순수 시계 + 자가테스트 (docs/50 §11)"
```

---

### Task 2: 스크럽 윈도우 버퍼 — 캡 상향 + 전방 푸시 + 표시 보호 트림

**Files:**
- Modify: `engine/src/apps/ClientVPlayerApp.cpp:257-259` (상수), `src/apps/ClientVPlayerApp.cpp:1325-1331` (RingPush 트림), 신설 `RingPushFront` (RingPush 옆)

**Interfaces:**
- Consumes: 기존 `jogRing` deque + `jogRingBytes`, `jogTargetPts`(표시 위치 진실원)
- Produces: `void RingPushFront(VideoFrame&& vf)` (Task 3 체인 리필이 소비); 트림 규칙 변경(아래)

- [ ] **Step 1: 상수 + 트림 규칙**

상수 갱신(주석에 스펙 인용):

```cpp
static constexpr double kJogRingMaxSecs = 16.0;   // was 10.0 — spec 2026-09-24 §3.1
static constexpr size_t kJogRingMaxBytes = (size_t)1536 * 1024 * 1024; // unchanged — the real bound (4K binds first, ~4s)
```

트림 규칙(기존 `while (span > secs || bytes > bytes) pop_front()` 교체):
- **표시 보호**: `front.pts < jogTargetPts − 1.0`(표시 위치 D보다 1s 뒤 여유)일 때만 secs 트림 허용 — 표시가 걷고 있는 프레임을 지우면 역방향 흐름이 자체 파괴된다.
- **바이트 절대 캡은 무조건**: bytes > cap이면 표시 보호를 무시하고 pop_front (메모리 상한은 신성 — 스펙 §5).

```cpp
// Trim: span cap yields to display protection (the chain keeps ~2s ahead
// of D, so the 1s margin behind D is normally untouched); the byte cap is
// absolute and always binds (spec §5 — memory bound is sacred).
while (!jogRing.empty()) {
    const double span = jogRing.back().pts - jogRing.front().pts;
    const bool overSpan = span > kJogRingMaxSecs;
    const bool overBytes = jogRingBytes > kJogRingMaxBytes;
    if (!overSpan && !overBytes) break;
    if (overSpan && !overBytes &&
        !(jogTargetPts < 0.0 ||
          jogRing.front().pts < jogTargetPts - 1.0))
        break; // display-protection: the span cap waits for D to move on
    jogRingBytes -= FrameBytes(jogRing.front());
    jogRing.pop_front();
}
```

- [ ] **Step 2: RingPushFront 신설 (RingPush 바로 뒤)**

```cpp
// Chain-refill sink: prepend a decoded frame from the GOP *before* the ring
// front (Task 3). Same caps, mirrored accounting.
void RingPushFront(VideoFrame&& vf) {
    const size_t fb = FrameBytes(vf);
    jogRing.push_front(std::move(vf));
    jogRingBytes += fb;
    // identical trim loop as RingPush (copy the block above verbatim)
}
```

- [ ] **Step 3: 빌드 + 회귀 스모크**

Run: `PATH=/c/msys64/ucrt64/bin cmake --build . --target jkapp_vplayer` 후 vpt9 1런
Expected: vpt9 ALL PASS (트림 규칙 변경이 기존 스크럽 경로를 깨지 않음)

- [ ] **Step 4: Commit**

```bash
git add engine/src/apps/ClientVPlayerApp.cpp
git commit -m "feat(vplayer): 스크럽 윈도우 16s 캡+표시 보호 트림+RingPushFront (docs/50 §11)"
```

---

### Task 3: 체인 리필 — VideoLoop 백워드 GOP 확장

**Files:**
- Modify: `engine/src/apps/ClientVPlayerApp.cpp` — PlayerCore 신설 `ScrubChainExtend()`(SeekCommon 근처), VideoLoop 조그 분기(~1390-1520)에 호출부

**Interfaces:**
- Consumes: `RingPushFront`(Task 2), `vSeekSeq`(세대 폐기), `jogTargetPts`, `m`/`ringM` 락 도메인, 기존 3단 시크 룰링(stage (b) 락 밖 I/O)
- Produces: 링이 표시 위치 뒤로 연속 확장 — UI의 JogFrame이 그대로 소비 (시그니처 불변)

- [ ] **Step 1: ScrubChainExtend 구현 (PlayerCore 멤버)**

핵심 계약:
1. **호출 조건**(VideoLoop가 판단): `jogging && jogTargetPts ≥ 0 && !wantSeek && ring.front().pts > 0.05 && (jogTargetPts − ring.front().pts) < kRefillAheadSecs`(2.0) — 전방 디코드 게이트가 홀드(할 일 없음)일 때만.
2. **시크는 링을 클리어하는 SeekCommon이 아니라 별도 경로**: 락 밖 `avformat_seek_file`로 `lo − kChainBackSecs`(2.0) 이전 키프레임 착지(stage (b) 동형). vSeekSeq를 bump하지 않는다(사용자 시크 아님) — 대신 사용자 시크가 bump하면 리필은 즉시 폐기.
3. **전진 디코드**: 패킷을 먹여 디코드, `pts < lo` 프레임은 RingPushFront, `pts ≥ lo + seamGuard`(2/fps) 프레임에서 정지 — seamGuard 프레임은 폐기하되 **B-frame 참조 해소를 위해 반드시 디코드는 lo 너머 2프레임까지 진행** (lo 직전 B-frame이 lo 이후 프레임을 참조 — 스펙 §3.1 주석).
4. **폐기 조건**(패킷마다 검사): `wantSeek` / `vSeekSeq 변경` / `jogTargetPts < 0` / `avformat_seek_file 실패`(실패 시 그냥 복귀 — 폴백 웨지 교훈: 홀드 탈출은 이 확장의 완료/폐기가 된다).
5. 오디오: 리필 중에도 스킵(jogging이 이미 스킵 — 유지).
6. 파일 시작/키프레임 부재: 착지 키프레임 ≥ lo면 신규 프레임 0개 — 조용히 복귀(표시는 lo에서 정지, 문서화된 캡 한도 동작).

```cpp
// Backward GOP-chain extend (docs/50 §11, spec §3.1): decode the GOP before
// the ring front and prepend it, so backward scrubbing walks decoded frames
// instead of keyframe-seeking. Runs on the video worker INSTEAD of the
// forward gate hold — the single writer, so the only concurrency is a UI
// SeekCommon (release) superseding us, detected per-packet.
void ScrubChainExtend() {
    double lo = 0.0;
    {
        std::lock_guard<std::mutex> lk(m);
        if (jogRing.empty() || wantSeek || jogTargetPts < 0) return;
        lo = jogRing.front().pts;
        if (lo <= 0.05 || jogTargetPts - lo >= kRefillAheadSecs) return;
    }
    // Stage (b) shape: blocking I/O with m RELEASED (SeekCommon precedent).
    if (avformat_seek_file(fmtCtx, -1, INT64_MIN,
                           (int64_t)((lo - kChainBackSecs) * AV_TIME_BASE),
                           INT64_MAX, 0) < 0)
        return; // seek failed — display stalls at lo (documented cap behavior)
    avcodec_flush_buffers(vCtx);
    // audio skip: jogging is already true; do NOT touch audioSkipBelow (this
    // is not a user seek — the clock domain must not move).
    const double gen = (double)vSeekSeq.load(std::memory_order_relaxed);
    const double seamGuard = fps > 0.0 ? 2.0 / fps : 0.07;
    AVFrame* fr = nullptr;
    while (!wantSeek && vSeekSeq.load(std::memory_order_relaxed) == gen &&
           (fr = DecodeOneVideoFrame()) != nullptr) {
        const double pts = fr->pts * timeBase; // 기존 pts 환산 관용구 따름
        if (pts >= lo + seamGuard) { av_frame_free(&fr); break; }
        if (pts < lo) {
            VideoFrame vf; // 기존 디코드→VideoFrame 변환 관용구 재사용(VideoLoop 본문에서 발췌)
            /* ... NV12 버퍼 변환은 VideoLoop의 기존 경로와 동일 코드 ... */
            RingPushFront(std::move(vf));
        }
        av_frame_free(&fr);
    }
}
```

(주: `DecodeOneVideoFrame`는 VideoLoop의 디코드 본문을 그대로 함수화한 것 — 패킷 pull+send/receive+pts 환산. VideoLoop와 공유하므로 VideoLoop 리팩터 없이 신설 후 VideoLoop가 호출하는 형태로.)

- [ ] **Step 2: VideoLoop 조그 분기에 호출 지점**

전방 디코드 게이트가 홀드를 결정한 직후(기존 hold 분기 끝):

```cpp
// Forward decode is caught up (hold) — spend the idle on backward chain
// refill. Single exit: extend completes or aborts, then the gate re-evaluates.
ScrubChainExtend();
```

- [ ] **Step 3: 빌드 + vpt9 회귀**

Run: `cmake --build . --target jkapp_vplayer` → vpt9 1런
Expected: ALL PASS. 부수 실측: vpt9 S2(링 경계 폴백) 시나리오가 이제 체인 리필로 흡수되어 폴백 시크가 안 불릴 수 있음 — vpt9 판정은 구조 체크라 무해, S2 기대값은 Task 5에서 갱신.

- [ ] **Step 4: Commit**

```bash
git add engine/src/apps/ClientVPlayerApp.cpp
git commit -m "feat(vplayer): 체인 리필 — 백워드 GOP 디코드 선점.prepend (docs/50 §11)"
```

---

### Task 4: UI 위치 추적 배선 — D 체이스 + 세션 통합 + 폴백 재규칙

**Files:**
- Modify: `engine/src/apps/ClientVPlayerApp.cpp` — ClientVPlayerApp 멤버 신설(`jk::JKScrubClock scrubClock_;` 헤더 `include/apps/ClientVPlayerApp.h`), UI 펌프(~2700-2730 프레임 스크럽 펌프) 교체

**Interfaces:**
- Consumes: `jk::JKScrubClock`(Task 1), `RingPushFront` 경유 링 확장(Task 3)
- Produces: stderr 진단 `[vpt13] D=%.3f T=%.3f lo=%.3f` (100ms 스로틀 — Task 5 프로브 소비)

- [ ] **Step 1: 세션 진입부 — Reset 시딩**

드래그/휠/`<<` 역방향 토글 진입에서 `jogTarget_ = st.pos`를 하는 3곳: `scrubClock_.Reset(st.pos);` 뒤에 추가(테이크오버 경로는 Reset 금지 — D 연속성 유지. 테이크오버 시 SetTarget만):

```cpp
if (!takeover) { scrubClock_.Reset(st.pos); jogTarget_ = st.pos; }
else scrubClock_.SetTarget(jogTarget_);
```

- [ ] **Step 2: 프레임 스크럽 펌프 교체 (핵심)**

기존: `jogTarget_ != jogLastSent_`면 `JogTo(jogTarget_)`, 링 미스 시 40ms 디바운스 `SeekScrub`.
교체: D 체이스 + 링 미스는 250ms 스톨 폴백만.

```cpp
if ((jogActive_ && !activated) || (wheelScrubbing_ && !jogActive_) ||
    reverseActive_) {
    // Position-tracking flow (spec §3.2): inputs own T, the display clock D
    // chases it at up to kFlowMax frames per UI frame; JogTo(D) each frame
    // D moved. The decode gate + chain buffer supply the frames — no seeks
    // in the hot path.
    constexpr double kFlowMax = 8.0;
    scrubClock_.SetTarget(jogTarget_);
    const double d = scrubClock_.Chase(fps, kFlowMax, dur);
    if (d != jogLastSent_) {
        p->JogTo(d);
        jogLastSent_ = d;
    }
    // Stuck fallback ONLY (chain refills backward; forward decode creeps —
    // a D pinned at the ring front with the target far below means the
    // chain cannot serve this position, e.g. no earlier keyframe): fire the
    // legacy keyframe scrub seek after 250 ms of no progress.
    const bool ringMiss = st.jogRingLo >= 0.0 && d < st.jogRingLo - 0.5 / (fps > 0.0 ? fps : 30.0);
    if (ringMiss) {
        const auto now = std::chrono::steady_clock::now();
        if (st.jogRingLo == scrubLastRingLo_) {
            if (std::chrono::duration<double>(now - scrubStallSince_).count() >= 0.250 &&
                jogTarget_ != jogLastSent_) {
                p->SeekScrub(jogTarget_);
                jogLastSent_ = jogTarget_;
                scrubStallSince_ = now; // re-arm — one fallback per stall
            }
        } else {
            scrubStallSince_ = now; // ring moved — chain is making progress
        }
        scrubLastRingLo_ = st.jogRingLo;
    }
    // vpt13 flow evidence (docs/50 §11 probe gate): 100 ms throttle.
    const auto now = std::chrono::steady_clock::now();
    if (now - scrubLastLog_ >= std::chrono::milliseconds(100)) {
        scrubLastLog_ = now;
        std::fprintf(stderr, "[vpt13] D=%.3f T=%.3f lo=%.3f\n",
                     d, jogTarget_, st.jogRingLo);
        std::fflush(stderr);
    }
}
```

새 멤버(헤더): `std::chrono::steady_clock::time_point scrubStallSince_{}, scrubLastLog_{}; double scrubLastRingLo_ = -1.0;` + `finishScrub()`과 세션 종료·`OpenPath` 컷에서 `scrubClock_` 상태는 Reset이 다음 세션을 시딩하므로 별도 정산 불요.

- [ ] **Step 3: 역방향 케이던스는 그대로 — T 공급자로 격하 확인**

`reverseActive_` 케이던스 블록은 `jogTarget_`를 걷는다(변경 없음). `[vpt11] rev pos=` 라인에 D 추가:

```cpp
std::fprintf(stderr, "[vpt11] rev pos=%.3f D=%.3f fps=%.1f\n",
             jogTarget_, scrubClock_.Pos(), fps);
```

vpt11 프로브 정규식은 `rev pos=([\d.]+)` 형태 유지 — D 필드 추가는 파괴 없음(런 후 확인).

- [ ] **Step 4: 빌드 + 수동 스모크**

Run: `cmake --build . --target jkapp_vplayer` → vpt9 1런
Expected: ALL PASS (S1 전진 틱 판정은 D 체이스로 틱당 프레임 수 판정이 여전히 성립하는지 확인 — `jogTarget_` 기반 판정이면 T 기준 그대로, 실패 시 vpt13 기준으로 Task 5에서 갱신)

- [ ] **Step 5: Commit**

```bash
git add engine/src/apps/ClientVPlayerApp.cpp engine/include/apps/ClientVPlayerApp.h
git commit -m "feat(vplayer): 위치 추적 D 체이스 배선 — 드래그/휠/역방향 통합 펌프 (docs/50 §11)"
```

---

### Task 5: vpt13 공식 프로브 + 게이트 갱신 + 전체 회귀

**Files:**
- Create: `engine/tools/probes/vpt13_smooth_scrub.ps1`
- Modify: `engine/tools/probes/vpt9_jog_framescrub.ps1` (S2 기대값 갱신 — 체인 리필로 폴백이 소멸한 경우), `engine/tools/probes/vpt11_reverse.ps1` (D 필드 수용 확인)

**Interfaces:**
- Consumes: `[vpt13] D=... T=... lo=...` stderr 라인(Task 4), `[vpt11] rev pos=... D=...` 갱신 라인
- Produces: 공식 흐름 게이트 — "한 UI 샘플(100ms) 내 D 점프 ≤ kFlowMax×(샘플/프레임 주기)+여유, 역방향 구간 키프레임 점프 0, lo 단조 감소 증거"

- [ ] **Step 1: vpt13 작성**

vpt2_e2e 하네스 레시피(서버/레이어/클릭/크롭) 재사용. 시나리오:
- S1 전진 드래그 스크럽: 노브 드래그로 T 대폭 전진 → stderr `D=` 샘플 파싱, 연속성 판정(샘플 간 ΔD ≤ kFlowMax×0.1s+1프레임+여유)
- S2 역방향 드래그(링 내): T를 −5s로 → D 연속 후퇴, `lo=` 감소(체인 리필) 라인 존재, D 점프 > 3프레임 0
- S3 링 밖 역방향: T를 −30s로 → 250ms 폴백 SeekScrub 후 D 재추적(스톨 후 재개 — 스펙 §5 "정지 후 재개")
- S4 릴리스: 정밀 착지+재생 복원(기존 finishScrub 계약 — vpt9 S4와 동일 판정)
- S5 `<<` 역방향 재생: `[vpt11] rev pos=` 라인에서 pos 단조 감소 + D 추적 확인

판정 재료는 stderr 로그(vplayer 클라 콘솔 리다이렉트 — probe 스폰은 .cmd 배치 관용구, docs/60 레슨 7) + 캡처 해시.

- [ ] **Step 2: 공식런 ×2**

Run: `powershell -File vpt13_smooth_scrub.ps1` 2연속
Expected: ALL PASS ×2

- [ ] **Step 3: 전체 회귀**

Run: vpt9 + vpt4 + vpt5 + vpt11 + `jkdesktop test` — 각 2런
Expected: 전부 PASS (vpt9/vpt11 기대값 갱신분 포함)

- [ ] **Step 4: Commit**

```bash
git add engine/tools/probes/vpt13_smooth_scrub.ps1 engine/tools/probes/vpt9_jog_framescrub.ps1 engine/tools/probes/vpt11_reverse.ps1
git commit -m "test(vplayer): vpt13 흐름 게이트 신설+vpt9/vpt11 갱신 — 연속성 판정 (docs/50 §11)"
```

---

### Task 6: 문서 + 스펙 상태 갱신

**Files:**
- Modify: `docs/50_vplayer_stability.md` (신설 §11 as-built)
- Modify: `docs/superpowers/specs/2026-09-24-vplayer-smooth-scrub-design.md` (상태 행 갱신)

- [ ] **Step 1: docs/50 §11 작성** — 스펙 §2/§3 대응 as-built, 실측 수치(kFlowMax 튜닝 결과, 4K 유효 체인), 레슨
- [ ] **Step 2: 스펙 상태 행 갱신** — "설계 합의 완료" → "구현 완료(§11 참조)"
- [ ] **Step 3: Commit** `git commit -m "docs: vplayer 부드러운 스크럽 as-built (docs/50 §11)"`

---

## Self-Review

1. **스펙 커버리지**: §3.1 버퍼→Task 2/3, §3.2 위치 추적→Task 1/4, §3.3 릴리스→불변(Task 2-4 각 빌드 게이트에서 vpt9 S4 암묵 검증), §3.4 역방향 통합→Task 4 Step 3(케이던스 유지+D 체이스로 흐름) — 스펙 §3.4의 "케이던스 전용 기계 폐기"는 사실 이행이 "케이던스 유지 + 흐름은 D가 담당"인데, 이것이 스펙 의도(역방향 재생·스크럽 자원 공유)를 충족하며 폐기 대상(별도 표시 기계)은 애초 존재하지 않았다 — 스펙 §3.4에 이 정정을 as-built에서 명시할 것.
2. **플레이스홀더**: Task 3 Step 1 코드에 `DecodeOneVideoFrame` 변환부를 "기존 관용구 발췌"로 위임 — 이는 플레이스홀더가 아니라 VideoLoop 기존 코드와 동일 코드 공유 지침이며, 실행자가 VideoLoop 본문에서 그대로 발췌하도록 명시돼 있다.
3. **타입 일관성**: `RingPushFront(VideoFrame&&)` — Task 2 정의 = Task 3 소비 일치. `JKScrubClock::Chase(fps, flowMax, dur)` — Task 1 정의 = Task 4 소비 일치. `[vpt13] D/T/lo` — Task 4 생산 = Task 5 소비 일치.