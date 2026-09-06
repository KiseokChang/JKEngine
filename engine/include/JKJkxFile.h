#ifndef JKJKXFILE_H
#define JKJkxFile_H

// .jkx single-file app container (Phase C) — Android-APK-like bundle of
// { manifest, native module (DLL), icon PNGs }.
//
// Layout (all integers little-endian):
//   [0..3]   magic "JKX1"
//   [4..7]   tocOffset  (uint32)
//   [8..11]  entryCount (uint32)
//   [tocOffset..] entryCount * 128-byte entries:
//     type[4]   ("MANI" | "MODL" | "ICON")
//     name[60]  NUL-padded ("manifest.txt", "jkapp_minesweeper.dll",
//                          "launcher@1x.png", ...)
//     offset[4] absolute file offset of the payload
//     size[4]   payload size in bytes
//
// The manifest is a small key=value text file (see JkxManifest). The client
// host (--jkx) extracts the MODL entry to a temp file and LoadLibrarys it;
// the server launcher decodes ICON entries from memory for launcher cells.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace jk {

struct JkxManifest {
    std::string name;    // spawn key, e.g. "minesweeper"
    std::string title;   // display title (informational; module meta is
                         // authoritative at runtime)
    std::string module;  // MODL entry name to load
    std::string icon;    // ICON entry name, @1x ("" when absent)
    std::string icon2x;  // ICON entry name, @2x ("" when absent)
    int width = 0;
    int height = 0;

    // Parses "key=value" lines; unknown keys are ignored. Returns false when
    // the required keys (name, module) are missing.
    bool Parse(const std::string& text);
};

class JKJkxFile {
public:
    struct Entry {
        char type[5];       // NUL-terminated 4cc: "MANI", "MODL", "ICON"
        std::string name;
        uint32_t offset = 0;
        uint32_t size = 0;
    };

    JKJkxFile() = default;
    ~JKJkxFile();

    JKJkxFile(const JKJkxFile&) = delete;
    JKJkxFile& operator=(const JKJkxFile&) = delete;

    // Open a container and read its TOC. The manifest (if present) is parsed
    // eagerly. The file stays open for ReadEntry.
    bool Open(const std::string& path);

    const std::vector<Entry>& Entries() const { return entries_; }
    int EntryCount() const { return static_cast<int>(entries_.size()); }
    const JkxManifest& Manifest() const { return manifest_; }
    const std::string& Path() const { return path_; }

    // Returns the entry index or -1. type may be nullptr to match any type.
    int FindEntry(const char* type, const std::string& name) const;

    // Reads one entry's payload into out. Returns false on I/O error.
    bool ReadEntry(int index, std::vector<uint8_t>& out) const;

    // Static entry type for a name, following the packer's convention:
    // "manifest.*" -> "MANI", "*.dll" -> "MODL", otherwise "ICON".
    static const char* TypeForName(const std::string& name);

    // Packs entries into a new container. entries = (name, bytes); the entry
    // type is derived via TypeForName. A "manifest.txt" entry, if present, is
    // also parsed and stored as the container manifest.
    static bool Write(const std::string& path,
                      const std::vector<std::pair<std::string, std::vector<uint8_t>>>& entries);

private:
    void* file_ = nullptr;              // FILE* (void* to avoid <cstdio> in header)
    std::string path_;
    std::vector<Entry> entries_;
    JkxManifest manifest_;
};

} // namespace jk

#endif // JKJKXFILE_H