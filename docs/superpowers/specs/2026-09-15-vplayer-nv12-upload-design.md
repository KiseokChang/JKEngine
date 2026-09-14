# vplayer NV12 텍스처 업로드 경로 설계 (2026-09-15)

상태: 컨트롤러 승인 (사용자 상임 지시 "묻지말고 진행" — docs/50 §7.4 ① 큐 항목)
관련: docs/50 §7.4 ① (4K 렌더 27Hz 병목), docs/50 §8 (조그 프레임 스크럽 — jogRing),
docs/23 §11.8 (vplayer)

## 1. 배경·목표

4K50 AV1 (3840×2160@50) 재생에서 디코드는 50fps를 달성했으나(demux/decode 분리,
docs/50 §7) **렌더 루프가 ~27Hz**로 떨어져 표시 게이트가 프레임을 드랍한다. 병목은
프레임당 33MB RGBA의 2단 복사다:

1. **sws_scale RGBA 변환** — 4K yuv→RGBA 패킹 (풀 재활용 후 ~9.5ms)
2. **SDL_UpdateTexture** — D3D11 업로드 Map+memcpy 33MB (매 프레임)

50fps 표시에 필요한 대역폭은 33MB×50 = 1.65GB/s — CPU 복사 한계다.

목표: **NV12(12bpp)로 2단 모두 절반** — sws는 yuv420p→NV12가 사실상 인터리브 복사
(가장 싼 경로), 업로드는 절반. SDL2가 D3D11 백엔드에서 NV12 텍스처를
셰이더 YUV→RGB로 렌더하므로 표시 품질 동일. 목표 측정치: **4K50에서 렌더 ≥40Hz**
(50fps까지 못 미쳐도 드랍률이 절반 이하로 감소하면 성공).
*(정오 2026-09-15, 최종리뷰: 본 절의 16.5MB/절반 산술은 2B/px 착오 — NV12@4K는
12.4MB(3/8), ≥40Hz 목표는 측정으로 반증됐다(docs/50 §9). 아래 ~1.8s도 동일
착오 상속, 실제 ~2.4s.)*

## 2. 컨트롤러 결정 (룰링 레저)

| 항목 | 결정 | 근거 |
|---|---|---|
| v1 범위 | **NV12 소프트웨어 변환** (sws → SDL_UpdateNVTexture). d3d11va 하드웨어 디코드·제로카피는 v2 레저 | d3d11va는 SDL_Renderer 우회(커스텀 D3D11 쿼드 드로우)가 필요해 규모가 다른 플랜 — NV12만으로도 대역폭 절반이라 병목 측정값을 먼저 확보 |
| 픽셀 레이아웃 | VideoFrame 버퍼를 단일 연속 버퍼 `w*h*3/2` — Y 평면 @ offset 0 (stride w), UV 인터리브 평면 @ offset w*h (stride w) | pooled shared_ptr 모델·풀 재활용·jogRing 기계 전부 불변 유지. 2-평면 분리 버퍼는 구조 변경 대비 이득 없음 |
| 필드명 | `VideoFrame::rgba` → `VideoFrame::pix` (주석 갱신 포함) | RGBA가 아닌데 rgba라 부르는 것은 부채 — 소비처 6곳뿐인 단일 파일이라 지금이 최저가 |
| 상수 | `kJogRingMaxSecs = 10.0` / `kJogRingMaxBytes = 1.5GB` 유지 | 바이트 캡은 절대 메모리 바운드로서 그대로 유효. 부수 효과: 4K에서 링 커버리지 ~0.9s→~2.4s (정오: 12.4MB 기준 산술) |
| 차원 상한 | `w*h*4 > 256MiB` 게이트는 `bytesFor(w,h)` 헬퍼(=w*h*3/2)로 교체 | 상한의 목적은 할당 폭주 방지 — NV12 실제 크기 기준이 정확. 초과분만 거부하는 경계는 동일 |
| 측정 | UI 렌더 주기 카운터(`SyncVideoTexture` 성공 업로드/초 EMA)를 Snap으로 노출 — 스펙 §6의 판정 지표 | 프로브가 재현 가능한 수치로 판정. 기존 §7 임시계측은 재사용하지 않음(제거됨) |

## 3. 메커니즘

### 3.1 VideoFrame (연속 NV12 버퍼)

```cpp
struct VideoFrame {
    double pts = 0;
    int w = 0, h = 0;
    // Single contiguous NV12 buffer: Y plane at offset 0 (stride w),
    // interleaved UV plane at offset w*h (stride w). Pooled like rgba before.
    std::shared_ptr<std::vector<uint8_t>> pix;
};
static inline size_t bytesFor(int w, int h) { return (size_t)w * (size_t)h * 3 / 2; }
```

### 3.2 디코드 경로 (DecodeVideoPacket / 스레드 디코드)

- `sws_getContext` 목적 포맷 `AV_PIX_FMT_RGBA` → `AV_PIX_FMT_NV12`, dstStride = w.
- 버퍼 크기/리사이즈 검사: `videoW*videoH*4` → `bytesFor(videoW, videoH)`.
- `dst[4]` = `{ data(), nullptr, nullptr, nullptr }` → `{ data(), data()+w*h, nullptr, nullptr }`,
  dstStride = `{ w, w, 0, 0 }`.
- 스레드 디코더 경로(vPktQ 소비 쪽, docs/50 §7)도 동일 교체 — sws 컨텍스트는 한 곳이므로
  실제 변경은 getContext 인자와 dst/stride 설정뿐.

### 3.3 표시 경로 (SyncVideoTexture)

