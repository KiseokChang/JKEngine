#include <JKJkxFile.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace jk {

namespace {

constexpr uint32_t kMagic = 0x31584B4A;      // "JKX1" little-endian
constexpr uint32_t kEntrySize = 128;
constexpr uint32_t kNameSize = 60;
constexpr uint32_t kJkxVersion = 1;          // docs/21 §header — v1 added version+codec
constexpr uint32_t kJkxCodecRaw = 0;         // codec registry: 0 = raw/stored

void PutU32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

uint32_t GetU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

std::string Trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

} // namespace

bool JkxManifest::Parse(const std::string& text) {
    size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == std::string::npos) eol = text.size();
        const std::string line = Trim(text.substr(pos, eol - pos));
        pos = eol + 1;
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Trim(line.substr(0, eq));
        const std::string value = Trim(line.substr(eq + 1));
        if (key == "name") name = value;
        else if (key == "title") title = value;
        else if (key == "module") module = value;
        else if (key == "script") script = value;
        else if (key == "icon") icon = value;
        else if (key == "icon2x") icon2x = value;
        else if (key == "width") width = std::atoi(value.c_str());
        else if (key == "height") height = std::atoi(value.c_str());
        else if (key == "scriptfile") scriptfile = value;  // docs/60 §2.2
        else if (key == "capabilities") capabilities = value;  // docs/74 —
            // 원문 보존: 정규화는 소비자 몫(스펙 §3.2)
        else if (key == "watch") watch = std::atoi(value.c_str());
    }
    return !name.empty() && !module.empty();
}

// I2 — 재생성 MANI에 authored 필드를 보존한다(docs/67 단 2 룰링: 화이트리스트
// 설계 폐기). 규칙: authored 행 순서 유지, regenerated에 키가 있는 행은 그
// 값(첫 등장 행)으로 치환, authored에 없는 regenerated 키는 뒤에 추가,
// 주석/빈 행 통과. (레슨: 미래 필드 — capabilities= — 도 "unknown"이 아니라
// 보존 대상이다.)
//
// 2-pass 행 재조립으로 구현 — 계획서 Step 3의 raw-offset replace 루프는 치환
// 길이가 원 행과 다를 때 원본 eol 좌표가 어긋나 이후 중복 키 행을 놓칠 수
// 있다(검토 룰링: 2-pass가 ruling). authored 행을 벡터로 쪼개고, 각 행을 reg
// 키와 대조해 행 문자열만 교체, 마지막에 join + 미존재 reg 키 덧붙임.
std::string JkxManifestMerge(const std::string& authored,
                             const std::string& regenerated) {
    if (authored.empty()) return regenerated;

    // regenerated의 key -> 첫 등장 "key=value" 행 전체 (치환 소스, 순서 유지).
    // reg 쪽 중복 키는 불법이므로 첫 등장만 살리고 이후는 무시.
    struct RegEntry { std::string key; std::string line; bool replaced; };
    std::vector<RegEntry> regEntries;
    {
        size_t pos = 0;
        while (pos < regenerated.size()) {
            size_t eol = regenerated.find('\n', pos);
            if (eol == std::string::npos) eol = regenerated.size();
            const std::string line = regenerated.substr(pos, eol - pos);
            pos = eol + 1;
            const std::string t = Trim(line);
            if (t.empty() || t[0] == '#') continue;
            const size_t eq = t.find('=');
            if (eq == std::string::npos) continue;
            const std::string key = Trim(t.substr(0, eq));
            bool seen = false;
            for (const RegEntry& re : regEntries) {
                if (re.key == key) { seen = true; break; }
            }
            if (!seen) regEntries.push_back({key, t, false});
        }
    }

    // authored 행 pass: 행을 쪼개 각 행을 reg 키와 대조, 치환은 행 교체뿐 —
    // 오프셋 추적이 전혀 없으므로 치환 길이가 달라도 안전.
    std::vector<std::string> lines;
    {
        size_t pos = 0;
        while (pos < authored.size()) {
            size_t eol = authored.find('\n', pos);
            if (eol == std::string::npos) {
                lines.push_back(authored.substr(pos));
                break;
            }
            lines.push_back(authored.substr(pos, eol - pos));
            pos = eol + 1;
        }
    }
    for (std::string& line : lines) {
        const std::string t = Trim(line);
        if (t.empty() || t[0] == '#') continue;
        const size_t eq = t.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Trim(t.substr(0, eq));
        for (RegEntry& re : regEntries) {
            if (re.key != key) continue;
            // authored 중복 키 행은 각각 치환(간단·멱등). CRLF 파일이면 행의
            // CR은 그대로 유지(내용만 교체).
            std::string repl = re.line;
            if (!line.empty() && line.back() == '\r') repl += '\r';
            line = std::move(repl);
            re.replaced = true;
            break;
        }
    }

    std::string out;
    for (const std::string& l : lines) {
        out += l;
        out += '\n';
    }
    // authored에 없는 regenerated 키를 regenerated 순서대로 뒤에 추가.
    for (const RegEntry& re : regEntries) {
        if (!re.replaced) out += re.line + "\n";
    }
    return out;
}

