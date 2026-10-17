# vplayer 백워드 조그 — 역방향 선디코드 프리필 (#101) 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development.

**Goal:** 뒤로 느린 조그 틱 = 링 히트(프레임 정밀 — FWD와 동일 체감) — 역방향 세션 개시 시 백그라운드 프리필이 링의 역방향 사전 확장.

**Architecture:** PlayerCore 백그라운드 프리필 워커(역방향 GOP 단위 — 각 GOP 끝점 시크→그 GOP 순방향 디코딩→링 **앞 삽입**(순서 보존)) — 세션 개시 트리거·유계(링 용량 계약·cancel-aware·세션 종료 정지)·링 히트 경로 원존 재용.

**Spec:** 사용자 실측 기각(2026-10-12 "backward는 프레임 단위로 안되는 듯 — 버퍼가 없나? 그때그때 디코딩하듯이") + 진단 원장(jogRing=재생 중 수집 16s — 역방향 폴백 wedge=신디코드 지연 노출)·기각 시 즉시 수리.

## Global Constraints

- **원존 계약 무변조:** 링 용량(kJogRingMaxSecs 16/6GB — user-approved 2026-09-24)·폴백 wedge(±1F)·finishScrub 단일 출구·무음/관성/가속 계약(#100)·idle 계약·캐논(Win 691/WSL 668/posix 304/폰 668 — 신설 어설션 재량·posix 하네스 사유).
- **디코드 경계:** 프리필 워커가 세션 디코드 경로와 경쟁하지 않는 수형(별도 디코더 컨텍스트 or 재사용 정책 — **원문 실측 후 선정·리포트 명시**)·프리필이 재생/세션 UI을 막지 않음.
- 커밋 계약 전부(명시 경로·IP 0건(-F)·push·amend 금지·트레일러)·빌드 ninja 3축·서브에이전트 스폰 금지·테스트 클립 = 엔진 소유(tmp/vpt2_test.mp4)만.

---

### Task 1: PlayerCore 백워드 프리필

- ①트리거: 역방향 세션 개시(첫 뒤 틱·«·노브 역방향) → 프리필 워커 개시 ②동작: 현재 pos에서 뒤로 GOP 단위(시크 keyframe→GOP 전체 순방향 디코드)→**링 앞 삽입**(deque front·순서 정합 — 2r/링 구조 원문 정합) ③유계: 링 용량 계약 소비+세션 종료/방향 전환/OpenPath = 즉시 정지(cancel-aware — 스캔 취소 수형 재용 가능) ④히트: JogFrame의 링 히팅이 프리필 프레임을 그대로 소비(경계 원문 정합) ⑤폴백 크리프 = 프리필 밖 graceful 유지.
- [ ] **Step 1:** 구현
- [ ] **Step 2:** 빌드 3축 등호
- [ ] **Step 3:** 라이브 smoke(tmp/vpt2_test.mp4 — BWD slow 시퀀스: 프리필 중 착지 err 원문+프리필 채움 속도 원문·캡처 2종)
- [ ] **Step 4:** 커밋 `feat(apps): vplayer 백워드 조그 프리필 — 역방향 선디코드 (T1)`

### Task 2: probe+docs

- probe `engine/tools/probes/wsl_jog_flow.sh` 확장: **BWD SLOW leg 신설**(프리필 히트로 FWD 동형 err 원문+프리필 밖 크리프 graceful 원문·2런)+FWD/REV 회귀 원존.
- docs/50 원장 갱신(백워드 선디코드 원장 — 진단/수리/계약 2-3행)+docs/90 §0 부기.
- [ ] **Step 1:** probe 확장+2런
- [ ] **Step 2:** docs 커밋

## 사용자 게이트

1. 뒤로 조작 체감(프레임 정밀·FWD 동일) = 사용자 선언(EYES).
2. 프리필 깊이/디코더 정책 = 재량 — 기각 시 즉시 수리.