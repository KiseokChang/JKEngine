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

    void SetReadOnly(bool readOnly) { readOnly_ = readOnly; }
    bool IsReadOnly() const { return readOnly_; }

    enum class InputMode { Ascii, InternalHangul, ImeHangul };

    void SetHangulMode(bool hangul) { inputMode_ = hangul ? InputMode::InternalHangul : InputMode::Ascii; }
    bool GetHangulMode() const { return inputMode_ == InputMode::InternalHangul; }
    void ToggleHangulMode() { inputMode_ = (inputMode_ == InputMode::InternalHangul)
                                            ? InputMode::Ascii : InputMode::InternalHangul; }
    InputMode GetInputMode() const { return inputMode_; }
    void SetInputMode(InputMode mode) { inputMode_ = mode; }

    void SetText(const std::string& text) override;
    const std::string& GetText() const override;

    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;
    void ApplyTheme() override;  // fieldBg 재캡처 (docs/52)

    void OnSetFocus() override;
    void OnKillFocus() override;

    bool IsCaretVisible() const { return focused_ && showCaret_; }

    // 화면 픽셀 → 바이트 위치 (마우스 히트 테스트용 public API).
    size_t PixelToPos(int32_t x, int32_t y) const;

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
    bool readOnly_ = false;
    bool focused_ = false;
    bool showCaret_ = true;

    InputMode inputMode_ = InputMode::Ascii;
    HangulAutomata automata_;
    bool composing_ = false;

    // IME composition state (SDL_TEXTEDITING).
    std::string compText_;
    size_t compCursor_ = 0;
    bool imeComposing_ = false;

    // 멀티라인 / 스크롤 상태 — 기본값은 비트맵 셀(16/8). 두 ctor가 셀 메트릭
    // 진실원(jk::text::CellMetrics, docs/63 §6 text.font_scale)으로 채운다.
    size_t firstVisibleLine_ = 0;
    int32_t lineHeight_ = 16;
    int32_t charWidth_ = 8;
    // 한 줄 편집 수평 스크롤 (표시 셀 단위 — ASCII 1셀=engW, KSSM 1글자=2셀).
    size_t firstVisibleCol_ = 0;

    // 선택 영역
    bool hasSelection_ = false;
    size_t selAnchor_ = 0;
    bool mouseSelecting_ = false;
    size_t mouseAnchor_ = 0;

    size_t GetLineCount() const;
    size_t GetLineStart(size_t line) const;
    size_t GetLineEnd(size_t line) const;
    size_t GetLineFromPos(size_t pos) const;

    // 표시 셀 매핑 (docs/60 §10): JKDC 비트맵 폰트는 ASCII 8px / KSSM 2바이트
    // 16px로 전진하므로 바이트 인덱스×charWidth_는 한글에서 캐럿·선택을 밀어낸다.
    // 셀 = ASCII 바이트 1개 또는 KSSM 쌍 1개의 점유 폭(8px 단위).
    static size_t DisplayCells(const std::string& buf, size_t from, size_t to);
    static size_t PosFromCells(const std::string& buf, size_t from, size_t to, size_t cells);

    void InsertText(const char* text);
    void InsertKssmChar(uint16_t code);
    void InsertKssmText(const char* text);

    // IME / text-input helpers
    void UpdateTextInputRect();
    void DetectWindowsImeState();
    void SilenceOsIme();
    void CommitComposition();
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
    void ProcessHangulKey(uint16_t keyCode, uint16_t modifier = 0);
    // 내부 오토마타 조합을 확정하고 상태를 비운다 (한/영 전환 핸드오버, docs/61 §16).
    void FinishInternalComposition();
    void ScrollToCursor();

    void UpdateSelection(size_t oldPos, bool shift);
    void CopyToClipboard();
    void CutToClipboard();
    void PasteFromClipboard();
};

} // namespace jk

#endif // JKEDIT_H
