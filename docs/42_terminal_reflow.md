# 42. 터미널 리플로우 + 스크롤 완성도

- 날짜: 2026-09-13
- 상태: 구현 완료 (커밋 e56c918 + fb7d6a9)
- 선행: docs/22 (터미널 기반), docs/26 단계 1 (전각), 단계 2 (선택/클립보드,
  docs/40), 단계 3 (마우스 보고, docs/41)

docs/26 단계 4 "리플로우 + 스크롤 완성도"의 as-built 문서. 터미널 폭이
바뀌면(창 리사이즈, 최대화) 화면+스크롤백을 논리 행으로 풀었다가 새 폭으로
다시 감아, 셀이 화면 밖으로 사라지지 않게 한다. 커서는 화면 하단 기준
위치(셸 프롬프트)를 유지하고, 넘치는 행은 스크롤백으로 밀린다.

## 1. 데이터 모델 — isWrapped 플래그

- **스크롤백 행**: `JKTermScrollLine { cells, wrapped }` (JKTerminalGrid.h).
  `wrapped=true`는 그 행이 논리적으로 다음 행에 이어진다는 뜻 — 리사이즈 때
  unwrap→rewrap의 접착 정보.
- **화면 행**: `rowWrapped_` (main) / `rowWrappedAlt_` (alt 스택) 비트
  벡터. 접근자 `RowWrapped(row)`.
- **설정 지점**: `PutChar`의 지연 랩(deferred wrap) 지점 — 커서가 마지막 열을
  넘어 (또는 전각 글리프가 follower와 함께 안 들어가) 다음 행으로 넘어가기
  직전에 현재 행의 플래그를 세운다.
- **해제 지점**:
  - **하드 LF** (`LineFeed(false)` — 파서의 LF/FF/IND/NEL, CR+LF): 떠나는 행의
    플래그를 지운다. 지연 랩 경로는 `LineFeed(true)`로 호출해 직전에 세운
    플래그를 살려둔다. CUP으로 돌아와 다시 쓰는 앱이 오염시키는 stale
    플래그의 주 원인을 이 한 지점에서 제거했다(§5).
  - **EL 2** (행 전체 지움), **ED 2** (전체), **ED 0/1** (커서 아래/위 완전
    소거 행들).
  - **스크롤 영역 이동**: 셀과 함께 플래그도 셰프트. ScrollRegionUpOne은
    스냅샷에 `RowWrapped(0)`을 싣고, 비는 바닥 행은 false.
  - **1049 스왑**: rowWrapped_ ↔ rowWrappedAlt_ 교환, 리사이즈 스태시는
    false로 채움. **RIS(Reset)**: 전체 false.

## 2. 리플로우 알고리즘 (ReflowMain)

`Resize()`는 메인 화면(alt 비활성, 기존 크기 유효)일 때만 ReflowMain;
alt 화면 중이거나 최초 크기 결정이면 LegacyResize(Phase-1 top-left 복사)로
폴백. **절대 기존 버퍼를 in-place로 고치지 않는다** — 논리 행 → 새 버퍼
조립 → 스왑.

1. **Unwrap**: 스크롤백 + 화면 행(0..endRow)을 논리 행으로 접착. `endRow`는
   커서 아래의 **후행 공백 행**을 잘라낸다(커서 아래 빈 행은 패딩). 커서와
   내용 사이의 빈 행, 커서 행 자체는 빈 논리 행으로 보존.
   - **행별 후행 공백 셀 트림**(1차 구현에서 잡은 버그): 소스 행 전체 폭
     (예: 20칸 중 "aaa")을 그대로 붙이면 논리 행이 패딩을 머고 좁은 폭에서
     내용 행 + 전부 공백인 행으로 **이중 분할**된다. appendLine에서
     `cp==0 && width!=0` 셀을 뒤에서 제거 — 전각 follower(width 0)는
     내용이므로 보존.
   - `rowLine[]/rowPrefix[]`: 각 화면 행이 속한 논리 행과 그 행 위쪽 셀 수 —
     커서 매핑용.
2. **Rewrap**: 논리 행을 새 폭으로 청킹. **전각 경계 규칙**: 청크가 폭을
   꽉 채우는데 마지막 셀이 width 2면 1칸 줄여 다음 행으로 통째로 — 글리프가
   follower와 찢어지지 않는다(docs/26 §4 "wrap 경계에서 원본만 따라감").
   마지막 청크가 아닌 produced 행은 다시 wrapped=true.
3. **화면 조립 + 커서 앵커**: `bottomOffset = rows_-1-cursor_.y`(커서의
   바닥에서 거리)를 보존한다 — 셸 프롬프트가 점프하지 않는다.
   produced 행들 중 커서 논리 행의 커서 열을 담는 행 `pCursor`를 역순
   스캔, `screenTop = clamp(pCursor - (rows-1-bottomOffset), 0,
   max(0, producedCount-rows))`. viewport 위 남는 produced 행은
   `JKTermScrollLine`으로 스크롤백 유입(scrollbackMax 트림).
   produced 부족 시 상단 패딩.
