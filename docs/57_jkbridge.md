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
  차단)이 실질 경계. 특정 인터페이스 바인딩은 백로그. **(2026-09-20 해소 —
  state/jkbridge.json의 옵션 `"bind":"<IPv4>"` 필드로 특정 인터페이스만
  바인딩. 미지정 = 기존 ANY. 오탈자 IP는 fail-closed 루프백+경고 — ANY
  폴백은 축소 의도를 확장으로 반전시킨다. docs/59 §16)**
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

## 12. 폰 UI 타임스탬프 + `/report` 원격 진단 저장 (2026-09-20)

### 12.1 발단 — "붙여넣기→패치" 루프의 물리적 한계

vplayer open 실패 진단(docs/59 §18)에서 사용자가 폰 대화 기록을 PC에 붙여넣어
패치로 이어진 사건 이후, 사용자 피드백 2건:

1. "대화에 시간 정보가 있으면 더 좋을까요?" — 증상 재현 간격(open 시도 후
   get_status 폴링 주기 등)을 판단할 시각 근거 부재.
2. "지금은 사람이 직접 복사해서 붙여넣고 있는데, 진짜 폰에서 했으면 그러기
   힘들었을 것" — PC 브라우저 덕에 가능했던 복사-붙여넣기, 실기기에서는
   비현실적.

### 12.2 설계 — 서버 스탬프 + 원탭 리포트

**타임스탬프 (진실원=서버 시계)**: 모든 WS 송신 프레임을 `SendText` 한 곳에서
`{"ts":<unix>,…}`로 래핑(선두 `{` 뒤 삽입). 클라이언트 시계·이벤트 경로별
수정 없이 중앙 1곳 — 수신 프레임도 같은 경로라 별도 주입 불요.

**`/report`**: 폰 UI가 `transcript[]`(모든 add/stream 줄을 `[HH:MM] 텍스트`
누적으로 유지, LLM 스트리밍은 인덱스 갱신)을 들고 있다가 `/report` 입력 시
`{type:"report", text:…}` 전송(256KiB 상한, 초과 시 앞부분 절단). 브리지:

- 텍스트 검증(빈 값/상한 초과 → `bad report`), **10초 쿨다운**
  (`lastReport_` atomic, `report_cooldown` 응답 — 플러드 방지).
- `state\bridge_report_YYYYMMDD_HHMMSS.txt` 기록 → reply에 path 반환.
- `agent.notify` publish_event("폰 리포트 도착") — PC 알림 센터에서 즉시 인지.

PC 측 사용 흐름: 알림 → `state\bridge_report_*.txt` 열람 → 시각 포함 대화
원문으로 진단 개시. 폰에서는 타이핑 `/report` 한 줄이 전부.

**`pending_ts`**: DoneMemo.Get이 {putTime, result} pair 반환 → 재접속 hello의
pending_result에도 스탬프 부착(끊긴 사이 완료된 턴의 시각 보존).

### 12.3 검증

probe_jkbridge §5b 신설 2체크: report 파일 기록+내용 일치(경로 언이스케이프)+
즉시 2회 전송 시 `report_cooldown`. **31체크 ×2 ALL PASS**, 라이브 브리지
재기동 HTTP 200 확인.

레슨: 진단 채널의 성능은 수집의 마찰에 반비례 — 폰→PC 보고는 복사-붙여넣기를
거치는 순간 실기기에서 죽는다. 리포트는 "사용자가 만든 대화"를 그대로 파일로
착지시키고, PC는 파일을 읽을 뿐이다.

### 12.4 라이브 결함 — ts 래퍼 쉼표 탈락, 모든 프레임 JSON 파산 (2026-09-20 오후)

**증상 (사용자)**: 폰에서 "브리지가 대답을 안 한다" — 도구 실행은 정상(receipts에
launch_app/list_windows 성공 기록, Script Demo 실제로 뜸)인데 폰 화면엔 아무
응답이 안 찍힘.

**원인**: §12의 중앙 ts 래퍼(`SendText`)가 `stamped += json.substr(1)`로 선두 `{`
뒤를 **쉼표 없이** 이어붙여 모든 프레임이 `{"ts":1789886783"type":"hello",...}`
꼴 — 폰의 `JSON.parse`가 전멸. 브리지·LLM·도구·승인 파이프라인은 전부 정상이라
서버 쪽 흔적만 완벽했고, 고장은 와이어 직후 폰 파서에서만 일어났다.

**픽스**: `stamped += ','` 1줄. probe_jkbridge에 `ws-hello-valid-json` 신설 —
hello 프레임을 `ConvertFrom-Json`으로 **엄격 파싱**(type/ok/ts 필드 검증).
기존 프로브가 regex `-match`만 써서 이 부류(문법 파산)를 못 잡은 게 방어 공백.

**레슨**: 정규식 매칭 프로브는 "JSON 형태를 한 번이라도 엄격 파싱"하는 체크를
같이 둬야 한다 — 문자열 포함 검사는 잘못된 문법도 전부 통과시킨다.

### 12.5 폰 답변 마크다운 아티팩트 정리 (2026-09-20 오후, 사용자 보고 "답변에 ``` 섞여나와")

