#ifndef TERMINALVIEW_H
#define TERMINALVIEW_H

// Terminal screen view (docs/22 §5): paints a JKTerminalGrid into a client
// window with JKGlyphAtlas glyphs. Keyboard input is translated to VT/ConPTY
// byte sequences (docs/22 §6.1) and handed to the app via onInput_.
// The app owns parser/grid/atlas/pty; the view holds raw pointers.

#include <JKWindow.h>
#include <terminal/JKTerminalGrid.h>
#include <apps/JKTermSelection.h>
#include <functional>
#include <string>

namespace jk {

class JKVtParser;
class JKGlyphAtlas;
class JKResourceCache;

constexpr int kTermCellW = 8;
constexpr int kTermCellH = 16;

class TerminalView : public JKWindow {
public:
    TerminalView(JKVtParser* parser, JKTerminalGrid* grid, JKGlyphAtlas* atlas,
                 JKResourceCache* cache);

    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;

protected:
    void OnRectChanged(const JKRect& rect) override;

public:
    // Timer tick from the app: toggles cursor blink.
    void TickBlink();

    // Mouse wheel scrollback: wheelY > 0 scrolls toward older output.
    // Positive values are lines per notch; the offset is clamped to the
    // grid's scrollback depth and reset to live view on any keyboard input.
    void HandleWheel(int wheelY, uint32_t option);

    int Cols() const { return cols_; }
    int Rows() const { return rows_; }

    // Overrides the default color theme (terminal.json "themeBg"/"themeFg",
    // docs/26 단계 5). Both are 0xRRGGBB.
    void SetTheme(uint32_t bg, uint32_t fg) { themeBg_ = bg; themeFg_ = fg; }

    void SetOnInput(std::function<void(const char*, size_t)> fn) { onInput_ = std::move(fn); }
    // Fired after the grid was resized to the new cell geometry.
    void SetOnResize(std::function<void(int, int)> fn) { onResize_ = std::move(fn); }

private:
    void RecalcCells();
    void HandleKeyDown(const JKEvent& ev);
    // Mouse selection (docs/26 단계 2): left-drag box selection over the live
    // grid rows. Called from RespondMessage BEFORE the JKWindow delegation —
    // single-process mode drops client-area mouse events there (the HitTest
    // target is the window itself), client mode receives them as a DOCK_FILL
    // child but with the same window/surface coordinates.
    void HandleMouseEvent(const JKEvent& ev);
    // Mouse-report gate (docs/26 단계 3, spec §3): encodes a mouse event with
    // the JKTermInput encoders and sends it to the pty via onInput_ instead
    // of the local selection path. Never touches selection state.
    void HandleMouseReport(const JKEvent& ev);
    JKPoint CellFromPoint(int32_t px, int32_t py) const;
    void ClearSelection();
    void CopySelection();
    void PasteClipboard();
    void PaintCell(JKDC& dc, const JKRect& cellRect, const JKTermCell& cell,
                   bool isCursor, bool selected = false);
    void PaintGlyph(JKDC& dc, const JKRect& cellRect, uint32_t cp,
                    uint32_t fg, bool bold);
    void PaintFallbackGlyph(JKDC& dc, const JKRect& cellRect, uint32_t cp,
                            uint32_t fg);
    // IME pre-edit overlay (docs/26 단계 5, spec §3): composition glyphs
    // drawn over the live grid cursor cell.
    void PaintPreEdit(JKDC& dc, const JKRect& client);
    void ClearPreEdit();

    JKVtParser* parser_ = nullptr;
    JKTerminalGrid* grid_ = nullptr;
    JKGlyphAtlas* atlas_ = nullptr;
    JKResourceCache* cache_ = nullptr;
    int cols_ = 0;
    int rows_ = 0;
    int scrollOffset_ = 0;   // rows scrolled up from the live bottom; 0 = live
    // Scrollback depth at the last paint (docs/26 단계 4): the delta since
    // then re-anchors the viewport while the user is scrolled back.
    int lastHist_ = 0;
    bool blinkOn_ = true;
    uint32_t themeBg_ = 0x0C0C0C;   // defaults mirror the classic terminal
    uint32_t themeFg_ = 0xCCCCCC;
    std::function<void(const char*, size_t)> onInput_;
    std::function<void(int, int)> onResize_;

    // Selection state (docs/26 단계 2): live grid cell coords; x < 0 = none.
    // v1 covers live screen rows only — scrollback snapshots are never
    // selected and the rect does not follow shell output (spec §5).
    JKPoint selAnchor_{ -1, -1 };
    JKPoint selEnd_{ -1, -1 };
    bool selDragging_ = false;   // left button held, drag in progress

    // Mouse-report state (docs/26 단계 3): reportedButtons_ is a bitmask of
    // the buttons the view has reported down (bit 0 = left .. bit 2 = right)
    // gating Button-mode (1002) motion reports; lastMouse_ is the last mouse
    // position seen in RespondMessage — MouseWheel events carry no
    // coordinates, so HandleWheel reports the wheel at the last seen cell.
    uint32_t reportedButtons_ = 0;
    JKPoint lastMouse_{ 0, 0 };

    // IME composition (docs/26 단계 5, spec §3): the UTF-8 pre-edit string
    // from JKEventType::TextEditing, empty when nothing is composing. Shown
    // as an overlay at the live grid cursor cell until a commit (Char) or a
    // key/paste/selection-start clears it.
    std::string preEdit_;
};

} // namespace jk

#endif // TERMINALVIEW_H