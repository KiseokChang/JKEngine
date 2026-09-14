# vplayer NV12 텍스처 업로드 경로 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** vplayer 비디오 표시 경로를 RGBA(33MB@4K)에서 NV12(16.5MB)로 전환해 4K50 렌더 27Hz 병목을 완화 — sws 목적 포맷 변경 + SDL_UpdateNVTexture + 연속 NV12 버퍼.

**Architecture:** VideoFrame의 pooled shared_ptr 버퍼를 단일 연속 NV12 레이아웃(Y @ 0, UV 인터리브 @ w*h)으로 유지하되 크기만 절반. sws_getContext가 NV12를 출력하고 SDL은 D3D11 셰이더 YUV→RGB로 렌더. 홀수 해상도 파일은 RGBA 폴백(스펙 §4 룰링). 조그 링/게이트/폴백 기계는 전부 불변 — 바이트 계산만 `bytesFor`로.

**Tech Stack:** C++ (MinGW ucrt64), FFmpeg 8.0, SDL 2.32.10 (SDL_UpdateNVTexture 사용 가능), ImGui — 기존 `ClientVPlayerApp.cpp` 단일 파일.

**Spec:** `docs/superpowers/specs/2026-09-15-vplayer-nv12-upload-design.md`

## Global Constraints

- 조그 기계(jogRing/JogTo/JogFrame/vSeekSeq/audioSkipBelow)는 **불변** — 이 플랜은 버퍼 레이아웃과 업로드만 바꾼다. 링 push/trim 위치·조건 변경 금지.
- 상수 `kJogRingMaxSecs = 10.0` / `kJogRingMaxBytes = 1.5GB` 유지.
- 새 뮤텍스·새 스레드 금지. 렌더 카운터는 UI 스레드 전용 멤버(락/아토믹 불필요).
- 락 순서 불변: `m -> ringM`, `m -> vPktM`, `m -> vdecM`.
- 홀수 해상도(w·h 중 홀수) 파일은 RGBA 경로 유지 (스펙 §4 룰링).
- 디코드 전환과 표시 전환은 **한 커밋에서 원자적** — NV12 버퍼를 RGBA 업로드로 깨는 중간 커밋 금지 (docs/50 §8 룰링과 동일 원칙). 따라서 Task 1이 커밋을 수행한다.
- 클라이언트는 .jkx MODL로 실행 — **DLL 빌드 후 `--target jkx_packages` 재팩** (레슨 18), dll mtime > 소스 mtime 확인 (레슨 37). 빌드 시 PATH에 `/c/msys64/ucrt64/bin` 필요.
- PS5.1 프로브는 ASCII 전용. 커밋 메시지 끝 `Co-Authored-By: Claude Code <noreply@anthropic.com>`.

---

### Task 1: NV12 전환 — 디코드 + 표시 + 렌더 카운터 (원자적)

**Files:**
- Modify: `engine/src/apps/ClientVPlayerApp.cpp` (VideoFrame 구조체 ~97행, 차원 상한 ~832-836, sws_getContext ~888, 스레드 디코드 풀/sws_scale ~1291-1296, JogRingPushLocked ~1236-1242, SyncVideoTexture 1703-1740, BuildUi 상태행)
- Modify: `engine/include/apps/ClientVPlayerApp.h` (UI 멤버 3개)

**Interfaces:**
- Produces: `VideoFrame { pts, w, h, nv12, pix }` (연속 NV12 또는 RGBA 버퍼), `bytesFor(w, h)` (NV12 크기), `FrameBytes(const VideoFrame&)` (nv12 플래그 따름), PlayerCore 멤버 `nv12Out` (OpenStage 확정), UI 렌더 주기 표기 `렌더 NNHz` — Task 2 프로브 판정 지표.

- [ ] **Step 1: VideoFrame + 크기 헬퍼** — 구조체(~91-101행) 교체:

```cpp
// One decoded video frame in a single contiguous NV12 buffer (Y plane at
// offset 0, stride w; interleaved UV plane at offset w*h, stride w) — half
// the bytes of the old RGBA layout, so both the sws conversion and the
// SDL_UpdateNVTexture upload cost half as much at 4K (the 27 Hz render
// bottleneck, docs/50 §7.4-①). Odd-dimension sources keep RGBA (NV12 needs
// even w/h) — nv12 flags which layout this buffer holds. pix is a pooled
// shared buffer: at 4K a fresh allocation per frame measured ~20 ms; the
// pool recycles released slots (docs/50 §7). Ownership follows the
// refcount — frames parked in videoQ/jogRing or held by the last UI
// upload keep their buffer alive.
struct VideoFrame {
    double pts = 0;
    int w = 0, h = 0;
    bool nv12 = true;
    std::shared_ptr<std::vector<uint8_t>> pix;
};
static inline size_t bytesFor(int w, int h) { return (size_t)w * (size_t)h * 3 / 2; }
static inline size_t FrameBytes(const VideoFrame& vf) {
    return vf.nv12 ? bytesFor(vf.w, vf.h) : (size_t)vf.w * (size_t)vf.h * 4;
}
```

