#ifndef MUSICDIRSTORE_H
#define MUSICDIRSTORE_H

// Music settings store (spec 2026-10-10-music-dirs-ui plan, T1): the file leg
// of the music "폴더 관리" line — settings.json "music.dirs"를 더하고 빼는
// 부품. MusicModel.h의 승계 쌍둥이: 읽기 쪽(DirsFromSettings·AudioDirFallback·
// NormalizeDirs)은 MusicModel이 소유하고 이 헤더는 **쓰기**를 소유한다.
//
// Header-inline on purpose — MusicModel.h 선례 자체: selftest 2o와 T2 모듈
// (ClientMusicApp)이 직소비하며 별도 TU 없이 링크된다. imgui/SDL/client 형을
// 먹지 않는다.
//
// 합성 파서 원문(브리프 재량 — 리포트 부기): 상위 객체의 키·원문 값을 뽑는
// **최소 스캐너**로 합성한다. AgentJson 재용은 두 가지로 기각 — ① read-only로
// 상위 키 열거가 없어(스캐너 없이 재합성 불가) ② quickjs ParseJSON은 문자열을
// UTF-8로 해석하므로 CP949 경로 바이트(A-API 규약 — jk::fs GetExecutablePath
// 원문)가 왕복에서 변질된다. 원문 슬라이스 재조립은 알 수 없는 상위 키
// (audio·retention·장래 키)와 비UTF-8 바이트를 **바이트 그대로** 남긴다 —
// "기존 키 무손상" 하드 계약의 충족 수형. 라이브러리 조달은 없다(순수 std).
//
// 원자적 쓰기 계약: 같은 폴더 settings.json.tmp에 완성본을 먼저 쓰고
// fs::rename — 부분 쓰기가 진짜 이름으로 도달하지 않는다(JKWorkshopStore
// WriteFileAll 원문 계약 승계). 첫 수는 **교체 rename**(posix는 원자적 교체로
// 통과)이고 실패만 2세대 사다리로 걷는다: Windows UCRT rename은 존재 대상
// 교체에 실패한다(2026-09-26 워크숍 라이브 게이트 실측 원문) — 이때 원본을
// .bak로 대피해 rename 실패 시 복원(브리프 "실패 시 원본 유지" 계약 —
// 워크숍 remove+rename 사다리의 복원 보강).
//
// T1 fix r1 (리뷰 I-1 필수 + M-1/M-2/M-3 동봉): quickjs 원독(DirsFromSettings)
// 은 CP949 바이트 혼입 문서를 **전체 거부**해서 쓰기 경로가 기존 항목을
// 무음 소각했다("기존 CP949 항목이 있는 settings에서 AddDir" — 빈 목록에서
// 재합성+ok=true) — dirs 한정 자기 스캐너 수취(ReadDirs: ScanObjectFields+
// ScanStringArray, 원문 슬라이스 규약)로 CP949 바이트 그대로 보존. 스캐너
// 문자열은 고바이트 뒤 0x5C를 리터럴로 두는 규칙(CP949/UTF-8 투명성 — 확장
// 한글 후행 0x5C는 파일명 문자; 이스케이프 복호는 ASCII 문맥 한정). 빈
// 리터럴(`{"a": ,}`)은 공백만 몸통으로 셈 없이 부적합 출하(M-1 파서 단정).
// Slashize는 MusicModel.h 본체로 승격(M-3 — 리졸버 MusicDirList 같은 규약
// 승계, 전 축 통일).

