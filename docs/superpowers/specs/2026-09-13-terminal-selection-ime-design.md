# 터미널 텍스트 선택 + 클립보드 + IME 조합 표시

- 날짜: 2026-09-13
- 상태: 설계 확정 (사용자 위임 진행 — docs/26 단계 2 + 단계 5 IME 조합 인라인)
- 선행: docs/22 (터미널 기반), docs/26 단계 1 (전각/한글 렌더 완료), 단계 5
  (terminal.json, bracketed paste 플래그 추적만 존재 — JKVtParser.cpp:248)
- 후속 문서: 구현 완료 시 `docs/40_terminal_selection_ime.md` + docs/26 §3 단계 2/5
  상태 갱신

## 0. 범위

TerminalView가 작업대 — 클라 모드(ClientTerminalApp: 뷰 = DOCK_FILL 자식 컨트롤,
마우스 이벤트가 HitTest→RespondMessage로 도착)와 단일 모드(TerminalApp: 뷰 = 메인
윈도우, 클라이언트 영역 마우스는 JKWindow::RespondMessage가 드롭하므로
TerminalView::RespondMessage 오버라이드에서 JKWindow 위임 전에 선처리) 양쪽 모두
커버. 그리드/파서/아틀라스 변경은 bracketed paste 게이트용 accessor 추가뿐.

## 1. 텍스트 선택

- **상태**: `selAnchor_{x,y}` + `selEnd_{x,y}` (그리드 셀 좌표, -1 = 선택 없음).
  드래그 중 `selActive_` true.
- **시작**: 클라 영역 MouseDown(좌클릭) → 셀 좌표로 환산(`(ev - clientRect.origin) /
  kTermCellW/H`, 클램프) → anchor=end=셀, selActive. 기존 선택이 있으면 클릭으로
  해제(선택 해제만, anchor 재설정은 드래그 시작일 때).
- **확장**: MouseMove(버튼 다운 상태) → end 갱신 + MarkAllDirty (뷰 프레임 게이트를
  위한 grid_->MarkAllDirty()).
- **종료**: MouseUp → selActive=false, 선택 유지.
- **정규화**: anchor/end를 (x0,y0,x1,y1) min/max로 정규화 — 행 단위 박스 선택
  (리플로우 없는 모델과 일치; 문서로 명시).
- **렌더**: PaintCell 루프에서 선택 셀이면 fg/bg 스왑(reverse와 동일 시각) — 셀
  페인트 경로만 바꾸고 새 그리기 원시는 추가하지 않음.
- **더블클릭 워드 선택**: 스코프 밖 (후속).

## 2. 복사/붙여넣기

- **복사 (Ctrl+Shift+C)**: 선택 영역의 각 행을 셀 cp에서 UTF-8로 재인코딩 — 행
  끝 잘림: 마지막 non-empty 셀까지만, 빈 행은 건너뛰지 않고 "\n"으로 연결
  (Windows 터미널 관례). 선택 없으면 무동작. `SDL_SetClipboardText`
  (JKEdit.cpp:716 선례 — 클라 프로세스도 SDL 링크 상태라 사용 가능).
- **붙여넣기 (Ctrl+Shift+V)**: `SDL_GetClipboardText` → 개행 정리("\r\n"/"\r"→"\n") →
  **bracketed paste 게이트**: `parser_->BracketedPaste()` true면
  `\x1b[200~` + 텍스트(`\x1b` 문자는 제거) + `\x1b[201~`로 감싸 onInput_ —
  JKVtParser가 이미 2004 플래그 추적 중, accessor 1개 추가.
- 키 처리 위치: TerminalView::HandleKeyDown — ctrl && shift && 'C'/'V' (기존
  ctrl+문자 경로보다 우선). KeyDown/Char/붙여넣기 시 선택 해제 + preEdit 클리어.

## 3. IME 조합 인라인 표시

- **이벤트**: `JKEventType::TextEditing` — 파이프라인 이미 존재(서버 JKWindowServer
  :1252 → 클라 JKClientSurface:245, 단일 모드 main.cpp:1002). TerminalView가
  지금은 버림 → RespondMessage에서 소비.
- **상태**: `preEdit_` (UTF-8 문자열) + 표시 셀 좌표 = 현재 그리드 커서 위치.
- **렌더**: 커서 셀 위치부터 preEdit_를 셀 단위로 오버레이 — 배경을 약간 밝은
  톤(themeFg 저휘도)으로 칠하고 글리프를 그 위에 그림 (JKEdit의 조합 표시 선례,
  JKEdit.cpp:417). 그리드 폭을 넘으면 잘라냄. 실제 커밋(SDL_TEXTINPUT→Char)이 오면
  preEdit_ 클리어 — 클리어 전 프레임에 조합 글자와 커밋 글자가 겹쳐 보일 수 있으나
  ConPTY가 커밋 후 화면을 다시 그리므로 1프레임 허용.
- **클리어 시점**: Char/KeyDown(아무 키)/붙여넣기/포커스 상실. 셸 출력이 커서를
  움직여도 오버레이는 커서를 따라감 (커밋 전까지).
- 멀티바이트 preEdit_는 UTF-8이므로 기존 UTF-8 디코딩 유틸(JKVtParser 내부) 재사용 —
  코드포인트 배열로 만들어 셀 하나당 한 글리프(전각이면 2셀), 폭 판별은 기존
  `JKTermCharWidth`.

## 4. 테스트

- **self-test (단위)**: 선택 정규화+행 추출 로직을 순수 함수로 분리해
  `jkdesktop.exe test`에 추가 (그리드 더미 셀 + UTF-8 재인코딩 + 개행 정리 +
  bracketed 래핑 게이트).
- **프로브** `probe_terminal_select.ps1`: 단일 모드 터미널
  (`jkdesktop.exe terminal`) 기동 → SendKeys로 `echo hello` 입력+엔드 →
  SendInput 마우스 드래그로 선택 → Ctrl+Shift+C 복사 → 클립보드에서 읽어 "hello"
  검증 → Ctrl+Shift+V 붙여넣기로 재실행 검증(또는 클립보드→pty 입력 로그).
  서버 창 캡처는 보너스.

## 5. 제한

- 박스 선택(행 단위) — 블록 선택은 행 단위만; 워드 더블클릭/쉬프트+클릭 확장 후속.
- 스크롤백 영역 선택은 v1에서 라이브 화면만 (스크롤 중 드래그는 후속).
- IME 오버레이는 커밋 전 1프레임 겹침 허용 (§3).
- 클라 모드에서 SDL 클립보드는 OS 클립보드 — 서버를 경유하지 않음 (문서 명시).