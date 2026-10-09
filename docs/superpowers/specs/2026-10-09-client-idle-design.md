# 스펙: 클라 idle 스핀 수리 — FrameDirty 진원 수리 (#89, 2026-10-09)

> **상태: 확정 — 컨트롤러 재량 결정(사용자 "재량껏 쭉쭉 진행하라" 지시 —
> 기각 말하면 즉시 수리).**
> 발단: D1 수리 리뷰 I-2 — 폰 gallery 클라 95.9% CPU 스핀 → 진단 스파이크
> (.superpowers/sdd/2026-10-09-clt-spin/spike-report.md) 원인 확정.

## 결함 원장 (스파이크 영수증)

**3요소 관용구**가 docs/78 activity 게이트를 무력화:
1. `JKClientApplication.cpp:309 DrainTimerChannel→activity=true` — 타이머
   틱 그 자체가 렌더 유발.
2. 16개 ImGui 앱 `PreProcessMessage`: `Timer 이벤트→frameDirty_=true`
   (gallery :127, files·notes·chat·vplayer·browser·shot·settings 등 16곳).
3. `RenderOverlay` 끝 무조건 `frameDirty_=true` — 렌더 후에도 더티 유지
   (자기유지).
폰 실측: 갤러리 클라 단독 **100.1~102.6%**(메인 스레드)·19fps 무변화
풀코어·프레임 작업 ≈52ms>16ms → 페이싱 `SDL_Delay(1)` 폴백 붕괴. 서버
동반 12.6→35.4%(CommitFull 합성 유발). 대조군 정상: terminal 7.8~8.9%
(이벤트/더티 구동)·taskbar <4%.

## 설계 (확정)

**원리: "레트야 하는 이유가 있을 때만 그린다."** 타이머 틱은 활동이
아니다 — 아래로 대체한다.

1. **게이트 수리(T1)** — `DrainTimerChannel>0`은 activity 마킹을 하지
   않는다(타이머 채널은 배송만, 활동 아님). 폴백 1s(HasDirtyWindows
   게이트)는 그대로 유지. 입력·에이전트·툴콜·테마는 활동 유지.
2. **관용구 스윕(T2)** — 16 ImGui 앱의 `Timer→frameDirty_=true`를
   **조건화**: 앱의 타이머 콜백이 이번 틱에 실제로 바뀌는 내용(초시계
   1Hz·비동기 썸네일 도착·재생 진행 등)이 있을 때만 더티. 내용 없는
   틱은 더 없음.
3. **자기유지 소멸(T2 동일 배치)** — `RenderOverlay` 끝 무조건
   `frameDirty_=true`는 앱이 다음 프레임을 **계속 필요로 하는 상태**
   (진행 중 애니메이션·재생)일 때만. gallery에 예: 비동기 thumb 로드
   진행 중, 전환 애니메이션 없음(뷰모드 스왑은 즉시 더티 1프레임이면
   충분).
4. **vplayer/terminal 예외 보존** — terminal은 이미 이벤트 구동(대조군
   실측) — 스윕 대상에서 제외 확인. vplayer 재생 중은 프레임마다 더티
   (동영상) 유지 — 스윕에서 "재생 중에는 유지" 조항.

## 결정 (컨트롤러 재량)

- **① FrameDirty 진원 수리 확정** — 타이머 간격 승격(②)·페이싱 수리(③)
  는 폰에서 무효(스파이크 판정: 52ms>16ms라 간격 조정 본질 아님) —
  ③은 hygiene로만.
- **폴백 1s 유지+현재 게이트(HasDirtyWindows) 유지** — UI 변화 없는
  idle에서의 무렌더는 설계상 의도(정적 UI는 그릴 이유 없음). 텍스트
  커서 블링크는 입력 이벤트 중에만 렌더되므로 입력 중 60fps 유지 —
  무입력 커서 블링크 정지는 수용(기각 시 1Hz 더티 조건으로 수리).
- **UX 위험 수용 원장**: 앱 idle에서 1회성 더티만 그린다 — 호버 등
  마우스 이동은 입력 이벤트라 즉시 렌더. WSL/폰 실기기 육안 게이트로
  검증.

## 비-목표
- 서버 측 CommitFull 합성 최적화(D-배열 docs/85 — 별개 라인).
- 폰 서버 프로세스 CPU 청구(D1 리뷰 I-2 잔여 — 클라 스핀 소거 전
  미실측 유지).
- 터미널/애니메이션 앱의 스코프 축소 불요 — 이벤트 구동은 이미 옳다.

## 성공 판정
- 폰 idle(앱 열어두고 무입력 5s 이상): 갤러리 클라 CPU < 5%(현
  100%)+[cpustat] frames/s < 3(현 19). WSL 동형 실측. terminal/taskbar
  회귀 없음. 캐논 전 축. **사용자 폰 육안 결제로 라인 봉합.**