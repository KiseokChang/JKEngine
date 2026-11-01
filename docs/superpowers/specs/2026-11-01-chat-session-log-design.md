# 스펙: 대화 세션 기록 (Chat Session Log) — 2026-11-01

> 상태: **사용자 승인**("좋네요", 2026-11-01). 출발점: 사용자 질문 "JK 앱에서 하는
> 대화를 가져올 수 있나" → 서버 use_journal에는 도구 호출만 남고 **말한 문장이
> 없어** 개선 분석이 추측이 됨(같은 날 실측: onebyone·pathfinding·mlist 없는 앱
> 실행, 지뢰찾기 중복 실행, 앞 창 닫기 4연속 — 원인 문장 불명). 사용자 제안:
> "Claude의 세션 같은 거지요? 참고해서 고도화".

## 목적

말로 하는 대화의 **한 턴 전체**(한 말 → 해석 경로 → 실행 → 답)를 세션 단위
JSONL로 폰 안에 남기고, 세션(Claude)이 그것을 가져와 개선점을 찾는다. #113
"사용 → 리뷰 → OS 업데이트" 루프의 빠진 입력을 채운다.

## 결정

- **D1 세션 단위 파일**: `state/sessions/<YYYYMMDD-HHMMSS>_<source>_<id4>.jsonl`.
  source = `jkapk`(JK 앱) · `chat`(데스크톱 Chat 창) · `web`(jkweb 브라우저 페이지)
  · `talk`(jktalk). state 디렉터리 = 서버 use_journal과 같은 StateDir 규약.
- **D2 세션 경계**: JK 앱 = 앱 프로세스 시작 시 만든 id를 /talk 바디
  `"session"`으로 보냄. 데스크톱 Chat = 창 하나가 세션 하나. id 없는 /talk
  (브라우저 페이지·구판 APK) = `web` 세션, 마지막 턴 뒤 30분 무활동이면 새 세션.
- **D3 줄 형식** (Claude Code 세션 JSONL 참고 — 한 줄 한 사건, 턴 번호로 묶음):
  ```
  {"type":"user","ts":…,"turn":N,"source":"jkapk","stt":"system|whisper|typed","text":"…","stt_ms":…}
  {"type":"route","ts":…,"turn":N,"path":"exact|fuzzy|ai|none","matched":"…","kind":"Launch","app":"…","ms":…}
  {"type":"action","ts":…,"turn":N,"tool":"launch_app","args":"…","ok":true,"err":"","ms":…}
  {"type":"reply","ts":…,"turn":N,"text":"…","total_ms":…}
  ```
  `stt`/`stt_ms`는 알 수 있는 표면만(JK 앱은 바디로 보냄, 없으면 생략).
  `fuzzy`의 `matched` = 채택 후보 문장. `ai`는 LLM 응답 원문 대신 해석 결과만.
- **D4 쓰기 실패 = 조용한 스킵**: 기록은 부가 기능 — 대화 처리에 영향 0
  (use_journal D1 계약 승계). 회전/용량 상한은 v1 비목표(원장행).
- **D5 개인정보**: 폰 안에만 저장. 외부 전송 없음. 커밋·문서에 대화 원문
  인용은 사용자 허락 범위 안에서만.
- **D6 경로 보고**: `ChatRouteTurn`에 선택 인자 `ChatTurnTrace*`(path·matched·
  route_ms·llm_ms) 추가 — 기존 호출 무변경(기본 nullptr). 근사 매칭 판별은
  응답 문자열이 아니라 trace 필드로.
- **D7 돌아보기 도구**: `engine/tools/probes/jksessions.sh` — 세션 목록(최근 N),
  한 세션 transcript 사람이 읽는 형태, 요약(경로별 턴 수·실패 턴·없는 앱 시도·
  3초 넘은 턴). Win/WSL/폰 공통, 폰은 ssh로 실행(주소 미기록).
- **D8 (2단계, 별도 승인)**: 같은 세션 최근 턴을 LLM 프롬프트 맥락으로 —
  "그거 다시 열어줘"류. 1단계 기록이 쌓인 뒤 실제 빈도를 보고 범위 결정.

## 구현 단위 (1단계)

