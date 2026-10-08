# 채팅 자연어 승격(cloud) 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 폰 채팅(jkweb/jktalk) 자연어가 cloud 모델 턴(~3-10s)을 거쳐 실제 동작으로 이어진다.

**Architecture:** 기존 tool dispatch 계약 무변 — 라우터의 뇌 자리만 cfg 선택형 LLM으로 교체. 동기 브리지(비동기 StartTurn 위 CV 래퍼), "ollama-direct" 폰 경로(`ollama run`/`/api/chat`), 실패 시 기존 stub 라우터 폴백.

**Tech Stack:** C++(jkcore 계열), Termux ollama, bash probes(ssh/tar 계약), selftest 1n 케이스.

**Spec:** docs/superpowers/specs/2026-10-08-chat-llm-promotion-design.md

## Global Constraints

- **순서: T1(조달 실측) 먼저**, 구현 배선(T2+)은 chat-close-fix 라인(리뷰+폰 재배포) 완전 착지 후 dispatch — 배포 원천 오염 재발 금지(docs/81 §3 #11).
- 사내 IP 커밋 비노출 — 폰 접속은 `PHONE_HOST` 환경변수 계약, `ssh -i ~/.ssh/termux_jkengine -p 8022` LAN 한정.
- 폰 ssh 계약: < /dev/null 금지, `$TMPDIR` 임시, pkill 대상 브래킷.
- vplayer 관련 없음. permissions.json 전면 allow 런타임 파일 수정 금지.
- amend 금지, origin main 직행, 커밋 전 rev-parse 실측.
- 가짜 결제 기록 금지 — 사용자 육안 선언만 결제.

---

### Task 1: 폰 ollama 조달 + cloud 턴 첫 실측 (honest-fail 허용)

**Files:**
- Create: `engine/tools/probes/phone_cloud_turn.sh` (phone_ollama_try.sh 기계 — 그 헤더의 함정 원장 전승)

**Interfaces:**
- Consumes: 기존 폰 ssh/tar 계약, `phone_ollama_try.sh`의 OLLAMA-VERDICT 원장(KEEP-STUB 원인 목록).
- Produces: ① 폰 ollama 신판 유무+버전 ② cloud 모델 턴 레이턴시 실측(초) — "빨리 도는거 보고 싶네요"에 대한 첫 눈보답 ③ 조달 실패 시 원인 목록(영수증이 곧 결과물).

- [ ] **Step 1:** probe 작성 — 단계: 폰 ollama version 실측 → 구판이면 조달(pkg install ollama → 실패 시 pinned release fetch, sha256 기록) → `ollama list` cloud 모델 인가 상태(서명 미필요하면 signin 안 함) → curl `/api/chat` 1회 턴(모델 glm-5.3-flash:cloud, 프롬프트 "지뢰찾기 켜줘 — 가능한 행동 한 개를 JSON {\"action\":\"launch\",\"app\":\"minesweeper\"} 형태로만 답해줘", timeout 60) → 응답+경과초 인쇄 → CLOUD-VERDICT: TURN-OK(초) / NEEDS-SIGNIN / PROCURE-FAIL(원인) 라벨.
- [ ] **Step 2:** 실행(Git Bash, 저장소 루트): `bash engine/tools/probes/phone_cloud_turn.sh`
- [ ] **Step 3:** NEEDS-SIGNIN이면 사용자 게이트 산출(폰 브라우저 ollama 서명 — 명령 블록 사용자 전달) — **컨트롤러가 진행 판단**: signin이 폰 브라우저로 폰 자체에서 가능한지 1행 안내와 함께 park·다음 task는 signin 무관 배선으로 진행 가능한지 룰링.
- [ ] **Step 4:** 원장 기록 + 커밋 `probes(chat): 폰 cloud-model 턴 조달 실측 probe (T1)`

---

### Task 2: claude CLI 조달 실측 — launch-claude 경로 진실 게이트

**Files:**
- Modify: `engine/tools/probes/phone_cloud_turn.sh` (§ 확장) 또는 신설 `phone_claude_cli.sh`

**Interfaces:**
- Consumes: T1의 ollama 신판(launch 서브커맨드 존재).
- Produces: `ollama launch claude` 폰 가동성 판정(LAUNCH-OK / NO-CLAUDE-CLI) — T3 cfg 분기의 근거.

- [ ] **Step 1:** `pkg install nodejs-lts` → `npm install -g @anthropic-ai/claude-code`(용량/시간 실측 기록) → `claude --version` → `ollama launch claude --model "<model>" -- -p "say ok" --output-format stream-json` 1회 실측.
- [ ] **Step 2:** 실패는 원인 메시지 그대로 원장(honest-fail 계약 — phone_ollama_try 선례). 스펙 결정 3의 "ollama-direct"가 폰 1차 경로인 이유가 여기서 확인된다.
- [ ] **Step 3:** 커밋 `probes(chat): 폰 claude CLI 조달 실측 — launch-claude 경로 판정 (T2)`

---

### Task 3: 엔진 동기 브리지 + ollama-direct leg (배선 1건)

**Files:**
- Modify: `engine/include/agent/JKLlmEngine.h` (cfg 신설 모드 + 동기 래퍼 선언)
- Modify: `engine/src/agent/JKLlmEngine.cpp` — 오타 대전 주의: 실제 파일명 `JKLlmEngine.cpp` (docs/81 §4 JKLlmEngine 오타 유예 유지 — 본 태스크에서 전수 정화하지 않는다)
- Test: `engine/tools/posix_selftest/main.cpp` (1n 계열 케이스)

**Interfaces:**
- Consumes: 기존 `BuildEngineCmd`(JKLlmEngine.cpp:110-190), `LoadChatConfig`, stream-json 파서.
- Produces: `jk::agent::LlmTurnSync(const std::string& promptUtf8, LlmTurnResult& out)` — 동기 1회 턴(스폰 실패 시 ok=false). cfg.engine=="ollama-direct" → `ollama run "<model>" "<promptEsc>"`(stdout 텍스트 → result) — 스폰 명령 원문은 selftest 단정에 들어간다.

- [ ] **Step 1:** 실패 케이스 먼저: selftest에 `TestLlmSyncOllamaDirect` — chat.json(engine:"ollama-direct")을 임시 exe-dir에 시딩 → TurnSync 결과 파싱 단정(프로세스 스폰은 echo 스터브 명령으로 — 자식 스폰 불가 환경도 ok=false 정직).
- [ ] **Step 2:** 동기 래퍼 구현: 내부 스레드에서 StartTurn 수행 + condition_variable로 Done 대기(타임아웃 120s — cloud 턴 3.2s의 40배 마진).
- [ ] **Step 3:** win32+WSL selftest 실측(캐논 라인: Windows 427/WSL 406 — +N 계보 기록), 커밋 `feat(chat): JKLlmEngine 동기 턴 브리지 + ollama-direct leg (T3)`

---

### Task 4: jkweb/jktalk 백엔드 슬롯 배선 (지연 최소 패칭)

**Files:**
- Modify: `engine/tools/jkweb/main.cpp:340-346` (뇌호출 자리 — 슬롯 주석 계약 이행)
- Modify: jktalk 진입부(kBackendName 블록 주석의 동형 자리 — 소재는 구현자가 실측)

**Interfaces:**
- Consumes: T3 `LlmTurnSync`; 기존 `ChatRouterRoute`; 기존 tool dispatch·ServerMeta 계약.
- Produces: `/talk`·jktalk 파이프의 자연어 턴 — LLM JSON → 기존 ChatAction 파이프라인.

- [ ] **Step 1:** 배선 순서(스펙 결정 4 — 계약): (1) ChatRouterRoute 정확 트리거 매치 → 기존 즉발 경로 유지, (2) 비매치 + cfg.engine 구성됨 → LLMTurnSync(프리앰블: 트리거 표·JSON 스키마 주입 — `kLlmTurnPreamble` 재용) → LLM 응답에서 JSON action 파싱(기존 EscapeJson 역방향 — 파서 신설) → ChatAction 구성, (3) LLM 실패/스폰 불가/응답 파싱 불가 → 기존 stub 안내문 폴백.
- [ ] **Step 4:** selftest 케이스(라우터 매치가 LLM을 우회하는 단정 — cfg 미구성 폴백 단정), 3축 캐논 갱신.
- [ ] **Step 5:** 커밋 `feat(chat): jkweb/jktalk 자연어 승격 배선 — cfg 선택형 + stub 폴백 (T4)`

---

### Task 5: 폰 E2E probe + 원장 착지

**Files:**
- Create: `engine/tools/probes/phone_promote.sh` (phone_chat_close.sh 기계)
- Modify: docs/80 §5 (T8 판정 보완 — cloud 미측정 정정), docs/84 신설 as-built or docs/81 후속

**Interfaces:**
- Consumes: T1-T4 전부 + chat-close-fix 라인 착지(폰 재배포 서술 재용).
- Produces: E2E 영수증 + 사용자 육안 게이트.

- [ ] **Step 1:** 폰 재배포(tar 오염 게이트 포함 — docs/81 §3 #11 계약 승계) → 리빌드 → jkweb 기동 → **E2E 4정판**: "지뢰찾기 켜줘"→launch ok+창 단정 / "창 목록 보여줘"→list_windows / "닫아줘"→close 해소단정 / 무의미 문자열→정직 회신 — 각 턴 경과초 기록(목표 <10s).
- [ ] **Step 2:** 원장: docs/80 §5 T8 판정 보완 + docs/81 §4 deferred에 JKLlmEngine 오타 유예 상태 갱신 + 신설 as-built 문서(§ 함정: signin 게이트·npm 용량·bionic).
- [ ] **Step 3:** 사용자 결제 항목 산출: 폰 브라우저 localhost:8090에서 자연어 실사용("지뢰찾기 켜줘" 말고 다른 문장도) — 컨트롤러는 명령 블록만 전달, 결제 기록은 사용자 선언만.
- [ ] **Step 4:** 커밋 `test(chat): 폰 자연어 E2E probe + 원장 착지 (T5)`

---

## 사용자 게이트 요약 (플랜 밖 정리 — 컨트롤러 운용)

1. ollama 서명(NEEDS-SIGNIN 시) — 명령 블록 전달.
2. T5 종료 후 폰 브라우저 자연어 실사용 육안.
3. chat-close-fix 라인의 기존 게이트(있다면)는 그 라인 원장 소관.