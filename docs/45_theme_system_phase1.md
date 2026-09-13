# 45. P2 테마 시스템 단계 1 (셸) as-built

- 날짜: 2026-09-13
- 상태: 구현 완료 (코드 커밋 817835b → a657714, 게이트 GATE_GREEN)
- 선행: docs/superpowers/specs/2026-09-13-theme-system-design.md (스펙),
  docs/43 (P1 split — JKTheme.h 위치를 강제한 의존 규칙)
- 비고: 문서 번호 44는 병행 세션(Phase A TUI 흡수, docs/44)이 선점 —
  본 문서는 45로 배정.

## 1. 개요

창 크롬·런처/데스크탑 폴백색·태스크바에 흩어져 있던 색 리터럴을 공유 테마
헤더 **`include/theme/JKTheme.h` (jkcore) 하나로 중앙화했다** — 스펙 §1
접근안 A 그대로. JKTheme 구조체가 단일 진실원이 되어, 서버(JKCompositor)·
단일 프로세스(JKWindow)·셸(JKDesktopShell)·태스크바(ClientTaskbarApp)가
전부 같은 상수를 읽는다. 특히 JKWindow.cpp↔JKCompositor.cpp가 의도적으로
복제하던 크롬 값(미러)이 소멸했다 — 색 중앙화가 하나의 구조적 버그 클래스를
없앤 사례.

기본 팔레트는 Win11 모던 다크 (`#1F1F1F` 대역 표면 + `#F0F0F0` 텍스트 +
액센트 블루 `#0078D4` 예약). 프리셋/설정 파일/런타임 스위칭은 없음 —
`kDefault` 하나 (스펙 §2 YAGNI 절제).

로드맵 위치: P1 split 완료 → **P2 단계 1(셸) 완료 → 다음은 단계 2 위젯
라이브러리** (JKControl 계열 Win95 실버 → 테마 토큰). 이후 단계 3에서
ImGui 앱 스타일과 터미널 통합을 재판단.

## 2. 단계별 커밋 (4종 — BASE e39dd5e → a657714)

| 커밋 | 내용 |
|---|---|
| 817835b | feat(theme): JKTheme 공유 헤더 — Win11 모던 기본 팔레트 (include/theme/JKTheme.h, jkcore; CMake 변경 없음 — jkcore public include가 engine/include 루트) |
| 448abe2 | feat(theme): 창 크롬 → JKTheme — JKWindow 미러 리터럴 소멸 (JKCompositor.cpp + JKWindow.cpp) |
| 994d705 | feat(theme): 런처/데스크탑 폴백색 → JKTheme (JKDesktopShell.cpp) |
| a657714 | feat(theme): 태스크바 → JKTheme — 값 불변 토큰화 (ClientTaskbarApp.cpp) |

## 3. 기존→신규 대응표 (스캔 사이트 단위)

되돌릴 때의 기준 (스펙 §2-1). 값이 움직인 클러스터와 움직이지 않은
클러스터를 구분해 기록한다.

| 클러스터 | 기존 리터럴 | 토큰 | 신규 값 |
|---|---|---|---|
| 크롬(컴포지터+단일프로세스) | 버튼 면 (192,192,192) | chromeButtonFace | (38,38,38) |
| 〃 | 버튼 윤곽 (0,0,0) | chromeBorder | (68,68,68) |
| 〃 | 글리프 (255,255,255) | chromeButtonGlyph | (240,240,240) |
| 〃 | 타이틀바 네이비 (0,0,128) | chromeTitleBg | (32,32,32) |
| 〃 | 타이틀 텍스트 (255,255,255) | chromeTitleText | (240,240,240) |
| 〃 | 창 테두리 (192,192,192) | chromeBorder | (68,68,68) |
| 런처/데스크탑 | 프레임 클리어 (96,96,96) | desktopBgFallback | (28,28,30) |
| 〃 | 제너릭 폴백 (100,100,100) | launcherCellFace | (44,44,46) |
| 〃 | 마인 플레이스홀더 (128,128,128) | launcherCellPlaceholder | (80,80,84) |
| 〃 | 셀 윤곽 (255,255,255) | launcherCellOutline | (255,255,255) 불변 |
| 〃 | 테트리스 (128,0,128) | **토큰화 안 함 (의도적 잔존 — 앱 식별색)** | 불변 |
| 태스크바 | 9종 리터럴 | taskbar* 토큰 9종 | **전부 불변** (값 불변 토큰화) |
| 단일 프로세스 | 클라이언트 배경 (240,240,240) | **토큰화 안 함 (의도적 잔존 — 위젯 표면, 단계 2 영역)** | 불변 |

