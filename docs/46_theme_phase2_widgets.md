# 46. P2 테마 단계 2 (위젯 라이브러리 + 스위칭 봉합) as-built

- 날짜: 2026-09-13
- 상태: 구현 완료 (코드 커밋 f0bb374 → 9121f4c, 게이트 GATE_GREEN — Task 5 최종
  게이트 15/15 프로브 + 픽셀 측정 통과)
- 선행: docs/superpowers/specs/2026-09-13-theme-phase2-widgets-design.md (스펙),
  docs/superpowers/plans/2026-09-13-theme-phase2-widgets.md (플랜),
  docs/45 (단계 1 as-built — kClassic 값표의 원본이자 대응표 관례의 선행)
- 비고: 게이트 보고서 `.superpowers/sdd/2026-09-13-theme-phase2-widgets/
  task-5-report.md`, 태스크별 스왑 증거는 같은 디렉터리 task-1/2/3-report.md.

## 1. 개요

위젯 라이브러리(Win95 실버 계열)의 하드코딩 색 사이트 전부를 `jk::theme` 위젯
토큰 15종으로 교체하고, 모든 그리기 사이트를 `jk::theme::current()` 봉합으로
전환했다. 단계 1(docs/45)이 "색의 단일 진실원"을 만들었다면 단계 2는 그
진실원을 **가리키는 포인터**를 만든 일이다 — `activeTheme_` inline 변수 +
`current()`/`setTheme()` 봉합으로, 프리셋 추가는 이제 constexpr 데이터 1블록이
된다. 실제로 `kLight`(Win11 라이트 신규 값표)와 `kClassic`(단계 1 이전 값 전부)
이 블록 1개씩으로 추가됐다.

프리셋 로딩은 `loadPresetFromFile` — theme.json의
`{"preset": "dark"|"light"|"classic"}` **1키만** 해석하며 quickjs를 띄우지
않는다(스펙 D3 YAGNI). 파일 없음/깨짐 → false, 활성 테마 불변. 기동 시 서버
main / jkwinserver main / JKClientApplication::Init 3점에서 exe 옆 theme.json을
1회 로딩한다(terminal.json 패턴 준용, 이중 로딩 무해).

**사용자 결정 (2026-09-13)**: 테마 변경 기능은 **current() 봉합 + 프리셋**
수준으로 단계 2에 포함. 핫스왑(렌더 중 스왑)·와이어 이벤트·설정 UI는 P3로
연기 — 46사이트를 지금 어차피 건드리므로 current() 전환의 한계비용이 현재
최소라는 판단(스펙 D1). 기동 시 1회 setTheme가 정석이며, 멤버 기본값/ctor
캡처는 생성 시점 평가이므로 기동 로딩 프리셋을 자연히 따른다.

로드맵 위치: P1 split → P2 단계 1(셸) → **단계 2(위젯+스위칭) 완료**. 다음은
P3(핫스왑 + 시작 메뉴 설정 UI)와 단계 3(ImGui PushStyleColor 래퍼, 터미널
통합 재판단).

## 2. 단계별 커밋 (4 feat + 3 fix — f0bb374 → 9121f4c)

| 커밋 | 내용 |
|---|---|
| f0bb374 | feat(theme): 위젯 토큰 15종 + kLight/kClassic 프리셋 + current() 봉합 + theme.json 로더 (JKTheme.h + JKThemeConfig.cpp 신규 + CMake jkcore 목록) |
| 5c79bfa | fix(theme): kClassic taskbar 주석 10→9필드 교정 + 미사용 include 제거 (T1 review — 주석/클린 전용) |
| 95eac9a | feat(theme): 위젯 3D/표면 클러스터 + 단계 1 소비처 current() 전환 (JKDC.h 기본 인자, JKButton/JKScrollBar/JKMessageBox/JKWindow/JKClientApplication/AppLauncherItem ctor, 컴포지터/셸/태스크바 참조 전환) |
| 9b1da3f | fix(theme): 런처 셀 Box3D paint를 current() 경유 — pressed 베벨은 인자 스왑으로 (T2 ruling) |
| 66ef5a4 | feat(theme): 필드/텍스트 위젯 클러스터 — IME 알파 보존 + 메뉴 팝업 리터럴 교정 포함 (JKEdit/JKListBox/JKComboBox/JKCheckBox/JKMenu/JKControl.cpp + JKControl.h 멤버 기본값) |
| 928fd3f | fix(theme): JKMenu 팝업 텍스트 주석 메커니즘 교정 (T3 review — 주석 전용, 코드 변경 없음) |
| 9121f4c | feat(theme): theme.json 프리셋 기동 로딩 와이어 — 서버 main / jkwinserver main / 클라 Init 3점 |