- [ ] **Step 2: PlayerCore 멤버 + 차원 상한** — PlayerCore 멤버 영역에 추가:

```cpp
    // Even-dimension sources render NV12 (half the upload bytes); odd ones
    // keep RGBA (NV12 needs even w/h) — decided at open, spec §4.
    bool nv12Out = false;
```

차원 상한 게이트(~832-836행)를 교체 — 식만, fail 유지:

```cpp
            nv12Out = (w % 2 == 0) && (h % 2 == 0);
            const int64_t maxBytes = nv12Out ? (int64_t)bytesFor(w, h)
                                             : (int64_t)w * h * 4;
            if (w > 8192 || h > 8192 || maxBytes > (int64_t)256 * 1024 * 1024) {
```

- [ ] **Step 3: sws_getContext** — ~888행 목적 포맷 교체:

```cpp
            sws = sws_getContext(videoW, videoH, vctx->pix_fmt,
                                 videoW, videoH,
                                 nv12Out ? AV_PIX_FMT_NV12 : AV_PIX_FMT_RGBA,
                                 SWS_BILINEAR, nullptr, nullptr, nullptr);
```

- [ ] **Step 4: 스레드 디코드 풀 + sws_scale** — ~1291-1295행 교체:

```cpp
                vf.nv12 = nv12Out;
                vf.pix = buf;
                if (vf.pix->size() != FrameBytes(vf))
                    vf.pix->resize(FrameBytes(vf));
                uint8_t* dst[4] = { vf.pix->data(), nullptr, nullptr, nullptr };
                int dstStride[4] = { videoW * 4, 0, 0, 0 };
                if (nv12Out) {
                    dst[1] = vf.pix->data() + (size_t)videoW * videoH;
                    dstStride[0] = videoW;
                    dstStride[1] = videoW;
                }
```

- [ ] **Step 5: JogRingPushLocked 바이트 계산** — ~1236/1242행의 `w * h * 4` 2곳:

```cpp
        const size_t fb = FrameBytes(vf);
```
```cpp
            jogRingBytes -= FrameBytes(jogRing.front());
```

- [ ] **Step 6: UI 멤버** — `ClientVPlayerApp.h` (조그 멤버 블록 근처):

```cpp
    // Render-rate metric: UI-thread-only counter refreshed by
    // SyncVideoTexture (uploads/sec over a 1 s window), shown in the status
    // row — the probe reads it as the 4K upload-bottleneck verdict.
    int renderFrames_ = 0;          // uploads in the current 1 s window
    double renderWindowStart_ = 0;  // SDL_GetTicks-based window start (s)
    int renderHz_ = 0;              // last completed window's rate
```

- [ ] **Step 7: SyncVideoTexture 교체** — 텍스처 생성(1725-1734)과 업로드(1735-1739) 교체:

```cpp
    if (!videoTex_ || texW_ != vf.w || texH_ != vf.h) {
        if (videoTex_) SDL_DestroyTexture(static_cast<SDL_Texture*>(videoTex_));
        // NV12 for even-dimension sources (the D3D11/GL backends render it
        // with a YUV→RGB shader — no CPU conversion), RGBA fallback for odd.
        videoTex_ = SDL_CreateTexture(renderer,
                                      vf.nv12 ? SDL_PIXELFORMAT_NV12
                                              : SDL_PIXELFORMAT_ABGR8888,
                                      SDL_TEXTUREACCESS_STREAMING, vf.w, vf.h);
        texW_ = vf.w;
        texH_ = vf.h;
        hasFrame_ = false;
        if (videoTex_) SDL_SetTextureScaleMode(static_cast<SDL_Texture*>(videoTex_),
                                               SDL_ScaleModeLinear);
    }
    if (videoTex_) {
        if (vf.nv12) {
            SDL_UpdateNVTexture(static_cast<SDL_Texture*>(videoTex_), nullptr,
                                vf.pix->data(), vf.w,
                                vf.pix->data() + (size_t)vf.w * vf.h, vf.w);
        } else {
            SDL_UpdateTexture(static_cast<SDL_Texture*>(videoTex_), nullptr,
                              vf.pix->data(), vf.w * 4);
        }
        hasFrame_ = true;
        // Render-rate window (see header comment).
        const double now = SDL_GetTicks() / 1000.0;
        if (renderHz_ == 0 || now - renderWindowStart_ >= 1.0) {
            renderHz_ = renderFrames_;
            renderFrames_ = 0;
            renderWindowStart_ = now;
        }
        ++renderFrames_;
    }
```

