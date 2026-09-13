#ifndef JKTERMSELECTION_H
#define JKTERMSELECTION_H

// Pure text-selection helpers for the terminal (docs/26 단계 2, spec §1-2).
// Header-only (no .cpp, no SDL/Windows dependencies) so TerminalView — shared
// by the single-process and window-server client modes — and the exe's
// self-test use one implementation.

#include <terminal/JKTerminalGrid.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace jk {

// Normalized selection: inclusive cell rect (x0,y0)..(x1,y1). x0 < 0 means
// "no selection". Box selection is row-based (no reflow model — the grid is
// a ConPTY repaint target, see docs/22 §5.2), so a rect is the whole model.
struct JKTermSelRect {
    int x0 = -1;
    int y0 = -1;
    int x1 = -1;
    int y1 = -1;

    bool Empty() const { return x0 < 0; }
    bool Contains(int col, int row) const {
        return !Empty() && row >= y0 && row <= y1 && col >= x0 && col <= x1;
    }
};

// Order anchor/end into a normalized rect and clamp both endpoints into the
// cols x rows grid (negative/out-of-range drag coordinates, e.g. a drag that
// leaves the window, snap to the edge cell).
inline JKTermSelRect NormalizeSel(int ax, int ay, int bx, int by,
                                  int cols, int rows) {
    if (cols <= 0 || rows <= 0) return {};
    const int maxX = cols - 1;
    const int maxY = rows - 1;
    const auto clamp = [maxX, maxY](int v, bool isCol) {
        const int hi = isCol ? maxX : maxY;
        return v < 0 ? 0 : (v > hi ? hi : v);
    };
    ax = clamp(ax, true);
    bx = clamp(bx, true);
    ay = clamp(ay, false);
    by = clamp(by, false);
    return JKTermSelRect{ std::min(ax, bx), std::min(ay, by),
                          std::max(ax, bx), std::max(ay, by) };
}

// Viewport row → live grid row while the view is scrolled back (TerminalView
// selection, docs/26 단계 2): the top visible line is (history - offset), so a
// viewport row r maps to the live grid row r - offset, clamped into the grid.
// Rows sitting over scrollback snapshots (r < off) clamp onto live row 0 —
// v1 selects live rows only (spec §5). Single source of truth for
// TerminalView::CellFromPoint (mouse → cell) and the self-test.
inline int ViewportRowToLive(int r, int off, int rows) {
    if (rows <= 0) return 0;
    const int gr = r - off;
    return gr < 0 ? 0 : (gr > rows - 1 ? rows - 1 : gr);
}

// UTF-8 encoder — the mirror of JKVtParser's decoder (docs/22 §4): standard
// 1-4 byte form, no BOM, no surrogate handling (cps are scalar values).
inline void AppendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// UTF-8 decoder — the mirror of AppendUtf8 above (docs/26 단계 5, spec §3):
// standard 1-4 byte form; invalid leads, overlong forms and surrogates decode
// as U+FFFD (one per bad byte, so the caller's cell pacing stays in sync with
// the input length). Pure helper so the view's pre-edit overlay and the
// self-test share one implementation.
inline std::vector<uint32_t> DecodeUtf8(const std::string& text) {
    const auto byte = [&text](size_t k) {
        return static_cast<unsigned char>(text[k]);
    };
    std::vector<uint32_t> cps;
    cps.reserve(text.size());
    size_t i = 0;
    while (i < text.size()) {
        const unsigned char b0 = byte(i);
        uint32_t cp = 0xFFFD;   // replacement char for anything malformed
        size_t len = 1;
        // A sequence is consumed only when every continuation byte exists and
        // has the 10xxxxxx form; otherwise emit FFFD for the lead byte alone
        // and resync at the next byte.
        const auto complete = [&](size_t n, uint32_t min) {
            if (i + n > text.size()) return false;
            uint32_t v = b0 & (0x7F >> n);
            for (size_t k = 1; k < n; ++k) {
                if ((byte(i + k) & 0xC0) != 0x80) return false;
                v = (v << 6) | (byte(i + k) & 0x3F);
            }
            // Unicode scalar values only: range floor, surrogates, and the
            // >U+10FFFF cap (a 4-byte sequence like F4 90 80 80 overflows).
            if (v < min || (v >= 0xD800 && v <= 0xDFFF) || v > 0x10FFFF) {
                return false;
            }
            cp = v;
            len = n;
            return true;
        };
        if (b0 < 0x80) {
            cp = b0;
        } else if ((b0 & 0xE0) == 0xC0) {
            complete(2, 0x80);
        } else if ((b0 & 0xF0) == 0xE0) {
            complete(3, 0x800);
        } else if ((b0 & 0xF8) == 0xF0) {
            complete(4, 0x10000);
        }
        cps.push_back(cp);
        i += len;
    }
    return cps;
}

// Copy rows rule (spec §2): per row take the cells up to and including the
// LAST non-empty cell (cp != 0); a row without any non-empty cell contributes
// an empty line. Rows are joined with "\n" — empty rows are never skipped
// (Windows Terminal convention). Wide-cell followers (width==0, cp==0)
// terminate the row extraction naturally: the wide glyph itself is the last
// non-empty cell.
//
// CellFn is a generic row-cell accessor: cellAt(col, row) -> const
// JKTermCell&, so the view passes the live grid and the self-test a dummy
// vector. (Scrollback selection is out of scope for v1 — spec §5.)
template <typename CellFn>
std::string ExtractSelectedText(CellFn&& cellAt, const JKTermSelRect& sel) {
    std::string out;
    if (sel.Empty()) return out;
    for (int r = sel.y0; r <= sel.y1; ++r) {
        if (r > sel.y0) out += '\n';
        int last = -1;
        for (int c = sel.x1; c >= sel.x0; --c) {
            if (cellAt(c, r).cp != 0) {
                last = c;
                break;
            }
        }
        for (int c = sel.x0; c <= last; ++c) {
            AppendUtf8(out, cellAt(c, r).cp);
        }
    }
    return out;
}

// Paste sanitizer (spec §2): normalize "\r\n"/"\r" to "\n", strip ESC bytes
// (a clipboard payload is plain text, never a VT sequence), and — when the
// application enabled bracketed paste (DECSET 2004, tracked by JKVtParser) —
// wrap the sanitized payload in \x1b[200~ ... \x1b[201~ so the shell can
// distinguish a paste from typed keys.
inline std::string SanitizeClipboardPaste(const std::string& raw,
                                          bool bracketed) {
    std::string text;
    text.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        const char c = raw[i];
        if (c == '\r') {
            if (i + 1 < raw.size() && raw[i + 1] == '\n') ++i;
            text += '\n';
        } else if (c != '\x1b') {
            text += c;
        }
    }
    if (bracketed) {
        return std::string("\x1b[200~") + text + "\x1b[201~";
    }
    return text;
}

} // namespace jk

#endif // JKTERMSELECTION_H