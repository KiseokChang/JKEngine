#ifndef JKDATAFILE_H
#define JKDATAFILE_H

#include <JKTypes.h>
#include <cstdint>
#include <string>
#include <vector>

namespace jk {

// Lightweight reader/writer for the legacy JKDBASE fixed-length record files.
// Keeps the same on-disk layout:
//   .dat: [FILEHEADER: 6 bytes] [Record0..RecordN]
//   .tbl: [FILEHEADER: 6 bytes] [TableHead:int16] [TableTail:int16] [RefEntry0..N]
//
// Only raw record bytes are exposed here; schema-aware Record/Entry objects
// can be layered on top by callers.
struct JKDbHeader {
    uint16_t recordId = 0;
    uint16_t allocCount = 0;
    uint16_t realCount = 0;
};

struct JKDbRefEntry {
    uint8_t hasData = 0;
    int16_t prev = -1;
    int16_t next = -1;
};

class JKDataFile {
public:
    JKDataFile();
    ~JKDataFile();

    // Open an existing pair. If the files do not exist, an empty pair is created.
    bool Open(const std::string& baseName, uint16_t recordId, size_t recordSize);

    // Create a fresh empty pair.
    bool Create(const std::string& baseName, uint16_t recordId,
                uint16_t allocCount, size_t recordSize);

    void Close();
    bool IsOpen() const;

    uint16_t AllocCount() const { return header_.allocCount; }
    uint16_t RealCount() const { return header_.realCount; }
    size_t RecordSize() const { return recordSize_; }

    // Raw record access (physical index).
    bool IsRecordActive(uint16_t index) const;
    std::vector<uint8_t> ReadRecord(uint16_t index) const;
    bool WriteRecord(uint16_t index, const std::vector<uint8_t>& data);

    // Logical table operations.
    int16_t AddRecord(const std::vector<uint8_t>& data);
    bool DeleteRecord(uint16_t index);
    std::vector<uint16_t> GetActiveIndices() const;
    bool Rearrange(const std::vector<uint16_t>& newOrder);

private:
    std::string datPath_;
    std::string tblPath_;
    JKDbHeader header_;
    size_t recordSize_ = 0;
    int16_t tableHead_ = -1;
    int16_t tableTail_ = -1;

    bool ReadHeader(FILE* dat, FILE* tbl);
    bool WriteHeader(FILE* dat, FILE* tbl);
    bool ReadRefEntry(uint16_t index, JKDbRefEntry& out) const;
    bool WriteRefEntry(uint16_t index, const JKDbRefEntry& in);
    int16_t FindEmptySlot() const;
    bool Expand(uint16_t count);
    bool SyncTableHeadTail();
    std::string MakePath(const std::string& base, const char* ext) const;
};

} // namespace jk

#endif // JKDATAFILE_H
