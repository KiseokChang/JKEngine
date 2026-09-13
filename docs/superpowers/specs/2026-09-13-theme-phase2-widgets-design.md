# 테마 시스템 설계 (P2) — 단계 2: 위젯 라이브러리 + 스위칭 봉합

> 2026-09-13. 단계 1(docs/45)의 연속. 사실 기반은 위젯 사이트 전수
> 인벤토리(2026-09-13 스캔, 46사이트/11파일+JKDC 헤더 기본 인자).
> 사용자 결정: 테마 변경 기능은 **current() 봉합 + 프리셋** 수준으로
> 단계 2에 포함 (핫스왑·설정 UI는 P3).

---

## 0. 인벤토리 (스캔 사실 요약)

- 페인트/기본값 색 사이트 **46곳** — JKScrollBar/JKControl(포커스 링)/
  JKEdit(IME 조합·캐럿 포함)/JKListBox/JKMenu(팝업 텍스트 리터럴 우회
  1건)/JKComboBox/JKCheckBox/JKStatic/JKButton/JKMessageBox +
  **include/JKControl.h:169-174 멤버 기본값** + **include/JKDC.h
  Box3D/Rectangle3D 기본 인자 3종**(face 192/light 255/dark 0 — JKDC
  내부 하드코딩 0건) + 표면 3건(JKWindow.cpp:270 클라 배경,
  JKClientApplication.cpp:491 앱 클리어, AppLauncherItem.cpp:24-25
  재지정).
- 특수 토큰: IME 조합 배경은 **알파 64 보존 필수**, IME 조합 캐럿/에러
  (255,0,0), 스크롤바 트랙 (220), 썸 그림자 (128) — Win95 삼색 밖.
- 누수 확인: 터미널(TerminalView/JKTerminalGrid)·ImGui 앱은 위젯
  라이브러리 미사용 — 다크 전환 영향 0.

## 1. 설계

### 1a. 위젯 다크 팔레트 (kDefault 확장)

