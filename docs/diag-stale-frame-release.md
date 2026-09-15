# DIAG — jog/reverse release → precision seek leaves a stale picture (docs/50 §9.4-①, §10.5-③)

- Status: **DONE** (diagnosis only; no fix committed; instrumentation fully reverted, `git diff` clean)
- HEAD under diagnosis: `6d2c2c5` (one docs-only commit past the briefed `8a66a5b`; code identical)
- File under diagnosis: `engine/src/apps/ClientVPlayerApp.cpp` (pristine line numbers below)
- Probes run: `engine/tools/probes/vpt9_jogframescrub.ps1` (instrumented + clean-binary) and
  `engine/tools/probes/vpt11_reverse.ps1` (instrumented) — both `RESULT: ALL PASS`; the clean-binary
  vpt9 gate was re-run once after revert and passes.
- Raw trace evidence (untracked scratch copies): `engine/tmp/vpt_diag_log_vpt9.txt`,
  `engine/tmp/vpt_diag_log_vpt11_worker.txt` (the second run includes worker-side instrumentation).
- Key media fact: `tmp/vpt2_test.mp4` has **250-frame GOPs — keyframes only at 0 / 8.333 / 16.667 / 25 s**
  (h264, 480x270@30, AAC 44.1 kHz stereo). Any precision seek lands the demuxer on a keyframe up to
  8.33 s below the target.

---

## 1. Hypotheses formed from the code read (method step 1)

| # | Hypothesis | Predicted signature | Verdict |
|---|---|---|---|
| H1 | Wall-clock rebase ↔ stage-(a) videoQ-flush window (docs/50 §9.4 ① guess): the clock starts at t while videoQ still holds pre-seek frames | A short burst of DISP-STALE pops right after SEEK-REQ, self-correcting within one tick | **Rejected** — no DISP-STALE events at all in either probe run; stage (a) runs ≤1 ms after the request |
| H2 | Something latched post-seek (`jogTargetPts`, dropBeforePts, EMA) starves the display until the next SeekCommon | videoQ stays empty after STAGE-C Ok for seconds, no DEC-ACCEPT | **Confirmed, root cause is upstream of all of these** (H3 feeds it) |
| H3 | The stale-audio gate (`audioSkipBelow`) is disarmed by the first post-seek audio packet (negative pts) and the landing-rewind audio floods the 1 MiB ring; the worker parks in RingPush and the demuxer never reaches the target | `AUD-DISARM aPts<0` immediately after STAGE-C, then a park with `wantSeek=0`, then silence | **CONFIRMED — root cause** |
| H4 | `ended=true` EOF park swallows the release seek's wakeup | No STAGE-A/B/C after the release SEEK-REQ | **Rejected** — the seek always ran (ended=1 at STAGE-A in the stalling runs; stage (c) Ok every time) |

## 2. Instrumented evidence (actual trace excerpts)

Instrumentation: timestamped `DiagLog()` at SeekCommon, DoSeekStages stage boundaries, VideoLoop gate
abort, DecodeVideoPacket drop/accept, PopVideoFrame stale-pop + throttled idle sampler, audio-callback
starving transitions, JogTo/SetJog, SyncVideoTexture upload, finishScrub, worker audio gate
(drop/disarm), RingPush park entry, av_read_frame terminal result. All removed after the runs.

### 2.1 The dead window — vpt11 S4 toggle-off (release precision seek to 6.933 s)

```
[ 19.424] SEEK-REQ t=6.933 scrub=0 inFlight=0 paused=1 jogTargetPts=6.917 clock=8.284
[ 19.424] SET-JOG-OFF clock=8.284
[ 19.429] STAGE-A t=6.933 scrub=0 dropBefore=-1.000->6.883 vQ=0 ring=251 ended=1
[ 19.436] STAGE-B r=0 ts=6.933
[ 19.436] STAGE-C Ok clock=6.933
[ 19.436] GATE-ABORT pts=8.667 clock=6.933
[ 19.436] AUD-DISARM aPts=-0.023 skipWas=6.883 jogging=0      <-- first audio packet, NEGATIVE pts
[ 19.436] AUD-DISARM aPts=0.000 skipWas=-1.000 jogging=0      <-- gate now disarmed forever
... 562 AUD-DISARM lines: audio pts 0.000 → ~6.88 all DECODED into the ring ...
[ 19.445] RING-PARK free=4095 bytes=4096 wantSeek=0 gen=5/5   <-- 1 MiB ring FULL, device PAUSED
[ 19.455] DEC-DROP pts=5.733 gate=6.883                       <-- last video packet ever enqueued
[ 19.460→20.919] (DISP-IDLE clock=6.933 ×9 — videoQ empty, no uploads, clock pinned)
[ 20.919] SEEK-REQ t=0.000 ...                                <-- next user seek
[ 20.924] DEC-ACCEPT pts=0.000 clock=0.000 vQ=0               <-- instant re-sync
```

