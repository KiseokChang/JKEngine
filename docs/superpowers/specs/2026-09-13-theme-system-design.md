# 테마 시스템 설계 (P2) — 단계 1: 셸

> 2026-09-13. 디자인 개선 로드맵 P1(split, docs/43) 다음 하위 프로젝트.
> 상위 결정은 P1 스펙 §0/§8에 기록됨: Win11 모던 비주얼, 테마 도달범위
> "단계 확장(셸 먼저)". 색 하드코딩 인벤토리(2026-09-13 스캔)가 설계의
> 사실 기반.

---

## 0. 인벤토리 — 지금 색이 어디에 떠 있는가 (스캔 사실)

| 클러스터 | 위치 | 사이트 수 | 현 값 | P2 단계 |
|---|---|---|---|---|
| 창 크롬 (닫기/최대화 오버레이) | `src/server/JKCompositor.cpp:313-383` | ~3 | 실버 `(192,192,192)`/검은 윤곽/흰 글리프 — **`src/JKWindow.cpp:205-260`에 값 복제** (컴포지터가 의도적으로 미러) | **단계 1** |
| 단일 프로세스 창 크롬 (타이틀바/테두리) | `src/JKWindow.cpp:205-267` (JKDC) | ~6 | 클래식 네이비 `(0,0,128)` 등 | **단계 1** |
| 데스크탑 배경/런처 셀 폴백 | `src/desktop/JKDesktopShell.cpp:191-241` | 5 | 회색조 + 퍼플 플레이스홀더 | **단계 1** |
| 태스크바 (별도 프로세스 jkapp_taskbar.dll) | `src/apps/ClientTaskbarApp.cpp:155-210` | 9+1 절차색 | 다크 `(24,26,32)` + 면/텍스트/경계 3상태 | **단계 1** |
| 공유 위젯 라이브러리 (JKControl/JKDC 계열, Win95 팔레트) | `src/JKScrollBar.cpp`, `JKEdit.cpp`, `JKListBox.cpp`, `JKMenu.cpp` 등 10파일 | ~40 | 네이비 선택/실버 면/밝은 회색 — **변경 안 함** | 단계 2 (네이티브 앱) |
| ImGui 앱 / 터미널 / 스냅 오버레이 | ClientPaletteApp, TerminalView(+terminal.json), ClientSnapApp | 소량 | ImGui 기본 스타일, 터미널 자체 테마 | 단계 3+ (터미널 통합은 별도 판단) |

## 1. 설계 (접근안 A — 공유 테마 헤더)

**하나의 헤더가 유일한 진실원.** `include/theme/JKTheme.h` (jkcore, 의존 규칙상
서버·클라·셸 전부가 포함할 수 있는 유일한 층 — P1 split이 이 위치를 강제):

```cpp
namespace jk { namespace theme {
struct JKTheme {
    // 창 크롬 — 컴포지터(JKCompositor)와 단일 프로세스(JKWindow)가 공유
    SDL_Color chromeTitleBg, chromeTitleText, chromeBorder, chromeActiveBorder;
    SDL_Color chromeButtonFace, chromeButtonGlyph, chromeCloseHover;
    // 데스크탑/런처 (JKDesktopShell)
    SDL_Color desktopBgFallback;
    SDL_Color launcherCellFace, launcherCellOutline, launcherCellPlaceholder;
    // 태스크바 (ClientTaskbarApp) — 3상태 (normal/active/minimized)
    SDL_Color taskbarBg;
    SDL_Color taskbarFaceNormal, taskbarFaceActive, taskbarFaceMinimized;
    SDL_Color taskbarTextNormal, taskbarTextActive, taskbarTextMinimized;
    SDL_Color taskbarBorderActive, taskbarBorderInactive;
};
inline constexpr JKTheme kDefault = { /* Win11 모던 다크 + 액센트 블루 #0078D4 */ };
} }
```

소비: 3개 클러스터의 `SDL_SetRenderDrawColor`/`JKDC::SetColor` 호출이 리터럴
→ `kDefault.<토큰>` 스왑. **JKWindow.cpp↔JKCompositor.cpp의 값 복제(미러)도
소멸** — 색 중앙화가 하나의 구조적 버그 클래스를 없앤다.

## 2. 범위 결정 (YAGNI 절제)

1. **값은 바뀐다 (P2는 동작 변경 프로젝트)** — 색 값만; 지오메트리/애셔/
   로그 문자열 불변 (P1 D6 계승). 기존값→신규값 **대응표**를 as-built
   문서(docs/44)에 기록 — 되돌릴 때의 기준.
2. **프리셋/설정 파일/런타임 스위칭 없음** — `kDefault` 하나. 구조체가
   나중 소스(theme.json/와이어)를 받을 자리만 열어둔다 (P3 이후 판단).
3. **태스크바는 와이어 없이 같은 헤더를 링크** — 별도 프로세스지만
   jkapp_taskbar는 jkclient→jkcore 링크라 헤더가 도달. 런타임 동적 테마가
   필요해지는 순간 와이어 이벤트 추가로 진화 (지금 아님).
4. **터미널 테마(terminal.json themeBg/Fg)는 건드리지 않는다** — 독립
   작동 중; 통합은 위젯 단계(2)에서 재판단.
5. **태스크바 per-window 칩(FNV-1a 해시색)은 절차색 유지** — 개인화가
   목적인 색이라 테마 토큰이 아님. 팔레트 스펙만 kDefault 범위로 정렬.

