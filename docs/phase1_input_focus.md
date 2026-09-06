# Phase 1: Input / Focus System (SDL2 prototype)

## Goal
Strengthen the low-level input layer so controls behave predictably across mouse capture, focus, and keyboard navigation.

## Scope
1. **Mouse capture**
   - Restore `JKApplication::captureControl_` so mouse-move/up events continue to be delivered to the control that started an action even when the pointer leaves its bounds.
   - Use capture in `JKButton` (pressed state tracking across leave/enter) and `JKEdit` (drag selection outside the edit box).
   - Keep window move/resize robust when the cursor leaves the window frame.

2. **Focusable controls & Tab navigation**
   - Add `JKControl::IsFocusable()` / `SetFocusable()`.
   - Mark `JKButton`, `JKCheckBox`, `JKEdit`, `JKListBox`, and `JKComboBox` as focusable.
   - Implement `JKWindow::FocusNextChild()` / `FocusPrevChild()`.
   - Route `Tab` / `Shift+Tab` to cycle focus inside the active (modal) window.
   - Draw a simple focus indicator on the currently focused control.

3. **Keyboard activation**
   - `Space` / `Enter` triggers the focused `JKButton` / `JKCheckBox`.
   - `JKMessageBox` treats `Enter` as the default button and `Escape` as cancel.

4. **Modal input isolation**
   - When a modal window is open, mouse hit-testing and Tab navigation stay inside the modal window.
   - Closing the modal restores focus to the previous window.

## Verification
- Build `prototype/sdl2_jkwindow`.
- Run automated probes:
  - `tools\verify_tab_navigation.ps1` — Tab/Shift+Tab cycles through focusable controls.
  - `tools\verify_dialog_keyboard.ps1` — Enter confirms `JKMessageBox`, Escape cancels `JKFileDialog`, focus restores to the previous control.
- Manual checks:
  - Drag-select in the multi-line edit works even when the mouse leaves the edit box.
  - Button pressed state is cancelled when the mouse leaves and restored when it re-enters while the button is held.
  - MessageBox/FileDialog modals block clicks on the main window.

## 5. IME 한글 입력 (Windows IME 우선, F2 내부 오토마타 폴백)

`JKEdit`에 OS IME를 지원하는 작업도 Phase 1 후반에 함께 완료했다. 자세한 설계는
`ARCHITECTURE_DOCS/16_sdl2_jkwindow_ime.md`를 참고한다.

- `JKEventType::TextEditing` 추가 — SDL `SDL_TEXTEDITING`(조합 중 문자열) 라우팅
- `JKHangulUtil`로 UTF-8 → CP949 → KSSM 2바이트 변환 공용화
- `JKEdit` 조합 상태(`compText_`, `compCursor_`, `imeComposing_`)와 시각적 표시
- `JKPlatformIme` PAL로 Win32 IMM32 API 격리
- 조합 중 백스페이스/딜리트/방향키/엔터를 OS IME에 양보
- `F2`로 내부 오토마타 전환 시 OS IME를 ASCII로 강제 전환
- 포커스 아웃/마우스 클릭 시 `CompleteComposition`으로 조합 문자 강제 확정

## Task state
- Completed on `phase2-full-stack` branch (2026-08-30).