Task 5(최종 게이트)는 검증 전용이라 커밋 없음. 본 문서의 커밋 a6b344d는 표 범위 밖.

## 3. 기존→신규 대응표 (역할 클러스터 단위)

되돌릴 때의 기준(스펙 §4 장부 D2: kClassic = 구값 전부). 브리프의 라인번호
표는 drift되어 있었고, 구현은 **역할 우선**으로 재매핑했다 — 어긋난 3건은
각주로 기록. 값이 움직인 클러스터와 움직이지 않은 것을 구분.

| 클러스터 (파일) | 역할 | 기존 리터럴 | 토큰 | kDefault 신값 |
|---|---|---|---|---|
| JKDC.h 기본 인자 | Box3D 면/밝은에지/어두운에지 | 192 / 255 / 0 | widgetFace / bevelLight / bevelDark | (43) / (70) / (16) |
| 〃 | Rectangle3D light/dark | 255 / 0 | bevelLight / bevelDark | (70) / (16) |
| JKButton.cpp ctor | 면/텍스트 | 192 / 0 | widgetFace / widgetText | (43) / (240) |
| 〃 paint | 베벨 (Rectangle3D 무인자) | — | **무수정** — JKDC.h 기본 인자가 자동 테마 추종 | — |
| JKScrollBar.cpp ctor | 면/텍스트 ¹ | 192 / 0 | widgetFace / widgetText | (43) / (240) |
| 〃 OnPaintClient 외곽 | 프레임 면+베벨 | 192 / 255 / 0 | widgetFace / bevelLight / bevelDark | (43) / (70) / (16) |
| 〃 트랙 fill | 트랙 | 220 | scrollbarTrack | (56) |
| 〃 썸 Box3D | 썸 면/밝은에지/그림자 | 255 / 255 / 128 | scrollbarThumb / bevelLight / bevelMid | (86) / (70) / (40) |
| JKMessageBox.cpp | 면/텍스트 ² | 240 / 0 | widgetFace / widgetText | (43) / (240) |
| JKWindow.cpp 클라 배경 | 클라이언트 표면 | 240 | windowClientBg | (32) — 단계 1 스왑 금지 해제 |
| 〃 크롬 | 타이틀/버튼/테두리 토큰 참조 | `kDefault` | `current()` (값 변화 없음) | — |
| JKClientApplication.cpp | 앱 표면 클리어 | 192 | appClearBg | (32) |
| AppLauncherItem.cpp ctor | 셀 면/텍스트 | 192 / 0 | widgetFace / widgetText | (43) / (240) |
| 〃 paint Box3D (Ruling 2) | 올라온 상태 면/베벨 | 192 / 255 / 0 | widgetFace / bevelLight / bevelDark | (43) / (70) / (16) |
| 〃 〃 눌린 상태 | 면 + 베벨 반전 | 면 128 / light 0 / dark 255 | widgetFace + **light 슬롯에 bevelDark, dark 슬롯에 bevelLight** ³ | (43) / (16)/(70) 스왑 |
| JKControl.h 멤버 기본값 | 일반 텍스트 | 0 | widgetText | (240) |
| 〃 | 기본 컨트롤 면 | 240 | widgetFace | (43) |
| JKEdit.cpp ctor+재페인트 | 필드 배경/텍스트 | 255 / 0 | fieldBg / widgetText | (26) / (240) |
| 〃 Box3D | 썬크 필드 경계 | 255 / 255 / 0 | fieldBg / bevelLight / bevelDark | (26) / (70) / (16) |
| 〃 읽기전용 배경 | 비활성 필드 면 | 240 | widgetFace | (43) |
| 〃 선택 | 배경/텍스트 | (0,0,128) / 흰색 | selectionBg / selectionText | (0,120,212) / 흰색 불변 |
| 〃 캐럿 | 캐럿 | 0 | widgetText | (240) |
| 〃 IME 조합 배경 | 조합 배경 | (0,0,255,**64**) | imeCompositionBg + **알파 64 리터럴 고정** ⁴ | (0,120,212,64) |
| 〃 IME 조합 캐럿 | 조합 캐럿/에러 | (255,0,0) | imeCaret (**값 유지 토큰** — 3 프리셋 모두 동일값) | (255,0,0) |
| JKListBox.cpp | 배경/텍스트, Box3D, 선택 | 255/0, 255/255/0, 네이비/흰색 | fieldBg/widgetText, fieldBg/bevelLight/bevelDark, selectionBg/selectionText | 상동 |
| JKComboBox.cpp 필드 | 배경/텍스트, Box3D | 255 / 0 | fieldBg / widgetText (+ Box3D 상동) | (26) / (240) |
| 〃 드롭버튼 Box3D | 면/밝은에지/그림자 ⁵ | 192 / 255 / **128** | widgetFace / bevelLight / bevelMid | (43) / (70) / (40) |
| 〃 화살표 | 글리프 | 0 | widgetText | (240) |
| JKCheckBox.cpp ctor | 면/텍스트 | 240 / 0 | widgetFace / widgetText | (43) / (240) |
| 〃 체크 상자 Box3D ⁶ | 입력 표면 경계·내부 | 255 / 255 / 0 | fieldBg / bevelLight / bevelDark | (26) / (70) / (16) |
| 〃 체크마크 | ✓ 글리프 | 0 | widgetText | (240) |
| JKStatic.cpp | 텍스트/면 | — | **수정 없음** — 소비 리터럴은 JKControl.h 멤버 기본값이며 Step 1에서 토큰화됨 (브리프가 멤버 기본값을 JKStatic.cpp 라인으로 오인) | — |
| JKMenu.cpp 메뉴바 | 면/텍스트, 활성 아이템 | 192/0, (0,0,128)/흰색 | widgetFace/widgetText, selectionBg/selectionText | 상동 |
| 〃 팝업 | 배경/선택 행 | 192, 네이비/흰색 | widgetFace, selectionBg/selectionText | 상동 |
| 〃 팝업 일반 텍스트 (:160) | 텍스트 | 0 | widgetText — **교정 완료** ⁷ | (240) |
| 〃 팝업 Rectangle3D | 베벨 | — | **무수정** — JKDC.h 기본 인자 경유(호출 시점 평가) | — |
| JKControl.cpp PaintFocus | 포커스 링 | (0,0,255) | focusRing | (0,120,212) |
| 단계 1 소비처 3 TU | 토큰 참조 | `kDefault` ×5 | `current()` ×5 (값 변화 없음 — JKCompositor 2, JKDesktopShell 1, ClientTaskbarApp 2) | — |