## 3. Win11 모던 팔레트 방향 (정확 값은 플랜에서 확정)

- **다크 표면**: 크롬/태스크바/배경 폴백 — `#1F1F1F` 대역 (기존 태스크바
  `(24,26,32)`와 근접, 크롬은 네이비/실버에서 전환)
- **액센트**: `#0078D4` (Win11 기본 액센트 블루) — 활성 경계/강조
- **텍스트**: `#F0F0F0` 대역, 최소화 상태는 디밍
- **닫기 호버**: Win11 빨강 `(196,43,28)`
- 아크릴릭/미카 반투명은 SDL2 상단 단일 패스로 불가 → **고체 근사**
  (직감: 반투명은 컴포지터 패스 구조 변경이 필요 — P3 전환 애니메이션과
  함께 재평가)

## 4. 검증

- 빌드 그린 + `jkdesktop test` 0 failures + **프로브 15종 회귀 PASS**
  (색 변경이 파서/와이어를 건드리지 않음을 실측으로).
- 스크린샷 스모크(probe_shot 계열)로 크롬/런처/태스크바 캡처 → **사용자
  눈검증** (색의 주 고객은 눈이다).
- 단일 프로세스 모드(`minesweeper`)에서 JKWindow 크롬도 확인 — 미러
  소멸이 양쪽에서 같은 값을 보장하는지.

## 5. 리스크

1. **JKDC 위젯과의 시각 불일치** — 셸은 Win11 다크인데 네이티브 위젯은
   Win95 실버인 혼재 상태가 단계 1 결과로 나타남. 이것은 단계 확장
   결정의 알려진 중간 상태 (단계 2에서 해소) — 역방향이 아닌 중간값임을
   문서에 명시.
2. **태스크바 스폰 경로 재빌드** — jkclient 소스(앱) 수정이므로
   jkx_packages 재포장 필요 (docs/43 빌드 매트릭스, 레슨 18).
3. **토큰 누수** — 리터럴이 하나라 남으면 "테마 적용됐다"가 반거짓.
   스캔 인벤토리(§0)를 체크리스트로 사용해 사이트 단위 검증.

## 6. 결정/직감 장부

| # | 결정/직감 | 근거 |
|---|---|---|
| D1 | **접근안 A(공유 헤더)** — B(테마 파일+와이어)는 과설계, C(분산 상수)는 목적 불일치 | 런타임 스위칭 소비자 부재; P1 의존 규칙이 헤더 위치 강제 |
| D2 | **단계 1 = 셸 3클러스터만** (위젯/ImGui/터미널 제외) | 사용자 결정 "단계 확장(셸 먼저)" — 혼재 리스크는 인지된 중간 상태(§5-1) |
| D3 | **JKWindow.cpp 미러 소멸을 단계 1에 포함** | 색 중앙화의 구조적 부산물 — 복제 리터럴이 스캔에서 확인된 유일한 2-TU 이중 기록 |
| D4 | **프리셋/설정 파일/런타임 스위칭 없음** | YAGNI; 구조체가 자리만 열어둠 |
| D5 | **태스크바 와이어 없이 헤더 링크** | jkclient→jkcore 경로로 도달; 동적 테마 필요 시점에 와이어 진화 |
| D6 | **터미널 테마 독립 유지** | 이미 작동하는 themeBg/Fg — 통합은 단계 2 재판단 |
| D7 | **P2는 동작 변경 프로젝트 — 색만, 지오메트리 불변** | P1 "구조/동작 분리"의 대칭 원칙: P2는 값의 프로젝트 |
| 직감 | 인벤토리 스캔이 스펙의 절반 — 색 사이트 표가 플랜의 체크리스트가 된다 | §0 ↔ §5-3 |
| 직감 | 태스크바의 절차색(칩)은 "테마"와 "개인화"의 경계 사례 — 해시색은 개인화라 중앙화하지 않는다 | §2-5 |
| D8 | **태스크바 = 값 불변 토큰화** (리터럴→토큰 스왑만, 9종 값 전부 유지) | 태스크바 다크 팔레트가 이미 Win11 모던 대역 — 리뷰가 "값 정확성" 검사에서 "토큰 정확성" 검사로 줄어듦; 값이 움직이지 않으니 눈검증 대상에서 탈락 (docs/45 §5) |
| 직감 | 스크린샷 픽셀 측정(타이틀바 #202020 실측)이 "미러 소멸" 주장의 실증 — 색 변경 PR에서 픽셀 측정은 가장 싼 검증 | docs/45 §4; §4 "단일 프로세스 모드에서 미러 소멸 확인"의 실측 해소 |
| 직감 | probe 고정 대기는 콜드스타트 레이스에 취약 — probe_agent_e2e 4초 대기가 3회 플레이크, 백오프/재시도 필요 (deferred) | docs/45 §4/§6 |

## 7. 후속 (단계 2+ 예고)

- 단계 2: 위젯 라이브러리 Win95→테마 토큰 (JKDC 패스스루 지점 확보 —
  `JKDC::Box3D/Rectangle3D`는 색을 인자로 받아 통과 지점이 이미 존재)
- 단계 3: ImGui 앱 스타일 (PushStyleColor 래퍼), 터미널 통합 재판단
- theme.json/와이어: 런타임 스위칭 소비자(사용자 설정 UI, P3 시작 메뉴)가
  나타나는 시점에