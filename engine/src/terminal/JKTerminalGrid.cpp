#include <terminal/JKTerminalGrid.h>
#include <algorithm>

namespace jk {

void JKTerminalGrid::ClearCells(std::vector<JKTermCell>& cells) {
    for (auto& c : cells) {
        c.cp = 0;
        c.fg = kTermDefaultColor;
        c.bg = kTermDefaultColor;
        c.attrs = 0;
        c.width = 1;
    }
}

JKTermCell* JKTerminalGrid::CellPtr(int col, int row) {
    if (col < 0 || col >= cols_ || row < 0 || row >= rows_) return nullptr;
    return &cells_[static_cast<size_t>(row) * cols_ + static_cast<size_t>(col)];
}

void JKTerminalGrid::Resize(int cols, int rows) {
    cols = std::max(1, cols);
    rows = std::max(1, rows);
    if (cols == cols_ && rows == rows_) return;

    std::vector<JKTermCell> next(static_cast<size_t>(cols) * rows);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            JKTermCell& dst = next[static_cast<size_t>(r) * cols + c];
            if (c < cols_ && r < rows_) {
                dst = cells_[static_cast<size_t>(r) * cols_ + c];
            }
        }
    }
    cells_ = std::move(next);
    // Alt buffer rebuilt to the same geometry; apps re-render after resize so
    // discarding its content is the documented Phase-1 approximation.
    altCells_.assign(static_cast<size_t>(cols) * rows, JKTermCell{});

    cols_ = cols;
    rows_ = rows;
    cursor_.x = std::min(cursor_.x, cols_ - 1);
    cursor_.y = std::min(cursor_.y, rows_ - 1);
    scrollTop_ = 0;
    scrollBottom_ = rows_ - 1;
    dirtyRows_.assign(static_cast<size_t>(rows_), false);
    MarkAllDirty();
}

void JKTerminalGrid::PutChar(uint32_t cp) {
    // Wide glyphs (docs/26 단계 1) occupy their cell plus a follower cell of
    // width 0 (bg-only dummy), keeping col index == pixel col mapping intact.
    const int w = JKTermCharWidth(cp);
    // Deferred wrap: writing past the last column — or a wide glyph that
    // cannot fit with its follower — first starts a new line.
    if (cursor_.x >= cols_ || cursor_.x + w > cols_) {
        cursor_.x = 0;
        LineFeed();
    }
    if (auto* cell = CellPtr(cursor_.x, cursor_.y)) {
        *cell = JKTermCell{ cp, curFg, curBg, curAttrs, static_cast<uint8_t>(w) };
        MarkRowDirty(cursor_.y);
    }
    if (w == 2) {
        if (auto* follower = CellPtr(cursor_.x + 1, cursor_.y)) {
            *follower = JKTermCell{ 0, curFg, curBg, curAttrs, 0 };
        }
        cursor_.x += 2;
    } else {
        ++cursor_.x;
    }
}

void JKTerminalGrid::ScrollRegionUpOne() {
    if (scrollTop_ < 0 || scrollBottom_ >= rows_ || scrollTop_ > scrollBottom_) return;
    // Record the row leaving through the top for scrollback — but only on the
    // main screen and only when the region starts at row 0 (xterm semantics:
    // margin scrolls and alt-screen scrolls are not recorded).
    if (!altActive_ && scrollTop_ == 0) {
        scrollback_.emplace_back(cells_.begin(),
                                 cells_.begin() + static_cast<ptrdiff_t>(cols_));
        if (scrollback_.size() > kScrollbackMax) {
            scrollback_.pop_front();
        }
    }
    for (int r = scrollTop_; r < scrollBottom_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            cells_[static_cast<size_t>(r) * cols_ + c] =
                cells_[static_cast<size_t>(r + 1) * cols_ + c];
        }
    }
    for (int c = 0; c < cols_; ++c) {
        cells_[static_cast<size_t>(scrollBottom_) * cols_ + c] = JKTermCell{};
    }
    for (int r = scrollTop_; r <= scrollBottom_; ++r) MarkRowDirty(r);
}

void JKTerminalGrid::LineFeed() {
    MarkRowDirty(cursor_.y);
    if (cursor_.y == scrollBottom_) {
        ScrollRegionUpOne();
    } else if (cursor_.y < rows_ - 1) {
        ++cursor_.y;
    }
    MarkRowDirty(cursor_.y);
}

void JKTerminalGrid::ReverseLineFeed() {
    MarkRowDirty(cursor_.y);
    if (cursor_.y == scrollTop_) {
        if (scrollTop_ < scrollBottom_ && scrollBottom_ < rows_) {
            for (int r = scrollBottom_; r > scrollTop_; --r) {
                for (int c = 0; c < cols_; ++c) {
                    cells_[static_cast<size_t>(r) * cols_ + c] =
                        cells_[static_cast<size_t>(r - 1) * cols_ + c];
                }
            }
            for (int c = 0; c < cols_; ++c) {
                cells_[static_cast<size_t>(scrollTop_) * cols_ + c] = JKTermCell{};
            }
            for (int r = scrollTop_; r <= scrollBottom_; ++r) MarkRowDirty(r);
        }
    } else if (cursor_.y > 0) {
        --cursor_.y;
    }
    MarkRowDirty(cursor_.y);
}

void JKTerminalGrid::CarriageReturn() {
    cursor_.x = 0;
    MarkRowDirty(cursor_.y);
}

void JKTerminalGrid::Backspace() {
    if (cursor_.x > 0) {
        --cursor_.x;
        // Land on the real cell of a wide glyph, not its width-0 follower.
        while (cursor_.x > 0 && Cell(cursor_.x, cursor_.y).width == 0) {
            --cursor_.x;
        }
    }
    MarkRowDirty(cursor_.y);
}

