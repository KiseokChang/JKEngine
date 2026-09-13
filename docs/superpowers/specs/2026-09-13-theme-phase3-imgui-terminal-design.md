# 테마 시스템 설계 (P2) — 단계 3: ImGui 래퍼 + 터미널 부분 통합 + 지정 초기자

> 2026-09-13. 단계 2(docs/46)의 연속. 사실 기반은 단계 3 인벤토리 스캔
> (2026-09-13): ImGui 앱 8종·공유 스타일 훅 전무, 터미널 themeBg/Fg 키
> 부재 판별 불가, C++17 고정 vs GNU 16.2.0.
> 사용자 결정: 로드맵 승인 ("순서대로 다") — 단계 2 스펙 §5 후속에
> 선언된 범위 그대로.

---

## 0. 인벤토리 (스캔 사실 요약)

- **ImGui 앱 8종** — jkapp_imguidemo/taskmgr/palette/notify/snap/shot/
  vplayer/browser. 전부 jkdesktop 프로세스 전용 모듈 DLL(jkwinserver는
  앱 모듈 로드 안 함). 공통 패턴: `CreateContext()` → 프레임마다
  NewFrame. **스타일 코드 전무** — 전부 ImGui 기본 다크. 앱별 임의색:
  루트 클리어 리터럴 4종(36,36,43 / 32,32,38 / 24,24,30×2), notify
  미읽 앰버(:137-140), vplayer 컨트롤 4색+TextColored 3건(알파 포함),
  snap 투명 WindowBg, browser 에러 레드.
- **터미널** — `JKTerminalConfig` themeBg=0x0C0C0C/themeFg=0xCCCCCC
  (quickjs 파싱, 키별 독립 폴백 — `getColor`는 성공시에만 기록해
  **키 부재≠기본값 판별 불가**). 시딩 2곳(ClientTerminalApp.cpp:50,
  TerminalApp.cpp:48). 소비: clear/스크롤바/IME 프리에디트/셀 기본값.
  VT 16색 팔레트(JKVtParser.cpp:11-15)는 고정 리터럴 — 테마 밖.
- **C++20** — CMake 전역 `CMAKE_CXX_STANDARD 17`(engine/CMakeLists.txt:4),
  컴파일러 GNU 16.2.0(ucrt64) — C++20 완전 지원. 전역 20 전환만으로
  지정 초기자 가능. per-target 오버라이드 전무.
- `current()` 소비처 56곳/20파일. JKTheme 집합 초기화는 JKTheme.h 안의
  3프리셋(kDefault/kLight/kClassic, 각 35필드 위치 초기화)이 전부.

## 1. 설계

### 1a. ImGui 래퍼 (헤더 온리)

- 신규 `include/theme/JKThemeImGui.h` — JKTheme.h와 동일 패턴의
  헤더 온리(CMake 변경 없음). `namespace jk::theme`:
  - `void ApplyToImGui(ImGuiStyle& style)` — 토큰→ImGuiCol 매핑:
    WindowBg/ChildBg/PopupBg→windowClientBg, FrameBg/FrameBgHovered/
    FrameBgActive→fieldBg(+밝기 변주), Button/Hovered/Active→widgetFace
    (호버/액티브는 bevelLight/bevelMid 브렌드), Text→widgetText,
    Header/Selection→selectionBg(+selectionText), Border→chromeBorder,
    CheckMark→widgetText, ScrollbarBg→scrollbarTrack, ScrollbarGrab
    →scrollbarThumb(+호버 변주), SliderGrab→selectionBg, TitleBg/
    TitleBgActive→chromeTitleBg, Separator→bevelLight 등.
  - `void ApplyImGuiTheme()` — `ApplyToImGui(GetStyle())` 편의형.
  - 매핑은 SDL_Color→ImVec4 변환 헬퍼 포함.
- **소비**: 8앱이 `CreateContext()` 직후(첫 NewFrame 전) 1회 호출.
- **루트 클리어 리터럴 통일** — 4종(36,36,43/32,32,38/24,24,30×2) →
  `current().appClearBg`. 값이 움직인다 (단계 2의 windowClientBg/appClearBg
  값 32,32,32로 수렴) — 대응표 기록.
- **의미색 잔존** (테트리스 퍼플 카테고리 — 의도 주석 유지): notify
  미읽 앰버, vplayer 비디오 오버레이 4색(알파 포함)+TextColored 3건,
  snap 투명 WindowBg, browser 에러 레드. 지오메트리(PushStyleVar
  WindowPadding/FramePadding) 불변.

### 1b. 터미널 부분 통합 (값 불변 시딩)

- 신규 토큰 2종: `terminalBg`/`terminalFg` — **kDefault는 구값
  (0x0C0C0C/0xCCCCCC) 불변** (태스크바 D8 값 불변 토큰화 패턴),
  kLight만 (0xFAFAFA/0x1F1F1F), kClassic = kDefault와 동일.
- `JKTerminalConfig`에 `bool themeBgSet/themeFgSet` 플래그 추가 —
  getColor 성공시 true. 키 있으면 사용자 지정 우선, 없으면
  `SetTheme(current().terminalBg, current().terminalFg)`.
