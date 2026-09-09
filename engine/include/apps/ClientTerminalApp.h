#ifndef CLIENTTERMINALAPP_H
#define CLIENTTERMINALAPP_H

// ConPTY terminal client (docs/22): hosts a JKTerminalGrid + JKVtParser +
// JKGlyphAtlas + JKConPtyBridge, renders through the client surface, and
// maps input to VT byte sequences (docs/22 §6.1).

#include <client/JKClientApplication.h>
#include <terminal/JKTerminalGrid.h>
#include <terminal/JKVtParser.h>
#include <terminal/JKGlyphAtlas.h>
#include <terminal/JKConPtyBridge.h>
#include <memory>
#include <string>

namespace jk {

class TerminalView;

class ClientTerminalApp : public JKClientApplication {
public:
    ClientTerminalApp() = default;
    ~ClientTerminalApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    bool PreProcessMessage(const JKEvent& ev) override;
    void OnIdle() override;
    bool IsFrameDirty() const override;
    void OnFrameCommitted() override;
    bool WantsTabFocusCycle() const override { return false; }

private:
    void PumpPty();
    void WriteToPty(const char* data, size_t len);
    void OnViewResized(int cols, int rows);
    // M2b trigger feed: publish sanitized output as agent events.
    void PublishTerminalOutput();
    static std::string StripVtEscapes(const std::string& raw);
    static std::string JsonEscape(const std::string& s);

    std::unique_ptr<JKTerminalGrid> grid_;
    std::unique_ptr<JKVtParser> parser_;
    std::unique_ptr<JKGlyphAtlas> atlas_;
    std::unique_ptr<JKConPtyBridge> pty_;
    TerminalView* view_ = nullptr;
    bool ptyStarted_ = false;
    bool firstFrame_ = true;
    int ptyCols_ = 0;
    int ptyRows_ = 0;
    std::string shell_;   // terminal.json "shell" (default in JKTerminalConfig)
    std::string pendingPub_;      // sanitized output awaiting publish
    uint32_t nextPubQueryId_ = 1;
    uint64_t lastPubTickMs_ = 0;  // steady_clock ms of last publish
};

} // namespace jk

#endif // CLIENTTERMINALAPP_H