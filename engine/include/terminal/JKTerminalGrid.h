#ifndef JKTERMINALGRID_H
#define JKTERMINALGRID_H

// Terminal screen model for the ConPTY terminal client (docs/22 §5.2).
// A pure C++ cell grid — no Windows or SDL dependencies — so the parser and
// grid can be unit-tested from the exe's self-test mode.
//
// ConPTY re-renders conhost's screen buffer as a normalized stream of cursor
// moves + changed-cell repaints, so this grid only needs: put char, cursor
// motion (clamped), erase, SGR state application, scroll-region scrolling,
// alt-screen swap and cursor save/restore.

#include <algorithm>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace jk {

// Attribute bits (SGR-derived).
enum JKTermAttr : uint8_t {
    kTermBold      = 1 << 0,
    kTermUnderline = 1 << 1,
    kTermReverse   = 1 << 2,
};

// Sentinel cell color meaning "theme default".
inline constexpr uint32_t kTermDefaultColor = 0xFFFFFFFFu;

// East Asian Width, minimal subset (docs/26 단계 1): the ranges ConPTY/conhost
// actually renders two cells wide. Hangul compatibility jamo (0x3130-0x318F)
// is intentionally narrow — conhost counts it as one cell.
inline int JKTermCharWidth(uint32_t cp) {
    if (cp >= 0x1100 && cp <= 0x115F) return 2;   // Hangul Jamo (leading)
    if (cp >= 0x2E80 && cp <= 0x303E) return 2;   // CJK radicals/symbols
    if (cp >= 0x3041 && cp <= 0x33FF) return 2;   // Kana .. CJK compat
    if (cp >= 0x3400 && cp <= 0x4DBF) return 2;   // CJK ext A
    if (cp >= 0x4E00 && cp <= 0x9FFF) return 2;   // CJK ideographs
    if (cp >= 0xAC00 && cp <= 0xD7A3) return 2;   // Hangul syllables
    if (cp >= 0xF900 && cp <= 0xFAFF) return 2;   // CJK compat ideographs
    if (cp >= 0xFE30 && cp <= 0xFE4F) return 2;   // CJK compat forms
    if (cp >= 0xFF00 && cp <= 0xFF60) return 2;   // Fullwidth forms
    if (cp >= 0xFFE0 && cp <= 0xFFE6) return 2;   // Fullwidth signs
    return 1;
}

// One grid cell. Sentinel colors (kTermDefaultColor) mean "theme default".
struct JKTermCell {
    uint32_t cp    = 0;                  // Unicode codepoint, 0 = empty cell
    uint32_t fg    = kTermDefaultColor;  // RGB (0x00RRGGBB) or sentinel
    uint32_t bg    = kTermDefaultColor;
    uint8_t  attrs = 0;                  // JKTermAttr bits
    uint8_t  width = 1;                  // reserved for CJK double-width (Phase 2)
};

class JKTerminalGrid {
public:
    struct Cursor {
        int  x = 0;          // column (0-based)
        int  y = 0;          // row (0-based)
        bool visible = true;
    };

    struct SavedCursor {
        int      x = 0;
        int      y = 0;
        uint32_t fg = kTermDefaultColor;
        uint32_t bg = kTermDefaultColor;
        uint8_t  attrs = 0;
    };

    JKTerminalGrid() = default;

    // --- geometry ----------------------------------------------------------
    void Resize(int cols, int rows);     // preserves content, top-left anchored
    int  Cols() const { return cols_; }
    int  Rows() const { return rows_; }

    // --- cursor ------------------------------------------------------------
    const Cursor& GetCursor() const { return cursor_; }
    Cursor&       MutableCursor() { return cursor_; }
    void SetCursorVisible(bool visible) { cursor_.visible = visible; MarkAllDirty(); }

    // SGR render state applied by the parser on PutChar (see JKVtParser).
    uint32_t curFg = kTermDefaultColor;
    uint32_t curBg = kTermDefaultColor;
    uint8_t  curAttrs = 0;

    // --- content -----------------------------------------------------------
    // Writes cp at the cursor and advances (with wrap at the last column).
    void PutChar(uint32_t cp);
    void LineFeed();                     // honours scroll margins
    void ReverseLineFeed();
    void CarriageReturn();
    void Backspace();
    void ForwardTab();
    void BackwardTab();

    // ED (J): 0 = cursor→end, 1 = start→cursor, 2 = all. EL (K) same per line.
    void EraseDisplay(int mode);
    void EraseLine(int mode);

    // DECSTBM (r) — 1-based inclusive rows from the CSI params.
    void SetScrollRegion(int top1, int bottom1);

    void ScrollRegionUp(int n);          // inside margins
    void ScrollRegionDown(int n);

    void SaveCursor();                   // DECSC / CSI s
    void RestoreCursor();                // DECRC / CSI u

    // 1049/1047/1048 alt-screen swap (own grid + cursor save).
    void SetAltScreen(bool on);
    bool InAltScreen() const { return altActive_; }

    // RIS (ESC c).
    void Reset();

    // --- scrollback ---------------------------------------------------------
    // Rows pushed off the top of the MAIN screen (scrollTop_ == 0 scroll) are
    // snapshotted here; margin scrolls and alt-screen scrolling do not record.
    // Lines keep the column count they had at capture time — the view clamps
    // to the current columns when painting (no reflow).
    static constexpr size_t kScrollbackMax = 1000;

    int ScrollbackLines() const { return static_cast<int>(scrollback_.size()); }
    const std::vector<JKTermCell>& ScrollbackLine(int index) const {
        return scrollback_[static_cast<size_t>(index)];
    }

    // --- accessors ---------------------------------------------------------
    const JKTermCell& Cell(int col, int row) const {
        return cells_[static_cast<size_t>(row) * cols_ + static_cast<size_t>(col)];
    }
    // Mutable accessor for parser-driven edits (ECH). Null when out of range.
    JKTermCell* CellPtrAt(int col, int row) {
        if (col < 0 || col >= cols_ || row < 0 || row >= rows_) return nullptr;
        return &cells_[static_cast<size_t>(row) * cols_ + static_cast<size_t>(col)];
    }
    const std::string& Title() const { return title_; }
    void SetTitle(const std::string& title) { title_ = title; MarkAllDirty(); }

    // --- dirty tracking ----------------------------------------------------
    bool  IsDirty() const { return dirty_; }
    bool  IsRowDirty(int row) const { return dirtyRows_[static_cast<size_t>(row)]; }
    void  MarkAllDirty() { dirty_ = true; }
    void  MarkRowDirty(int row) { dirty_ = true; dirtyRows_[static_cast<size_t>(row)] = true; }
    void  ClearDirty() { dirty_ = false; std::fill(dirtyRows_.begin(), dirtyRows_.end(), false); }

private:
    void     ClearCells(std::vector<JKTermCell>& cells);
    JKTermCell* CellPtr(int col, int row);
    void ScrollRegionUpOne();

    int cols_ = 0;
    int rows_ = 0;

    std::vector<JKTermCell> cells_;    // main screen
    std::vector<JKTermCell> altCells_; // alternate screen buffer
    bool altActive_ = false;

    std::deque<std::vector<JKTermCell>> scrollback_;

    Cursor cursor_;
    int scrollTop_ = 0;                // 0-based inclusive
    int scrollBottom_ = 0;
    SavedCursor savedCursor_;          // DECSC slot (main screen)
    SavedCursor altSavedCursor_;       // separate slot while in alt screen

    std::string title_;

    bool dirty_ = true;
    std::vector<bool> dirtyRows_;
};

} // namespace jk

#endif // JKTERMINALGRID_H