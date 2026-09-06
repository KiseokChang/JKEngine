#ifndef TERMINALAPP_H
#define TERMINALAPP_H

// Single-process ConPTY terminal (jkdesktop.exe terminal): hosts the same
// grid/parser/atlas/pty stack as the window-server client (ClientTerminalApp,
// docs/22) but directly on JKApplication — no server, no client surface.
// The client host drives frames with OnIdle/IsFrameDirty hooks that
// JKApplication does not have, so the pty pump rides the legacy timer event
// (SetTimerInterval) and quit is signaled by returning false from
// PreProcessMessage (the single-process exit idiom, cf. VectorFontApp).

#include <JKApplication.h>
#include <terminal/JKTerminalGrid.h>
#include <terminal/JKVtParser.h>
#include <terminal/JKGlyphAtlas.h>
#include <terminal/JKConPtyBridge.h>
#include <memory>
#include <string>

namespace jk {

class TerminalView;

class TerminalApp : public JKApplication {
public:
    TerminalApp() = default;
    ~TerminalApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    bool PreProcessMessage(const JKEvent& ev) override;

private:
    void PumpPty();
    void WriteToPty(const char* data, size_t len);
    void OnViewResized(int cols, int rows);
    // Reserves a repaint for grid changes. Single-process has no
    // IsFrameDirty/OnFrameCommitted commit gate — Invalidate is the repaint
    // reservation, so this order is enough.
    void SyncRepaint();

    std::unique_ptr<JKTerminalGrid> grid_;
    std::unique_ptr<JKVtParser> parser_;
    std::unique_ptr<JKGlyphAtlas> atlas_;
    std::unique_ptr<JKConPtyBridge> pty_;
    TerminalView* view_ = nullptr;
    bool ptyStarted_ = false;
    bool ptyStartFailed_ = false;   // failed Start() is not transient — log once, stop retrying
    int ptyCols_ = 0;
    int ptyRows_ = 0;
    int blinkCounter_ = 0;          // 30ms ticks; cursor blinks at ~540ms like the client
    bool quitRequested_ = false;    // shell exited → stop the run loop
    std::string shell_;             // terminal.json "shell" (default in JKTerminalConfig)
};

} // namespace jk

#endif // TERMINALAPP_H