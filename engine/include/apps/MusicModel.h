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
#include <cstring>  // memcmp — 태그 매직·프레임 ID 판별(#99 T1)
#include <filesystem>
#include <fstream>  // ParseTag — ifstream(fs::path 개방 — 2q 실측 수형, #99 T1)
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

// ---- 태그 파서 (T1 — 스펙 2026-10-11-music-tags-design) ----
//
// 곡명/아티스트/앨범을 파일에서 읽어내는 **순수 부품**(전부 헤더 인라인 —
// 버퍼판) + 파일 접점(ParseTag — 필요 범위만 읽는 D2 원문). 조달 0 원칙
// 승계(D1): 파서는 own이고 조달된 third_party(dr_flac/dr_mp3/dr_wav/
// stb_vorbis)의 내장 메타 API는 T1 실측 결과 **본 헤더에서 재용 불가** —
// ①공급 경로 계약: spatial-player 레포의 헤더는 SPATIAL_PLAYER_ROOT 환경변수
// 힌트로만 소비(레포 경로의 커밋 소스 기재 금지 규약)라 MusicModel.h에
// include 경로를 남길 수 없고, audio_core는 레그 온일 때만 링크(미설정 빌드
// = API 자체 부재). ②stb_vorbis_get_comment는 열린 stb_vorbis*를 요구 —
// 열기가 v2.4의 코드북·setup 헤더 체인 전체를 요구해 **합성 블롭이 열리지
// 않는다(2s ⑧ 결함 원문 봉인). ③dr_mp3 onMeta는 **원문 태그데이터 전달만**
// (dr_mp3.h 5082: "Currently this only reports the raw tag data which means
// applications need to parse the data themselves") — 텍스트 프레임 파서 갭이
// own ID3v2 파서가 소관. ④dr_flac/dr_wav의 메타 API는 내부 narrow fopen
// (Windows ACP 해석 — #94 인코딩 사슬 함정)을 태워 네이티브 경로 개방 자체를
// 어긋내고, utf8 fs::path 개방으로 대체한다.
//
// FLAC/Ogg 코멘트는 동일 vorbis comment 구조(vendor+count+entries)라
// ParseVorbisComments 공용 1개를 두고, 컨테이너 해석만 FLAC 블록 사슬
// (ParseFlacTags)과 Ogg 페이지 연쇄(ParseOggTags)로 갈라낸다.
namespace tags {

// 구조 계약(브리프 원문 — 3필드 고정). TDRC/TRCK 프레임은 D1 표기에 있으나
// 싱크 필드가 없어(원장 부기) v1 스코프 밖 — 프레임 사슬에서 건너뛴다(2s ①의
// TRCK 포함 블롭 실측).
struct TagSet {
    std::string title, artist, album;
};

// 스캔 1패스 접점이 읽는 머리 영역 상한(D2 — "필요 범위만 read"). FLAC 메타
// 사슬·ID3v2 태그(커버 아트 포함)는 통상 수십 KB — 1MB 컷은 실사용 여유이고
// 폭주(대형 사진 프레임)를 절단한다(잠복: 사진이 코멘트 앞인 드문 정렬은
// 태그 누락 — 리포트 부기).
inline constexpr size_t kTagHeadCap = 1u << 20;

inline bool TagEmpty(const TagSet& t) {
    return t.title.empty() && t.artist.empty() && t.album.empty();
}

// ---- 인코딩 정규화 (UTF-8 규약 #94 — 파서가 정규화한다) ----

// Latin-1 승격 (원장 명시 — CP949 아님): 0x00-0x7F는 바이트 동치 그대로,
// 0x80-0xFF는 2바이트 UTF-8로 승격(첫 바이트 0xC2/0xC3 — Latin-1 유니코드
// 계수식의 직접 전사). ID3v2 인코딩 바이트 0(ISO-8859-1)·ID3v1 필드 규약.
inline std::string Latin1ToUtf8(const unsigned char* p, size_t n) {
    std::string out;
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        const unsigned char c = p[i];
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
        } else {  // 0xC2 | (c>>6): 0x80-0xBF → 0xC2, 0xC0-0xFF → 0xC3
            out.push_back(static_cast<char>(0xC2 | (c >> 6)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
    return out;
}

inline void AppendCodepointUtf8(std::string& out, unsigned int cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// UTF-16 유닛열(바이트, LE/BE 지정) → UTF-8. 느슨 규약: 짝이 밑 빠진
// 서러게이트는 유닛 단위 스킵(태그 데이터 전멸 계약 아님 — 남는 값이 진
 // 규약보다 우선). 홀수 말단 바이트는 무시. NUL 유닛(0x0000)도 코드포인트로
// 그대로 → 종결 절단은 PostprocessText의 첫 NUL 절단이 자동 소화.
inline std::string Utf16BytesToUtf8(const unsigned char* p, size_t byteN,
                                    bool bigEndian) {
    std::string out;
    size_t i = 0;
    const auto unit = [p, bigEndian](size_t k) -> unsigned int {
        return bigEndian ? (static_cast<unsigned int>(p[k]) << 8) | p[k + 1]
                         : static_cast<unsigned int>(p[k]) | (p[k + 1] << 8);
    };
    while (i + 2 <= byteN) {
        const unsigned int u = unit(i);
        i += 2;
        if (u >= 0xD800 && u <= 0xDBFF) {  // high surrogate — 쌍 결합
            if (i + 2 > byteN) break;      // 절단 — 유닛 스킵(느슨 규약)
            const unsigned int v = unit(i);
            if (v < 0xDC00 || v > 0xDFFF) continue;  // 쌍 미스 — 스킵
            i += 2;
            AppendCodepointUtf8(
                out, 0x10000 + ((u - 0xD800) << 10) + (v - 0xDC00));
        } else if (u >= 0xDC00 && u <= 0xDFFF) {
            // low surrogate 단독 — 스킵
        } else {
            AppendCodepointUtf8(out, u);
        }
    }
    return out;
}

// ID3 텍스트 프레임 본문 → UTF-8. enc: 0=ISO-8859-1(Latin-1 승격)·
// 1=UTF-16(BOM 절취 — 무BOM=LE 계약, v1 확장 원문)·2=UTF-16BE(BOM 없음)·
// 3=UTF-8 pass(검증 없는 통과 — #94 파서 정규화의 pass leg).
inline std::string DecodeId3Text(const unsigned char* p, size_t n, int enc) {
    if (enc == 1) {  // BOM 우선 — FF FE=LE·FE FF=BE·무BOM = LE(v1 계약)
        if (n >= 2 && p[0] == 0xFF && p[1] == 0xFE)
            return Utf16BytesToUtf8(p + 2, n - 2, false);
        if (n >= 2 && p[0] == 0xFE && p[1] == 0xFF)
            return Utf16BytesToUtf8(p + 2, n - 2, true);
        return Utf16BytesToUtf8(p, n, false);
    }
    if (enc == 2) return Utf16BytesToUtf8(p, n, true);
    if (enc == 3) return std::string(reinterpret_cast<const char*>(p), n);
    return Latin1ToUtf8(p, n);
}

// 값 정규화 1행 — 첫 NUL 절단(다중 값/종결, v2.4 NUL 사슬 = 첫 값 계약) +
// 말미 ASCII 공백/NUL rstrip(ID3v1 스페이스 패딩 수형 재용). 0x80 이상
// 바이트는 절대 벗기지 않는다(UTF-8 후속 바이트 무손상 — FoldAscii 원문 계약).
inline std::string PostprocessText(std::string s) {
    const size_t nul = s.find('\0');
    if (nul != std::string::npos) s.resize(nul);
    while (!s.empty() &&
           (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' ||
            s.back() == '\n'))
        s.pop_back();
    return s;
}

// --- 하부 리더 (모든 접점 경계 클램프 — 2s ⑤ 결함 봉인 원문) ---

// syncsafe(4바이트, 바이트당 하위 7비트). 0x7F 7F 7F 7F = 0x0FFFFFFF
// (2^28-1 — syncsafe32의 최대 — 2s ⑤ 경계 수치). 상위 비트는 무시 마스크.
inline unsigned long long SyncSafe32(const unsigned char* b) {
    return (static_cast<unsigned long long>(b[0] & 0x7F) << 21) |
           (static_cast<unsigned long long>(b[1] & 0x7F) << 14) |
           (static_cast<unsigned long long>(b[2] & 0x7F) << 7) |
           (b[3] & 0x7F);
}

// 32비트 BE/LE (v2.3 프레임 크기·RIFF 청크 크기·vorbis 개수 — 무부호).
inline unsigned int RdBE32(const unsigned char* b) {
    return (static_cast<unsigned int>(b[0]) << 24) |
           (static_cast<unsigned int>(b[1]) << 16) |
           (static_cast<unsigned int>(b[2]) << 8) | b[3];
}
inline unsigned int RdLE32(const unsigned char* b) {
    return static_cast<unsigned int>(b[0]) |
           (static_cast<unsigned int>(b[1]) << 8) |
           (static_cast<unsigned int>(b[2]) << 16) |
           (static_cast<unsigned int>(b[3]) << 24);
}

// ---- ID3v2.3/2.4 (own 파서 — dr_mp3 갭 봉합, 실측 리포트) ----

// v2.2(6바이트 프레임 헤더·TT2류)는 스펙 스코프 밖(§1 — v2.3/2.4)·
// v2.2는 빈 TagSet(폴백 규약 — v1에만 다시 도달).
inline TagSet ParseId3v2(const unsigned char* p, size_t n) {
    TagSet tags;
    if (n < 10) return tags;
    if (p[0] != 'I' || p[1] != 'D' || p[2] != '3') return tags;  // 매직
    const int ver = p[3];
    if (ver < 3 || ver > 4) return tags;  // 2.3/2.4만 (2.2 = 스코프 밖)
    const unsigned char flags = p[5];
    const unsigned long long tagSize = SyncSafe32(p + 6);
    // 유한 본문 — 선언 크기가 버퍼를 넘으면 클램프(2s ⑤ 2^28-1 파산 블롭 —
    // 선언을 믿지 않는 경계 클램프 원문).
    const unsigned long long endU = 10ULL + tagSize;
    const size_t bodyEnd = endU > static_cast<unsigned long long>(n)
                               ? n
                               : static_cast<size_t>(endU);
    // v2.3 전역 unsynchronisation(헤더 flags 0x80): 0xFF 0x00 → 0xFF 전개 —
    // 본문을 사본에 전개해 같은 사슬 걸음(규약 하나·사이본 상한 1MB).
    std::vector<unsigned char> unsync;
    const unsigned char* body = p + 10;
    size_t bodyN = bodyEnd - 10;
    if (ver == 3 && (flags & 0x80)) {
        unsync.reserve(bodyN);
        for (size_t i = 0; i < bodyN; ++i) {
            unsync.push_back(body[i]);
            if (body[i] == 0xFF && i + 1 < bodyN && body[i + 1] == 0x00) ++i;
        }
        body = unsync.data();
        bodyN = unsync.size();
    }
    // 프레임 사슬 — 10B 헤더 반복. 패딩(선단 0x00)·부서진 프레임 = 전체
    // 종료(무리한 재동기 금지 — v1 원문 계약), 태그 크기 = 사슬 절단.
    size_t off = 0;
    while (off + 10 <= bodyN) {
        const unsigned char* fh = body + off;
        if (fh[0] == 0) break;  // 패딩 — 남은 영역은 스킵(2s ⑥)
        // 프레임 ID — v2.4 관용: 글자판 'A'-'Z'·'0'-'9' 4바이트. 탈자는
        // 파산(절단 — 파산 프레임 경계에서 재동기 침공 금지).
        const auto fidByte = [](unsigned char c) {
            return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        };
        if (!fidByte(fh[0]) || !fidByte(fh[1]) || !fidByte(fh[2]) ||
            !fidByte(fh[3]))
            break;
        // 프레임 크기: v2.3 = BE32. v2.4 = syncsafe, 단 바이트가 0x7F를 넘
        // 면 구형 리터(구형 v2.4 리터가 BE32를 남겼다 — 관용 폴백, 원문 부기)
        size_t fsz;
        if (ver == 4 && (fh[4] <= 0x7F && fh[5] <= 0x7F && fh[6] <= 0x7F &&
                         fh[7] <= 0x7F)) {
            fsz = static_cast<size_t>(SyncSafe32(fh + 4));
        } else {
            fsz = static_cast<size_t>(RdBE32(fh + 4));
        }
        const unsigned char fmt = fh[9];
        // v2.3 포맷 플래그(포맷 바이트 = fh[9]): 0x80 압축·0x40 암호화·
        // 0x20 그룹. v2.4: 0x40 그룹·0x08 압축·0x04 암호화·0x02 unsync·
        // 0x01 데이터 길이 표지(dli). 압축/암호화/unsync 프레임 = 스킵.
        const bool compressed = ver == 3 ? (fmt & 0x80) : (fmt & 0x08);
        const bool encrypted = ver == 3 ? (fmt & 0x40) : (fmt & 0x04);
        const bool grouped = ver == 3 ? (fmt & 0x20) : (fmt & 0x40);
        const bool frameUnsync = ver == 4 && (fmt & 0x02);
        const bool dli = ver == 4 && (fmt & 0x01);
        off += 10;
        const size_t fLen = fsz < bodyN - off ? fsz : bodyN - off;  // 클램프
        const size_t nextOff = off + fLen;
        if (compressed || encrypted || frameUnsync) {
            off = nextOff;
            continue;  // 미해독 프레임 — 건너 뛴다(태그 누락 폴백)
        }
        size_t d = off;
        if (grouped) d += 1;  // 그룹 ID 1바이트 선단
        if (dli) d += 4;      // v2.4 데이터 길이 표지 4바이트(syncsafe)
        const size_t dEnd = off + fLen;
        // 텍스트 프레임 3종 + TDRC/TRCK는 싱크 없음 — 건너 뛴다
        if (d + 1 <= dEnd) {
            const int enc = body[d];
            const unsigned char* txt = body + d + 1;
            const size_t txtN = dEnd - (d + 1);
            std::string field;
            if (fh[0] == 'T' && fh[1] == 'I' && fh[2] == 'T' &&
                fh[3] == '2') {
                if (tags.title.empty())
                    tags.title =
                        PostprocessText(DecodeId3Text(txt, txtN, enc));
            } else if (fh[0] == 'T' && fh[1] == 'P' && fh[2] == 'E' &&
                       fh[3] == '1') {
                if (tags.artist.empty())
                    tags.artist =
                        PostprocessText(DecodeId3Text(txt, txtN, enc));
            } else if (fh[0] == 'T' && fh[1] == 'A' && fh[2] == 'L' &&
                       fh[3] == 'B') {
                if (tags.album.empty())
                    tags.album =
                        PostprocessText(DecodeId3Text(txt, txtN, enc));
            }
        }
        off = nextOff;
    }
    return tags;
}

// ---- ID3v1 (말미 128B — 폴백 계약) ----

// 버퍼의 말미 128바이트("TAG"+30/30/30+4년+1장르). 필드 = 스페이스 패딩
// 러스트립(공백 철자 보존 — 선단은 그대로)·Latin-1 승격(#94 원문 파서 규약).
// 128바이트 미만·매직 불일치 = 빈 TagSet.
inline TagSet ParseId3v1(const unsigned char* p, size_t n) {
    TagSet tags;
    if (n < 128) return tags;
    const unsigned char* t = p + (n - 128);
    if (t[0] != 'T' || t[1] != 'A' || t[2] != 'G') return tags;
    const auto field = [&t](size_t start, size_t len) {
        size_t end = len;
        while (end > 0 && (t[start + end - 1] == ' ' || t[start + end - 1] == 0))
            --end;
        return PostprocessText(Latin1ToUtf8(t + start, end));
    };
    tags.title = field(3, 30);
    tags.artist = field(33, 30);
    tags.album = field(63, 30);
    return tags;
}

// ---- vorbis comment (공용 구조 — FLAC 블록·Ogg 코멘트 패킷 공용) ----

// [32LE vendorlen][vendor][32LE count][count × ([32LE len]"KEY=value").
// 코멘트 값은 UTF-8 규약(구조 규약 통과 — CP949 승계 없음 — 원장). 키는
// ASCII 폴드 대소문자 무시(FoldAscii 재용), 첫 TITLE/ARTIST/ALBUM만 취한다
// (다중 값 계약 = 첫 값 — ID3v2 사슬과 같은 판정).
inline TagSet ParseVorbisComments(const unsigned char* p, size_t n) {
    TagSet tags;
    size_t off = 0;
    unsigned int vlen = 0;
    // 32LE 경계 리더 — 부족하면 종료(폭주 선절단 — 결함 원문 방어).
    const auto le = [p, n, &off](unsigned int& out) -> bool {
        if (off + 4 > n) return false;
        out = RdLE32(p + off);
        off += 4;
        return true;
    };
    if (n < 4) return tags;
    if (!le(vlen)) return tags;
    if (vlen > n - off) return tags;  // 부서진 벤더 — 구조 파산
    off += vlen;
    unsigned int count = 0;
    if (!le(count) || count > 16384) return tags;  // 폭주 절단
    for (unsigned int i = 0; i < count; ++i) {
        unsigned int clen = 0;
        if (!le(clen)) break;
        if (clen > n - off) break;  // 잘림 — 남은 구조 파산
        const unsigned char* entry = p + off;
        off += clen;
        size_t eq = 0;
        while (eq < clen && entry[eq] != '=') ++eq;  // KEY= 형식
        if (eq == 0 || eq >= clen) continue;         // '=' 부재·빈 키 — 스킵
        std::string key;
        key.reserve(eq);
        for (size_t k = 0; k < eq; ++k)
            key.push_back(FoldAscii(static_cast<char>(entry[k])));
        const size_t valStart = eq + 1;
        const size_t valN = clen - valStart;
        std::string value(
            reinterpret_cast<const char*>(entry + valStart), valN);
        if (key == "title") {
            if (tags.title.empty()) tags.title = PostprocessText(value);
        } else if (key == "artist") {
            if (tags.artist.empty()) tags.artist = PostprocessText(value);
        } else if (key == "album") {
            if (tags.album.empty()) tags.album = PostprocessText(value);
        }
    }
    return tags;
}

// ---- FLAC 메타 사슬 (own — dr_flac 메타 API 재용 불가 실측의 대체 leg) ----

// "fLaC"+블록([1비트 last][7비트 type][24BE size]): 4=VORBIS_COMMENT 소비,
// 그 외(STREAMINFO/PADDING/PICTURE/SEEKTABLE…) = 사슬 절차만 건너 뛴다.
// 블록 상한 256(폭주 절단)·잘린 사슬 = 그 자리에서 종료(선언을 믿지 않음).
inline TagSet ParseFlacTags(const unsigned char* p, size_t n) {
    TagSet tags;
    if (n < 4 || std::memcmp(p, "fLaC", 4) != 0) return tags;
    size_t off = 4;
    for (int guard = 0; off + 4 <= n && guard < 256; ++guard) {
        const bool last = (p[off] & 0x80) != 0;
        const unsigned blockType = static_cast<unsigned>(p[off] & 0x7F);
        const size_t bsz = (static_cast<size_t>(p[off + 1]) << 16) |
                           (static_cast<size_t>(p[off + 2]) << 8) |
                           static_cast<size_t>(p[off + 3]);
        off += 4;
        if (off + bsz > n) break;  // 잘린 블록 — 파산(선언을 믿지 않음)
        if (blockType == 4) {  // DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT
            const TagSet c = ParseVorbisComments(p + off, bsz);
            // 필드별 합쳐 모음 — 첫 값 계약(여러 사슬을 걷더라도).
            if (tags.title.empty()) tags.title = c.title;
            if (tags.artist.empty()) tags.artist = c.artist;
            if (tags.album.empty()) tags.album = c.album;
        }
        off += bsz;
        if (last) break;
    }
    return tags;
}

// ---- Ogg 페이지 연쇄 (own — stb_vorbis 합성 불기입 실측의 대체 leg) ----

// "OggS" 페이지 연쇄(27B 헤더: 매직+버전+htype+granule8+serial4+seq4+crc4+
// 세그 수+세그 표)에서 페이로드를 모아 "\x03vorbis" 패킷(두 번째 패킷 —
// 코멘트)을 지목해 ParseVorbisComments로 넘긴다. 페이지 표 255 상한·잘린
// 페이지 = 파산. "\x03vorbis"는 머리 컷(1MB — kTagHeadCap)의 헤더 패킷
// 영역에서만 지목되므로 오디오 데이터의 우연 일치 접촉이 사실상 없다.
inline TagSet ParseOggTags(const unsigned char* p, size_t n) {
    TagSet tags;
    std::vector<unsigned char> payload;
    size_t off = 0;
    while (off + 27 <= n) {
        if (std::memcmp(p + off, "OggS", 4) != 0) break;  // 사슬 절단
        const size_t segs = p[off + 26];
        if (off + 27 + segs > n) break;  // 잘린 헤더
        size_t plen = 0;
        for (size_t s = 0; s < segs; ++s) plen += p[off + 27 + s];
        if (off + 27 + segs + plen > n) break;  // 잘린 페이로드
        payload.insert(payload.end(), p + off + 27 + segs,
                       p + off + 27 + segs + plen);
        off += 27 + segs + plen;
    }
    // "\x03vorbis" 검색 — 지목되면 그 뒤가 코멘트 구조(벤더+엔트리).
    for (size_t i = 0; i + 7 <= payload.size(); ++i) {
        if (payload[i] == 0x03 && std::memcmp(payload.data() + i + 1,
                                              "vorbis", 6) == 0) {
            tags = ParseVorbisComments(payload.data() + i + 7,
                                       payload.size() - (i + 7));
            break;  // 첫 코멘트 패킷만(v1 계약 — 코멘트는 패킷 2)
        }
    }
    return tags;
}

// ---- WAV — RIFF "id3 " 청크 (dr_wav 실측 대체 leg — LIST INFO 밖 스코프) ----

// RIFF("RIFF"+32LE 크기+"WAVE")의 청크 연쇄에서 "id3 " 청크(대소문자 무시
// — 리터 관용)의 payload를 ParseId3v2로 재용한다. 청크 사이즈 LE32·홀수
// 패딩 바이트 규약. dr_wav의 LIST INFO(INAM/IART/IPRD — 실측: 표 계약과
// 동형)는 T1 스코프 밖(리포트 부기 — id3 청크가 현실 WAV 태그의 주류).
inline TagSet ParseWavTags(const unsigned char* p, size_t n) {
    TagSet tags;
    if (n < 12 || std::memcmp(p, "RIFF", 4) != 0 ||
        std::memcmp(p + 8, "WAVE", 4) != 0)
        return tags;
    size_t off = 12;
    for (int guard = 0; off + 8 <= n && guard < 1024; ++guard) {
        const size_t csz = static_cast<size_t>(RdLE32(p + off + 4));
        const bool id3 =
            FoldAscii(static_cast<char>(p[off])) == 'i' &&
            FoldAscii(static_cast<char>(p[off + 1])) == 'd' &&
            FoldAscii(static_cast<char>(p[off + 2])) == '3';
        if (id3) {
            if (off + 8 + csz <= n) tags = ParseId3v2(p + off + 8, csz);
            break;  // 첫 id3 청크만(v1 계약)
        }
        off += 8 + csz + (csz & 1);  // 청크 8B 헤더+홀수 패드 규약
    }
    return tags;
}

// ---- 파일 접점 (D2 — 필요 범위만 read) ----

// 매직 분기(ID3/fLaC/OggS/RIFF — 첨자 무관·확장자 밀표 파일도 파싱) + mp3의
// ID3v1 폴백(태그 계열 — v2가 비면 말미 128B). 실패 = 빈 TagSet(폴백 규약
// — D2: 오류 표기 없이 파일명 폴백이 정상 경로), err는 진단 표기만(개방/
// 크기 실패 = 비공개 원문 "open"/"size"). 개방 = fs::path(2q 실측 수형 —
// MinGW libstdc++ narrow = UTF-8 strict, 네이티브 와이드 개방 = 인코딩
// 자유 — narrow fopen ACP 함정 회피).
inline TagSet ParseTag(const std::string& path, std::string& err) {
    TagSet tags;
    err.clear();
    std::ifstream f(std::filesystem::path(path), std::ios::binary);
    if (!f.is_open()) {
        err = "open";
        return tags;  // 빈 TagSet(폴백 계약)
    }
    f.seekg(0, std::ios::end);
    const std::streamoff total = f.tellg();
    if (total < 0) {
        err = "size";
        return tags;
    }
    std::streamsize headN =
        static_cast<std::streamsize>(total) <
                static_cast<std::streamsize>(kTagHeadCap)
            ? static_cast<std::streamsize>(total)
            : static_cast<std::streamsize>(kTagHeadCap);
    std::vector<unsigned char> head(static_cast<size_t>(headN));
    if (headN > 0) {
        f.seekg(0, std::ios::beg);
        f.read(reinterpret_cast<char*>(head.data()), headN);
        if (f.gcount() != headN) {  // 경합 절단 — 읽은 만큼만
            headN = f.gcount();
            head.resize(static_cast<size_t>(headN));
        }
    }
    bool isMp3 = false;
    const unsigned char* buf = head.data();
    if (headN >= 3 && std::memcmp(buf, "ID3", 3) == 0) {
        tags = ParseId3v2(buf, static_cast<size_t>(headN));
        isMp3 = true;
    } else if (headN >= 4 && std::memcmp(buf, "fLaC", 4) == 0) {
        tags = ParseFlacTags(buf, static_cast<size_t>(headN));
    } else if (headN >= 4 && std::memcmp(buf, "OggS", 4) == 0) {
        tags = ParseOggTags(buf, static_cast<size_t>(headN));
    } else if (headN >= 12 && std::memcmp(buf, "RIFF", 4) == 0 &&
               std::memcmp(buf + 8, "WAVE", 4) == 0) {
        tags = ParseWavTags(buf, static_cast<size_t>(headN));
    } else {
        // 매직 미일치 — 확장자 보조 분기(ID3v1 원존 mp3는 매직이 없다)
        std::string ext = std::filesystem::path(path).extension().string();
        isMp3 = jk::crt::Stricmp(ext.c_str(), ".mp3") == 0;
    }
    if (TagEmpty(tags) && isMp3 && total >= 128) {
        // v2→v1 폴백(태그 계열 — 스펙 §1 원문). 말미 128B만 seek+read.
        f.seekg(total - 128, std::ios::beg);
        unsigned char tail[128];
        f.read(reinterpret_cast<char*>(tail), 128);
        if (f.gcount() == 128) {
            const TagSet v1 = ParseId3v1(tail, 128);
            if (!TagEmpty(v1)) tags = v1;
        }
    }
    return tags;  // 부재 = 빈 TagSet(파일명 폴백 — 정상 경로, err 0)
}

} // namespace tags

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