의도적 잔존 2건:

1. **테트리스 퍼플 (128,0,128)** — 앱 식별색은 테마 토큰이 아니라 개성.
2. **클라이언트 배경 (240,240,240)** — 위젯 표면으로 단계 2 영역. 지금
   바꾸면 Win95 실버 위젯 위에 다크 배경만 얹는 혼재가 심해진다.

예약 토큰: **chromeActiveBorder (0,120,212) `#0078D4`** — 현재 미사용,
값만 확정해 둔 상태. 소비처(활성 창 경계)는 후속 과제.

## 4. 검증

- **빌드 exit 0** + exe mtime 게이트 통과 (레슨 37).
- **`jkdesktop test` → 0 failures**.
- **프로브 15/15 PASS** (terminal_mouse 재시도 1회; probe_agent_e2e 3회 —
  콜드스타트 스폰/파이프 레이스 플레이크로 판정, 제품 회귀 아님. 4초
  고정 대기의 취약성은 후속 과제로 남김).
- **리터럴 누수 스캔 클린** — 일치 1건 = ClientTaskbarApp.cpp:41
  JKRect 지오메트리 (색 아님).
- **스크린샷**: `.superpowers/sdd/2026-09-13-theme-system-phase1/theme-phase1-after.png`
  — 서버 모드(다크 태스크바/크롬) + 단일 프로세스 minesweeper.
  **타이틀바 픽셀 측정 R32G32B32 (#202020) 정확** — 미러 소멸이 컴포지터와
  단일 프로세스 양쪽에서 동일 값을 실측 보장함을 입증 (스펙 §4 "단일
  프로세스 모드에서 미러 소멸 확인" 항목의 실측 해소).
- Win95 실버 위젯과의 시각 혼재 = 스펙 §5-1 인지된 중간 상태. 역방향이
  아니라 단계 2에서 해소될 중간값.

## 5. 실행 직감 (스펙 §6 장부에 기록)

- **값 불변 토큰화의 효용**: 태스크바 리뷰가 "값 정확성" 검사에서
  "토큰 정확성" 검사로 줄어듦 — 값이 움직이지 않으니 눈검증 대상에서
  탈락. 토큰화 자체가 리뷰 비용을 줄이는 리팩터가 된다.
- **스크린샷 픽셀 측정(#202020)이 "미러 소멸" 주장의 실증** — 색 변경
  PR에서 픽셀 측정은 가장 싼 검증. 문장("양쪽이 같다")으로는 못 믿는
  주장을 한 픽셀로 닫는다.
- **probe_agent_e2e 4초 고정 대기는 콜드스타트 레이스에 취약** —
  백오프/재시도 필요 (deferred).

## 6. 후속 과제

미해결 마이너 (단계 1 범위 유지로 남김):

- JKTheme.h EOF 개행 없음 (P1의 4종 + 1 = 5종)
- JKCompositor.cpp:345 DrawMaximizeButton 문서 주석 구식 "grey box"
- probe_agent_e2e 4초 고정 대기 → 백오프/재시도
- chromeActiveBorder (`#0078D4`) 소비처 — 활성 창 경계에 토큰 연결

다음: **단계 2 위젯 라이브러리** (JKDC 패스스루 — `JKDC::Box3D/
Rectangle3D`는 색을 인자로 받아 통과 지점이 이미 존재). 이후 **단계 3**
에서 ImGui 앱 스타일(PushStyleColor 래퍼)과 터미널 통합 재판단.
