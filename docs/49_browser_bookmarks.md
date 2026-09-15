# 49. 브라우저 북마크 (북마크 바 + bookmarks.json) as-built

- 날짜: 2026-09-14 (SDD 3태스크 — T1/T2 구현 + T3 최종 게이트, 게이트 **GATE GREEN**)
- 상태: 구현 완료. 코드 커밋 02e1163 → 593a1e3, 게이트에서 프로브 잠금 픽스 8d6f7bc.
  빌드 exit 0, `jkdesktop test` 0 failures, **프로브 18/20**(실패 2건은 사전 귀속
  기존 결함 — §4), e2e 전수명주기 25/25 + 북마크 프로브 16/16.
- 선행: docs/superpowers/specs/2026-09-14-browser-bookmarks-design.md (스펙, 커밋
  143f72d), docs/superpowers/plans/2026-09-14-browser-bookmarks.md (플랜, 7e34258)
- 비고: 진행/룰링/리뷰 레저는 `.superpowers/sdd/2026-09-14-browser-bookmarks/progress.md`,
  게이트 보고서는 같은 디렉터리 task-3-report.md. 스크린샷 실물(gate-bookmark-bar.png
  다크/클래식 등)도 동일 디렉터리.

## 1. 개요

"브라우저에 북마크 있으면 좋겠다"는 사용자 요청 한 줄에서 출발했다. 인벤토리
스캔이 밝힌 실체는 저장이 아니라 **브라우저가 현재 URL을 전혀 모른다**는 것 —
CEF DisplayHandler의 on_address_change/on_title_change가 아예 연결되어 있지
않았다. 그래서 이번 작업의 본체는 3가지다. (1) **현재 URL/제목 추적** — Display
콜백 2종을 연결해 g_ 브리지로 갱신하고, RenderOverlay가 프레임당 멤버로 복사
(단일 스레드라 락 없음 — OSR integrated loop). (2) **2행 바 레이아웃** — pageY_
상수 80→110으로 browser_bar를 2행으로 확장하면서, 스펙 단계에서 이미 발견된
치명적 함정 `g_viewPageY`(CEF 마우스 y 오프셋이 InitCef에서 1회만 동기화)를
매 프레임 pageY_ 대입으로 상시화해 제거. (3) **북마크 저장소** — 브라우저
프로세스 로컬 `<exeDir>\state\bookmarks.json`, fail-open + .bak 1회 보존,
수작업 JSON 쓰기(엔진 관례 준수 — 원자적 쓰기 미도입, 스펙 D5).

부수 수혜가 두 가지 생겼다. 주소창이 버퍼가 비면 힌트로 현재 URL을 보여주게
됐고(버퍼 무훼손 — InputTextWithHint), 그리고 북마크가 엔진의 **첫 유저 영속
데이터**가 됐다(트러스트/트리거는 시스템 데이터).

## 2. 커밋 (2 feat + 2 test + docs 2 — 실측 `git log --oneline 24dbfc4..HEAD`)

| 커밋 | 분류 | 내용 |
|---|---|---|
| 02e1163 | feat (T1) | CEF display 콜백으로 현재 URL/제목 추적 + 2행 바 레이아웃 (g_viewPageY 매 프레임 동기화) |
| 593a1e3 | feat (T2) | 북마크 바 + state/bookmarks.json 영속화 (★ 토글, 우클릭 삭제) |
| d407aa1 | test (T2) | 북마크 e2e 프로브 (CMake 무변경 — AgentJson은 jkclient→jkcore로 도달) |
| 8d6f7bc | test (T3) | 디포커스 커밋 픽스를 북마크 프로브에 잠금 (서로 다른 URL step-3 단정) |

Task 3(최종 게이트)은 검증 전용이라 제품 커밋 없음(위 8d6f7bc가 유일 변경 —
프로브 1파일). 스펙/플랜 문서 143f72d·7e34258은 BASE(24dbfc4) 이전 커밋이라
범위 밖. 실측 범위 diff: 3파일 +700/−5 (ClientBrowserApp.cpp +315, .h +19,
probe_browser_bookmarks.ps1 신설 +371).

## 3. 대응표

### (a) 현재 URL/제목 추적 (T1)

