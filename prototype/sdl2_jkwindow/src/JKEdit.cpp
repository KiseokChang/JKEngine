#include <JKEdit.h>
#include <JKEvent.h>
#include <JKWindow.h>
#include <JKApplication.h>
#include <JKHangulUtil.h>
#include <JKPlatform.h>
#include <SDL.h>
#include <algorithm>
#include <cstring>

namespace jk {

JKEdit::JKEdit() = default;

JKEdit::JKEdit(const JKRect& rect, uint16_t controlId, size_t maxLength, bool multiLine)
    : maxLength_(maxLength), multiLine_(multiLine) {
    SetRect(rect);
    SetControlId(controlId);
    SetBackColor(255, 255, 255);
    SetTextColor(0, 0, 0);
    SetFocusable(true);
}

void JKEdit::SetText(const std::string& text) {
    buffer_ = text;
    if (buffer_.size() > maxLength_) buffer_.resize(maxLength_);
    cursorPos_ = 0;
    firstVisibleLine_ = 0;
    showCaret_ = true;
    composing_ = false;
    automata_.InitAutomata();
    ClearSelection();
}

void JKEdit::SetSelection(size_t start, size_t end) {
    if (start > buffer_.size()) start = buffer_.size();
    if (end > buffer_.size()) end = buffer_.size();
    selAnchor_ = start;
    cursorPos_ = end;
    hasSelection_ = (start != end);
}

void JKEdit::ClearSelection() {
    hasSelection_ = false;
    selAnchor_ = cursorPos_;
}

std::string JKEdit::GetSelectedText() const {
    if (!hasSelection_) return {};
    size_t a = std::min(selAnchor_, cursorPos_);
    size_t b = std::max(selAnchor_, cursorPos_);
    return buffer_.substr(a, b - a);
}

const std::string& JKEdit::GetText() const {
    return buffer_;
}

size_t JKEdit::GetLineCount() const {
    if (buffer_.empty()) return 1;
    size_t count = 1;
    for (char c : buffer_) if (c == '\n') ++count;
    return count;
}

size_t JKEdit::GetLineStart(size_t line) const {
    if (line == 0) return 0;
    size_t current = 0;
    for (size_t i = 0; i < buffer_.size(); ++i) {
        if (buffer_[i] == '\n') {
            ++current;
            if (current == line) return i + 1;
        }
    }
    return buffer_.size();
}

size_t JKEdit::GetLineEnd(size_t line) const {
    size_t start = GetLineStart(line);
    for (size_t i = start; i < buffer_.size(); ++i) {
        if (buffer_[i] == '\n') return i;
    }
    return buffer_.size();
}

size_t JKEdit::GetLineFromPos(size_t pos) const {
    size_t line = 0;
    for (size_t i = 0; i < pos && i < buffer_.size(); ++i) {
        if (buffer_[i] == '\n') ++line;
    }
    return line;
}

size_t JKEdit::GetColFromPos(size_t pos) const {
    size_t start = GetLineStart(GetLineFromPos(pos));
    return pos - start;
}

void JKEdit::ScrollToCursor() {
    if (!multiLine_) return;
    size_t line = GetLineFromPos(cursorPos_);
    const JKRect client = GetScreenClientRect();
    int32_t visibleLines = std::max(1, client.h / lineHeight_);
    if (line < firstVisibleLine_) firstVisibleLine_ = line;
    if (line >= firstVisibleLine_ + static_cast<size_t>(visibleLines))
        firstVisibleLine_ = line - static_cast<size_t>(visibleLines) + 1;
}

void JKEdit::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    dc.Box3D(client, 1, 255, 255, 255, 255, 255, 255, 0, 0, 0);

    JKRect inner = client;
    inner.x += 2; inner.y += 2;
    inner.w -= 4; inner.h -= 4;
    if (readOnly_) {
        dc.SetColor(240, 240, 240, 255);
    } else {
        dc.SetColor(backR_, backG_, backB_, 255);
    }
    dc.FillRect(inner);

