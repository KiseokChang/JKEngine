# jkbridge — 폰(브라우저)에서 jkchat 기능 설계 — 2026-09-18

사용자 확정: ①폰 클라이언트 = **브라우저 웹 UI**(앱 설치 없음) ②기능 범위 =
jkchat 전 기능, 단 브리지는 **범용 도구 릴레이**로 설계해 files/notes 등
후속 도구가 같은 프레임으로 확장 가능 ③네트워크 = **LAN 한정 + URL 토큰**.

## 1. 배경과 비목표

- 현재 그림: jkchat(PC 전용 Win32) → `JKAgentClient` → named pipe
  (`JKWindowServerPipe`) → jkwinserver. LLM(claude CLI)은 jkchat이
  **로컬 서브프로세스**로 스폰. 폰은 pipe에 직접 붙을 수 없다.
- 해법: PC에 **jkbridge.exe** 게이트웨이 — HTTP(정적 웹 UI) + WebSocket.
  폰 브라우저 ↔ WS ↔ jkbridge ↔ pipe ↔ 서버.
- 비목표: 외부 인터넷 접속(WireGuard/TLS 프록시 — 후속), 파일 전송, 푸시
  알림(WebView 래퍼 앱의 영역), jkwinserver 자체에 WS 내장(외부 프로세스
  원칙 유지 — jkchat과 같은 보안 격리).

## 2. 아키텍처

```
폰 브라우저(단일 HTML)
   │  HTTP GET /            (정적 UI, 토큰 무검증)
   │  WS  /ws?token=...     (토큰 검증 후 업그레이드)
   ▼
jkbridge.exe (engine/tools/jkbridge, 콘솔 앱)
   │  HTTP+WS 수기 서버(Winsock2, 스레드/연결)
   │  세션당: JKAgentClient 1개 + 펌프 스레드 1개 + LLM 엔진 1개
   ▼
JKLlmEngine (engine/src/agent — jkchat에서 추출한 공용 모듈)
   │  claude CLI 서브프로세스 + stream-json 파싱 + --resume
   ▼  named pipe
jkwinserver (승인 파이프라인 — 기존 그대로)
```

- 여러 WS 세션 동시 허용(상한 4) — 각자 자기 JKAgentClient 연결. 서버 입장에선
  jkagentd/jkchat과 같은 다중 control-only 클라이언트.
- jkchat의 "서버·렌더러 밖 별도 프로세스" 원칙 계승 — jkbridge는 서버 코드를
  몰라도 되고, 서버 변경은 pipe 프로토콜 뒤에 숨는다.

## 2.1 JKLlmEngine 추출 (jkchat 리팩터)

`tools/jkchat/main.cpp`의 LLM 부분(ChatConfig, LoadChatConfig,
BuildEngineCmd, ParseStreamLine, LlmThread, LlmTurnResult)을
`engine/src/agent/JKLlmEngine.h/.cpp`로 이동. UI 종속(PostMessage)은
**콜백으로 대체**:

```cpp
struct LlmTurnResult { bool ok=false, streamed=false; std::string result, sessionId; };
class JKLlmEngine {
  using DeltaFn = void(*)(const std::string& utf8, void* user);
  using DoneFn  = void(*)(LlmTurnResult&&, void* user);
  bool StartTurn(promptUtf8, DeltaFn, DoneFn, void* user);  // 스레드/턴, busy 게이트
  bool Busy() const;
  const std::string& SessionId() const;  void ResetSession();
};
struct ChatConfig { engine, model, skipPermissions, directory; };  // state/chat.json
```

- 콜백은 워커 스레드에서 온다 — 구독자가 스레드 안전하게 처리(jkchat은
  PostMessage로 위임, jkbridge는 WS 송신 큐에 넣음). **기존 동작 보존이
  원칙**: jkchat은 콜백이 PostMessage(WM_APP_LLM_DELTA/DONE)만 하도록
  얇게 재연결 — 프로브(probe_agent_chat, smoke_llm_stream) 회귀로 검증.
- stub 엔진(`"engine":"stub"`) 전수 유지 — 프로브가 네트워크 없이 회귀.
- Utf8↔Wide 유틸은 모듈 내부에 로컬 사본(jkchat 로컬은 그대로 — UI 전용).

## 2.2 WS 프로토콜 (범용 릴레이)

JSON 한 줄 = 1 프레임(WS text). **채팅 특화 명령 집합 대신 범용 4종** —
후속 도구(files/notes/settings) 확장은 웹 UI가 tool 프레임만 늘리면 된다.

폰 → 브리지:
- `{"type":"chat","text":"..."}` → LLM 턴 시작(스레드/턴). busy면
  `{"type":"error","text":"busy"}`.
- `{"type":"tool","tool":"<t>","args":{...},"label":"..."}` → SendQuery
  (논블로킹). reply는 `reply` 프레임으로 회신. **모든 서버 도구가 이 한
  프레임으로 열린다** — 승인 게이트는 서버가 원래대로 강제(폰은 approve만
  할 수 있음).
- `{"type":"approve","request":N,"decision":"allow"|"deny"}`
- `{"type":"hello","token":"...","resume_session":"<claude id>"}` — 첫
  프레임에서 토큰 검증. 검증 실패 → close. `resume_session`이 있으면
  LLM 세션 이어가기(폰이 localStorage에서 유지).

