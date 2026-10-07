// Task manager client (ImGui Phase 2, docs/23 §11.2): shell-protocol window
// list + self-sampled per-process stats. See ClientTaskmgrApp.h.
#include <apps/ClientTaskmgrApp.h>

#include <imgui_impl_jkwindow.h>
#include <implot.h>
#include "theme/JKThemeImGui.h"
#include <JKWindow.h>
#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <port/JKCrtShim.h>
#include <set>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#endif
#ifndef _WIN32
// posix leg (app-coverage Task 5): psapi 대응물은 procfs — 파일 읽기 원만 필요.
#include <cerrno>    // EINTR
#include <cstdlib>   // strtoul
#include <fcntl.h>   // O_RDONLY|O_CLOEXEC
#include <unistd.h>  // getpid/sysconf/read/close
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
        const auto& t = jk::theme::current();
        dc.SetColor(t.appClearBg.r, t.appClearBg.g, t.appClearBg.b, 255);
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
#endif

// FormatBytes는 순수 포맷팅 — 플랫폼 중립(게이트 제거, linux stage 3 task 2).
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

#ifndef _WIN32
// --- posix leg (app-coverage Task 5, 2026-10-07) ---------------------------
// win32 leg는 psapi OpenProcess/GetProcessTimes/GetProcessMemoryInfo로
// per-process stats를 얻는다(FileTimeToU64 위 참조). posix의 psapi 대응물은
// /proc — 행당 3종 읽기(comm/stat/status)가 최소 계약(brief). prev_ 단위는
// leg별 로컬 계약(헤더 주석은 win32 뷰만 기술): procTime은 win32 100ns 단위 ↔
// posix CLK_TCK 틱, wallTime은 win32 벽시계 100ns ↔ posix steady ms.
// CPU는 절대값이 아니라 샘플 간 Δ — 갱신 37 렛슨(수명평균 %CPU는 판정 무용).