    dc.SetTextColor(textR_, textG_, textB_);
    if (!multiLine_) {
        int32_t textY = inner.y + (inner.h - 16) / 2;
        size_t selA = std::min(selAnchor_, cursorPos_);
        size_t selB = std::max(selAnchor_, cursorPos_);

        if (hasSelection_ && selB > selA) {
            int32_t selX = inner.x + static_cast<int32_t>(selA * charWidth_);
            int32_t selW = static_cast<int32_t>((selB - selA) * charWidth_);
            if (selX < inner.x) {
                selW -= (inner.x - selX);
                selX = inner.x;
            }
            if (selX + selW > inner.x + inner.w) {
                selW = inner.x + inner.w - selX;
            }
            if (selW > 0) {
                dc.SetColor(0, 0, 128, 255);
                dc.FillRect(JKRect{ selX, textY, selW, 16 });
            }

            const char* buf = buffer_.c_str();
            dc.SetTextColor(textR_, textG_, textB_);
            if (selA > 0) {
                dc.TextOut(jk::JKPoint{ inner.x, textY }, selA, buf);
            }
            dc.SetTextColor(255, 255, 255);
            dc.TextOut(jk::JKPoint{ inner.x + static_cast<int32_t>(selA * charWidth_), textY },
                       selB - selA, buf + selA);
            dc.SetTextColor(textR_, textG_, textB_);
            if (selB < buffer_.size()) {
                dc.TextOut(jk::JKPoint{ inner.x + static_cast<int32_t>(selB * charWidth_), textY },
                           buffer_.size() - selB, buf + selB);
            }
        } else {
            dc.SetTextColor(textR_, textG_, textB_);
            dc.TextOut(jk::JKPoint{ inner.x, textY }, buffer_.c_str());
        }

        // Draw IME composition string at the caret position.
        if (focused_ && !compText_.empty()) {
            int32_t compX = inner.x + static_cast<int32_t>(cursorPos_ * charWidth_);
            int32_t compW = static_cast<int32_t>(compText_.size()) * charWidth_;
            dc.SetColor(0, 0, 255, 64);
            dc.FillRect(JKRect{ compX, textY, compW, 16 });
            dc.SetTextColor(textR_, textG_, textB_);
            dc.TextOut(jk::JKPoint{ compX, textY }, compText_.c_str());
        }

        if (focused_ && showCaret_) {
            int32_t caretX = inner.x + static_cast<int32_t>(cursorPos_ * charWidth_);
            int32_t caretY = textY;
            dc.SetColor(0, 0, 0, 255);
            dc.DrawLine(caretX, caretY, caretX, caretY + 12);

            // Additional caret inside the composition string.
            if (!compText_.empty()) {
                int32_t compCaretX = caretX + static_cast<int32_t>(compCursor_ * charWidth_);
                dc.SetColor(255, 0, 0, 255);
                dc.DrawLine(compCaretX, caretY, compCaretX, caretY + 12);
            }
        }
    } else {
        int32_t visibleLines = std::max(1, inner.h / lineHeight_);
        size_t lineCount = GetLineCount();
        size_t lastLine = std::min(lineCount, firstVisibleLine_ + static_cast<size_t>(visibleLines));
        for (size_t line = firstVisibleLine_; line < lastLine; ++line) {
            size_t start = GetLineStart(line);
            size_t end = GetLineEnd(line);
            std::string_view view(buffer_.data() + start, end - start);
            dc.TextOut(jk::JKPoint{ inner.x, inner.y + static_cast<int32_t>((line - firstVisibleLine_) * lineHeight_) },
                       std::string(view).c_str());
        }
        // Draw IME composition string at the caret position.
        if (focused_ && !compText_.empty()) {
            size_t line = GetLineFromPos(cursorPos_);
            size_t col = GetColFromPos(cursorPos_);
            int32_t compX = inner.x + static_cast<int32_t>(col * charWidth_);
            int32_t compY = inner.y + static_cast<int32_t>((line - firstVisibleLine_) * lineHeight_) + 2;
            int32_t compW = static_cast<int32_t>(compText_.size()) * charWidth_;
            dc.SetColor(0, 0, 255, 64);
            dc.FillRect(JKRect{ compX, compY, compW, 16 });
            dc.SetTextColor(textR_, textG_, textB_);
            dc.TextOut(jk::JKPoint{ compX, compY }, compText_.c_str());
        }

        if (focused_ && showCaret_) {
            size_t line = GetLineFromPos(cursorPos_);
            size_t col = GetColFromPos(cursorPos_);
            int32_t caretX = inner.x + static_cast<int32_t>(col * charWidth_);
            int32_t caretY = inner.y + static_cast<int32_t>((line - firstVisibleLine_) * lineHeight_) + 2;
            dc.SetColor(0, 0, 0, 255);
            dc.DrawLine(caretX, caretY, caretX, caretY + 12);

            if (!compText_.empty()) {
                int32_t compCaretX = caretX + static_cast<int32_t>(compCursor_ * charWidth_);
                dc.SetColor(255, 0, 0, 255);
                dc.DrawLine(compCaretX, caretY, compCaretX, caretY + 12);
            }
        }
    }

