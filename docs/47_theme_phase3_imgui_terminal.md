# 47. P2 테마 단계 3 (ImGui 봉합 + 터미널 시딩 + 지정 초기자) as-built

- 날짜: 2026-09-13
- 상태: 구현 완료 (코드 커밋 c6ebce3 → 94692c8, 게이트 GATE GREEN — 15/15 프로브
  + 픽셀 측정 통과, 게이트 발견 Table*/Tab* 잔존은 즉시 픽스, 최종 전체리뷰
  APPROVE — MINOR 1건은 kLight 터미널 미실측 갭 기록으로 §4에 이월)
- 선행: docs/superpowers/specs/2026-09-13-theme-phase3-imgui-terminal-design.md
  (스펙), docs/superpowers/plans/2026-09-13-theme-phase3-imgui-terminal.md (플랜),
  docs/46 (단계 2 as-built — 토큰 체계와 대응표 관례의 선행)
- 비고: 게이트 보고서 `.superpowers/sdd/2026-09-13-theme-phase3-imgui-terminal/
  task-5-report.md`, Table*/Tab* 픽스 보고는 같은 디렉터리 task-5fix-report.md,
  태스크별 스왑 증거는 task-2/3/4-report.md.

## 1. 개요

단계 2(docs/46)가 JK 위젯 계열을 `current()` 봉합으로 묶었다면 단계 3은 그
바깥에 있던 두 계열을 같은 진실원에 붙인 일이다. (1) **ImGui 8앱의 스타일 훅
설치** — 스캔 결과가 "스타일 코드 전무"(전부 ImGui 기본 다크)였으므로 이번에
한 일은 새 기능이 아니라 부재하던 훅(`ApplyImGuiTheme()` 호출 1회)의 설치다.
(2) **터미널 부분 통합** — terminal.json에 키가 없을 때 `current()` 토큰으로
시딩하되 kDefault는 구값(0x0C0C0C/0xCCCCCC) 불변, 사용자 지정 키는 항상 우선.
(3) **C++20 전환 + 지정 초기자** — 3프리셋 × 37필드를 `.field = {...}`로
전환해 35필드 시절부터 계승된 위치 의존 위험을 구조적으로 닫았다
(docs/46 §7 후속 실행).

**사용자 통합 방향 결정 (2026-09-13)**: JK계열↔ImGui계열을 한방에 통합하지
않고 **"하나씩 JK계열로 흡수해가며 JK를 개선"**한다 (앱 단위 점진 흡수 —
killer app 흡수 패턴, docs/44). 흡수 순서는 P4 앱 SDK 계약에서 앱별 판단.
단계 3의 팔레트 봉합은 흡수 이전에도 두 계열이 같은 토큰을 읽게 하는
과도기 봉합으로 유효 → **P4 봉인 링크**.

로드맵 위치: P1 split → P2 단계 1(셸) → 단계 2(위젯+스위칭) → **단계 3
(ImGui+터미널) 완료**. 다음은 P3(핫스왑 + 설정 UI)와 P4(앱 SDK 계약 + 흡수).

## 2. 단계별 커밋 (4 feat + 1 fix + 2 docs — 6f8ee0a → 94692c8, 실측 `git log --oneline 6f8ee0a..HEAD`)

| 커밋 | 분류 | 내용 |
|---|---|---|
| f88794f | docs | 통합 방향 기록 — "하나씩 JK계열로 흡수" 사용자 결정 |
| e9a2eaa | docs | jkchat 흡수 1순위 후보 기록 (사용자 피드백) |
| c6ebce3 | feat (T1) | C++20 전역 전환(CMAKE_CXX_STANDARD 17→20) + 3프리셋 지정 초기자 |
| 9491885 | feat (T2) | JKThemeImGui 팔레트 봉합(헤더 온리) + terminalBg/Fg 토큰 2종 |
| 5c99bf8 | feat (T3) | ImGui 8앱 봉합 배선 — 루트 클리어 appClearBg 통일 |
| b0e22e7 | feat (T4) | 터미널 themeBg/Fg 키 부재 시 current() 시딩 (플래그 판별) |
| 94692c8 | fix (T5fix) | 봉합을 ImGuiCol_Table*/Tab*까지 확장 — 게이트 실측 기본값 잔존 |

