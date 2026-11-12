// include/fs/JKFileRead.h — JSON/매니페스트 파일 읽기 정본 (docs/106 §2-5 잔여)
//
// 왜: "파일 전체를 읽고 NUL을 붙인다"는 같은 루틴이 TU마다 복사돼 있었다
// (JKTerminalConfig·JKDesktopShell …). 상한 상수 이름만 다르고 동작은 같은
// 복사본이었고, 한쪽만 NUL/부분읽기 처리가 빠지면 QuickJS 파서가 조용히
// 오작동한다. 여기서 계약을 한 번만 적는다.
//
// 계약:
//  - maxBytes 초과/0바이트/부분 읽기 = false(정직 실패 — 절단 파싱 금지).
//  - 성공 시 out.back() == '\0' (JS_ParseJSON 계약: len은 size()-1).
#ifndef JK_FILE_READ_H
#define JK_FILE_READ_H

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace jk {
namespace fs {

inline bool ReadFileNulBytes(const std::string& path, std::vector<uint8_t>& out,
                             size_t maxBytes = 1u << 20) {
#ifdef _WIN32
    std::FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0) return false;
#else
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
#endif
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0 || static_cast<size_t>(size) > maxBytes) {
        std::fclose(f);
        return false;
    }
    out.resize(static_cast<size_t>(size) + 1);
    const size_t read =
        std::fread(out.data(), 1, static_cast<size_t>(size), f);
    std::fclose(f);
    if (read != static_cast<size_t>(size)) return false;   // 부분 읽기 = 실패
    out[static_cast<size_t>(size)] = '\0';
    return true;
}

}  // namespace fs
}  // namespace jk

#endif  // JK_FILE_READ_H