The demuxer parked **1.15 s of content short of the target gate** (park at video ≈5.73, gate at
6.883): the ring absorbed ~5.8 s of landing-rewind audio (0 → 5.8), then `RingPush` parked the
worker mid-GOP. The video decode thread had already consumed everything queued (all DEC-DROPPED,
pts 0.000→5.733, in 16 ms) and parked on an empty `vPktQ`. **videoQ never receives the frames at
or above the target.** The texture keeps the last jog-session frame; the audio clock sits pinned
at the landed target 6.933 (paused → `framesPlayed` frozen). Nothing notifies `cvRing` — the stall
is permanent until the next seek.

### 2.2 The same signature on every vpt11 release

```
[  7.163] STAGE-C Ok clock=2.520 | AUD-DISARM aPts=-0.023 skipWas=2.517   (S1)
[ 24.466] STAGE-C Ok clock=0.000 | AUD-DISARM aPts=-0.023 skipWas=-0.050  (S3 auto-finish)
[ 33.857] STAGE-C Ok clock=5.633 | AUD-DISARM aPts=-0.023 skipWas=5.583   (S5-a drag finish)
[ 39.266] STAGE-C Ok clock=1.520 | AUD-DISARM aPts=-0.023 skipWas=1.517   (S5-b)
```

The disarm itself fires on **every** backward seek whose landing rewind reaches the audio track's
file start (the AAC first packet carries pts −0.023 s = −1024/44100, encoder priming). Whether it
*stalls the display* depends on how much stale audio gets decoded before the demuxer reaches the
target: > ~5.8 s overflows the ring and parks the worker (dead display), less merely plays the
rewind audio into a ring that never overflows.

### 2.3 The converging control case — vpt9 S3 release (landing keyframe 8.333, target 15.933)

```
[ 21.013] STAGE-A t=15.933 scrub=0 dropBefore=-1.000->15.883 vQ=0 ring=244 ended=1
[ 21.020] STAGE-C Ok clock=15.887
[ 21.048] DEC-ACCEPT pts=15.900 clock=15.887 vQ=0        <-- converged in ~35 ms
```

Here the rewind starts at 8.333 s, so the first audio packet has pts ≈ +8.31 (≥ 0): the gate **holds**
and drops the whole rewind without decoding it — no ring fill, no park, instant re-sync. This is the
same code path working as designed, and it isolates the difference: the defect needs the rewind to
reach the track start (landing keyframe = file start) so that the first audio packet is the
negative-pts priming packet.

## 3. Confirmed root cause

**`engine/src/apps/ClientVPlayerApp.cpp:1245-1249` — the worker's stale-audio gate disarms on the
first audio packet whose `aPts` is negative, and the disarm is a one-way latch.**

```cpp
if (audioSkipBelow >= 0 && aPts >= 0 && aPts < audioSkipBelow) {
    // stale: keep gating — the demuxer must reach t
} else {
    audioSkipBelow = -1; // audio caught up: normal decode   <-- line 1249
```

The `aPts >= 0` guard exists so NOPTS packets are "not dropped" (a missed drop is benign, a wrong
drop loses audio — per the member comment at line 183-197). But the **else branch treats any packet
failing the drop test — including an unclassifiable one — as "audio caught up" and permanently
disarms the gate.** The first packet of an mp4 AAC track has pts −0.023 s (priming); after a
precision seek whose landing keyframe is the file's first keyframe (250-frame GOP → e.g. release to
6.93 s lands at 0.000), the demuxer rewind replays that packet first → the gate disarms → the
entire landing rewind (0 → t−0.05, up to 8.33 s of audio) decodes into the ring.

**Stall chain (why the picture never reaches the landed position):**

1. The decoded rewind audio fills the 1 MiB ring (`ringCap`, ~5.8 s S16 stereo). The jog session
   paused the device (`SetPaused(true)` at session entry; `jogWasPlaying_ == false` at release means
   it is never unpaused), so **nothing drains it**.
2. `RingPush` (line 1545-1557) parks the worker when `RingFreeLocked() < bytes` — confirmed by
   `RING-PARK free=4095 bytes=4096 wantSeek=0`.
3. The parked worker stops enqueuing video packets mid-GOP (last video pts ≈ 5.73, i.e. park point =
   rewind start + ring seconds ≈ 5.8). The video decode thread drains the queued pre-target packets
   (all dropped by `dropBeforePts = t−0.05` at line 1380) and parks on an empty `vPktQ`.
