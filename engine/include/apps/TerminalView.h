#ifndef TERMINALVIEW_H
#define TERMINALVIEW_H

// Terminal screen view (docs/22 §5): paints a JKTerminalGrid into a client
// window with JKGlyphAtlas glyphs. Keyboard input is translated to VT/ConPTY
// byte sequences (docs/22 §6.1) and handed to the app via onInput_.
// The app owns parser/grid/atlas/pty; the view holds raw pointers.

#include <JKWindow.h>
#include <terminal/JKTerminalGrid.h>
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
    void HandleWheel(int wheelY);

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
    void PaintCell(JKDC& dc, const JKRect& cellRect, const JKTermCell& cell,
                   bool isCursor);
    void PaintGlyph(JKDC& dc, const JKRect& cellRect, uint32_t cp,
                    uint32_t fg, bool bold);
    void PaintFallbackGlyph(JKDC& dc, const JKRect& cellRect, uint32_t cp,
                            uint32_t fg);

    JKVtParser* parser_ = nullptr;
    JKTerminalGrid* grid_ = nullptr;
    JKGlyphAtlas* atlas_ = nullptr;
    JKResourceCache* cache_ = nullptr;
    int cols_ = 0;
    int rows_ = 0;
    int scrollOffset_ = 0;   // rows scrolled up from the live bottom; 0 = live
    bool blinkOn_ = true;
    uint32_t themeBg_ = 0x0C0C0C;   // defaults mirror the classic terminal
    uint32_t themeFg_ = 0xCCCCCC;
    std::function<void(const char*, size_t)> onInput_;
    std::function<void(int, int)> onResize_;
};

} // namespace jk

#endif // TERMINALVIEW_H