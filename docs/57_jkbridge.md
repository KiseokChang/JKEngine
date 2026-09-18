# 57. jkbridge — 폰 웹 게이트웨이 as-built — 2026-09-18

스펙: `docs/superpowers/specs/2026-09-18-jkbridge-design.md` / 플랜:
`docs/superpowers/plans/2026-09-18-jkbridge.md`. 요구: "jkchat 기능을 다른
기기(안드로이드 폰)에서" — 브라우저 웹 UI + 전 기능 설계(확장 가능) + LAN 한정
URL 토큰. jkchat을 서버에서 띄우는 대신 **브리지가 폰 브라우저를 제어 채널에
연결**한다: 승인 파이프라인(allow/ask/deny, 파킹) 무수정, 브리지는 jkchat과
동급의 control-only 파이프 클라이언트.

## 1. JKLlmEngine 공용 모듈 추출 (jkcore)

- `include/agent/JKLlmEngine.h` + `src/agent/JKLlmEngine.cpp` — jkchat의 LLM
  턴 러너를 추출(ChatConfig/LoadChatConfig/BuildEngineCmd/ParseStreamLine/
  LlmTurnThread). claude CLI 서브프로세스(stream-json+partial), engine=
  stub|claude|ollama, 10분 타임아웃 — 전부 원본 그대로.
- 계약: ①턴당 콜백(DoneFn) 1회 보장 — **스폰 실패 포함**(CreatePipe 실패 경로도
  Finish 호출 — jkchat의 잠복 갭을 추출하면서 픽스) ②세션 연속성은 소비자
  소유(`--resume` 세션 id를 소비자가 보관) ③엔진은 턴보다 수명이 김
  (busy가 atomic<int> 포인터로 TurnJob에 전달 — 엔진 수명과 턴 수명 분리).
- jkchat은 consumer-owned `g_sessionId` + 정적 콜백(OnLlmDelta/OnLlmDone →
  PostMessage WM_APP_LLM_*)로 전환. WM_APP 핸들러 흐름 불변.
- JKAgentJson에 `GetRaw(key, out)` 추가 — 1레벨 JSON.stringify 원문 인출
  (GetObjRaw의 형제). jkbridge가 tool args를 재직렬화 없이 패스스루하는 데
  쓰인다. 회귀: probe_agent_chat 7체크 PASS, smoke_llm_stream 3체크 PASS.

## 2. jkbridge.exe — HTTP + WebSocket 게이트웨이

- 콘솔 프로세스(MinGW, `-static-libstdc++`), 링크 ws2_32+bcrypt(jkcore).
  외부 의존 0 — HTTP 파서/WS 프레밍/SHA-1/base64 전부 수기.
- **HTTP**: `/`=임베드 웹 UI(토큰 무관 — 토큰은 WS에만 요구되어 URL 공유로
  접속 가능), `/health`="ok", 나머지 404. 헤드 8KiB 캡.
- **WebSocket** `/ws?token=`: 토큰 게이트(아래 §4) → RFC 6455 핸드셰이크
  (Sec-WebSocket-Accept = B64(SHA1(key+GUID))) → 세션.
- **프레밍**: 클라 프레임은 마스크 필수(무마스크=프로토콜 위반 절단), 텍스트만
  (binary/close/ping/pong 처리 — ping은 마스크 응답 불가라 unmasked pong
  2바이트 회신), **1MiB 캡 — 선언 길이 먼저 검사**해 거대 프레임 버퍼링 전
  절단(서버는 클라 프레임을 읽기 전에 상한을 안다).
- **세션 = JKAgentClient 1 + JKLlmEngine 1 + pump 스레드(400ms ping) 1**,
  상한 4(`too many sessions` — 핸드셰이크 101 직후 WS 오류 프레임으로 거절).

## 3. 릴레이 프레임 (generic tool relay — 확장 포인트)

- 클라→서버: `hello{resume_session}` / `chat{text}` / `tool{tool,args,label}`
  / `approve{request,decision}`.
- 서버→클라: `hello{ok,busy,pending_result?}` / `stream{text}` / `chat_done
  {ok,streamed,result,session_id}` / `reply{label,json}` / `event{topic,json}`
  / `error{text}`.
- tool 프레임은 args를 **GetRaw 원문 패스스루** — 서버 도구 추가(files/notes
  등)에 브리지 수정 불요. 요청 형태는 agentctl과 동일(`{"tool","args"}`).
- pump가 queryId→label 맵으로 reply를 라우팅(미래의 어떤 서버 도구든 label만
  달면 폰에 회신). event는 토픽 통짜 전달.
- **approve도 라벨 등록** — 초기 구현은 approve 응답을 전달하지 않아 폰이
  승인 결과를 영원히 못 보는 결함(probe가 잡음). 이제
  `{"type":"reply","label":"approve","json":{"ok":true,"approved":true}}` 회신.