    JKControl::OnPaintClient(dc);
}

void JKEdit::OnSetFocus() {
    focused_ = true;
    showCaret_ = true;
    DetectWindowsImeState();
    UpdateTextInputRect();
}

void JKEdit::OnKillFocus() {
    focused_ = false;
    showCaret_ = false;
    if (imeComposing_ && g_currentJKApp) {
        // Ask the OS IME to flush the composed string first, then fall back to
        // a local commit if the OS did not deliver a TEXTINPUT event in time.
        SDL_Window* window = g_currentJKApp->GetSdlWindow();
        if (window) JKPlatform::CompleteComposition(window);
        CommitComposition();
    }
}

void JKEdit::UpdateTextInputRect() {
    if (!g_currentJKApp) return;
    SDL_Window* window = g_currentJKApp->GetSdlWindow();
    if (!window) return;
    const JKRect client = GetScreenClientRect();
    SDL_Rect rect{ client.x, client.y, client.w, client.h };
    SDL_SetTextInputRect(&rect);
}

void JKEdit::DetectWindowsImeState() {
#ifdef _WIN32
    if (!g_currentJKApp) return;
    SDL_Window* window = g_currentJKApp->GetSdlWindow();
    if (!window) return;
    JKPlatform::ImeMode mode = JKPlatform::GetCurrentConversionMode(window);
    if (mode == JKPlatform::ImeMode::Hangul) {
        inputMode_ = InputMode::ImeHangul;
    } else if (mode == JKPlatform::ImeMode::Ascii) {
        inputMode_ = InputMode::Ascii;
    }
    // JKPlatform::ImeMode::Unknown leaves the current mode unchanged (non-Windows or no IME).
#else
    // On non-Windows platforms, leave the mode as-is and let F2 toggle.
    (void)0;
#endif
}

void JKEdit::CommitComposition() {
    if (!imeComposing_ || compText_.empty()) return;
    InsertKssmText(compText_.c_str());
    compText_.clear();
    compCursor_ = 0;
    imeComposing_ = false;
}