#include <apps/MusicModel.h>
#include <port/JKCrtShim.h>  // FopenS — Win/posix 공용 파일 오픈

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace jk {
namespace music {
namespace store {

// 쓰기 결과 — ok=false면 err에 1행 사유(다른 표기 없음 — 브리프 계약).
struct DirWriteResult {
    bool ok = false;
    std::string err;
};

// settings.json 절대 경로 — exeDir 끝 구분자 유무 무관(AudioDirFallback 같은
// fs::path 합성 — MusicModel.h 원문 수형). 빈 exeDir은 상대경로 state/
// settings.json — 여전히 1건(fail-safe, 원문 규약 승계).
inline std::string SettingsPath(const std::string& exeDir) {
    return (std::filesystem::path(exeDir) / "state" / "settings.json")
        .generic_string();
}

// ---- 최소 상위 스캔 (원문 슬라이스 재합성용) ----

inline void SkipWs(const std::string& t, size_t& i) {
    while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\n' ||
                            t[i] == '\r'))
        ++i;
}

// 문자열 원문 통과 — i는 여는 따옴표 자리, 닫는 따옴표 직후로 밀어준다.
// 이스케이프 감식이 전부(\" 뒤의 따옴표는 끝이 아니다 — 중괄호·대괄호 깊이
// 계산도 같은 사유로 이 통과를 먼저 돈다).
//
// fix r1 — 고바이트 뒤 0x5C는 리터럴: 인코딩 무관 원문 스캐너의 규약. 0x5C는
// UTF-8 다중바이트 성분에 나타나지 않아(연속 바이트 전원 0x80 이상) ASCII
// 뒤의 0x5C만 이스케이프로 읽으면 UTF-8 문서는 무접촉이고, CP949 확장 영역
// (선단 0x81-A0)의 후행 0x5C 파일명 문자(뷁류)를 이스케이프로 오인해
// 따옴표·깊이가 어긋나는 것을 봉한다. 한계(정직 표기): 고바이트 바로 뒤에
// 의도된 `\\` 이스케이프가 오면(작성자가 역슬래시를 이중화한 원문) 복호
// 어긋남 — 저장소 쓰기는 Slashize로 역슬래시를 먼저 접어 이 원문을 만들지
// 않는다.
inline bool ScanJsonString(const std::string& t, size_t& i) {
    if (i >= t.size() || t[i] != '"') return false;
    ++i;
    bool prevHigh = false;
    while (i < t.size()) {
        const unsigned char c = static_cast<unsigned char>(t[i]);
        if (c == '"') {
            ++i;
            return true;
        }
        if (c == '\\' && !prevHigh) {
            ++i;
            if (i >= t.size()) return false;
            prevHigh = false;  // 이스케이프 대상은 ASCII 1바이트
            ++i;
            continue;
        }
        prevHigh = (c >= 0x80);
        ++i;
    }
    return false;  // 닫는 따옴표 없음 — 부적합
}

// \uXXXX 4자리 16진수 해독 — 성공하면 코드포인트, 실패하면 false. 키 복호화
// 전용("music" 같은 철자 키) — 키는 실사용 ASCII이므로 이 경로는 휴면.
// 서로게이트 쌍 조립은 미지원(한 쌍 전부 다른 키로 접힌다 — 휴면
// 경로 한계, 실사용 무접촉).
inline bool DecodeHex4(const std::string& t, size_t& i, unsigned& cp) {
    if (i + 4 > t.size()) return false;
    cp = 0;
    for (int k = 0; k < 4; ++k) {
        const char h = t[i++];
        cp <<= 4;
        if (h >= '0' && h <= '9')
            cp |= static_cast<unsigned>(h - '0');
        else if (h >= 'a' && h <= 'f')
            cp |= static_cast<unsigned>(h - 'a' + 10);
        else if (h >= 'A' && h <= 'F')
            cp |= static_cast<unsigned>(h - 'A' + 10);
        else
            return false;
    }
    return true;
}

// 코드포인트 → UTF-8 인코드(0x80-0x10FFFF — 서로게이트 미조립은 위 참조).
inline void AppendUtf8(std::string& out, unsigned cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// 키·문자열 원소 복호화 — i는 여는 따옴표(키, ScanStringArray 원소 공용).
// fix r1: 고바이트 뒤 0x5C는 리터럴(ScanJsonString 원문 규약 — CP949 확장
// 한글 후행이 복호 어긋남을 만드는 것 봉합). 미지원 이스케이프 = 부적합
// (보수 파 — 원본 보존이 우선이므로 애매한 원문은 손대지 않는다).
inline bool DecodeJsonKey(const std::string& t, size_t& i, std::string& key) {
    if (i >= t.size() || t[i] != '"') return false;
    ++i;
    key.clear();
    bool prevHigh = false;
    while (i < t.size()) {
        const unsigned char c = static_cast<unsigned char>(t[i]);
        if (c == '"') {
            ++i;
            return true;
        }
        if (c != '\\' || prevHigh) {
            key += t[i];
            prevHigh = (c >= 0x80);
            ++i;
            continue;
        }
        ++i;  // ASCII 문맥의 이스케이프 — '\\' 소비
        if (i >= t.size()) return false;
        const char e = t[i++];
        switch (e) {
        case '"': key += '"'; break;
        case '\\': key += '\\'; break;
        case '/': key += '/'; break;
        case 'b': key += '\b'; break;
        case 'f': key += '\f'; break;
        case 'n': key += '\n'; break;
        case 'r': key += '\r'; break;
        case 't': key += '\t'; break;
        case 'u': {
            unsigned cp = 0;
            if (!DecodeHex4(t, i, cp)) return false;
            AppendUtf8(key, cp);
            break;
        }
        default: return false;  // 미지원 이스케이프 = 부적합
        }
        prevHigh = false;  // 이스케이프 출력은 ASCII 몫(\u 출력 다중바이트는
                           //   휴면 경로 — CP949 원문과 무접촉)
    }
    return false;
}

// 값 원문 통과 — start는 값 앞(내부에서 공백 건너뜀), end만 갱신(값 직후).
// 문자열 내부의 괄호는 깊이에서 무사히 무시(ScanJsonString으로 먼저 통과).
// 그 밖 리터럴(수·true/false/null)은 JSON 구두자와 공백 직전까지 원문 수용.
inline bool ScanJsonValue(const std::string& t, size_t start, size_t& end) {
    size_t i = start;
    SkipWs(t, i);
    if (i >= t.size()) return false;
    const size_t body = i;  // 값 몸통 첫 바이트(선행 공백은 몸통이 아니다 — M-1)
    const char c = t[i];
    if (c == '"') return ScanJsonString(t, i) ? (end = i, true) : false;
    if (c == '{' || c == '[') {
        const char open = c;
        const char close = (c == '{') ? '}' : ']';
        int depth = 0;
        while (i < t.size()) {
            const char d = t[i];
            if (d == '"') {  // string 내부 괄호 — 깊이 무시 구간
                if (!ScanJsonString(t, i)) return false;
                continue;
            }
            if (d == open) {
                ++depth;
            } else if (d == close) {
                if (--depth == 0) {
                    ++i;
                    end = i;
                    return true;
                }
            }
            ++i;
        }
        return false;  // 짝 없음 — 부적합
    }
    while (i < t.size() && t[i] != ',' && t[i] != '}' && t[i] != ']' &&
           t[i] != ' ' && t[i] != '\t' && t[i] != '\n' && t[i] != '\r')
        ++i;
    end = i;
    return end > body;  // 몸통 없음(공백만 — `{"a": ,}`) = 부적합(M-1 출하)
}

// 상위 객체 스캔 — 키·원문 값 슬라이스(내부 공백 포함 그대로) 목록. 루트가
// 객체가 아니거나 문법 파손이면 false(호출자는 원본을 그대로 남긴다 —
// 부적합 JSON 오염 금지 계약).
struct JsonField {
    std::string key;
    std::string raw;
};

// 객체 필드 스캔 — start는 여는 '{' 자리(또는 그 앞 공백), 닫는 '}' 직후를
// end로 돌려준다. 상위 문서와 music 내부 객체(fix r1 — I-1 dirs 한정 재합성)
// 가 같은 이 스캔을 먹는다.
inline bool ScanObjectFields(const std::string& t, size_t start, size_t& end,
                             std::vector<JsonField>& out) {
    out.clear();
    size_t i = start;
    SkipWs(t, i);
    if (i >= t.size() || t[i] != '{') return false;
    ++i;
    SkipWs(t, i);
    if (i < t.size() && t[i] == '}') {  // 빈 객체 — 정합
        ++i;
        end = i;
        return true;
    }
    while (true) {
        SkipWs(t, i);
        std::string key;
        if (!DecodeJsonKey(t, i, key)) return false;
        SkipWs(t, i);
        if (i >= t.size() || t[i] != ':') return false;
        ++i;
        const size_t vstart = i;  // 값 원문 슬라이스 시작(선행 공백 포함 보존)
        size_t vend = 0;
        if (!ScanJsonValue(t, i, vend)) return false;
        out.push_back(JsonField{std::move(key),
                                t.substr(vstart, vend - vstart)});
        i = vend;
        SkipWs(t, i);
        if (i >= t.size()) return false;  // 닫는 괄호 없음 — 부적합
        if (t[i] == '}') {
            ++i;
            end = i;
            return true;
        }
        if (t[i] != ',') return false;
        ++i;  // 다음 키로 — 트레일링 콤마는 DecodeJsonKey가 거부
    }
}

inline bool ScanTopLevelFields(const std::string& text,
                               std::vector<JsonField>& out) {
    size_t end = 0;
    return ScanObjectFields(text, 0, end, out);
}

// JSON 문자열 이스케이프(EscapeJson 쌍둥이 원문 수형 — ClientMusicApp.cpp:
// 따옴표·역슬래시·제어 바이트; 경로는 정규화가 역슬래시를 먼저 접지만 이
// 이스케이프는 공용 규약 유지). 공백·멀티바이트는 원문 수용 — JSON 문자열
// 꾸러미가 감싸므로 이스케이프 불요(2m-g 원문 계약).
inline std::string EscapeJsonStr(const std::string& in) {
    std::string out;
    bool prevHigh = false;  // fix r1 — 고바이트 뒤 0x5C는 리터럴(CP949 확장
                            //   한글 후행을 이중화해 원문을 갉아먹지 않는다 —
                            //   ScanJsonString 복호 규약과 같은 짝)
    for (char ch : in) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if ((c == '"' || c == '\\') && !prevHigh) {
            out += '\\';
            out += ch;
            prevHigh = false;
        } else if (c < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<int>(c));
            out += buf;
            prevHigh = false;
        } else {
            out += ch;  // 공백·멀티바이트·CP949 후행 0x5C = 원문 수용
            prevHigh = (c >= 0x80);
        }
    }
    return out;
}

