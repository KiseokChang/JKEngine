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
    // Reflow (docs/26 단계 4) covers the MAIN screen + scrollback. While an
    // app owns the alt screen the stashed main content is out of sync with
    // the live geometry — fall back to the Phase-1 approximation there (the
    // app repaints everything after the resize anyway).
    if (altActive_ || cols_ == 0) {
        LegacyResize(cols, rows);
        return;
    }
    ReflowMain(cols, rows);
}

void JKTerminalGrid::LegacyResize(int cols, int rows) {
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
    rowWrappedAlt_.assign(static_cast<size_t>(rows), false);

    cols_ = cols;
    rows_ = rows;
    cursor_.x = std::min(cursor_.x, cols_ - 1);
    cursor_.y = std::min(cursor_.y, rows_ - 1);
    scrollTop_ = 0;
    scrollBottom_ = rows_ - 1;
    rowWrapped_.assign(static_cast<size_t>(rows_), false);
    dirtyRows_.assign(static_cast<size_t>(rows_), false);
    MarkAllDirty();
}

void JKTerminalGrid::ReflowMain(int cols, int rows) {
    // --- 1. Unwrap: history + main screen → logical lines. -----------------
    // A line with wrapped=true is glued to the next source line. Trailing
    // BLANK screen rows below the cursor are padding, not content — they are
    // dropped (reflow re-pads); blank rows between content and the cursor
    // (or the cursor row itself) are kept as empty logical lines.
    std::vector<std::vector<JKTermCell>> logical;
    std::vector<int> rowLine(static_cast<size_t>(rows_), -1);   // screen row → logical line
    std::vector<int> rowPrefix(static_cast<size_t>(rows_));     // cells of that line above the row
    int endRow = rows_ - 1;
    while (endRow > cursor_.y) {
        bool blank = true;
        for (int c = 0; c < cols_ && blank; ++c) {
            if (cells_[static_cast<size_t>(endRow) * cols_ + c].cp != 0) blank = false;
        }
        if (!blank) break;
        --endRow;
    }
    int curLine = -1;
    bool open = false;   // previous source line soft-wrapped into this one
    auto appendLine = [&](const JKTermCell* cells, int width, bool wrapped,
                          bool isScreenRow, int screenRow) {
        // Trailing blank cells are padding, not content — without this trim a
        // 3-char row on a 20-col screen becomes a 20-cell logical line and
        // rewraps into a content row plus an all-blank row at any width < 20.
        // Width-0 follower dummies (wide-glyph tails) are content and stay.
        while (width > 0 && cells[width - 1].cp == 0 &&
               cells[width - 1].width != 0) {
            --width;
        }
        if (!open) {
            logical.emplace_back();
            ++curLine;
        }
        if (isScreenRow) {
            rowLine[static_cast<size_t>(screenRow)] = curLine;
            rowPrefix[static_cast<size_t>(screenRow)] =
                static_cast<int>(logical.back().size());
        }
        auto& dst = logical.back();
        dst.insert(dst.end(), cells, cells + width);
        open = wrapped;
    };
    for (const auto& snap : scrollback_) {
        appendLine(snap.cells.data(), static_cast<int>(snap.cells.size()),
                   snap.wrapped, false, 0);
    }
    for (int r = 0; r <= endRow; ++r) {
        appendLine(&cells_[static_cast<size_t>(r) * cols_], cols_,
                   r < endRow ? RowWrapped(r) : false, true, r);
    }
    // Cursor position inside its logical line (cell units).
    const int cursorLine = (cursor_.y <= endRow)
        ? rowLine[static_cast<size_t>(cursor_.y)] : curLine;
    const int cursorCol = (cursor_.y <= endRow)
        ? rowPrefix[static_cast<size_t>(cursor_.y)] +
              std::min(cursor_.x, cols_)
        : 0;

    // --- 2. Rewrap to the new width. ----------------------------------------
    // A chunk never splits a wide glyph from its follower: the glyph moves to
    // the next row whole (docs/26 §4 — "wrap 경계에서 원본만 따라감"). Each
    // chunk that is not the last of its logical line is itself soft-wrapped.
    struct RowMap { int line; int start; };
    std::vector<RowMap> producedMap;
    std::vector<std::vector<JKTermCell>> produced;
    std::vector<bool> producedWrapped;
    for (size_t li = 0; li < logical.size(); ++li) {
        const auto& src = logical[li];
        if (src.empty()) {
            produced.emplace_back(cols);
            producedWrapped.push_back(false);
            producedMap.push_back({ static_cast<int>(li), 0 });
            continue;
        }
        size_t i = 0;
        while (i < src.size()) {
            size_t take = std::min<size_t>(cols, src.size() - i);
            if (take == static_cast<size_t>(cols) && i + take < src.size() &&
                src[i + take - 1].width == 2) {
                --take;   // the wide glyph's follower would fall off the edge
            }
            auto& dst = produced.emplace_back(src.begin() + static_cast<ptrdiff_t>(i),
                                              src.begin() + static_cast<ptrdiff_t>(i + take));
            dst.resize(static_cast<size_t>(cols));   // pad with default cells
            producedWrapped.push_back(i + take < src.size());
            producedMap.push_back({ static_cast<int>(li), static_cast<int>(i) });
            i += take;
        }
    }

    // --- 3. Assemble screen + scrollback (new buffers, then swap). ----------
    // Cursor anchor (docs/26 단계 4): the cursor keeps its offset from the
    // screen bottom — the shell prompt must not jump. Produced rows above
    // the resulting viewport overflow into scrollback.
    const int producedCount = static_cast<int>(produced.size());
    const int bottomOffset = rows_ - 1 - cursor_.y;
    int pCursor = producedCount - 1;
    for (int p = producedCount - 1; p >= 0; --p) {
        const RowMap& m = producedMap[static_cast<size_t>(p)];
        if (m.line == cursorLine && m.start <= cursorCol) {
            pCursor = p;
            break;
        }
    }
    const int screenTop = std::clamp(pCursor - (rows - 1 - bottomOffset),
                                     0, std::max(0, producedCount - rows));
    const int pad = std::max(0, rows - producedCount);
    std::vector<JKTermCell> next(static_cast<size_t>(cols) * rows);
    std::vector<bool> nextWrapped(static_cast<size_t>(rows), false);
    for (int r = 0; r < rows; ++r) {
        const int p = screenTop + r - pad;   // produced index (negative = pad)
        auto* dstRow = &next[static_cast<size_t>(r) * cols];
        if (p >= 0) {
            std::copy(produced[static_cast<size_t>(p)].begin(),
                      produced[static_cast<size_t>(p)].end(), dstRow);
            nextWrapped[static_cast<size_t>(r)] = producedWrapped[static_cast<size_t>(p)];
        }
    }
    std::deque<JKTermScrollLine> nextHist;
    for (int p = 0; p < screenTop; ++p) {
        JKTermScrollLine snap;
        snap.cells = std::move(produced[static_cast<size_t>(p)]);
        snap.wrapped = producedWrapped[static_cast<size_t>(p)];
        nextHist.emplace_back(std::move(snap));
    }
    while (nextHist.size() > scrollbackMax_) {
        nextHist.pop_front();
    }

    // --- 4. Cursor mapping: logical (line, col) → new screen cell. ----------
    int newX = 0, newY = rows - 1;
    for (int p = screenTop; p < producedCount; ++p) {
        if (producedMap[static_cast<size_t>(p)].line != cursorLine) continue;
        if (producedMap[static_cast<size_t>(p)].start > cursorCol) break;
        const int screenR = pad + p - screenTop;
        if (screenR < 0 || screenR >= rows) continue;
        newX = cursorCol - producedMap[static_cast<size_t>(p)].start;
        newY = screenR;
    }
    newX = std::min(newX, cols);   // == cols keeps the deferred-wrap state

    cells_ = std::move(next);
    altCells_.assign(static_cast<size_t>(cols) * rows, JKTermCell{});
    scrollback_ = std::move(nextHist);
    cols_ = cols;
    rows_ = rows;
    rowWrapped_ = std::move(nextWrapped);
    cursor_.x = newX;
    cursor_.y = newY;
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
    // cannot fit with its follower — first starts a new line. The row is
    // flagged soft-wrapped (docs/26 단계 4) so a reflow can re-glue it.
    if (cursor_.x >= cols_ || cursor_.x + w > cols_) {
        if (cursor_.y >= 0 && cursor_.y < static_cast<int>(rowWrapped_.size())) {
            rowWrapped_[static_cast<size_t>(cursor_.y)] = true;
        }
        cursor_.x = 0;
        LineFeed(true);   // softWrap: the flag set above must survive
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
    // margin scrolls and alt-screen scrolls are not recorded). The snapshot
    // carries the row's soft-wrap flag (docs/26 단계 4): a wrapped line that
    // scrolled off continues into the next recorded/screen line.
    if (!altActive_ && scrollTop_ == 0) {
        JKTermScrollLine snap;
        snap.cells.assign(cells_.begin(),
                          cells_.begin() + static_cast<ptrdiff_t>(cols_));
        snap.wrapped = RowWrapped(0);
        scrollback_.emplace_back(std::move(snap));
        if (scrollback_.size() > scrollbackMax_) {
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
    // Wrap flags shift with the cells; the vacated bottom row is unwrapped.
    for (int r = scrollTop_; r < scrollBottom_; ++r) {
        rowWrapped_[static_cast<size_t>(r)] = rowWrapped_[static_cast<size_t>(r + 1)];
    }
    rowWrapped_[static_cast<size_t>(scrollBottom_)] = false;
    for (int r = scrollTop_; r <= scrollBottom_; ++r) MarkRowDirty(r);
}

void JKTerminalGrid::LineFeed(bool softWrap) {
    // A hard line feed voids the departing row's soft-wrap state (docs/26
    // 단계 4): apps that CUP back to a row and rewrite it would otherwise
    // leave stale wrapped=true flags behind. The deferred-wrap path passes
    // softWrap=true — the flag it just set must survive.
    if (!softWrap && cursor_.y >= 0 &&
        cursor_.y < static_cast<int>(rowWrapped_.size())) {
        rowWrapped_[static_cast<size_t>(cursor_.y)] = false;
    }
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
            // Wrap flags shift down with the cells; the fresh top row is
            // unwrapped.
            for (int r = scrollBottom_; r > scrollTop_; --r) {
                rowWrapped_[static_cast<size_t>(r)] =
                    rowWrapped_[static_cast<size_t>(r - 1)];
            }
            rowWrapped_[static_cast<size_t>(scrollTop_)] = false;
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
        // A wiped screen has no logical structure left (docs/26 단계 4) —
        // stale wrap flags would glue the post-clear repaint into the void.
        std::fill(rowWrapped_.begin(), rowWrapped_.end(), false);
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
    // Fully erased rows lose their soft-wrap continuation (docs/26 단계 4):
    // ED 0 wipes whole rows below the cursor, ED 1 above it.
    if (mode == 0) {
        for (int r = cursor_.y + 1; r < rows_; ++r) {
            rowWrapped_[static_cast<size_t>(r)] = false;
        }
    } else {
        for (int r = 0; r < cursor_.y; ++r) {
            rowWrapped_[static_cast<size_t>(r)] = false;
        }
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
        // EL 2 blanks the whole row — its continuation is gone.
        rowWrapped_[static_cast<size_t>(cursor_.y)] = false;
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
        // Wrap flags shift down with the cells; the fresh top row is
        // unwrapped (same shape as ReverseLineFeed).
        for (int r = scrollBottom_; r > scrollTop_; --r) {
            rowWrapped_[static_cast<size_t>(r)] =
                rowWrapped_[static_cast<size_t>(r - 1)];
        }
        rowWrapped_[static_cast<size_t>(scrollTop_)] = false;
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
        // Stash the main screen (wrap flags travel with it, docs/26 단계 4);
        // the alt buffer starts blank (xterm 1049).
        altCells_ = std::move(cells_);
        cells_.assign(altCells_.size(), JKTermCell{});
        rowWrappedAlt_ = std::move(rowWrapped_);
        rowWrapped_.assign(static_cast<size_t>(rows_), false);
    } else {
        // Restore the main screen and reset the alt buffer for next time.
        cells_ = std::move(altCells_);
        altCells_.assign(cells_.size(), JKTermCell{});
        rowWrapped_ = std::move(rowWrappedAlt_);
        rowWrappedAlt_.assign(static_cast<size_t>(rows_), false);
        rowWrapped_.resize(static_cast<size_t>(rows_), false);
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
    std::fill(rowWrapped_.begin(), rowWrapped_.end(), false);
    cursor_ = Cursor{};
    scrollTop_ = 0;
    scrollBottom_ = rows_ - 1;
    savedCursor_ = SavedCursor{};
    altSavedCursor_ = SavedCursor{};
    curFg = kTermDefaultColor;
    curBg = kTermDefaultColor;
    curAttrs = 0;
    cursorShape_ = CursorShape::Block;   // RIS resets DECSCUSR too (xterm);
                                         // Resize/alt-swap still preserve it
    MarkAllDirty();
}

} // namespace jk