void JKEdit::RespondMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::MouseDown) {
        SetFocus();
        focused_ = true;
        showCaret_ = true;

        // If the IME was composing when the user clicked, ask the OS to commit
        // the string before we move the caret or change selection.
        if (imeComposing_ && g_currentJKApp) {
            SDL_Window* window = g_currentJKApp->GetSdlWindow();
            if (window) JKPlatform::CompleteComposition(window);
        }

        size_t oldPos = cursorPos_;
        cursorPos_ = PixelToPos(ev.x, ev.y);
        bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        if (shift) {
            if (!hasSelection_) selAnchor_ = oldPos;
            hasSelection_ = true;
        } else {
            selAnchor_ = cursorPos_;
            hasSelection_ = false;
        }
        mouseAnchor_ = selAnchor_;
        mouseSelecting_ = true;
        ScrollToCursor();
        if (g_currentJKApp) g_currentJKApp->SetCapture(this);
    } else if (ev.type == JKEventType::MouseMove) {
        if (mouseSelecting_) {
            cursorPos_ = PixelToPos(ev.x, ev.y);
            selAnchor_ = mouseAnchor_;
            hasSelection_ = (cursorPos_ != selAnchor_);
            ScrollToCursor();
        }
    } else if (ev.type == JKEventType::MouseUp) {
        mouseSelecting_ = false;
        if (g_currentJKApp && g_currentJKApp->GetCapture() == this) {
            g_currentJKApp->ReleaseCapture();
        }
    } else if (ev.type == JKEventType::Timer) {
        if (focused_) showCaret_ = !showCaret_;
    } else if (ev.type == JKEventType::KeyDown) {
        SDL_Keymod mod = SDL_GetModState();
        bool ctrl = (mod & KMOD_CTRL) != 0;
        bool shift = (mod & KMOD_SHIFT) != 0;

        if (readOnly_) {
            // Read-only edits allow navigation and copy, but no modification.
            if (ctrl && ev.keyCode == SDLK_a) {
                SetSelection(0, buffer_.size());
                showCaret_ = true;
            } else if (ctrl && ev.keyCode == SDLK_c) {
                CopyToClipboard();
            } else {
                size_t oldPos = cursorPos_;
                switch (ev.keyCode) {
                    case SDLK_LEFT:     MoveCursorLeft(); break;
                    case SDLK_RIGHT:    MoveCursorRight(); break;
                    case SDLK_UP:       if (multiLine_) MoveCursorUp(); break;
                    case SDLK_DOWN:     if (multiLine_) MoveCursorDown(); break;
                    case SDLK_HOME:     MoveCursorHome(); break;
                    case SDLK_END:      MoveCursorEnd(); break;
                    case SDLK_PAGEUP:   if (multiLine_) MoveCursorPageUp(); break;
                    case SDLK_PAGEDOWN: if (multiLine_) MoveCursorPageDown(); break;
                    default: break;
                }
                UpdateSelection(oldPos, shift);
            }
            return;
        }

        if (ctrl && ev.keyCode == SDLK_a) {
            SetSelection(0, buffer_.size());
            showCaret_ = true;
        } else if (ctrl && ev.keyCode == SDLK_c) {
            CopyToClipboard();
        } else if (ctrl && ev.keyCode == SDLK_x) {
            CutToClipboard();
        } else if (ctrl && ev.keyCode == SDLK_v) {
            PasteFromClipboard();
        } else if (ev.keyCode == SDLK_F2) {
            if (!imeComposing_) {
                ToggleHangulMode();
                // When the user switches to the internal automata, force the OS
                // IME into ASCII mode so both systems do not compose at the same
                // time and create duplicate characters.
                if (inputMode_ == InputMode::InternalHangul && g_currentJKApp) {
                    SDL_Window* window = g_currentJKApp->GetSdlWindow();
                    if (window) JKPlatform::SetConversionMode(window, JKPlatform::ImeMode::Ascii);
                }
            }
        } else if (inputMode_ == InputMode::InternalHangul &&
                   !imeComposing_ &&
                   ev.keyCode >= SDLK_a && ev.keyCode <= SDLK_z) {
            ProcessHangulKey(static_cast<uint16_t>(ev.keyCode));
        } else {
            // While the OS IME is composing, let the IME own navigation and
            // editing keys. Handling them ourselves would delete or move the
            // cursor underneath the active composition and corrupt the input.
            if (imeComposing_) {
                switch (ev.keyCode) {
                    case SDLK_LEFT:
                    case SDLK_RIGHT:
                    case SDLK_UP:
                    case SDLK_DOWN:
                    case SDLK_HOME:
                    case SDLK_END:
                    case SDLK_PAGEUP:
                    case SDLK_PAGEDOWN:
                    case SDLK_BACKSPACE:
                    case SDLK_DELETE:
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        return;
                    default: break;
                }
            }
            size_t oldPos = cursorPos_;
            switch (ev.keyCode) {
                case SDLK_LEFT:     MoveCursorLeft(); break;
                case SDLK_RIGHT:    MoveCursorRight(); break;
                case SDLK_UP:       if (multiLine_) MoveCursorUp(); break;
                case SDLK_DOWN:     if (multiLine_) MoveCursorDown(); break;
                case SDLK_HOME:     MoveCursorHome(); break;
                case SDLK_END:      MoveCursorEnd(); break;
                case SDLK_PAGEUP:   if (multiLine_) MoveCursorPageUp(); break;
                case SDLK_PAGEDOWN: if (multiLine_) MoveCursorPageDown(); break;
                case SDLK_BACKSPACE: DeleteBackward(); break;
                case SDLK_DELETE:    DeleteForward(); break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER: if (multiLine_) ProcessReturn(); break;
                default: break;
            }
            UpdateSelection(oldPos, shift);
        }
    } else if (ev.type == JKEventType::TextEditing) {
        if (readOnly_) return;
        // SDL IME composition event. Convert the UTF-8 pre-edit string to KSSM
        // and store it for rendering. The actual commit happens on TEXTINPUT.
        compText_ = Utf8ToKssm(ev.text);
        compCursor_ = static_cast<size_t>(ev.editStart);
        if (compCursor_ > compText_.size()) compCursor_ = compText_.size();
        imeComposing_ = !compText_.empty();
        showCaret_ = true;
        UpdateTextInputRect();
    } else if (ev.type == JKEventType::Char) {
        if (readOnly_) return;
        // SDL_TEXTINPUT carries the IME's committed string. It replaces any
        // pending composition state, so clear the pre-edit visual state without
        // committing it locally; doing so would duplicate the composed text
        // when TEXTEDITING and TEXTINPUT arrive in opposite orders.
        compText_.clear();
        compCursor_ = 0;
        imeComposing_ = false;
        if (inputMode_ == InputMode::InternalHangul) {
            // In internal automata mode ASCII letters are handled by KeyDown.
            // Punctuation, digits, and space still come through TEXTINPUT.
            // Non-ASCII text from an external IME is converted to KSSM so it
            // is not silently dropped.
            unsigned char c = static_cast<unsigned char>(ev.text[0]);
            if (c < 0x80 && !std::isalpha(static_cast<int>(c))) {
                InsertText(ev.text);
            } else if (c >= 0x80) {
                InsertKssmText(Utf8ToKssm(ev.text).c_str());
            }
        } else {
            InsertKssmText(Utf8ToKssm(ev.text).c_str());
        }
        UpdateTextInputRect();
    } else {
        JKControl::RespondMessage(ev);
    }
}