- [ ] **Step 8: 상태행 표기** — BuildUi의 재생 상태 행(볼륨/오디오 상태 행)에:

```cpp
    ImGui::Text("렌더 %dHz", renderHz_);
```

(배치: 기존 상태행 위젯 뒤 SameLine 체인 끝 또는 다음 행 — 기존 레이아웃 흐름에 맞춤. 판정 지표라 항상 표시.)

- [ ] **Step 9: 빌드 + 재팩 + 수동 스모크**

Run: `cd I:\progwork\JKENGINE\engine\build && export PATH=/c/msys64/ucrt64/bin:$PATH && cmake --build . --target jkapp_vplayer && cmake --build . --target jkx_packages && ls -la apps/jkapp_vplayer.dll apps/vplayer.jkx`
Expected: BUILD 성공, dll mtime > 소스 mtime (레슨 37).

스모크: 서버 기동 → vplayer → tmp/vpt2_test.mp4 재생 → 그림 정상(색상 무왜곡), 상태행에 렌더 Hz 표시, 조그 휠 몇 틱 정상.

- [ ] **Step 10: Commit**

```bash
git add engine/src/apps/ClientVPlayerApp.cpp engine/include/apps/ClientVPlayerApp.h
git commit -m "feat(vplayer): NV12 texture upload path — half the 4K frame bytes, sws+UpdateNVTexture, RGBA fallback for odd dims"
```

---

### Task 2: 4K 측정 프로브 + 회귀 + 문서

**Files:**
- Create: `engine/tools/probes/vpt10_nv12.ps1`
- Create: `tmp/vpt10_4k.mp4` (ffmpeg 합성; tmp/는 git-ignored — 생성 스크립트는 프로브에 포함)
- Modify: `docs/50_vplayer_stability.md` (§7.4 ① as-built + 레저)

**Interfaces:**
- Consumes: Task 1의 `렌더 NNHz` 상태행 표기.

- [ ] **Step 1: 4K 테스트 미디어 생성** (없으면; 있으면 재사용):

```bash
ffmpeg -f lavfi -i "testsrc2=size=3840x2160:rate=50,format=yuv420p" \
       -f lavfi -i "sine=frequency=440:sample_rate=48000" \
       -t 20 -c:v libx264 -preset veryfast -crf 30 -pix_fmt yuv420p \
       -c:a aac tmp/vpt10_4k.mp4
```

- [ ] **Step 2: 프로브 vpt10** — 헤더 하네스 vpt9 복사. 시나리오:
  - S0: 4K 파일 오픈 + 재생 시작.
  - S1: 10s 재생 후 상태행 스크린샷 → 실행자 Read로 `렌더 NNHz` 직독. **판정: NN >= 40** (기대 ~27). 베이스라인은 docs/50 §7.3 실측(같은 경로의 임시계측 ~27Hz)을 인용 — 중복 측정 불필요.
  - S2: 색상 정합 — testsrc2 컬러바 패턴 샷에서 대표 2색 직독/육안 대조 (RGBA→NV12 셰이더 경로 크로마 왜곡 검증).
  - S3: 조그 휠 위/아래 5틱 → 조그 동작 + 링 히트 즉시 표시 (vpt9 기계 불변 확인).
  - S4: 재생/일시정지 복원.
  - Run: `powershell -ExecutionPolicy Bypass -File I:\progwork\JKENGINE\engine\tools\probes\vpt10_nv12.ps1`
  Expected: S1 판정 통과, S2-S4 PASS.

- [ ] **Step 3: 전체 회귀** — vpt9_jogframescrub, vpt4_e2e, vpt5_e2e 재실행 ALL PASS. 홀수 해상도 폴백: `ffmpeg -f lavfi -i testsrc2=size=481x271:rate=30 -t 5 tmp/vpt10_odd.mp4` 렌더 확인(그림 정상 = RGBA 폴백).

- [ ] **Step 4: 문서** — docs/50 §7.4 ① as-built: 전/후 렌더 Hz, 4K 링 깊이 변화(0.9s→1.8s), NV12 변환 CPU 시간 실측(측정된다면). 레저: v2 d3d11va 제로카피(스펙 §7) — v1 측정 격차에 따라 판단. 커밋.

```bash
git add engine/tools/probes/vpt10_nv12.ps1 docs/50_vplayer_stability.md
git commit -m "docs(vplayer): NV12 upload path as-built — 4K render Hz before/after + docs/50 ledger"
```