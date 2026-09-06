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
        else if (key == "icon") icon = value;
        else if (key == "icon2x") icon2x = value;
        else if (key == "width") width = std::atoi(value.c_str());
        else if (key == "height") height = std::atoi(value.c_str());
    }
    return !name.empty() && !module.empty();
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

    uint8_t header[12] = {};
    if (std::fread(header, 1, sizeof(header), f) != sizeof(header) ||
        GetU32(header) != kMagic) {
        std::fprintf(stderr, "JKJkxFile: '%s' is not a JKX1 container\n", path.c_str());
        std::fclose(f);
        return false;
    }
    const uint32_t tocOffset = GetU32(header + 4);
    const uint32_t entryCount = GetU32(header + 8);

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

    const uint32_t tocOffset = sizeof(uint32_t) * 3;
    uint32_t payloadOffset = tocOffset + kEntrySize * static_cast<uint32_t>(entries.size());

    uint8_t header[12] = {};
    PutU32(header, kMagic);
    PutU32(header + 4, tocOffset);
    PutU32(header + 8, static_cast<uint32_t>(entries.size()));
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