void JKEdit::InsertText(const char* text) {
    if (!text || !text[0]) return;
    size_t len = std::strlen(text);
    DeleteSelection();
    if (buffer_.size() + len > maxLength_) {
        len = maxLength_ - buffer_.size();
    }
    if (len == 0) return;
    buffer_.insert(cursorPos_, text, len);
    cursorPos_ += len;
    ClearSelection();
    ScrollToCursor();
    showCaret_ = true;
}

void JKEdit::InsertKssmChar(uint16_t code) {
    if (buffer_.size() + 2 > maxLength_) return;
    char pair[2] = { static_cast<char>(code >> 8), static_cast<char>(code & 0xFF) };
    buffer_.insert(cursorPos_, pair, 2);
    cursorPos_ += 2;
    ScrollToCursor();
    showCaret_ = true;
}

void JKEdit::InsertKssmText(const char* text) {
    if (!text || !text[0]) return;
    size_t len = std::strlen(text);
    if (buffer_.size() + len > maxLength_) {
        len = maxLength_ - buffer_.size();
    }
    if (len == 0) return;
    buffer_.insert(cursorPos_, text, len);
    cursorPos_ += len;
    ClearSelection();
    ScrollToCursor();
    showCaret_ = true;
}

void JKEdit::ProcessHangulKey(uint16_t keyCode) {
    uint16_t converted = automata_.ConvertKey(keyCode, 0);
    bool complete = automata_.Automata(converted);

    if (complete) {
        // 조합 중이던 문자를 먼저 제거한다.
        if (composing_ && cursorPos_ >= 2) {
            buffer_.erase(cursorPos_ - 2, 2);
            cursorPos_ -= 2;
            composing_ = false;
        }
        // 완료된 문자(들)를 출력한다.
        for (uint16_t i = 0; i < automata_.outSP; ++i) {
            InsertKssmChar(automata_.outStack[i]);
        }
        automata_.InitAutomata();
    }

    if (automata_.curHanState && automata_.charCode != 0x8441) {
        if (composing_ && cursorPos_ >= 2) {
            cursorPos_ -= 2;
            buffer_[cursorPos_]     = static_cast<char>(automata_.charCode >> 8);
            buffer_[cursorPos_ + 1] = static_cast<char>(automata_.charCode & 0xFF);
            cursorPos_ += 2;
        } else {
            InsertKssmChar(automata_.charCode);
            composing_ = true;
        }
    }
}