Task 5(최종 게이트)는 검증 전용이라 커밋 없음. 94692c8은 게이트가 잡은
발견의 즉시 픽스(§4)다.

## 3. 기존→신규 대응표

### (a) ImGuiCol 매핑 (JKThemeImGui.h 실측 — 전부 `current()` 토큰, 리터럴 1건)

| ImGuiCol 클러스터 | 토큰 (상태 변주) |
|---|---|
| WindowBg / ChildBg / PopupBg | windowClientBg |
| MenuBarBg / TitleBg / TitleBgActive / TitleBgCollapsed | chromeTitleBg |
| Text / TextDisabled | widgetText (Disabled는 alphaScale 0.5) |
| Border / BorderShadow | chromeBorder / bevelDark |
| Separator / Hovered / Active | bevelLight / bevelMid / focusRing |
| FrameBg / Hovered / Active | fieldBg (호버+0.08, 액티브+0.16 Lighten) |
| Button / Hovered / Active | widgetFace (호버+0.08, 액티브 bevelMid) |
| Header / Hovered / Active | selectionBg (호버+0.12, 액티브 bevelMid) |
| CheckMark | widgetText |
| ScrollbarBg / Grab / GrabHovered / GrabActive | scrollbarTrack / scrollbarThumb (+0.10) / focusRing |
| SliderGrab / SliderGrabActive | selectionBg / focusRing |
| TextSelectedBg | selectionBg (alphaScale 0.6) |
| NavCursor | selectionBg (alphaScale 0.5) — 1.92.9b 정식 명칭, NavHighlight는 폐기 예정 별칭 |
| ResizeGrip / Hovered / Active | bevelLight / bevelMid / focusRing |
| DragDropTarget | focusRing |
| **ModalWindowDimBg** | **유일 리터럴** (0,0,0, 96/255) — 디밍은 기능색, 테마 토큰 아님 |
| TableHeaderBg / BorderStrong / BorderLight | widgetFace / bevelDark / bevelLight |
| TableRowBg / RowBgAlt | windowClientBg / Lighten(+0.05) — 얼룩말 줄 |
| Tab / TabHovered | widgetFace / Lighten(+0.12) |
| TabActive | Lighten(+0.08) — **TabSelected 별칭 슬롯이라 미작동** (아래 (e)) |
| TabSelected / TabSelectedOverline | selectionBg |
| TabDimmed / TabDimmedSelected | Lighten(windowClientBg, +0.04) / (+0.10) |

범위 밖: implot 계열 enum(플롯 스타일은 implot 소관), 지오메트리
(PushStyleVar/라운딩/패딩 — 이 헤더는 절대 안 건드림).

### (b) 루트 클리어 → appClearBg (스펙 "4종"의 실측 정정: 7앱 7사이트·서로 다른 값 5종)

스펙 §0 스캔은 "4종(36,36,43 / 32,32,38 / 24,24,30×2)"이었으나 실측(task-3)은
**7앱 7사이트**(snap은 무페인트 투명 오버레이라 제외), 서로 다른 리터럴 5종.
vplayer(18,18,22)와 browser(24,24,28)는 **브리프 스캔 누락** — 구현에서
발견돼 함께 통일됐다. 스캔 수치는 drift한다 (§5 직감 1).

| 파일 | 구값 리터럴 | kDefault | kClassic |
|---|---|---|---|
| ClientImGuiDemoApp.cpp | (36,36,43) | (32,32,32) | (192,192,192) |
| ClientTaskmgrApp.cpp | (32,32,38) | 〃 | 〃 |
| ClientPaletteApp.cpp | (24,24,30) | 〃 | 〃 |
| ClientNotifyApp.cpp | (24,24,30) | 〃 | 〃 |
| ClientShotApp.cpp | (24,24,30) | 〃 | 〃 |
| ClientVPlayerApp.cpp | (18,18,22) ⚠ 스캔 누락 | 〃 | 〃 |
| ClientBrowserApp.cpp | (24,24,28) ⚠ 스캔 누락 | 〃 | 〃 |
| ClientSnapApp.cpp | 없음 (투명 WindowBg push) | — | — |

