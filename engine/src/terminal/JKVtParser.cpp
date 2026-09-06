#include <terminal/JKVtParser.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace jk {

namespace {

// 16-color palette (SGR 30-37/40-47 + bright 90-97/100-107), xterm-ish order:
// black, red, green, yellow, blue, magenta, cyan, white.
const uint32_t kPal16[16] = {
    0x000000, 0xcd0000, 0x00cd00, 0xcdcd00, 0x0000ee, 0xcd00cd, 0x00cdcd, 0xe5e5e5,
    0x7f7f7f, 0xff0000, 0x00ff00, 0xffff00, 0x5c5cff, 0xff00ff, 0x00ffff, 0xffffff,
};

uint32_t LookupColor256(int n) {
    if (n < 0)   return kTermDefaultColor;
    if (n < 16)  return kPal16[n];
    if (n < 232) {
        // 6x6x6 cube (indices 16..231).
        const int v = n - 16;
        auto level = [](int i) { return i == 0 ? 0 : 55 + i * 40; };
        const int r = level(v / 36);
        const int g = level((v / 6) % 6);
        const int b = level(v % 6);
        return (static_cast<uint32_t>(r) << 16) |
               (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
    }
    // Grayscale ramp 232-255: 8..238 step 10.
    const int gray = 8 + (n - 232) * 10;
    return (static_cast<uint32_t>(gray) << 16) |
           (static_cast<uint32_t>(gray) << 8) | static_cast<uint32_t>(gray);
}

// (UTF-8 decoding is inlined in HandleGroundByte: lead-byte payload bits are
// seeded from the lead byte and continuation bytes shift in six bits each.)

bool IsC0Control(uint8_t b) { return b < 0x20 || b == 0x7F; }

uint32_t ClampColor(int v) {
    return static_cast<uint32_t>(std::min(255, std::max(0, v)));
}

} // namespace

int JKVtParser::ParseParams(const std::string& s, int* out, int maxCount, int defVal) {
    int idx = 0;
    const char* p = s.c_str();
    while (*p && idx < maxCount) {
        if (*p == ';') {
            out[idx++] = defVal;
            ++p;
            continue;
        }
        char* end = nullptr;
        const long v = std::strtol(p, &end, 10);
        out[idx++] = static_cast<int>(v);
        if (!end || end == p) ++p;
        else p = end;
        if (*p == ';') ++p;
    }
    return idx;
}

void JKVtParser::HandleCodepoint(uint32_t cp) {
    if (!grid_) return;
    if (cp == 0) return;
    grid_->PutChar(cp);
}

void JKVtParser::ExecuteControl(uint8_t b) {
    if (!grid_) return;
    switch (b) {
        case 0x08: grid_->Backspace(); break;        // BS
        case 0x09: grid_->ForwardTab(); break;       // TAB
        case 0x0A:                                    // LF
        case 0x0B:                                    // VT
        case 0x0C: grid_->LineFeed(); break;         // FF
        case 0x0D: grid_->CarriageReturn(); break;   // CR
        case 0x07: break;                             // BEL (no bell, Phase 1)
        case 0x0E:                                    // SO
        case 0x0F: break;                             // SI — charset shift ignored
        default: break;                               // other C0: ignore
    }
}

void JKVtParser::HandleGroundByte(uint8_t b) {
    switch (b) {
        case 0x1B: state_ = State::Esc; return;
        case 0x18:                                   // CAN
        case 0x1A: return;                           // SUB — abort, stay grounded
        default: break;
    }
    if (IsC0Control(b)) {
        ExecuteControl(b);
        return;
    }
    // UTF-8 decoding into codepoints.
    if (utf8Need_ == 0) {
        if (b < 0x80) {
            HandleCodepoint(b);
            return;
        }
        const size_t total = (b & 0xE0) == 0xC0 ? 2
                           : (b & 0xF0) == 0xE0 ? 3
                           : (b & 0xF8) == 0xF0 ? 4 : 1;
        if (total == 1) { HandleCodepoint(0xFFFD); return; }
        // Lead byte accumulator: payload bits by sequence length.
        utf8Cp_ = static_cast<uint32_t>(b & (total == 2 ? 0x1F
                                            : total == 3 ? 0x0F : 0x07));
        utf8Need_ = static_cast<int>(total) - 1;
        return;
    }
    // Continuation bytes.
    if ((b & 0xC0) == 0x80) {
        utf8Cp_ = (utf8Cp_ << 6) | (b & 0x3F);
        if (--utf8Need_ == 0) {
            HandleCodepoint(utf8Cp_);
        }
    } else {
        utf8Need_ = 0;
        HandleCodepoint(0xFFFD);
        HandleGroundByte(b);   // reprocess the stray byte
    }
}

void JKVtParser::HandleEscByte(uint8_t b) {
    switch (b) {
        case '[': state_ = State::Csi; csiBuf_.clear(); return;
        case ']': state_ = State::Osc; oscBuf_.clear(); return;
        case '7': grid_->SaveCursor(); state_ = State::Ground; return;
        case '8': grid_->RestoreCursor(); state_ = State::Ground; return;
        case 'c': grid_->Reset(); state_ = State::Ground; return;
        case 'D': grid_->LineFeed(); state_ = State::Ground; return;          // IND
        case 'M': grid_->ReverseLineFeed(); state_ = State::Ground; return;   // RI
        case 'E': grid_->CarriageReturn(); grid_->LineFeed();                 // NEL
                  state_ = State::Ground; return;
        case 0x1B: state_ = State::Esc; return;      // ESC ESC — restart
        default:
            if (IsC0Control(b) && b != 0x18 && b != 0x1A) {
                ExecuteControl(b);                    // C0 runs inside ESC too
                return;                               // remain in ESC state
            }
            state_ = State::Ground;                   // unsupported: ignore
            return;
    }
}

void JKVtParser::HandleCsiByte(uint8_t b) {
    // Parameter bytes 0x30-0x3F, intermediates 0x20-0x2F, final 0x40-0x7E.
    if (b >= 0x40 && b <= 0x7E) {
        DispatchCsi(b);
        csiBuf_.clear();
        state_ = State::Ground;
        return;
    }
    if ((b >= 0x20 && b <= 0x3F) || b == ';') {
        csiBuf_.push_back(static_cast<char>(b));
        return;
    }
    if (IsC0Control(b)) {
        ExecuteControl(b);
        return;
    }
    state_ = State::Ground;   // stray byte: abort sequence
}

void JKVtParser::HandleOscEnd() {
    // OSC 0/2 ; title — BEL or ST (ESC \) terminated.
    if (grid_ && oscBuf_.size() >= 2 &&
        (oscBuf_[0] == '0' || oscBuf_[0] == '2') && oscBuf_[1] == ';') {
        grid_->SetTitle(oscBuf_.substr(2));
    }
    oscBuf_.clear();
}

void JKVtParser::DispatchCsi(uint8_t final_) {
    if (!grid_) return;
    int p[8] = { 0 };
    int count = 0;
    const std::string params = csiBuf_;
    const bool hasParams = !params.empty() && params != "?";
    const bool privateMode = !params.empty() && params[0] == '?';
    if (hasParams) {
        count = ParseParams(privateMode ? params.c_str() + 1 : params.c_str(), p, 8, 1);
    }
    const int n = std::max(1, p[0]);

    switch (final_) {
        case 'A': for (int i = 0; i < n; ++i) CursorUp(); break;
        case 'B': for (int i = 0; i < n; ++i) CursorDown(); break;
        case 'C': for (int i = 0; i < n; ++i) CursorForward(); break;
        case 'D': for (int i = 0; i < n; ++i) CursorBackward(); break;
        case 'E': for (int i = 0; i < n; ++i) CursorDown(); grid_->CarriageReturn(); break;
        case 'F': for (int i = 0; i < n; ++i) CursorUp(); grid_->CarriageReturn(); break;
        case 'G': MoveToColumn(std::max(1, p[0])); break;
        case 'd': MoveToRow(std::max(1, p[0])); break;
        case 'H':
        case 'f': MoveTo(std::max(1, p[0]), p[1] >= 1 ? p[1] : 1); break;
        case 'J': grid_->EraseDisplay(p[0] == 0 && !hasParams ? 0 : p[0]); break;
        case 'K': grid_->EraseLine(p[0] == 0 && !hasParams ? 0 : p[0]); break;
        case 'L': InsertLines(n); break;
        case 'M': DeleteLines(n); break;
        case 'P': DeleteChars(n); break;
        case '@': InsertChars(n); break;
        case 'X': EraseChars(n); break;
        case 'r':
            // "CSI r" (reset) leaves both params at the default 1 — pass the
            // grid height so the region resets to the full screen.
            grid_->SetScrollRegion(p[0], p[1] > p[0] ? p[1] : grid_->Rows());
            MoveTo(1, 1);
            break;
        case 'S': grid_->ScrollRegionUp(std::min(n, grid_->Rows())); break;
        case 'T': grid_->ScrollRegionDown(std::min(n, grid_->Rows())); break;
        case 's': grid_->SaveCursor(); break;
        case 'u': grid_->RestoreCursor(); break;
        case 'm': DispatchSgr(p, count); break;
        case 'n':  // DSR 6 → cursor position report
            if (p[0] == 6) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "\x1b[%d;%dR",
                              grid_->GetCursor().y + 1, grid_->GetCursor().x + 1);
                replies_ += buf;
            }
            break;
        case 'c':  // DA → VT102
            if (p[0] == 0) replies_ += "\x1b[?6c";
            break;
        case 'h':
        case 'l':
            if (privateMode) HandlePrivateMode(p, final_ == 'h');
            break;
        default: break;   // unsupported final byte: ignore
    }
}