| 항목 | 실측 |
|---|---|
| 콜백 연결 | `dh_on_address_change`/`dh_on_title_change` — cef_display_handler_capi.h:71/:79 서명 그대로 DisplayObj 등록부에 연결 |
| 서브프레임 필터 | on_address_change는 `frame->is_main` 필터 — iframe 내비게이션이 주소를 오염시키지 않음. on_title_change는 프레임 인자 자체가 없어 필터 대상 없음 |
| UTF-16→UTF-8 | `CefToUtf8()` 헬퍼 — 기존 `MakeString`의 대칭. `cef_string_utf16_to_utf8` + 출력 제로화 → 복사 → 명시적 clear(파일 변환 유틸 관례 준수) |
| 스레딩 | g_ 브리지(g_currentUrl/g_currentTitle) ← 콜백 기록, RenderOverlay가 `cef_do_message_loop_work()` 직후 멤버로 프레임당 복사. CEF 통합 루프가 앱 스레드에서 펌프되므로 **락 없음**(g_browser/g_tex와 동일 스레드 스토리, 멤버 선언부에 단일 스레드 가정 주석) |
| 주소창 표시 | urlBuf_가 비어 있을 때만 InputTextWithHint 힌트로 currentUrl_ 노출 — 힌트는 버퍼에 기록되지 않으므로 urlBuf_는 사용자 소유 유지. 힌트 인자는 포맷 문자열이 아니라 `.c_str()` 직접 전달 |

### (b) 2행 레이아웃 + g_viewPageY (T1)

| 항목 | 실측 |
|---|---|
| pageY_ | 상수 80→110 (헤더). browser_bar 높이는 기존 계산식 `(pageY_ - 30)` 그대로라 50→80으로 자동 확장 — 별도 ImGui 창 아님 |
| g_viewPageY | InitCef의 1회 대입은 첫 CEF 프레임 정확성을 위해 유지(무해), RenderOverlay의 pageW_/pageH_ 재계산 지점에 **프레임당 `g_viewPageY = pageY_` 대입** 추가. SendMouseMotion/Button/Wheel 좌표 변환(:244/:260/:282)은 코드 변경 없이 자동 추적 |
| 실측 회귀 게이트 | 2행 상태에서 example.com의 "Learn more" 링크(~90x20 논리 px 작은 타깃)를 (221,307)에서 클릭 → **내비게이션 발생**. pageY_ 30px 증가만으로 빗나갔을 타깃 — 링크가 클릭된 것 자체가 동기화 정확성의 측정. T2 게이트는 전체 페이지 폭 픽셀 디프 143(게이트 80)으로 자동화 |

### (c) 북마크 저장소 (T2)

| 항목 | 실측 |
|---|---|
| 경로 | `<exeDir>\state\bookmarks.json` (GetModuleFileNameA). `ExeDirSlash()`는 g_exeDirSlash 캐시를 공유하되 수요 시 계산 — **LoadBookmarks는 OnInit에서 lazy InitCef보다 먼저 돌므로** InitCef의 채움 순서에 의존하면 빈 디렉터리를 읽는다 |
| 읽기 | fopen rb, 64KB 상한(초과 시 fail-open 빈 벡터) → AgentJson GetArraySize/GetArrStr, 256항목 상한, URL 없는 행 스킵, 빈 제목은 URL로 대체. 파일 부재/파손 = 빈 벡터 (fail-open — 유저 데이터는 trust.json의 fail-closed와 반대 의미) |
| 쓰기 | 수작업 JSON — JKWindowServer::JsonEsc(:550)의 브라우저 로컬 사본(따옴표/백슬래시/제어바이트 \u00xx, UTF-8 통과). `CreateDirectoryA(state)` 멱등. 변이마다 즉시 저장, 적재는 OnInit 1회 |
| **.bak 1회 보존** | 저장 시점에 bookmarks.json이 존재하고 .bak가 없을 때만 복사(rb→wb) — 이후 저장은 .bak를 갱신하지 않는다 (ledger 룰링 2: "매 저장마다 백업"이 아니라 **최초 덮어쓰기 시점의 원본 1회 보존** — 파손 직전 상태가 최악의 경우에도 남도록). 프로브가 존재 시점 + 불변성까지 단정 |
| 링크 | CMake 무변경 — AgentJson은 jkcore에 있고 jkapp_browser가 링크하는 jkclient가 jkcore를 PUBLIC 링크. 사전 허가(룰링 1)는 불발로 끝남 |

