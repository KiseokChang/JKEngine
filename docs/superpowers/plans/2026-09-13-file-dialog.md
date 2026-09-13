# 엔진 네이티브 파일 대화상자(filedlg) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 엔진 네이티브, 테마 추종 파일 열기 대화상자 앱 모듈(jkapp_filedlg)을 만들고, 파킹 쿼리 + `filedlg:` 접두 관례로 vplayer가 호출해 재생 파일을 고르게 한다.

**Architecture:** 신규 ImGui 앱 모듈(ClientFileDialogApp) + 서버 에이전트 도구 3종(file_open=파킹 후 `filedlg:<json>` 스폰 / file_dialog_params=기동 직후 파라미터 회수 / file_open_result=파킹 쿼리 완료) + vplayer "열기..." 버튼. 모듈 ABI 무변경.

**Tech Stack:** C++20 / SDL2 / Dear ImGui v1.92.9b / std::filesystem / MinGW-Ninja (msys2 ucrt64)

**Spec:** docs/superpowers/specs/2026-09-13-file-dialog-design.md

## Global Constraints

- **모듈 ABI 무변경** — `jk_app_meta()`/`jk_app_run_client(pipe)` 시그니처 불변. 인자 전달은 전부 `filedlg:<json>` 스폰 접두 + 기동 직후 `file_dialog_params` 쿼리 (스펙 D3)
- **서버 타이틀 특수코드 추가 금지** — snap 오버레이류 하드코딩 확산 금지 (스펙 D6)
- **테마** — 기존 JKThemeImGui.h 봉합 경유, 신규 색 리터럴 금지(의미색 예외만) (스펙 D5)
- **신규 에이전트 도구 3종 전부 기본 allow** — permissions.json 기본값 유지
- **레거시 JKFileDialog의 로직 이식은 다이얼로그 로컬** — jkcore 승격 금지 (스펙 D4)
- 모든 테마 소비처는 `current()` — kDefault 직접 참조 금지
- 프로브 15종(`engine/tools/probes/`) 회귀 유지; 빌드 전제 `export PATH="/c/msys64/ucrt64/bin:$PATH"` (레슨 37), exe 락 시 `taskkill //F //IM jkdesktop.exe`
- 실행 브랜치: main 직행 (관례). 라인번호는 2026-09-13 인벤토리 참조 — 내용 기준 탐색
- as-built는 docs/48로 기록 (커밋 표/대응표/실측/직감/후속)

---