void JKVtParser::HandlePrivateMode(const int* p, bool set) {
    if (!grid_) return;
    switch (p[0]) {
        case 25:   grid_->SetCursorVisible(set); break;
        case 1047: if (!set) grid_->EraseDisplay(2); else grid_->SetAltScreen(true); break;
        case 1048: if (set) grid_->SaveCursor(); else grid_->RestoreCursor(); break;
        case 1049:
            if (set) { grid_->SaveCursor(); grid_->SetAltScreen(true); grid_->EraseDisplay(2); }
            else     { grid_->SetAltScreen(false); grid_->RestoreCursor(); }
            break;
        case 2004: bracketedPaste_ = set; break;   // tracked only (Phase 2)
        default: break;
    }
}

// Cursor helpers (clamped to the grid).
void JKVtParser::CursorUp() {
    auto& c = grid_->MutableCursor();
    if (c.y > 0) --c.y;
    grid_->MarkAllDirty();
}
void JKVtParser::CursorDown() {
    auto& c = grid_->MutableCursor();
    const int bottom = grid_->Rows() - 1;
    if (c.y < bottom) ++c.y;
    grid_->MarkAllDirty();
}
void JKVtParser::CursorForward() {
    auto& c = grid_->MutableCursor();
    if (c.x < grid_->Cols() - 1) ++c.x;
    // Skip a wide glyph's width-0 follower so the cursor lands on real cells.
    while (c.x < grid_->Cols() - 1 && grid_->Cell(c.x, c.y).width == 0) ++c.x;
    grid_->MarkAllDirty();
}
void JKVtParser::CursorBackward() {
    auto& c = grid_->MutableCursor();
    if (c.x > 0) --c.x;
    while (c.x > 0 && grid_->Cell(c.x, c.y).width == 0) --c.x;
    grid_->MarkAllDirty();
}
void JKVtParser::MoveTo(int row1, int col1) {
    auto& c = grid_->MutableCursor();
    c.y = std::min(std::max(0, row1 - 1), grid_->Rows() - 1);
    c.x = std::min(std::max(0, col1 - 1), grid_->Cols() - 1);
    grid_->MarkAllDirty();
}
void JKVtParser::MoveToColumn(int col1) {
    auto& c = grid_->MutableCursor();
    c.x = std::min(std::max(0, col1 - 1), grid_->Cols() - 1);
    grid_->MarkAllDirty();
}
void JKVtParser::MoveToRow(int row1) {
    auto& c = grid_->MutableCursor();
    c.y = std::min(std::max(0, row1 - 1), grid_->Rows() - 1);
    grid_->MarkAllDirty();
}
void JKVtParser::InsertLines(int n) { (void)n; }   // conhost re-renders; rare
void JKVtParser::DeleteLines(int n) { (void)n; }
void JKVtParser::InsertChars(int n) { (void)n; }
void JKVtParser::DeleteChars(int n) { (void)n; }
void JKVtParser::EraseChars(int n) {
    if (!grid_) return;
    auto& cur = grid_->MutableCursor();
    for (int i = 0; i < std::max(1, n); ++i) {
        if (auto* cell = grid_->CellPtrAt(cur.x + i, cur.y)) {
            *cell = JKTermCell{ 0, grid_->curFg, grid_->curBg, 0, 1 };
        }
    }
    grid_->MarkRowDirty(cur.y);
}