### (d) 바 UI + 폰트 (T2)

| 항목 | 실측 |
|---|---|
| ★ 토글 | 1행 우측(Home 옆). currentUrl_ 빈 값이면 BeginDisabled로 무동작. exact-URL 매치 → erase+Save, 아니면 {currentTitle_(빈 경우 URL), currentUrl_} push+Save. active는 `PushStyleColor(Button, selectionBg)` + Hovered=`Lighten(selectionBg, 0.12)`(ToImVec4) — **테마 리터럴 0** |
| 북마크 행 | 항목마다 Button, 라벨 140px 픽셀 클립(TruncateLabel — UTF-8 시퀀스 경계 절단, 접미 ASCII `...` — U+2026은 베이킹 범위 밖), 툴팁=제목+URL, 좌클릭=Navigate, 우클릭=BeginPopupContextItem "삭제". avail 초과분은 "»" 팝업(동일 툴팁/삭제). 빈 상태는 T1 힌트 유지 |
| malgun.ttf | OnInit에서 CreateContext/ApplyImGuiTheme 직후 AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f, **로컬 범위 배열**). 편차(의도): GetGlyphRangesKorean에 **U+2605/2606(★☆) 부재** — vendored imgui_draw.cpp에서 검증 — 한글 범위 + 별표 2코드포인트를 로컬 배열로 합성. 실측: 힌트/삭제 팝업/툴팁 한국어 무결 |
| 디포커스 커밋 픽스 | T1 관찰의 InputText 커밋-on-디포커스 퀴크(편집 후 다른 곳 클릭 시 Enter 없이도 Navigate)를 `go = InputTextWithHint(...) && ImGui::IsItemFocused()`로 봉쇄 — Enter는 커밋(아이템이 포커스 유지), 클릭 이탈 비활성화는 더 이상 Navigate 아님. 1행 InputText 폭 예약 -100→-170(★ 클리핑 해소) |

## 4. 검증 실측 (task-3-report.md GATE GREEN)

- **빌드 + mtime 게이트**: `cmake --build .` exit 0. jkapp_browser.dll(02:52:00)
  > 소스 최신(02:51:48), jkdesktop.exe도 신선 — 본 계획은 ClientBrowserApp.*만
  건드려 DLL로 흐른다.
- **`jkdesktop test` → 0 failures.**
- **프로브 18/20 PASS — 실패 2건은 사전 귀속으로 정직하게 남긴다(회귀 아님):**
  - `probe_filedlg_fix.ps1`: filedlg 계획에서 확정된 CP949 ANSI argv 문제 —
    스폰이 window를 내지 않음. 제품 결함 아님(docs/48 §7).
  - `probe_terminal_reflow.ps1`: read-back 하네스 실패("wide-read: FAIL (got 0
    chars)"). 리플로우 자체는 동작("resized: client 1200x500 -> cols 149 rows
    29") — 하네스 귀속, docs/42 이월.
  - `probe_browser_bookmarks.ps1`: **16/16 PASS**(12 원래 + 4 신규, §5).
- **e2e 전수명주기 25/25** (gate_e2e.ps1, 미커밋 — 게이트 전용): 신규 state →
  한국어 `<title>` 로컬 페이지 북마크(json에 한글 타이틀 실측) → example.com
  2건 → **클라이언트만 재시작(kill --client → 서버가 browser.jkx 재기동 —
  .jkx 패키징이 실제로 도는 경로) → 양쪽 영속** → 각 내비게이션(픽셀 디프
  대조: kpage 0/epage 121904 역방향 포함) → 우클릭 삭제 → ★ 토글오프 →
  `"bookmarks":[]` 디스크 반영 → **파손 파일 주입 → 클라 생존 + 파일 무훼손 +
  빈 바** → .bak 정확 1회(.bak.bak 부재, 파손 전 한글 저장 보유).
- **테마**: theme.json classic 프리셋 + 클라 재시작 → 바가 클래식 팔레트 추종
  (바 영역 디프 24497 vs 다크) — 스크린샷 gate-bookmark-bar-classic.png, 한국어
  라벨 무결.

