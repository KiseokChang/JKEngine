# docs/36 — 데스크톱 에이전트: 채팅 LLM 토큰 스트리밍

날짜: 2026-09-11
이전: docs/35 (스크린샷), docs/31 §5-6 (채팅 LLM 위임 + 스트리밍 후속)
플랜: docs/superpowers/plans/2026-09-11-agent-chat-streaming.md

## 1. 요약

jkchat의 claude 턴이 **토큰 단위로 라이브 타이핑**된다. claude CLI의
`--output-format stream-json --verbose --include-partial-messages` (세 플래그
모두 필수 — `--verbose`는 stream-json의 -p 모드 요구사항)가 내보내는
line-delimited 이벤트를 워커가 읽으면서 파싱하고, `content_block_delta`의
`text_delta`를 즉시 PostMessage로 UI에 전사한다. stub/ollama-구성 불변,
state/chat.json 스키마 불변, 세션 연속성(--resume) 불변.

## 2. 구현

### 2.1 엔진 플래그 (BuildEngineCmd)

- claude/ollama 엔진 공통 (ollama도 `ollama launch claude --`로 claude CLI를
  감싸므로 같은 stream-json 경로 — 실측으로 확인).
- stub은 echo-JSON 그대로 — 스트림 이벤트가 없는 경로가 반드시 살아있어야
  기계 회귀가 가능.

### 2.2 워커 (LlmThread)

- **읽기 루프 안에서 라인 파싱** — 청크를 stdoutBuf에 누적하되 소비는 별도
  offset 스캔 (`find('\n', lineScan)`)으로 하고 버퍼는 intact. 왜냐면 EOF 후
  레거시 폴백(전체 버퍼를 AgentJson으로 파싱)이 같은 버퍼를 필요로 하기 때문.
  첫 구현이 버퍼를 erase해 stub 폴백이 빈 버퍼를 파싱해 회귀 — 실측으로 잡음.
- `ParseStreamLine`: `type=="stream_event"` → `GetObjRaw("event","delta")` →
  하위 AgentJson의 `"text"` → UTF-16 변환 후 heap wstring PostMessage
  (`WM_APP_LLM_DELTA`, WM_APP+2). `type=="result"` → ok/result/session_id
  (result 필드가 최종 진실원). session_id는 있는 이벤트마다 채택 (init/result
  동일값).
- 델타 없이 result만 오면 기존 완료 경로와 동일하게 동작 (하위 호환).
- AgentJson은 2레벨 객체 접근자만 제공 — 3레벨(event.delta.text)은 raw 추출 +
  재파싱으로 해결 (docs/34 레슨 39의 "평면으로 내림"과 같은 계열 판단).

### 2.3 UI

- `LogRaw` — 줄바꿈 없이 트랜스크립트 끝 어펜드.
- 첫 델타가 "[LLM] " 라인을 열고(`g_streamLineOpen`), 이후 델타는 같은 줄에
  이어짐 → 라이브 타이핑.
- `WM_APP_LLM_DONE`: 스트림이 열려 있으면 줄 정리("\r\n") 후 "[LLM 완료]" —
  **result 전문 재출력 안 함** (`streamed` 플래그). 스트림이 없던 턴(stub,
  파서 실패)은 기존처럼 result 전문 출력.

## 3. 검증

- **stub 회귀**: probe_agent_chat_llm.ps1 2/2 (stub-turn/busy-line) — 첫 구현의
  버퍼 erase 버그가 여기서 걸렸고(레슨 44), 오프셋 스캔 수정 후 PASS.
- **실엔진 스모크**: tools/probes/smoke_llm_stream.ps1 3/3 —
  ① stream-line: "[LLM] " 라이브 델타 라인 존재, ② turn-done: "[LLM 완료]",
  ③ no-retype: 완료 후 result 재출력 없음. 기본 엔진(ollama/kimi-k2.7-code:cloud)
  경로에서 실측 (~1 turn 실쿼터).
- **전체 회귀**: jkagentd selftest 0, mcp/e2e/palette/chat/chat_llm/triggers/
  notify/triggerctl/shot 전부 PASS.

## 4. 제한 / 후속

- 툴 사용 블록(tool_use)은 텍스트 델타가 아니므로 트랜스크립트에 claude의 도구
  호출 과정은 안 보임 — 최종 텍스트만 스트리밍. 과정 표시는 assistant 이벤트의
  tool_use 파싱 후속.
- thinking 모델의 thinking 델타는 텍스트가 아니라 무시됨.
- 델타 파싱이 AgentJson(quickjs 런타임 2회/라인) — 토큰 속도에서 무시 가능하나
  초고속 스트림에서 부담되면 경량 파서로 교체.

## 5. 레슨

1. **스트림 파서가 버퍼를 소비하면 EOF 폴백이 굶는다** — 라인을 erase하며
   파싱하면 stub의 일반 JSON(레거시 전체 파싱 경로)이 빈 버퍼를 만났다.
   소비형 파싱에는 스캔 오프셋을 쓰고 원본 버퍼를 보존.
2. **엔진 래퍼(ollama)도 claude 플래그를 통과한다** — `ollama launch claude --
   <claude args>` 구조상 스트리밍 플래그는 claude 몫이라 래퍼 무수정 동작.
   래퍼 뒤의 실제 CLI를 보고 설계하면 이중 경로를 만들지 않는다.
3. **라이브 UI 전사는 3부 구성** — "첫 프래그먼트가 라인을 열고 / 이후는
   raw 어펜드 / 완료 신호가 줄을 닫는다". 프래그먼트마다 새 줄을 로그하면
   토큰당 한 줄이 되어 판독 불능.

## 6. 파일

- `tools/jkchat/main.cpp` — BuildEngineCmd 플래그, ParseStreamLine,
  LlmThread 오프셋 스캔, LogRaw, WM_APP_LLM_DELTA/스트림 인지 DONE.
- `tools/probes/smoke_llm_stream.ps1` — 실엔진 스트리밍 스모크.
- `tools/probes/probe_agent_chat_llm.ps1` — stub 회귀 (변경 없이 재검증).