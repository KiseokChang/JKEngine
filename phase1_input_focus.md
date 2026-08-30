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
- Run `jkproto_sdl2_jkwindow` and confirm:
  - Tab cycles through edit, button, checkbox, list, combo in the demo window.
  - Drag-select in the multi-line edit works even when the mouse leaves the edit box.
  - Button pressed state is cancelled when the mouse leaves and restored when it re-enters while the button is held.
  - MessageBox/FileDialog modals block clicks on the main window and close with Escape.

## Task state
- In progress.