JKJkxFile::~JKJkxFile() {
    if (file_) {
        std::fclose(static_cast<std::FILE*>(file_));
        file_ = nullptr;
    }
}

const char* JKJkxFile::TypeForName(const std::string& name) {
    if (name.rfind("manifest.", 0) == 0) return "MANI";
    if (name.size() >= 4 && name.compare(name.size() - 4, 4, ".dll") == 0) return "MODL";
    if (name.size() >= 3 && name.compare(name.size() - 3, 3, ".js") == 0) return "SCRI";
    return "ICON";
}

bool JKJkxFile::Open(const std::string& path) {
    if (file_) {
        std::fclose(static_cast<std::FILE*>(file_));
        file_ = nullptr;
    }
    entries_.clear();
    manifest_ = JkxManifest{};
    path_.clear();

    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        std::fprintf(stderr, "JKJkxFile: cannot open '%s'\n", path.c_str());
        return false;
    }

    uint8_t header[20] = {};
    const size_t got = std::fread(header, 1, sizeof(header), f);
    if (got < 12 || GetU32(header) != kMagic) {
        std::fprintf(stderr, "JKJkxFile: '%s' is not a JKX1 container\n", path.c_str());
        std::fclose(f);
        return false;
    }
    const uint32_t tocOffset = GetU32(header + 4);
    const uint32_t entryCount = GetU32(header + 8);
    // v0 files (tocOffset == 12) predate the version/codec fields; v1+ keeps
    // the TOC at >= 20 so both fields fit between the counts and the TOC.
    if (tocOffset >= sizeof(header)) {
        if (got < sizeof(header)) {
            std::fprintf(stderr, "JKJkxFile: truncated header in '%s'\n", path.c_str());
            std::fclose(f);
            return false;
        }
        version_ = GetU32(header + 12);
        codec_ = GetU32(header + 16);
    }
    if (version_ > kJkxVersion) {
        std::fprintf(stderr,
                     "JKJkxFile: '%s' has unsupported version %u (reader supports <= %u)\n",
                     path.c_str(), version_, kJkxVersion);
        std::fclose(f);
        return false;
    }
    if (codec_ != kJkxCodecRaw) {
        std::fprintf(stderr,
                     "JKJkxFile: '%s' uses codec %u, which this reader cannot decode\n",
                     path.c_str(), codec_);
        std::fclose(f);
        return false;
    }

    bool ok = true;
    for (uint32_t i = 0; ok && i < entryCount; ++i) {
        uint8_t raw[kEntrySize] = {};
        if (std::fseek(f, static_cast<long>(tocOffset + i * kEntrySize), SEEK_SET) != 0 ||
            std::fread(raw, 1, kEntrySize, f) != kEntrySize) {
            ok = false;
            break;
        }
        Entry entry;
        std::memcpy(entry.type, raw, 4);
        entry.type[4] = '\0';
        entry.name.assign(reinterpret_cast<const char*>(raw + 4),
                          strnlen(reinterpret_cast<const char*>(raw + 4), kNameSize));
        entry.offset = GetU32(raw + 64);
        entry.size = GetU32(raw + 68);
        entries_.push_back(std::move(entry));
    }

    if (ok) {
        file_ = f;
        path_ = path;
        const int mani = FindEntry("MANI", "manifest.txt");
        if (mani >= 0) {
            std::vector<uint8_t> text;
            if (ReadEntry(mani, text)) {
                text.push_back(0);
                manifest_.Parse(reinterpret_cast<const char*>(text.data()));
            }
        }
        return true;
    }

    std::fprintf(stderr, "JKJkxFile: bad TOC in '%s'\n", path.c_str());
    std::fclose(f);
    return false;
}

