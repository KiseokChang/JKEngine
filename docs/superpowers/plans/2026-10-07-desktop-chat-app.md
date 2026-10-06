# 데스크톱 채팅 앱(jkapp_chat) + 플러그형 턴 백엔드 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** DeX 화면(폰 X11)에서 텍스트를 받아 에이전트 턴(stub 기본)으로 jkdesktop 앱을 조작하는 채팅 앱 + 백엔드 cfg 승격선 + 폰 ollama 설치 실험.

**Architecture:** `jkapp_chat`(ImGui 클라 앱 — settings/library 가족)이 턴 백엔드(JKLlmEngine cfg: stub/ollama/claude)를 플러그로 받고, 스텁의 명령 라우터가 도구 지시를 내보내면 앱이 SendAgentQuery로 서버 실행을 위임한다. 라우터는 jkcore pure 코드로 CLI·앱·셀프테스트가 같이 먹는다(앱 라이브러리 single-truth 선례 승계).

**Tech Stack:** C++17·ImGui(엔진 내장)·JKLlmEngine(기존)·Termux aarch64.

**Spec:** docs/superpowers/specs/2026-10-07-desktop-chat-app-design.md (결제판 5074375)

## Global Constraints

- 스펙 §2 계약 전부: 채팅 앱의 도구 실행=SendAgentQuery 위임 단발 — 직접 실행 금지; 백엔드 cfg 승격 시 UI 무수정; 권한 게이트는 서버 권한 행렬 소유 — 앱 우회 금지.
- 라우터(ChatRouter)는 jkcore pure(창·imgui 무접촉) — CLI·앱·셀프테스트가 같은 진실원.
- Windows 서버 스폰 금지 — Windows 축은 빌드+셀프테스트+CLI, GUI 실측은 WSL/폰 probe 소유.
- probe 품질 원장: 수신=어설션, `|| true` 금지, rc 전파 PIPESTATUS→exit $rc, 커밋 SHA는 rev-parse 실측.
- ImGui 함정 원장(스펙 §3): 본문은 Begin/End **안**; 스크롤 창=AlwaysVerticalScrollbar; 16ms 항시 렌더=가족 계약(docs/78 백로그 주석).
- 폰 절차 원장: ssh `u0_a4@192.168.219.109 -p 8022` + MSYS_NO_PATHCONV=1 + tar `-C ~/JKENGINE` + pkill/pgrep `-f 'buildterm/[j]kdesktop'` 브래킷.

---

### Task 1: ChatRouter (jkcore pure 명령 라우터)

**Files:**
- Create: `engine/include/apps/ChatRouter.h`, `engine/src/apps/ChatRouter.cpp`
- Modify: `engine/src/main.cpp`(셀프테스트 케이스 1n 추가), `engine/CMakeLists.txt`(jkcore 소스 1행)

**Interfaces:**
- Consumes: 없음(신규 pure).
- Produces: `namespace jk { struct ChatAction { enum Kind { Launch, Close, Focus, ListWindows, Info }; Kind kind; std::string app; }; std::string ChatRouterRoute(const std::string& text, ChatAction& out); }` — 응답 문자열은 사용자에게 보여질 안내/확인문(한국어), `Kind==Info`면 도구 지시 없음.

- [ ] **Step 1: 표 구현** — 스펙 §1.3 어휘: `~켜줘/열어줘` → Launch(app=접어체 앞 어절, 라이브러리 appName 규약 인지 — 내장 mine/tetris 별명 표 포함) · `꺼줘/닫아줘` → Close · `포커스/앞으로` → Focus · `창 목록/뭐 떠` → ListWindows · 인식 불가 → Info+안내문. 패턴 표는 파일 상단 정적 배열(single-source).
- [ ] **Step 2: 셀프테스트 1n** — 어휘 4종+불인+별명 각 최소 1 case, 10 check 목표(개수는 실측 보고).
- [ ] **Step 3: 빌드+셀프테스트** — 표준 2줄 PATH+ninja -j3 → `AppSelfTest: 0 failure(s)` → 커밋 `feat(chat): 채팅 명령 라우터 jkcore + 셀프테스트 1n (스펙 §1.3)`.

### Task 2: jkapp_chat 모듈 (ImGui 채팅 앱)

**Files:**
- Create: `engine/include/apps/ClientChatApp.h`, `engine/src/apps/ClientChatApp.cpp`, `engine/src/apps/JKAppModule_chat.cpp`
- Modify: `engine/src/JKLibraryCatalog.cpp`(내장 3원+chat — 소스 목록 1행), `engine/CMakeLists`(모듈), `engine/src/main.cpp`(1m 내장 개수 전면화 — 아래 함정)