// 작은 procfs 파일 원문 읽기. 실패(부재 = exited, EACCES = hidepid 차단)는
// '통계 없음'으로 곧장 통보된다 — 조용한 0 채움 금지.
bool ReadProcFile(const char* path, char (&buf)[2048], size_t* lenOut) {
    const int fd = ::open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    size_t total = 0;
    while (total < sizeof(buf) - 1) {
        const ssize_t n = ::read(fd, buf + total, sizeof(buf) - 1 - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;
        total += static_cast<size_t>(n);
    }
    ::close(fd);
    buf[total] = '\0';
    *lenOut = total;
    return total > 0;
}

// /proc/<pid>/stat → utime+stime(틱). comm에 공백·')'이 있어도 필드가 시프트되지
// 않게 마지막 ')' 이후부터 토큰화한다(셸 ps 파서와 같은 근거 — awk $14는
// comm 공백에서 어긋난다, wsl_cpu_pacing.sh 원전이 단순 필드 번호였던 이유).
bool ReadProcCpuTicks(const char* path, uint64_t* ticksOut) {
    char buf[2048];
    size_t n = 0;
    if (!ReadProcFile(path, buf, &n)) return false;
    const char* body = std::strrchr(buf, ')');  // 마지막 ')'이 comm 닫음
    if (!body) return false;
    // body+1 토큰: state ppid pgrp session tty tpgid flags minflt cminflt
    //               majflt cmajflt utime stime — 11개 억제 후 2개 수취.
    unsigned long long utime = 0, stime = 0;
    if (std::sscanf(body + 1,
                    "%*s %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %llu %llu",
                    &utime, &stime) != 2) {
        return false;
    }
    *ticksOut = utime + stime;
    return true;
}

// /proc/<pid>/status VmRSS(kB) → 바이트. RSS 0(커널 스레드)도 유효 값.
bool ReadProcVmRss(const char* path, uint64_t* bytesOut) {
    char buf[2048];
    size_t n = 0;
    if (!ReadProcFile(path, buf, &n)) return false;
    const char* v = std::strstr(buf, "VmRSS:");
    if (!v) return false;
    v += 6;
    while (*v == ' ' || *v == '\t') ++v;
    *bytesOut = std::strtoul(v, nullptr, 10) * 1024ull;
    return true;
}

// /proc/<pid>/comm — 프로세스 이름(커널 15자 절단). 툴팁 + 빈 창제목 폴백.
bool ReadProcComm(const char* path, std::string* out) {
    char buf[2048];
    size_t n = 0;
    if (!ReadProcFile(path, buf, &n)) return false;
    std::string s(buf);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\0')) s.pop_back();
    if (s.empty()) return false;
    *out = std::move(s);
    return true;
}

long ClkTck() {
    static const long v = ::sysconf(_SC_CLK_TCK);
    return v > 0 ? v : 100;  // sysconf 실패 폴백(CONFIG_HZ=100 전통 기본)
}

uint64_t WallMs() {
    // 벽시계 대신 steady — 벽시계 역행(NTP 등)은 Δ 게이트(nowWall > prev)를
    // 영구 오염시킨다. Δ 산출에 필요한 건 단조 시원뿐.
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// pid → comm 지도(SampleProcesses가 채우고 위생 소각) + /proc 차단 플래그.
// 헤더 무변 계약(단일 파일 이식)이므로 leg 상태는 파일 스코프로.
std::map<uint32_t, std::string> g_procNames;
bool g_procBlocked = false;   // 행은 살아있는데 /proc 읽기가 전멸(hidepid 등)
int  g_procFailStreak = 0;
const std::string* LookupProcName(uint32_t pid) {
    const auto it = g_procNames.find(pid);
    return it == g_procNames.end() ? nullptr : &it->second;
}
#endif  // !_WIN32
} // namespace

ClientTaskmgrApp::~ClientTaskmgrApp() = default;

void ClientTaskmgrApp::OnInit() {
    auto main = std::make_unique<TaskmgrRoot>("Task Manager");
    main->SetWindowRect(JKRect{ 0, 0, 900, 620 });
    main->SetAttrFlags(WA_CHROMELESS); // server close button only
    SetMainWindow(std::move(main));

    SetTimerInterval(16); // ~60 Hz frame cadence (same clock as the demo)

    ImGui::CreateContext();
    jk::theme::ApplyImGuiTheme(); // JKTheme 팔레트 봉합 (P2 단계 3)
    ImPlot::CreateContext();
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

#ifndef _WIN32
    // posix leg (Task 5): pid·코어 수는 getpid/sysconf — win32
    // GetCurrentProcessId/GetSystemInfo 패리티. numCores는 Δ CPU%의 기준 용량
    // (총 머신 % — win32 leg와 동일 스타일).
    selfPid_ = static_cast<uint32_t>(::getpid());
    const long cores = ::sysconf(_SC_NPROCESSORS_ONLN);
    numCores_ = cores > 0 ? static_cast<unsigned>(cores) : 1u;
    g_procNames.clear();
    g_procBlocked = false;
    g_procFailStreak = 0;
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
        ImPlot::DestroyContext(); // while the ImGui context is still current
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

#ifndef _WIN32
    // posix leg (Task 5): 행은 이미 셸 프로토콜 창 목록(rows_)이 주고, 통계는
    // /proc에서 행당 3종 읽기로 채운다. CPU는 Δ — 샘플 창 밖 절대값 없음.
    const uint64_t nowWall = WallMs();
    int attempted = 0, sampled = 0;
    for (Row& row : rows_) {
        if (row.pid == 0) continue;
        ++attempted;
        char path[64];
        uint64_t procTime = 0;
        std::snprintf(path, sizeof(path), "/proc/%u/stat", row.pid);
        if (!ReadProcCpuTicks(path, &procTime)) {
            row.statsValid = false;  // exited, or /proc denied (hidepid)
            continue;
        }
        ++sampled;
        std::snprintf(path, sizeof(path), "/proc/%u/status", row.pid);
        uint64_t rss = 0;
        if (ReadProcVmRss(path, &rss)) row.workingSet = rss;

        auto it = prev_.find(row.pid);
        if (it != prev_.end() && nowWall > it->second.wallTime &&
            procTime >= it->second.procTime) {  // pid 재사용 역행 보호(posix leg)
            const uint64_t dProc = procTime - it->second.procTime;  // ticks
            const uint64_t dWall = nowWall - it->second.wallTime;   // ms
            // Task-Manager style: percent of total machine capacity.
            const double dSec = static_cast<double>(dWall) / 1000.0;
            if (dSec > 0.0) {
                row.cpuPercent =
                    100.0 * (static_cast<double>(dProc) / ClkTck()) / dSec /
                    static_cast<double>(numCores_);
            }
            row.statsValid = true;
        }
        prev_[row.pid] = { procTime, nowWall };

        std::snprintf(path, sizeof(path), "/proc/%u/comm", row.pid);
        std::string comm;
        if (ReadProcComm(path, &comm)) {
            g_procNames[row.pid] = comm;
            if (row.title.empty()) row.title = comm;  // 이름 = comm(보조 표시)
        }

        History& hist = history_[row.surfaceId];
        hist.cpu[hist.offset] = row.statsValid
                                    ? static_cast<float>(row.cpuPercent)
                                    : 0.0f;
        hist.offset = (hist.offset + 1) % 90;
        if (hist.samples < 90) ++hist.samples;
    }

    // 정직 뷰(controller ruling): 행이 살아있는데 /proc 읽기가 전멸하면 빈
    // "..." 표가 아니라 메시지로 말한다. 2연속 샘플 확정 — 방금 죽은 pid 경주
    // 노이즈 1회를 흡수.
    if (attempted > 0 && sampled == 0) {
        ++g_procFailStreak;
    } else {
        g_procFailStreak = 0;
    }
    g_procBlocked = g_procFailStreak >= 2;

    // 이름 지도 위생 — 행 목록에서 사라진 pid 소각(장기 세션 성장 방지).
    for (auto it = g_procNames.begin(); it != g_procNames.end();) {
        bool alive = false;
        for (const Row& r : rows_) {
            if (r.pid == it->first) { alive = true; break; }
        }
        if (alive) ++it; else it = g_procNames.erase(it);
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

#ifndef _WIN32
        // posix leg (Task 5): /proc 읽기가 막혔을 때(hidepid 마운트 등)의 정직한
        // 표시 — 침묵하는 "..." 표 방지(controller ruling).
        if (g_procBlocked) {
            ImGui::TextDisabled("per-process stats unavailable — /proc reads "
                                "denied (hidepid mount?)");
        }
#endif

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
                                      cmp = jk::crt::Stricmp(a.title.c_str(),
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
#ifndef _WIN32
                // posix leg (Task 5): 프로세스 이름(comm)은 툴팁으로 — 창 제목만
                // 으로는 같은 프로세스의 여러 창을 구분하기 어렵다.
                if (const std::string* nm = LookupProcName(row.pid)) {
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s  (pid %u)", nm->c_str(),
                                          row.pid);
                    }
                }
#endif

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

        // CPU history of the selected row — ImPlot ring-buffer line
        // (offset makes the 90-sample ring draw from oldest to newest).
        auto hit = history_.find(selectedId_);
        if (hit != history_.end()) {
            const History& hist = hit->second;
            if (ImPlot::BeginPlot("cpu history", ImVec2(-1, 60),
                                  ImPlotFlags_CanvasOnly)) {
                ImPlot::SetupAxes(nullptr, nullptr,
                                  ImPlotAxisFlags_NoDecorations,
                                  ImPlotAxisFlags_NoDecorations |
                                      ImPlotAxisFlags_Lock);
                ImPlot::SetupAxesLimits(0, 90, 0, 100, ImGuiCond_Always);
                ImPlot::PlotLine("cpu", hist.cpu, 90, 1.0, 0.0,
                                 ImPlotSpec(ImPlotProp_Offset, hist.offset));
                ImPlot::EndPlot();
            }
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


// P3 theme hot-swap (docs/52): the palette was snapshotted into ImGuiStyle
// at OnInit - re-apply it after a preset swap.
void ClientTaskmgrApp::OnThemeChanged() { jk::theme::ApplyImGuiTheme(); }

} // namespace jk