가시 변화: 앱별 틴트가 수렴한다(스펙 리스크 2 — 의도된 동작, YAGNI로 앱별
토큰 분리 안 함). kDefault에서 vplayer/browser는 구값보다 밝아지고,
kClassic에서 8앱 전부 실버로 따라온다.

### (c) 터미널 시딩 (키 부재 시 current(), 구값 불변)

`JKTerminalConfig`에 `themeBgSet`/`themeFgSet` 플래그 추가 — getColor가
`bool`을 반환(성공시에만 기록, 파싱/폴백/출력 시맨틱 바이트 불변). 시딩
2곳(ClientTerminalApp.cpp, TerminalApp.cpp)은 **반키 독립 삼항**:

```cpp
const auto& t = jk::theme::current();
view->SetTheme(cfg.themeBgSet ? cfg.themeBg : ThemeRgb(t.terminalBg),
               cfg.themeFgSet ? cfg.themeFg : ThemeRgb(t.terminalFg));
```

| 상태 | 배경 | 전경 |
|---|---|---|
| 키 지정 (terminal.json) | **사용자값 우선** (themeBgSet) | **사용자값 우선** (themeFgSet) |
| 키 부재 → kDefault/kClassic | (12,12,12) = 0x0C0C0C **구값 불변** | (204,204,204) = 0xCCCCCC **구값 불변** |
| 키 부재 → kLight | (250,250,250) = 0xFAFAFA | (31,31,31) = 0x1F1F1F |

- **ThemeRgb 패커**: 브리프 스케치는 SDL_Color 토큰을 그대로 넘겼으나
  `TerminalView::SetTheme(uint32_t, uint32_t)`라(SDL_Color↔uint32_t 혼합 불가)
  SDL_Color→0xRRGGBB file-local 패커가 필요했다. 공개 인터페이스 변경 없음.
- 키 부재 상태에서 kDefault 시딩값 = 구 하드코딩 기본값과 정확히 동일 →
  기존 terminal.json 무키 파일과의 동작 차이 0. 반키 파일은 해당 키만
  사용자값 (필수 반키 케이스).
- **키 파싱 실패 시**(예: `"themeBg": "red"` 같은 비정형값)는 키 부재와
  동일하게 취급 — 플래그 false → 토큰 시딩. "파싱 불가 ≠ 사용자 선택"이므로
  정확한 시맨틱이나, kLight 프리셋 하에서는 구 동작(다크 0x0C0C0C)과 달리
  라이트 배경으로 시딩될 수 있다 (최종리뷰 NOTE — 이월 없음, 기록만).
- **VT 16색 팔레트 미변경** — ANSI 색은 터미널 표준 영역 (스펙 D4).

### (d) 의미색 잔존 목록 (테트리스 퍼플 카테고리 — 코드 무변경, 의도 주석 부착)

| 앱 | 색 | 내용 |
|---|---|---|
| notify | 앰버 | 미읽 PushStyleColor(ImGuiCol_Text, {1.0,0.85,0.5,1.0}) |
| vplayer | 4색 + TextColored 3건 | 조그 오버레이 반투명 push(Button/Hovered/Active/Text, 알파 포함) + 에러 레드 2건·힌트 그레이 1건 |
| snap | 투명 | ImGuiCol_WindowBg 투명 push (기능적 투명, 팔레트 아님) |
| browser | 에러 레드 | CEF 초기화 실패 TextColored |

### (e) 미매핑/편차 항목 (명시 기록)

1. **selectionText 토큰 미소비** — 계획은 "HeaderText/TextSelectedBg 계열 =
   selectionText/selectionBg"였으나 ImGui에 HeaderText가 없어 TextSelectedBg만
   selectionBg로 매핑됐고 selectionText는 이번 봉합에서 소비처 0.
