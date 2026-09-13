#include <apps/TerminalView.h>
#include <apps/JKTermInput.h>
#include <terminal/JKVtParser.h>
#include <terminal/JKGlyphAtlas.h>
#include <JKResourceCache.h>
#include <JKApplicationHost.h>
#include <SDL.h>
#include <algorithm>
#include <cstring>

namespace jk {

namespace {

// Theme (docs/22 §5): near-black background, light gray default text. The
// values are instance members now (terminal.json overrides, docs/26 단계 5);
// these constants are only the member initializers.
constexpr uint32_t kDefaultThemeBg = 0x0C0C0C;
constexpr uint32_t kDefaultThemeFg = 0xCCCCCC;

uint32_t RgbOf(uint32_t c) { return c & 0x00FFFFFF; }

// Blend num/255 of fg into bg, per channel — used for the IME pre-edit
// overlay background (a slightly bright tint of the theme fg, spec §3).
uint32_t MixRgb(uint32_t bg, uint32_t fg, int num) {
    const auto chan = [num, bg, fg](int shift) {
        const int b = static_cast<int>((bg >> shift) & 0xFF);
        const int f = static_cast<int>((fg >> shift) & 0xFF);
        return (b * (255 - num) + f * num) / 255;
    };
    return static_cast<uint32_t>((chan(16) << 16) | (chan(8) << 8) | chan(0));
}

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

    dc.SetColor(static_cast<uint8_t>((themeBg_ >> 16) & 0xFF),
                static_cast<uint8_t>((themeBg_ >> 8) & 0xFF),
                static_cast<uint8_t>(themeBg_ & 0xFF), 255);
    dc.FillRect(client);

    // Selection (docs/26 단계 2) covers LIVE grid rows only — scrollback
    // snapshots are never selected (spec §5 v1 restriction: no scroll-while-
    // select). The anchor/end pair is normalized to an inclusive cell rect;
    // a drag performed while scrolled back still targets live rows.
    JKTermSelRect sel;   // Empty() until a selection exists
    if (selAnchor_.x >= 0) {
        sel = NormalizeSel(selAnchor_.x, selAnchor_.y, selEnd_.x, selEnd_.y,
                           grid_->Cols(), grid_->Rows());
    }

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
            // Never selection-highlighted (see the sel comment above).
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
                          off == 0 && gr == cursor.y && c == cursor.x,
                          sel.Contains(c, gr));
            }
        }
    }

    // IME pre-edit overlay (docs/26 단계 5, spec §3) paints AFTER the
    // selection swap: the cursor cell may be selected and the overlay wins.
    // Shown on the live view only (off == 0, the same gate the cursor outline
    // uses) — while scrolled back the composition stays hidden until the
    // user returns to the live view.
    if (!preEdit_.empty() && off == 0) {
        PaintPreEdit(dc, client);
    }
}

void TerminalView::PaintPreEdit(JKDC& dc, const JKRect& client) {
    const auto& cursor = grid_->GetCursor();
    const std::vector<uint32_t> cps = DecodeUtf8(preEdit_);
    // Fixed blend: 25% of themeFg mixed into themeBg — a slightly bright
    // background strip marking the composition span without clashing with
    // reverse/selection cells it covers.
    constexpr int kPreEditMix = 64;   // 64/255 ≈ 25%
    const uint32_t bg = MixRgb(RgbOf(themeBg_), RgbOf(themeFg_), kPreEditMix);
    dc.SetColor(static_cast<uint8_t>((bg >> 16) & 0xFF),
                static_cast<uint8_t>((bg >> 8) & 0xFF),
                static_cast<uint8_t>(bg & 0xFF), 255);
    for (size_t i = 0, x = static_cast<size_t>(std::max(0, cursor.x));
         i < cps.size() && x < static_cast<size_t>(cols_); ++i) {
        // One glyph per cell; wide glyphs (JKTermCharWidth == 2) occupy 2
        // cells, clamped at the grid width (spec §3).
        const int w = std::min(JKTermCharWidth(cps[i]), cols_ - static_cast<int>(x));
        const JKRect r{ client.x + static_cast<int>(x) * kTermCellW,
                        client.y + cursor.y * kTermCellH,
                        kTermCellW * w, kTermCellH };
        dc.FillRect(r);
        PaintGlyph(dc, r, cps[i], RgbOf(themeFg_), false);
        x += static_cast<size_t>(w);
    }
}