bool JKEdit::DeleteSelection() {
    if (!hasSelection_) return false;
    size_t a = std::min(selAnchor_, cursorPos_);
    size_t b = std::max(selAnchor_, cursorPos_);
    buffer_.erase(a, b - a);
    cursorPos_ = a;
    ClearSelection();
    ScrollToCursor();
    showCaret_ = true;
    return true;
}

void JKEdit::DeleteBackward() {
    if (DeleteSelection()) return;
    if (cursorPos_ == 0) return;
    size_t prev = cursorPos_ - 1;
    while (prev > 0 && (static_cast<uint8_t>(buffer_[prev]) & 0xC0) == 0x80) --prev;
    buffer_.erase(prev, cursorPos_ - prev);
    cursorPos_ = prev;
    ScrollToCursor();
    showCaret_ = true;
}

void JKEdit::DeleteForward() {
    if (DeleteSelection()) return;
    if (cursorPos_ >= buffer_.size()) return;
    size_t next = cursorPos_ + 1;
    while (next < buffer_.size() &&
           (static_cast<uint8_t>(buffer_[next]) & 0xC0) == 0x80) ++next;
    buffer_.erase(cursorPos_, next - cursorPos_);
    showCaret_ = true;
}

void JKEdit::MoveCursorLeft() {
    if (cursorPos_ == 0) return;
    --cursorPos_;
    while (cursorPos_ > 0 &&
           (static_cast<uint8_t>(buffer_[cursorPos_]) & 0xC0) == 0x80) --cursorPos_;
    ScrollToCursor();
    showCaret_ = true;
}

void JKEdit::MoveCursorRight() {
    if (cursorPos_ >= buffer_.size()) return;
    ++cursorPos_;
    while (cursorPos_ < buffer_.size() &&
           (static_cast<uint8_t>(buffer_[cursorPos_]) & 0xC0) == 0x80) ++cursorPos_;
    ScrollToCursor();
    showCaret_ = true;
}

void JKEdit::MoveCursorHome() {
    cursorPos_ = GetLineStart(GetLineFromPos(cursorPos_));
    ScrollToCursor();
    showCaret_ = true;
}

void JKEdit::MoveCursorEnd() {
    cursorPos_ = GetLineEnd(GetLineFromPos(cursorPos_));
    ScrollToCursor();
    showCaret_ = true;
}

