# 구두 서비스 분리+브라우저 구두 v1 — 설계 (2026-11-01, #124 후보)

## 1. 사용자 지시 원존

- "구두로 브라우저를 조작하는게 가능할까요?" → 3단계 구조 합의(개봉·검색 →
  페이지 조작 → AI 판단 조작) — **사용자 "좋아요"**.
- "JK앱에 겹쳐 있어서 구두 처리가 어렵지 않아?" → 지적 채택 — **구두를 앱의
  백그라운드 서비스로 분리**(근본 수형), 브라우저 창에서도 구두 작동.
- 착수 순서 합의: ①구두 서비스 분리 ②브라우저 구두 1단계(개봉·검색 어휘).

## 2. 기존 조달 원존 (재용 — 조달기 0 원칙)

- 구두 사슬: STT→`POST http://localhost:8090/talk`→jkweb→ChatRouteTurn(계량
  라우터+LLM 폴백) — #112/#115b/#121 v2/dc6b0fc(별명 접미 우선) 계보.
- SpeechRecognizer 뼈대: 음성 라인 세션(jkengine-89)이 jkapk 잠금 중 작업 중
  (MainActivity.kt+AndroidManifest) — **의존 원존: 본 라인 T1은 그 판 위에서
  개시(잠금 복귀 커밋 SHA를 기점으로 재판독 후 수형)**.
- am start 스폰 선례: #217 #117 TTS leg(termux-tts-speak 스폰·15s 마감·실패무시)
  — 서버(Termux) 쪽 `am start` 스폰 동형.

## 3. 서비스 계약 (판정 원문 조사 2026-11-01)

- 마이크 foreground service: `foregroundServiceType="microphone"`+
  `FOREGROUND_SERVICE_MICROPHONE`+`RECORD_AUDIO`(while-in-use).
- **뒤에서(bkgrd) 서비스 시작 = SecurityException**(Android 14+) — 시작 트리거는
  반드시 포그라운드 인터랙션(사용자 [구두] 터치·앱 개봉 중 버튼) — fail-closed 원존.
- 앱이 화면 밖(두 Activity 모두 paused) → 마이크 정지(사생 축 정직 계약 —
  fail-closed: 화면 안에서 듣는 것만), 복귀 시 재개.
- 공지(POST_NOTIFICATIONS) 권한 + 알림 채널 1종(FGS 소유 안내 — Android 13+).
- SpeechRecognizer 사용은 서비스 컨텍스트 가능(RecognitionListener).

## 4. 아키텍처 (이층)

```
[JkVoiceService(포그라운드 FGS — 마이크+STT 전사+POST /talk 통로 유일 소관)]
        ▲ 모든 창(=MainActivity·BrowserActivity)이 [구두] 토글 버튼으로 시작/정지
[MainActivity]                       [BrowserActivity(WebView)]
  ├ 구두 상태행(서비스 바인딩 상태 표기)   ├ 주소창·측정·엔진 토글(기존 T1-T5)
  └ 채팅 로그(POST /talk 회신 3면)        └ [구두] = 서비스 토글(신설)
```

- 전사 원문·채팅 로그는 MainActivity 화면에 유지(진실원 유지) — 서비스가
  /talk 회신을 브로드캐스트로 두 Activity에 전달(수신자 1종 — chang.jkapk 내부
  broadcast, exported=false).
- 단일 마이크 소관 소각: MainActivity 자체 AudioRecord/STT 경로는 서비스로 이동
  — 이중 마이크 개방 금지(기존 마이크 해제 단일 소관 계약 #121 승계).

## 5. 브라우저 구두 v1 어휘 (1단계 — 서버 쪽 chat 어휘 3행+개봉 스폰)

| 발화 예 | 동작 | 배선 |
|---|---|---|
| "브라우저 열어줘" | BrowserActivity 개봉(최근 주소 그대로) | ChatAction→am start 스폰 |
| "youtube.com 열어줘" / "브라우저로 https://… 열어줘" | URL 개봉 | am start -e jkUrl |
| "○○ 검색해줘"(브라우저 모드) | 검색 URL 개봉(기본 구글 — 매개 1종) | am start -e jkUrl "https://www.google.com/search?q=…" |
- 스폰 계약: Termux `am start -n chang.jkapk/.BrowserActivity [extras]`(#117
  스폰 선례·15s 마감·실패무시·rc=137 원존 재용) — 서버 쪽 leg, APK 재빌드 불요
  (BrowserActivity에 intent extras 수취 leg만 동봉).
- BrowserActivity: singleTask(launchMode) + onNewIntent로 창이 살아 있어도
  URL이 교체 개봉(재생성 금지 — 측정 상태 보존 계약 승계).

## 6. 라인 밖 (원장행)

- 2단계(페이지 안 입력·버튼 via broadcast+JS)·3단계(LLM 페이지 판단 루프) —
  본 라인 폐곡 뒤 별도 스펙.
- 결제 홉아웃+사람 하드게이트 — #123 원존 그대로.

## 7. 판정 게이트 (EYES)

1. 브라우저 앞에서 구두("검색해줘") 발화 → 검색 개봉 실측(사용자 선언만)
2. 앱 화면 밖 마이크 정지 확인(사생 축)
3. 서비스 알림 체감(FGS 소유 안내)

## 8. 조율

- jkapk 잠금(음성 라인 세션 — SpeechRecognizer 전환 ~1h) 종료 커밋 SHA를
  기점(재판독)으로 T1 개시 — 중간에 잠금 반납 통보 받을 것(계약).