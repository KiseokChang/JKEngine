#include <JKDataFile.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace jk {

namespace {

bool FileExists(const char* path) {
    FILE* fp = std::fopen(path, "rb");
    if (fp) {
        std::fclose(fp);
        return true;
    }
    return false;
}

} // anonymous namespace

JKDataFile::JKDataFile() = default;

JKDataFile::~JKDataFile() {
    Close();
}

std::string JKDataFile::MakePath(const std::string& base, const char* ext) const {
    return base + ext;
}

bool JKDataFile::Open(const std::string& baseName, uint16_t recordId, size_t recordSize) {
    Close();
    datPath_ = MakePath(baseName, ".dat");
    tblPath_ = MakePath(baseName, ".tbl");
    recordSize_ = recordSize;

    if (!FileExists(datPath_.c_str()) || !FileExists(tblPath_.c_str())) {
        return Create(baseName, recordId, 0, recordSize);
    }

    FILE* dat = std::fopen(datPath_.c_str(), "rb");
    FILE* tbl = std::fopen(tblPath_.c_str(), "rb");
    if (!dat || !tbl) {
        if (dat) std::fclose(dat);
        if (tbl) std::fclose(tbl);
        return false;
    }

    bool ok = ReadHeader(dat, tbl);
    std::fclose(dat);
    std::fclose(tbl);

    if (!ok || header_.recordId != recordId || recordSize_ == 0) {
        Close();
        return false;
    }
    return true;
}

bool JKDataFile::Create(const std::string& baseName, uint16_t recordId,
                        uint16_t allocCount, size_t recordSize) {
    Close();
    datPath_ = MakePath(baseName, ".dat");
    tblPath_ = MakePath(baseName, ".tbl");
    recordSize_ = recordSize;
    header_.recordId = recordId;
    header_.allocCount = allocCount;
    header_.realCount = 0;
    tableHead_ = -1;
    tableTail_ = -1;

    FILE* dat = std::fopen(datPath_.c_str(), "wb");
    FILE* tbl = std::fopen(tblPath_.c_str(), "wb");
    if (!dat || !tbl) {
        if (dat) std::fclose(dat);
        if (tbl) std::fclose(tbl);
        return false;
    }

    if (!WriteHeader(dat, tbl)) {
        std::fclose(dat);
        std::fclose(tbl);
        return false;
    }

    uint8_t blank = ' ';
    for (uint16_t i = 0; i < allocCount; ++i) {
        for (size_t b = 0; b < recordSize; ++b) {
            std::fwrite(&blank, 1, 1, dat);
        }
    }

    JKDbRefEntry empty{ 0, -1, -1 };
    for (uint16_t i = 0; i < allocCount; ++i) {
        std::fwrite(&empty, sizeof(empty), 1, tbl);
    }

    std::fclose(dat);
    std::fclose(tbl);
    return true;
}

void JKDataFile::Close() {
    datPath_.clear();
    tblPath_.clear();
    header_ = JKDbHeader();
    recordSize_ = 0;
    tableHead_ = -1;
    tableTail_ = -1;
}

bool JKDataFile::IsOpen() const {
    return !datPath_.empty() && !tblPath_.empty() && recordSize_ > 0;
}


bool JKDataFile::ReadHeader(FILE* dat, FILE* tbl) {
    if (!dat || !tbl) return false;
    std::rewind(dat);
    if (std::fread(&header_.recordId, sizeof(header_.recordId), 1, dat) != 1) return false;
    if (std::fread(&header_.allocCount, sizeof(header_.allocCount), 1, dat) != 1) return false;
    if (std::fread(&header_.realCount, sizeof(header_.realCount), 1, dat) != 1) return false;

    std::rewind(tbl);
    JKDbHeader tblHeader;
    if (std::fread(&tblHeader.recordId, sizeof(tblHeader.recordId), 1, tbl) != 1) return false;
    if (std::fread(&tblHeader.allocCount, sizeof(tblHeader.allocCount), 1, tbl) != 1) return false;
    if (std::fread(&tblHeader.realCount, sizeof(tblHeader.realCount), 1, tbl) != 1) return false;
    if (tblHeader.recordId != header_.recordId ||
        tblHeader.allocCount != header_.allocCount ||
        tblHeader.realCount != header_.realCount) {
        return false;
    }
    if (std::fread(&tableHead_, sizeof(tableHead_), 1, tbl) != 1) return false;
    if (std::fread(&tableTail_, sizeof(tableTail_), 1, tbl) != 1) return false;
    return true;
}

