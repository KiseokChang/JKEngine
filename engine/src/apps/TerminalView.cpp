#include <apps/TerminalView.h>
#include <terminal/JKVtParser.h>
#include <terminal/JKGlyphAtlas.h>
#include <JKResourceCache.h>
#include <SDL.h>
#include <algorithm>
#include <cstring>

namespace jk {

namespace {

// Theme (docs/22 §5): near-black background, light gray default text.
constexpr uint32_t kThemeBg = 0x0C0C0C;
constexpr uint32_t kThemeFg = 0xCCCCCC;

uint32_t RgbOf(uint32_t c) { return c & 0x00FFFFFF; }

} // namespace

TerminalView::TerminalView(JKVtParser* parser, JKTerminalGrid* grid,
                           JKGlyphAtlas* atlas, JKResourceCache* cache)
    : JKWindow(""), parser_(parser), grid_(grid), atlas_(atlas), cache_(cache) {}

void TerminalView::OnRectChanged(const JKRect& rect) {
    JKWindow::OnRectChanged(rect);
    RecalcCells();
}

void TerminalView::RecalcCells() {
    const JKRect client = GetScreenClientRect();
    const int newCols = std::max(1, client.w / kTermCellW);
    const int newRows = std::max(1, client.h / kTermCellH);
    if (newCols == cols_ && newRows == rows_) return;
    cols_ = newCols;
    rows_ = newRows;
    if (grid_) {
        grid_->Resize(cols_, rows_);
    }
    if (onResize_) {
        onResize_(cols_, rows_);
    }
}

void TerminalView::TickBlink() {
    blinkOn_ = !blinkOn_;
    if (grid_) {
        grid_->MarkAllDirty();
    }
}

void TerminalView::HandleWheel(int wheelY) {
    if (!grid_) return;
    scrollOffset_ = std::clamp(scrollOffset_ + wheelY * 3, 0,
                               grid_->ScrollbackLines());
    // Wheel scrolling alone must repaint: the frame gate only renders when
    // the grid content is dirty.
    grid_->MarkAllDirty();
}

void TerminalView::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    if (!grid_ || client.w <= 0 || client.h <= 0) return;

    dc.SetColor(static_cast<uint8_t>((kThemeBg >> 16) & 0xFF),
                static_cast<uint8_t>((kThemeBg >> 8) & 0xFF),
                static_cast<uint8_t>(kThemeBg & 0xFF), 255);
    dc.FillRect(client);

    // Viewport lines are indexed over scrollback + live grid: the top visible
    // line is (history - offset); rows past the history come from the grid.
    // offset == 0 is the live view (identical to painting the grid alone).
    const auto& cursor = grid_->GetCursor();
    const int hist = grid_->ScrollbackLines();
    const int off = std::min(scrollOffset_, hist);
    const int top = hist - off;
    for (int r = 0; r < grid_->Rows(); ++r) {
        const int line = top + r;
        const int cellY = client.y + r * kTermCellH;
        if (line < hist) {
            // Scrollback snapshot — may be narrower than the current columns
            // after a resize; cells past its width paint as default bg.
            const auto& lineCells = grid_->ScrollbackLine(line);
            const int lineCols =
                std::min<int>(static_cast<int>(lineCells.size()), grid_->Cols());
            for (int c = 0; c < grid_->Cols(); ++c) {
                const JKRect cellRect{ client.x + c * kTermCellW, cellY,
                                       kTermCellW, kTermCellH };
                static const JKTermCell kEmpty{};
                const JKTermCell& cell = (c < lineCols) ? lineCells[c] : kEmpty;
                PaintCell(dc, cellRect, cell, false);
            }
        } else {
            const int gr = line - hist;
            for (int c = 0; c < grid_->Cols(); ++c) {
                const JKRect cellRect{ client.x + c * kTermCellW, cellY,
                                       kTermCellW, kTermCellH };
                const JKTermCell& cell = grid_->Cell(c, gr);
                PaintCell(dc, cellRect, cell,
                          off == 0 && gr == cursor.y && c == cursor.x);
            }
        }
    }
}