- 시딩 2곳만 수정(클라+단일 프로세스). **VT 16색 팔레트는 미변경** —
  ANSI 색은 터미널 표준 영역이라 테마가 아님 (직감: 256색/트루컬러
  생태계와 충돌).

### 1c. C++20 전환 + 지정 초기자

- `engine/CMakeLists.txt` 전역 `CMAKE_CXX_STANDARD 17`→`20`.
- 3프리셋(kDefault/kLight/kClassic)을 위치 초기화→**지정 초기자**
  (`.field = {...},`)로 전환 — 35필드 위치 의존 위험의 근본 해소
  (docs/46 §7 후속 실행). 전체 빌드+프로브로 전역 표준 상향의
  부작용 유무 실측.

## 2. 검증

- 빌드 exit 0 + mtime 게이트 + `jkdesktop test` 0 failures + 프로브 15종.
- ImGui 앱 스크린샷 (palette 또는 taskmgr): 다크(기본)에서 팔레트 일치
  확인 + kClassic 스위치 → ImGui 앱이 클래식 팔레트로 따라오는지.
- 터미널: terminal.json 키 부재 상태에서 기본 다크 유지(0x0C0C0C 실측) +
  키 지정 시 사용자값 우선 + kClassic/kLight 스위치 추종 확인.
- 지정 초기자: 기계 검증(필드명=초기자 라벨 1:1) — 위치 의존 위험 소멸 선언.

## 3. 리스크

1. **전역 C++20 상향의 미지 부작용** — GCC 16.2는 성숙했으나 기존 코드가
   C++20에서 경고/오류로 갈 수 있음. 전체 빌드로 실측, 문제 시 per-target
   전환으로 회귀 (룰링: 우선 전역, 실패 시 축소).
2. **루트 클리어 통일의 가시 변화** — 앱별 틴트(36,36,43 등)가
   32,32,32로 수렴. 의도된 동작이나 앱 개성 상실 소지 — docs에
   기록, 이의 있으면 앱별 토큰으로 분리 가능(하지 않음 — YAGNI).
3. **터미널 키 부재 판별 플래그** — getColor 시맨틱 변경 없이 플래그만
   추가하므로 기존 terminal.json 호환 유지. 회귀 위험 낮음.

## 4. 결정/직감 장부

| # | 결정/직감 | 근거 |
|---|---|---|
| D1 | **ImGui 래퍼는 헤더 온리** (JKThemeImGui.h) | CMake 변경 0 — JKTheme.h와 같은 성공 패턴; ImGui 링크는 앱 DLL 몫 |
| D2 | **루트 클리어 → appClearBg 통일** | 앱별 임의 틴트는 테마의 적(값 분열) — 단계 2 미러 소멸과 같은 구조적 수혜 |
| D3 | **의미색(앰버/에러레드/오버레이/투명)은 잔존** | 테트리스 퍼플 카테고리 — 개성/의미≠테마 (docs/45 §3 교훈) |
| D4 | **터미널 = 부분 통합 + 구값 불변 kDefault** | terminal.json 사용자 지정 존중(키 우선) + VT 팔레트 표준성 보존; 전면 통합은 생태계와 충돌 |
| D5 | **지정 초기자는 이번에 전환** | C++20 전환의 유일한 소비처가 테마 헤더 — 35필드 위치 위험을 구조적으로 닫는 타이밍 |
| 직감 | ImGui 8앱이 "스타일 코드 전무"였다는 것 자체가 봉합 가치 — 새 기능이 아니라 부재하던 훅의 설치 | §0 |
| 직감 | terminal.json의 키 부재 판별 불가는 "폴백 설계"의 고전적 함정 — 기본값=정답인 시스템에서는 무해했지만, 시딩이 들어오는 순간 플래그가 필수가 된다 | §0 |

## 5. 후속

- P3: 핫스왑 + 설정 UI (단계 2 후속 계승)
- **JK계열↔ImGui계열의 통합 — 방향 결정 (사용자 결정 2026-09-13)**:
  "한방 통합은 아니고, **하나씩 JK계열로 흡수해가며 JK를 개선**한다."
  (① JKControl 수렴을 목표로 하되 전면 재작성이 아니라 앱 단위 점진
  흡수 — killer app 흡수(lf/helix→터미널, docs/44)와 같은 패턴).
  흡수 순서/우선순위는 P4 앱 SDK 계약에서 앱별로 판단. 단계 3의 팔레트
  봉합은 흡수 이전에도 두 계열이 같은 토큰을 읽게 하는 과도기 봉합으로
  유효.
- 앱 고유 의미색의 토큰화 재판단 — 앱이 스스로 판단 (P4 앱 템플릿)
- jkchat(Win32)은 ImGui/JKControl 어느 쪽도 아님 — 별도 범위 (P4 재료).
  **사용자 피드백 (2026-09-13): "JKChat도 너무 안이쁘긴 해"** — 흡수
  대상 1순위 후보로 기록. 권장 경로: Win32 재스타일링이 아니라
  JKControl 재작성 (IM 조합·스크롤 등 기존 위젯 + 테마 토큰 재사용).