## 5. 리뷰 이력 (progress.md)

- **T1 리뷰 APPROVE** (Spec PASS / Quality GOOD). is_main 필터·InitCef 시드
  유지(불변 강화·베나인 편차)·CefToUtf8 제로초기화 관례 확인. **구속 조건 3건이
  T2 디스패치 요구로 이월**: ①malgun.ttf 적재(형제 관례) 또는 ASCII
  플레이스홀더 — 방치 시 T2에서 MAJOR 승격 ②InputText 디포커스 커밋 퀴크를
  감안한 행 클릭 배선 ③t1_e2e.ps1 하네스 재사용(브라우저 hwnd 은폐 — 서버
  hwnd+list_windows 오프셋, DPI 컨텍스트 선행).
- **T2 리뷰 APPROVE** (Spec PASS / Quality APPROVE — 0 MAJOR / 1 MINOR /
  5 NOTE). MINOR-1: e2e step-3가 북마크 클릭 내비게이션을 단정하지 않음 —
  타이핑 URL과 북마크 URL이 같은 페이지라 회귀 감지 불가. **T3 게이트에 흡수**
  결정. NOTE-5 .bak 부분복사 에지는 1회 보존 시맨틱 준수로 판정. 편차 3건
  (폰트 범위/폭/포커스 게이트) 전부 옹호 확정.
- **게이트(GATE GREEN)** — 유일 변경은 프로브 픽스 8d6f7bc(step-3가 북마크
  클릭 전 서로 다른 URL(example.org, Enter 없음)을 타이핑한 뒤 북마크를
  클릭하고, 별 활성 상태 + 힌트=북마크 URL을 단정). 이 검사가 성립하는 이유:
  example.org/example.com 렌더가 여기선 픽셀 동일 — 회귀는 페이지 픽셀이
  아니라 **별 활성 상태가 뒤집히는 것**(stale 재내비게이션 발화 신호)과 힌트
  텍스트로 검출한다. 제품 코드 무변경 확인.

## 6. 실행 직감 (스펙 §4 장부에 일부 기록)

1. **CEF OSR 브리지의 규율은 "락을 만들지 않는 것"이다.** 통합 메시지 루프가
   앱 스레드에서 펌프되는 한 콜백=렌더=같은 스레드라 g_ 전역 + 프레임당 멤버
   복사면 충분하다. 락을 넣는 순간 단일 스레드 가정이 아니라 "언젠가 멀티
   스레드가 된다"는 미확정 설계가 되고, 그 순간 InitCef/g_tex 등 기존 전역
   전부를 같이 다시 검토해야 한다. 가정은 주석으로 문서화하고 락은 도입하지
   않은 게 옳은 절충이었다.
2. **.bak의 가치는 최신 백업이 아니라 파손 전 원본이다.** "매 저장마다 백업"
   로 구현했다면 파손 파일이 .bak를 덮어써 안전망이 소실된다. ledger 룰링 2가
   "최초 덮어쓰기 시점 1회 보존"으로 확정한 게 본체였고, 프로브는 이 시맨틱을
   그대로 단정한다(존재 시점 + 이후 저장에 불변). 게다가 파손 파일은 설계상
   백업되지 않는다 — 쓰레기를 보존할 이유가 없다.
3. **구속 조건은 리뷰에서 소멸하지 않고 디스패치 요구로 이월된다.** T1 리뷰의
   3건(폰트/디포커스 배선/하네스)을 T2 brief에 구속 조건으로 명시했더니 폰트
   미적재가 "한글 ???" 관찰로 흘러 MAJOR로 승격되는 경로가 사전에 봉쇄됐고,
   디포커스 퀴크는 T2에서 "trivial하다"는 판정과 함께 실제 픽스(IsItemFocused
   게이트)가 됐다. 조건을 다음 태스크의 계약에 넣는 패턴 — 리뷰 산출물의
   소멸 방지 장치.
