# 구두 응답 TTS leg (#117) 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development.

**Goal:** 폰 구두 명령의 답신을 termux-tts-speak로 소리로 읽어주는 leg — 구두 표면 답신 턴 1회.

**Architecture:** ClientChatApp 구두 턴 origin 마킹+DrainVoice 답신 도착 트리거+#ifdef __ANDROID__ spawn(조달 0)+2z 단정 부품.

**Spec:** docs/superpowers/specs/2026-10-27-tts-response-design.md (D1-D6)

## Global Constraints

- 실패 무시(도구 응답 무영향 — 저널 leg 동형)·타이핑 답신 TTS 금지(D3 — 무음 존중)·T4/R5 치환 계약 원존.
- 캐논(Win 873/WSL 850/posix 304/폰 850) — 2z 증분만.
- 커밋 계약 전부·폰 주소 인쇄 금지·런 중 편집 금지.

---

### Task 1: TTS leg+2z

**Files:**
- Modify: `engine/src/apps/ClientChatApp.cpp`(+.h) — 구두 턴 origin 표지+D2 opt-out+D4 절단 부품+spawn(#ifdef — VoiceCaptureTermux 스포 계열 원존 수형 — 실패 무시 3행)
- Test: `engine/src/main.cpp` **2z** ①-③(스펙 §1)
- [ ] **Step 1:** 2z 작성(fail 확인) 2. 구현·빌드 2축 등호 3. 커밋 `feat(apps): 구두 응답 TTS leg — termux-tts-speak (T1)`

### Task 2: 폰 배포+probe+docs

- 폰 배포 런(기존 드라이버 승계 — 기대 폰 캐논 = 850+2z 승계)+폰 probe 세그먼트(phone_voice.sh — 발화→답신→speak 원문+TTS 지연 원문 — 미접속=BLOCKED 원장)·docs/90 §7.14(EYES: 폰 답듣기 체감)+§4.1 부기.
- [ ] **Step 1:** 폰 런 2. docs 3. 커밋 분리

## 사용자 게이트

1. **폰에서 답을 듣는 체감** = 사용자 선언만 결제(EYES). 2. D1-D6 = 재량 — 기각 시 즉시 수리.