void TerminalView::PaintCell(JKDC& dc, const JKRect& cellRect,
                             const JKTermCell& cell, bool isCursor) {
    bool reverse = (cell.attrs & kTermReverse) != 0;
    const bool bold = (cell.attrs & kTermBold) != 0;
    uint32_t fg = (cell.fg == kTermDefaultColor) ? kThemeFg : RgbOf(cell.fg);
    uint32_t bg = (cell.bg == kTermDefaultColor) ? kThemeBg : RgbOf(cell.bg);
    if (reverse) std::swap(fg, bg);

    auto setColor = [&dc](uint32_t rgb) {
        dc.SetColor(static_cast<uint8_t>((rgb >> 16) & 0xFF),
                    static_cast<uint8_t>((rgb >> 8) & 0xFF),
                    static_cast<uint8_t>(rgb & 0xFF), 255);
    };

    // Wide glyphs (docs/26 단계 1) paint across their follower cell too; the
    // width-0 follower itself paints bg-only through the same path.
    JKRect r = cellRect;
    if (cell.width == 2) r.w *= 2;

    if (bg != kThemeBg) {
        setColor(bg);
        dc.FillRect(r);
    }

    if (cell.cp != 0) {
        PaintGlyph(dc, r, cell.cp, fg, bold);
    }
    if (cell.attrs & kTermUnderline) {
        setColor(fg);
        dc.DrawLine(r.x, r.y + kTermCellH - 2, r.x + r.w - 1,
                    r.y + kTermCellH - 2);
    }
    if (isCursor && grid_->GetCursor().visible && blinkOn_) {
        setColor(fg);
        dc.DrawRect(r);
        dc.DrawRect(JKRect{ r.x + 1, r.y + 1, r.w - 2, r.h - 2 });
    }
}

void TerminalView::PaintGlyph(JKDC& dc, const JKRect& cellRect, uint32_t cp,
                              uint32_t fg, bool bold) {
    if (atlas_ && atlas_->IsLoaded() && cache_ &&
        atlas_->EnsurePage(cache_, fg, bold, cp)) {
        const JKRect src = atlas_->GlyphSrc(cp);
        if (src.w > 0) {
            const auto handle = cache_->GetImage(atlas_->PageKey(fg, bold, cp));
            if (handle != JKRenderBackend::InvalidTexture) {
                dc.GetBackend()->BlitTexture(handle, &src, cellRect, 255);
                return;
            }
        }
    }
    PaintFallbackGlyph(dc, cellRect, cp, fg);
}

// Only used when the font/atlas is unavailable: approximate the box-drawing
// subset ConPTY repaints and a placeholder for everything else.
void TerminalView::PaintFallbackGlyph(JKDC& dc, const JKRect& cellRect,
                                      uint32_t cp, uint32_t fg) {
    dc.SetColor(static_cast<uint8_t>((fg >> 16) & 0xFF),
                static_cast<uint8_t>((fg >> 8) & 0xFF),
                static_cast<uint8_t>(fg & 0xFF), 255);
    const int mx = cellRect.x + kTermCellW / 2;
    const int my = cellRect.y + kTermCellH / 2;
    switch (cp) {
        case 0x2500:   // ─
            dc.DrawLine(cellRect.x, my, cellRect.x + kTermCellW - 1, my);
            return;
        case 0x2502:   // │
            dc.DrawLine(mx, cellRect.y, mx, cellRect.y + kTermCellH - 1);
            return;
        case 0x250C:   // ┌
            dc.DrawLine(mx, my, mx, cellRect.y + kTermCellH - 1);
            dc.DrawLine(mx, my, cellRect.x + kTermCellW - 1, my);
            return;
        case 0x2510:   // ┐
            dc.DrawLine(mx, my, mx, cellRect.y + kTermCellH - 1);
            dc.DrawLine(cellRect.x, my, mx, my);
            return;
        case 0x2514:   // └
            dc.DrawLine(mx, cellRect.y, mx, my);
            dc.DrawLine(mx, my, cellRect.x + kTermCellW - 1, my);
            return;
        case 0x2518:   // ┘
            dc.DrawLine(mx, cellRect.y, mx, my);
            dc.DrawLine(cellRect.x, my, mx, my);
            return;
        case 0x251C:   // ├
            dc.DrawLine(mx, cellRect.y, mx, cellRect.y + kTermCellH - 1);
            dc.DrawLine(mx, my, cellRect.x + kTermCellW - 1, my);
            return;
        case 0x2524:   // ┤
            dc.DrawLine(mx, cellRect.y, mx, cellRect.y + kTermCellH - 1);
            dc.DrawLine(cellRect.x, my, mx, my);
            return;
        case 0x252C:   // ┬
            dc.DrawLine(cellRect.x, my, cellRect.x + kTermCellW - 1, my);
            dc.DrawLine(mx, my, mx, cellRect.y + kTermCellH - 1);
            return;
        case 0x2534:   // ┴
            dc.DrawLine(cellRect.x, my, cellRect.x + kTermCellW - 1, my);
            dc.DrawLine(mx, cellRect.y, mx, my);
            return;
        case 0x253C:   // ┼
            dc.DrawLine(cellRect.x, my, cellRect.x + kTermCellW - 1, my);
            dc.DrawLine(mx, cellRect.y, mx, cellRect.y + kTermCellH - 1);
            return;
        case 0x2588:   // █
            dc.FillRect(cellRect);
            return;
        default:
            break;
    }
    if (cp >= 0x2500 && cp <= 0x259F) {
        // Double-line / shades: single mid line placeholder.
        dc.DrawLine(cellRect.x, my, cellRect.x + kTermCellW - 1, my);
        return;
    }
    dc.FillRect(JKRect{ mx - 2, cellRect.y + 3, 4, kTermCellH - 6 });
}

