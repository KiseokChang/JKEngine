#include <JKEdit.h>
#include <JKEvent.h>
#include <JKWindow.h>
#include <JKApplication.h>
#include <JKHangulUtil.h>
#include <JKPlatform.h>
#include <theme/JKTheme.h>
#include <SDL.h>
#include <algorithm>
#include <cstring>

namespace jk {

JKEdit::JKEdit() = default;

JKEdit::JKEdit(const JKRect& rect, uint16_t controlId, size_t maxLength, bool multiLine)
    : maxLength_(maxLength), multiLine_(multiLine) {
    SetRect(rect);
    SetControlId(controlId);
    const auto& t = jk::theme::current();
    SetBackColor(t.fieldBg.r, t.fieldBg.g, t.fieldBg.b);
    SetTextColor(t.widgetText.r, t.widgetText.g, t.widgetText.b);
    SetFocusable(true);
}

void JKEdit::SetText(const std::string& text) {
    buffer_ = text;
    if (buffer_.size() > maxLength_) buffer_.resize(maxLength_);
    cursorPos_ = 0;
    firstVisibleLine_ = 0;
    firstVisibleCol_ = 0;
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
    ScrollToCursor();
    showCaret_ = true;
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

// 표시 셀 매핑 (docs/60 §10). 셀 = 8px 한 칸: ASCII 바이트 1개 = 1셀,
// KSSM 2바이트 쌍 = 2셀(JKDC::TextOut이 16px 전진). 범위 경계가 쌍 중간에
// 걸리면 그 바이트는 1셀로 계산된다(호출부는 쌍 경계 좌표만 넘긴다).
size_t JKEdit::DisplayCells(const std::string& buf, size_t from, size_t to) {
    if (to > buf.size()) to = buf.size();
    size_t cells = 0;
    size_t i = from;
    while (i < to) {
        if (static_cast<uint8_t>(buf[i]) >= 0x80) { cells += 2; i += 2; }
        else { cells += 1; i += 1; }
    }
    return cells;
}

// [from, to) 바이트 구간에서 cells셀 만큼 전진한 바이트 위치. KSSM 쌍은
// 통째로 전진하므로 반환값은 항상 글리프 경계(쌍 중간에 걸리지 않음).
size_t JKEdit::PosFromCells(const std::string& buf, size_t from, size_t to, size_t cells) {
    if (to > buf.size()) to = buf.size();
    size_t i = from;
    size_t c = 0;
    while (i < to && c < cells) {
        if (static_cast<uint8_t>(buf[i]) >= 0x80) { c += 2; i += 2; }
        else { c += 1; i += 1; }
    }
    return (i > to) ? to : i;
}

void JKEdit::ScrollToCursor() {
    const JKRect client = GetScreenClientRect();
    if (!multiLine_) {
        // 수평 스크롤: 캐럿 셀이 표시 밴드 밖이면 밴드를 민다 (docs/60 §10).
        int32_t visibleCells = std::max(1, (client.w - 4) / charWidth_);
        size_t caretCell = DisplayCells(buffer_, 0, cursorPos_);
        if (caretCell < firstVisibleCol_) firstVisibleCol_ = caretCell;
        if (caretCell >= firstVisibleCol_ + static_cast<size_t>(visibleCells))
            firstVisibleCol_ = caretCell - static_cast<size_t>(visibleCells) + 1;
        return;
    }
    size_t line = GetLineFromPos(cursorPos_);
    int32_t visibleLines = std::max(1, client.h / lineHeight_);
    if (line < firstVisibleLine_) firstVisibleLine_ = line;
    if (line >= firstVisibleLine_ + static_cast<size_t>(visibleLines))
        firstVisibleLine_ = line - static_cast<size_t>(visibleLines) + 1;
}

void JKEdit::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    const auto& t = jk::theme::current();
    dc.Box3D(client, 1,
             t.fieldBg.r, t.fieldBg.g, t.fieldBg.b,
             t.bevelLight.r, t.bevelLight.g, t.bevelLight.b,
             t.bevelDark.r, t.bevelDark.g, t.bevelDark.b);

    JKRect inner = client;
    inner.x += 2; inner.y += 2;
    inner.w -= 4; inner.h -= 4;
    if (readOnly_) {
        dc.SetColor(t.widgetFace.r, t.widgetFace.g, t.widgetFace.b, 255);
    } else {
        dc.SetColor(backR_, backG_, backB_, 255);
    }
    dc.FillRect(inner);

    dc.SetTextColor(textR_, textG_, textB_);
    if (!multiLine_) {
        int32_t textY = inner.y + (inner.h - 16) / 2;
        // 수평 스크롤 오프셋: 화면의 첫 바이트 off, x는 off 기준 셀 수로 환산
        // (docs/60 §10 — 바이트×charWidth_는 KSSM에서 밀린다).
        size_t off = PosFromCells(buffer_, 0, buffer_.size(), firstVisibleCol_);
        auto xOf = [&](size_t bytePos) -> int32_t {
            return inner.x + static_cast<int32_t>(
                       DisplayCells(buffer_, off, bytePos) * static_cast<size_t>(charWidth_));
        };
        size_t selA = std::min(selAnchor_, cursorPos_);
        size_t selB = std::max(selAnchor_, cursorPos_);

        if (hasSelection_ && selB > selA) {
            // 선택 밴드 — off 이전/이후 부분 잘라내기.
            if (selA < off) selA = off;
            if (selB < off) selB = off;
            if (selB > selA) {
                int32_t selX = xOf(selA);
                int32_t selW = static_cast<int32_t>(
                    (DisplayCells(buffer_, selA, selB)) * static_cast<size_t>(charWidth_));
                if (selX + selW > inner.x + inner.w)
                    selW = inner.x + inner.w - selX;
                if (selW > 0) {
                    dc.SetColor(t.selectionBg.r, t.selectionBg.g, t.selectionBg.b, 255);
                    dc.FillRect(JKRect{ selX, textY, selW, 16 });
                }

                const char* buf = buffer_.c_str();
                dc.SetTextColor(textR_, textG_, textB_);
                if (off < selA) {
                    dc.TextOut(jk::JKPoint{ inner.x, textY }, selA - off, buf + off);
                }
                dc.SetTextColor(t.selectionText.r, t.selectionText.g, t.selectionText.b);
                dc.TextOut(jk::JKPoint{ xOf(selA), textY }, selB - selA, buf + selA);
                dc.SetTextColor(textR_, textG_, textB_);
                if (selB < buffer_.size()) {
                    dc.TextOut(jk::JKPoint{ xOf(selB), textY },
                               buffer_.size() - selB, buf + selB);
                }
            } else {
                // 선택이 완전히 좌측 밖 — 일반 렌더로 폴백.
                dc.TextOut(jk::JKPoint{ inner.x, textY }, buffer_.c_str() + off);
            }
        } else {
            dc.SetTextColor(textR_, textG_, textB_);
            dc.TextOut(jk::JKPoint{ inner.x, textY }, buffer_.c_str() + off);
        }

        // Draw IME composition string at the caret position.
        if (focused_ && !compText_.empty()) {
            int32_t compX = xOf(cursorPos_);
            int32_t compW = static_cast<int32_t>(
                DisplayCells(compText_, 0, compText_.size()) * static_cast<size_t>(charWidth_));
            // IME 조합 배경 — 토큰 rgb, 알파 64 고정 보존 (스펙 §1c).
            dc.SetColor(t.imeCompositionBg.r, t.imeCompositionBg.g, t.imeCompositionBg.b, 64);
            dc.FillRect(JKRect{ compX, textY, compW, 16 });
            dc.SetTextColor(textR_, textG_, textB_);
            dc.TextOut(jk::JKPoint{ compX, textY }, compText_.c_str());
        }

        if (focused_ && showCaret_) {
            int32_t caretX = xOf(cursorPos_);
            int32_t caretY = textY;
            dc.SetColor(t.widgetText.r, t.widgetText.g, t.widgetText.b, 255);
            dc.DrawLine(caretX, caretY, caretX, caretY + 12);

            // Additional caret inside the composition string.
            if (!compText_.empty()) {
                int32_t compCaretX = caretX + static_cast<int32_t>(
                    DisplayCells(compText_, 0, compCursor_) * static_cast<size_t>(charWidth_));
                dc.SetColor(t.imeCaret.r, t.imeCaret.g, t.imeCaret.b, 255);
                dc.DrawLine(compCaretX, caretY, compCaretX, caretY + 12);
            }
        }
    } else {
        int32_t visibleLines = std::max(1, inner.h / lineHeight_);
        size_t lineCount = GetLineCount();
        size_t lastLine = std::min(lineCount, firstVisibleLine_ + static_cast<size_t>(visibleLines));
        size_t selA = std::min(selAnchor_, cursorPos_);
        size_t selB = std::max(selAnchor_, cursorPos_);
        // 가로 클리핑 (docs/61 §2): 상자보다 긴 라인이 경계 밖으로 흘러넘치지
        // 않게 표시 셀 폭으로 렌더 범위를 절단한다(멀티라인은 가로 스크롤 없음).
        const size_t maxCells = static_cast<size_t>(std::max(1, inner.w / charWidth_));
        for (size_t line = firstVisibleLine_; line < lastLine; ++line) {
            size_t start = GetLineStart(line);
            size_t end = GetLineEnd(line);
            size_t clipEnd = PosFromCells(buffer_, start, end, maxCells);
            int32_t lineY = inner.y + static_cast<int32_t>((line - firstVisibleLine_) * lineHeight_);
            // 선택 렌더 (docs/60 §10 — 대장 기존 결함: 멀티라인은 선택을
            // 렌더하지 않았다). 라인과 선택 구간의 교집합을 3분할 색상으로.
            size_t a = selA, b = selB;
            if (a < start) a = start;
            if (b > clipEnd) b = clipEnd;
            if (hasSelection_ && a < b) {
                auto xOf = [&](size_t bytePos) -> int32_t {
                    return inner.x + static_cast<int32_t>(
                               DisplayCells(buffer_, start, bytePos) * static_cast<size_t>(charWidth_));
                };
                const char* buf = buffer_.c_str();
                dc.SetTextColor(textR_, textG_, textB_);
                if (start < a) {
                    dc.TextOut(jk::JKPoint{ inner.x, lineY }, a - start, buf + start);
                }
                int32_t selX = xOf(a);
                int32_t selW = static_cast<int32_t>(
                    DisplayCells(buffer_, a, b) * static_cast<size_t>(charWidth_));
                dc.SetColor(t.selectionBg.r, t.selectionBg.g, t.selectionBg.b, 255);
                dc.FillRect(JKRect{ selX, lineY, selW, lineHeight_ });
                dc.SetTextColor(t.selectionText.r, t.selectionText.g, t.selectionText.b);
                dc.TextOut(jk::JKPoint{ selX, lineY }, b - a, buf + a);
                dc.SetTextColor(textR_, textG_, textB_);
                if (b < clipEnd) {
                    dc.TextOut(jk::JKPoint{ xOf(b), lineY }, clipEnd - b, buf + b);
                }
            } else {
                std::string_view view(buffer_.data() + start, clipEnd - start);
                dc.TextOut(jk::JKPoint{ inner.x, lineY }, std::string(view).c_str());
            }
        }
        // Draw IME composition string at the caret position.
        if (focused_ && !compText_.empty()) {
            size_t line = GetLineFromPos(cursorPos_);
            size_t lineStart = GetLineStart(line);
            int32_t compX = inner.x + static_cast<int32_t>(
                DisplayCells(buffer_, lineStart, cursorPos_) * static_cast<size_t>(charWidth_));
            int32_t compY = inner.y + static_cast<int32_t>((line - firstVisibleLine_) * lineHeight_) + 2;
            int32_t compW = static_cast<int32_t>(
                DisplayCells(compText_, 0, compText_.size()) * static_cast<size_t>(charWidth_));
            // IME 조합 배경 — 토큰 rgb, 알파 64 고정 보존 (스펙 §1c).
            dc.SetColor(t.imeCompositionBg.r, t.imeCompositionBg.g, t.imeCompositionBg.b, 64);
            dc.FillRect(JKRect{ compX, compY, compW, 16 });
            dc.SetTextColor(textR_, textG_, textB_);
            dc.TextOut(jk::JKPoint{ compX, compY }, compText_.c_str());
        }

        if (focused_ && showCaret_) {
            size_t line = GetLineFromPos(cursorPos_);
            size_t lineStart = GetLineStart(line);
            int32_t caretX = inner.x + static_cast<int32_t>(
                DisplayCells(buffer_, lineStart, cursorPos_) * static_cast<size_t>(charWidth_));
            int32_t caretY = inner.y + static_cast<int32_t>((line - firstVisibleLine_) * lineHeight_) + 2;
            dc.SetColor(t.widgetText.r, t.widgetText.g, t.widgetText.b, 255);
            dc.DrawLine(caretX, caretY, caretX, caretY + 12);

            if (!compText_.empty()) {
                int32_t compCaretX = caretX + static_cast<int32_t>(
                    DisplayCells(compText_, 0, compCursor_) * static_cast<size_t>(charWidth_));
                dc.SetColor(t.imeCaret.r, t.imeCaret.g, t.imeCaret.b, 255);
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
    if (imeComposing_ && g_jkAppHost) {
        // Ask the OS IME to flush the composed string first, then fall back to
        // a local commit if the OS did not deliver a TEXTINPUT event in time.
        SDL_Window* window = g_jkAppHost->GetSdlWindow();
        if (window) JKPlatform::CompleteComposition(window);
        CommitComposition();
    }
}

void JKEdit::UpdateTextInputRect() {
    if (!g_jkAppHost) return;
    SDL_Window* window = g_jkAppHost->GetSdlWindow();
    if (!window) return;
    const JKRect client = GetScreenClientRect();
    SDL_Rect rect{ client.x, client.y, client.w, client.h };
    SDL_SetTextInputRect(&rect);
}

void JKEdit::DetectWindowsImeState() {
#ifdef _WIN32
    if (!g_jkAppHost) return;
    SDL_Window* window = g_jkAppHost->GetSdlWindow();
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
        if (imeComposing_ && g_jkAppHost) {
            SDL_Window* window = g_jkAppHost->GetSdlWindow();
            if (window) JKPlatform::CompleteComposition(window);
        }

        size_t oldPos = cursorPos_;
        // 조합 중 클릭으로 캐럿이 옮겨지면 자동사를 마친다 — 씨앗 위치가
        // 어긋난 채 다음 키를 조합하면 엉뚱한 바이트를 덮어쓴다.
        if (composing_) {
            composing_ = false;
            automata_.InitAutomata();
        }
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
        if (g_jkAppHost) g_jkAppHost->SetCapture(this);
    } else if (ev.type == JKEventType::MouseMove) {
        if (mouseSelecting_) {
            cursorPos_ = PixelToPos(ev.x, ev.y);
            selAnchor_ = mouseAnchor_;
            hasSelection_ = (cursorPos_ != selAnchor_);
            ScrollToCursor();
        }
    } else if (ev.type == JKEventType::MouseUp) {
        mouseSelecting_ = false;
        if (g_jkAppHost && g_jkAppHost->GetCapture() == this) {
            g_jkAppHost->ReleaseCapture();
        }
    } else if (ev.type == JKEventType::Timer) {
        if (focused_) showCaret_ = !showCaret_;
    } else if (ev.type == JKEventType::ImeChanged) {
        // 서버가 폴링한 OS IME 변환 상태 변화(docs/61 §16) — 한/영 전환키는
        // OS IME가 삼켜 SDL에 도달하지 않으므로 상태 변화가 유일한 관측점.
        // Unknown은 정보 없음, 조합 중(OS IME 조합)엔 상태를 건드리지 않는다.
        JKPlatform::ImeMode mode = static_cast<JKPlatform::ImeMode>(ev.option);
        if (mode == JKPlatform::ImeMode::Unknown || imeComposing_)
            return;
        const bool hangul = (mode == JKPlatform::ImeMode::Hangul);
        if (inputMode_ == InputMode::InternalHangul) {
            // 내부 오토마타 모드: OS IME가 방금 전환했다(한/영 키의 일반 효과)
            // — 진행 중 조합을 확정하고 OS IME 경로를 따른다.
            FinishInternalComposition();
        }
        inputMode_ = hangul ? InputMode::ImeHangul : InputMode::Ascii;
    } else if (ev.type == JKEventType::ImeToggle) {
        // 서버 저수준 훅이 물리 한/영 키를 관측(docs/61 §16.1 — OS IME가 키를
        // 삼키고 신식 IME는 IMM 변환 플래그를 갱신하지 않아 절대 상태를 읽을
        // 방법이 없다). 기능적으로 중요한 건 내부 모드만 손을 떼는 것: OS IME가
        // 방금 언어를 전환했고 조합/커밋은 TEXTINPUT/TEXTEDITING 경로로 오므로
        // OS IME를 따르면 된다. 절대 모드 판정은 포커스 시점의
        // DetectWindowsImeState가 IMM이 살아 있는 환경에서 보정한다.
        if (inputMode_ == InputMode::InternalHangul && !imeComposing_) {
            FinishInternalComposition();
            inputMode_ = InputMode::ImeHangul;
        }
    } else if (ev.type == JKEventType::MouseWheel) {
        // 멀티라인 휠 스크롤 (docs/61 §2) — dy>0 = 위(이전 라인).
        if (multiLine_) {
            size_t lineCount = GetLineCount();
            if (ev.dy > 0) {
                if (firstVisibleLine_ > static_cast<size_t>(ev.dy))
                    firstVisibleLine_ -= static_cast<size_t>(ev.dy);
                else
                    firstVisibleLine_ = 0;
            } else if (ev.dy < 0) {
                size_t scroll = static_cast<size_t>(-ev.dy);
                size_t maxFirst = (lineCount > 1) ? lineCount - 1 : 0;
                firstVisibleLine_ = std::min(firstVisibleLine_ + scroll, maxFirst);
            }
            showCaret_ = true;
        }
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
                if (inputMode_ == InputMode::InternalHangul && g_jkAppHost) {
                    SDL_Window* window = g_jkAppHost->GetSdlWindow();
                    if (window) JKPlatform::SetConversionMode(window, JKPlatform::ImeMode::Ascii);
                }
            }
        } else if (ev.keyCode == (SDLK_SCANCODE_MASK | SDL_SCANCODE_LANG1)) {
            // 한/영 전환키(IME LANG1, docs/61 §15): OS IME가 이미 토글했다.
            // 내부 오토마타 모드면 손을 내리고(OS IME가 방금 전환한 상태를
            // 따라간다), OS IME 경로면 현재 변환 상태를 다시 읽어 동기화한다.
            // 옛 코드는 이 키에 대한 처리가 없어 F2만 응답했다.
            if (!imeComposing_) {
                if (inputMode_ == InputMode::InternalHangul)
                    inputMode_ = InputMode::Ascii;
                DetectWindowsImeState();
            }
        } else if (inputMode_ == InputMode::InternalHangul &&
                   !imeComposing_ &&
                   ((ev.keyCode >= SDLK_a && ev.keyCode <= SDLK_z) ||
                    (ev.keyCode >= 'A' && ev.keyCode <= 'Z'))) {
            // 라이브 SDL은 shift를 키코드에 반영해 'R'(대문자)로 온다 —
            // 소문자 범위만 검사하면 쌍자음 키가 아예 조합에 못 들어간다
            // (docs/61 §13).
            ProcessHangulKey(static_cast<uint16_t>(ev.keyCode),
                             (shift != static_cast<bool>(mod & KMOD_CAPS)) ? 0x0040 : 0);
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
            // 조합 중 커서를 옮기거나 지우면 자동사를 마친다 — 씨앗 위치가
            // 어긋난 채 다음 키를 조합하면 엉뚱한 바이트를 덮어쓴다.
            if (composing_) {
                switch (ev.keyCode) {
                    case SDLK_LEFT: case SDLK_RIGHT: case SDLK_UP: case SDLK_DOWN:
                    case SDLK_HOME: case SDLK_END: case SDLK_PAGEUP:
                    case SDLK_PAGEDOWN: case SDLK_DELETE: case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        composing_ = false;
                        automata_.InitAutomata();
                        break;
                    default: break;
                }
            }
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
                // 조합 중 공백/구두점이 오면 자동사를 마친다 — 마치지 않으면
                // 다음 조합 키가 방금 삽입된 바이트를 덮어쓴다(docs/61 §10).
                if (composing_) {
                    composing_ = false;
                    automata_.InitAutomata();
                }
                InsertText(ev.text);
            } else if (c >= 0x80) {
                if (composing_) {
                    composing_ = false;
                    automata_.InitAutomata();
                }
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
    // 방어선 (docs/61 §8): 8비트 슬롯 코드가 그대로 오면 {0x00,XX} NUL 쌍이
    // 버퍼에 기록돼 c_str() 렌더가 절단된다(한글 줄 통째로 안 보임). 자동사
    // End1/End2 플러시 원인은 근본 픽스됐으나, NUL 유입은 전면 침묵이라 여기서도
    // 1회 변환해 막는다.
    if (code < 0x8000) code = HangulAutomata::ToStandaloneKssm(code & 0xFF);
    if (buffer_.size() + 2 > maxLength_) return;
    char pair[2] = { static_cast<char>(code >> 8), static_cast<char>(code & 0xFF) };
    buffer_.insert(cursorPos_, pair, 2);
    cursorPos_ += 2;
    ScrollToCursor();
    showCaret_ = true;
}

void JKEdit::InsertKssmText(const char* text) {
    if (!text || !text[0]) return;
    // KSSM 삽입 경로도 선택을 지운다 (docs/60 §10): InsertText(ASCII)와 달리
    // 누락돼 한글 타이핑 시 선택이 살아 남았다.
    DeleteSelection();
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

// 내부 오토마타 조합을 마친다(docs/61 §16): 조합 중이던 글자를 확정해 버퍼에
// 남기고 자동사 상태를 비운다 — 한/영 전환 등 내부 모드에서 손을 뗄 때 쓴다.
void JKEdit::FinishInternalComposition() {
    if (!composing_ && automata_.curHanState && automata_.charCode != 0x8441)
        InsertKssmChar(automata_.charCode);   // 씨앗 상태만 남은 경우 확정
    // composing_면 조합 쌍이 이미 버퍼에 있다 — 확정(상태 해제)만 하면 된다.
    automata_.InitAutomata();
    composing_ = false;
}

void JKEdit::ProcessHangulKey(uint16_t keyCode, uint16_t modifier) {
    // 타이핑은 선택을 대체한다 (docs/60 §10).
    if (hasSelection_) DeleteSelection();
    uint16_t converted = automata_.ConvertKey(keyCode, modifier);
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
        // End1/End2는 트리거 키를 다음 조합의 씨앗으로 심고 돌아온다 —
        // 살아 있는 상태는 유지하고 아래 블록이 씨앗 글자를 삽입한다.
        // (InitAutomata는 시드를 지워 받침 뒤 조합을 끊는다.)
        if (!automata_.curHanState || automata_.charCode == 0x8441)
            automata_.InitAutomata();
        else
            automata_.outSP = 0;  // 시드 유지 — 플러시 버퍼만 비운다
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

// KSSM characters are stored as 2-byte pairs where the first byte is >= 0x80.
// ASCII bytes (< 0x80) are single-byte units. The second byte of a KSSM pair
// may be anywhere in 0x00..0xFF, so deletion and cursor movement must look at
// the previous byte to decide whether it is a KSSM first byte.
void JKEdit::DeleteBackward() {
    if (DeleteSelection()) return;
    if (cursorPos_ == 0) return;
    // If the internal automata is mid-composition, cancel it first so that a
    // single Backspace removes the whole in-progress Hangul character.
    if (composing_) {
        composing_ = false;
        if (cursorPos_ >= 2) {
            buffer_.erase(cursorPos_ - 2, 2);
            cursorPos_ -= 2;
            automata_.InitAutomata();
        }
        ScrollToCursor();
        showCaret_ = true;
        return;
    }
    size_t prev = cursorPos_ - 1;
    // If the byte before the cursor is preceded by a KSSM first byte, the pair
    // ends right before the cursor; delete both bytes.
    if (prev >= 1 && static_cast<uint8_t>(buffer_[prev - 1]) >= 0x80) {
        prev = prev - 1;
    }
    buffer_.erase(prev, cursorPos_ - prev);
    cursorPos_ = prev;
    ScrollToCursor();
    showCaret_ = true;
}

void JKEdit::DeleteForward() {
    if (DeleteSelection()) return;
    if (cursorPos_ >= buffer_.size()) return;
    // If the byte at the cursor is a KSSM first byte, delete the whole pair.
    size_t len = (static_cast<uint8_t>(buffer_[cursorPos_]) >= 0x80) ? 2 : 1;
    buffer_.erase(cursorPos_, len);
    ScrollToCursor();
    showCaret_ = true;
}

void JKEdit::MoveCursorLeft() {
    if (cursorPos_ == 0) return;
    // If the byte two positions back is a KSSM first byte, we are at the end
    // of a KSSM pair; jump over the whole pair.
    if (cursorPos_ >= 2 && static_cast<uint8_t>(buffer_[cursorPos_ - 2]) >= 0x80) {
        cursorPos_ -= 2;
    } else {
        --cursorPos_;
    }
    ScrollToCursor();
    showCaret_ = true;
}

void JKEdit::MoveCursorRight() {
    if (cursorPos_ >= buffer_.size()) return;
    // If the byte at the cursor is a KSSM first byte, jump over the pair.
    cursorPos_ += (static_cast<uint8_t>(buffer_[cursorPos_]) >= 0x80) ? 2 : 1;
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
    size_t pos;
    if (multiLine_) {
        int32_t relX = x - inner.x;
        int32_t relY = y - inner.y;
        size_t line = firstVisibleLine_ + static_cast<size_t>(std::max(0, relY / lineHeight_));
        size_t lineCount = GetLineCount();
        if (line >= lineCount) line = lineCount - 1;
        size_t start = GetLineStart(line);
        size_t end = GetLineEnd(line);
        // 셀 단위 역매핑 (docs/60 §10): KSSM 쌍은 통째로 건너뛰므로 반환값이
        // 자연히 글리프 경계에 스냅된다(쌍 중간 클릭 방지).
        pos = PosFromCells(buffer_, start, end, static_cast<size_t>(std::max(0, relX) / charWidth_));
    } else {
        size_t cell = firstVisibleCol_ +
                      static_cast<size_t>(std::max(0, x - inner.x) / charWidth_);
        pos = PosFromCells(buffer_, 0, buffer_.size(), cell);
    }
    // Snap to a valid character boundary: if the cursor landed between the two
    // bytes of a KSSM pair, move it to the start of the pair. This prevents
    // later DeleteBackward/DeleteForward from removing only half a Hangul
    // character and corrupting the buffer.
    if (pos > 0 && pos < buffer_.size() &&
        static_cast<uint8_t>(buffer_[pos - 1]) >= 0x80) {
        --pos;
    }
    return pos;
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
        // 버퍼는 KSSM — 클립보드는 UTF-8 (docs/60 §10). 무변환 복사는 한글을
        // CP949 바이트로 클립보드에 밀어 넣어 붙여넣기 측에서 오염됐다.
        std::string utf8 = KssmToUtf8(selected.c_str());
        SDL_SetClipboardText(utf8.c_str());
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
            // 클립보드는 UTF-8 — 버퍼(KSSM)로 변환해 삽입. 무변환 삽입은
            // UTF-8 바이트가 그대로 버퍼에 들어가 렌더·커서 로직을 깬다.
            std::string kssm = Utf8ToKssm(text);
            InsertText(kssm.c_str());
            SDL_free(text);
        }
    }
}

// fieldBg 함정 (docs/52): 이 위젯군의 ctor는 back에 widgetFace가 아니라
// fieldBg를 캡처한다 (JKEdit.cpp:21). 자기 멤버만 재포착하고 재귀·무효화는
// 반복하지 않는다 — children_ 재귀 후 베이스와 동일하게 마무리.
void JKEdit::ApplyTheme() {
    const auto& t = jk::theme::current();
    SetBackColor(t.fieldBg.r, t.fieldBg.g, t.fieldBg.b);
    SetTextColor(t.widgetText.r, t.widgetText.g, t.widgetText.b);
    for (auto& c : children_) c->ApplyTheme();
    InvalidateRect(JKRect{ 0, 0, rect_.w, rect_.h });
}

} // namespace jk
