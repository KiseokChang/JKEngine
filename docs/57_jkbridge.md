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
  상한 4(`too many sessions` — 핸드셰이크 101 직후 WS 오류 프레임으로 거절,
  fetch_add 후 반납으로 TOCTOU 봉쇄).
- **연결별 30s SO_RCVTIMEO/SO_SNDTIMEO** + **pump WS 하트비트(~4.8s ping,
  브라우저가 자동 pong)** — 무타임아웃이면 서스펜드된 폰이 dispatcher의 recv를
  영구 블록해 세션 슬롯을 영구 누수하고(opus MAJOR-3), 45s 무pong 세션은
  pump가 강제 종료해 슬롯 회수. 소켓 close는 소멸자로 이동(핸들 재사용
  경합 봉쇄 — opus MINOR-6), shutdown(SD_BOTH)만 즉시.

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
- IP 게이트: 실패 토큰 10/60s → 403(성공 시도는 카운트하지 않음). 토큰 비교는
  상수-형태(TokenEq, 길이 먼저 — 32hex라 길이 누출은 무해).
- 부트 **SHA-1 셀프테스트(RFC 6455 §1.3 벡터)** — FAIL이면 기동 거부.
- 키 길이 상한 64(RFC 키는 24자; Sha1 입력 ≤ 64+36+패딩 ≤ 128 — 스크래치는
  256으로 이중 여유. 초기 컷의 "상한 120"은 GUID 36바이트를 빠뜨린 계산 착오로
  opus 리뷰 MAJOR-1에서 스택 오버플로로 판명되어 픽스).
- 토큰은 URL에 붙어 다닌다(공유 편의) — LAN 신뢰 전제, 스펙 §6 한계 그대로.

## 5. 웹 UI (단일 임베드 HTML)

모바일 다크 UI: 승인 스트립 큐(agent.approval_request 이벤트 → 승인/거부
버튼, 종류별 안내 문구는 jkchat에서 이식), 채팅 스트리밍, 슬래시 명령
(/close·/restore 등 — /close는 pre_undo 2단 연쇄), localStorage 세션 재개,
2s 자동재접속, 재접속 시 pending_result 복구. 서버 상태 없음 — 전부 클라.

## 6. 검증 — probe_jkbridge 16체크 ×2 연속 ALL PASS

빌드 디렉터리 실행(jkbridge는 jkcore → SDL2.dll 등 런타임 DLL 의존 — temp
복사 실행은 즉사, **probe-owned state 스왑**: state\jkbridge.json 고정
토큰/포트 + state\chat.json stub 엔진 + permissions.json까지 전부 백업/복원
— docs/54 레슨 ⑥ 재적용). 원시 TcpClient WS 클라(핑 자동 응답):

http-health / http-ui / http-404 / ws-no-token-401 / ws-handshake-101 /
ws-hello-ok / stub-chat-done(stub ok+stub-1) / tool-reply(list_windows) /
minesweeper-launched / approval-request-event(close=ask 파킹 이벤트) /
approve-roundtrip(allow → 창 파괴 + approve reply) / session-cap(성장 거절) /
frame-cap(선언 길이 0xFF×8 → 절단 — 타임아웃과 진짜 close 변별) /
slot-reclaim(닫힌 세션 슬롯 회수 — FIN 누수 회귀 가드) /
resume-memo(stub-1 → pending_result) / rate-limit(11회 → 403, 마지막 배치 —
403 등급 IP 게이트가 이후 체크를 오염하는 것을 피하려는 순서).

회귀: probe_agent_chat 7체크 PASS, smoke_llm_stream 3체크 PASS(픽스 후 재실행).

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
- **INADDR_ANY 바인딩** — "LAN 한정"은 PC가 붙은 모든 네트워크 인터페이스
  (사내망 포함)에 노출된다는 뜻. 방화벽(Windows가 기본으로 공용 네트워크
  차단)이 실질 경계. 특정 인터페이스 바인딩은 백로그.
- 1세션 = 1 에이전트 파이프 연결 — 서버 파이프 접속 예산과 공유.
- 웹 UI는 눈확인 항목(폰 실기기 레이아웃/터치).
- 확장: tool 프레임이 generic이라 files/notes 등 서버 도구 추가 시
  브리지 무수정 — 스펙 §2의 확장 프리미스 충족.