2. **NavCursor = selectionBg@0.5 (계획 편차)** — 계획 서술(“NavCursor
   deprecated”)는 역방향이었다. 1.92.9b(imgui.h:1815/1825)에서 **NavCursor가
   정식**, NavHighlight가 폐기 예정 별칭. 정식 명칭 채택, 코드 주석으로 근거 남김.
3. **TabActive = TabSelected 별칭** — 1.90.9+에서 동일 enum 슬롯. 라인 순서상
   나중 배정인 `TabSelected → selectionBg`가 최종값이 승리하므로 TabActive
   라인은 미작동(사실상 selectionBg 렌더). forward-compat(ImGui가 슬롯을
   재분리할 경우) 위해 라인+주석 유지.
4. **SliderGrabHovered 미존재** — 매핑표 외 추가 시도를 컴파일러가
   "1.92.9b 미존재"로 잡아 제거. SliderGrab/Active만 설정.
5. **alphaScale 변주는 알파 전용** — 계획 스케치의 ×1.2 밝기 변주는 알파
   상한(1.0) 때문에 불가능 → 밝기 변주는 전부 Lighten RGB 헬퍼로 구현.

## 4. 검증 (task-5-report.md GATE GREEN + task-5fix)

- **빌드 exit 0 + 11아티팩트 mtime 게이트** — jkdesktop.exe, jkwinserver.exe,
  jkapp_taskbar.dll + jkapp_*.dll 8종 전부 artifact mtime > 최신 소스.
  JKThemeImGui.h vs jkwinserver.exe의 전역 mtime 오탐은 `ninja -n` →
  "no work to do" 11타깃으로 소멸 확인.
- **`jkdesktop test` → 0 failures** (terminal.json 파싱 시나리오 6건 포함 전부 PASS).
- **프로브 15/15 PASS — 전부 첫 시도** (docs/46의 15/15 재현. 자가 스폰형).
- **kDefault 누수 grep 0건** — `jk::theme::kDefault` 참조가 JKTheme.h /
  JKThemeConfig.cpp 밖에 없음. 봉합(JKThemeImGui.h)도 포함 — 팔레트는 전부
  current() 경유.
- **지정 초기자 기계 검증 37×3** — 구조체 37필드(35 + terminalBg/Fg) ×
  3프리셋 = 라벨 1:1, 라벨 중복 없음, 선언 순서 일치(C++20 요구),
  터미널 쌍 3프리셋 전부 존재. 값 스팟 체크(kClassic chromeTitleBg 네이비 등) PASS.
- **ImGui 스크린샷 픽셀 측정** (capture_window 에이전트 도구, docs/35 —
  서버 모드 SDL 창은 화면 캡처 불가라 엔진 자체 도구 사용):
  다크 창 (32,32,32)·필드 (26,26,26) / 클래식 창 (240,240,240)·필드
  (255,255,255) — 팔레트 스위치 명확.
- **터미널 3상 측정** (단일 프로세스, 코너 4점 각 런 동일):
  (a) 픽스처 없음 → **(12,12,12)** = kDefault 시딩, 구값 유지 실증.
  (b) terminal.json `{"themeBg":"#7A1FA2","themeFg":"#FFD54F"}` →
  **(122,31,162)** = #7A1FA2 정확 + 프롬프트 #FFD54F — 사용자값 우선.
  (c) classic + 무 terminal.json → **(12,12,12)** = kClassic 토큰 추종
  (kClassic terminalBg = kDefault 동일값 설계), 크롬 네이비로 프리셋 활성 확인.
- **검증 갭 기록 (최종리뷰 MINOR-1)** — 스펙 §2의 "kClassic/kLight 스위치
  추종" 중 **kLight 시딩 상태는 픽셀 미실측**이다 (kClassic 측정은
  kClassic=kDefault 동일값이라 약한 오라클 — 네이비 크롬의 간접 증명만 성립).
  코드 경로는 구조적으로 검증(토큰 값 (250,250,250)/(31,31,31)) — P3에서
  1회 kLight 픽셀 측정으로 닫을 것.