### Task 1: 서버 — file_open 파킹 + filedlg: 스폰 + params/result 도구

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` (HandleAgentQuery 스위치, ProcessPendingClients 부근, SpawnClient/SpawnProcess)
- Modify: `engine/include/server/JKWindowServer.h` (파킹/params 저장소 멤버 선언 시 필요시)

**Interfaces:**
- Produces (T2/T3 소비):
  - 에이전트 도구 `file_open` args `{filter?, start?, title?}` → 응답은 파킹(요청자가 다이얼로그 닫을 때 나중에 완료됨)
  - 에이전트 도구 `file_dialog_params` → 응답 `{filter, start, title}` (1슬롯 pending 소진)
  - 에이전트 도구 `file_open_result` args `{ok, path?}` → 요청자의 파킹 쿼리 완료 (ok=false면 `{"ok":false}`)
  - `SpawnClient("filedlg:<json>")` → `--filedlg <json>` 인자로 자식 프로세스 기동

- [ ] **Step 1: pending 저장소 + file_open 핸들러** — 멤버 1개(구조체: requesterConnId + filter/start/title json 문자열) 추가. `file_open` 처리: (a) 쿼리 파킹(jkchat close_window/trust_request 파킹 선례 준용 — 같은 parked-query 기계), (b) `filedlg:<json>` appName으로 SpawnClient 호출(json = args 그대로), (c) 파킹 쿼리 id 저장. 스폰 실패 시 파킹 즉시 해소(오류 응답).
- [ ] **Step 2: SpawnProcess `--filedlg` 파싱** — `--jkx`/`--client` 파싱과 같은 자리에서 `--filedlg <json>` argv를 받아 자식에게 전달(quoted 인용 — terminal: 접두 선례의 인용 처리 준용).
- [ ] **Step 3: file_dialog_params 핸들러** — pending 슬롯이 있고 아직 소진 전이면 params+requesterConnId 응답 후 슬롯 소진. pending이 요청자 연결 종료로 무효화되면(요청자 disconnect 정리에 훅) 응답 없음/오류 — 파킹 만료 기계가 회수 담당.
- [ ] **Step 4: file_open_result 핸들러** — args `{ok, path?}`로 요청자 conn id의 파킹 쿼리를 완료(approve 도구의 파킹 해소 선례 준용). 파킹이 이미 만료/부재면 no-op + `{"tool":"file_open_result","ok":true,"parked":false}` 응답.
- [ ] **Step 5: MCP 미러 불가 확인** — tools/jkagentd/main.cpp는 이번에 건드리지 않는다(엔진 내부 도구 — 후속 기록).
- [ ] **Step 6: 빌드 + 셀프테스트** — `cmake --build .` exit 0, `./jkdesktop test` → 0 failures. 락 시 taskkill.
- [ ] **Step 7: 커밋**

```bash
git add engine/src/server/JKWindowServer.cpp engine/include/server/JKWindowServer.h
git commit -m "feat(server): file_open parked query + filedlg: spawn prefix + params/result tools"
```

---

### Task 2: filedlg 앱 모듈 — ClientFileDialogApp

**Files:**
- Create: `engine/src/apps/ClientFileDialogApp.cpp` (+ `engine/include/apps/ClientFileDialogApp.h`)
- Create: `engine/src/apps/JKAppModule_filedlg.cpp`
- Modify: `engine/CMakeLists.txt` (jkapp_filedlg SHARED 타깃 — 기존 jkapp_* 타깃 블록과 동일 패턴)

**Interfaces:**
- Consumes: T1의 `file_dialog_params`(기동 직후), `file_open_result`(종료 시)
- Produces: `jkapp_filedlg.dll` — name "filedlg", title "파일 열기", 560x400

- [ ] **Step 1: 모듈 골격 + 테마 봉합** — palette/snap 모듈 패턴 준용: `JKClientApplication` 상속, 루트 rect 560x400 + `WA_TITLEMOVEABLE` 전용, `CreateContext()` 직후 `jk::theme::ApplyImGuiTheme();` + `IniFilename = nullptr`, 16ms 타이머 프레임 루프. `jk_app_meta()`/`jk_app_run_client()` export.
- [ ] **Step 2: 열거/필터 이식** — 레거시 `JKFileDialog.cpp`의 `MatchFilter`(`*.ext`/정확명/`;` 목록)와 dirs-first 열거 로직을 다이얼로그 로컬로 이식(std::filesystem, `skip_permission_denied`). 접근 거부 디렉터리는 진입 시 오류 오버레이(의미색 리터럴 — 의도 주석).
- [ ] **Step 3: UI** — (1) 위로 버튼 + 경로 InputText(Enter=이동) + 새로고침, (2) 항목 리스트(Child+Selectable, dirs-first, 더블클릭 폴더=하강/파일=선택), (3) 필터 콤보 + 파일명 InputText(Enter=OK), (4) [열기][취소]. Esc=취소(snap 선례), Enter 계약(레거시 RespondMessage 계승).
- [ ] **Step 4: 채널 배선** — OnInit 직후 `SendAgentQuery(file_dialog_params)` → 응답으로 start/filter 반영. 종료 경로(열기/취소/서버 닫기 Quit 모두)에서 `file_open_result {ok, path?}` 1회 발송 후 `Close()` — 중복 발송 방지 플래그.
- [ ] **Step 5: 빌드 + 셀프테스트** — exit 0 + 0 failures + jkapp_filedlg.dll 링크 mtime.
- [ ] **Step 6: 커밋**

```bash
git add engine/src/apps/ClientFileDialogApp.cpp engine/include/apps/ClientFileDialogApp.h engine/src/apps/JKAppModule_filedlg.cpp engine/CMakeLists.txt
git commit -m "feat(apps): filedlg ImGui app module — theme-following file open dialog"
```

---

### Task 3: vplayer 통합 — "열기..." 버튼

**Files:**
- Modify: `engine/src/apps/ClientVPlayerApp.cpp` + `engine/include/apps/ClientVPlayerApp.h` (버튼/쿼리 폼프/lastDir 멤버)

**Interfaces:**
- Consumes: T1의 `file_open`(파킹 회수), T2의 jkapp_filedlg

- [ ] **Step 1: 버튼 + 쿼리** — 수동 경로 입력 행 옆 "열기..." 버튼. `SendAgentQuery`로 `{"tool":"file_open","args":{"filter":"동영상 (*.mp4;*.mkv;*.avi;*.webm;*.mov)","start":lastDir_}}` 1-in-flight(palette SendTool 패턴 — 이미 진행 중이면 무시). 응답 프레임 폼프(PollAgentReply): `{"path":...}` → `OpenPath`, `{"ok":false}` → 무시(취소).
- [ ] **Step 2: lastDir 기억** — 재생 시작(Open 성공) 시 파일의 부모 디렉터리를 lastDir_에 저장(프로세스 수명 내 — 영속화는 후속).
- [ ] **Step 3: 빌드 + 셀프테스트** — 동일 게이트.
- [ ] **Step 4: 커밋**

```bash
git add engine/src/apps/ClientVPlayerApp.cpp engine/include/apps/ClientVPlayerApp.h
git commit -m "feat(vplayer): file picker via filedlg — Open button on the agent channel"
```

---

### Task 4: 최종 게이트 (검증 전용 — 구현 금지)

**Files:** 없음 (보고서만: `.superpowers/sdd/<plan-dir>/task-4-report.md`)

- [ ] **Step 1: 풀 빌드 + mtime 게이트** — 신규 jkapp_filedlg.dll 포함 전 아티팩트 최신성.
- [ ] **Step 2:** `jkdesktop test` → 0 failures.
- [ ] **Step 3: 프로브 15종** — 전부 exit 0.
- [ ] **Step 4: e2e 실측** — vplayer 열기 → filedlg 스폰 + 포커스 → 폴더 하강 → 파일 선택 → 재생 시작. 회귀: 취소(ok=false), 다이얼로그 중 요청자 닫기(파킹 만료, 무크래시), 존재하지 않는 디렉터리, 접근 거부 디렉터리.
- [ ] **Step 5: 테마 스크린샷** — classic에서 filedlg 팔레트 추종 1장(SDD 디렉토리 저장).

---

### Task 5: 문서 산출물

**Files:**
- Create: `docs/48_file_dialog.md` (EOF 개행 필수)
- Modify: `docs/superpowers/specs/2026-09-13-file-dialog-design.md` (장부에 실행 직감 추가)

- [ ] **Step 1: docs/48** — 개요/커밋 표(실측 git log)/대응표(도구 3종 스펙+UI 구조)/검증 실측/실행 직감/후속.
- [ ] **Step 2: 스펙 장부 행 추가.**
- [ ] **Step 3: 커밋**

```bash
git add docs/48_file_dialog.md docs/superpowers/specs/2026-09-13-file-dialog-design.md
git commit -m "docs: record filedlg execution — docs/48 + spec ledger"
```