4. **커서 매핑**: `newX = cursorCol - producedMap[p].start`, `newY = pad +
   p - screenTop`. `newX == cols`는 지연 랩 상태 보존(==cols 허용).

**alt 화면 리사이즈는 의도된 Phase-1 근사**: 앱이 리사이즈 후 전면
다시 그리므로 stash는 폐기.

## 3. 뷰 — 스크롤 앵커 + 스크롤바 (TerminalView)

- **스크롤 앵커**: `lastHist_`(마지막 페인트 시 스크롤백 깊이) 대비 증분을
  페인트마다 반영 — 사용자가 위로 스크롤해 있는 동안(offset>0) 새 출력이
  밀려도 화면이 그 자리에 고정된다(`offset += hist - lastHist_`). 히스토리
  감소(리플로우) 시 clamp.
- **스크롤바**: 오른쪽 4px 오버레이 트랙(hist>0일 때만). Track =
  MixRgb(themeBg, themeFg, 14), Thumb = MixRgb(..., 64). Thumb 높이 = rows
  / (hist+rows) 지분.

## 4. 단일 프로세스 창 리사이즈 (fb7d6a9)

레거시 단일 프로세스 창(JKRenderThread)에 `SDL_WINDOW_RESIZABLE` 추가 —
한 줄. SizeChanged→SetWindowRect→OnRectChanged→RecalcCells→grid.Resize→
PTY 리사이즈 파이프라인은 교차 모니터 DPI 때문에 이미 존재했으므로
플래그 하나로 터미널 리사이즈가 열렸다. 모든 단일 프로세스 앱(minesweeper
스모크 확인)이 OS 리사이즈/최대화 가능 — 리플로우 미지원 앱은 여백만
남는다.

## 5. 디버깅 기록 — 11 failures의 실체

1차 구현 후 self-test 11개가 몰실패. 원인은 2단계:

1. **jkagentd.exe가 링크 출력을 잠그고 있었다** — 빌드가 실패한 채 옛
   exe로 테스트가 돌고 있었다(프로세스 점검 → kill → 재빌드).
2. 진짜 버그: **후행 공백 셀 트림 부재** (§2-1). "aaa"가 20칸 논리 행이
   되어 폭 10에서 내용 행 + 공백 행으로 이중 분할 → produced 6행, 화면
   배치 전체 어긋남. `[DBG]` 덤프(c01=0, c02='c')로 국지화.
3. 이후 1개 잔여 실패(`!ScrollbackLine(0).wrapped`)는 stale 플래그 —
   CUP 재출력 후 하드 LF가 행을 떠날 때 플래그를 안 지웠던 것. §1의
   하드 LF 해제로 해결(지연 랩은 softWrap 파라미터로 보호).

## 6. 검증

- **Self-test 266 PASS / 0 FAIL**: 랩 플래그 설정/해제, 제어 리플로우
  (10x3↔6x3 왕복, 히스토리 병합), 전각 경계 청킹, 커서 매핑/앵커,
  alt 스크롤 미기록.
- **엔드투엔드 (probe_terminal_reflow.ps1)**: 단일 프로세스 터미널에서
  200자 마커 라인 → 99→149열 리사이즈 → **스크린샷으로 리랩 확인**
  (build/probe_reflow_wide.png: 149+51 재배치, 프롬프트 무손상).
  클립보드 리드백은 바쁜 데스크탑에서 불안정(활성화 클릭 드랍 + 물리
  마우스 간섭) — PNG 권위. 프로브 헤더에 교훈 기록:
  - SetWindowPos 0x13의 **SWP_NOSIZE 비트가 cx/cy를 조용히 무시** —
    topmost 고정용 플래그를 리사이즈에 재사용하지 말 것(0x14 사용).
  - 합성 입력은 idle 기계에서만 신뢰된다.

## 7. 한계 (v1)

- **stale 플래그 잔여 경로**: CUP으로 행을 재출력하고 erasure 없이
  다른 행으로 점프하는 앱은 wrapped=true를 남길 수 있다(하드 LF/전체
  소거로 대부분 정화됨). 영향: 리플로우 시 과접착 — xterm도 유사 근사.
- **선택 영역은 리플로우를 따라가지 않는다**(docs/40 v1 규칙 유지) —
  리사이즈 후 셀 좌표 선택은 새 격자에서 재해석된다.
- **마우스 휠 스크롤 중 리플로우**: offset 클램프만, 앵커는 하단 기준.
- CJK 전각(단계 1)과의 상호작용은 docs/26 §4 리스크 노트 그대로 —
  전각 경계 규칙으로 청크 분할은 안전.