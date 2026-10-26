# 구두 어휘 확장 v1.5 (#115b) 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development.

**Goal:** LLM 없이 즉발되는 구두 어휘를 재생(vplayer/music)+알림 도메인으로 확장 — 기존 도구 접점 재용·무인자 강제.

**Architecture:** ChatRouter kRouteTable/kAliasTable 확장+ChatAction 소비 경로 재용(새 파서 0)+kVoicePrompt 갱신+2y selftest+probe.

**Spec:** docs/superpowers/specs/2026-10-27-voice-vocab-design.md (D1-D6)

## Global Constraints

- 새 파서/새 도구 금지(D3 — 도구 부재 어휘=배제+원장행)·무인자 우선(D1 — 인자 조작=LLM 몫)·stub 폴백 원존.
- 캐논(Win 856/WSL 833/posix 304/폰 833) — 2y 증분만·기존 등호.
- 커밋 계약 전부·폰 주소 인쇄 금지·런 중 편집 금지.

---

### Task 1: 어 표 확장+2y

**Files:**
- Modify: `engine/src/apps/ChatRouter.cpp` — kRouteTable/kAliasTable 신설 어휘(재생계+알림 — 접점 도구 실측: vplayer 재생 도구 부재 시 원장행으로 배제·알림 = notify 도구 접점)·`ChatRouter.h`의 ChatAction 대차 재판정(기존 액션 재용 — 가능시 새 enum 금지)
- Modify: `engine/src/voice/SttVoice.cpp` — kVoicePrompt 갱신(D4)
- Test: `engine/src/main.cpp` **2y** — ①신설 어휘 N건 → ChatAction 등호 ②기존 어휘 회귀(정확매치) ③별명 우선순위(이중 해석 방지) ④무인자 강제(인자 포함 발화 ≠ 신설 어휘 매치) — **2y ≥ 8건**
- [ ] **Step 1:** 접점 실측(도구 존재 표)→어휘 표+2y·빌드 2축 등호 2. 커밋 `feat(apps): 구두 어휘 확장 — 재생+알림 도메인 (T1)`

### Task 2: probe+폰 배포+docs

**Files:**
- Modify: `engine/tools/probes/wsl_voice.sh` — 신설 어휘 E2E 세그먼트(발화→동작 원문+회귀 0)·`engine/tools/probes/phone_voice.sh` — 동형+폰 배포 런(기대 캐논 승계)+재실측
- Create: docs/90 §7.13(어휘 원장·접점 표·EYES)+§4.1 부기
- [ ] **Step 1:** probe+폰 런 2. docs 3. 커밋들(분리)

## 사용자 게이트

1. 폰에서 신설 어휘 발화 체감 = EYES. 2. D1-D6 = 재량 — 기각 시 즉시 수리.