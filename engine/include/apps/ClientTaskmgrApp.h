#ifndef APPS_CLIENTTASKMGRAPP_H
#define APPS_CLIENTTASKMGRAPP_H

#include <client/JKClientApplication.h>
#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace jk {

// ImGui Phase 2 showcase (docs/23 §11.2): a task manager over the shell
// protocol. It subscribes to WindowList snapshots (WindowListSubscribe — the
// shell slot stays with the taskbar) and samples per-process CPU/memory
// itself from the client-reported pids, keeping the server a dumb
// compositor. Window actions reuse WindowActivate/WindowMinimizeToggle.
class ClientTaskmgrApp : public JKClientApplication {
public:
    ~ClientTaskmgrApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    bool PreProcessMessage(const JKEvent& ev) override;
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;

private:
    // One display row: the wire snapshot fields plus locally sampled stats.
    struct Row {
        uint32_t surfaceId = 0;
        uint32_t flags = 0;
        uint32_t pid = 0;
        std::string title;
        double cpuPercent = 0.0;
        uint64_t workingSet = 0;
        bool statsValid = false;   // false until the second sample lands
    };
    // Per-surface CPU history ring for PlotLines.
    struct History {
        float cpu[90] = {};
        int offset = 0;
        int samples = 0;
    };
    struct PrevSample {            // previous GetProcessTimes sample, by pid
        uint64_t procTime = 0;     // kernel+user, 100 ns units
        uint64_t wallTime = 0;     // wall clock, 100 ns units
    };

    void TakeSnapshot();      // pull GetWindowList into rows_ (keep stats)
    void SampleProcesses();   // ~2 Hz: OpenProcess + GetProcessTimes/MemoryInfo
    void BuildUi(int w, int h);
    const Row* FindRow(uint32_t surfaceId) const;

    bool frameDirty_ = true;
    bool imguiReady_ = false;
    bool listDirty_ = false;
    std::chrono::steady_clock::time_point lastFrame_;
    std::chrono::steady_clock::time_point lastSample_;

    std::vector<Row> rows_;
    std::map<uint32_t, History> history_;   // by surfaceId
    std::map<uint32_t, PrevSample> prev_;   // by pid
    uint32_t selectedId_ = 0;

    unsigned numCores_ = 1;
    uint32_t selfPid_ = 0;
};

} // namespace jk

#endif // APPS_CLIENTTASKMGRAPP_H