// 합성 — 기존 상위 키 원문을 슬라이스 그대로(바이트 보존) 두고, music는 같은
// 자리에 재기입(부재 말미 신설). 읽던 자리 유지 = 재쓰기 diff 최소화(사람이
// settings.json을 열어 보는 규약 존중). dirs는 슬래시 정규형 원문 수형.
inline std::string ComposeMusicDirs(const std::vector<JsonField>& fields,
                                    const std::vector<std::string>& dirs) {
    std::string music = "\"music\":{\"dirs\":[";
    for (size_t k = 0; k < dirs.size(); ++k) {
        if (k) music += ',';
        music += '"';
        music += EscapeJsonStr(dirs[k]);
        music += '"';
    }
    music += "]}";
    std::string out = "{";
    bool first = true;
    bool musicEmitted = false;
    for (const JsonField& f : fields) {
        if (f.key == "music") musicEmitted = true;  // 재기입 자리 예약
        if (!first) out += ',';
        first = false;
        if (f.key == "music") {
            out += music;
        } else {
            out += '"';
            out += EscapeJsonStr(f.key);
            out += "\":";
            out += f.raw;  // 알 수 없는 상위 키 = 원문 그대로(무손상 계약)
        }
    }
    if (!musicEmitted) {
        if (!first) out += ',';
        out += music;
    }
    out += '}';
    return out;
}

