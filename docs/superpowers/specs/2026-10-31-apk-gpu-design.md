# 스펙: #122 APK GPU 가속 전사 (2026-10-31)

> **상태: 확정 — 사용자 결제("APK인데 GPU 안 쓸 이유 없죠" — 2026-10-31) —
> 진원 원장(tiny-ko 빈 인식→small 전환·폰 CPU 30-47s 원장)에서 온 라인.
> 재량 = 기각 시 즉시 수리.**

## 목적

APK의 내장 음성 인식(small-komixv2)을 **폰 GPU(Vulkan)로 가속** — 30~47s
변환을 목표 지연(실측 후 확정 — 리서치 문턱 체감 5s 이하 지향)으로 단축.

## 재량 결정 원장 (기각 시 즉시 수리)

- **D1 Vulkan backend 재조달**: vendor subset 확장(ggml-vulkan+SPIR-V
  shaders·glslc=NDK 내장 활용) — 조달 규약 원존(소스만 커밋).
- **D2 런타임 검출 fail-closed**: Vulkan instance 생성 실패/기기 미지원 =
  **CPU 폴백**(원존 계약 — 현재 동작 유지)·"GPU로 전환"은 정직 표기
 (상태행 "GPU 가속 (몇 초급)" vs "CPU 변환").
- **D3 데스크톱/Termux 무접촉**: GPU leg는 APK 전용(CPU 원존 유지 — Termux
  Vulkan 불가 원장 #2370).
- **D4 모델 불변**: small-komixv2 asset(456MB판·a428f646 핀) — GPU/CPU
  공용(ggml 파일은 backend 무관).
- **D5 APK 크기**: Vulkan shaders(+몇 MB)·CPU 폴백 유지(~456MB 유지 밴드).
- **D6 검증 원존**: 전사 비결정 원칙 — 속도 판정 = 실측 원문(같은 fixture를
  CPU/GPU 각각 변환 지원 대조)·품질 = EYES.

## 1. 구현 단위

- jni/CMake: GGML_VULKAN=ON(또는 상당 — vendored 재판정·glslc NDK 지정)·
  gradle ABI 유지(arm64-v8a)·libvulkan 연결 안드로이드 로더.
- MainActivity: 초기화 시 Vulkan 검출(instance/physical device 열거 —
  성공=GPU leg·실패=CPU)·상태행 표기 leg("GPU 가속"/"CPU")·전사 경로
  분기(1곳 — JNI 인자 확장 or 환경 스위치 — 구현 재량).
- 검출 실패의 정직 원문 리포트.

## 2. 테스트·게이트

- 빌드 rc=0(봉인)·폰 설치(D5 사용자 손)·**변환 속도 실측 원문**(동일
  발화 CPU 대 GPU 지원 각각 — 지연 표)·품질 체감 = EYES.
- CPU 원존 회귀 0(GPU 실패 시 동작 = 기존과 동일 — 원존).

## 3. 비-목표

- OpenCL(주류 아님)·폰 발열/배터리 최적화·데스크톱(Termux) GPU leg·
  Windows APK 런타임 렌더 변경.