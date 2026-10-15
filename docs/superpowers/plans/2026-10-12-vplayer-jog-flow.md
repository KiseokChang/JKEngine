# vplayer 조그 재설계 — 휠 연속 워크+관성 (#100) 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development.

**Goal:** 휠=연속 워크(틱=속도·초 계기)·마지막 틱 후 관성 감쇠·스냅 시크 단일 출구 — 사용자 결제(2026-10-12 "휠=연속 워크"·"진짜 어떻게좀 해봐").

**Architecture:** 기존 chase 펌프 재용(JogTo/링 히트) — 휠 세션을 "틱률→속도→타깃 누적"으로 재정의+관성 감쇠(quiet 120ms 이후 속도 감쇠 연장) — 점프성 틱 절식 소멸.

**Spec:** 사용자 결정+본 플랜 결정 원장(기각 시 즉시 수리).

## Global Constraints

- **원존 계약:** OpenPath 절단(휠 세션 소멸)·jogRing 폴백(±1F wedge)·finishScrub 단일 출구·"«"버튼(역방향 자동재생) 유지·노브 드래그(휠 세션과 동일 승계 계약)·idle 계약(조작 중만 활성)·캐논(Win 691/WSL 668/posix 304/폰 668 — 신설 어설션 0·등호 승계).
- **무음 계약(사용자 지시 2026-10-12):** 조그 세션(휠 워크·노브 드래그·«·관성 감쇠) 전 구간 = **오디오 무음**(디코드 스킵 유지+출력 뮤트 — 잔향 오디오 큐 플러시·세션 종료(스냅 시크) 후 재개 — 원존 SetJog(true) 스킵 경로와 통일).
- 커밋 계약 전부(명시 경로·IP 0건(-F)·push·amend 금지·트레일러)·빌드 ninja 3축·서브에이전트 스폰 금지·테스트 클립 = 엔진 소유(tmp/vpt2_test.mp4)만.

---

### Task 1: 휠 연속 세션+관성 (ClientVPlayerApp.cpp)

- ①세션 진입: 첫 틱=개시(SetJog(true)·jogWasPlaying_ 보존 — 기존 계약 원존) ②틱마다: **틱률 속도 추정**(최근 3틱 윈도우·초당 이동량 스무스)→jogTarget_ 누적+매 프레임 JogTo(기존 펌프 :3157 수형 재용) ③마지막 틱 후 **120ms 조용 → 관성 감쇠**: 속도 반감기 ~150ms로 타깃 연장(연장 총량 ≤ 잔속×0.5s 상한) 후 0 → finishScrub 스냅(단일 출구) ④역방향 동형(대칭·dir만) ⑤«/노드/재생 takeover — 기존 승계 계약 원존.
- [ ] **Step 1:** 구현
- [ ] **Step 2:** 빌드 3축 등호+라이브 smoke(tmp/vpt2_test.mp4 — 휠 시퀀스 캡처 2종·진행량 연속성 원문)
- [ ] **Step 3:** 커밋 `feat(apps): vplayer 조그 휠 = 연속 워크+관성 감쇠 (T1)`

### Task 2: WSL probe+docs 갱신

- 신설 probe `engine/tools/probes/wsl_jog_flow.sh`(wsl_* 수형): 재생→휠 시퀀스(send_input wheel — 기존 원문 수형)→관성 정지 후 pos 원문(연속성/역방향 검증·2런).
- docs: docs/89(vplayer as-built) 조그 원장 갱신 1행+docs/90 §0 부기.
- [ ] **Step 1:** probe 확장+런 2회
- [ ] **Step 2:** docs 커밋

## 사용자 게이트

1. 휠 조작 체감(부드러움·역방향 동일) = 사용자 선언(EYES).
2. 감쇠 매개(윈도우 3틱·반감기 150ms·quiet 120ms) = 재량 — 기각 시 즉시 수리.