// ---- 원자적 쓰기 ----

inline bool ReadAll(const std::string& path, std::string& out) {
    std::FILE* f = nullptr;
    if (jk::crt::FopenS(&f, path.c_str(), "rb") != 0 || !f) return false;
    char buf[4096];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    const bool ok = std::ferror(f) == 0;
    std::fclose(f);
    return ok;
}

// tmp 완성 → 교체 rename 시도 → 실패만 .bak 사다리. 사다리의 원본 대피 실패/
// 복원 실패 전부 err(정직 — 조용한 눌먹음 금지, 워크숍 실측 원문 계약).
inline bool WriteSettingsAtomic(const std::string& path,
                                const std::string& data, std::string& err) {
    const std::string tmp = path + ".tmp";  // 같은 폴더 — rename 원자성 계약
    const std::string bak = path + ".bak";
    std::FILE* f = nullptr;
    if (jk::crt::FopenS(&f, tmp.c_str(), "wb") != 0 || !f) {
        err = "tmp open failed";
        return false;
    }
    const size_t w = std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    if (w != data.size()) {
        std::remove(tmp.c_str());
        err = "tmp write failed";
        return false;
    }
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::rename(tmp, path, ec);  // 1세대 — 교체 가능 플랫폼(posix)은 원자적
    if (!ec) return true;
    // 2세대 사다리(UCRT 존재 대상 교체 실패 — 2026-09-26 워크숍 실측 원문):
    fs::remove(bak, ec);
    if (ec) {
        std::remove(tmp.c_str());
        err = "bak remove failed";
        return false;
    }
    ec.clear();
    fs::rename(path, bak, ec);
    if (ec) {
        std::remove(tmp.c_str());
        err = "original backup failed";
        return false;
    }
    ec.clear();
    fs::rename(tmp, path, ec);
    if (ec) {
        std::error_code rec;
        fs::rename(bak, path, rec);  // 원본 복원 시도 — 실패도 err 표기
        std::remove(tmp.c_str());
        err = "rename failed";
        return false;
    }
    std::error_code bc;
    fs::remove(bak, bc);  // 성공 세대의 이주 잔산 소각 — 실패는 무해 잔산
    return true;
}