**Interfaces:**
- Consumes: `jk::ChatRouterRoute`(T1), `SendAgentQuery`/`PollReplies`(ClientLibraryApp.cpp:195-237 원준), CapabilityBadgeText 불요.
- Produces: app id `chat` — `launch_app {"app":"chat"}` 실행 가능해야 함(서버 존재 검증 = jkapp_chat 모듈 파일).

- [ ] **Step 1: 모듈 구현** — meta `{"chat","Chat",720,540}`; UI: 기록 창(AlwaysVerticalScrollbar)+하단 InputText+전송 버튼; SendQuery/poll 계약은 ClientLibraryApp 원준 복사(에이전트 답신 drain 계약 포함); 폴백 컨트롤. **주석 함정 원장 전수 이행(스펙 §3)**.
- [ ] **Step 2: 카탈로그 내장 chat 추가** — 1m 내장 개수 기대치가 실측으로 바뀐다: selftest 1m-1의 `n==5` → `n==6`(셀프테스트 1m 잠김 어설션 갱신), CLI count=29→30 불일치는 docs 갱신이 아니라 **실측 결과**로 기록. 격리 트리 ARG count=4 유지(트리는 chat 무설치 게이트 유지 — 내장이 트리에도 존재하면 계약 재검토 리뷰 위탁).
- [ ] **Step 3: 빌드+셀프테스트+커밋** — `feat(chat): jkapp_chat ImGui 채팅 앱 + 내장 카탈로그 등록 (스펙 §1.1)`.

### Task 3: Windows 축 영수증 (CLI)

- [ ] **Step 1:** 표준 빌드 → `AppSelfTest: 0 failure(s)` → `library-list` rc=0(새 count 실측 보고, chat 행 source=builtin 확인) → probe 격리 트리 정판 재실행(기존 probe 파일 재용) → 커밋 없으면 스킵.
- [ ] **Step 2:** 영수증은 report에. 커밋 대상 없으면 report만.

### Task 4: WSL 실측 probe

**Files:**
- Create: `engine/tools/probes/wsl_chat_boot.sh` (wsl_library_boot.sh 원준 재용)

- [ ] **Step 1:** WSL 리빌드 → setsid 부팅 → agentctl launch_app chat → list_windows 기하 어설션(title=Chat, 720x540, focused — tight형 단정) → pkill 철수. rc 전파 원장 준수.
- [ ] **Step 2:** 커밋 `test(chat): WSL 부팅·런치 probe — LIBRARY-BOOT 원준 승계 (스펙 §1.1)`.

### Task 5: 폰 실측 probe (IME 게이트 포함)

**Files:**
- Create: `engine/tools/probes/phone_chat.sh` (phone_library.sh 원준 재용)

- [ ] **Step 1:** tar 재배포(변분 7종+신규 chat 3파일) → 폰 리빌드(~9-10분 정상) → 셀프테스트 → launch chat → list_windows(Chat 720x540 focused 단정) → 폰 소프트 키보드 입력 도달성: **자동 어설션 불가 분은 사용자 육안 게이트로 명시**(probe 종료 상태=서버 UP+Chat 창 유지). ninja NOUT 인쇄 보장·rc 전파 원장 승계.
- [ ] **Step 2:** 커밋 `test(chat): 폰 실측 probe — aarch64 자립 + IME 육안 게이트 산출 (스펙 §1.4)`.

### Task 6: 폰 ollama 설치 실험 (honest-fail 허용)

**Files:**
- Create: `engine/tools/probes/phone_ollama_try.sh`

- [ ] **Step 1:** `pkg install ollama`(또는 대안 경로 실측) → `ollama serve` → 소형 모델 pull(qwen2.5:0.5b 계열) → `ollama launch claude` 계열이 아니라 JKLlmEngine cfg(state/chat.json engine=ollama)로 실턴 1건 시도 → 발화→응답 시 초 실측. **실패해도 실패 영수증이 결과물** — 원인(패키지 부재/빌드 실패/메모리)을 로그로 남기고 유예 판정.
- [ ] **Step 2:** 결과 기록 커밋 `test(chat): 폰 ollama 설치 실험 — 결과 영수증+승격 판정 (스펙 §1.2)`.

### Task 7: as-built docs/80

**Files:**
- Create: `docs/80_desktop_chat_asbuilt.md`

- [ ] **Step 1:** 배선·영수증 전수(3축)·함정 원장·deferred minors 표·커밋 원장 — T6 결과(stub 유지 or ollama 승격) 명기. 스펙 헤더 링크+푸시.
- [ ] **Step 2:** 커밋 `docs(chat): as-built 원장 docs/80 + 스펙 헤더 링크` + push(origin main 직행 관행).