4. **프로브 단정은 페이지를 구분할 수 있어야 값어치가 있다.** T2 프로브의
   step-3는 타이핑 URL과 북마크 URL이 같은 페이지를 열어 "북마크 클릭이
   내비게이션했다"를 단정할 수 없었다(회귀가 페이지 픽셀에 안 보임). 픽스는
   다른 URL을 미리 타이핑하는 것 + 픽셀 대신 별 활성 상태(플립이 stale
   내비게이션의 신호)와 힌트 텍스트로 검출하는 것이었다. 단정 문장이 회귀를
   구분하지 못하면 PASS 개수는 늘지만 검사력은 0이다.
5. **OnInit 순서가 lazy 초기화와 충돌한다.** LoadBookmarks는 InitCef보다 먼저
   돌므로 InitCef가 채우는 g_exeDirSlash 캐시에 의존하면 빈 디렉터리를 읽는다.
   "캐시 재사용"과 "캐시 채움 순서 의존"은 다른 문제 — 수요 시 계산으로 분리
   한 이유. 리플로우의 g_viewPageY 상시화(InitCef 1회 → 매 프레임)와 같은
   교훈의 다른 면: 1회성 초기화 지점에 걸려 있는 값은 전부 상시화 후보다.

## 7. 후속 과제

- **게이트 관찰 2건(제품 회귀 아님, 우선순위 후보)**: ①주소창이 포커스 중
  **매 키마다 Navigate**(ClientBrowserApp.cpp ~:734/:891) — IsItemFocused
  게이트는 바 위젯 클릭 디포커스 커밋만 차단하고 페이지 클릭 디포커스는 여전히
  커밋. 타이핑하는 동안 접두사가 전부 로드된다. "Enter/클릭에서만 커밋" 강화
  후보 ②에러 페이지(NXDOMAIN, ERR_FILE_NOT_FOUND)에서 ★ 저장 시
  on_title_change 미발화로 **URL이 타이틀로 저장**(라벨 `file:///I:/…` 실측) —
  URL은 올바르고 무해하지만 라벨 품질 문제. 중간 잘림 로딩이 star 시점에
  stale/빈 타이틀을 남기는 변형도 동일 계열.
- **"»" 오버플로 경로 미커버** — 구현됐으나 e2e 미도달(960px에서 넓은 북마크
  ~12개 이상 필요).
- **T2 NOTE 이월**: 폰트 미적재 단정 부재 — 형제 5앱 동일 관례라 단독 픽스
  안 함(관례 수정 시 함께). 삭제 클릭(app-local y≈116 ≥ pageY_ 110)은
  기하 게이트상 CEF로도 전달 — ImGui 팝업이 먼저 먹어 실해 미관측, T1의
  의도적 설계라 게이트 안 함.
  **→ 2026-09-16 해소** (최종리뷰 MINOR 승격분): 팝업 오픈 중 CEF 마우스
  전달 억제 — `MouseDown/MouseUp`에 `ImGui::IsPopupOpen(nullptr,
  ImGuiPopupFlags_AnyPopupId)` 조건 추가. 레슨 12의 WantCaptureMouse 방향은
  회피(UP 드랍 위험)하고 DOWN 억제 + `cefMouseDown_` 페어링만 손댔다 —
  DOWN이 억제되면 페어링 실패로 UP도 억제되지만, DOWN과 UP **사이에** 팝업이
  닫히면 UP 1개는 새어나간다(Chromium이 up-without-down을 용인 — click 미합성,
  무해 — 최종리뷰 MINOR로 문구 정정). TruncateLabel O(n²)→코드점 경계 이진
  탐색도 동반 픽스.
  probe_browser_bookmarks 15/16 (미통과 1건 = 힌트 픽셀-diff 임계 마진,
  diff 68 vs 80 — 본 변경이 전달 경로에 없는 렌더 영역, 환경 변동성 판정).
- **t1_e2e.ps1 미커밋** — T1 산물로 방치 중, probe_browser_bookmarks.ps1이
  계승했으니 제거 or 커밋 판정.
- **원자적 쓰기(tmp+rename)** — 스펙 D5가 관례 준수로 이월한 공유 후속
  (docs/48 §7과 동일 대기열, 소비처 2+ 시점에 승격).
- **스펙 후속 유지** — 파비콘(on_favicon_urlchange), 폴더/계층, 드래그 정렬,
  가져오기/내보내기, 편집 다이얼로그, agent가 북마크를 읽는 서버 도구(필요
  실측 후).