```cpp
// texture creation
videoTex_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_NV12,
                              SDL_TEXTUREACCESS_STREAMING, vf.w, vf.h);
// upload
SDL_UpdateNVTexture(static_cast<SDL_Texture*>(videoTex_), nullptr,
                    vf.pix->data(), vf.w,
                    vf.pix->data() + (size_t)vf.w * vf.h, vf.w);
```

- 조그 경로(JogFrame)·일반 경로(PopVideoFrame) 공통 — 분기 밖 업로드라 한 곳만 바뀐다.
- Y/UV stride w는 sws 요구(w, w)와 SDL 요구(yPitch ≥ w, uvPitch ≥ w) 모두 충족.

### 3.4 조그 링 / 부수 계산

- `JogRingPushLocked`·trim의 `w*h*4` 2곳 → `bytesFor(w,h)`.
- 링이 잡는 풀 슬롯 33MB→12.4MB(정오, 본 문서 상단 정오 참조) — 같은
  1.5GB 캡에서 4K 커버리지 ~0.9s→~2.4s.

### 3.5 렌더 주기 카운터 (신규, 측정 계측)

- `ClientVPlayerApp` UI 멤버(스레드 불필요 — 측정·표시·갱신이 전부 UI 스레드):
  `renderFrames_`/`renderWindowStart_`/`renderHz_`. `SyncVideoTexture` 업로드
  성공 시 +1, 1초 창 경계에 확정. (룰링: PlayerCore Snap 노출 불필요 — UI 스레드
  로컬 지표라 Snap을 거치는 것은 우회일 뿐.)
- 상태행에 `렌더 27Hz` 식 표기(항상 표시 — 측정 신뢰성을 상시 검증).

## 4. 에지 케이스

| 케이스 | 처리 |
|---|---|
| 10-bit 소스 (yuv420p10le 등) | sws가 깊이 변환 — RGBA 때보다 저렴. 품질은 sws 기본 |
| RGB 소스 (AV_PIX_FMT_RGB24 등) | sws RGB→NV12 정상 지원 |
| 홀수 해상도 (w*h 홀수) | NV12는 w·h 짝수 필요 — **차원 상한 게이트에서 w,h 짝수 검사 추가** (홀수면 RGBA 폴백 유지? → 룰링: 홀수는 sws가 `sws_scale`에서 클램프 오류를 낸다. 소스가 홀수 해상도인 미디어는 드물지만 존재 — **w·h 홀수 파일은 기존 RGBA 경로 유지**, sws 포맷 선택 시 결정. 단순화: `videoW%2||videoH%2`면 NV12 대신 RGBA 픽셀 포맷 세트 사용 — 분기 2곳(tex 생성·업로드)만) |
| 텍스처 생성 실패 (드라이버 NV12 미지원) | D3D11/GL 백엔드 전부 NV12 지원(SDL 2.32) — 실패 시 기존 `videoTex_` null 처리 경로 유지(검은 화면+오류 없음, 기존 동일) |
| 조그 링 표시 | 동일 업로드 경로 — 별도 처리 없음 |

**홀수 해상도 룰링(확정):** `pixFmt` 멤버를 UI 쪽에 두고 `SyncVideoTexture`가
`SDL_PIXELFORMAT_NV12` 또는 `SDL_PIXELFORMAT_ABGR8888`을 선택 — 홀수 해상도
파일은 RGBA 경로 그대로. 코스트: 분기 3곳(생성·업로드·버퍼 stride 계산).
RGBA 폴백 버퍼도 `bytesFor` 대신 w*h*4 할당이 필요하므로 VideoFrame에
`bool nv12` 플래그를 두고 stride/size 계산이 이를 따른다.

## 5. 변경 범위

- `engine/src/apps/ClientVPlayerApp.cpp` — VideoFrame/bytesFor, sws, 업로드,
  링 바이트 2곳, 차원 상한, 렌더 카운터. 단일 파일.
- `engine/include/apps/ClientVPlayerApp.h` — 카운터 멤버·Snap 필드(있다면
  PlayerCore 내부라 헤더 무변경 가능성).
- 문서: docs/50 §7.4 ① as-built 갱신(측정치 + 레저: v2 d3d11va).

## 6. 검증

1. **베이스라인**: 픽스 전 빌드에서 4K50 테스트 파일 렌더 Hz 측정 (~27 예상).
2. **픽스 후**: 동일 파일 렌더 Hz ≥40 — 상태행 카운터로 프로브 판정.
3. **정합성**: 번인 타임코드 미디어(tmp/vpt2_test.mp4 확장 또는 4K 합성)로
   샷 직독 — 색상/크로마 왜곡 없음(NV12 셰이더 경로 육안+샷 대조).
4. **회귀**: vpt9(조그 프레임 스크럽)·vpt4·vpt5 전부 PASS — 링/폴백/무음 그대로.
5. **홀수 해상도**: 481x271 합성 파일 → RGBA 폴백 경로 렌더 확인.

테스트 미디어: ffmpeg로 3840x2160@50 H.264 합성 카운터 파일 (인코딩 수 분).
jogRing이 바이트 상한 지배임을 확인하는 측정(4K에서 링 깊이)은 as-built 기록.

## 7. v2 (레저, 미구현)

d3d11va 하드웨어 디코드 + 제로카피: `SDL_RenderGetD3D11Device`로 디바이스 획득,
FFmpeg hwframes가 SDL 공유 D3D11 텍스처에 직접 디코드, 커스텀 NV12 쿼드 드로우
(SDL_Renderer 우회). 업로드 0 — 50fps 도달의 완전 해법. v1 측정치가 남는 격차의
유일한 다음 단계. 리스크: ImGui 렌더 순서·서버 렌더 스레드와의 상호작용.