**원인**: LLM이 코드펜스(```)/사고 블록(```
**)/헤딩 `#`/`**굵게**`를 원문 마크다운으로 뱉는데 폰 UI는 플레인 텍스트 —
펜스 마커가 문자 그대로 찍힌다. 라이브 WS 실측으로 재현("print hello world in
python" → ```python 펜스 그대로 스트리밍).

**픽스**: 폰 UI에 `clean()` 신설 — XSS 포스트(textContent 전용, innerHTML 금지)를
지키는 최소 정리. 태그 변환 없이 **마커만 제거**: 닫힌/미닫힌 사고 블록, 펜스
마커 줄, 헤딩 `#`, `**`, 인라인 백틱, 잔여 빈 줄 정리. 스트리밍은 매 프레임
누적 원문을 통째로 정해 다시 그린다(펜스가 프레임 경계에 걸쳐도 안전).
chat_done 비스트리밍 결과와 재접속 pending_result에도 동일 적용.

probe_jkbridge ×2 ALL PASS(정리 함수 탑재 후 전체 회귀 무손상).

## 13. 폰 실전 개선 4건 — args 캡·창 기하 도구·턴 프리앰블·focused 디바운스 (2026-09-21)

§12.6 백로그 4건 소각 (스펙 `.superpowers/sdd/2026-09-21-phone-practical-improvements`,
커밋 `c9b7e75..3c57127`, 작업 1-3 + 회귀 스윕 본 절).

| 건 | 변경 | 검증 |
|---|---|---|
| ① app_tool args 앞단 캡 8KiB→256KiB | JKWindowServer.cpp :3125 — 워크숍 set_script가 앱 측 256KiB 백스톱을 두고 있어 8KiB 앞단이 병목. 앱 백스톱과 정렬(근사치 — argsRaw는 프레이밍·이스케이프 팽창을 포함한 내부 args 원문이라 앱 캡 경계의 소스가 앞단에서 먼저 막힐 수 있음). **result 16KiB 캡은 건드리지 않음(별도 백로그)** | probe_app_tools c3c-args-cap-256k-passes(10250바이트(십진 10.25 kB) args + 미등록 도구 → `unknown_app_tool` — 구 8KiB 캡이면 `args_too_large`가 먼저였으므로 에러 순서로 캡 통과 간접 단정) ×2 |
| ② window_move / window_resize 서버 도구 | window_fullscreen 뼈대 복계(:2998-3095). 대상 선정(생략=자기 창, 명시 id=연결 존재 판정)/거절 체계: `no_window`(control-only 생략형)/`window_not_found`(shell·캡처 오버레이·control-only 타깃)/`window_maximized`(preMaxRects_)/`window_fullscreen_state`/`bad_args`. move는 클램프 없음(Windows 동작 — 화면 밖 허용)+±32768 범위 검사, resize는 **[80,8192] 범위 검사(클램프 아님 — 범위 밖 bad_args)**, 동일 픽셀 크기 no-op ok, fit-scaled 레이어(ScaleX/Y≠1)는 표시 크기 dispW/H를 `lround(w*ScaleX)` 산출로 넘겨 scale 보존(ResizeLayer가 scale을 1로 리셋). 위치는 `SetLayerPosition`+`SetPosition` BOTH 쌍수술(docs/28 — 합성 입력이 client->X()/Y() 직독)+`PushWindowListUnsafe`(태스크바 동기). 게이트 **kPermMatrix none→allow**(window_fullscreen 분류 승계 — 화면 상태 변경일 뿐 승인 행위 아님). 브로커 4곳 등록(tools/list 정적부·IsKnownTool·LoadPermissions·릴레이 args 원문 패스스루) | probe_window_geom 신설 29체크 ×2 — 기하 단정(list_windows x/y/w/h 대응 축), bad_args 5종, 생략형 no_window, fullscreen-state 거절+rect 복원, jkagentd MCP tools/list 노출+tools/call 릴레이 e2e |
| ③ kLlmTurnPreamble | JKLlmEngine.cpp :100-110 상수 신설 + BuildEngineCmd가 escape 루프 앞에서 매 턴 접두(한국어 3지시문: 최종 답변만 출력/마크다운 문법 금지/도구는 조용히 실행하고 결과만 보고, 접미 `[사용자] `). 따옴표·백슬래시 0개로 기존 escape 루프와 CRT argv 재파싱에 안전. stub/claude/ollama 3분기 앞단이라 전 실엔진 턴 적용, `--resume` 턴도 동일 | 실엔진 1턴 실측(kimi-k2.7-code:cloud, 도구 유도형 프롬프트): 응답 `2026-09-21 06:46:33` 1줄 — CoT 내레이션 0, 마크다운 0, 도구 조용 실행. smoke_llm_stream 3/3 ×2(스트리밍 경로 무손상)+probe_agent_chat 7/7 ×2 |
| ④ window.focused 디바운스 | FocusClient 선두 **last-pushed-id 방식**(`lastFocusedPushed_`, JKWindowServer.h:250) — `surfaceId == lastFocusedPushed_` 조기귀환, push가 **resolve된 경우에만** 갱신. 같은 id 재포커스 무push(스팸 봉쇄 유지)+스폰 인테이크(push 없음) 이후 그 창의 첫 명시 포커스 1회 push 보존. 1차 구현(focusedClientId_ 비교)은 인테이크가 push 전 focusedClientId_만 세팅하는 구조(:587/:589 → push_back :597) 탓에 스폰 창 첫 포커스를 삼켜 개정(badc136) | probe_agent_events 8/8 ×2 — refocus 불변/스폰 첫 포커스 +1/즉시 재포커스 불변/포커스 변화 +1 전 단정 |

### 13.1 회귀 실측 (×2 연속, 전부 GREEN — 데스크탑 정지 후 본트리 빌드)

| 프로브 | run1 | run2 |
|---|---|---|
| probe_app_tools.ps1 | ALL PASS (64/0) | ALL PASS (64/0) |
| probe_agent_events.ps1 | 8/8 | 8/8 |
| probe_window_geom.ps1 (신설, task-2) | 29/29 | 29/29 |
| smoke_llm_stream.ps1 | 3/3 | 3/3 |
| probe_agent_chat.ps1 | 7/7 | 7/7 |
| probe_workshop.ps1 | ALL PASS | ALL PASS |
| probe_send_input.ps1 | ALL PASS | ALL PASS |
| jkdesktop test | 0 failure | 0 failure |

### 13.2 하네스 정합 2건 (제품 결함 아님)

1. **probe_workshop c4 스테일 기대** — `c4-args-too-large`(9KiB set_script →
   args_too_large)는 ①의 캡 상향으로 무효화된 기대치. `c4-args-cap-256k-passes`
   (9KiB → ok)로 개정. 회귀 FAIL은 결함 판정 전에 스펙 변화를 먼저 본다.
2. **probe_send_input.ps1 $build가 conquest-ladder worktree에 박혀 있던 것** —
   병합(1779f0e) 후 본트리가 진실원이라 main build 경로로 이동. 프로브 경로가
   worktree에 박혀 있으면 병합 후 본트리 회귀가 조용히 누락된다.

부수: probe_agent_chat이 permissions.json을 소거하는 패턴(§16.1 사건 동형) 재확인 —
실행 전 백업+후 원문 복원(전면 allow 9키)으로 운용. probe_agent_events의 [theme]
오염 JSON 행 필터는 task-1에서 이미 반영.

### 13.3 백로그 이월

1. **✅ 소각 (2026-09-22, 7620c5a 이후 커밋)** — AgentJson에
   GetInt64/GetObjInt64/GetDeepInt64를 플랫폼 파서 계약으로 신설하고 가드가
   읽는 값 전부 이관(window_move/resize 좌표·크기, settings_set idle_minutes/
   receipt_retention_days/audio_master, LoadSettingsKv mute/volume/retention).
   식별·인덱스 조회(id/request/row/col)는 랩어웃이 유효 범위 밖으로 빠져 조회
   실패에 그치므로 int 리더 유지 — 분류 근거는 헤더 주석. jkagentd의 lines/
   timeoutSec 가드는 자체 클램프형(래핑해도 유효값 내 수렴)이라 대상 아님.
   실물: move x=4294967396/resize w=2^32+300/idle_minutes=2^32+100 전부
   거부, 정상값 통과. probe_window_geom 29×2+probe_settings ×2+self-test PASS.
2. **probe_window_geom은 docs/28 BOTH 쌍의 client 절반만 고정** — 서버 레이어
   위치는 외부 관측 불가(하네스 한계). list_windows가 보고하는 서버페이스 축으로
   대응 단정한 것이 한계 안의 최선.
3. **intake-focus 그림자 엣지** — 스폰 인테이크 focus는 push하지 않는다(구 동작
   보존). last-pushed 디바운스 설계 고유의 그림자이며 미러 수렴은 정상. 장기는
   인테이크 push 여부 자체의 재설계 후보.
4. **CommitChromeResize 실패도 ok:true** — 정규 리사이즈 경로와 선례 일치(실패
   반환 무시). 실패 가시화는 별도 과제.
5. **✅ 소각 (2026-09-23, docs/60 §5)** — HandleToolResult 결과 상한 16KiB→256KiB
   상향(args 캡과 대칭). 100KiB 단일행 스크립트 set_script→get_script 왕복
   100,205자 완전 복원 실측. probe_workshop 17체크+probe_app_tools 64체크 ×2.
6. **✅ 소각 (2026-09-23, probe_args_boundary 신설)** — 예측대로 raw 파이프
   하네스로 직접 단정 완료. agentctl이 못 실던 이유도 실증(CRT 32K 명령행
   한계 — 256KiB 페이로드는 와이어로만 운반 가능). 에러 순서가 증인: 캡 검사가
   후보 매칭보다 앞이라 존재하지 않는 앱("nope")이 경계 하회 시
   unknown_app_tool(캡 통과=룩업 실행), 초과 시 args_too_large(룩업 전 거부).
   실측 ×2 ALL PASS: 200KiB 통과 / 300KiB args_too_large / **정확 경계**
   내부 args 원문 `{"pad":"a"*n}` = 10+n 바이트 — n=262134(262144B, 캡은
   `>`라 통과) / n=262135(262145B, 거부) 1바이트 양쪽 단정. 와이어 무상한
   확인도 부수 실측: 파이프 전송 계층(JKPipeTransport)엔 프레임 상한이 없다.
7. **window_maximized 거절과 fit-scaled(ScaleX/Y≠1) resize dispW 산출은 런타임
   미단정** — 전자는 최대화를 배열할 도구가 없어 하네스 불가(코드 리뷰 검증),
   후자는 프로브 환경이 스케일 1 레이어뿐(CommitChromeResize 주석 정합 커버).
8. **probe_agent_events 4b는 라이브 유저 데스크탑 fired 카운터를 300-500ms
   윈도 ±1 정밀 단정** — 프로브 실행 중 유저 클릭 1회가 FAIL로 반전 가능(플레이크
   소지). 자각 기록된 한계.
9. **probe_window_geom Stop-ProbeProcs는 라이브 jkbridge까지 강제 종료** —
   NOTICE 문구에 jkbridge 종료 고지 반영(최종리뷰 반영). 폰 링크 무고지 사망 방지.

### 13.4 레슨

1. **계층 캡은 정합으로 소각된다** — 서버 앞단 캡이 앱 백스톱의 병목인 구조에서는
   백스톱이 와이어로 도달 불가라 존재 가치가 없다. 앞단을 앱 캡에 맞춰 들어 올리는
   한 줄이 워크숍 대형 스크립트 원문 통로를 연다(docs/60 §4 표 3행·레슨 6 갱신).
2. **디바운스는 "직전 상태"가 아니라 "마지막으로 알린 값"과 비교하라** —
   스폰 인테이크가 상태를 먼저 세팅하고 push가 나중에 일어나면, 상태 기준 비교는
   그 사이의 진짜 변화를 삼킨다. last-pushed-id + resolve 시에만 갱신이 정답.
3. **프로브가 스펙을 먼저 먹는다** — 캡 상향 같은 의도된 계약 변경은 기존 프로브의
   기대치를 스테일로 만든다. 회귀 FAIL 시 결함 가설보다 "어느 태스크가 이 기대를
   바꿨는가"를 먼저 대조.

## 14. 폰 미러 — 대화형 창 미러 + 탭→클릭 좌표 수학 (2026-09-27, docs/67 단 1 리파인)

**스펙 원장**: docs/67_workshop_vision.md §4 단 1 리파인 예약(§14로 소각). 사용자
결정: **대화형 미러**(폰이 창을 보고 탭이 실제 클릭으로 착지) + **모든 창 선택
가능**(워크숍 전용 아님). 원칙: **jkbridge 세션 모델에 얹기 — 새 채널 금지**
(docs/67 §6: 폰 미러를 새 채널로 만들면 jkbridge 이중화).

### 14.1 구조 — 도구 2종 + 웹 UI 패널

- **서버 도구 `window_frame`** (JKWindowServer): 창 표면을 JPEG base64로
  반환 — **디스크 기록 없음**(폰이 800ms 폴링하면 screenshots 디렉터리가
  쓰레기로 덮이는 capture_window 계약과 분리). 인자 `{id 필수, maxw 옵션}`.
  응답 `{"ok":true,"w","h","sw","sh","data":"<b64>"}`. 인코딩은
  `stbi_write_jpg_to_func`로 메모리 싱크(품질 75→60→45 하단, 900KiB b64
  예산 초과 시 하단, 전부 초과 시 `frame_too_large`). 축소는 최근접-이웃
  RGBA→RGB. 서면 픽셀은 1회 복사(클라 커밋 레이스 방지, capture 선례).
- **`list_windows` 확장키 `dw/dh`** — 표시 크기(display)=
  `llround(Width()*ScaleX)`(레이어 lookup, 무레이어면 Width 폴백). 기존
  `w/h`=서면 px **비파괴 유지**. item 버퍼 640→760.
- **jkagentd 3중 등록**(docs/53:14 삼각): kCoreToolsListJson 스키마+IsKnownTool+
  permissions 기본 allow+릴레이 args raw passthrough(게이트는 서버 몫). 셀프테스트
  34→35행.
- **kWebUi `#mirror` 오버레이**: 헤더 미러 버튼 + 픽커(list_windows,
  **textContent 전용** — 창 제목에 태그가 와도 안전) + ▲▼ 휠(dy=±1, dx=0) +
  800ms 폴링+in-flight 가드(mBusy) + `data:image/jpeg;base64` src
  (**base64 문자셋 선검** — data: URL은 이미지 mime 한정, XSS 포스트 유지).
  `/mirror` 슬래시 커맨드. 재접속 시 mBusy 해제.

### 14.2 좌표 수학 (착지 정확성의 전부 — 검증 완료)

- 캡처 픽셀은 **서면(surface) px** — 레이어 fit-scale 무시.
  `list_windows` x,y = 클라 논리 데스크톱 원점, w/h = 서면 px, **dw/dh = 표시 px**.
- `send_input`은 논리 데스크톱 점을 받아 `((x - X())/ScaleX)`로 서면 px 역변환.
- 폰 탭: `fx = (clientX-rect.left)/rect.width` → 데스크톱 점 = `win.x + fx*win.dw`.
- 서버 도달: `(fx*dw)/sx = fx*w` — **서면 정확 픽셀 착지, maxw 축소와 무관,
  1:1 가정 없음**(fit-scaled 레이어에도 정확). 구 서버(무 dw/dh) 폴백 `dw||w`.
- 프로브 c9가 서버 단정: `x = list.x + floor(dw*0.5)` → send_input ok.

### 14.3 결정들

| 항목 | 결정 | 근거 |
|---|---|---|
| 도구명 | `window_frame` | capture_window는 디스크 기록+shot 뷰어 계약 — 비접촉 구속, 별도 도구 |
| 인코딩 | JPEG q75→60→45 하단 | PNG 200-400KB×b64 1.33 → 1MiB 프레임 캡 위험; JPEG 60-150KB |
| 축소 | 서버 `maxw<=0`=원본, 폰이 `maxw:960` 명시 | 도구 순수성 — 데스크톱 에이전트는 원본 비전 리드백 가능 |
| 폴링 | 폰 800ms+in-flight 가드 | 펌프 400ms 폴링 → 실효 ~1fps, 서버 메인 스레드 인코딩 비용 계약(docs/58:319) |
| 프레임 상한 | kMaxFrame 불변+900KiB 예산 | 송신 경로는 64-bit length 지원 — 방어선만 |
| 권한 | kPermMatrix `allow` + **askCapable 편입** | capture_window/region과 동일 캡처 쌍 — 파일값 "ask"는 capture_ask 하드거부(승인 파킹 아님) |
| 트랜스크립트 | `mirror ` 라벨 프리픽스 reply 미기록 | base64가 매 폴링 적립되면 /report 256KiB 캡 붕괴 — **load-bearing** |

### 14.4 실측 레슨

1. **bridge WS 세션이 열리면 agentctl이 응답 없음** — 세션 슬롯 점유. 세션
   성립 후 도구 확인은 전부 WS 릴레이 프레임으로(probe c3-c11 일원화).
2. **askCapable 누락은 조용히 열화된다** — permissions 파일값 "ask"가
   askCapable 밖 도구에서 Allow로 열화(실측 결함, 프로브 c11이 잡음). 캡처류
   신설 시 askCapable 편입을 체크리스트화.
3. **프로브가 스펙을 먹는다(재현)** — probe_workshop/probe_conquest_workshop이
   마지막 슬롯 영속(단 1)과 충돌: 부팅 슬롯=사용자 실슬롯이면 프로브가 사용자
   슬롯에 기록. probe_workshop 18체크·probe_conquest_workshop에 **probe-ws
   전용 슬롯 격리** 픽스(conquest의 구 finally `.history` 전체 삭제는 사용자
   버전 리본 파괴 — 금지). 레슨: 슬롯 접촉 프로브는 전면 프로브 전용 슬롯+
   포인터 finally 원복.
4. **`.current_<app>` 위치** — scripts 디렉토리 안(DirOf(scriptPath_)). 프로브
   원복 경로를 state\로 잡으면 조용히 원복 실패(실측).

### 14.5 게이트

- probe_phone_mirror.ps1 신설 19체크 ×2 ALL PASS — list_windows strict
  JSON+dw/dh, JPEG SOI/문자셋/960·320 축소, 프레임 예산, bad id 2종,
  **탭 수학 서버 단정**, capture_window 무손상, permissions ask→capture_ask,
  rate-limit 맨 끝.
- 회귀 ×2: probe_jkbridge·probe_workshop(18체크, 격리 픽스 후)·
  probe_conquest_workshop(×2 CONQUEST PASS, 격리 픽스 후)·probe_agent_maximize
  + AppSelfTest 0 fail. 임베드 웹 JS node --check 통과.
- 커밋: 88f9551(T1 서버)·0154ce7(askCapable 픽스)·0a89832(T3 프로브)·
  1e102dc(T4 kWebUi)·b6bcced/c02e407(프로브 격리 픽스).
- **잔여**: 폰 실기기 눈확인(픽커→미러→탭 착지→휠→ask 승인 스트립) — 사용자,
  맨 뒤.

### 14.6 우클릭 — 길게 누름 (2026-09-27 사용자 보고 즉시 봉합)

- 폰 브라우저에 우클릭 수단이 없다(지뢰찾기 깃발류). 서버 `click` op의
  `button` 인자(3=오른쪽, MouseDown/Up keyCode로 실림)는 기존 계약 — 서버
  무수정, 폰 UI에 **길게 누름 500ms = 우클릭** 제스처 추가(커밋 89d126d).
- 방어: touchmove=손가락 미끄러짐 취소(오조작 방지), 브라우저 길눳
  contextmenu 억제, 길눳 발동 뒤 합성 click 이벤트 삁. /mirror 헬프 갱신.
- probe_phone_mirror c9b 신설(button:3 와이어 계약 서버 단정) — 20체크 ×2
  ALL PASS + probe_jkbridge PASS.

### 14.7 미러 중 픽커 자동 갱신 (2026-09-27 사용자 보고 즉시 봉합)

- 미러 중 앱 실행/종료가 픽커에 반영 안 돼 패널을 닫았다 다시 여는 불편. 폴링
  5회마다(≈4s) list_windows 재수집 — **집합(id+title) 비교로 변화시에만
  재렌더**(재렌더 도중 탭 흔들림 방지), 미선택 중에도 목록은 살아있게(커밋
  다음). 미러 중 창이 닫히면(window_not_found) mWin 해제+즉시 목록 재수집
  유도. 바에 새로고침 버튼, 선택 행 sel 강조. 게이트: probe_phone_mirror
  ×2 ALL PASS + probe_jkbridge PASS.

### 14.8 라이브 chat.json stub 잔여 — 폰 자연어 사망 (2026-09-27 사용자 보고 즉시 봉합)

- 폰에서 "지뢰찾기 띄워줘요" → "stub ok" (stub 엔진의 대답). 라이브
  `state\chat.json`이 `{"engine":"stub"}`로 고정 — 초기 jkbridge 기계 시험
  잔여가 그대로 살아있었다(프로브 스왑·복원은 정상, 원본이 stub).
- 픽스: `{"engine":"ollama"}` 기록 — LoadChatConfig는 **턴마다 재조회**
  (LlmTurnThread 첫 줄)라 브리지 재시작 불필요, 폰에서 즉시 재시도 가능.
  연기 실측: `ollama launch claude --model glm-5.3-flash:cloud` 실제 턴 성공
  (result 도달). `[claude-code:unrecognized_model]` 경고는 stderr 파이프로
  분리 수집 — 파서 무영향(엔진이 이미 분리 설계).
- 레슨: **config 잔여는 프로브 스왑·복원으로 잡히지 않는다** — 원본 자체가
  비정상이면 모든 게이트가 녹색으로 통과한다. 라이브 구성 파일(chat.json 등)
  변경은 사용자 보고 대응 때 원본부터 확인.

### 14.9 라이브 잔여 감사 전수 + 프로브 경화 2라운드 (2026-09-27)

- 사용자 요청("또다른 잔여가 없는지") 전수 감사 — 라이브 config/state 파일
  분류: 정상(theme/terminal/settings/jkbridge/bookmarks/notes/chat.json=
  ollama/permissions.json 전면 allow=사용자 설정/permission_set 기록 형식,
  trust trig_* 3팩+rate_probe, triggers_loaded 동기, layout_auto_idle=
  trig_idle 정상 산출물) / 잔여 소각(layout_e2e_final·layout_probe_e2e·
  tmp_smoke·toolsdump·bash.exe.stackdump 5종).
- **프로브 경화**: ①probe_agent_e2e/mcp save_layout 산출물 소각(사용자
  /restore 네임스페이스 오염) ②probe_agent_mcp deny 체크가 라이브 전면
  allow permissions.json에서 거짓 실패 — 권한 상태 프로브 소유(백업→프로브
  파일→finally 원복) ③probe_agent_trust 동일 클래스 — 라이브
  `trust_request:"allow"`면 서버가 ask 파이프라인을 생략하고 ok:true
  (:3927 Allow 분기)라 approval-request 체크가 영구 실패. try/finally
  원복로 5개 exit 경로 전부 커버. ×2 ALL PASS.
- **감사 오탈 2건 자기 교정**(검증 라운드에서 실측): ①rate_probe 행은
  잔여가 아니라 **정착 상태** — CMake가 build/apps/triggers/rate_probe.jkx를
  항상 패킹하고 팩 레코드 4종 유지가 프로브 설계 정착 상태. 필터하면
  팩·레코드 불일치로 신뢰 게이트에 걸림 → ratelimit 필터 블록 철회.
  ②sampletodo trust 레코드는 유령(대상 스크립트 소멸, 콘솔 앱 승인은
  trust.json 불사용) — 팩 전용 스토어 재기록에서 소실돼도 무영향.
- 레슨: 감사 자체도 게이트를 거친다 — 분류는 가설이고 ×2 실측이 판정.
  "레코드가 있으니 잔여" 추정(①)과 "있으니 정상" 추정(②) 모두 틀릴 수 있다.

### 14.10 폰 빈 응답 3겹 결함 — stub 세션 잔여 resume 연쇄 (2026-09-27 사용자 보고 즉시 봉합)

- 폰 "지뢰찾기 띄워주세요" → **(빈 응답)** ×2. chat.json ollama 픽스 후에도.
- **결함 사슬 3겹**: ①stub 엔진이 남긴 `session_id:"stub-1"`이 브리지 메모리에
  상주(chat.json 픽스는 프로세스 메모리를 못 고침) → 매 턴
  `--resume "stub-1"` ②claude CLI는 `--print` 모드의 무효 resume에서
  `type:"result", is_error:true, errors[]` 라인을 stdout으로 내고 종료 —
  파서는 `type=="result"`만 보고 `ok=true` 마킹, `result` 필드는 없어
  **빈 성공** ③레거시 EOF 폴백이 `stdoutBuf`(연결된 스트림 라인)를
  재파싱해 실패 판정을 `ok=true`로 되돌림 + 오류 라인의 신규 UUID를
  session_id로 심어 **다음 턴 resume도 연쇄 오염**(오류 UUID도 resume 불가).
- **픽스(JKLlmEngine.cpp)**: ①에러 결과 라인 판정
  (`subtype=="error_during_execution" || errors[0]`) → ok=false,
  `errors[0]` 문면을 result로, sessionId 공란(오류 라인의 UUID는 재개 불가
  대화) — 단락 평가로 추출이 생략되는 자체 버그도 봉합(항상 추출)
  ②`sawResult` 게이트 — 스트림 result 라인을 본 적 있으면 레거시 폴백
  불가(stub 엔진의 단일 echo-JSON 경로는 유지) ③**idle 킬 + job 트리** —
  기존 10분 타임아웃은 EOF *이후*에만 검사돼 ReadFile이 막힌 채 영영
  불발; PeekNamedPipe 폴링 루프로 stdout/stderr 동시 읽기( stderr
  파이프 채움 교착 제거)+10분 무데이터 시 TerminateJobObject(손자가 파이프를
  쥐고 살아남는 유출 근절 — KILL_ON_JOB_CLOSE).
- **픽스(jkbridge)**: OnLlmDone 실패 턴은 resumeSession_ 공란(자가 치유 —
  오염 id로 매 턴 재실패하지 않음, 다음 턴 신규 세션); 폰도 chat_done
  ok:0이면 저장한 세션 폐기.
- 실측(진단 스크립트 ×2 런, 실제 ollama 턴): 오염 resume 1턴=ok:0+claude
  오류 문면+세션 공란 → 2턴=신규 세션 ok:1 실답(≈7-10s). probe_jkbridge
  ×2 PASS(stub 레거시 경로 무손상). 커밋 (이 세션).
- 레슨: **프로세스 메모리 상주 상태는 파일 픽스로 치유 안 된다** —
  config 픽스 후에도 세션 id 같은 런타임 상태가 이전 엔진의 흔적을 물고
  재발시킨다. 게다가 claude CLI의 실패도 type:"result"로 온다 — 성공/실패
  판별은 is_error/errors 필드로. 진단 중 발견: 신규 세션에 맥락 없는
  질문("방금 답한 숫자는?")은 모델이 도구를 헤매며 수분 소모 — 진단
  질문은 도구 유혹 없는 단순 형태로.
- **2차 보강 — 조용한 resume 재시도**: 자가 치유 설계는 "스테일 기기당
  1회 실패 후 회복"이었는데 그 1회 실패도 보이지 않게 — resume 실패는
  claude **인자 검증 단계**(생성·도구 동작 전)에서 죽으므로 같은 프롬프트를
  신규 세션으로 1회 재시도해도 부작용 없음. OnLlmDone에서
  "No conversation found"/"--resume requires" 서명 검출 → 같은 턴을
  resume 없이 재시작, 첫 실패 chat_done은 미전송(폰은 성공 1건만 봄).
  재시도 턴은 lastTurnResume_ 공란이라 재귀 없음. BridgeSession에
  lastTurnText_/lastTurnResume_ 신설(모든 StartTurn 호출점에서 기록).
  실측: 오염 resume 턴이 실패 프레임 없이 ok:1 실답(진단 ×2), 
  probe_jkbridge PASS.

## 14.11 미러 탭 좌표 진실원 — 레터박스 재계산 + 길눳 경합 정리 (2026-09-27)

사용자 실전 보고: "지뢰찾기하는데 마우스 좌표 안맞는 것 같고, 길게 눌렀을 때
오른쪽 버튼 인식이 안 되는 것 같다". 두 증상의 공통 뿌리는 하나였다.

- **뿌리 — object-fit:contain 상자=이미지 착각**: `#mimg`가
  `flex:1; width:100%; object-fit:contain`이라 가로폭 창+세로폰 화면에서
  상하 레터박스가 크게 생기는데, 탭 수학이 `getBoundingClientRect()`(상자)를
  써 fx/fy가 그림이 아니라 레터박스 포함 상자 기준이 됨 → 탭이 어긋나고,
  길눳 우클릭도 **발사는 되지만 엉킨 곳에 착지**해서 "안 되는 것"으로 보임.
- **픽스 — `mirrorRect()` 진실원**: 자연 크기(naturalWidth/Height)와 상자의
  min 비율로 실제 렌더 영역을 재계산
  (`left: r.left+(r.width-iw*scale)/2` …). 첫 프레임 로드 전(natural 0)은
  탭 무시. mirrorTap·mirrorTapAt(길눳/▲▼) 전부 이 진실원으로 통일.
  수학 검증: 상자 390×500+이미지 800×600 → rect
  {left:0, top:103.75, w:390, h:292.5}; 이미지 좌상단 탭 fx:0 fy:0,
  우하단 ≈1.0/1.0, 레터박스 탭 fy −0.29로 **범위 밖 무시**(레터박스 유령
  클릭도 사라짐).
- **길눳 경합 방어 보강**: preventDefault(touchstart)는 클릭 생성까지 죽여
  일반 탭을 망침 — 하지 않고, 억제는 CSS로
  (`-webkit-touch-callout:none; user-select:none; touch-action:none;`) +
  `draggable="false"`. contextmenu preventDefault(크롬 이미지 길눳 메뉴
  억제)는 기존 그대로 — touch-action:none이어도 클릭은 생성됨 실측.
- **부수 — `Cache-Control: no-store`**: 폰 브라우저가 서빙 HTML을 캐싱해
  브리지 갱신 후 스테일 UI를 먹는 사고 1건 — HttpReply 헤더에 추가.
- 게이트: probe_phone_mirror ×2 ALL PASS + probe_jkbridge PASS.
  레슨: **CSS object-fit이 좌표 수학의 일부다** — 렌더 사각형과 DOM 상자는
  다른 진실원이며, 비율 좌표를 파는 쪽은 렌더 진실원을 직접 계산해야 한다.

## 14.12 길눳 미세 떨림 + 우클릭 토글 (2026-09-27, 사용자 재보고 2연속)

§14.11 배포 후 사용자 재보고: "좌표는 잡혔는데 길게 눌러 우클릭 보내기가 안 된다".
전선·서버·앱 역직렬화 전 경로 실측상 정상(p.keyCode=3 → 클라 역직렬화에서
`ev.detail=payload.keyCode` 스왑 → ImGui 백엔드 RIGHT 매핑) — 결함은 폰 제스처
자체였다: touchmove **1px 미세 떨림에도** 타이머 취소 → 실제 폰은 길게 누르는
동안 1px급 touchmove가 거의 항상 온다.

- **슬롭 임계치**: 터치 시작점 기록(lpX/lpY), touchmove에서 10px 초과 이동
  (손가락 미끄러짐)만 취소 — 미세 떨림은 관용. 수학 검증: ≤10px TOLERATE,
  11px CANCEL.
- **우클릭 토글(사용자 요청 — "편법이 아니라 직접 오른쪽 클릭 이벤트")**:
  미러 바에 `우클릭` 버튼 — 켜면 다음 탭 1회가 `button:3`으로 발사되고
  자동 해제(모드 잊음 방어). 꺼져 있으면 탭=왼쪽이라 양쪽 클릭 모두 가능.
  길눳(슬롭 픽스)은 자연 제스처 경로로 병존. iOS는 브라우저 contextmenu
  합성이 불안정해 토글이 확정 경로.
- 게이트: probe_phone_mirror ×2 ALL PASS + probe_jkbridge PASS.

## 14.13 분할 모드 — 채팅과 미러 동시 보기 (2026-09-27, 사용자 요청)

미러가 전체화면 오버레이라 채팅을 가리던 것 → 미러 바에 **`분할` 토글**:
`#mirror.split { top:45% }`(픽커 18vh)으로 화면 아래 절반에 미러, 위쪽에
채팅(log+입력)이 남아 계속 대화 가능. 탭 좌표는 `mirrorRect()`가 실렌더
사각형을 매번 다시 계산하므로 레이아웃 무관 정확(§14.11 진실원의 부수 이익).

- **운영 레슨(정정)**: 프로브 종료 정리는 라이브 스택(jkdesktop 포함)을 죽인다.
  복원을 클로드가 Hidden 서버로 대행하면 싱글 인스턴스 가드를 클로드가 쥐어
  사용자 콘솔 기동이 거절된다(사용자 지적 2회). **확정 규칙: 서버 소유권은
  사용자 콘솔 — 클로드는 jkdesktop --server를 절대 띄우지 않고, 프로브 런
  뒤 복원은 jkbridge만 한다. 서버 재기동이 필요하면 사용자에게 부탁한다.**

### §14.13 정정 2 — 세로 분할 + 입력창 가림 픽스 (83daa4c 이후)

사용자 재보고: "분할은 있는데 입력창이 없다 + 세로 분할이 더 나을 것". 입력창
(#inrow)은 flex 열 맨 아래라 `top:45%` 미러에 가려졌던 것. 재설계: 채팅 열
(#hdr/#log/#appr/#inrow)을 `#chatcol`로 감싸 **세로 분할(좌/우)**로 전환 —
`body.msplit { flex-direction:row }`, `#chatcol` 왼쪽 절반(입력창 포함, 우측
경계선), `#mirror.split { inset:0 0 0 50% }` 오른쪽 절반, mbar 랩 허용.
이 변경은 순수 클라이언트 UI — 전선·좌표 경로 무접촉, 프로브 생략(서버
소유권 규칙에 따라 사용자 서버 존중) + 사용자 눈확인이 곧 게이트.

## 14.14 키패드 — 키보드 앱 조작 + 한/영 텍스트 (2026-09-27, 사용자 질문→요청)

"테트리스처럼 키로 조작하는 앱은?" — 후보 3(온스크린 패드/스와이프 매핑/
자연어 중계) 중 **온스크린 패드** 채택(사용자 확정). 사용자 후속 질문
"한/영 모두 가능?" → 텍스트 바까지 확장. 서버 무수정 — 기존 send_input의
`key` op(action tap/down/up 분리 실측, docs/62 §3.1)와 `type` op(UTF-8 Char
63B 분할, :1967)만 소비.

- **D-pad**: ◀▲▼▶+␣+⏎+⌫, 미러 이미지 하단 오버레이(#mwrap/#pad). 터치
  down→`key down`, 뗌→`key up` — **홀드 지원**(테트리스 소프트 드롭). 키
  코드=SDL 키코드(◀ 0x40000050 등 4종 단정). 터치 preventDefault로 합성
  마우스 이벤트 이중 발사 방지, 데스크톱 마우스 down/up 병기.
- **한/영 텍스트**: 폰 IME에서 조합 **완료 문자열**을 `type` op로 전송 — PC
  앱의 한글 자판 상태와 무관한 완성형 진입(HangulAutomata 경유 없음).
  Enter(isComposing 제외)+전송 버튼. `#mwrap` 감싸기는 mirrorRect 진실원에
  영향 없음(mImg rect만 사용).
- 선언형 앱별 매핑(스와이프=이동 등)은 워크숍 대형에 예약 — 범용 패드가
  기반. 게이트: JS 문법+키코드 단정; 폰 실기기 눈확인 대기.

### §14.14 정정 — 패드 배치: 겹침 없는 하단 고정 (사용자 질문 "플로팅 vs 하단")

초판은 이미지 **하단 겹침 오버레이**였는데 겹침은 테트리스·지뢰찾기의 하단
행=전장을 정확히 가리고, 플로팅은 어느 자리에 놓든 결국 화면을 가린다+
드래그 오조작. 재배치: 패드를 **이미지 아래 별도 flex 영역**으로 — 이미지가
스스로 줄어 겹침 0, 위치 항상 고정(손 기억), 엄지 도달 유지.

## 14.15 데스크톱 와이드 뷰 — 미선택 미러의 기본 화면 + id=0 히트테스트 탭 (2026-09-27, 사용자 제안)

"앱 선택 안 했을 때 jkdesktop 전체 화면을 보여주면 어때?" — 채택. 텍스트 픽커
만으로는 "지금 뭐가 떠 있는지" 안 보이므로 미선택 상태의 기본 화면을 데스크톱
와이드 프레임으로.

- **서버 window_frame id=0**: capture_region 파이프라인의 크롭 없음 변형 —
  Composite(false)+SDL_RenderReadPixels 전체 프레임버퍼 → EncodeLayerJpegB64
  재사용. 응답에 `"desktop":1` + 논리 데스크톱 dw/dh. 디스크 기록 없음 유지.
- **서버 send_input id=0 = 히트테스트 전달**: 논리 데스크톱 점을
  compositor HitTest → 최상위 클라이언트에 click/wheel 전달. p.surfaceId는
  `client->Id()`로(데스크톱 모드 수신자), BuildSendInputOp는 id=0 허용
  (음수만 bad_target). clientsMutex_ 보유 경로라 직접 순회(레슨 35).
  셸(태스크바)·캡처 오버레이는 기존 쌍검사 그대로 배제 — **태스크바 클릭은
  미지원**(선결제: 크롬 상태 오염 위험, 앱 실행은 픽커/LLM 경로).
- **폰**: 미선택 폴링이 `window_frame {id:0}` — mDeskW/H 저장, 탭 수학은
  fx*mDeskW. 스테일 프레임 이중 가드(픽 도중 도착한 데스크톱 프레임/선택
  해제 직후 도착한 창 프레임 폐기). `데스크톱` 버튼=선택 해제·복귀. 창이
  닫히면 기존 window_not_found 경로가 자동으로 데스크톱 뷰로 떨어짐.
- **프로브 갱신**: c8 id=0 bad_request 단정은 계약 변경(데스크톱 뷰)으로
  id=-1→bad_request로 교체 + c8-desktop-frame(ok+desktop:1+dw/dh+SOI)+
  c9c 데스크톱 탭(창 중심 ok / 빈 공간 window_not_found) 2체크 추가.
  probe_phone_mirror ×2 ALL PASS + probe_jkbridge PASS + jkdesktop test 0.