## 10. opus 최종리뷰 — **FIX REQUIRED → 전부 픽스 (2026-09-18)**

**MAJOR-1** 키 상한 120은 핸드셰이크 SHA1 입력(key+36 GUID)을 빠뜨려 키 93자
부터 스택 오버플로 → 상한 64+버퍼 256. **MAJOR-2** pump 재접속 경로(agent
연결/구독)가 agentMtx_ 밖 — dispatcher의 SendQuery와 JKAgentClient(내부 락
없음)를 경합 → 전 agent 접촉을 잠금 내로. **MAJOR-3** 소켓 타임아웃 0 +
WS keepalive 부재 → 서스펜드 폰이 슬롯 영구 점유("too many sessions"로
전면 마비) + pump가 wsMtx_ 보유 중 블로킹 send로 세션 교착 → 30s 타임아웃 +
pump WS 하트비트(4.8s ping/45s 무pong 강제종료) + pump SendText 실패 시
dead 루트.

**MINOR-1** resumeSession_ 레이스(LLM 워커 쓰기) → resumeMtx_.
**MINOR-2** 세션 캡 TOCTOU → fetch_add/fetch_sub. **MINOR-3** WsReadFrame의
정확-크기 단일 recv 가정(2바이트 확장/4바이트 마스크) → RecvAll 루프 통일.
**MINOR-4** ping/pong 마스크·페이로드 미소비(스트림 desync)+재귀(플러드 시
스택 사망)+납입 pong → 전량 소비·루프 전환·wsMtx_ 경유. **MINOR-5** probe가
permissions.json을 백업 없이 파괴 → 백업/복원 패턴 적용. **MINOR-6**
closesocket 즉시 실행의 핸들 재사용 경합 → shutdown만 즉시, close는 소멸자.

**NIT**: 토큰 상수시간 비교(TokenEq)/CreatePipe 부분 실패 핸들 누수/
ReadHttpHead(1바이트 독해 — 미종결 헤드 거부+과잉독해 소멸)/설정 쓰기 실패
로그/labels_ 128 상한/probe frame-cap 타임아웃 오판 변별+tool-reply 루즈
매치 강화/웹 UI esc() 데드 코드 제거(XSS는 전 경로 textContent로 클린 명시)/
INADDR_ANY 한계 문서화(§9). probe는 슬롯 회수 체크(slot-reclaim) 신설 —
16체크 ×2 ALL PASS로 재검증.
## 11. 콘솔 QR 출력 — 수기 QR 인코더 (2026-09-19)

요구: "보통 이런경우 바코드를 사용하지 않나요?" — 기동 시 URL+토큰을 폰 카메라가
스캔하는 QR을 콘솔에 출력. 외부 의존 0 원칙 유지로 **QR 인코더를 수기 구현**
(바이트 모드, ECC L, V1–V6).

### 11.1 구현

- **비트스트림**: 모드 니블 0100 + 8비트 카운트(V1–9) + 데이터 + 터미네이터
  (최대 4비트, 용량 부족 시 절단 허용) → 0으로 바이트 경계 패딩 → 0xEC/0x11
  교대로 데이터 용량까지 충전.
- **버전 선택**: 필요 비트 = 4+8+8×len vs 데이터 cw×8 — 초기 컷은 원본 바이트
  수를 코드워드 용량과 비교해 경계 URL이 무음 잘림(예: 135바이트 URL을 V6 데이터
  캡 136cw로 판정했으나 실제 필요는 137cw → V7 필요). 필요 비트 비교로 픽스.
- **Reed–Solomon**: GF(256), 원시다항식 0x11D, 모닉 생성 다항식 ∏(x−α^i),
  합성 나눗셈. V1–V6 L은 블록이 항상 동일 크기 1–2개라 라운드로빈 인터리브.
- **행렬 배치**: 파인더+세퍼레이터, 타이밍(6행/6열), 정렬 패턴(파인더 겹침
  스킵), 포맷 예약+다크 모듈 (n−8,8), 지그재그 2폭 컬럼(6열 스킵),
  마스크 0–7 페널티 평가로 최적 선택, 포맷 정보 2사본+다크 모듈.
