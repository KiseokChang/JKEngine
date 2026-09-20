# docs/61 — JKEdit 결함 픽스 (2026-09-20)

사용자 보고 "JKEdit이 전반적으로 버그 투성이"(docs/46 §7 결함 대장)의 수술.
근본 원인은 두 갈래였다: **KSSM 글리프 폭 무시**(한글 사용자에게 전면 오작동)와
**버퍼(KSSM)↔외부(UTF-8) 경계 무변환**. 개별 증상은 이 둘의 파생이었다.

## 1. 결함 목록과 픽스

| # | 결함 | 원인 | 픽스 |
|---|------|------|------|
| 1 | 멀티라인 선택 미렌더 (대장 기존 결함) | multiline 페인트 분기에 선택 렌더 부재 | 라인×선택 교집합을 3분할 색상(일반/선택 배경+선택 텍스트/일반)으로 렌더 |
| 2 | 한글에서 캐럿·선택·클릭이 밀림 | 좌표 = `바이트×charWidth_(8px)` — JKDC 비트맵 폰트는 ASCII 8px / KSSM 2바이트 16px 전진 | 표시 셀 매핑 신설: `DisplayCells`(범위→8px 셀 수, KSSM 쌍=2셀) + `PosFromCells`(역매핑, 쌍은 통째 전진→반환값이 항상 글리프 경계) |
| 3 | 한글 복사→클립보드 오염 | 버퍼(KSSM) 원바이트를 클립보드에 | 복사 = `KssmToUtf8` 경유 |
| 4 | 한글 붙여넣기 오염 | 클립보드(UTF-8) 원바이트를 KSSM 버퍼에 | 붙여넣기 = `Utf8ToKssm` 경유 |
| 5 | 한글 타이핑이 선택을 살려둠 | `InsertKssmText`/`ProcessHangulKey`에 `DeleteSelection` 누락(ASCII `InsertText`만 있었음) | 양 경로 + `InsertKssmText` 진입점에 `DeleteSelection()` |
| 6 | 한 줄 편집 가로 스크롤 부재 — 길이 초과 시 캐럿 소실 | 수직 스크롤(`firstVisibleLine_`)만 존재 | `firstVisibleCol_`(표시 셀 단위) 신설, `ScrollToCursor`가 캐럿 셀을 밴드로 유지, 페인트·`PixelToPos` 양쪽에 오프셋 반영 |
| 7 | 사소: `DeleteForward`/`SetSelection` 미 스크롤 | — | `ScrollToCursor()` 추가 |

**설계 결정 — 셀 단위 좌표**: KSSM 1글자 = 16px = 2셀(8px 단위)로 보고
캐럿 x = `inner.x + 셀수×8`. 클릭 역매핑은 KSSM 쌍을 통째로 건너뛰므로
쌍 중간 클릭이 자연히 글자 경계에 스냅된다(왼쪽 반 클릭도 글자 뒤에 놓임 —
단순화된 동작, 문서화). `PixelToPos`는 히트 테스트 API로서 private→public 이동.

**`SetText("")` 후 붙여넣기 함정**: 붙여넣기는 커서 위치의 *삽입*이다 — 기존
내용이 남은 버퍼에 ctrl+v하면 2배 길이가 정답이다. 첫 단위 프로브가 이 기대치를
틀려 FAIL을 냈다(코드 결함 아님 — 테스트 정정).

## 2. 검증 (2연속 원칙)

- **`tools/probes/jkedit_probe.cpp` 신설** — JKEdit를 이벤트 시뮬레이션으로
  직접 구동(SDL_SetModState 모디파이어 주입, JKEvent Char/KeyDown). 9체크 ×2
  ALL PASS: KSSM 쌍 원자 클릭 스냅 3건 / 수평 스크롤 / 클립보드 UTF-8 복사 /
  KSSM 붙여넣기 왕복 / 한글 타이핑 선택 대체(오토마타 경로) / Char 경로 선택
  대체(InsertKssmText) / Ctrl+A.
- **probe_workshop ×2 ALL PASS**(createEdit 위젯 경유 회귀, jkclient 경로).
- 전체 ninja 재빌드(ld exit-67 플레이크 + jkagentd 링크 락 + cc1plus OOM —
  `-j3`로 회피, 전부 기존 레슨).

**눈확인 대기(사용자)**: 터미널/워크숍 createEdit에서 ①한글 캐럿 위치 ②멀티라인
선택 강조 ③한글 복붙 ④긴 한 줄 스크롤.

## 3. 레슨

1. **버그 대장의 "전반적 버그 투성이"는 원인 2개로 수렴했다** — 좌표계(셀 매핑)
   과 문자셋 경계(변환). 증상 목록을 원인으로 압축하면 픽스도 테스트도 좁아진다.
2. **프로브 기대치 자체를 검증하라** — 삽입 vs 치환, 전체 선택 범위 같은
   의미론 오해가 FAIL의 반이었다. FAIL 재현 불명 시 먼저 기대치를 의심.
3. JKEvent.text는 char[64] 배열 — 대입 불가, snprintf로.