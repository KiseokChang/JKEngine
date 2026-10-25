# 스펙: JK APK — 안드로이드 네이티브 구두 앱 (#114, 2026-10-25)

> **상태: 확정 — 사용자 결정(C=진짜 안드로이드 앱·B=전부 내장 APK)·설계 승인 "진행"
> (2026-10-25 chat). 재량 = 기각 시 즉시 수리.**

## 목적

폰 홈 화면의 **JK 아이콘** = 독립 안드로이드 앱 — **전사·대화는 APK 단독**
(APK 내장 whisper.cpp NDK+모델), **OS 조작은 폰 jkweb 사슬 재용** (DeX 세션
전제 — 미기동 시 정직 표기). DeX 큰화면 OS를 구두로 조작하는 사용자 그림의
독립 표면.

## 확정 원장

- **D1 (C)**: 진짜 안드로이드 앱(APK — 별도 설치) — ImGui 창 표면과 별개.
- **D2 (B→v1 전환 — 사용자 2026-10-25 지시 "음성인식은 다른 좋은 앱을 쓰면
  되니 우선 타이핑하는 형태로")**: v1 APK = **얇은 타이핑 APK** — 전사(STT)는
  안드로이드 IME 음성입력 등 외부 앱 몫. APK는 텍스트 입력+[보내기]+기록 →
  localhost:8090/talk. whisper NDK 내장+모델 번들 = **v2 백로그**(삭제 아님).
- **D3 조작 사슬 재용**: 전사 텍스트 → `POST http://localhost:8090/talk`
  (jkweb — ChatRouteTurn 원존·폰 서버가 chat.json 구성 시 cloud 승격까지) —
  APK에 새 판정 계층 0("STT 텍스트==타이핑 텍스트" 규약 승계).
- **D4 무추적**: SDK/NDK·Gradle 캐시·모델 ggml·빌드 산출물 APK 전부 무추적 +
  **빌드 산출물 커밋 금지**("기계만 커밋" 원칙). 조달기 스크립트만 커밋.
- **D5 debug 서명 사이드로드 설치** — v1·스토어 출고 비목표.
- **D6 v1 모델 번들 = 비-목표(v2)** — tiny-ko asset 전개 절차는 v2 원장
  행으로 보존.
- **D7 v1 품질 승계 = 비-목표(외부 STT 앱 몫)** — whisper 쪽 품질 수형(
  어휘 prompt 힌트 등)은 데스크톱 [구두](#112)에만 적용.

## 1. 빌드 사슬 (Win 머신)

- `engine/tools/probes/install_android_sdk.sh` — 멱등 조달기: Android
  commandline-tools+platform-tools+build-tools+platform 34(또는 기기 호환
  실측)+NDK(r28 계열 — ggml C17 호환)·무추적 dir(`build/android-sdk` 계열)
 ·sha256 기록·부재 시에만 네트워크.
- APK 모듈 위치: `engine/android/jkapk/` — 소스(Kotlin/manifest)·JNI
  CMake(vendored whisper.cpp 재사용 — `../third_party/whisper.cpp` 상대
  참조·whisper JNI lib 1종)·Gradle wrapper. **전체 엔진 빌드(ninja)와
  무접촉**(별도 Gradle 빌드 — 캐논 무영향).
- 서명: debug keystore 자동 생성(Gradle debug — 자동).

## 2. 앱 본체

- Kotlin+최소 UI(v1 얇은판): **텍스트 입력 1줄+[보내기]**(IME 음성입력 =
  외부 STT 앱의 정상 유입 경로 — 사용자가 좋아하는 앱으로 dictate)·대화
  목록([나]=보낸 텍스트·[시스템]=답신/오류·append-only — chat 앱 문화 승계)·
  상태행(서버 연결 정직 표기).
- 조작: HTTP(POST /talk — 타임아웃)·서버 부재=정직("서버 미기동 — DeX
  세션 필요").
- v2 원장행(비-목표 보존): whisper NDK JNI+마이크 AudioRecord 16k 모노
  정규화+모델 asset — #3117 격리 근거 원문(앱 자체 프로세스 = 격리 본질
  동일)은 v2 리포트에 승계.

## 3. 조작 사슬 (폰 측 — 기존 원존)

- jkweb 8090(`/talk` — HandleTalk — ChatRouteTurn·launch/focus/close/list
  동작) — **폰 DeX에서 서버 필요** = DeX 세션 전제 계약(스폰: 기존 배포
  드라이버 부팅 계약 유지 — 폰 서버가 살아있으면 jkweb도 기동(기존 배치
  절차 원존)·jkweb 계약 = jkweb --port 8090 --bind loopback(루프백 기본 —
  LAN 무인증 노출 수수 원장 승계)·**localhost 조작만** — APK가 쓸 것.
- 조작 결과(창 생성)는 DeX 화면(기존 사슬)·APK 기록에도 답신 원문.

## 4. 테스트·게이트

- **APK 결정론 어설션 = 빌드 계약**: gradle assembleDebug rc=0+APK 산출+
  **설치(adb install -r) rc=0+런치(adb shell am start) rc=0** — probe 원문.
- v1 결정론 = 빌드/설치/런치 rc+POST 조작 원문 — STT/전사 축은 v2(외부
  앱 몫이라 앱이 아예 만지지 않는다).
- 캐논: 4축 등호 유지(엔진 코드 무접촉 — APK는 별도 dir·별도 빌드 —
  ninja/캐논 계보 무변조).
- 폰 실측: 설치·런치·구두 전사 1회·조작(deX 서버 동기 시)·jkweb 미구동
  정직 표기 — probe(scripted — 촬영 원문).
- EYES: 사용자 APK 홈 아이콘 터치→구두 발화→DeX 창 실행 체감.