void TerminalView::RespondMessage(const JKEvent& ev) {
    if (onInput_) {
        if (ev.type == JKEventType::KeyDown) {
            HandleKeyDown(ev);
        } else if (ev.type == JKEventType::Char) {
            // Printable UTF-8 plus control bytes (CR/Tab/Esc arrive here when
            // the input source synthesizes unicode chars instead of key events,
            // e.g. SendKeys/IME commit). NUL and DEL are never meaningful.
            const unsigned char b0 = static_cast<unsigned char>(ev.text[0]);
            if (b0 != 0x00 && b0 != 0x7F) {
                scrollOffset_ = 0;   // typing returns to the live view
                onInput_(ev.text, std::strlen(ev.text));
            }
        }
    }
    JKWindow::RespondMessage(ev);
}

void TerminalView::HandleKeyDown(const JKEvent& ev) {
    const SDL_Keycode key = static_cast<SDL_Keycode>(ev.keyCode);
    // The forwarded modifier state travels in ev.option (server mode) and is
    // filled by TranslateSDLEvent (single-process mode). SDL_GetModState() is
    // the local window's state, which is always empty for piped-in input.
    const SDL_Keymod mod = static_cast<SDL_Keymod>(ev.option);
    const bool ctrl = (mod & KMOD_CTRL) != 0;
    const bool alt = (mod & KMOD_ALT) != 0;
    const bool shift = (mod & KMOD_SHIFT) != 0;

    // docs/22 §6.1 input mapping.
    const char* seq = nullptr;
    char buf[8];
    size_t n = 0;
    if (alt) buf[n++] = '\x1b';

    switch (key) {
        case SDLK_UP:      seq = "\x1b[A"; break;
        case SDLK_DOWN:    seq = "\x1b[B"; break;
        case SDLK_RIGHT:   seq = "\x1b[C"; break;
        case SDLK_LEFT:    seq = "\x1b[D"; break;
        case SDLK_HOME:    seq = "\x1b[H"; break;
        case SDLK_END:     seq = "\x1b[F"; break;
        case SDLK_PAGEUP:  seq = "\x1b[5~"; break;
        case SDLK_PAGEDOWN: seq = "\x1b[6~"; break;
        case SDLK_INSERT:  seq = "\x1b[2~"; break;
        case SDLK_DELETE:  seq = "\x1b[3~"; break;
        case SDLK_F1:      seq = "\x1bOP"; break;
        case SDLK_F2:      seq = "\x1bOQ"; break;
        case SDLK_F3:      seq = "\x1bOR"; break;
        case SDLK_F4:      seq = "\x1bOS"; break;
        case SDLK_F5:      seq = "\x1b[15~"; break;
        case SDLK_F6:      seq = "\x1b[17~"; break;
        case SDLK_F7:      seq = "\x1b[18~"; break;
        case SDLK_F8:      seq = "\x1b[19~"; break;
        case SDLK_F9:      seq = "\x1b[20~"; break;
        case SDLK_F10:     seq = "\x1b[21~"; break;
        case SDLK_F11:     seq = "\x1b[23~"; break;
        case SDLK_F12:     seq = "\x1b[24~"; break;
        case SDLK_TAB:
            seq = shift ? "\x1b[Z" : "\t";
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            seq = "\r";
            break;
        case SDLK_BACKSPACE:
            seq = "\x7f";
            break;
        case SDLK_ESCAPE:
            seq = "\x1b";
            break;
        default:
            if (ctrl && key >= 'a' && key <= 'z') {
                buf[n++] = static_cast<char>(key & 0x1F);
            } else if (ctrl && key >= 'A' && key <= 'Z') {
                buf[n++] = static_cast<char>(key & 0x1F);
            } else {
                return;   // printable text arrives via the Char event
            }
            break;
    }
    if (seq) {
        const size_t len = std::strlen(seq);
        std::memcpy(buf + n, seq, std::min(len, sizeof(buf) - n));
        n += std::min(len, sizeof(buf) - n);
    }
    if (n > 0) {
        scrollOffset_ = 0;   // key input returns to the live view
        onInput_(buf, n);
    }
}

} // namespace jk