# #124 구두 서비스 분리+브라우저 구두 v1 as-built (docs/102)

**상태: T1-T3+T4 서버 leg 폐곡·APK 배포는 병행 세션 조율 대기·EYES 대기**

## 1. 결정 원존

- 스펙 `docs/superpowers/specs/2026-11-01-voice-service-browser-v1-design.md`
  ·플랜 `docs/superpowers/plans/2026-11-01-voice-service-browser-v1.md`
- 사용자: "구두로 브라우저 조작 가능?"→"좋아요"(서비스 이층 확정)·"녹음 트리거?"
  → **푸시토크 계약 재확정(홀드=듣기·떼기=마침·평시 녹음 0·웨이크워드 넣지 않음)**
- "JK앱에 겹쳐 있어서 구두 처리가 어렵지 않아?" → 지적 채택 — **근본 수형=서비스 분리**.

## 2. 실측 원존 (빌드·스코프)

- **T1+T2 (695bf33)**: JkVoiceService(마이크 FGS·기본 STT+/talk POST 유일 소관)
  — 양창 [구두] 홀드 릴레이(Down=ACTION_START·Up=ACTION_STOP), BC_STATUS/BC_REPLY
  브로드캐스트(Main=채팅 진실원·Browser=토스트). 앱 덮어쓰기 함정 회피:
  STOP→stopListening 후 **onResults에서 finishSelf**(즉시 중단 시 결과 미수취).
  - **스코프 정직**: v1 서비스=기본(STT) 엔진만 이동 — Whisper는 본창 예비 유지
    ([인식:Whisper] 모드 — 본창 자체 사슬 = 기존 계약 승계).
- **T3 (4b67320+09bf21d)**: ChatAction::AmpkOpen(숫자 계보 보존 — OverlayNumbers
  뒤 추가·2y-⑲ 동형)·어휘 exact "브라우저 열어줘/켜줘"(Launch 행 **앞 배치** —
  접미 순서 계약 2y-⑩ 동형)+**Launch URL 휴리스틱**(앱어에 점/http = 브라우저
  승천 — 파서 신설 금지 D1 준수·그 외 search 어휘는 v1 생략 — LLM 스키마 확장
  = 뒤 라인)·jkweb 스폰 leg(이중 fork+`am start -e jkUrl`·xdg-open/cmd 폴백·
  **URL 화이트리스트 fail-closed**(인젝션 봉쇄)·#117 선례 실패무시 계약)
  - **세션 기록 바디 확장**(jkengine-89 세션 요청 — 대화 세션 기록 스펙
    2026-11-01-chat-session-log-design 연동): POST /talk 바디에
    `"session"`(프로세스 수명 짧은 id)+`"stt":"system"`+`"stt_ms"`(누름~결과 ms).
    MainActivity(whisper/typed) 쪽 필드 = 원장행(그쪽 소관).
  - DefaultActionGuide AmpkOpen case (-Wswitch — 09bf21d).
- **selftest**: WSL posix_selftest **0 failure(s)** — 회귀 0.
- WSL 전체 72타깃 PASS·기존 소비자(클라이언트) switch 회귀 0.

## 3. 배포 원존 (2026-11-01)

- **폰 엔진 = 단일 축 증분 배포**(docs/90 park4 단일 blob 선례 승계): 변경 3파일
  (ChatRouter.h/.cpp·jkweb/main.cpp) git archive HEAD 스테이징 → 폰 트리 tar →
  `ninja -C buildterm -j4` 증분(aarch64) — **전수 재배포/드라이버 신설 금지 회피
  (이 라인 판정: 소규모 변화라 단일 축 적합)**.
- **jkweb 재기동 원존**: 라이브 서버 exe md5 불등호 실측(구판 잔존 — jkweb이
  ChatRouter 내장 함정 원존 docs/90 승계) → **explicit pid kill(29706)+nohup
  재기동(stdin /dev/null 함정 승계)** → 신선 원문: pid cmdline
  `./jkweb --port 8090`·`/proc/*/exe md5 = buildterm/jkweb md5(3dad0ff7) 등호`.
  - **jkdesktop 서버 무접촉 계약**: 창 소실 회피 — DeX chat(F6) 표면 어휘는
    **다음 jkdesktop 자연 재기동에서 자동 승계**(소스 동기 완료 — 정직 원존).
- **어휘 3경 실측 원문(폰 /talk)**:
  - "브라우저 열어줘" → `{"ok":true,"reply":"브라우저 창을 엽니다(폰 — 성립은
    화면에서 확인)","kind":"AmpkOpen","app":""}`
  - "youtube.com 열어줘" → `{"ok":true,"reply":"브라우저로 개봉합니다:
    youtube.com","kind":"AmpkOpen"}` (URL 휴리스틱 leg)
  - "지뢰찾기 켜줘" → `{"ok":true,…,"kind":"Launch","app":"minesweeper"}` —
    **기존 경로 회귀 0**
  - 검증 검토 잔상: 실측이 스폰한 Minesweeper 창(id=29) close_window로 소각
    (기존 창 무접촉 — 잔상 0 계약).
- **APK(구두 서비스·URL 인텐트 판)**: 병행 세션(jkengine-89)의 695bf33판 반입
  절차 중(사용자 설치 대기) — **내 미커밋→커밋 후 URL 인텐트 포함판은 그 뒤
  배포(덮어쓰기 통지 계약)** — 세션 기록 필드 포함 최종판 md5는 배포와 동시 원존.

## 4. EYES 대기 (사용자 선언만)

1. 브라우저 창에서 [구두] 홀드 → "youtube.com 열어줘" 발화 → URL 개봉 체감
2. "검색어 단어 검색해줘" — v1은 URL 휴리스틱만이므로 검색 전용 어휘는 LLM
   확장 뒤(원장행 §5 — 현재 검색 발화는 talk 안내 폴백)
3. 서비스 FGS 알림(Importance LOW) 체감·앱 화면 밖 마이크 정지(사생 축)

## 5. 라인 밖 원장행

- 검색 어휘(다중 어절)+2단계(페이지 안 입력 via broadcast+JS)·3단계(LLM 페이지
  판단 루프) — 별도 스펙(음성 라인의 session-log 위에 얹는 형태).
- MainActivity(whisper/typed) POST 바디에 session/stt 필드 승계(음성 세션).
- 결제 축·DeX 그리드 am start 래퍼(#123 원존 승계).