int JKJkxFile::FindEntry(const char* type, const std::string& name) const {
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (type && std::strcmp(entries_[i].type, type) != 0) continue;
        if (!name.empty() && entries_[i].name != name) continue;
        return static_cast<int>(i);
    }
    return -1;
}

bool JKJkxFile::ReadEntry(int index, std::vector<uint8_t>& out) const {
    if (index < 0 || index >= static_cast<int>(entries_.size()) || !file_) return false;
    const Entry& e = entries_[static_cast<size_t>(index)];
    out.resize(e.size);
    if (e.size == 0) return true;
    std::fseek(static_cast<std::FILE*>(file_), static_cast<long>(e.offset), SEEK_SET);
    return std::fread(out.data(), 1, e.size, static_cast<std::FILE*>(file_)) == e.size;
}

bool JKJkxFile::Write(const std::string& path,
                      const std::vector<std::pair<std::string, std::vector<uint8_t>>>& entries) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        std::fprintf(stderr, "JKJkxFile: cannot create '%s'\n", path.c_str());
        return false;
    }

    const uint32_t tocOffset = sizeof(uint32_t) * 5;  // magic/toc/count/version/codec
    uint32_t payloadOffset = tocOffset + kEntrySize * static_cast<uint32_t>(entries.size());

    uint8_t header[20] = {};
    PutU32(header, kMagic);
    PutU32(header + 4, tocOffset);
    PutU32(header + 8, static_cast<uint32_t>(entries.size()));
    PutU32(header + 12, kJkxVersion);
    PutU32(header + 16, kJkxCodecRaw);
    bool ok = std::fwrite(header, 1, sizeof(header), f) == sizeof(header);

    std::vector<uint8_t> entryRaw(kEntrySize, 0);
    for (const auto& [name, data] : entries) {
        std::memset(entryRaw.data(), 0, entryRaw.size());
        const char* type = TypeForName(name);
        std::memcpy(entryRaw.data(), type, 4);
        std::memcpy(entryRaw.data() + 4, name.c_str(),
                    std::min(name.size(), static_cast<size_t>(kNameSize - 1)));
        PutU32(entryRaw.data() + 64, payloadOffset);
        PutU32(entryRaw.data() + 68, static_cast<uint32_t>(data.size()));
        if (std::fwrite(entryRaw.data(), 1, kEntrySize, f) != kEntrySize) {
            ok = false;
            break;
        }
        payloadOffset += static_cast<uint32_t>(data.size());
    }

    for (const auto& [name, data] : entries) {
        if (!ok) break;
        if (!data.empty() &&
            std::fwrite(data.data(), 1, data.size(), f) != data.size()) {
            ok = false;
        }
    }

    if (std::fclose(f) != 0) ok = false;
    if (!ok) {
        std::fprintf(stderr, "JKJkxFile: failed to write '%s'\n", path.c_str());
        std::remove(path.c_str());
    }
    return ok;
}

} // namespace jk