**범위 밖 잔존 (의도)**: 테트리스 퍼플, TitleChipColor FNV-1a, 터미널
(terminal.json), ImGui 앱, ClientTestWindowApp/EquipApp 콘텐츠 색 — 스펙 §1c.

주석 (브리프 대비 역할 재매핑/정정 — 브리프 라인번호 표의 drift):

1. **JKScrollBar ctor 오기(Erratum)** — 브리프가 :13,14를 "트랙/썸"이라
   표기했으나 실제 내용은 ctor `SetBackColor(192)/SetTextColor(0)` (그리고
   이 ctor 값은 스크롤바 페인트 경로가 쓰지 않는다 — 페인트는 Box3D에 명시적
   색을 전달). 역할 = 위젯 면/텍스트 → `widgetFace`/`widgetText`. 실제
   트랙(220)/썸(255) 리터럴은 OnPaintClient에서 별도 매핑. 최종 토큰 목록은
   브리프와 동일 — 라인 라벨링만 틀렸다.
2. **JKMessageBox 면은 192가 아니라 240** — 메시지 JKStatic의 back color라
   스태틱 면 값. 스태틱 면 전용 토큰은 없고 플랜의 제약(스태틱 면 =
   widgetFace 통합 + kClassic 240→192 한계 수용)대로 `widgetFace` 매핑.
3. **AppLauncherItem 눌린 상태 (T2 Ruling 2)** — 구현은 눌린-보기를 인자
   스왑(bevelDark가 위, bevelLight가 아래)으로 보존하고, 구 면 어둡게 하기
   (192→128)는 **의도적으로 폐기**(다크 팔레트는 이미 어두운 베이스 면).
   모든 프리셋에서 눌린 셀 면이 달라 보이는 유일한 시각 변화 — kClassic에서도
   128이 아닌 192로 렌더(§4).