void JKTerminalGrid::ForwardTab() {
    // Next 8-column tab stop, clamped (no erase) — conhost re-renders content.
    int next = ((cursor_.x / 8) + 1) * 8;
    cursor_.x = std::min(next, cols_ - 1);
    MarkRowDirty(cursor_.y);
}

void JKTerminalGrid::BackwardTab() {
    int prev = (std::max(0, cursor_.x - 1) / 8) * 8;
    cursor_.x = prev;
    MarkRowDirty(cursor_.y);
}

void JKTerminalGrid::EraseDisplay(int mode) {
    if (mode == 2) {
        ClearCells(cells_);
        cursor_.x = 0;
        cursor_.y = 0;
        scrollTop_ = 0;
        scrollBottom_ = rows_ - 1;
        MarkAllDirty();
        return;
    }
    const size_t first = (mode == 0)
        ? static_cast<size_t>(cursor_.y) * cols_ + cursor_.x
        : 0;
    const size_t last = (mode == 0)
        ? cells_.size()
        : static_cast<size_t>(cursor_.y) * cols_ + cursor_.x;
    for (size_t i = first; i <= last && i < cells_.size(); ++i) {
        auto& c = cells_[i];
        c.cp = 0;
        c.fg = kTermDefaultColor;
        c.bg = kTermDefaultColor;
        c.attrs = 0;
        c.width = 1;
    }
    MarkAllDirty();
}

void JKTerminalGrid::EraseLine(int mode) {
    if (cursor_.y < 0 || cursor_.y >= rows_) return;
    auto rowBeg = cells_.begin() + static_cast<size_t>(cursor_.y) * cols_;
    auto rowEnd = rowBeg + cols_;
    auto resetCell = [](JKTermCell& c) {
        c.cp = 0;
        c.fg = kTermDefaultColor;
        c.bg = kTermDefaultColor;
        c.attrs = 0;
        c.width = 1;
    };
    if (mode == 0) {
        std::for_each(rowBeg + std::min(cursor_.x, cols_), rowEnd, resetCell);
    } else if (mode == 1) {
        std::for_each(rowBeg, rowBeg + std::min(cursor_.x + 1, cols_), resetCell);
    } else {
        std::for_each(rowBeg, rowEnd, resetCell);
    }
    MarkRowDirty(cursor_.y);
}

void JKTerminalGrid::SetScrollRegion(int top1, int bottom1) {
    const int top = std::max(0, top1 - 1);
    const int bottom = std::min(rows_ - 1, bottom1 - 1);
    if (top < bottom) {
        scrollTop_ = top;
        scrollBottom_ = bottom;
    }
}

void JKTerminalGrid::ScrollRegionUp(int n) {
    for (int i = 0; i < n; ++i) ScrollRegionUpOne();
}

void JKTerminalGrid::ScrollRegionDown(int n) {
    if (scrollTop_ >= scrollBottom_) return;
    for (int i = 0; i < n; ++i) {
        for (int r = scrollBottom_; r > scrollTop_; --r) {
            for (int c = 0; c < cols_; ++c) {
                cells_[static_cast<size_t>(r) * cols_ + c] =
                    cells_[static_cast<size_t>(r - 1) * cols_ + c];
            }
        }
        for (int c = 0; c < cols_; ++c) {
            cells_[static_cast<size_t>(scrollTop_) * cols_ + c] = JKTermCell{};
        }
    }
    for (int r = scrollTop_; r <= scrollBottom_; ++r) MarkRowDirty(r);
}

void JKTerminalGrid::SaveCursor() {
    SavedCursor& slot = altActive_ ? altSavedCursor_ : savedCursor_;
    slot.x = cursor_.x;
    slot.y = cursor_.y;
    slot.fg = curFg;
    slot.bg = curBg;
    slot.attrs = curAttrs;
}

void JKTerminalGrid::RestoreCursor() {
    SavedCursor& slot = altActive_ ? altSavedCursor_ : savedCursor_;
    cursor_.x = std::min(slot.x, std::max(0, cols_ - 1));
    cursor_.y = std::min(slot.y, std::max(0, rows_ - 1));
    curFg = slot.fg;
    curBg = slot.bg;
    curAttrs = slot.attrs;
    MarkRowDirty(cursor_.y);
}

void JKTerminalGrid::SetAltScreen(bool on) {
    if (on == altActive_) return;
    if (on) {
        // Stash the main screen; the alt buffer starts blank (xterm 1049).
        altCells_ = std::move(cells_);
        cells_.assign(altCells_.size(), JKTermCell{});
    } else {
        // Restore the main screen and reset the alt buffer for next time.
        cells_ = std::move(altCells_);
        altCells_.assign(cells_.size(), JKTermCell{});
    }
    altActive_ = on;
    // xterm 1049 semantics: cursor home on swap.
    cursor_.x = 0;
    cursor_.y = 0;
    scrollTop_ = 0;
    scrollBottom_ = rows_ - 1;
    MarkAllDirty();
}

void JKTerminalGrid::Reset() {
    ClearCells(cells_);
    ClearCells(altCells_);
    altActive_ = false;
    scrollback_.clear();
    cursor_ = Cursor{};
    scrollTop_ = 0;
    scrollBottom_ = rows_ - 1;
    savedCursor_ = SavedCursor{};
    altSavedCursor_ = SavedCursor{};
    curFg = kTermDefaultColor;
    curBg = kTermDefaultColor;
    curAttrs = 0;
    MarkAllDirty();
}

} // namespace jk