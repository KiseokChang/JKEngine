// Task manager client (ImGui Phase 2, docs/23 §11.2): shell-protocol window
// list + self-sampled per-process stats. See ClientTaskmgrApp.h.
#include <apps/ClientTaskmgrApp.h>

#include <imgui_impl_jkwindow.h>
#include <JKWindow.h>
#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#endif

namespace jk {

namespace {
// Root window paints the dark clear color so the surface never flashes white
// behind ImGui's rounded windows (same idiom as the Phase 1 demo).
class TaskmgrRoot : public JKWindow {
public:
    explicit TaskmgrRoot(const std::string& title) : JKWindow(title) {}
    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetClientRect();
        dc.SetColor(32, 32, 38, 255);
        dc.FillRect(client);
    }
};

#ifdef _WIN32
uint64_t FileTimeToU64(const FILETIME& ft) {
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return u.QuadPart;
}

const char* FormatBytes(uint64_t bytes, char (&buf)[32]) {
    if (bytes >= (1ull << 20)) {
        std::snprintf(buf, sizeof(buf), "%.1f MB",
                      static_cast<double>(bytes) / 1048576.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%.0f KB",
                      static_cast<double>(bytes) / 1024.0);
    }
    return buf;
}
#endif
} // namespace

ClientTaskmgrApp::~ClientTaskmgrApp() = default;

void ClientTaskmgrApp::OnInit() {
    auto main = std::make_unique<TaskmgrRoot>("Task Manager");
    main->SetWindowRect(JKRect{ 0, 0, 900, 620 });
    main->SetAttrFlags(WA_CHROMELESS); // server close button only
    SetMainWindow(std::move(main));

    SetTimerInterval(16); // ~60 Hz frame cadence (same clock as the demo)

    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    lastFrame_ = std::chrono::steady_clock::now();
    // Force the first sample on frame 1: it only seeds prev_, but without it
    // the first real CPU numbers would wait a full second.
    lastSample_ = lastFrame_ - std::chrono::milliseconds(600);

#ifdef _WIN32
    selfPid_ = ::GetCurrentProcessId();
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    numCores_ = si.dwNumberOfProcessors ? si.dwNumberOfProcessors : 1;
#endif

    // Opt into WindowList pushes — the shell slot stays with the taskbar.
    jk::client::JKClientSurface* surface = Surface();
    if (!surface || !surface->SendWindowListSubscribe(true)) {
        std::fprintf(stderr, "[taskmgr] WindowListSubscribe failed\n");
        std::fflush(stderr);
    }
}

void ClientTaskmgrApp::OnClose() {
    if (imguiReady_) {
        ImGui_ImplJKWindow_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
}

bool ClientTaskmgrApp::PreProcessMessage(const JKEvent& ev) {
    // Every event goes to the backend before the next NewFrame consumes the
    // input queue (docs/23 §5.2-3). Unhandled kinds are ignored inside the
    // backend, so blind feeding is safe.
    ImGui_ImplJKWindow_ProcessJKEvent(ev);
    // The 16ms timer is the frame clock — re-arm the frame gate here (see the
    // Phase 1 lesson, docs/23 §11.5).
    if (ev.type == JKEventType::Timer) {
        frameDirty_ = true;
    }
    if (ev.type == JKEventType::WindowListChanged) {
        listDirty_ = true;
    }
    return true;
}

void ClientTaskmgrApp::OnFrameCommitted() {
    frameDirty_ = false;
}

void ClientTaskmgrApp::TakeSnapshot() {
    jk::client::JKClientSurface* surface = Surface();
    std::vector<client::ShellWindowInfo> fresh;
    if (!surface || !surface->GetWindowList(fresh)) {
        return;
    }

    std::vector<Row> next;
    next.reserve(fresh.size());
    std::set<uint32_t> alivePids;
    for (const auto& w : fresh) {
        Row row;
        row.surfaceId = w.surfaceId;
        row.flags = w.flags;
        row.pid = w.pid;
        row.title = w.title;
        // Preserve already-sampled stats for a surface that is still alive.
        for (const Row& old : rows_) {
            if (old.surfaceId == w.surfaceId) {
                row.cpuPercent = old.cpuPercent;
                row.workingSet = old.workingSet;
                row.statsValid = old.statsValid;
                break;
            }
        }
        alivePids.insert(w.pid);
        next.push_back(std::move(row));
    }

    // Drop samples and history of entries that vanished (also keeps history_
    // from growing with monotonic surface ids over a long session).
    for (auto it = prev_.begin(); it != prev_.end();) {
        if (alivePids.count(it->first) == 0) {
            it = prev_.erase(it);
        } else {
            ++it;
        }
    }
    std::set<uint32_t> aliveSurfaces;
    for (const Row& r : next) aliveSurfaces.insert(r.surfaceId);
    for (auto it = history_.begin(); it != history_.end();) {
        if (aliveSurfaces.count(it->first) == 0) {
            it = history_.erase(it);
        } else {
            ++it;
        }
    }
    if (selectedId_ != 0 && aliveSurfaces.count(selectedId_) == 0) {
        selectedId_ = 0;
    }
    rows_ = std::move(next);
}

void ClientTaskmgrApp::SampleProcesses() {
#ifdef _WIN32
    FILETIME nowFT;
    GetSystemTimeAsFileTime(&nowFT);
    const uint64_t nowWall = FileTimeToU64(nowFT);

    for (Row& row : rows_) {
        if (row.pid == 0) continue;
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, row.pid);
        if (!h) {
            row.statsValid = false; // exited or access denied
            continue;
        }
        FILETIME c, e, k, u;
        const BOOL okTimes = GetProcessTimes(h, &c, &e, &k, &u);
        const uint64_t procTime =
            okTimes ? FileTimeToU64(k) + FileTimeToU64(u) : 0;
        PROCESS_MEMORY_COUNTERS pmc{};
        if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc))) {
            row.workingSet = pmc.WorkingSetSize;
        }
        CloseHandle(h);
        if (!okTimes) continue;

        auto it = prev_.find(row.pid);
        if (it != prev_.end() && nowWall > it->second.wallTime) {
            const uint64_t dProc = procTime - it->second.procTime;
            const uint64_t dWall = nowWall - it->second.wallTime;
            // Task-Manager style: percent of total machine capacity.
            row.cpuPercent = 100.0 * static_cast<double>(dProc) /
                             static_cast<double>(dWall) / numCores_;
            row.statsValid = true;
        }
        prev_[row.pid] = { procTime, nowWall };

        History& hist = history_[row.surfaceId];
        hist.cpu[hist.offset] = row.statsValid
                                    ? static_cast<float>(row.cpuPercent)
                                    : 0.0f;
        hist.offset = (hist.offset + 1) % 90;
        if (hist.samples < 90) ++hist.samples;
    }
