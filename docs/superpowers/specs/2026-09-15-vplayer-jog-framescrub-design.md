# vplayer 조그 프레임 스크럽 설계 (2026-09-15)

상태: 구현 완료 (2026-09-15) — v1(§3~§6) 구현 + 결함 픽스 9c7979e (docs/50 §8),
v2(§7) 구현 (플랜 docs/superpowers/plans/2026-09-15-vplayer-reverse-autoplay.md,
커밋 24bf943..6c4fec1 — **docs/50 §10 as-built**, vpt11 공식 런 ALL PASS)
관련: docs/50 §7.4 레저, docs/23 §11.8.1 (조그 다이얼 v1), docs/23 §11.8 (vplayer)

## 1. 배경·목표

현행 조그(노브 드래그 + 휠 스크럽)는 40ms 디바운스 latest-wins 키프레임 시크(`SeekScrub`)라
"프레임 부드러운" 스크럽이 아니다 — 키프레임 사이를 건너뛰고, 역방향은 과거 키프레임 점프뿐이다.

사용자 요구 (2026-09-15):
- 조그 다이얼 = **소리 없이(무음) 앞/뒤 프레임 단위 부드러운 스크럽**
- 역방향은 **버퍼 크기에 따른다** (버퍼에 있는 만큼 즉시, 넘으면 시크)
- 역방향 자동 재생도 있으면 좋음 → **v2로 후속** (v1 = 조그 스크럽 개선만)

## 2. 사용자 결정 (레저)

| 항목 | 결정 |
|---|---|
| v1 범위 | 조그 프레임 스크럽(앞/뒤) + 히스토리 버퍼 + 폴백. 역방향 자동 재생은 v2 |
| 버퍼 정책 | GOP 단위 전체 → **시간 기반으로 정정** (사용자 제안: "앱 스펙은 기본 몇 초"가 맞음) |
| 안전장치 | 바이트 상한 폴백 (초안 결정 유지) |
| 기본값 | **10초 고정** (설정화 없음, 상수. 후속 티켓에서 설정화 가능) |

## 3. 메커니즘

### 3.1 jogRing — 히스토리 링 (PlayerCore, m 보호)

- `std::deque<VideoFrame> jogRing` + `size_t jogRingBytes`
- **정의**: 마지막으로 디코드된 프레임들의 **연속 접미사**. 키프레임 경계를 넘어 이어
  보관해도 표시에 문제 없음(이미 디코드된 픽셀) — GOP는 폴백 경로에만 존재하는 개념.
- **채움**: `DecodeVideoPacket` push 루프에서 videoQ에 넣은 동일 `VideoFrame`
  (pooled `shared_ptr`, **추가 복사 0**)을 jogRing에도 push.
- **trim (push 시)**: `(newest.pts - front.pts > 10s) || jogRingBytes > 1.5GB` 동안
  front-pop. 상수 `kJogRingMaxSecs = 10.0`, `kJogRingMaxBytes = 1.5GB`.
- **클리어**: 시크 stage (a)에서만 (videoQ와 함께). 랜딩 후 재구축.
- **체감**: 1080p 30fps = 10초 ≈ 930MB (시간 상한 지배), 4K 50fps = 바이트 상한 지배
  ≈ 45프레임 ≈ 0.9초. "역방향은 버퍼 크기에 따른다"의 구체적 형태.
- **풀 상호작용**: ring이 붙잡은 풀 슬롯(`use_count>1`)은 재활용 불가 → ring 유지분은
  사실상 풀 밖. ring 유지 비용 = 사용자 결정(10s + 1.5GB) 그대로 수용.

### 3.2 조그 세션 (UI ↔ PlayerCore)

- 조그 시작/종료 기존 그대로: 자동 일시정지 + `SetJog(true)`(오디오 디코드 스킵 =
  **완전 무음**), 종료 시 정밀 시크(프레임 반올림) 착지.
