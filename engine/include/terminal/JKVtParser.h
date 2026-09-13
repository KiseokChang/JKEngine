#ifndef JKVTPARSER_H
#define JKVTPARSER_H

// VT/ANSI subset parser for the ConPTY terminal (docs/22 §4).
// Pure C++ (no Windows/SDL headers) so it is unit-testable from self-test.
//
// ConPTY does NOT forward the raw application stream: conhost re-renders its
// screen buffer and emits a normalized stream of absolute cursor moves +
// changed-cell repaints. A full emulator parser is therefore unnecessary —
// this subset (cursor motion, erase, SGR, alt screen, scroll margins, OSC
// title, minimal DSR/DA replies) reproduces vim/htop/powershell output.

#include <terminal/JKTerminalGrid.h>
#include <string>
#include <utility>

namespace jk {

// Mouse reporting modes (DECSET 1000/1002/1003, docs/26 단계 3):
// Off = no reporting; Normal = button press/release only (1000);
// Button = + motion while a button is held (1002, same encoding as 1000);
// Any = every motion event (1003). 1002/1003 share the 1000 encoding —
// only the gate differs. TerminalView uses this to decide whether mouse
// input is encoded to the pty or handled locally.
enum class TermMouseMode { Off, Normal, Button, Any };

class JKVtParser {
public:
    // Replies generated while parsing (DSR/DA). Returns the bytes accumulated
    // so far and clears the buffer; the app writes these to the pty stdin.
    std::string TakeReplies() { return std::move(replies_); }

    // Feed raw pty output bytes. Applies state changes into the attached grid.
    void Feed(const uint8_t* data, size_t len);

    void Attach(JKTerminalGrid* grid) { grid_ = grid; }
    JKTerminalGrid* GetGrid() const { return grid_; }

    // Bracketed paste (DECSET 2004, docs/26 단계 5): true while the
    // application wants pastes wrapped in \x1b[200~/\x1b[201~. TerminalView
    // gates Ctrl+Shift+V on this (docs/26 단계 2, spec §2).
    bool BracketedPaste() const { return bracketedPaste_; }

    // Mouse reporting (DECSET 1000/1002/1003/1006) + application cursor keys
    // (DECSET 1), docs/26 단계 3 spec §1. Tracked only — the parser never
    // acts on them; TerminalView gates input encoding on the accessors.
    // Alt-screen (1049) entry/exit must NOT touch these (xterm standard:
    // the application enables and disables mouse reporting itself).
    TermMouseMode MouseMode() const { return mouseMode_; }
    bool SgrMouse() const { return sgrMouse_; }
    bool AppCursorKeys() const { return appCursor_; }

private:
    enum class State { Ground, Esc, Csi, Osc, Utf8 };

    void HandleGroundByte(uint8_t b);
    void HandleEscByte(uint8_t b);
    void HandleCsiByte(uint8_t b);
    void HandleOscEnd();
    void ExecuteControl(uint8_t b);       // C0 shared by Ground/ESC abort
    void DispatchCsi(uint8_t final_);
    void DispatchSgr(const int* params, int count);
    void HandleCodepoint(uint32_t cp);
    void HandlePrivateMode(const int* params, bool set);

    // Cursor motion helpers (CSI A/B/C/D/E/F/G/d/H/f), clamped by the grid.
    void CursorUp();
    void CursorDown();
    void CursorForward();
    void CursorBackward();
    void MoveTo(int row1, int col1);
    void MoveToColumn(int col1);
    void MoveToRow(int row1);

    // Editing (CSI L/M/P/@/X) — no-ops for the ConPTY repaint stream.
    void InsertLines(int n);
    void DeleteLines(int n);
    void InsertChars(int n);
    void DeleteChars(int n);
    void EraseChars(int n);

    static int ParseParams(const std::string& s, int* out, int maxCount, int defVal);

    State state_ = State::Ground;
    JKTerminalGrid* grid_ = nullptr;

    std::string csiBuf_;     // parameters + intermediate bytes
    std::string oscBuf_;
    std::string replies_;

    // UTF-8 decoder accumulator (ground state).
    uint32_t utf8Cp_ = 0;
    int      utf8Need_ = 0;

    // Saved SGR across alt-screen swap? No — xterm keeps SGR across swap.
    bool bracketedPaste_ = false;      // 2004 tracked, not acted on (Phase 2)

    // Mouse reporting + app cursor keys (docs/26 단계 3): tracked only.
    TermMouseMode mouseMode_ = TermMouseMode::Off;
    bool sgrMouse_   = false;          // 1006 — SGR mouse encoding
    bool appCursor_  = false;          // 1   — application cursor keys (SS3)
};

} // namespace jk

#endif // JKVTPARSER_H