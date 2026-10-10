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
//
// 취소 기구 (#93 T1 — 플랜 2026-10-10-music-scan-cancel): ScanCancelFn(참=
// 취소)을 스캔 leg에 폭탄 전달 — 재귀 경계(진입)마다 `if (cancel()) return;`,
// 엔트리 256개마다 보조 체크(단일 디렉터리가 수천 엔트리여도 취소에 응답).
// 취소 시 부분 수집을 폐기하고 빈 목록을 반환한다(2p 원문). 무인자
// ListAudioFiles 오버로드는 항상-false 콜백 위임 — 원존 시맨틱 무변조
// (기존 캐논 케이스 전원 그대로 통과).

#include <agent/JKAgentJson.h>
#include <port/JKCrtShim.h>  // Stricmp — Win/posix 공용 ASCII 대소문자 무시 비교

#include <algorithm>
#include <cstdint>
#include <cstdio>  // OpenRequestJsonPath의 snprintf — 전이 include 의존 봉합(M-1)
#include <filesystem>
#include <functional>
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

// 역슬래시 슬래시 접기 (T1 fix r1 — 플랜 2026-10-10-music-dirs-ui M-3 승계):
// 2m-g 원문 수형(OpenRequestJsonPath의 문자 스캔). fs::path 정규형만으로는
// '\'가 Windows 축에서만 스페이퍼라 위 fs::path 접기가 축 분기한다 — posix
// (폰·WSL) 축에서 '\'는 그냥 파일명 문자 남는다(2m-b 원문 계약 — "backslash
// 수형은 문자 스캔이 소유"). 리졸버와 문서 스캐너(MusicDirStore.h)가 같은
// 이 접기를 먼저 간다 — 본체는 리졸버 쪽(더 이른 소비).
//
// fix r1 — 고바이트 뒤 0x5C는 리터럴(데이터 무손상 승규약): CP949 확장 영역
// (선단 0x81-A0)의 후행 0x5C 파일명 문자(뷁류)를 스페이퍼로 접으면 경로
// 문자열 자체가 변질되고 재쓰기에서 소실한다. 한계: UTF-8 멀티바이트 바로
// 뒤의 진짜 스페이퍼 '\'는 접히지 않는다("D:/가요\MPC" 표기 혼합 — 데이터는
// 무손상, 정규화 철자만 혼합 — T2 UI가 호출부 슬래시 형식을 존중하는 규약).
inline std::string Slashize(const std::string& p) {
    std::string s;
    s.reserve(p.size());
    bool prevHigh = false;
    for (char ch : p) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c == '\\' && !prevHigh) {
            s += '/';
            prevHigh = false;
            continue;
        }
        s += ch;
        prevHigh = (c >= 0x80);
    }
    return s;
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
        dirs.push_back(Slashize(user));  // fix r1 — 역슬래시 접기 승계(M-3)
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

// Scan cancel callback (#93 T1): 참 = 취소 요청. 콜백은 재귀 경계에서만
// 청구된다(진입 1회 + 엔트리 256개마다 보조 1회) — 경계당 최대 1회 원문
// (2p-② 호출 상한 단정).
using ScanCancelFn = std::function<bool()>;