| 역할 | 구값 | 신값 | 토큰 |
|---|---|---|---|
| 버튼/스크롤바/메뉴/콤보버튼 면 | (192,192,192) | (43,43,43) | widgetFace |
| 스태틱/체크박스 기본 면 | (240,240,240) | (43,43,43) | widgetFace (동일 토큰) |
| 창 클라 배경 (JKWindow:270) | (240,240,240) | (32,32,32) | windowClientBg |
| 앱 표면 클리어 (JKClientApplication:491) | (192,192,192) | (32,32,32) | appClearBg |
| 입력 필드 배경 (에디트/리스트/콤보) | (255,255,255) | (26,26,26) | fieldBg |
| 텍스트 전반 (에디트/리스트/메뉴/버튼 라벨/글리프) | (0,0,0) | (240,240,240) | widgetText |
| 선택/하이라이트 배경 | (0,0,128) | (0,120,212) | selectionBg (액센트 #0078D4 — chromeActiveBorder와 동일값) |
| 선택/하이라이트 텍스트 | (255,255,255) | (255,255,255) | selectionText (불변) |
| 3D 가장자리 light | (255,255,255) | (70,70,70) | bevelLight |
| 3D 가장자리 dark | (0,0,0) | (16,16,16) | bevelDark |
| 썸/콤보버튼 그림자 | (128,128,128) | (40,40,40) | bevelMid |
| 스크롤바 트랙 | (220,220,220) | (56,56,56) | scrollbarTrack |
| 스크롤바 썸 면 | (255,255,255) | (86,86,86) | scrollbarThumb |
| 포커스 링 (JKControl:374) | (0,0,255) | (0,120,212) | focusRing |
| 읽기전용 필드 배경 (JKEdit:117) | (240,240,240) | (43,43,43) | widgetFace (동일) |
| IME 조합 배경 | (0,0,255,**64**) | (0,120,212,**64**) | imeCompositionBg (**알파 보존**) |
| IME 조합 캐럿/에러 | (255,0,0) | (255,0,0) | imeCaret (**값 유지 토큰**) |
| 캐럿 (에디트) | (0,0,0) | (240,240,240) | widgetText (동일 토큰) |
| 체크 마크/콤보 화살표 글리프 | (0,0,0) | (240,240,240) | widgetText |

지오메트리/베벨 깊이/드로우 순서 불변 (단계 1 D7 계승).

### 1b. 스위칭 봉합 (current() + 프리셋)

- `jk::theme`에 추가:
  - 프리셋 3종 — `kDefault`(Win11 다크), **`kLight`**(Win11 라이트 신규
    값표), **`kClassic`**(단계 1 이전 값 전부 — 픽셀 회귀 도구).
  - `inline const JKTheme* activeTheme_ = &kDefault;` +
    `inline JKTheme const& current()` + `inline void setTheme(const JKTheme*)`.
  - `bool loadPreset(const std::string& path)` — theme.json의
    `{"preset": "dark"|"light"|"classic"}` 1키만 해석. 임의 색 정의는
    파싱하지 않는다 (YAGNI — 프리셋 조합이 임의 색의 99%).
- **모든 그리기 사이트는 `kDefault` 직접 참조 대신 `current()` 사용** —
  단계 1의 4 TU 소비처도 이번에 전환 (46+4사이트 전부). 이것이 봉합의
  본체. 이후 프리셋 추가는 constexpr 데이터 1블록이 된다.
- 로딩 시점: 각 프로세스 기동 직후 1회 (서버는 Init, 클라는
  JKClientApplication init — terminal.json 패턴 준용). 파일 없으면
  기본값 (다크) — 실패가 조용한 것은 기본값이 곧 정답이라.

### 1c. 범위 경계

1. **엔진 소유 기본값만** — AppLauncherItem 재지정 포함.
   ClientTestWindowApp/EquipApp의 콘텐츠 색(노랑/BROWN)은 제외.
2. 지오메트리/알고리즘 불변 — 색 인자만.
3. 핫스왑/와이어 이벤트/설정 UI 없음 — P3 시작 메뉴 때.
4. JKMenu.cpp:160 팝업 텍스트 리터럴 — 토큰 파이프라인으로 교정
   (유일한 페인트 내 우회).

## 2. 검증

- 빌드 + `jkdesktop test` 0 failures + 프로브 15종.
- **kClassic 픽셀 회귀**: theme.json에 "classic" → 단일 프로세스
  minesweeper 스크린샷이 단계 1 이전 값과 일치(픽셀 측정: 버튼 면
  192,192,192 확인) — 프리셋 시스템의 정합성을 기존 진실값으로 닫는다.
- **kDefault/ kLight 스크린샷** — 위젯이 실제 보이는 화면 (버튼 면
  #2B2B2B 픽셀 측정 + 눈검증).
- JKMenu 팝업 열림 스모크 (팝업 텍스트 토큰 확인).

## 3. 리스크

1. **앱 콘텐츠 색과의 혼재** — EquipApp BROWN 등 미스왑 사이트가 다크
   위젯 위에서 튄다. 인지된 상태 — 앱 콘텐츠는 각자 판단 (P4 앱 템플릿).
2. **current() 누수** — kDefault 직접 참조가 하나라 남으면 그 사이트만
   프리셋 안 따라감. grep 게이트(`kDefault` 참조는 헤더 내부만)로 검증.
3. **kClassic 값 보존** — 구값을 정확히 복사했는지가 회귀 도구의 신뢰.
   단계 1 대응표(docs/45 §3)를 원본으로 사용.

## 4. 결정/직감 장부

| # | 결정/직감 | 근거 |
|---|---|---|
| D1 | **스위칭은 current() 봉합 + 프리셋** (사용자 결정 2026-09-13) | 핫스왑/설정 UI는 P3; 46사이트를 지금 건드리므로 current() 전환의 한계비용이 현재 최소 |
| D2 | **프리셋 3종 — kLight + kClassic 포함** | kClassic은 구값 그대로라 픽셀 회귀 검증 도구를 겸함 — 스위칭 시스템의 정합성을 기존 진실값으로 닫는 직감 |
| D3 | **theme.json은 프리셋 선택 1키만** | 임의 색 파싱은 프리셋 존재 하에 99% 대체 불가 이득(YAGNI); 확장은 나중 |
| D4 | **위젯 선택색 = 액센트 통일** | 네이비(0,0,128)→#0078D4 — chromeActiveBorder 재료가 드디어 소비되며 예약 토큰 2종 문제 해소 |
| D5 | **IME 알파 보존/에러 색 값 유지** | 동작(가독성)이 색 자체에 의존하는 특수 토큰 — 테마가 아니라 UX 상수 |
| 직감 | 인벤토리가 JKDC 헤더 기본 인자까지 잡아냄 — "라이브러리 표면의 색"은 .cpp 밖에도 산다 (단계 1의 미러 사례와 동류) | §0 |
| 직감 | kClassic = 시간을 거슬러 가는 스크린샷. 새 테마 값이 아니라 옛 값으로 검증하는 것이 가장 싼 정합성 증명 | §2 kClassic 픽셀 회귀 |
| 직감 | **역할 우선 매핑이 라인번호 표를 이겼다** — 브리프 대응표 3건 오류 발견 (JKScrollBar :13,14는 트랙/썸이 아니라 ctor back/text; JKComboBox 드롭버튼 세번째 색 128 미기재; JKCheckBox :22 Box3D 행 누락). 브리프는 drift하지만 역할은 drift하지 않는다 — 모호한 매핑은 구값(kClassic)과의 일치로 판정 | task-2/3 보고서, docs/46 §3 |
| 직감 | **kClassic 픽셀 회귀가 스위칭 시스템의 가장 싼 정합성 증명** — 버튼 면 (192,192,192) + 타이틀바 네이비 (0,0,128) 2곳의 픽셀 측정으로 게이트 전체가 닫힘 (실측: task-5 §6) | §2 |
| NOTE | **JKMenu :160은 SDL 우회가 아니었다** — 실제는 `dc.SetTextColor`의 JKDC 파이프라인 경유. §1c-4는 결과(토큰화)로 충족됐고 "우회" 서술만 교정 (주석 전용, 커밋 928fd3f) | task-3 §6/fix round 1 |
| 직감 | **JKDC.h 기본 인자 토큰화가 최고 레버리지의 단일 수정** — 기본 인자는 호출 시점 평가라, 헤더 1회 변경으로 모든 무인자 Box3D/Rectangle3D 호출(JKButton 베벨, JKMenu 팝업 Rectangle3D 등)이 코드 0줄로 프리셋을 따르게 됐다 | task-2 §1, docs/46 §6 |

## 5. 후속

- P3: 핫스왑(와이어 이벤트) + 시작 메뉴 설정 UI — current() 봉합 위에 얹음
- 단계 3: ImGui PushStyleColor 래퍼, 터미널 terminal.json 통합 재판단
- 앱 콘텐츠 색(BROWN 등) — 각 앱이 자기 판단으로 토큰 채택 (P4 앱 템플릿 재료)