- **Table 헤더 픽스 실측 (94692c8)** — 게이트가 클래식 taskmgr 테이블 헤더가
  ImGui 기본값 **(48,48,51)**로 남아 (240,240,240) 실버 위에 어두운 띠로
  위화감을 일으키는 것을 픽셀로 발견 → 봉합에 Table*/Tab* 12항목 확장 후
  **(192,192,192)** = kClassic widgetFace 정확. 열 구분선 (255,255,255) =
  bevelLight, 표 본문 (240,240,240) PASS.
- **per-frame 비용 0** — ApplyImGuiTheme는 CreateContext 직후 1회 호출이며
  current()는 inline 변수 읽기 O(1)이라 프레임당 비용 없음 (팔레트는 스타일
  스냅샷 — P3 핫스왑 시 재적용 지점의 근거, §6).
- 최종 상태: theme.json / terminal.json 부재, 잔류 프로세스 없음, 트리 클린.

## 5. 실행 직감 (스펙 §4 장부에 일부 기록)

1. **스캔 수치는 drift한다 — 컴파일러와 픽셀이 판정기다.** 루트 클리어
   "4종" 스캔은 실측 7사이트로 드러났고(vplayer/browser 스캔 누락), 계획의
   SliderGrabHovered는 컴파일러가, TableHeaderBg 잔존은 픽셀 (48,48,51)이
   잡았다. 브리프 표를 믿지 말고 사이트에서 다시 세라.
2. **게이트가 잡은 기본값 잔존은 문서로 남기고 즉시 픽스한다.** 94692c8의
   코드 주석이 "게이트 실측에서 기본값 잔존 확인 (docs/47)"을 직접 인용 —
   발견→기록→픽스가 같은 세션에서 닫히면 문서와 코드가 같은 이력을 가리킨다.
3. **"폴백=기본값" 시스템은 시딩이 들어오는 순간 플래그가 필수가 된다.**
   getColor의 키 부재≠기본값 판별 불가는 구값 시대엔 무해했지만
   current() 시딩이 들어오는 순간 "사용자가 골랐다"는 정보가 사라진다 —
   bool 2개의 추가가 아니라 정보의 복원이었다.
4. **같은 슬롯의 이중 배정은 나중이 이긴다.** TabActive(TabSelected 별칭
   슬롯) 매핑은 TabSelected 배정에 덮인다 — 죽은 라인을 지르지 말고
   별칭 사실과 함께 forward-compat 주석으로 남긴다.
5. **알파 스케일로 밝기를 만들 수 없다.** 계획의 ×1.2 변주 스케치는 알파
   상한 때문에 구현 불가 — 밝기 변주는 RGB 헬퍼(Lighten, 클램프)의 전유물.

## 6. 후속 과제

- **P3 핫스왑** — current() 봉합 위의 와이어 이벤트 + 시작 메뉴 설정 UI.
  생성 시점 평가 재초기화 전략 필요: JK 위젯 멤버 기본값/ctor 캡처(docs/46
  §7 계승) + 이번에 추가된 ImGui 팔레트 스냅샷(ApplyImGuiTheme가 CreateContext
  시점 1회) 모두 핫스왑 시 재적용 지점이 필요하다.
- **P4 앱 SDK 계약 + JK계열 흡수** — 통합 방향 확정(2026-09-13 "하나씩
  JK계열로 흡수"). **jkchat 1순위** (사용자 피드백 "JKChat도 너무 안이쁘긴 해";
  권장 경로는 Win32 재스타일링이 아니라 JKControl 재작성 — 스펙 §5).
  앱이 스스로 의미색 토큰화를 재판단하는 P4 앱 템플릿과 연결.
- **봉합 확장 잔여** — Table*/Tab*은 94692c8로 완료. 남은 것: 시딩 2곳의
  file-local `ThemeRgb` 패커 중복(공용 헬퍼 승격 후속), ImGui 미매핑 나머지
  (TextLink, NavWindowing*, TreeLines 등 — 필요성 실측 전 유지), VT 16색
  팔레트는 표준성 보존 원칙으로 영구 범위 밖.
- **브라우저 북마크 기능 (신규 사용자 요청, 2026-09-13)** — ClientBrowserApp
  기능 요청. P4 흡수/SDK 계약 논의와 별도 트랙으로 기록.