void JKVtParser::DispatchSgr(const int* params, int count) {
    if (!grid_) return;
    if (count == 0) {
        grid_->curFg = kTermDefaultColor;
        grid_->curBg = kTermDefaultColor;
        grid_->curAttrs = 0;
        return;
    }
    for (int i = 0; i < count; ++i) {
        const int v = params[i];
        if (v == 0) {
            grid_->curFg = kTermDefaultColor;
            grid_->curBg = kTermDefaultColor;
            grid_->curAttrs = 0;
        } else if (v == 1) {
            grid_->curAttrs |= kTermBold;
        } else if (v == 4) {
            grid_->curAttrs |= kTermUnderline;
        } else if (v == 7) {
            grid_->curAttrs |= kTermReverse;
        } else if (v == 22) {
            grid_->curAttrs &= ~kTermBold;
        } else if (v == 24) {
            grid_->curAttrs &= ~kTermUnderline;
        } else if (v == 27) {
            grid_->curAttrs &= ~kTermReverse;
        } else if (v >= 30 && v <= 37) {
            grid_->curFg = kPal16[v - 30];
        } else if (v == 39) {
            grid_->curFg = kTermDefaultColor;
        } else if (v >= 40 && v <= 47) {
            grid_->curBg = kPal16[v - 40];
        } else if (v == 49) {
            grid_->curBg = kTermDefaultColor;
        } else if (v >= 90 && v <= 97) {
            grid_->curFg = kPal16[v - 90 + 8];
        } else if (v >= 100 && v <= 107) {
            grid_->curBg = kPal16[v - 100 + 8];
        } else if (v == 38 || v == 48) {
            uint32_t color = kTermDefaultColor;
            if (i + 1 < count) {
                if (params[i + 1] == 5 && i + 2 < count) {        // 256-color
                    color = LookupColor256(params[i + 2]);
                    i += 2;
                } else if (params[i + 1] == 2 && i + 4 < count) { // truecolor
                    color = (ClampColor(params[i + 2]) << 16) |
                            (ClampColor(params[i + 3]) << 8) |
                            ClampColor(params[i + 4]);
                    i += 4;
                }
            }
            if (v == 38) grid_->curFg = color; else grid_->curBg = color;
        }
        // 2/21/22 dim/faint variants and others fall through.
    }
}

void JKVtParser::Feed(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        const uint8_t b = data[i];
        switch (state_) {
            case State::Ground:
                HandleGroundByte(b);
                break;
            case State::Esc:
                HandleEscByte(b);
                break;
            case State::Csi:
                HandleCsiByte(b);
                break;
            case State::Osc:
                if (b == 0x07) {                 // BEL terminator
                    HandleOscEnd();
                    state_ = State::Ground;
                } else if (b == 0x1B) {          // ESC \ (ST) — swallow next
                    HandleOscEnd();
                    state_ = State::Ground;
                    if (i + 1 < len && data[i + 1] == '\\') ++i;
                } else {
                    oscBuf_.push_back(static_cast<char>(b));
                }
                break;
            case State::Utf8:
                HandleGroundByte(b);   // UTF-8 handled inline in ground
                break;
        }
    }
}

} // namespace jk