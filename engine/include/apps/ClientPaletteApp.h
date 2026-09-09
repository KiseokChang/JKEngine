#ifndef APPS_CLIENTPALETTEAPP_H
#define APPS_CLIENTPALETTEAPP_H

#include <client/JKClientApplication.h>
#include <client/JKClientSurface.h>
#include <chrono>
#include <string>
#include <vector>

namespace jk {

// Command palette (agent platform M2a, spec §6.2): an ImGui window client
// that speaks the Desktop Agent API over its own window connection. Slash
// commands are deterministic (parsed and executed locally); natural-language
// delegation is a later stage. It doubles as the minimal notification
// center: AgentEventSubscribe feeds desktop events into the panel below the
// input box.
class ClientPaletteApp : public JKClientApplication {
public:
    ~ClientPaletteApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    bool PreProcessMessage(const JKEvent& ev) override;
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;

private:
    void BuildUi(int w, int h);
    void AppendLog(const std::string& line);

    bool frameDirty_ = true;
    bool imguiReady_ = false;
    bool scrollDirty_ = false;
    bool focusInput_ = true;   // grab keyboard focus on open
    std::chrono::steady_clock::time_point lastFrame_;

    char input_[256] = {};
    std::vector<std::string> log_;
};

} // namespace jk
#endif // APPS_CLIENTPALETTEAPP_H