- **콘솔 출력**: 하프블록 글리프(█▀▄+공백)로 2 QR 행/텍스트 행 — V4 기준
  ~25줄. 콘솔 속성을 흰 바탕/검은 글리프로 스왑해 ██=진짜 어두운 모듈(폰 카메라
  대응). 리다이렉트(프로브 관례)에서는 스킵 — `--qr-debug`(0/1 행렬 덤프)와
  `--qr-print`(강제 글리프 출력, 프로브 재조립용)로 검증 경로 분리.

### 11.2 검증 — 외부 진실원 3중

1. **코드워드**: segno(`boost_error=False`)를 제 데이터 위로 돌려 ECC 대조 —
   초기에 불일치 → 원인은 **C `EccBytes`의 계수 관례 불일치**(생성 다항식은
   저차수 우선으로 만들고 나눗셈은 고차원처럼 `gen[j+1]` 인덱싱 — 수정:
   `gen[ecc-1-j]`). 파이썬 RS 재구현이 segno와 일치해 C만 결함임을 분리.
2. **배치**: segno 프리미티브(make_matrix+finder+alignment+add_codewords+
   best_mask+format_info)에 **제 코드워드**를 넣어 만든 행렬과 셀 단위 diff —
   V1–V6 전 버전 **0 diff** (동일 입력에 동일 출력 = 배치 비트 완전 일치).
3. **디코드**: cv2.QRCodeDetector + pyzbar 이중 — `--qr-debug` 행렬과
   `--qr-print` 글리프 재조립 행렬 둘 다 원문 URL 복원. V1–V6 길이 배터리 7/7.

probe_jkbridge에 6체크 신설(qr-debug-parse/finder/timing/dark-module/
determinism/print-glyphs) → **22체크 ×2 ALL PASS**. 회귀: probe_agent_chat,
smoke_llm_stream PASS.

### 11.3 디버그 결판 — 오판 레퍼런스 3연쇄 (레슨)

1. **파이썬 복제 동일성 착시**: "내 C == 내 파이썬 복제" 검증만으로는 segno와
   같다는 증거가 안 된다 — 복제가 C의 버그를 같이 복제한다(레슨: 독립 기준은
   **외부 구현을 그대로 호출**하는 것).
2. **segno 쿼크**: `write_padding_bits`가 `8-(len%8)` 비트를 **무조건** 추가 —
   바이트 경계에서 정확히 끝나는 스트림에도 0x00 1바이트를 덧붙임(ISO 18004
   §7.4.10은 경계면 무패딩). 바이트 모드는 4+8+8L+4비트라 터미네이터가 항상
   경계에 착지 → segno는 매번 0x00을 추가, 제 출력은 ISO 순응. **매트릭스
   diff가 0이 나올 수 없는 구조** — 배치 비교는 segno 프리미티브에 제
   코드워드를 넣는 방식으로 우회.
3. **`calc_format_info(4, 'L', 0)` M-레벨 함정**: segno의 error 인자는 문자열
   'L'이 아니라 숫자 상수 — 'L'을 넘기면 레벨 오프셋이 무시돼 FORMAT_INFO[0]
   (M 레벨)을 깔았고, 제 C 포맷 셀 16개가 "틀렸다"는 오탐. 숫자 상수로 수정하니
   0 diff.
4. **진짜 결함**: `Encode`가 `Compose`에 **데이터 비트스트림만** 넘김 — ECC
   코드워드 20개가 매트릭스에 아예 배치되지 않았고 (비트 640 = 코드워드 80
   경계부터 전부 0 패딩). 불일치 시작점이 정확히 ECC 경계여서 발견. 픽스:
   BuildCodewords(데이터+ECC 인터리브) → 비트 확장 → Compose.
5. **디버그 도구의 힘**: `--qr-debug`(행렬+코드워드 덤프)→`--qr-debug-func`(함수
   셀 맵 덤프)→`--qr-print`(글리프) 단계 덤프가 "func 맵 일치+순회 일치+코드워드
   일치 → 행렬 불일치" 모순을 코드워드 경계(비트 640)에서 분쇄.
