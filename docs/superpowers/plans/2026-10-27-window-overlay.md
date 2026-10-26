# 창 번호 오버레이 (#118) 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development.

**Goal:** "창 번호 보여줘"→각 창 제목줄 좌측 번호 배지(compositor 오버레이·10분 소멸)→"창 3"→focus_window — 구두 창 지시 v1.

**Architecture:** 서버 compositor 임시 레이어(기존 오버레이 수형 원존 — 새 창 아님·이벤트 통과 투명)·`overlay_numbers` 도구 1종·어휘 즉발 2-3행.

**Spec:** docs/superpowers/specs/2026-10-27-window-overlay-design.md (D1-D5)

## Global Constraints

- 이벤트 통과 투명(클릭 불가 — D2)·창 단위만(요소 그리드=v2)·fail-closed(창 없음=stub)·10분 타이머.
- 캐논(4축) 승계 — 신설 계보 **3a** 증분만.
- 커밋 계약 전부·폰 주소 인쇄 금지·런 중 편집 금지.

---

### Task 1: 서버 오버레이 배지+도구

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp`(+h) — `overlay_numbers` 도구(1건 — agentctl 등록 표에 append·게이트 = 캡처 쌍 계열 "server(flip)" 재판정 — 도구 표 원존 실측)·compositor 배지 렌더(기존 오버레이 수형 원문 실측 후 병합 — z 최상단·10분 타이머·idempotent 갱신)·**3a 부품 단정**(배치 순서 z 등호·이중 켬 소멸 타이머 원존)
- Test: `engine/src/main.cpp` 3a 3건 또는 posix 어셜션 계보 확인(posix 축 원존 실측)
- [ ] **Step 1:** 접점 실측(오버레이 수형·도구 표)→구현·빌드 3축 등호 2. 커밋 `feat(apps): 창 번호 오버레이 배지+도구 (T1)`

### Task 2: 어휘+probe+폰 배포+docs

**Files:**
- Modify: `engine/src/apps/ChatRouter.cpp`(+h 필요시 — "3번 창" 파싱은 정확매치 어휘 한계 — **숫자 포함 발화의 즉발 = 정규식성 — D3의 "창 3""3번 창"은 접미수 판정 부품**(순수 — 3a 재용)·kVoicePrompt 동기
- Modify: `engine/tools/probes/wsl_voice.sh`(오버레이 E2E 세그먼트)+`engine/tools/probes/phone_voice.sh`(동형)·폰 배포 런(기대 캐논 승계)
- Create: docs/90 §7.15(어휘+오버레이·D1-D5·EYES)+§4.1 부기
- [ ] **Step 1:** 어휘+부품 2. probe+폰 런 3. docs 4. 커밋 분리

## 사용자 게이트

1. 폰에서 "창 번호 보여줘"→배지→"창 3"=포커스 체감 = EYES. 2. D1-D5 = 재량 — 기각 시 즉시 수리.