4. **IME 알파 보존** — 조합 배경 2곳 모두 rgb만 토큰에서 가져오고 알파 64는
   리터럴로 고정(프리셋의 alpha 필드와 무관하게 불변). 조합 캐럿은
   `imeCaret` 토큰 + 알파 255, 원값 (255,0,0) 유지 — 3 프리셋 모두 동일값.
5. **JKComboBox 드롭버튼 세번째 색 오기(Erratum)** — 브리프 표에 128
   그림자 리터럴이 누락돼 있었음. 실제 Box3D는 192/255/**128** 3색이고
   128 → `bevelMid` 매핑 (kClassic bevelMid=128과 정확히 일치 — 역할+구값
   교차 검증).
6. **JKCheckBox :22 Box3D 오기(Erratum)** — 브리프 표에 이 리터럴 행이
   없었음. 255/255/0 → `fieldBg`/`bevelLight`/`bevelDark`. 체크 상자 내부를
   fieldBg로 매핑한 근거: 원 리터럴 255 = kClassic fieldBg와 정확히 일치
   (kClassic widgetFace=192와는 불일치) + 역할이 입력 표면.
7. **JKMenu :160 교정 + 정정** — 팝업 일반 텍스트 리터럴 0 → `widgetText`
   스왑으로 스펙 §1c-4(유일한 페인트 내 우회 교정) 충족. 단, 부모 노트의
   "직접 SDL_SetRenderDrawColor 우회" 서술은 부정확 — 실제는
   `dc.SetTextColor(0,0,0)`의 **JKDC 파이프라인 경유**였고 우회는 아니었다.
   코드 변경 없이 주석만 교정(커밋 928fd3f).

## 4. kClassic 알려진 한계 (수용된 8비트 불일치)

kClassic은 "단계 1 이전 값 전부"가 원칙이지만, 단일 토큰 통합이 강제한
차이가 2곳 있다. 둘 다 플랜 Global Constraints에서 명시 수용.

1. **스태틱/체크박스 면 — 구값 (240,240,240) → kClassic에서 (192,192,192)
   렌더**. 구 코드는 위젯 면(실버 192)과 스태틱/체크박스 기본 면(240)을
   리터럴로 구분했지만, 토큰은 widgetFace 하나로 통합했다. 회귀 게이트는
   버튼 면(구값 192) 기준으로 측정 — 스태틱 면 240은 회귀 진실값에서
   의도적으로 이탈.
2. **런처 셀 눌린 면 — 구값 128(면 어둡게 하기) → kClassic에서 192 렌더**.
   Ruling 2의 비용(§3 주석 3). 게이트 측정 표면 밖이라 픽셀 검증은 없음.

## 5. 검증 (task-5-report.md — GATE GREEN)

- **빌드 exit 0 + mtime 게이트** — jkdesktop.exe / jkwinserver.exe /
  jkapp_taskbar.dll (21:35:28~34) > 최신 소스 (main.cpp/jkwinserver_main.cpp
  21:35:16). 레슨 37 준용.
- **`jkdesktop test` → 0 failures**.
- **프로브 15/15 PASS — 전부 첫 시도, 재시도 0회** (probe_agent_e2e 포함 —
  docs/45의 3회 재시도와 대비. 자가 스폰형이라 수동 기동 불필요 확인).
- **kDefault 누수 grep 0건** — `jk::theme::kDefault` 참조가 JKTheme.h /
  JKThemeConfig.cpp 밖에 없음 (예상됐던 main.cpp 단위테스트 참조조차 없음).
- **kDefault(다크) 픽셀 측정** — 단일 프로세스 minesweeper(App Launcher 모드;
  런처 셀 면이 `current().widgetFace` 소비라 측정 표면으로 적법):
  테트리스 셀 면 (250,90)/(250,140), 지뢰찾기 셀 면 (110,145) 전부
  **(43,43,43) 정확**, 클라 배경 (32,32,32) = windowClientBg/appClearBg 일치.
- **kClassic 픽셀 회귀 PASS** — theme.json `{"preset": "classic"}` → 기동 로그
  `[theme] preset 'classic'` 확인, 셀 면 3곳 **(192,192,192) 정확**, 창 내
  타이틀바 **(0,0,128) 네이비 정확**, 클라 (240,240,240) = kClassic
  windowClientBg. 눈검증: 단계 1 이전 완전 복원. 측정 후 theme.json 삭제 확인.
- **kLight 스모크 PASS** — `{"preset": "light"}` → 셀 면 (240,240,240),
  타이틀 (243,243,243), 클라 (249,249,249), 검은 텍스트 — Win11 라이트
  정상 렌더, 다크 잔여 없음. 측정 후 theme.json 삭제 확인.
- **JKMenu 팝업 스모크 NOT RUN** — 팝업을 구동하는 기존 프로브/스크립트가
  없음(tools/ 전역 grep 무매치). 브리프 지시(기존 경로만 사용)에 따라 미실시
  기록. 팝업 코드 경로는 빌드/셀프테스트로 컴파일·실행됐고 시각 스모크만
  결여 — **후속**으로 SendInput 우클릭 프로브 제안.
- **최종 상태** — theme.json 부재 확인, 잔류 프로세스 없음.
- 스크린샷: `.superpowers/sdd/2026-09-13-theme-phase2-widgets/` 밑의
  `theme-phase2-dark.png`, `theme-phase2-classic-regression.png`,
  `theme-phase2-light.png` (+측정 헬퍼 gate_capture.ps1/gate_measure.ps1).

## 6. 실행 직감 (스펙 §4 장부에 기록)

- **역할 우선 매핑이 라인번호 표를 이겼다** — 브리프 대응표에서 3건의 오류가
  발견됐고(JKScrollBar 라벨 오기, JKComboBox 128 누락, JKCheckBox :22 행
  누락) 전부 역할 기준으로 잡혔다. 브리프는 drift하지만 역할(면/텍스트/선택/
  베벨/캐럿)은 drift하지 않는다 — 구값 교차 검증(kClassic 값과의 일치)이
  모호한 매핑의 판정기를 됐다(체크 상자 내부 fieldBg 근거).
- **kClassic 픽셀 회귀가 스위칭 시스템의 정합성을 두 픽셀로 닫았다** —
  "46사이트가 전부 프리셋을 따른다"는 주장을 버튼 면 192 + 타이틀바 네이비
  2개 측정으로 증명. 새 값이 아니라 옛 값으로 검증하는 것이 가장 싼 정합성
  증명이라는 스펙 직감의 실증.
- **JKDC.h 기본 인자 토큰화가 최고 레버리지의 단일 수정** — 헤더 한 번의
  변경(기본 인자 = 호출 시점 평가)으로 JKButton 베벨, JKMenu 팝업
  Rectangle3D 등 무인자 Box3D/Rectangle3D 호출 전부가 코드 0줄로 프리셋을
  따르게 됐다. "라이브러리 표면의 색"이 .cpp 밖에서도 산다는 단계 1 교훈의
  회수.
- **프로브 15/15 첫 시도 통과** — docs/45에서 probe_agent_e2e가 3회 걸렸던
  것과 대비. 자가 스폰형 프로브(잔류 프로세스 정리 후 자기 실행) 패턴의
  재현성이 확인된 세션.

## 7. 후속 과제

- **P3 핫스왑** — current() 봉합 위에 와이어 이벤트 + 시작 메뉴 설정 UI.
  멤버 기본값/ctor 캡처는 생성 시점 평가라 렌더 중 스왑 시 기존 위젯이
  구값을 유지 — 핫스왑 구현 시 재초기화/재캡처 전략 필요.
- **단계 3** — ImGui PushStyleColor 래퍼(ImGui 앱 스타일), 터미널
  terminal.json과 theme.json의 통합 재판단.
- **JKEdit 결함 대장** — 멀티라인 분기가 선택을 렌더하지 않는 기존 결함
  (선택 토큰화와 무관, 사전 존재). 사용자 보고 "JKEdit이 전반적으로 버그
  투성이"와 합쳐 JKEdit 전용 강화 태스크의 후보.
- **JKMenu 팝업 스모크 도구** — SendInput 우클릭으로 메뉴 앱을 구동하는
  1회성 프로브 (게이트 Step 8 NOT RUN의 해소).
- **probe_agent_e2e 백오프** — 4초 고정 대기의 콜드스타트 레이스 취약성
  (docs/45 §6 계승; 이번 세션은 첫 시도 통과였으나 취약성 자체는 미해결).