bool JKDataFile::WriteHeader(FILE* dat, FILE* tbl) {
    if (!dat || !tbl) return false;
    std::rewind(dat);
    if (std::fwrite(&header_.recordId, sizeof(header_.recordId), 1, dat) != 1) return false;
    if (std::fwrite(&header_.allocCount, sizeof(header_.allocCount), 1, dat) != 1) return false;
    if (std::fwrite(&header_.realCount, sizeof(header_.realCount), 1, dat) != 1) return false;

    std::rewind(tbl);
    if (std::fwrite(&header_.recordId, sizeof(header_.recordId), 1, tbl) != 1) return false;
    if (std::fwrite(&header_.allocCount, sizeof(header_.allocCount), 1, tbl) != 1) return false;
    if (std::fwrite(&header_.realCount, sizeof(header_.realCount), 1, tbl) != 1) return false;
    if (std::fwrite(&tableHead_, sizeof(tableHead_), 1, tbl) != 1) return false;
    if (std::fwrite(&tableTail_, sizeof(tableTail_), 1, tbl) != 1) return false;
    return true;
}

bool JKDataFile::ReadRefEntry(uint16_t index, JKDbRefEntry& out) const {
    if (!IsOpen() || index >= header_.allocCount) return false;
    FILE* tbl = std::fopen(tblPath_.c_str(), "rb");
    if (!tbl) return false;
    const long offset = static_cast<long>(sizeof(JKDbHeader) + sizeof(int16_t) * 2 +
                         index * sizeof(JKDbRefEntry));
    std::fseek(tbl, offset, SEEK_SET);
    bool ok = (std::fread(&out, sizeof(out), 1, tbl) == 1);
    std::fclose(tbl);
    return ok;
}

bool JKDataFile::WriteRefEntry(uint16_t index, const JKDbRefEntry& in) {
    if (!IsOpen() || index >= header_.allocCount) return false;
    FILE* tbl = std::fopen(tblPath_.c_str(), "rb+");
    if (!tbl) return false;
    const long offset = static_cast<long>(sizeof(JKDbHeader) + sizeof(int16_t) * 2 +
                         index * sizeof(JKDbRefEntry));
    std::fseek(tbl, offset, SEEK_SET);
    bool ok = (std::fwrite(&in, sizeof(in), 1, tbl) == 1);
    std::fclose(tbl);
    return ok;
}

bool JKDataFile::IsRecordActive(uint16_t index) const {
    JKDbRefEntry ref;
    if (!ReadRefEntry(index, ref)) return false;
    return ref.hasData != 0;
}

std::vector<uint8_t> JKDataFile::ReadRecord(uint16_t index) const {
    std::vector<uint8_t> result;
    if (!IsOpen() || index >= header_.allocCount) return result;
    FILE* dat = std::fopen(datPath_.c_str(), "rb");
    if (!dat) return result;
    const long offset = static_cast<long>(sizeof(JKDbHeader) + index * recordSize_);
    std::fseek(dat, offset, SEEK_SET);
    result.resize(recordSize_);
    size_t read = std::fread(result.data(), 1, recordSize_, dat);
    std::fclose(dat);
    if (read != recordSize_) result.clear();
    return result;
}

bool JKDataFile::WriteRecord(uint16_t index, const std::vector<uint8_t>& data) {
    if (!IsOpen() || index >= header_.allocCount || data.size() != recordSize_) {
        return false;
    }
    FILE* dat = std::fopen(datPath_.c_str(), "rb+");
    if (!dat) return false;
    const long offset = static_cast<long>(sizeof(JKDbHeader) + index * recordSize_);
    std::fseek(dat, offset, SEEK_SET);
    bool ok = (std::fwrite(data.data(), 1, recordSize_, dat) == recordSize_);
    std::fclose(dat);
    return ok;
}

int16_t JKDataFile::FindEmptySlot() const {
    JKDbRefEntry ref;
    for (uint16_t i = 0; i < header_.allocCount; ++i) {
        if (ReadRefEntry(i, ref) && ref.hasData == 0) {
            return static_cast<int16_t>(i);
        }
    }
    return -1;
}