브리지 → 폰:
- `{"type":"hello","ok":true,"busy":false}`
- `{"type":"stream","text":"<delta>"}` — LLM 토큰 델타(워커→WS 송신 큐)
- `{"type":"chat_done","ok":1,"streamed":1,"result":"...","session_id":"..."}`
- `{"type":"reply","label":"...","json":"<원문 reply JSON>"}`
- `{"type":"event","topic":"...","json":"<원문 event JSON>"}` —
  approval_request / approval_resolved / agent.notify / window.* 전체.
- `{"type":"error","text":"..."}`

슬래시 커맨드(/list /launch /theme …)는 **웹 UI JS가 tool 프레임으로 매핑**
— 브리지는 슬래시를 모른다. jkchat Submit의 매핑 표를 JS로 옮기는 것뿐.
/close·/restore의 pre_undo → save reply → close 2단 체인도 JS에서
(reply 프레임의 queryId 상관관계로) 동일 구현.

## 3. 승인 흐름 (기존 파이프라인 무수정)

서버의 ask 파킹/승인 파이프라인은 **수정 없음** — jkbridge는 jkchat과 같은
control-only 클라이언트일 뿐이다. approval_request 이벤트 → 폰 승인 스트립
(큐 구조 — jkchat EnqueueApproval/승인 큐 순환을 JS로 이식), [허용][거부] →
approve tool → approval_resolved 이벤트로 스트립 재무장. 퍼가닝 문구(kind별
전용 라벨 — opus MAJOR-1 계승)도 JS에 kind 분기로 이식.

## 4. HTTP 서버 + 보안

- 포트 기본 8790, `state/jkbridge.json` {token, port} 저장/로드.
  첫 실행 시 토큰(CSPRNG 32-hex) 생성, 콘솔에 `http://<IP>:8790/?token=...`
  출력(전면 어댑터 IP 열거). Ctrl+C로 종료.
- HTTP: GET `/` → 웹 UI(임베디드 HTML 문자열 — exe 자족), GET `/health` →
  `ok`. 그 외 404. 토큰은 **WS 핸드셰이크 쿼리에서만 검증**(UI 정적 서빙은
  무검증 — 토큰은 도구 실행 권한이고, 실패 시에도 페이지만 보이면 안전).
- 토큰 검증 실패: 연결 즉시 close + IP당 실패 레이트리밋(10회/60초 초과 시
  60초 드롭).
- 프레임 상한 1MiB(초과 = 연결 닫기), 텍스트 프레임만 수용.
- WS 핸드셰이크: Sec-WebSocket-Accept = SHA-1+base64(수기 — ~60+30줄).
  Upgrade가 아니면 일반 HTTP로 처리.

## 5. 웹 UI (단일 HTML, exe에 임베디드)

- 모바일 세로 배치: 트랜스크립트(스크롤) / 승인 스트립(조건 표시) / 입력+보내기.
  다크 배경, 시스템 폰트(한글), 레이아웃은 100dvh. 슬래시 커맨드 자동완성
  없음(MVP — 입력 힌트는 /help 텍스트).
- 트랜스크립트: `> ` 입력, `[LLM]` 스트리밍 라인(델타 append), `[label] reply`,
  `[알림]`, `[승인]` — jkchat 로그 형식 계승. 대화 이력은 **localStorage**
  (폰 측 보관 — 브리지는 무상태), `resume_session`으로 claude 세션만 이어감.
- WS 재접속: 끊기면 2초 간격 자동 재접속(토큰 재전송, resume 포함).

## 6. 오류 처리

- 서버(pipe) 연결 끊김: 펌프가 재접속 시도(jkchat Pump의 재시도 패턴), 폰에
  `[!] 연결 끊김` 이벤트 프레임.
- LLM 스폰 실패/stderr: jkchat 선례 그대로(결과에 stderr 꼬리 400자).
- WS 끊김 중 LLM 완료: 결과는 세션 메모리에 1개 유지(마지막 chat_done) —
  재접속 hello의 `ok` 응답에 붙여 전달(유실 방지, 그 이상은 큐잉하지 않음).
- jkwinserver 부재: 도구/채팅은 error 프레임으로 회신, 프로세스는 유지.

## 7. 테스트

- `probe_jkbridge.ps1`: probe_agent_chat 관례(서버 수명 관리 + agentctl
  상관검증). jkbridge.exe를 **임시 디렉터에 복사**해 state/chat.json stub
  엔진 구성(실제 jkchat chat.json 오염 방지 — docs/54 레슨⑥).
  체크: ①HTTP GET / 200+HTML ②토큰 없는 WS = 거부 ③올 토큰 hello ④stub
  chat 스트림+done ⑤tool 프레임 list_windows(서버 기동 상태) ⑥approve
  라운드트립(ask 게이트 close — probe_agent_chat의 봉합 패턴) ⑦동시 세션
  상한 ⑧프레임 상한. ×2 연속 ALL PASS 관례.
- jkchat 회귀: probe_agent_chat.ps1 + smoke_llm_stream.ps1 재실행 —
  리팩터(콜백화) 후 동작 동일 검증.

## 8. 한계

- 폰 백그라운드/화면꺼 시 WS 단절 — 재접속+resume으로 복구(전달 유실은
  §6 1개 유지). 푸시 알림은 웹이 못 한다(WebView 래퍼 앱의 몫 — 비목표).
- HTTPS 아님(자체 인증서 불필요 — LAN+토큰, 클립보드로 붙여넣는 URL).
- 인증은 토큰 단일 요소 — LAN 신뢰 전제.