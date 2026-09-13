# 브라우저 북마크 (북마크 바 + bookmarks.json) 설계

> 2026-09-14. 사용자 요청: "브라우저에 북마크 있으면 좋겠다". 사실 기반은
> 인벤토리 스캔 (2026-09-14): 브라우저는 CEF C API OSR(창 없음) → BGRA 프레임
> → SDL 텍스처 → ImGui::Image. 앱은 현재 로드된 URL을 전혀 모르고
> (on_address_change 미연결), 영속 데이터는 없음.

---

## 0. 인벤토리 (스캔 사실 요약)

- **레이아웃**: 루트 960x640 WA_CHROMELESS(:360). 프레임 2 ImGui 창 —
  `browser_page`(전면 NoInputs, CEF 텍스처, :633)와 `browser_bar`(y=30..80,
  50pt 툴바 1행: <, >, URL 입력, Go, Home, :664). 페이지 원점 `pageY_` = 80
  (헤더 :50, 상수).
- **치명적 함정 — `g_viewPageY`**: CEF 공간 마우스 y 오프셋이 InitCef에서
  1회만 동기화(:602). pageY_를 런타임에 바꾸면 CEF 마우스 좌표가 전부 어긋남
  (:244/:260/:282가 전부 g_viewPageY 사용). 북마크 바로 pageY_를 올리면
  이 동기화를 상시화해야 한다.
- **URL 부재**: DisplayObj는 on_console_message만 등록(:580). C API에
  on_address_change(cef_display_handler_capi.h:71)/on_title_change(:79) 존재
  — "현재 페이지 북마크"에 전제.