bool JKDataFile::Expand(uint16_t count) {
    if (!IsOpen() || count == 0) return false;

    FILE* dat = std::fopen(datPath_.c_str(), "rb+");
    if (!dat) return false;
    std::fseek(dat, 0, SEEK_END);
    uint8_t blank = ' ';
    for (uint16_t i = 0; i < count; ++i) {
        for (size_t b = 0; b < recordSize_; ++b) {
            std::fwrite(&blank, 1, 1, dat);
        }
    }
    std::fclose(dat);

    FILE* tbl = std::fopen(tblPath_.c_str(), "rb+");
    if (!tbl) return false;
    std::fseek(tbl, 0, SEEK_END);
    JKDbRefEntry empty{ 0, -1, -1 };
    for (uint16_t i = 0; i < count; ++i) {
        std::fwrite(&empty, sizeof(empty), 1, tbl);
    }
    std::fclose(tbl);

    header_.allocCount += count;
    return true;
}

bool JKDataFile::SyncTableHeadTail() {
    FILE* dat = std::fopen(datPath_.c_str(), "rb+");
    FILE* tbl = std::fopen(tblPath_.c_str(), "rb+");
    if (!dat || !tbl) {
        if (dat) std::fclose(dat);
        if (tbl) std::fclose(tbl);
        return false;
    }
    bool ok = WriteHeader(dat, tbl);
    std::fclose(dat);
    std::fclose(tbl);
    return ok;
}

int16_t JKDataFile::AddRecord(const std::vector<uint8_t>& data) {
    if (!IsOpen() || data.size() != recordSize_) return -1;

    int16_t index = FindEmptySlot();
    if (index == -1) {
        if (!Expand(1)) return -1;
        index = static_cast<int16_t>(header_.allocCount - 1);
    }

    if (!WriteRecord(static_cast<uint16_t>(index), data)) return -1;

    JKDbRefEntry ref{ 1, -1, -1 };
    if (tableTail_ != -1) {
        JKDbRefEntry tail;
        if (ReadRefEntry(static_cast<uint16_t>(tableTail_), tail)) {
            tail.next = index;
            WriteRefEntry(static_cast<uint16_t>(tableTail_), tail);
            ref.prev = tableTail_;
            tableTail_ = index;
        }
    } else {
        tableHead_ = index;
        tableTail_ = index;
    }
    WriteRefEntry(static_cast<uint16_t>(index), ref);

    header_.realCount++;
    SyncTableHeadTail();
    return index;
}

bool JKDataFile::DeleteRecord(uint16_t index) {
    if (!IsOpen() || index >= header_.allocCount) return false;
    JKDbRefEntry ref;
    if (!ReadRefEntry(index, ref) || ref.hasData == 0) return false;

    if (ref.prev != -1) {
        JKDbRefEntry prev;
        if (ReadRefEntry(static_cast<uint16_t>(ref.prev), prev)) {
            prev.next = ref.next;
            WriteRefEntry(static_cast<uint16_t>(ref.prev), prev);
        }
    } else {
        tableHead_ = ref.next;
    }
    if (ref.next != -1) {
        JKDbRefEntry next;
        if (ReadRefEntry(static_cast<uint16_t>(ref.next), next)) {
            next.prev = ref.prev;
            WriteRefEntry(static_cast<uint16_t>(ref.next), next);
        }
    } else {
        tableTail_ = ref.prev;
    }

    ref.hasData = 0;
    ref.prev = -1;
    ref.next = -1;
    WriteRefEntry(index, ref);
    header_.realCount--;
    return SyncTableHeadTail();
}

std::vector<uint16_t> JKDataFile::GetActiveIndices() const {
    std::vector<uint16_t> result;
    if (!IsOpen()) return result;
    result.reserve(header_.realCount);
    int16_t idx = tableHead_;
    while (idx != -1) {
        result.push_back(static_cast<uint16_t>(idx));
        JKDbRefEntry ref;
        if (!ReadRefEntry(static_cast<uint16_t>(idx), ref)) break;
        idx = ref.next;
    }
    return result;
}

bool JKDataFile::Rearrange(const std::vector<uint16_t>& newOrder) {
    if (!IsOpen() || newOrder.size() != header_.realCount) return false;

    for (uint16_t idx : newOrder) {
        if (!IsRecordActive(idx)) return false;
    }

    tableHead_ = static_cast<int16_t>(newOrder.front());
    tableTail_ = static_cast<int16_t>(newOrder.back());

    for (size_t i = 0; i < newOrder.size(); ++i) {
        JKDbRefEntry ref;
        ref.hasData = 1;
        ref.prev = (i == 0) ? -1 : static_cast<int16_t>(newOrder[i - 1]);
        ref.next = (i + 1 == newOrder.size()) ? -1 : static_cast<int16_t>(newOrder[i + 1]);
        WriteRefEntry(newOrder[i], ref);
    }
    return SyncTableHeadTail();
}

} // namespace jk
