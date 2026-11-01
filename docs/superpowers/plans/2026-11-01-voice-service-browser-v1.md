# 구두 서비스 분리+브라우저 구두 v1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans — 이 세션에서 inline 집행.

**Goal:** 구두(마이크+STT+/talk 통로)를 Foreground Service로 분리해 브라우저 창에서도 홀드 푸시토크가 작동하게 하고, 서 측 chat 어휘 3행으로 "브라우저 열어줘/URL/검색" 구두 조작 v1을 성립시킨다.

**Architecture:** JkVoiceService(마이크 FGS·기본 STT 엔진·/talk POST 유일 소관) — 두 Activity의 [구두] 홀드 터치가 START/STOP 인텐트로 서비스에 릴레이, 결과/회신은 in-app broadcast로 양 Activity에 전달(MainActivity=채팅 로그 진실원, BrowserActivity=토스트). 서버 쪽 = chat 어휘 3행(브라우저 개봉/URL/검색)+Termux am start 스폰 leg(#117 선례).

**Tech Stack:** Kotlin 1.9.24·SpeechRecognizer(b3f1970 뼈대)·FGS microphone type·jkdesktop C++(ChatRouteTurn/ChatAction — posix phone engine)

**Spec:** `docs/superpowers/specs/2026-11-01-voice-service-browser-v1-design.md`

## Global Constraints

- 매 합성 단계 끝에 gradle compile PASS(빌드 불성립 커밋 금지).
- push-to-talk 계약 유지(홀드=듣기·떼기=마침 — 평시 녹음 0·사생 fail-closed).
- 마이크 FGS 시작은 포그라운드 인터랙션에서만(Android 14+ SecurityException 원존).
- APK 폰 덮어쓰기 전 jkengine-89 세션 통지 계약(실행 중 probe 편집 금지 원존 동일).
- 서버(jkdesktop) 재기동 = 사전 통지(/talk 1~2분 끊김 — 조율 계약).
- `git add -A` 금지·빌드 산출물 무추적(D4)·probe는 설치 금지(D5).

---

### Task 1: JkVoiceService — 마이크 FGS 뼈대+기본 STT+릴레이 계약

**Files:** Create `JkVoiceService.kt` / Modify `AndroidManifest.xml` / Modify `MainActivity.kt`(릴레이로 교체)

**Interfaces:** 서비스 액션 `ACTION_START`(듣기 개시)·`ACTION_STOP`(마침) — 브로드캐스트 `jk_voice_status`(partial/상태)·`jk_voice_reply`(최종 텍스트+/talk 회신 요약). MainActivity의 [구두] 홀드 터치 = 인텐트 릴레이로 전환.

- Step 1: manifest — `<uses-permission FOREGROUND_SERVICE>`, `FOREGROUND_SERVICE_MICROPHONE`, `POST_NOTIFICATIONS`, `<service android:name=".JkVoiceService" android:foregroundServiceType="microphone" android:exported="false">`, 알림 채널 문자열.
- Step 2: JkVoiceService — FGS 시작(포그라운드 인터랙션 전제)+기본 STT(b3f1970 sttListener 논리 재용)·START/STOP 처리·최종 텍스트 → /talk POST(기존 doTalk/postTalk 사슬 이동)→ 회신 브로드캐스트. Whisper 경로는 서비스화 제외(스코프 원존 — MainActivity 유지).
- Step 3: MainActivity — [구두] 홀드 → 서비스 인텐트 릴레이(Down=START/Up=STOP), 인식 엔진이 "기본"일 때만 릴레이(Whisper 모드=기존 자체 사슬), receiver로 partial→상태행·reply→채팅 로그.
- Step 4: BUILD PASS→커밋.

### Task 2: BrowserActivity [구두] 진입+브라우저 표면

**Files:** Modify `BrowserActivity.kt`
- Step 1: 조작행에 [구두] 버튼(토일 라이프사이클 동형 릴레이)+receiver로 회신 토스트 표기.
- Step 2: BUILD PASS→커밋.

### Task 3: 서버 쪽 chat 어휘 3행+am start 스폰 leg (posix phone engine)

**Files:** engine 쪽 chat router/action(C++), 폰 엔진 재배포.
- "브라우저 열어줘"→개봉 / URL 포함→`-e jkUrl` / "○○ 검색해줘"→구글 검색 URL(매개 1종).
- 스폰=Termux `am start`(#117 termux-tts-speak 선례 — 15s 마감·실패무시·rc=137 원존 재용).
- WSL 빌드+캐논 회귀 없음 확인 → 폰 엔진 배포는 서버 재기동 통지 계약(jkengine-89).
- Step: 어휘 3행+ChatAction 추가→buildwsl 빌드 PASS→selftest 회귀 0→커밋→폰 배포 런.

### Task 4: probe+docs as-built+EYS

- APK 반입(md5 등어)+폰 어휘 3행 실측 원문+jkweb 재기동 원장(docs/90 함정 원존 — jkweb이 ChatRouter 내장).
- docs/101 as-built+기억창고 갱신 74.
- EYS: 브라우저에서 홀드 구두("브라우저 닫아줘"류)→실 발화·서비스 알림 체감·사생 축.