#ifndef JKEDIT_H
#define JKEDIT_H

#include <JKControl.h>
#include <JKHangulAutomata.h>
#include <string>

namespace jk {

class JKEdit : public JKControl {
public:
    JKEdit();
    explicit JKEdit(const JKRect& rect, uint16_t controlId = 0,
                    size_t maxLength = 256, bool multiLine = false);

    void SetMaxLength(size_t maxLength) { maxLength_ = maxLength; }
    size_t GetMaxLength() const { return maxLength_; }

    void SetMultiLine(bool multiLine) { multiLine_ = multiLine; }
    bool IsMultiLine() const { return multiLine_; }

    void SetHangulMode(bool hangul) { hangulMode_ = hangul; }
    bool GetHangulMode() const { return hangulMode_; }
    void ToggleHangulMode() { hangulMode_ = !hangulMode_; }

    void SetText(const std::string& text) override;
    const std::string& GetText() const override;

    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;

    void OnSetFocus() override;
    void OnKillFocus() override;

    // 선택 영역
    void SetSelection(size_t start, size_t end);
    void ClearSelection();
    bool HasSelection() const { return hasSelection_; }
    std::string GetSelectedText() const;

private:
    std::string buffer_;
    size_t cursorPos_ = 0; // 전체 버퍼 내 바이트 인덱스
    size_t maxLength_ = 256;
    bool multiLine_ = false;
    bool focused_ = false;
    bool showCaret_ = true;

    bool hangulMode_ = false;
    HangulAutomata automata_;
    bool composing_ = false;

    // 멀티라인 / 스크롤 상태
    size_t firstVisibleLine_ = 0;
    int32_t lineHeight_ = 16;
    int32_t charWidth_ = 8;

    // 선택 영역
    bool hasSelection_ = false;
    size_t selAnchor_ = 0;
    bool mouseSelecting_ = false;
    size_t mouseAnchor_ = 0;

    size_t GetLineCount() const;
    size_t GetLineStart(size_t line) const;
    size_t GetLineEnd(size_t line) const;
    size_t GetLineFromPos(size_t pos) const;
    size_t GetColFromPos(size_t pos) const;

    void InsertText(const char* text);
    void InsertKssmChar(uint16_t code);
    void DeleteBackward();
    void DeleteForward();
    bool DeleteSelection();
    void MoveCursorLeft();
    void MoveCursorRight();
    void MoveCursorHome();
    void MoveCursorEnd();
    void MoveCursorUp();
    void MoveCursorDown();
    void MoveCursorPageUp();
    void MoveCursorPageDown();
    void ProcessReturn();
    void ProcessHangulKey(uint16_t keyCode);
    void ScrollToCursor();

    size_t PixelToPos(int32_t x, int32_t y) const;
    void UpdateSelection(size_t oldPos, bool shift);
    void CopyToClipboard();
    void CutToClipboard();
    void PasteFromClipboard();
};

} // namespace jk

#endif // JKEDIT_H
