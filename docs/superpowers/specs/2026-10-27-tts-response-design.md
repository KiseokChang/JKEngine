# 스펙: #117 구두 응답 TTS leg (폰 — 2026-10-27)

> **상태: 확정 — v2 재판정 원장(리서치 §5 — "TTS 첫 단어 ≤1s" 문턱 근거)+
> 사용자 자율 지시("쭉쭉 진행·재량 2026-10-27"). 재량 = 기각 시 즉시 수리.**

## 목적

폰에서 구두 명령의 **답을 소리로 듣는다** — 구두 표면(chat [구두])에서 답신이
도착하면 `termux-tts-speak`(Termux:API — 조달 0·사용자 권한 계통 이미 확정)
으로 읽어준다. 사용자 그림(구두로 OS 조작)의 완성 축.

## 재량 결정 원장 (기각 시 즉시 수리)

- **D1 폰 전용**: `#ifdef __ANDROID__` — termux-tts-speak CLI 스폰
 ($PREFIX/bin·VoiceCaptureTermux 원존 수형 — 접미 pid 분리 파일 불요 —
  스트림 폐기). Win/WSL은 v1 끔(SAPI 조달 = 별도 라인).
- **D2 기본 켬+opt-out**: 폰에서 구두 발화의 답신 = 기본 TTS(음소거 아님 —
  UX 원칙)·`VOICE_TTS=0` env = 끔(fail-closed 원존의 opt-out 변형 —
  원장행).
- **D3 트리거 = 구두 출발 발화의 답신만**: R5 치환 경로(워커 우편함 —
  ClientChatApp DrainVoice)에서 구두 시작 발화가 찍힌 턴의 답신 도착 시
  1회. **타이핑 발화의 답신은 TTS 아님**(무음 존중 — 조작의 소리 아님).
- **D4 쓰는 문언**: [시스템] 답신의 reply 원문(전사가 아니라 답 — guide/앱
  실행 문언)·길이 상한(~120자 — 긴 안내문은 절단 후 말미 "…" — 1행 규약).
- **D5 실패=무시**: termux-tts-speak 부재/실패 = 조용히 스킵(UI 무영향
  — 저널 leg D1 계약 동형)·재시도 없음.
- **D6 계보**: selftest **2z** — TTS 결정 부품(문언 절단·구두 출발 판정)만
  단정·스폰 어셜션 금지(비결정 원존).

## 1. 구현 단위

- `ClientChatApp.cpp` — 구두 턴 origin 표지(StartVoice 시 턴 마킹)+DrainVoice
  답신 도착에서 #ifdef __ANDROID__ TTS 스폰(별도 스레드 — 워커 재용·
  termux-tts-speak args Dq 계약)·env opt-out 원존.
- selftest 2z: ①답신 절단(121자→120+"…" 등호) ②구두 출발 판정(마킹 턴만
  TTS — 타이핑 턴 부정) ③opt-out env 판정 부품 — 3건.

## 2. 테스트·게이트

- 캐논 승계(4축 — 2z 증분)·probe = 폰 E2E(발화→답신→speak 원문 —
  `[TTS] speak` 행 + 실패 원장행)·EYES: **폰에서 답을 듣는 체감**.
- 지연 판정 원문(termux-tts-speak 개발력 — 리서치 문턱 1s 대조).

## 3. 비-목표

- Win/WSL TTS(SAPI — 별도)·TTS 톤/속도 설정·구두 외 표면(타이핑 답신)·
  TTS 큐(연속 발화 처리 — v1 단건).