// 배열 원소 추출 — dirs 배열 한정(fix r1 I-1): 각 원소가 문자열이면
// DecodeJsonKey 복호 수취(CP949 후행 0x5C 리터럴 규약 포함), 아니면 원문
// 스킵(DirsFromSettings 원문 계약 — 비문자열 성분 스킵). 배열 원문 부적합 =
// false — 쓰기 경로는 err로 거부(원본 보존), 뷰는 빈 목록.
inline bool ScanStringArray(const std::string& t, size_t start, size_t& end,
                            std::vector<std::string>& out) {
    out.clear();
    size_t i = start;
    SkipWs(t, i);
    if (i >= t.size() || t[i] != '[') return false;
    ++i;
    SkipWs(t, i);
    if (i < t.size() && t[i] == ']') {  // 빈 배열 — 0건(정합)
        ++i;
        end = i;
        return true;
    }
    while (true) {
        SkipWs(t, i);
        if (i < t.size() && t[i] == '"') {
            std::string v;
            if (!DecodeJsonKey(t, i, v)) return false;
            out.push_back(std::move(v));
        } else {  // 비문자열 성분 — 원문 스킵
            size_t vend = 0;
            if (!ScanJsonValue(t, i, vend)) return false;
            i = vend;
        }
        SkipWs(t, i);
        if (i >= t.size()) return false;  // 닫는 괄호 없음 — 부적합
        if (t[i] == ']') {
            ++i;
            end = i;
            return true;
        }
        if (t[i] != ',') return false;
        ++i;
    }
}

// 쓰기 경로의 dirs 수취 (fix r1 I-1 — ReadDirs): 상위 필드에서 music 객체를
// 찾고(첫 등장 — NormalizeDirs의 첫-승규약과 같은 규약), 내부 "dirs"의 값을
// 원문으로 판정한다. quickjs 원독 기각 — CP949 혼입 문서를 전체 거부해 쓰기
// 경로가 기존 항목을 무음 소각했다. 읽기 원문: dirs 부재·비배열("P:/sounds"
// 같은 문자열) = ok+빈 목록(2m-a 원문 계약 — 무시), dirs 배열 원문 파손만
// ok=false(쓰기 거부 — 원본 보존; 조용한 절단 없음).
struct DirsRead {
    bool ok = false;                // false = 부적합 — 쓰기 거부(원본 보존)
    std::vector<std::string> dirs;  // 정규화 전 원문 순서(호출자가 접는다)
};