// Recursive scan worker — ListAudioFiles의 재귀 leg. ec 중립형(throwing
// 오버로드 금지 — 갤러리/shot 원문 계약): dir 열기 실패 = 독립 실패(빈 목록,
// 스펙 §2 "각 dir 독립 — 폴백 0건이어도 목록은 그린다"), 열거 중 소명 성분은
// 스킵. 하위 디렉터리 재귀는 순환 가드(fix r1) 이후에만.
//
// 취소(#93 T1): 진입 체크가 "자식 재귀 직전" 경계를 겸한다 — 호출자가
// 내려보내는 지점에서 callee 진입이 먼저 판정하므로 경계당 콜백 청구는
// 정확히 1회(2p-② 상한 원리). 취소 시 즉시 복귀(이 브랜치만 절단 — 형제와
// 상위의 나머지 열거는 계속되고, 최종 폐기는 ListAudioFiles 래치가 소관).
// 엔트리 256개마다 보조 체크 — 단일 디렉터리 폭주(수천 엔트리)에서도 취소에
// 응답한다(경계가 디렉터리마다뿐이면 폭주 leg를 끝까지 견딘다).
inline void ScanAudioTree(const std::filesystem::path& dir,
                          const std::filesystem::path& root,
                          std::set<DirId>& visited, std::vector<Track>& out,
                          const ScanCancelFn& cancel) {
    if (cancel()) return;  // 재귀 경계 — 진입(자식 재귀 직전 경계 포함)
    std::error_code ec;
    const std::filesystem::directory_iterator it(dir, ec);
    if (ec) return;  // 이 브랜치만 실패 — 다른 dir의 트랙은 살아 있다
    int aux = 0;  // 엔트리 보조 취소 체크 계수기
    for (const std::filesystem::directory_entry& entry : it) {
        if (++aux == 256) {  // 엔트리 256개마다 보조 체크(#93 T1)
            aux = 0;
            if (cancel()) return;
        }
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
            ScanAudioTree(entry.path(), root, visited, out, cancel);
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
//
// 취소판(#93 T1): cancel 참이 되는 즉시(래치) 스캔을 절단하고 **부분 수집을
// 폐기한 빈 목록**을 반환한다(부분 결과 유출 금지 — 2p-①/② 원문). 콜백 청구
// 경계는 ScanAudioTree 원문(진입 + 엔트리 256 보조) 그대로 — 래치는 해소 후
// 재청구를 막아 경계당 1회를 보장한다(2p-② 호출 상한).
inline std::vector<Track> ListAudioFiles(const std::string& root,
                                         const ScanCancelFn& cancel) {
    std::vector<Track> out;
    if (root.empty()) return out;  // 방어선(호출부 무접촉)
    const std::filesystem::path rootP(root);
    std::set<DirId> visited;
    bool fired = false;  // 취소 래치 — 콜백이 한 번 참이면 끝까지 참
    auto latch = [&fired, &cancel]() -> bool {
        if (fired) return true;
        if (cancel()) {
            fired = true;
            return true;
        }
        return false;
    };
#if !defined(_WIN32)
    // 루트 정체성 미리 시드 — sub/loop→루트 수형(재귀가 루트로 되돌아오는
    // 순환)을 절단하는 원문(fix r1).
    DirId rootId;
    if (DirIdOf(rootP, rootId)) visited.insert(rootId);
#endif
    ScanAudioTree(rootP, rootP, visited, out, latch);
    if (fired)  // 취소 = 부분 수집 폐기(빈 목록 — 정렬도 생략)
        return std::vector<Track>();
    std::sort(out.begin(), out.end(),
              [](const Track& a, const Track& b) {
                  if (a.mtime != b.mtime) return a.mtime > b.mtime;
                  return a.rel < b.rel;
              });
    return out;
}

// 원존 오버로드 (시맨틱 무변조 — #93 T1): 취소 불요 호출부(ClientMusicApp
// 워커 포함)는 항상-false 콜백 위임 — 캐논 기존 케이스 전원이 그대로 통과
// 하는 수형(2p-④).
inline std::vector<Track> ListAudioFiles(const std::string& root) {
    return ListAudioFiles(root, [] { return false; });
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

// ---- 폴더 브라우저 순수 부품 (#97 T1 — 플랜 2026-10-11-music-dir-browser) ----
//
// 내장 미니 브라우저(플랜 결정 ①~⑤ — 별도 대화상자 창/AppTool 릴레이 없음,
// 키보드 불요)의 모델 leg. cwd 한 겹의 **폴더 목록만** 책임진다: 브라우저는
// 폴더 전용(결정 ② — 확장자 필터와의 혼동 방지)이라 파일은 목록에서 보이지
// 않고, 상위 ".." 행은 UI 측 상수 행(모델이 내보내지 않는다 — UI가 Parent로
// 경로만 계산). fs 접점 계약은 본 header 원문 승계: ec 중립형(throwing 금지 —
// 갤러리/shot 원문 계약), '/' 규약(#94 — 모든 경로는 슬래시 표기,
// Slashize 재용), 열기 실패 = 빈 페이지(빈 반환 — ok 플래그 없이 "없는 폴더"
// 는 목록 0건으로 읽는다).
namespace browse {

// cwd의 subdir 원문만 담는 꾸러미 — 상위 이동·이 폴더 추가는 UI가
// Parent/Store.AddDir로 조립(결정 ③)하므로 모델은 목록 원문만.
struct DirPage {
    std::vector<std::string> dirs;
};

// cwd의 폴더만(파일·상위 ".." ·숨김 제외), 이름 오름차순(byte asc — 결정론:
// 재귀 열거 순서가 축(Win/WSL)마다 흔들리는 것과 같은 함정이므로 정렬로
// 못 매 둔다 — ListAudioFiles tie 계약 원문 수형). 숨김 = 선단 '.' 도트
// 이름(LibraryCatalog 수형 — ".git"/".hidden" 스킵, ".",".." 도 도트 런으로
// 자연 스킵). 없는 cwd/빈 cwd = 빈 페이지(ec 중립형).
inline DirPage ListSubdirs(const std::string& cwd) {
    DirPage page;
    if (cwd.empty()) return page;  // 방어선(호출부 무접촉)
    std::error_code ec;
    const std::filesystem::directory_iterator it(cwd, ec);
    if (ec) return page;  // 열기 실패(부재 cwd 포함) = 빈 페이지(독립 실패)
    for (const std::filesystem::directory_entry& entry : it) {
        std::error_code entryEc;
        const bool isDir = entry.is_directory(entryEc);
        if (entryEc || !isDir) continue;  // 열거 중 소멸 성분·파일 = 스킵
        const std::string name = entry.path().filename().string();
        if (name.empty() || name.front() == '.') continue;  // 숨김 .도트 스킵
        page.dirs.push_back(std::move(name));
    }
    std::sort(page.dirs.begin(), page.dirs.end());  // 바이트 오름차순 결정론
    return page;
}

// 폴더 합성 — '/' 규약(#94): cwd의 말단 구분자를 접고 1슬래시로 붙인다.
// 끝 슬래시 유무 무관 같은 결과(2r 수형). cwd의 '\'는 Slashize 접기(fix r1
// — 고바이트 뒤 0x5C 리터럴 계약까지 원문 승계).
// fix r1 — 루트 폼 2건(리뷰 M-1·M-2, 같은 3행의 같은 원인): ①드라이브
// 루트 조건행(base.size()==2 → '/' 재부여)이 이어지는 무조건 `+= '/'`와
// 겹쳐 "I://" 이중 슬래시를 만들었다(Win32가 directory_iterator("I://")를
// ec=0로 받아들이는 실측 때문에 셀프테스트가 영원히 미검출 — 회귀 봉인
// 2r-b). 조건행 삭제+무조건 1슬래시 하나로 충분("C:"+"/" = 드라이브-상대
// 경로 "C:name" 함정 회피 원문 유지). ②posix 절대 루트 "/"는 말단 접기가
// ""로 만들어 빈-cwd 분기에 빠져 상대명을 돌려주었다(ListSubdirs가 프로세스
// cwd 기준으로 읽는 검사 무결성 위반 — 회귀 봉인 2r-c). 판정은 접기
// **이전** 절대성 표지로 한다 — 슬래시뿐인 폼("/"·"///")은 접기 후 ""가 되
// 어 접기 뒤 판정으로는 도달 불가("Parent(/sdcard)=/" 사다리가 "/"를 정당
// cwd로 도달시키므로 루트는 반드시 열린다).
inline std::string JoinDir(const std::string& cwd, const std::string& name) {
    const std::string s = Slashize(cwd);   // '\'→'/' 접기(원문 수형)
    const std::string rest = Slashize(name);
    // 절대성 표지 — 마커: 선단 '/' = 절대 경로 폼. 접기로 빈값이 되면
    // 이 표지로 루트("/"+"name")와 빈 cwd(이름 원문)를 갈라낸다.
    const bool absoluteRoot = (!s.empty() && s.front() == '/');
    std::string base = s;
    while (!base.empty() && base.back() == '/') base.pop_back();
    if (base.empty())
        return absoluteRoot ? "/" + rest : rest;  // 루트 보존 | 상대 조립
    base += '/';  // 드라이브 루트 "C:"도 이 1슬래시로 "C:/name" 성립(fix r1)
    base += rest;
    return base;
}

// 상위 한 단 — 1단 제거. 루트에서 상위 = 빈 문자열(최상위 부모는 자기 반환
// 금지 — fs::path::parent_path는 루트("C:/", "/")에서 자기 자신을 돌려주는
// 실측 함정이 있어 문자 스캔 승계): 루트/빈값/단일 성분 = 빈값, "/a" = "/"
// (루트 표기 원문), 중간 단 = 1슬래시 앞 잘라 원문 승계. 말단 구분자는 접고
// 계산한다("sub/" = "sub" — JoinDir 접기 수형 재용).
inline std::string Parent(const std::string& cwd) {
    std::string s = Slashize(cwd);
    while (!s.empty() && s.back() == '/') s.pop_back();
    if (s.empty()) return {};                     // 루트/빈값 — 상위 없음
    const size_t pos = s.rfind('/');
    if (pos == std::string::npos) return {};      // 단일 성분 — 상위 없음
    if (pos == 0) return "/";                     // "/a" → 루트 표기 원문
    std::string out = s.substr(0, pos);
    if (out.size() == 2 && out[1] == ':') out += '/';  // 드라이브 루트 유지
    return out;
}

} // namespace browse

// ---- 위임 답신 본문 재판정 (T3 fix r3 — T4 결함 원장: 폴백 자기 소멸) ----

// 위임 답신의 대분법. 클라 봉투(AgentReply.ok)는 전송원이 임의로 고정한 수가
// 들어온다: 서버 즉답 경로(agent 쿼리의 sync 답신 — JKWindowServer.cpp
// WriteAgentJson(..., 1, reply))는 봉투 ok=**1 고정**(본문
// {"ok":false,"error":"unknown_app_tool"}와 무관 — T4 MUSIC-GLUE-COLD-REPLY
// 실측), 릴레이 지연응답(HandleToolResult)은 봉투 ok=앱 결과 ok. 봉투만 믿으면
// 즉답 거부를 성공으로 읽어 재청구 크레딧이 최초 거부에서 즉시 소멸한다(T3
// fix r2까지의 클라 판정이 이 겉돌이로 T4 폴백 자기 소멸 실측). 진실원은
// **본문 판정** — 본문이 "ok"를 기술하면 본문(봉투 무신), 미기술이면 봉투
// 원문 fallback(fix r1 I-2 계약의 본문계층 승격). err는 본문 error 표기 원문(
// "unknown_app_tool"=재청구 대상 — 도구 등록 경기, "ambiguous"=복수 후보
// 자기교정, 그 밖=즉시 종착 부기).
struct DelegationVerdict {
    bool ok = false;       // 본문 기술 ok(봉투 무신) 또는 봉투 fallback
    std::string err;       // 본문 "error" 원문(기술 시만 — 판정 보조 표기)
};

inline DelegationVerdict DelegationReplyVerdict(bool envelopeOk,
                                                const std::string& replyJson) {
    DelegationVerdict v;
    const agent::AgentJson body(replyJson);
    int bodyOk = 0;        // 본문 "ok"는 불리언(true/false) — Int 리더가
                           // JS_ToInt64로 1/0을 읽는다(숫자 표기 원문 수용)
    if (body.ok() && body.GetInt("ok", bodyOk)) {
        v.ok = (bodyOk != 0);
    } else {
        v.ok = envelopeOk; // 본문 ok 미기술·파손 — 봉투 원문 fallback
    }
    if (!body.ok() || !body.GetStr("error", v.err)) v.err.clear();
    return v;
}

} // namespace music
} // namespace jk

#endif // MUSICMODEL_H