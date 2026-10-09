#ifndef MUSICMODEL_H
#define MUSICMODEL_H

// Music app model (spec 2026-10-09-music-library-design, T1): the pure parts
// of the music hub — source-dir resolver over settings.json "music.dirs",
// recursive audio-file enumeration (D3: 전체 트리 재귀 — 서브디렉터리 포함),
// filename scan metadata (D2: 파일명·크기·mtime만 — ID3/태그는 백로그), and
// the name filter. Playback is delegated to vplayer (D1) — this header owns
// no decode/pipeline part (갤러리의 Fnv1a·FitThumb는 오디오에 불요 — YAGNI).
//
// Header-inline on purpose — GalleryModel.h 선례 승계: the selftest twins
// (engine/src/main.cpp selftest 2m) assert these parts by direct link with no
// extra TU, and the T2 module (ClientMusicApp) consumes the same functions.
// This header must stay free of imgui/SDL/client types.
//
// T1 fix r1: the D3 recursion got a cycle guard — Windows skips reparse-point
// (symlink/junction) directories via GetFileAttributesW, posix keeps a
// (st_dev, st_ino) visited set (seeds the root). 2m-e asserts a looping tree
// ends in finite time and is counted exactly once.

#include <agent/JKAgentJson.h>
#include <port/JKCrtShim.h>  // Stricmp — Win/posix 공용 ASCII 대소문자 무시 비교

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#if !defined(_WIN32)
#include <sys/stat.h>  // ::stat — (st_dev, st_ino) 방문 정체성 원문(fix r1)
#endif