- **영속 선례**: state 파일은 `<exeDir>\state\` (서버 StateDir() :2626).
  읽기는 `jk::agent::AgentJson`(QuickJS 파서, 클라 DLL 링크 가능 — filedlg가
  선례), 쓰기는 JsonEsc식 수작업 JSON 문자열 + fopen "wb" (trust.json
  SaveTrustRecords jktriggers/main.cpp:255 선례). 엔진에 원자적 쓰기 패턴
  전무 — 이번 패스도 관례 따름(후속 기록).
- **소속 규칙**: state 파일 소유자 = 런타임에 왕복 없이 필요한 쪽. 북마크는
  브라우저 단일 소비 → **브라우저 프로세스 로컬 소유, 서버 도구 불필요**
  (filedlg "module owns its core copy" 선례).
- **기존 즐겨찾기 구조 전무** — 북마크가 첫 영속 유저 데이터(트러스트/트리거는
  시스템 데이터).
- **테마**: ApplyImGuiTheme 이미 호출(:368) — Button/Selectable/PopupBg/Text
  전부 토큰화. 북마크 바는 신규 리터럴 0.

## 1. 설계

### 1a. 현재 URL/제목 추적 (전제 공사)

- DisplayObj에 on_address_change + on_title_change 연결 → 앱 멤버
  `currentUrl_`/`currentTitle_` 갱신(락: 콜백은 CEF UI 스레드 = 앱 스레드 동일
  — OSR integrated loop라 단일 스레드, 락 불필요).
- 주소창 표시 개선(부수 수혜): URL 입력이 포커스 잃으면 currentUrl_ 표시
  (빈 버퍼일 때만) — 브라우저 기본 동작에 부합.

### 1b. 북마크 바 (pageY_ 80→110)

- `browser_bar` 아래 두 번째 행을 **같은 ImGui 창 내 행 추가**가 아니라
  pageY_ 상수 80→110 + browser_bar 높이(pageY_-30) 확장으로 단일 창 유지 —
  북마크 바는 툴바의 둘째 행(별도 ImGui 창 아님). `g_viewPageY` 동기화를
  InitCef 1회식에서 **매 프레임 pageY_ 대입**으로 상시화(함정 제거).
- 위젯: 북마크마다 `Button(title)`(길이 클립, 툴팁=전체 제목+URL). 클릭 =
  Navigate(url). 우클릭 = `BeginPopupContextItem` 미니 메뉴 "삭제".
- 북마크가 많으면 행 오버플로 → 우측 끝에 "»" 더보기 콤보(넘치는 항목을
  popup 리스트로) — v1은 단순 클립 후보정: avail 넘는 항목은 콤보로.
- 추가/토글: 주소창 옆 "★" 버튼 — currentUrl_이 이미 북마크면 제거(토글),
  아니면 추가. 라벨은 currentTitle_ (빈 경우 URL). active 표시는 selectionBg
  토큰(푸시 스타일 Button 배경 스왑).
- 빈 상태: 북마크 0개면 행은 생략(pageY_ 110 고정 유지 — 리사이즈 시
  레이아웃 흔들림 방지, 빈 행에 힌트 텍스트 "★로 현재 페이지 추가").

### 1c. bookmarks.json

- 경로: `<exeDir>\state\bookmarks.json` (GetModuleFileNameA → exeDir —
  StateDir/DefaultThemePath 동일 관례). 브라우저 프로세스가 직접 소유.
- 스키마: `{"bookmarks":[{"title":"...","url":"..."}]}` (trust/triggers의
  행 배열 관례). 순서 = 추가 순서 = 표시 순서.
- 읽기: AgentJson + 256항목 상한(파손 파일 방어), 개별 행 이상은 스킵.
  **fail-open**: 파일 부재/파손 = 빈 바 (북마크는 유저 데이터 — trust.json의
  fail-closed와 반대 의미).
- 쓰기: 수작업 JSON( JsonEsc 동일 이스케이프) + fopen "wb" 오버라이트
  (SaveTrustRecords 선례). 추가/삭제/토글 시 즉시 저장(데이터량 미미).
- 동시성: 단일 스레드(CEF integrated loop) — 락 불필요.

### 1d. 범위 밖 (후속)

- 파비콘(on_favicon_urlchange), 폴더/계층, 드래그 정렬, 가져오기/내보내기,
  북마크 편집 다이얼로그, 원자적 쓰기(tmp+rename — 엔진 최초 도입 별도 태스크).

## 2. 검증

- 빌드 exit 0 + `jkdesktop test` 0 failures + 프로브 전량.
- 실측: 페이지 이동 → currentUrl_ 갱신 확인 → ★ 추가 → 바에 버튼 등장 +
  bookmarks.json 생성 확인 → 클릭 재이동 → 우클릭 삭제 → 토글 제거 동작.
  재기동 후 북마크 유지. **g_viewPageY 회귀**: 북마크 바 표시 상태에서 CEF
  페이지 클릭/휠 좌표 정확(링크 클릭이 의도 위치에 히트) — 픽스 검증의 핵심.
- 스크린샷: 북마크 바 표시 1장 + 테마 스왑 추종 1장.

## 3. 리스크

1. **g_viewPageY 상시화가 좌표를 두 번 밀거나 0으로 가는 것** — pageY_는
   상수 110이라 사실상 단순화이지만, InitCef가 첫 프레임 전에 pageY_를 이미
   알아야 함(상수니 무해). 상수화 전제가 깨지면(동적 바 높이) 재설계.
2. **on_address_change 빈도** — CEF가 리다이렉트마다 발화; currentUrl_ 갱신만
   하므로 무해. 다만 ★ 토글 판정이 최종 URL 기준이 됨(의도).
3. **bookmarks.json 파손** — fail-open이라 최악이 빈 바 + 다음 저장이 파손
   파일을 덮어씀(유저 데이터 소실). 256항목 상한 + 파손 시 원본 .bak 보존
   (1회, 무료 안전망).

## 4. 결정/직감 장부

| # | 결정/직감 | 근거 |
|---|---|---|
| D1 | **북마크 소유 = 브라우저 프로세스 로컬**, 서버 도구 없음 | 단일 소비자 + filedlg 모듈 로컬 선례 — 에이전트 접근 필요성 전무 |
| D2 | **pageY_ 상수 110 + browser_bar 2행 확장** — 별도 창/동적 높이 폐지 | g_viewPageY 1회 동기화 함정 제거가 최소 침해; 동적 높이는 리스크 1 |
| D3 | **fail-open + .bak 보존** | 유저 데이터(신뢰 저장소 아님) — trust.json과 반대 의미, 덮어쓰기 전 1회 백업이 파손 대비 유일 안전망 |
| D4 | **★ 토글 = 추가/제거 동일 버튼** | 표준 브라우저 관례, UI 최소화 |
| D5 | **쓰기는 수작업 JSON + fopen "wb" 관례 준수** — 원자적 쓰기 도입 안 함 | 엔진 전 관례 일관(YAGNI), 후속으로 원자적 쓰기 단독 태스크 기록 |
| 직감 | "북마크"의 어려움은 저장이 아니라 **현재 URL을 모르는 것**이었다 — on_address_change 연결이 기능의 실제 문이자 주소창 UX의 부수 수혜 | §0 |
| 직감 | pageY_ 함정은 스펙 단계에서 걸렸다 — 인벤토리 스캔이 런타임 좌표 어긋남을 컴파일 전에 잡은 사례 | §0 리스크 1 |

## 5. 후속

- 원자적 쓰기(tmp+rename) — 엔진 공용 유틸로 승격 후보 (소비처 2+ 시점)
- 파비콘 + 북마크 폴더 + 가져오기/내보내기
- agent가 북마크를 읽는 서버 도구 (필요 실측 후)