inline DirsRead ReadDirs(const std::vector<JsonField>& fields) {
    DirsRead rd;
    rd.ok = true;
    for (const JsonField& f : fields) {
        if (f.key != "music") continue;
        std::vector<JsonField> inner;
        size_t iend = 0;
        if (!ScanObjectFields(f.raw, 0, iend, inner)) {
            return rd;  // music 비객체 = 무시 — ok(빈 목록, 2m-a fail-safe 계약)
        }
        for (const JsonField& g : inner) {
            if (g.key != "dirs") continue;
            size_t vi = 0;
            SkipWs(g.raw, vi);
            if (vi < g.raw.size() && g.raw[vi] == '[') {
                size_t aend = 0;
                if (!ScanStringArray(g.raw, 0, aend, rd.dirs)) {
                    rd.dirs.clear();
                    rd.ok = false;  // 배열 원문 파손 — 쓰기 거부
                    return rd;
                }
            }
            return rd;  // 첫 dirs 키만 — 부재·비배열 = ok(빈 목록)
        }
        return rd;  // dirs 키 부재 = ok(빈 목록)
    }
    return rd;  // music 필드 부재 = ok(빈 목록)
}

// 순수 정규화 — 2m-b NormalizeDirs의 바이트 투명 쌍둥이(fix r1): fs::path
// 원문(MusicModel)은 Win 축(libstdc++)의 narrow 변환에 CP949 바이트를 못
// 먹는다 — filesystem_error "Illegal byte sequence" 출하(2o-f 실측 원문).
// dirs 왕복은 문자 조작만으로 같은 규약(뒤 구분자 접기+빈 성분 제거+중복
// 철자 첫-승)을 흉내 낸다 — Slashize가 '\' 접기를 먼저 하므로 fs::path의
// 스페이퍼 역할은 이행되고, Slashize된 입력에서 두 정규화는 동형이다.
inline std::vector<std::string> NormalizeDirsPure(
    const std::vector<std::string>& dirs) {
    std::vector<std::string> out;
    out.reserve(dirs.size());
    for (const std::string& in : dirs) {
        std::string p = in;  // 뒤 구분자 중복 — 몇 겹이어도 한 번에 벗긴다
        while (!p.empty() && p.back() == '/') p.pop_back();
        if (p.empty()) continue;  // 빈 성분 제거
        if (std::find(out.begin(), out.end(), p) == out.end())
            out.push_back(std::move(p));
    }
    return out;
}

// settings.json "music"."dirs"의 기록 뷰 — fix r1: dirs 한정 스캐너 수취로
// 승격(quickjs 원독의 CP949 무음 소각 봉합). 뷰가 리졸버 MusicDirList의
// 유저 뒷성분과 같은 표기 — Slashize+NormalizeDirsPure(2m-b 규약의 바이트
// 투명 수형). 파싱 실패 = 빈 목록(뷰 전용 — 오류 비표기 브리프 계약).
inline std::vector<std::string> UserDirs(const std::string& settingsText) {
    std::vector<JsonField> fields;
    if (!ScanTopLevelFields(settingsText, fields)) return {};
    const DirsRead rd = ReadDirs(fields);
    if (!rd.ok) return {};
    std::vector<std::string> raw = rd.dirs;
    for (std::string& d : raw) d = Slashize(d);
    return NormalizeDirsPure(raw);
}

// ---- AddDir / RemoveDir ----

