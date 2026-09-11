# LLM 토큰 스트리밍 구현 플랜 (docs/31 §6 후속)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** jkchat의 claude 턴을 토큰 단위로 라이브 타이핑 — stream-json 파셜 메시지를 PostMessage 청크로 전사.

**Architecture:** claude CLI `--output-format stream-json --verbose --include-partial-messages`의 line-delimited 이벤트를 LlmThread가 읽으면서 파싱 — `content_block_delta`/`text_delta`를 `WM_APP_LLM_DELTA`(WM_APP+2)로 UI 스레드에 전송해 트랜스크립트에 즉시 어펜드. `result` 이벤트에서 ok/session_id/result 확정 (기존 완료 경로). stub/ollama-미지원 경로 불변, state/chat.json 스키마 불변.

**Tech Stack:** Win32 (jkchat.exe), jk::agent::AgentJson (quickjs throwaway reader), 기존 anonymous pipe 워커.

**Spec:** docs/31 §5-6 (chat LLM delegation + streaming 후속)

## Global Constraints

- stub 엔진은 echo-JSON 그대로 (스트리밍 이벤트 없음 — 파서가 일반 JSON 라인도 견뎌야 함).
- state/chat.json 스키마 불변 (engine/model/directory/skip_permissions).
- 세션 연속성(--resume) 불변 — result 이벤트의 session_id 사용.
- UI는 절대 블록하지 않음 (PostMessage 청크 모델).

---

### Task 1: BuildEngineCmd 스트리밍 플래그

**Files:**
- Modify: `engine/tools/jkchat/main.cpp:96-127` (BuildEngineCmd)

**Interfaces:**
- Produces: claude/ollama 엔진의 claudeArgs가 `--output-format stream-json --verbose --include-partial-messages`를 포함 (stub은 불변 echo-JSON).

- [ ] **Step 1: claudeArgs 교체** — `--output-format json` →
  `--output-format stream-json --verbose --include-partial-messages`
  (`--verbose`는 stream-json -p 모드에서 필수; `--include-partial-messages`가
  stream_event/content_block_delta를 켠다).

- [ ] **Step 2: 커밋** — `feat(chat): stream-json engine flags`

### Task 2: LlmThread 라인 파서 + 델타 PostMessage

**Files:**
- Modify: `engine/tools/jkchat/main.cpp` (LlmThread, WM_APP 상수)

**Interfaces:**
- Produces: `WM_APP_LLM_DELTA = WM_APP + 2` (lparam = heap `std::wstring*`,
  UI가 free); LlmTurnResult에 `streamed` 필드 (델타 수신 여부 — 완료 로그 형식
  결정).

- [ ] **Step 1: stdout 읽기 루프 개편** — 청크를 stdoutBuf에 누적하며 완결 라인
  ('\n' 경계)을 즉시 파싱. 파서:
  - line에 `AgentJson lineJson(line)`; `GetStr("type", type)`.
  - `type=="stream_event"` → `GetObjRaw("event","delta")` → 하위 AgentJson의
    `"text"` → **UTF-8 → UTF-16 변환 후** heap wstring으로 PostMessage
    (WM_APP_LLM_DELTA). 텍스트 델타만.
  - `type=="result"` → ok/session_id/result 저장 (기존 규약 유지) + 결과 텍스트는
    델타 누적과 무관하게 result 필드가 최종 진실원.
  - `session_id`는 어떤 이벤트에 있든 마지막 값 채택.
  - stub의 단일 echo-JSON 라인은 type이 없어 파서가 무시 — EOF 후 기존
    "전체를 AgentJson으로" 폴백이 그대로 처리 (stub 회귀 보호).
- [ ] **Step 2: 완료 처리** — `out->streamed = (델타 1개라도 수신)`. 기존 result
  필드 로직 유지; streamed면 UI가 전문 재출력하지 않음.
- [ ] **Step 3: 빌드** — kill exes → `cmake --build build` (필터 금지) → mtime 확인.
- [ ] **Step 4: 커밋** — `feat(chat): worker parses stream-json deltas`

### Task 3: UI 라이브 타이핑

**Files:**
- Modify: `engine/tools/jkchat/main.cpp` (WndProc, Log 유틸)

**Interfaces:**
- Consumes: WM_APP_LLM_DELTA (heap wstring), WM_APP_LLM_DONE의 `streamed`.

- [ ] **Step 1: LogRaw** — 줄바꿈 없이 트랜스크립트 끝에 어펜드 (Log의 "\r\n"
  접두만 뺀 버전).
- [ ] **Step 2: WM_APP_LLM_DELTA** — 첫 델타면 `Log(L"[LLM] ")` 선행 후 LogRaw;
  이후 델타는 LogRaw만. 프래그먼트 delete.
- [ ] **Step 3: WM_APP_LLM_DONE** — 기존 로직 + `streamed`면 result 대신
  `Log(L"[LLM 완료]")`; 스트림이면 줄바꿈 정리(LogRaw 후 L"\r\n").
- [ ] **Step 4: 커밋** — `feat(chat): live token streaming in transcript`

### Task 4: 프로브 확장 + 실엔진 스모크 + 문서

**Files:**
- Modify: `engine/tools/probes/probe_agent_chat_llm.ps1` (stub 회귀 유지)
- Create: `docs/36_desktop_agent_chat_streaming.md`
- Modify: `docs/31_desktop_agent_chat.md` §6 표시 갱신 (구현됨 → docs/36)

- [ ] **Step 1: probe_agent_chat_llm 재실행** — stub 엔진 2/2 PASS 유지 확인
  (stub은 스트림 이벤트가 없으므로 기존 판정 그대로 통과해야 함).
- [ ] **Step 2: 실엔진 스모크** — cfg.engine=claude (cfg 파일 편집) → jkchat 기동
  → 자연어 턴 → 트랜스크립트에 [LLM] 라이브 델타 + [LLM 완료] 확인 (GetWindowTextW
  판정, 레슨 29/30). ollama 엔진도 stream-json 동작 확인 (기본 엔진).
- [ ] **Step 3: 회귀** — jkagentd selftest + 전 프로브.
- [ ] **Step 4: 문서 + 커밋/푸시** — `feat(chat): token streaming docs/36 + probe`.