namespace jk {
namespace music {

// user dirs 상한 — settings 폭주 절단(갤러리 원문 승계; 실사용은 1-3건이
// 현실).
inline constexpr int kMaxUserDirs = 64;

// 오디오 확장자 (스펙 결정: mp3/flac/wav/ogg/m4a/aac/wma). 확장자 문자열은
// ".mp3" 도트 포함 형태(fs::path::extension() 원문 형식)로 비교한다.
inline bool IsAudioExtension(const std::string& ext) {
    return jk::crt::Stricmp(ext.c_str(), ".mp3") == 0 ||
           jk::crt::Stricmp(ext.c_str(), ".flac") == 0 ||
           jk::crt::Stricmp(ext.c_str(), ".wav") == 0 ||
           jk::crt::Stricmp(ext.c_str(), ".ogg") == 0 ||
           jk::crt::Stricmp(ext.c_str(), ".m4a") == 0 ||
           jk::crt::Stricmp(ext.c_str(), ".aac") == 0 ||
           jk::crt::Stricmp(ext.c_str(), ".wma") == 0;
}

// Default scan dir (brief 계약명 AudioDirFallback — 갤러리 디폴트 폴더 합성
// 쌍둥이): <exeDir>/state/music. fs::path 합성+generic_string — exeDir 끝
// 구분자 유무 무관, 폰/WSL의 '/' 표기와 같은 형태. exeDir 빈값(jk::fs 계약
// 실패)은 상대경로 "state/music"을 남긴다 — 여전히 1건 기본(fail-safe).
struct AudioDirFallback {
    static std::string Path(const std::string& exeDir) {
        return (std::filesystem::path(exeDir) / "state" / "music")
            .generic_string();
    }
};

// Path normalization (2m-b — gallery::NormalizeDirs 쌍둥이, brief 계약형
// const 참조): generic separator form ('\\'→'/'), duplicate trailing
// separators stripped, and empty components ("", "/", "//") dropped. Dedupe
// is byte-exact keeping the first occurrence — 갤러리 원문 계약 승계
// (대소문자 무시 dedupe는 폰/WSL 대소문자 구분 파일시스템에서 서로 다른
// 폴더를 잘못 접는다 — v1은 기록 철자 존중).
inline std::vector<std::string> NormalizeDirs(
    const std::vector<std::string>& dirs) {
    std::vector<std::string> out;
    out.reserve(dirs.size());
    for (const std::string& raw : dirs) {
        // fs::path 정규형(generic_string) — '\'→'/' (Win 원문과 폰/WSL의 '/'
        // 쓰기를 한 표기로 묶는다). 부재 경로도 path 구성은 실패하지 않는다.
        std::string p = std::filesystem::path(raw).generic_string();
        // 뒤 구분자 중복 — 몇 겹이어도 한 번에 벗긴다.
        while (!p.empty() && p.back() == '/') p.pop_back();
        if (p.empty()) continue;  // 빈 성분 제거(원래 빈값·"/"·"//" 전부)
        if (std::find(out.begin(), out.end(), p) == out.end())
            out.push_back(std::move(p));
    }
    return out;
}

// settings.json "music"."dirs" — a JSON array of path strings (gallery.dirs와
// 같은 구조·같은 정규화; 폰에서는 /sdcard/Music 등을 폰 settings에 직접 기록해
// 켠다). 비문자열 성분은 스킵, 부재/파손/빈 배열 = 빈 목록(리졸버가 기본 폴더만
// 남긴다 — fail-safe 계약). GetObjRaw 원문 보존 소비 + {"dirs":raw} 래퍼
// 재파싱은 갤러리 DirsFromSettings 원문 승계 — AgentJson의 배열 리더는 root가
// **객체**일 때만 열린다(GetArraySize/GetArrValStr 계약).
inline std::vector<std::string> DirsFromSettings(
    const std::string& settingsText) {
    std::vector<std::string> out;
    if (settingsText.empty()) return out;
    const agent::AgentJson root(settingsText);
    if (!root.ok()) return out;
    std::string raw;
    if (!root.GetObjRaw("music", "dirs", raw)) return out;
    const agent::AgentJson arr("{\"dirs\":" + raw + "}");
    if (!arr.ok()) return out;
    int n = 0;
    if (!arr.GetArraySize("dirs", n) || n <= 0) return out;
    if (n > kMaxUserDirs) n = kMaxUserDirs;  // 폭주 절단
    for (int i = 0; i < n; ++i) {
        std::string v;
        if (arr.GetArrValStr("dirs", i, v) && !v.empty())
            out.push_back(std::move(v));
    }
    return out;
}

// Source-dir resolver (selftest 2m-a) — the app's single truth for which
// folders the music hub scans. Pure: no file I/O here; the caller hands in
// the raw settings.json text it read from disk. Order contract: the default
// dir <exeDir>/state/music (AudioDirFallback — 기본 폴더 규약) first, then
// user dirs in settings order, duplicates and empty components removed
// (NormalizeDirs). settings 부재/파손 = 기본 1건(fail-safe).
inline std::vector<std::string> MusicDirList(const std::string& exeDir,
                                             const std::string& settingsText) {
    std::vector<std::string> dirs;
    dirs.push_back(AudioDirFallback::Path(exeDir));
    for (std::string& user : DirsFromSettings(settingsText))
        dirs.push_back(std::move(user));
    return NormalizeDirs(std::move(dirs));
}

// 스캔 행 (D2 파일명 스캔 — v1 메타데이터의 전부): full=절대(전체) 경로,
// rel=루트 기준 상대경로(슬래시 표기 — 재귀 모음에서 "어느 폴더의 파일인가"
// 표기 계약), name=말단 파일명, size/mtime=스탬프(mtime은 last_write_time의
// time_since_epoch().count() 정수열 — 갤러리 ThumbStamp 원문 수형; 같은 기기
// 안에서 안정, 시계 epoch는 플랫폼 몫).
struct Track {
    std::string full;
    std::string rel;
    std::string name;
    long long size = 0;
    long long mtime = 0;
};

// ---- 재귀 순환 가드 (T1 fix r1 — 리뷰 I-1: 심링크 디렉터리 무한 재귀) ----

// 방문 정체성 키. posix: 디렉터리 하드링크가 있어 심링크·하드링크 순환이 모두
// 성립한다 — (st_dev, st_ino) 방문 집합이 순환 절단+동일 실제 폴더 이중 계수
// 방지(링크 경유 재등장)를 동시에 봉합한다. Windows: 디렉터리 하드링크가 없어
// 정체성 순환이 (심링크·junction 제외) 성립하지 않는다 — 키 자리는 문형 통일
// 이 있으나 미소비(아래 reparse 가드가 대신 봉합).
struct DirId {
    unsigned long long dev = 0;
    unsigned long long ino = 0;
};
inline bool operator<(const DirId& a, const DirId& b) {
    return a.dev != b.dev ? a.dev < b.dev : a.ino < b.ino;
}

#if defined(_WIN32)
// kernel32 단일 함수 선언 — windows.h 전개를 헤더에서 금한다(imgui/SDL/
// client 무접촉 계약·include 오염 회피). fileapi.h 원문 형식과 호환 선언
// (DWORD=unsigned long·LPCWSTR=const wchar_t* — windows.h 공존 TU도 무충돌).
extern "C" __declspec(dllimport) unsigned long __stdcall
    GetFileAttributesW(const wchar_t* fileName);
// FILE_ATTRIBUTE_REPARSE_POINT (winnt.h) — 상수 로컬 복제(원문 형식 주석).
inline constexpr unsigned long kFileAttributeReparsePoint = 0x400ul;

// Windows: reparse point(심링크·junction) 디렉터리를 재귀 대상에서 뺀다 —
// 이 가드 하나로 Windows 순환 전부 봉합. **순수 std 수형이 부족하다가 실측**:
// MinGW libstdc++는 junction(IO_REPARSE_TAG_MOUNT_POINT)을
// directory_entry::is_symlink()=**0**으로 놓치고(GetFileAttributesW는 링크
// 자동 속성에서 REPARSE=**1**을 잡는다 — engine/tmp/reparse_probe 실측
// 원문, 리포트 §2), canonical()도 junction을 해석하지 않아(항등 경로 실측)
// 경로 문자열 키로도 순환을 자르지 못한다. 정체성 검사 실패(링크 소멸 레이스
// 등) = 재귀 스킵(보수 파 — 못 읽으면 못 감, ec 중립형).
inline bool IsReparseDir(const std::filesystem::path& dir) {
    const unsigned long attr = GetFileAttributesW(dir.c_str());
    if (attr == 0xFFFFFFFFul)  // INVALID_FILE_ATTRIBUTES
        return true;
    return (attr & kFileAttributeReparsePoint) != 0ul;
}
#else
// posix: (st_dev, st_ino) 정체성 — ::stat가 심링크를 따른 값이라 정체성은
// "실제 폴더" 기준(링크 경유 재등장도 같은 id로 접힌다 — 이중 계수 방지).
inline bool DirIdOf(const std::filesystem::path& dir, DirId& out) {
    struct ::stat st;
    if (::stat(dir.c_str(), &st) != 0)
        return false;  // 못 읽으면 못 감(보수 파 — ec 중립형)
    out.dev = static_cast<unsigned long long>(st.st_dev);
    out.ino = static_cast<unsigned long long>(st.st_ino);
    return true;
}
#endif

// Recursive scan worker — ListAudioFiles의 재귀 leg. ec 중립형(throwing
// 오버로드 금지 — 갤러리/shot 원문 계약): dir 열기 실패 = 독립 실패(빈 목록,
// 스펙 §2 "각 dir 독립 — 폴백 0건이어도 목록은 그린다"), 열거 중 소명 성분은
// 스킵. 하위 디렉터리 재귀는 순환 가드(fix r1) 이후에만.
inline void ScanAudioTree(const std::filesystem::path& dir,
                          const std::filesystem::path& root,
                          std::set<DirId>& visited, std::vector<Track>& out) {
    std::error_code ec;
    const std::filesystem::directory_iterator it(dir, ec);
    if (ec) return;  // 이 브랜치만 실패 — 다른 dir의 트랙은 살아 있다
    for (const std::filesystem::directory_entry& entry : it) {
        std::error_code entryEc;
        const bool isDir = entry.is_directory(entryEc);
        if (entryEc) continue;  // 열거 스캔 중 소멸 성분은 스킵
        if (isDir) {
#if defined(_WIN32)
            // reparse 디렉터리(심링크·junction) = 미진입(fix r1 가드).
            if (IsReparseDir(entry.path())) continue;
#else
            // 방문 집합 — 재방문(순환·링크 중복)은 스킵(fix r1 가드). 미삽입
            // 실패(::stat 실패)도 못 감(보수 파).
            DirId id;
            if (!DirIdOf(entry.path(), id) || !visited.insert(id).second)
                continue;
#endif
            ScanAudioTree(entry.path(), root, visited, out);
            continue;
        }
        const std::string ext = entry.path().extension().string();
        if (!IsAudioExtension(ext)) continue;
        std::error_code te, se;
        const std::filesystem::file_time_type t = entry.last_write_time(te);
        if (te) continue;  // mtime을 못 읽는 성분은 열외(정렬 단정 보존)
        const uintmax_t sz = entry.file_size(se);
        if (se) continue;  // 개별 stat 실패 = 해당 항목 스킵(계약)
        Track tr;
        tr.full = entry.path().generic_string();
        tr.name = entry.path().filename().string();
        tr.size = static_cast<long long>(sz);
        tr.mtime = static_cast<long long>(t.time_since_epoch().count());
        std::error_code re;
        std::filesystem::path relP =
            std::filesystem::relative(entry.path(), root, re);
        if (re || relP.empty()) relP = entry.path().filename();  // 방어선
        tr.rel = relP.generic_string();
        out.push_back(std::move(tr));
    }
}

// Enumerate one root's audio files across the whole tree, newest first
// (selftest 2m-c — D3 재귀 + mtime desc 정렬). 확장자 필터는
// IsAudioExtension(mp3/flac/wav/ogg/m4a/aac/wma, 대소문자 무시 — Stricmp).
// Sort tie(같은 mtime) = rel 오름차순 — 재귀 열거 순서가 축(Win/WSL)마다
// 다르게 흔들릴 수 있어 tie를 rel로 못 매 둔다(결정론 보존; 갤러리 원문의
// "동점 tiebreak" 자리 계약 승계 — 갤러리는 이름 desc, 유저 트랙 폴더에는
// 어휘 오름차순이 읽기 순서라 asc를 선택 — 구현 재량, 리포트 부기).
// 없는 루트/빈 루트 = 빈 목록(독립 스캔 실패 — ok 플래그 없이 0건으로 표현,
// 스펙 §2).
inline std::vector<Track> ListAudioFiles(const std::string& root) {
    std::vector<Track> out;
    if (root.empty()) return out;  // 방어선(호출부 무접촉)
    const std::filesystem::path rootP(root);
    std::set<DirId> visited;
#if !defined(_WIN32)
    // 루트 정체성 미리 시드 — sub/loop→루트 수형(재귀가 루트로 되돌아오는
    // 순환)을 절단하는 원문(fix r1).
    DirId rootId;
    if (DirIdOf(rootP, rootId)) visited.insert(rootId);
#endif
    ScanAudioTree(rootP, rootP, visited, out);
    std::sort(out.begin(), out.end(),
              [](const Track& a, const Track& b) {
                  if (a.mtime != b.mtime) return a.mtime > b.mtime;
                  return a.rel < b.rel;
              });
    return out;
}

// ASCII fold (MatchFilter 유일 조작 — 한글은 이진 비교, v1 계약): 'A'-'Z'만
// 소문자로 접는다. unsigned 캐스트로 0x80 이상 바이트(UTF-8 후속 바이트)를
// 음수 인덱스로 굴리지 않는다 — 촉점 없음, 한글·기타 멀티바이트는 원문 그대로
// 이진 비교된다.
inline char FoldAscii(char c) {
    return (c >= 'A' && c <= 'Z')
               ? static_cast<char>(c - 'A' + 'a')
               : c;
}

// Name filter (selftest 2m-d — 스펙 "이름 필터 1행, 실시간 부분일치"):
// 빈 필터 = 전부 참(필터 꺼짐), 대소문자 무시 부분일치(ASCII fold만 —
// 한글은 이진 비교).
inline bool MatchFilter(const std::string& name, const std::string& filter) {
    if (filter.empty()) return true;
    std::string n;
    n.reserve(name.size());
    for (char c : name) n.push_back(FoldAscii(c));
    std::string f;
    f.reserve(filter.size());
    for (char c : filter) f.push_back(FoldAscii(c));
    return n.find(f) != std::string::npos;
}

// ---- 재생 위임 (T3 — 스펙 §4 D1) ----

// 위임 꾸러미 조립 (selftest 2m-g — 브리프 T3 원문 부품): app_tool 대상 인자
// 전문 {"app":"vplayer","tool":"open","args":{"path":"<full>"}} — app/tool은
// vplayer 도구 선언 원문(ClientVPlayerApp.cpp "open" — path 필수) 리터럴.
// **windowId 미기술**(서버 app_tool 릴레이 후보 수집 계약 — JKWindowServer
// §4.2: 지정=직행, 미지정+단일 후보=직행, 미지정+복수=ambiguous+후보 목록
// 자기교정 — vplayer 다중 인스턴스에서만 그 경로가 열리고, 클라는 추측 없이
// 그 표기를 그대로 흡수한다). core는 경로 문자열판(답신 수령 후 사본 path로
// 재청구하는 폴백이 소비), Track 판은 full 원문을 건네는 얇은 껍데기.
// path 표기: '\'→'/' 정규화만(generic_string 원형 — MusicDirFallback/Track
// 어느 쪽도 같은 표기, Windows 수형 경로가 JSON 이스케이프로 쌓이지 않고
// 폰/WSL의 '/' 표기와 동형이 된다 — vplayer OpenPath는 fs::path로 다시
// 접는다). 이어지는 이스케이프는 ClientLibraryApp::EscapeJson 쌍둥이 원문
// 수형(따옴표·제어 문자 \uXXXX — 역슬래시는 정규화가 전부 먹는다). 런치
// 인자 {"app":"vplayer"}는 이 부품의 대상이 아니다(핸들러 리터럴 — 라이브러
// 리 LaunchSelected 원문 쌍둥이, 쌍 쿼리 ①launch_app ②app_tool).
inline std::string OpenRequestJsonPath(const std::string& full) {
    std::string out;
    out.reserve(56 + full.size());
    out += "{\"app\":\"vplayer\",\"tool\":\"open\",\"args\":{\"path\":\"";
    for (const char ch : full) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (ch == '\\') {
            out += '/';                    // 슬래시 정규화
        } else if (ch == '"') {
            out += "\\\"";
        } else if (c < 0x20) {
            char num[8];
            std::snprintf(num, sizeof(num), "\\u%04x", static_cast<int>(c));
            out += num;
        } else {
            out += ch;                     // 공백 포함 원문 수용 — JSON 문자열
                                           //   꾸러미가 감싸므로 이스케이프 불요
        }
    }
    out += "\"}}";
    return out;
}
inline std::string OpenRequestJson(const Track& t) {
    return OpenRequestJsonPath(t.full);
}

} // namespace music
} // namespace jk

#endif // MUSICMODEL_H