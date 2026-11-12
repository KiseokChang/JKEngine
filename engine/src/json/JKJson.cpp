// src/json/JKJson.cpp — 이스케이프 정본(docs/106 §2-5 통합 1호).
// 네 변형은 한 구현(플래그)에서 파생한다 — 복사본 증식 금지, 계약은 이름으로.
#include <json/JKJson.h>

#include <cstdio>

namespace jk {
namespace json {
namespace {

enum Flags : unsigned {
    kShortBackspace = 1u << 0,   // \b \f 단축
    kShortNrt = 1u << 1,        // \n \r \t 단축
    kEscapeBraces = 1u << 2,    // { } 이스케이프(행 수술 스캔)
};

std::string EscapeImpl(const std::string& in, unsigned flags) {
    std::string out;
    out.reserve(in.size() + 8);
    auto ucode = [&out](unsigned char c) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
        out += buf;
    };
    for (unsigned char c : in) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '{':
            case '}':
                if (flags & kEscapeBraces) {
                    out += (c == '{') ? "\\u007b" : "\\u007d";
                } else {
                    out += static_cast<char>(c);
                }
                break;
            case '\b':
            case '\f':
                if (flags & kShortBackspace) out += (c == '\b') ? "\\b" : "\\f";
                else ucode(c);
                break;
            case '\n':
            case '\r':
            case '\t':
                if (flags & kShortNrt) {
                    out += (c == '\n') ? "\\n" : (c == '\r') ? "\\r" : "\\t";
                } else {
                    ucode(c);
                }
                break;
            default:
                if (c < 0x20) ucode(c);
                else out += static_cast<char>(c);   // UTF-8 원문 통과
        }
    }
    return out;
}

}  // namespace

std::string Escape(const std::string& in) {
    return EscapeImpl(in, kShortBackspace | kShortNrt);
}

std::string EscapeLegacyLog(const std::string& in) {
    return EscapeImpl(in, kShortNrt);
}

std::string EscapeStrict(const std::string& in) {
    return EscapeImpl(in, 0u);
}

std::string EscapeRowSafe(const std::string& in) {
    return EscapeImpl(in, kEscapeBraces);
}

std::string Quote(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    out += '"';
    out += Escape(in);
    out += '"';
    return out;
}

}  // namespace json
}  // namespace jk