#endif
}

const ClientTaskmgrApp::Row* ClientTaskmgrApp::FindRow(uint32_t surfaceId) const {
    for (const Row& r : rows_) {
        if (r.surfaceId == surfaceId) return &r;
    }
    return nullptr;
}

void ClientTaskmgrApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
    if (!imguiReady_) {
        if (!ImGui_ImplJKWindow_Init(renderer))
            return;
        imguiReady_ = true;
    }

    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - lastFrame_).count();
    lastFrame_ = now;

    if (listDirty_) {
        TakeSnapshot();
        listDirty_ = false;
    }
    if (now - lastSample_ >= std::chrono::milliseconds(500)) {
        lastSample_ = now;
        SampleProcesses();
    }

    ImGui_ImplJKWindow_NewFrame(dt, w, h);
    ImGui::NewFrame();

    BuildUi(w, h);

    ImGui::Render();
    ImGui_ImplJKWindow_RenderDrawData(ImGui::GetDrawData(), renderer);
}

void ClientTaskmgrApp::BuildUi(int w, int h) {
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
    if (ImGui::Begin("taskmgr", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
        ImGui::Text("jkwindow Task Manager  (pid %u)  -  Dear ImGui %s",
                    selfPid_, ImGui::GetVersion());
        ImGui::Separator();

        // Reserve room for buttons + plot + IO dump below the table.
        const float footer = ImGui::GetFrameHeightWithSpacing() * 2.0f + 70.0f;
        if (ImGui::BeginTable(
                "windows", 5,
                ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_ScrollY,
                ImVec2(0.0f, -footer))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch,
                                    0, 0);
            ImGui::TableSetupColumn("PID",
                                    ImGuiTableColumnFlags_WidthFixed |
                                        ImGuiTableColumnFlags_DefaultSort,
                                    70.0f, 1);
            ImGui::TableSetupColumn("CPU %", ImGuiTableColumnFlags_WidthFixed,
                                    80.0f, 2);
            ImGui::TableSetupColumn("Memory", ImGuiTableColumnFlags_WidthFixed,
                                    100.0f, 3);
            ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed,
                                    110.0f, 4);
            ImGui::TableHeadersRow();

            // Server-driven sort: re-order the local rows_ copy.
            if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
                specs->SpecsDirty && !rows_.empty()) {
                const ImGuiTableColumnSortSpecs& s = specs->Specs[0];
                std::sort(rows_.begin(), rows_.end(),
                          [&s](const Row& a, const Row& b) {
                              int cmp = 0;
                              switch (s.ColumnUserID) {
                                  case 0:
                                      cmp = _stricmp(a.title.c_str(),
                                                     b.title.c_str());
                                      break;
                                  case 1:
                                      cmp = (a.pid < b.pid)   ? -1
                                            : (a.pid > b.pid) ? 1
                                                              : 0;
                                      break;
                                  case 2:
                                      cmp = (a.cpuPercent < b.cpuPercent)   ? -1
                                            : (a.cpuPercent > b.cpuPercent) ? 1
                                                                            : 0;
                                      break;
                                  case 3:
                                      cmp = (a.workingSet < b.workingSet)   ? -1
                                            : (a.workingSet > b.workingSet) ? 1
                                                                            : 0;
                                      break;
                                  case 4:
                                      cmp = ((a.flags & 1u) ? 1 : 0) -
                                            ((b.flags & 1u) ? 1 : 0);
                                      break;
                              }
                              if (cmp == 0) {
                                  cmp = (a.surfaceId < b.surfaceId)   ? -1
                                        : (a.surfaceId > b.surfaceId) ? 1
                                                                      : 0;
                              }
                              return s.SortDirection ==
                                             ImGuiSortDirection_Ascending
                                         ? cmp < 0
                                         : cmp > 0;
                          });
                specs->SpecsDirty = false;
            }

            for (const Row& row : rows_) {
                ImGui::PushID(static_cast<int>(row.surfaceId));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (ImGui::Selectable("##row", selectedId_ == row.surfaceId,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    selectedId_ = row.surfaceId;
                }
                if (ImGui::IsItemHovered() &&
                    ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (jk::client::JKClientSurface* surface = Surface()) {
                        surface->SendWindowActivate(row.surfaceId);
                    }
                }
                ImGui::SameLine();
                ImGui::TextUnformatted(row.title.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%u", row.pid);
                ImGui::TableNextColumn();
                if (row.statsValid) {
                    ImGui::Text("%.1f", row.cpuPercent);
                } else {
                    ImGui::TextUnformatted("...");
                }
                ImGui::TableNextColumn();
                char mem[32];
                ImGui::TextUnformatted(FormatBytes(row.workingSet, mem));
                ImGui::TableNextColumn();
                const bool active = (row.flags & 1u) != 0;
                const bool minimized = (row.flags & 2u) != 0;
                ImGui::TextUnformatted(minimized  ? "minimized"
                                       : active   ? "active"
                                                  : "running");
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        // Actions on the selected row (same messages the taskbar sends).
        const Row* sel = FindRow(selectedId_);
        jk::client::JKClientSurface* surface = Surface();
        ImGui::BeginDisabled(!sel);
        if (ImGui::Button("Activate") && surface) {
            surface->SendWindowActivate(selectedId_);
        }
        ImGui::SameLine();
        const char* minLabel =
            (sel && (sel->flags & 2u)) ? "Restore" : "Minimize";
        if (ImGui::Button(minLabel) && surface) {
            surface->SendWindowMinimizeToggle(selectedId_);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("double-click a row to activate");

        // CPU history of the selected row.
        auto hit = history_.find(selectedId_);
        if (hit != history_.end()) {
            const History& hist = hit->second;
            ImGui::PlotLines("cpu history", hist.cpu, 90, hist.offset,
                             nullptr, 0.0f, 100.0f, ImVec2(-1, 60));
        } else {
            ImGui::Dummy(ImVec2(0.0f, 60.0f));
        }

        // Phase 1 IO dump panel, kept as a collapsible.
        if (ImGui::CollapsingHeader("Input dump")) {
            ImGui::BeginChild("io", ImVec2(0, 150), ImGuiChildFlags_Borders);
            ImGui::Text("MousePos: %.1f, %.1f", io.MousePos.x, io.MousePos.y);
            ImGui::Text("MouseDown: %d%d%d%d%d", io.MouseDown[0],
                        io.MouseDown[1], io.MouseDown[2], io.MouseDown[3],
                        io.MouseDown[4]);
            ImGui::Text("MouseWheel: %.2f %.2f", io.MouseWheelH, io.MouseWheel);
            ImGui::Text("WantCaptureMouse: %d", io.WantCaptureMouse);
            ImGui::Text("WantCaptureKeyboard: %d", io.WantCaptureKeyboard);
            ImGui::Text("WantTextInput: %d", io.WantTextInput);
            ImGui::Text("Modifiers: %s%s%s%s", io.KeyCtrl ? "Ctrl " : "",
                        io.KeyShift ? "Shift " : "", io.KeyAlt ? "Alt " : "",
                        io.KeySuper ? "Super " : "");
            ImGui::Text("%.1f FPS", io.Framerate);
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

} // namespace jk