void JKEdit::MoveCursorUp() {
    if (cursorPos_ == 0) return;
    size_t line = GetLineFromPos(cursorPos_);
    if (line == 0) { cursorPos_ = 0; }
    else {
        size_t col = GetColFromPos(cursorPos_);
        size_t prevStart = GetLineStart(line - 1);
        size_t prevEnd = GetLineEnd(line - 1);
        cursorPos_ = std::min(prevStart + col, prevEnd);
    }
    ScrollToCursor();
    showCaret_ = true;
}

void JKEdit::MoveCursorDown() {
    size_t line = GetLineFromPos(cursorPos_);
    if (line + 1 >= GetLineCount()) { cursorPos_ = buffer_.size(); }
    else {
        size_t col = GetColFromPos(cursorPos_);
        size_t nextStart = GetLineStart(line + 1);
        size_t nextEnd = GetLineEnd(line + 1);
        cursorPos_ = std::min(nextStart + col, nextEnd);
    }
    ScrollToCursor();
    showCaret_ = true;
}

void JKEdit::MoveCursorPageUp() {
    const JKRect client = GetScreenClientRect();
    int32_t visibleLines = std::max(1, client.h / lineHeight_);
    size_t line = GetLineFromPos(cursorPos_);
    if (line < static_cast<size_t>(visibleLines)) cursorPos_ = 0;
    else cursorPos_ = GetLineStart(line - static_cast<size_t>(visibleLines));
    firstVisibleLine_ = GetLineFromPos(cursorPos_);
    showCaret_ = true;
}

void JKEdit::MoveCursorPageDown() {
    const JKRect client = GetScreenClientRect();
    int32_t visibleLines = std::max(1, client.h / lineHeight_);
    size_t line = GetLineFromPos(cursorPos_);
    size_t total = GetLineCount();
    if (line + static_cast<size_t>(visibleLines) >= total) cursorPos_ = buffer_.size();
    else cursorPos_ = GetLineStart(line + static_cast<size_t>(visibleLines));
    firstVisibleLine_ = GetLineFromPos(cursorPos_);
    showCaret_ = true;
}

void JKEdit::ProcessReturn() {
    InsertText("\n");
}

size_t JKEdit::PixelToPos(int32_t x, int32_t y) const {
    const JKRect client = GetScreenClientRect();
    JKRect inner = client;
    inner.x += 2; inner.y += 2;
    inner.w -= 4; inner.h -= 4;
    if (multiLine_) {
        int32_t relX = x - inner.x;
        int32_t relY = y - inner.y;
        size_t line = firstVisibleLine_ + static_cast<size_t>(std::max(0, relY / lineHeight_));
        size_t lineCount = GetLineCount();
        if (line >= lineCount) line = lineCount - 1;
        size_t start = GetLineStart(line);
        size_t end = GetLineEnd(line);
        int32_t col = std::max(0, relX / charWidth_);
        size_t pos = start + static_cast<size_t>(col);
        if (pos > end) pos = end;
        return pos;
    } else {
        int32_t relX = x - inner.x;
        int32_t col = relX / charWidth_;
        if (col < 0) col = 0;
        size_t pos = static_cast<size_t>(col);
        if (pos > buffer_.size()) pos = buffer_.size();
        return pos;
    }
}

void JKEdit::UpdateSelection(size_t oldPos, bool shift) {
    if (shift) {
        if (cursorPos_ != oldPos) {
            if (!hasSelection_) selAnchor_ = oldPos;
            hasSelection_ = true;
        }
    } else {
        if (cursorPos_ != oldPos || hasSelection_) {
            ClearSelection();
        }
    }
    ScrollToCursor();
    showCaret_ = true;
}

void JKEdit::CopyToClipboard() {
    std::string selected = GetSelectedText();
    if (!selected.empty()) {
        SDL_SetClipboardText(selected.c_str());
    }
}

void JKEdit::CutToClipboard() {
    CopyToClipboard();
    DeleteSelection();
}

void JKEdit::PasteFromClipboard() {
    if (SDL_HasClipboardText()) {
        char* text = SDL_GetClipboardText();
        if (text) {
            InsertText(text);
            SDL_free(text);
        }
    }
}

} // namespace jk
