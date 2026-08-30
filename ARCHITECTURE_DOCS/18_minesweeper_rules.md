# Minesweeper Game Rules Improvement Plan

> 지뢰찾기 앱의 게임 룰과 UX를 한국 윈도우 지뢰찾기(Minesweeper)에 가깝게 개선하는
> 단계별 계획.
>
> 문서 날짜: 2026-08-31

---

## 1. 목표

현재 `MineSweeperApp`은 고정된 9×9 초급 보드만 지원합니다. 게임 룰 측면에서
다음을 개선합니다.

- 난이도 선택: 초급 / 중급 / 고급 / 사용자 정의
- 첫 클릭은 반드시 안전(지뢰 및 주변 8칸 모두 안전)
- 타이머는 첫 클릭 시점에 시작, 게임 종료 시 정지
- 게임 종료 후 보드 잠금(추가 입력 불가)
- 난이도별 지뢰 수와 보드 크기 표준 적용

---

## 2. 난이도 정의

| 난이도 | 가로 | 세로 | 지뢰 수 | 비고 |
|--------|------|------|---------|------|
| 초급 (Beginner)   |  9 |  9 | 10 | 기본 |
| 중급 (Intermediate)| 16 | 16 | 40 | |
| 고급 (Expert)     | 30 | 16 | 99 | 가로(30) x 세로(16) |
| 사용자 (Custom)   | 가변 | 가변 | 가변 | 선택적 |

첫 단계에서는 초급/중급/고급 3단계를 구현하고, 사용자 정의는 이후 확장합니다.

---

## 3. 첫 클릭 안전 규칙

- `OpenCell(firstRow, firstCol)` 호출 시점에 지뢰를 생성.
- 선택한 칸과 주변 8칸(총 3×3)을 지뢰 배제 구역으로 설정.
- 이 구역에 지뢰가 없도록 보장.
- 결과: 첫 클릭은 항상 0 이상의 숫자이며, 주변에 지뢰가 많으면 첫 클릭이 0일
  확률이 높아짐.

---

## 4. 타이머 규칙

- `NewGame()` 호출 시 타이머는 0으로 리셋되지만 **작동하지 않음**.
- 첫 번째 셀이 열리는 순간 타이머 시작.
- 지뢰 클릭(패배) 또는 모든 안전 셀 개봉(승리) 시 타이머 정지.
- "New Game" 버튼 클릭 시 다시 0으로 리셋.

---

## 5. 게임 종료 후 입력 처리

- `gameOver_ == true`가 되면:
  - 좌클릭/우클릭/코드 모두 무시.
  - 보드는 모든 지뢰를 공개한 상태로 유지.
- 메시지 박스의 OK 버튼으로만 새 게임 시작 가능.
- 상단 "New Game" 버튼도 언제든 재시작 가능.

---

## 6. UI 변경

### 상단 툴바

```
[New Game] [Beginner] [Intermediate] [Expert]  Mines: XX  Time: XXX
```

- 난이도 버튼은 토글/라디오 형태로 현재 선택 표시.
- 보드 크기가 커지면 창 크기도 함께 조정(최소한 창이 화면에 들어오도록).

### 보드

- `MineGrid`는 `MineSweeperGame`의 `GetRows()`/`GetCols()`를 기준으로 그리드
  렌더링.
- 셀 크기는 `std::min(client.w / cols, client.h / rows)`로 계산.

---

## 7. 내부 구조 변경

### MineSweeperGame

- `kRows`/`kCols`를 상수에서 실행 시간 변수로 변경.
- `struct Difficulty { int rows; int cols; int mines; };` 추가.
- `SetDifficulty(const Difficulty&)` / `GetDifficulty()` 추가.
- 상태 배열을 `std::vector` 기반 동적 2차원 배열로 변경.
- `GetRows()`, `GetCols()`, `GetMineCount()` 상수 메서드 추가.

### MineGrid

- `MineSweeperGame`의 `GetRows()`/`GetCols()`를 사용하도록 수정.
- 셀 크기와 보드 중앙 정렬 계산을 동적 크기에 맞게 변경.

### MineSweeperApp

- 난이도 버튼 3개 추가(101=Beginner, 102=Intermediate, 103=Expert).
- 난이도 변경 시 `Init(...)` 크기 재계산 또는 `mainWindow_` 크기 변경.
- 타이머는 첫 클릭 콜백에서 시작.
- 게임 오버 시 `MineGrid`가 자동으로 잠금 상태를 인지(이미 `gameOver_` 플래그로
  처리됨).

---

## 8. 검증

1. `AppSelfTest`에 난이도별 보드 크기와 지뢰 수 검증 추가.
2. 중급/고급 보드에서 첫 클릭이 지뢰가 아님을 확인.
3. 게임 종료 후 추가 클릭이 무시됨을 확인.
4. 실제 실행: 난이도 버튼 클릭 시 보드가 바뀌고 타이머가 첫 클릭에 시작.

---

## 9. 관련 파일

- `prototype/sdl2_jkwindow/include/apps/MineSweeperApp.h`
- `prototype/sdl2_jkwindow/src/apps/MineSweeperApp.cpp`
- `prototype/sdl2_jkwindow/src/main.cpp` (self-test)