void TerminalView::PaintCell(JKDC& dc, const JKRect& cellRect,
                             const JKTermCell& cell, bool isCursor,
                             bool selected) {
    // Selected cells render with fg/bg swapped — the same visual as a
    // kTermReverse cell, through the same paint path (spec §1).
    bool reverse = (cell.attrs & kTermReverse) != 0 || selected;
    const bool bold = (cell.attrs & kTermBold) != 0;
    uint32_t fg = (cell.fg == kTermDefaultColor) ? themeFg_ : RgbOf(cell.fg);
    uint32_t bg = (cell.bg == kTermDefaultColor) ? themeBg_ : RgbOf(cell.bg);
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

    if (bg != themeBg_) {
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
    // IME composition (docs/26 단계 5, spec §3): TextEditing carries the UTF-8
    // pre-edit string while the OS IME is composing. Stored for the cursor
    // overlay and dropped on commit (Char) or any key/paste/selection start.
    if (ev.type == JKEventType::TextEditing) {
        preEdit_ = ev.text;
        if (grid_) grid_->MarkAllDirty();   // repaint through the frame gate
    }
    if (onInput_) {
        if (ev.type == JKEventType::KeyDown) {
            HandleKeyDown(ev);
        } else if (ev.type == JKEventType::Char) {
            // Printable UTF-8 plus control bytes (CR/Tab/Esc arrive here when
            // the input source synthesizes unicode chars instead of key events,
            // e.g. SendKeys/IME commit). NUL and DEL are never meaningful.
            const unsigned char b0 = static_cast<unsigned char>(ev.text[0]);
            if (b0 != 0x00 && b0 != 0x7F) {
                ClearPreEdit();   // the commit replaces the composition (§3)
                ClearSelection();   // any text input drops the selection
                scrollOffset_ = 0;   // typing returns to the live view
                onInput_(ev.text, std::strlen(ev.text));
            }
        }
    }
    // Mouse selection MUST run before the JKWindow delegation: in single-
    // process mode the view IS the main window and JKWindow::RespondMessage
    // drops client-area mouse events (the HitTest target is the window
    // itself, guarded by "target != this", JKWindow.cpp:478). In client mode
    // the events arrive here as a DOCK_FILL child via HitTest with the same
    // window/surface coordinates, so one handler covers both modes. Events
    // outside the client area fall through unchanged and keep the chrome
    // behavior (title-bar drag, border resize, close button).
    HandleMouseEvent(ev);
    JKWindow::RespondMessage(ev);
}

JKPoint TerminalView::CellFromPoint(int32_t px, int32_t py) const {
    // ev.x/y are window/surface coordinates; the client-area origin converts
    // them into cell pixel space in BOTH modes (single-process: the window's
    // own chrome; client mode: the DOCK_FILL child offset inside its parent).
    const JKRect client = GetScreenClientRect();
    const int cx = std::clamp((px - client.x) / kTermCellW, 0,
                              std::max(0, cols_ - 1));
    int cy = std::clamp((py - client.y) / kTermCellH, 0,
                        std::max(0, rows_ - 1));
    // Viewport row → live grid row. OnPaintClient shows the live rows under
    // a scroll offset: the top visible line is (history - offset), so live
    // grid row = viewport row - offset (same arithmetic as OnPaintClient's
    // gr = line - hist in the live branch). Viewport rows sitting over
    // scrollback snapshots (r < off) clamp onto live row 0 — v1 selects live
    // rows only (spec §5). Mapping lives in JKTermSelection.h so the
    // self-test exercises the production formula.
    if (grid_) {
        const int off = std::min(scrollOffset_, grid_->ScrollbackLines());
        cy = ViewportRowToLive(cy, off, rows_);
    }
    return JKPoint{ cx, cy };
}

void TerminalView::HandleMouseEvent(const JKEvent& ev) {
    if (!grid_) return;
    const bool isMouse = ev.type == JKEventType::MouseDown ||
                         ev.type == JKEventType::MouseUp ||
                         ev.type == JKEventType::MouseMove;
    if (!isMouse) return;

    const JKRect client = GetScreenClientRect();
    const bool inside = client.Contains(ev.x, ev.y);
    // Outside the client area the event is delegated unchanged — unless a
    // drag is already active: the capture keeps motion/release flowing while
    // the cursor leaves the window, and the coordinates are clamped to the
    // edge cell so the selection keeps its extreme (CellFromPoint clamps).
    if (!inside && !selDragging_) return;

    switch (ev.type) {
        case JKEventType::MouseDown:
            if (ev.detail != SDL_BUTTON_LEFT) {
                // Right/middle during an active drag: the host released the
                // capture on this MouseDown, so re-arm it — otherwise motion
                // outside the client stops reaching the view and the
                // selection freezes until the next click.
                if (selDragging_ && g_jkAppHost) g_jkAppHost->SetCapture(this);
                return;   // right/middle: chrome
            }
            // A left click always drops an existing selection; the new anchor
            // is armed at once and degenerates away on MouseUp if the user
            // only clicked (no drag → no selection, spec §1).
            selDragging_ = true;
            ClearPreEdit();   // selection start drops the composition (§3)
            selAnchor_ = CellFromPoint(ev.x, ev.y);
            selEnd_ = selAnchor_;
            grid_->MarkAllDirty();
            // Keep motion/release flowing outside the window while dragging
            // (same capture idiom as JKEdit.cpp:308).
            if (g_jkAppHost) g_jkAppHost->SetCapture(this);
            break;
        case JKEventType::MouseMove:
            if (!selDragging_) return;
            {
                const JKPoint cell = CellFromPoint(ev.x, ev.y);
                if (cell.x != selEnd_.x || cell.y != selEnd_.y) {
                    selEnd_ = cell;
                    grid_->MarkAllDirty();   // repaint through the frame gate
                }
            }
            break;
        case JKEventType::MouseUp:
            // Release BEFORE the drag-flag check: a mid-drag key press can
            // clear selDragging_ through ClearSelection (which releases too);
            // this covers any other path — a stuck capture would hijack all
            // mouse input until the next click.
            if (g_jkAppHost && g_jkAppHost->GetCapture() == this) {
                g_jkAppHost->ReleaseCapture();
            }
            if (!selDragging_) return;
            selDragging_ = false;
            if (selAnchor_.x == selEnd_.x && selAnchor_.y == selEnd_.y) {
                ClearSelection();   // click without drag → no selection
            }
            break;
        default:
            break;
    }
}

void TerminalView::ClearPreEdit() {
    if (preEdit_.empty()) return;
    preEdit_.clear();
    if (grid_) grid_->MarkAllDirty();   // drop the overlay this frame
}

void TerminalView::ClearSelection() {
    if (selAnchor_.x < 0 && !selDragging_) return;
    selAnchor_ = JKPoint{ -1, -1 };
    selEnd_ = JKPoint{ -1, -1 };
    // A mid-drag key press reaches here with the drag flag set and the mouse
    // capture still held — drop the capture too, or the MouseUp would
    // early-return on the cleared flag and the view keeps ALL mouse input
    // until the next click (capture leak).
    if (selDragging_ && g_jkAppHost && g_jkAppHost->GetCapture() == this) {
        g_jkAppHost->ReleaseCapture();
    }
    selDragging_ = false;
    if (grid_) grid_->MarkAllDirty();   // un-reverse the highlighted cells
}

void TerminalView::CopySelection() {
    if (!grid_ || selAnchor_.x < 0) return;
    const JKTermSelRect sel = NormalizeSel(selAnchor_.x, selAnchor_.y,
                                           selEnd_.x, selEnd_.y,
                                           grid_->Cols(), grid_->Rows());
    // Live grid rows only (spec §5); the accessor keeps ExtractSelectedText
    // independent of the grid type so the self-test can share it.
    const JKTerminalGrid* g = grid_;
    const std::string text = ExtractSelectedText(
        [g](int c, int r) -> const JKTermCell& { return g->Cell(c, r); }, sel);
    if (!text.empty()) {
        SDL_SetClipboardText(text.c_str());   // JKEdit.cpp:716 precedent
    }
}

void TerminalView::PasteClipboard() {
    if (!onInput_ || !SDL_HasClipboardText()) return;
    char* raw = SDL_GetClipboardText();
    if (!raw) return;
    // Bracketed paste gate (spec §2): JKVtParser already tracks DECSET 2004;
    // when on, the sanitized payload is wrapped so the shell sees a paste.
    const std::string data =
        SanitizeClipboardPaste(raw, parser_ && parser_->BracketedPaste());
    SDL_free(raw);
    ClearPreEdit();   // pasting drops the composition (spec §3)
    ClearSelection();   // pasting drops the selection (spec §2)
    if (!data.empty()) {
        scrollOffset_ = 0;   // paste returns to the live view
        onInput_(data.data(), data.size());
    }
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

    // Any key drops the IME composition (docs/26 단계 5, spec §3) — bare
    // modifier presses included, harmlessly: the next TextEditing update
    // re-arms it. Ordering with SDL is KeyDown first, TextEditing after, so
    // the composition repaints within the same frame.
    ClearPreEdit();

    // Selection lifetime (docs/26 단계 2, spec §2): any key drops the
    // selection — except the clipboard chords (handled in the switch below,
    // which copy/paste the selection first) and bare modifier presses. The
    // modifier exclusion is load-bearing: Ctrl and Shift arrive as their own
    // KeyDown events BEFORE the C/V of the chord, and clearing there would
    // destroy the selection CopySelection is about to read.
    const bool clipboardChord =
        ctrl && shift &&
        (key == 'C' || key == 'c' || key == 'V' || key == 'v');
    const bool modifierKey =
        key == SDLK_LCTRL || key == SDLK_RCTRL ||
        key == SDLK_LSHIFT || key == SDLK_RSHIFT ||
        key == SDLK_LALT || key == SDLK_RALT ||
        key == SDLK_LGUI || key == SDLK_RGUI;
    if (!clipboardChord && !modifierKey) {
        ClearSelection();
    }

    // docs/22 §6.1 input mapping.
    const char* seq = nullptr;
    char buf[8];
    size_t n = 0;

    // Arrow/navigation keys (spec §4): delegated to EncodeArrow — DECSET 1
    // (application cursor keys, parser_->AppCursorKeys()) switches the
    // arrow/Home/End family to SS3, and any modifier produces CSI 1;<m><char>
    // (Ctrl+Left = \x1b[1;5D for vim word motion; Shift = 1, Alt = 2,
    // Ctrl = 4 xterm wire bits — NOT SDL KMOD, translated here). Alt-modified
    // arrows therefore drop the legacy bare-ESC prefix above: the modifier is
    // encoded inside the sequence per xterm. PgUp/PgDn without modifiers keep
    // the plain \x1b[5~/\x1b[6~ forms. This branch only fires for these keys;
    // the ctrl+shift C/V clipboard chords and the ctrl+letter control-byte
    // path in the switch below are untouched (docs/40 key-order regression
    // guard).
    const bool appCursor = parser_ && parser_->AppCursorKeys();
    const int navMods = (shift ? 1 : 0) | (alt ? 2 : 0) | (ctrl ? 4 : 0);
    std::string navSeq;
    switch (key) {
        case SDLK_UP:       navSeq = jk::EncodeArrow(jk::NavKey::Up,    navMods, appCursor); break;
        case SDLK_DOWN:     navSeq = jk::EncodeArrow(jk::NavKey::Down,  navMods, appCursor); break;
        case SDLK_RIGHT:    navSeq = jk::EncodeArrow(jk::NavKey::Right, navMods, appCursor); break;
        case SDLK_LEFT:     navSeq = jk::EncodeArrow(jk::NavKey::Left,  navMods, appCursor); break;
        case SDLK_HOME:     navSeq = jk::EncodeArrow(jk::NavKey::Home,  navMods, appCursor); break;
        case SDLK_END:      navSeq = jk::EncodeArrow(jk::NavKey::End,   navMods, appCursor); break;
        case SDLK_PAGEUP:   navSeq = jk::EncodeArrow(jk::NavKey::PgUp,  navMods, appCursor); break;
        case SDLK_PAGEDOWN: navSeq = jk::EncodeArrow(jk::NavKey::PgDn,  navMods, appCursor); break;
        default: break;
    }
    if (!navSeq.empty()) {
        scrollOffset_ = 0;   // key input returns to the live view
        onInput_(navSeq.data(), navSeq.size());
        return;
    }

    // Legacy ESC prefix for Alt-modified keys that fall through to the
    // mapping below (the nav branch above encodes Alt inside CSI 1;<m><char>
    // and returns early, so it never sees this byte).
    if (alt) buf[n++] = '\x1b';

    switch (key) {
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
            // Clipboard chords (docs/26 단계 2, spec §2) are checked FIRST,
            // before the ctrl+letter path: ctrl+C WITHOUT shift must still
            // send 0x03 (SIGINT), only ctrl+shift+C is Copy.
            if (ctrl && shift && (key == 'C' || key == 'c')) {
                CopySelection();   // the selection survives a copy
                return;
            }
            if (ctrl && shift && (key == 'V' || key == 'v')) {
                PasteClipboard();
                return;
            }
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