1. `engine/src/apps/ChatSessionLog.cpp`+`include/apps/ChatSessionLog.h` — 세션
   열기/턴 기록(user·route·action·reply)/30분 경계, JSON 이스케이프·UTF-8 안전
   절단 재사용, 뮤텍스(jkweb 연결당 스레드).
2. `ChatRouter` — `ChatTurnTrace` 구조 + `ChatRouteTurn` 선택 인자.
3. jkweb `HandleTalk` — 바디 `session`·`stt`·`stt_ms` 수취, 턴 기록, 서버 도구
   결과(ok/err/ms)를 action 줄로.
4. 데스크톱 `ClientChatApp` — 타이핑·[구두] 턴 기록(source chat, stt whisper).
5. jkapk — 앱 시작 시 세션 id 생성, /talk 바디에 `session`·`stt`·`stt_ms` 추가.
6. selftest(순수 부품만): 줄 형식·이스케이프·턴 번호 증가·30분 경계·trace 경로
   판정(exact/fuzzy/none). 스폰·파일 경합 어셜션 금지 원칙.
7. `jksessions.sh` + 폰 배포(서버+jkweb 재기동) + APK 반입.

## 테스트·게이트

- Win/WSL selftest 0 failure, 폰 배포 캐논 등호.
- 폰 실측: JK 앱으로 3턴(정확·근사·AI 각 1) → jksessions.sh로 transcript가
  그대로 나오는지.
- EYES: 사용자가 며칠 쓴 뒤 Claude가 가져온 분석이 쓸모 있는지.

## 비목표

- 서버 밖 전송·클라우드 동기화, 기록 UI(앱 안 보기 화면), 회전/압축, D8.

## 조율

같은 날 다른 세션이 `ChatRouter.cpp/.h`·`jkweb/main.cpp`·`main.cpp`(#124 브라우저
구두 어휘)와 `JKWindowServer.cpp` 등(#125 APK 이식 스파이크)을 미커밋 상태로
수정 중 — 그 커밋 뒤에 착수. jkapk /talk 전송부는 #124 `JkVoiceService` 소관이라
5번은 그쪽과 합의해 넣는다.

## 진행 상태 (2026-11-01 세션 마감 — 다음 세션 인계)

- ✅ 단위 1 기록 모듈 `engine/src/apps/ChatSessionLog.cpp`·`include/apps/ChatSessionLog.h`
  (jkcore 링크) + selftest 3c 19건 + 단위 7 중 `engine/tools/probes/jksessions.sh` —
  커밋 be12936, Win 961 PASS. WSL·폰 selftest 미실행.
- ⏳ 남은 것(순서대로):
  1. `ChatRouter`에 `ChatTurnTrace*` 선택 인자(D6) — path exact/fuzzy/ai/none·matched·ms.
  2. jkweb `HandleTalk`: `ChatSessionRegistry` 하나(stateDir = `<exeDir>/state` — 서버
     use_journal과 같은 폴더인지 폰 레이아웃 확인), 바디 `session`·`stt`·`stt_ms` 수취,
     user/route/action/reply 기록.
  3. 데스크톱 `ClientChatApp`: 창마다 `ChatSessionLog(stateDir,"chat")`, 타이핑·[구두] 턴.
  4. jkapk: `/talk` 바디에 `session`(앱 시작 id)·`stt`·`stt_ms` — /talk 전송 소관이
     #124 `JkVoiceService`로 옮겨졌으니 그 파일에서. 브라우저 세션에 요청해 둠(미확답).
  5. selftest(trace 경로 판정), Win/WSL/폰 배포(서버+**jkweb 재기동**), 폰에서 JK 앱 3턴
     (정확·근사·AI) 후 `jksessions.sh show latest`로 transcript 확인.
- API 요약: `ChatSessionLog(stateDir, source, key="", openMs=-1)` · `int BeginTurn(text,
  stt="", sttMs=-1)` · `Route(turn,path,matched,kind,app,ms)` · `Action(turn,tool,args,ok,
  err,ms)` · `Reply(turn,text,totalMs)` · `ChatSessionRegistry(stateDir, idleMs=30분)` ·
  `Get(key, keyedSource="jkapk")` (key 비면 "web" 단일+30분 경계).