- **`JogTo(t)` 신설** — 40ms 디바운스 `SeekScrub`을 조그 주 경로에서 폐기:
  - m 아래 `jogTargetPts` 갱신 + 디코드 스레드 notify. 블로킹 0.
  - **전진**: `VideoLoop` 클록 게이트가 조그 중엔 `pts <= max(ClockNow(), jogTargetPts)
    + vPipeDelay + kVideoLead`로 재편 — 다이얼이 가리키는 프레임까지 디코드 전진, 이상은 hold.
  - **조그 중 videoQ push 생략**: 일시정지라 videoQ 3프레임 캡이 조그 전진을 막는다 —
    조그 중 디코드 출력은 jogRing 전용 push.
  - **역방향 (ring 안)**: 디코드 불필요, UI가 ring에서 즉시 표시. 디코드 스레드는
    게이트 hold로 정지.
  - **역방향 (ring 시작 이전)**: 기존 `SeekScrub` 폴백 (키프레임 시크 + 전진 디코드,
    랜딩 후 조그 재개). 블로킹 I/O라 기존 40ms 디바운스 latest-wins **유지** — 이 경로만
    시크. 판정: `target < ring.front().pts`(ring 비었으면 항상 폴백).

### 3.3 표시 경로

- **`JogFrame(targetPts, out)` 신설**: m 아래 videoQ + jogRing 합쳐 target 이하 최신
  프레임 1개 반환 (역순 스캔 — 최신부터 몇 개면 끝).
- `SyncVideoTexture`: 조그 중엔 `PopVideoFrame(clock)` 대신 `JogFrame` — 분기 1개,
  비조그 경로 무변경.
- 진행 표시/OSD: 기존 `jogTarget_` 표시 유지.

### 3.4 조그 종료

- 기존 `finishScrub` 그대로: 정밀 시크(프레임 반올림) → stage (a) ring 클리어 →
  정상 재생 복원. 휠 스크럽 세션(`wheelScrubbing_`)도 동일 세션 기계 공유.

## 4. 에지 케이스

| 케이스 | 처리 |
|---|---|
| fps 미지(0) | 프레임 인덱스 대신 pts 기반 — 다이얼이 시간 누적이라 자연 동작 |
| EOF | `jogTargetPts` = duration 클램프 (기존 UI clamp와 동일) |
| 폴백 시크 중 계속 돌림 | 기존 latest-wins + `seekInFlight` 스냅샷 기계 |
| wall-clock 파일 | 일시정지 클록 고정 경로 그대로 |
| ring 비었음 (오픈 직후/직후 시크) | 모든 역방향 = 폴백 시크 |
| 조그 중 오디오 | `jogging` 스킵 + paused 디바이스 = 이중 무음 보장 |

## 5. 변경 범위

- `engine/src/apps/ClientVPlayerApp.cpp` 단일 파일 (PlayerCore + UI) + 문서
  (docs/50 레저 갱신 + as-built 섹션, docs/23 §11.8.1 보강).

## 6. 검증

probe 스크립트 (기존 vplayer probe 관례, ffmpeg 합성 카운터 파일):
1. 전진 조그 = 프레임 번호 연속 증가 (키프레임 점프 없음)
2. GOP 내 역방향 = 즉시 표시 (폴백 시크 로그 0)
3. 링 시작 이전 역방향 = 폴백 시크 도달 후 조그 재개
4. 조그 중 오디오 링 불변 (무음 실측)
5. 조그 종료 = 정밀 시크 착지, 정상 재생 복원
6. 회귀: 일반 재생 / 정밀 시크 / 휠 스크럽 / ±1F 스텝

사용자 실측 항목: 조그 손맛, 링 경계 지연 체감(4K), 메모리 점유.

## 7. v2 역방향 자동 재생 (→ 구현됨, docs/50 §10)

역방향 자동 재생: 링을 역방향 케이던스로 자동 진행, 링 소진 시 이전 키프레임 시크 후
반복. GOP 경계 끊김은 불가피. v1 링 기계 위에 얹는다.

→ **구현됨 (2026-09-15)** — "<<" 토글 + 콘텐츠 fps 역보행 케이던스, 소진은 기존
폴백 시크 공유 펌프로 결선, 단일 출구(finishScrub)·인수 보존 규칙·PlayerCore 경화
2건(vSeekSeq 제2 bump, audioSkipBelow -0.05 슬랙) 포함. 검증 실측·관찰 3건은
docs/50 §10.4~§10.5.