# 브라우저 북마크 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** CEF OSR 브라우저에 현재 URL 추적 + 북마크 바(2행) + `<exeDir>\state\bookmarks.json` 영속화를 얹는다.

**Architecture:** 앱 단일 스레드(CEF integrated loop) 위에 DisplayHandler 2콜백 연결 → currentUrl_/currentTitle_ 추적; browser_bar를 2행으로 확장(pageY_ 80→110, g_viewPageY 상시 동기화); 북마크 저장은 브라우저 프로세스 로컬 JSON(fail-open + .bak 보존).

**Tech Stack:** C++20 / CEF C API (OSR) / Dear ImGui v1.92.9b / SDL2 / jk::agent::AgentJson (읽기) / 수작업 JSON (쓰기)

**Spec:** docs/superpowers/specs/2026-09-14-browser-bookmarks-design.md

## Global Constraints

- **북마크 소유 = 브라우저 프로세스 로컬** — 서버 도구/채널 변경 금지 (스펙 D1)
- **pageY_ 상수 110 고정** — 동적 바 높이 금지, g_viewPageY는 매 프레임 pageY_ 대입 (스펙 D2)
- **fail-open + .bak 1회 보존** — 파일 부재/파손 = 빈 바, 덮어쓰기 전 기존 파일 1회만 .bak로 복사 (스펙 D3)
- **테마 리터럴 0** — active 표시는 기존 토큰(selectionBg) 스왑 (스펙 §0)
- **스키마** `{"bookmarks":[{"title":...,"url":...}]}`, 256항목 상한, 개별 행 이상 스킵
- CEF 콜백/앱이 동일 스레드(단일 스레드 가정 문서화) — 신규 락 금지
- 빌드 전제 `export PATH="/c/msys64/ucrt64/bin:$PATH"`, exe 락 시 `taskkill //F //IM jkdesktop.exe`
- 실행 브랜치: main 직행. as-built는 docs/49

---

### Task 1: 현재 URL/제목 추적 + 2행 레이아웃

**Files:**
- Modify: `engine/src/apps/ClientBrowserApp.cpp` + `engine/include/apps/ClientBrowserApp.h`

**Interfaces:**
- Produces (T2 소비): 멤버 `currentUrl_`/`currentTitle_` (std::string, 단일 스레드 갱신)
- Produces: pageY_ = 110, g_viewPageY 매 프레임 동기화

- [ ] **Step 1: DisplayHandler 콜백** — on_address_change(cef_display_handler_capi.h:71 서명) + on_title_change(:79)를 기존 DisplayObj(:579 근방)에 추가. 콜백에서 self를 거쳐 전역 브리지(기존 g_ 패턴)로 currentUrl_/currentTitle_ 갱신. UTF-16→UTF-8 변환 확인(CEF C API 문자열은 char16) — 기존 Navigate/urlBuf_ 흐름의 변환 유틸 재사용.
- [ ] **Step 2: 레이아웃** — pageY_ 80→110 (헤더 :50), browser_bar 높이는 pageY_-30 그대로라 자동 확장. 2행차: 첫 행 기존(<, >, URL, Go, Home) 유지, 둘째 행은 ImGui::Separator 후 새 행(이번 태스크는 빈 행 + 힌트 텍스트 "★로 현재 페이지 추가"). g_viewPageY를 InitCef 1회 동기화(:602)에서 매 프레임 pageY_ 대입으로 교체.
- [ ] **Step 3: 주소창 표시** — URL 버퍼가 비어 있고 포커스 없으면 currentUrl_ 표시(InputText 앞 스왑 로직 — 기존 버퍼 보존 주의: 표시는 렌더 시 버퍼에 쓰지 말고 placeholder 방식. ImGui InputTextWithHint 힌트로 currentUrl_ 노출이 가장 안전 — 버퍼 무훼손).
- [ ] **Step 4: 빌드 + 셀프테스트** — exit 0 + 0 failures.
- [ ] **Step 5: 커밋**

```bash
git add engine/src/apps/ClientBrowserApp.cpp engine/include/apps/ClientBrowserApp.h
git commit -m "feat(browser): track current URL/title via CEF display callbacks + 2-row bar layout (g_viewPageY per-frame sync)"
```

---