4. `videoQ` therefore **never** contains a frame at/above the target. `PopVideoFrame` returns false
   forever; `SyncVideoTexture` never uploads; the picture stays on the last jog-displayed frame while
   the transport clock reads exactly the landed target (audio clock pinned by the paused rebase,
   stage (a) line ~698-702). Display and clock disagree and stay disagreeing.
5. While paused the stall has **no self-heal**: the audio callback doesn't run (device paused),
   `wantSeek` is consumed, no cvRing notifier exists. Only the next `SeekCommon` exits it.

**Why a following small seek re-syncs instantly:** `SeekCommon` (line ~512-515) notifies `cvRing`
under `ringM` after setting `wantSeek`; `RingPush`'s predicate (`stop || wantSeek || seekGen != gen
|| free`) sees it, the worker aborts the park, the fresh seek's stage (a) re-arms
`audioSkipBelow = t − 0.05`, empties the ring, and the decode-forward refills `videoQ` in tens of
milliseconds (measured: SEEK-REQ 20.919 → DEC-ACCEPT 20.924). The next seek is the only exit —
exactly the observed "small seek re-syncs it immediately".

**Why the magnitude ranges ~1 s ↔ ~6.2 s:**

- **Paused release** (`jogWasPlaying_ == false`, the vpt11 S4-b/S5-b2 shape): the stall is
  *unbounded* — it lasts until the next user seek. The probe shots captured it at +1.5 s; the
  recorded "~1 s" is the shot timing, not the bound.
- **Playing release** (`jogWasPlaying_ == true` → `SetPaused(false)`): the device drains the ring in
  real time, so the worker creeps forward one ring-drain at a time; the target-area packets arrive
  after up to the ring's content time (~5.8 s) plus the residual decode-forward — the recorded
  **~6.2 s** max. When the rewind is short (< ring capacity) the demuxer never parks and the lag is
  just the decode-forward (~tens of ms at 480p).
- The magnitude in both cases is governed by **(landing-rewind distance) − (ring capacity ~5.8 s)**:
  with 8.33 s GOPs this ranges from 0 (landing at a later keyframe — the converging control case) to
  ~2.5 s within a GOP window for the park-gap, and up to the full ~5.8 s ring drain in the playing
  case. The docs/50 §9.4 ① figure (6.2 s) was taken on the older pre-split build whose ring/park
  dynamics differed slightly; the mechanism above reproduces the same order of magnitude on the
  current build.

Note the video-side twin (`dropBeforePts`, line 1380) does **not** have this hole: negative pts is
rejected before the gate (line 1341), so the gate is never disarmed by an unclassifiable frame.

## 4. Minimal fix proposal (design only — nothing committed)

1. **Root fix (one condition):** the audio gate may disarm only on a *positively classified* packet
   at/above the target. Restructure the branch at lines 1245-1249 so that `aPts < 0` (NOPTS or
   negative priming pts) keeps the gate armed — treat it like the stale branch (drop, keep gating;
   or decode without disarming — dropping is simpler and matches the existing "a wrong drop loses
   audio" asymmetry note only in the opposite direction: after a seek, ALL content below the target
   is rewind, so dropping an unclassifiable *first* packet is safe because the gate disarms on the
   first packet at/above the target anyway, which is the normal case). Concretely: disarm only when
   `aPts >= 0 && aPts >= audioSkipBelow`; otherwise drop and keep the gate armed.
2. **Optional hardening (defense in depth, separate commit):** stop treating the disarm as a latch —
   keep the seek-time `audioSkipBelow` value effective for the whole post-seek window (every packet
   is classified against it; no `-1` write at all), so a future unclassifiable-packet source cannot
   re-create the flood. The ring-full `RingPush` park while the device is paused remains a liveness
   trap for any future producer-side stall; worth a separate ledger note, not part of this fix.

With fix 1, the landing rewind audio is dropped at source (the design the comment at line 183-197
already describes), the ring never fills, the demuxer never parks, and the measured decode-forward
(~16 ms for a 173-frame GOP at 480p) puts the landing frame into `videoQ` ~30-50 ms after the
release seek — the stale-display window collapses to the same ~1-2 frame gap the converging control
case shows.

## 5. Hygiene record

- Instrumentation: 111 lines added to `engine/src/apps/ClientVPlayerApp.cpp`, fully reverted via
  `git checkout`; `git status --porcelain` clean for the file before and after the final gate run.
- No .png read (lesson 62/63); probe structure verdicts + the trace are the evidence.
- Final clean-binary `vpt9_jogframescrub.ps1` after revert+rebuild (dll/jkx mtimes newer than the
  source, lesson 37): **RESULT: ALL PASS**. The server was left running by the probe (its own
  convention); the diagnostic logs remain only as untracked scratch files in `engine/tmp/`.