- 세션 연속성: localStorage의 claude session id → hello.resume_session →
  `--resume`. DoneMemo(세션 id별 마지막 chat_done, 5분, 4항목)가 재접속 시
  pending_result로 복구.

## 4. 보안

- 토큰: 부트 시 state\jkbridge.json 없으면 BCryptGenRandom으로 32hex 생성·
  저장. **WS 핸드셰이크 쿼리 파라미터에서 검증**(스펙 §4 hello 프레임 검증에서
  변경 — as-built 위임; hello는 resume_session만 운반). HTTP UI/health는
  토큰 무관(스펙 §4 명시).
- IP 게이트: 실패 토큰 10/60s → 403(성공 시도는 카운트하지 않음).
- 부트 **SHA-1 셀프테스트(RFC 6455 §1.3 벡터)** — FAIL이면 기동 거부.
- 키 길이 상한 120(SHA1 스크래치 128바이트 경계 — 적대적 긴 키 오버플로 봉쇄).
- 토큰은 URL에 붙어 다닌다(공유 편의) — LAN 신뢰 전제, 스펙 §6 한계 그대로.

## 5. 웹 UI (단일 임베드 HTML)

모바일 다크 UI: 승인 스트립 큐(agent.approval_request 이벤트 → 승인/거부
버튼, 종류별 안내 문구는 jkchat에서 이식), 채팅 스트리밍, 슬래시 명령
(/close·/restore 등 — /close는 pre_undo 2단 연쇄), localStorage 세션 재개,
2s 자동재접속, 재접속 시 pending_result 복구. 서버 상태 없음 — 전부 클라.

## 6. 검증 — probe_jkbridge 15체크 ×2 연속 ALL PASS

빌드 디렉터리 실행(jkbridge는 jkcore → SDL2.dll 등 런타임 DLL 의존 — temp
복사 실행은 즉사, **probe-owned state 스왑**: state\jkbridge.json 고정
토큰/포트 + state\chat.json stub 엔진 백업/복원). 원시 TcpClient WS 클라:

http-health / http-ui / http-404 / ws-no-token-401 / ws-handshake-101 /
ws-hello-ok / stub-chat-done(stub ok+stub-1) / tool-reply(list_windows) /
minesweeper-launched / approval-request-event(close=ask 파킹 이벤트) /
approve-roundtrip(allow → 창 파괴 + approve reply) / session-cap(4+1) /
frame-cap(선언 길이 0xFF×8 → 절단) / resume-memo(stub-1 → pending_result) /
rate-limit(11회 → 403).

## 7. SHA-1 디버그 — 오탐 3연쇄 (레슨)

셀프테스트가 부트를 거부해 시작한 3시간: 제 "유명 상수" 암기 오류가 3겹.

1. **h4 = 0xC3D2E1F0**(FA 아님 — 니블 내림 패턴 01..67/89..EF/FE..98/76..10/
   F0 E1 D2 C3). 2. 디버깅 중 옳은 체인을 "고친" 것 — 위키 체인은
   `c←rotl30(b); b←a`이고, 라운드 79 상태가 h1의 순수 회전값만 남는
   (혼합 소실) 관찰로 확정. 3. 셀프테스트 기대문자열도 29자 오탐(20바이트
   base64는 28자 불변) — 28자 `s3pPLMBiTxaQ9kYGzzhZRbK+xOo=`가 참.
   외부 근거(Wikipedia) 대조 + 파이썬 hashlib 자동대조 변형 탐색이 결판.
   **교훈: "유명한 값"도 제 기억이 아니라 hashlib/외부 문서로 검증 —
   자기 일관 오류는 자기 재현으로는 못 잡는다.**

## 8. 빌드 함정 (레슨)

- build_with_temp.sh는 I: 원본 → /c/temp_jkdesktop **전체 동기화 후** 빌드.
  이후 main.cpp만 고치고 temp에서 `ninja`를 돌리면 **stale 복사본**을
  빌드한다(probe FAIL 2회의 근원). 원본 수정 후 반드시 `cp -f <원본>
  /c/temp_jkdesktop/<경로>` 후 ninja, 또는 스크립트 전체 재실행.
- ninja의 ld 일시 실패(exit 67)은 재실행으로 회복(백신/파일락 추정).

## 9. 한계 / 백로그

- WS 토큰이 URL 쿼리로 다닌다 — LAN 한정 전제. HTTPS/WSS 없음(브라우저가
  localhost가 아닌 origin의 http WS를 허용하므로 동작하나 평문).
- 1세션 = 1 에이전트 파이프 연결 — 서버 파이프 접속 예산과 공유.
- 웹 UI는 눈확인 항목(폰 실기기 레이아웃/터치).
- 확장: tool 프레임이 generic이라 files/notes 등 서버 도구 추가 시
  브리지 무수정 — 스펙 §2의 확장 프리미스 충족.