### Task 2: 북마크 저장소 + 바 UI

**Files:**
- Modify: `engine/src/apps/ClientBrowserApp.cpp` + `engine/include/apps/ClientBrowserApp.h`

**Interfaces:**
- Consumes: T1의 currentUrl_/currentTitle_, pageY_=110
- Produces: `bookmarks_` vector<{title,url}>, LoadBookmarks()/SaveBookmarks() (앱 로컬)

- [ ] **Step 1: 저장소** — LoadBookmarks(): `<exeDir>\state\bookmarks.json` fopen fread(64KB 상한) → AgentJson GetArraySize/GetArrStr("bookmarks") → 256항목 상한, 행 이상 스킵, 부재/파손 = 빈 벡터(fail-open). SaveBookmarks(): 기존 파일 존재 시 최초 1회만 .bak 복사(fopen rb→wb) → 수작업 JSON(JsonEsc 동일 이스케이프 — 서버 JsonEsc/JKWindowServer.cpp:550 참조해 브라우저 로컬 복사) → fopen "wb" 오버라이트. dir 없으면 CreateDirectoryA(StateDir 선례).
- [ ] **Step 2: ★ 토글 버튼** — 주소창 행(1행) 우측: currentUrl_이 bookmarks_에 있으면 제거, 없으면 {currentTitle_(빈 경우 URL), currentUrl_} push_back + Save. active 배경은 PushStyleColor(Button, selectionBg 토큰) 스왑. currentUrl_ 빈 값(아직 아무것도 로드 전)이면 버튼 무동작.
- [ ] **Step 3: 북마크 바 렌더** — 2행에 북마크 Button들(title 클립, 툴팁=제목+URL), 클릭=Navigate(url). avail 넘는 항목은 "»" 콤보로. 우클릭 BeginPopupContextItem "삭제" → erase + Save.
- [ ] **Step 4: 기동 적재** — OnInit에서 LoadBookmarks() 1회.
- [ ] **Step 5: 빌드 + 셀프테스트 + 실측** — exit 0 + 0 failures. 실측: 페이지 이동 → ★ 추가 → bookmarks.json 생성/내용 확인 → 클릭 재이동 → 우클릭 삭제 → 재기동 유지 → **g_viewPageY 회귀: 바 표시 상태에서 CEF 페이지 클릭이 의도 위치에 히트**(링크 클릭) → 파손 파일 fail-open 확인.
- [ ] **Step 6: 커밋**

```bash
git add engine/src/apps/ClientBrowserApp.cpp engine/include/apps/ClientBrowserApp.h
git commit -m "feat(browser): bookmark bar + state/bookmarks.json persistence (toggle star, right-click delete)"
```

---

### Task 3: 최종 게이트 (검증 전용 — 구현 금지)

**Files:** 없음 (보고서만: `.superpowers/sdd/<plan-dir>/task-3-report.md`)

- [ ] **Step 1: 풀 빌드 + mtime 게이트** — jkapp_browser.dll 포함 전 아티팩트 최신성.
- [ ] **Step 2:** `jkdesktop test` → 0 failures.
- [ ] **Step 3: 프로브 전량** — engine/tools/probes/ 전부 exit 0.
- [ ] **Step 4: e2e 실측** — 스펙 §2 전 시나리오(추가/재이동/삭제/재기동 유지/g_viewPageY 좌표/fail-open) + 북마크 바 스크린샷 1장.
- [ ] **Step 5: 테마 스크린샷** — classic에서 바 팔레트 추종 1장.

---

### Task 4: 문서 산출물

**Files:**
- Create: `docs/49_browser_bookmarks.md` (EOF 개행 필수)
- Modify: `docs/superpowers/specs/2026-09-14-browser-bookmarks-design.md` (장부에 실행 직감 추가)

- [ ] **Step 1: docs/49** — 개요/커밋 표(실측 git log)/대응표(콜백 2종·레이아웃·저장소)/검증 실측/실행 직감/후속.
- [ ] **Step 2: 스펙 장부 행 추가.**
- [ ] **Step 3: 커밋**

```bash
git add docs/49_browser_bookmarks.md docs/superpowers/specs/2026-09-14-browser-bookmarks-design.md
git commit -m "docs: record browser bookmarks execution — docs/49 + spec ledger"
```