inline DirWriteResult AddDir(const std::string& exeDir,
                             const std::string& path) {
    DirWriteResult r;
    namespace fs = std::filesystem;
    // 역슬래시 → 슬래시 정규화(2m-g 원문 수형 — 기본 폴더와 동일 규약).
    const std::vector<std::string> one = NormalizeDirs({Slashize(path)});
    if (one.empty()) {
        r.err = "empty dir";
        return r;
    }
    const std::string target = one[0];
    // 기본 폴더는 대상 아님(fix r1 M-2 — RemoveDir 방어선과 대칭: 기본 폴더를
    // dirs에 기록하면 리졸버의 중복 성분만 무의미하게 늘어난다).
    if (target == AudioDirFallback::Path(exeDir)) {
        r.err = "default dir not appendable";
        return r;
    }
    const std::string sp = SettingsPath(exeDir);
    std::error_code eec;
    const bool exists = fs::exists(sp, eec) && !eec;
    std::string text;
    std::vector<JsonField> fields;
    if (exists) {
        if (!ReadAll(sp, text)) {
            r.err = "settings read failed";
            return r;  // 존재를 못 읽으면 쓰지도 않는다 — 보수 파(오염 금지)
        }
        if (!text.empty() && !ScanTopLevelFields(text, fields)) {
            r.err = "settings json unparsable";
            return r;  // 원본 보존 — 부적합 원문은 손대지 않는다
        }
        // 빈 파일 = 신설 취급(데이터 0바이트 — 손실 없음)
    }
    // fix r1: dirs 수취는 자기 스캐너(ReadDirs) — quickjs 원독의 CP949 무음
    // 소각 봉합. 배열 원문 파손만 거부(원본 보존).
    const DirsRead rd = ReadDirs(fields);
    if (!rd.ok) {
        r.err = "music dirs unparsable";
        return r;
    }
    std::vector<std::string> dirs = rd.dirs;
    for (std::string& d : dirs) d = Slashize(d);
    dirs = NormalizeDirsPure(dirs);
    if (std::find(dirs.begin(), dirs.end(), target) != dirs.end()) {
        r.ok = true;  // 중복(정규화 후) — no-op ok
        return r;
    }
    if (static_cast<int>(dirs.size()) >= kMaxUserDirs) {
        r.err = "user dirs capacity";  // 상한 도달 — 쓰기 거부(폭주 절단)
        return r;
    }
    dirs.push_back(target);
    std::error_code mkec;
    fs::create_directories(fs::path(sp).parent_path(), mkec);
    if (mkec) {
        r.err = "state dir create failed";
        return r;
    }
    if (!WriteSettingsAtomic(sp, ComposeMusicDirs(fields, dirs), r.err))
        return r;
    r.ok = true;
    r.err.clear();
    return r;
}

inline DirWriteResult RemoveDir(const std::string& exeDir,
                                const std::string& path) {
    DirWriteResult r;
    namespace fs = std::filesystem;
    const std::vector<std::string> one = NormalizeDirs({Slashize(path)});
    const std::string sp = SettingsPath(exeDir);
    // 기본 폴더는 대상 아님(removable=false 규약 — 저장소 방어선: UI가 버튼을
    // 막아도 저장소 스스로도 거부한다). ok 아닌 err로 정직 표기.
    if (!one.empty() && one[0] == AudioDirFallback::Path(exeDir)) {
        r.err = "default dir not removable";
        return r;
    }
    if (one.empty()) {
        r.err = "empty dir";
        return r;
    }
    const std::string target = one[0];
    std::error_code eec;
    const bool exists = fs::exists(sp, eec) && !eec;
    if (!exists) {
        r.ok = true;  // 기록 자체가 없다 — 미존재 no-op ok(정직)
        return r;
    }
    std::string text;
    if (!ReadAll(sp, text)) {
        r.err = "settings read failed";
        return r;
    }
    if (text.empty()) {
        r.ok = true;  // 빈 파일 — 기록 없음(뷰도 빈 목록 — 0바이트를 쓰지 않음)
        return r;
    }
    std::vector<JsonField> fields;
    if (!ScanTopLevelFields(text, fields)) {
        r.err = "settings json unparsable";
        return r;  // 원본 보존 — 부적합 원문은 손대지 않는다
    }
    const DirsRead rd = ReadDirs(fields);  // fix r1 — 스캐너 수취(I-1 승계)
    if (!rd.ok) {
        r.err = "music dirs unparsable";
        return r;
    }
    std::vector<std::string> dirs = rd.dirs;
    for (std::string& d : dirs) d = Slashize(d);
    dirs = NormalizeDirsPure(dirs);
    const auto it = std::find(dirs.begin(), dirs.end(), target);
    if (it == dirs.end()) {
        r.ok = true;  // 미존재 — no-op ok(정직)
        return r;
    }
    dirs.erase(it);
    if (!WriteSettingsAtomic(sp, ComposeMusicDirs(fields, dirs), r.err))
        return r;
    r.ok = true;
    r.err.clear();
    return r;
}

} // namespace store

using store::DirWriteResult;  // T2 UI 소비 편의 — jk::music::DirWriteResult
} // namespace music
} // namespace jk

#endif // MUSICDIRSTORE_H
