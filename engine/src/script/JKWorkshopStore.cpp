// JKWorkshopStore 구현 (docs/67 단 1) — 이 TU만 windows.h를 포함한다.
// 선례: FilesListOpJson(JKWindowServer.cpp:2880)의 SDK FindFirstFileA 열거 +
// FILETIME→epoch 절단. 수기 Win32 선언 금지(docs/60 §7 세그폴트 전례).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <script/JKWorkshopStore.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace jk::workshop {
namespace {

constexpr size_t kMaxSlotName = 32;
constexpr size_t kMaxGens = 20;  // 슬롯당 세대 캡 — 256KiB 캡 슬롯 최대 ~5MiB

bool ReadFileAll(const std::string& path, std::string& out) {
    std::FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) return false;
    char buf[4096];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

bool WriteFileAll(const std::string& path, const std::string& data) {
    // .new→rename 사다리(JKWindowServer.cpp:2620-2629 1세대 규약의 다세대 확장):
    // 부분 쓰기가 진짜 이름으로 도달하지 않는다. UCRT rename은 대상 존재 시
    // 실패한다(POSIX와 다름 — 2026-09-26 라이브 게이트 c7 실측: 슬롯 재전환의
    // .current 재쓰기가 조용히 눌먹음). tmp 완성 후 remove+rename — 부분 쓰기
    // 방어는 그대로 유지된다.
    const std::string tmp = path + ".new";
    std::FILE* f = nullptr;
    if (fopen_s(&f, tmp.c_str(), "wb") != 0 || !f) return false;
    const size_t w = std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    if (w != data.size()) {
        std::remove(tmp.c_str());
        return false;
    }
    std::remove(path.c_str());  // 존재하지 않아도 무해 — rename 선결조건
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

} // namespace

bool IsValidSlotName(const std::string& slot) {
    if (slot.empty() || slot.size() > kMaxSlotName) return false;
    for (char c : slot) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

std::string DirOf(const std::string& filePath) {
    const size_t pos = filePath.find_last_of("\\/");
    if (pos == std::string::npos) return ".";
    return filePath.substr(0, pos);
}

std::string HistoryDir(const std::string& scriptsDir, const std::string& slot) {
    return scriptsDir + "\\.history\\" + slot;
}

bool ListSlots(const std::string& scriptsDir, std::vector<std::string>& out) {
    out.clear();
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((scriptsDir + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool more = true;
    while (more) {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            const std::string name = fd.cFileName;
            // *.js 만. 최소 4자("x.js") — 스템이 비면 무효.
            if (name.size() > 3 && name.compare(name.size() - 3, 3, ".js") == 0) {
                std::string stem = name.substr(0, name.size() - 3);
                if (IsValidSlotName(stem)) out.push_back(stem);
            }
        }
        more = FindNextFileA(h, &fd) != 0;
    }
    FindClose(h);
    std::sort(out.begin(), out.end());
    return true;
}

bool ListHistory(const std::string& scriptsDir, const std::string& slot,
                 std::vector<HistoryEntry>& out) {
    out.clear();
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((HistoryDir(scriptsDir, slot) + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return false;  // 세대 없음(첫 쓰기 전)
    bool more = true;
    while (more) {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            const std::string name = fd.cFileName;
            // NNNN.js 만 — 4자리 숫자 스템.
            if (name.size() == 7 && name.compare(4, 3, ".js") == 0) {
                bool digits = true;
                for (int i = 0; i < 4; ++i)
                    if (name[i] < '0' || name[i] > '9') { digits = false; break; }
                if (digits) {
                    HistoryEntry e;
                    e.gen = std::atoi(name.c_str());
                    e.bytes = (static_cast<size_t>(fd.nFileSizeHigh) << 32) |
                              fd.nFileSizeLow;
                    // FILETIME은 구조체 — 고·하위 32비트를 합쳐 epoch 초로
                    // (FilesListOpJson의 절단 규약 동일).
                    const unsigned long long ft =
                        (static_cast<unsigned long long>(fd.ftLastWriteTime.dwHighDateTime)
                         << 32) | fd.ftLastWriteTime.dwLowDateTime;
                    e.mtimeSecs = ft > 116444736000000000ULL
                        ? static_cast<long long>((ft - 116444736000000000ULL) /
                                                 10000000ULL)
                        : 0;
                    out.push_back(e);
                }
            }
        }
        more = FindNextFileA(h, &fd) != 0;
    }
    FindClose(h);
    std::sort(out.begin(), out.end(),
              [](const HistoryEntry& a, const HistoryEntry& b) {
                  return a.gen < b.gen;
              });
    return true;
}

int AppendSnapshot(const std::string& scriptsDir, const std::string& slot,
                   const std::string& prevSource) {
    if (prevSource.empty()) return 0;  // 신규 슬롯 첫 쓰기 — gen 0 = 없음
    const std::string dir = HistoryDir(scriptsDir, slot);
    // CreateDirectoryA는 마지막 성분만 만든다 — 두 층을 각각 시도(이미 있으면
    // 오류 무시).
    CreateDirectoryA((scriptsDir + "\\.history").c_str(), nullptr);
    CreateDirectoryA(dir.c_str(), nullptr);

    std::vector<HistoryEntry> gens;
    ListHistory(scriptsDir, slot, gens);
    int gen = 1;
    if (!gens.empty()) gen = gens.back().gen + 1;

    char name[16];
    std::snprintf(name, sizeof(name), "%04d.js", gen);
    if (!WriteFileAll(dir + "\\" + name, prevSource)) return 0;

    // 캡 프룬 — 최오소부터 지워 20세대를 유지.
    gens.clear();
    if (ListHistory(scriptsDir, slot, gens)) {
        int excess = static_cast<int>(gens.size()) - static_cast<int>(kMaxGens);
        for (int i = 0; excess > 0 && i < static_cast<int>(gens.size()); ++i, --excess) {
            std::snprintf(name, sizeof(name), "%04d.js", gens[i].gen);
            std::remove((dir + "\\" + name).c_str());
        }
    }
    return gen;
}

bool ReadGen(const std::string& scriptsDir, const std::string& slot, int gen,
             std::string& out) {
    if (gen <= 0) return false;
    char name[16];
    std::snprintf(name, sizeof(name), "%04d.js", gen);
    return ReadFileAll(HistoryDir(scriptsDir, slot) + "\\" + name, out);
}

bool ReadCurrentSlotFile(const std::string& scriptsDir,
                         const std::string& appName, std::string& slot) {
    if (appName.empty()) return false;
    std::string content;
    if (!ReadFileAll(scriptsDir + "\\.current_" + appName, content)) return false;
    // 공백 트림 — 메모장으로 고쳐도 살아남게.
    size_t a = 0, b = content.size();
    while (a < b && (content[a] == ' ' || content[a] == '\r' ||
                     content[a] == '\n' || content[a] == '\t')) ++a;
    while (b > a && (content[b - 1] == ' ' || content[b - 1] == '\r' ||
                     content[b - 1] == '\n' || content[b - 1] == '\t')) --b;
    slot = content.substr(a, b - a);
    return true;
}

bool WriteCurrentSlotFile(const std::string& scriptsDir,
                          const std::string& appName, const std::string& slot) {
    if (appName.empty()) return false;
    return WriteFileAll(scriptsDir + "\\.current_" + appName, slot);
}

} // namespace jk::workshop