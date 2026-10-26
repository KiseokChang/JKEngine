#ifndef MUSICMODEL_H
#define MUSICMODEL_H

// Music app model (spec 2026-10-09-music-library-design, T1): the pure parts
// of the music hub — source-dir resolver over settings.json "music.dirs",
// recursive audio-file enumeration (D3: 전체 트리 재귀 — 서브디렉터리 포함),
// filename scan metadata (D2: 파일명·크기·mtime — 태그 2단계는 #99 T2에서
// 접합: 스캔 1패스가 tags::ParseTag로 머리 영역만 읽는다), and the name
// filter (D5 확장 — 파일명+곡명+아티스트). Playback is delegated to vplayer
// (D1) — this header owns
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
// 엔트리 64개마다 보조 체크(단일 디렉터리가 수천 엔트리여도 취소에 응답 —
// #99 T2 fix r1: 256→64, 간격 상수만·2p 호출 수 계약 무변조).
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
#include <cerrno>      // errno EINTR — 태그 쓰기 ::write 분절(#111 T1)
#include <fcntl.h>     // ::open — 태그 쓰기 원자적 tmp(#111 T1 — D6 fsync leg)
#include <sys/stat.h>  // ::stat — (st_dev, st_ino) 방문 정체성 원문(fix r1)
#include <unistd.h>    // ::write/::fsync/::close/::getpid — 태그 쓰기 leg(#111 T1)
#endif

namespace jk {
namespace music {

// ---- 태그 구조 + 스캔 접점 전방 선언 (#99 T2 — 스캔 1패스 접합의 순서 계약) ----
// ScanAudioTree(#92 열거 leg)가 tags::ParseTag를 호출하므로(스펙 D2 — 같은
// 패스에서 머리 영역만 읽는다) 구조와 접점만 정의/선언을 열거 leg 앞으로
// 올린다. 파서 본체(ParseId3v2/ParseId3v1/ParseVorbisComments/ParseFlacTags/
// ParseOggTags/ParseWavTags+인코딩 정규화)는 파일 말단의 namespace tags 블록
// 원문 그대로 — 옮긴 것 없음(정의 위치만).
namespace tags {

// 구조 계약(브리프 원문 — 3필드 고정). TDRC/TRCK 프레임은 D1 표기에 있으나
// 싱크 필드가 없어(원장 부기) v1 스코프 밖 — 프레임 사슬에서 건너뛴다(2s ①의
// TRCK 포함 블롭 실측).
struct TagSet {
    std::string title, artist, album;
};

TagSet ParseTag(const std::string& path, std::string& err);

} // namespace tags

// user dirs 상한 — settings 폭주 절단(갤러리 원문 승계; 실사용은 1-3건이
// 현실).
inline constexpr int kMaxUserDirs = 64;

// 오디오 확장자 (스펙 결정: mp3/flac/wav/ogg/m4a/aac/wma + .opus/.m4b/.mp4
// — #115 T2, 스펙 D5 "확장자 스니프 정합" + T1 리뷰 r1 지시(라이브러리 행
// 통과). 트리 파싱은 무조건 매직 진실원 — 확장자는 열거 보조만. .mp4를
// 오디오로 판정하는 근거: 읽기 leg는 음악 메타 ilst만 보므로(비디오 사양
// 접촉 0) 매직 ftyp+ilst 'mdir' 합격을 오디오 행으로 취급 — 정직(비디오
// mp4는 보통 ilst 부재·비-'mdir' hdlr로 걸러진다). 재생 지원 별도(D6).
// 확장자 문자열는 ".mp3" 도트 포함 형태(fs::path::extension() 원문 형식)로
// 비교한다.
inline bool IsAudioExtension(const std::string& ext) {
    return jk::crt::Stricmp(ext.c_str(), ".mp3") == 0 ||
           jk::crt::Stricmp(ext.c_str(), ".flac") == 0 ||
           jk::crt::Stricmp(ext.c_str(), ".wav") == 0 ||
           jk::crt::Stricmp(ext.c_str(), ".ogg") == 0 ||
           jk::crt::Stricmp(ext.c_str(), ".opus") == 0 ||
           jk::crt::Stricmp(ext.c_str(), ".m4a") == 0 ||
           jk::crt::Stricmp(ext.c_str(), ".m4b") == 0 ||
           jk::crt::Stricmp(ext.c_str(), ".mp4") == 0 ||
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

// 스캔 행: full=절대(전체) 경로, rel=루트 기준 상대경로(슬래시 표기 — 재귀
// 모음에서 "어느 폴더의 파일인가" 표기 계약), name=말단 파일명, size/mtime=
// 스탬프(mtime은 last_write_time의 time_since_epoch().count() 정수열 — 갤러리
// ThumbStamp 원문 수형; 같은 기기 안에서 안정, 시계 epoch는 플랫폼 몫).
// #99 T2의 태그 필드는 D3(파일명 스캔 v1의 2단계 — 스펙 2026-10-11)로 아래
// 확장.
struct Track {
    std::string full;
    std::string rel;
    std::string name;
    long long size = 0;
    long long mtime = 0;
    // 태그 필드 (#99 T2 — D3): UTF-8 규약(#94 — 파서가 정규화). **원문 보존
    // 필드** — 태그 부재는 빈값 그대로(폴백 "-" 표기·title 파일명 폴백은 표기
    // leg가 소유 — 아래 RowLabel/AlbumLabel): 모델에 프레젠테이션 "-"를 심으면
    // 태그 유무의 진실원이 죽는다. 신설 필드는 구조 말단(위치 초기자 계열 소비
    // — 2m/2p 테스트 Track 기본 구성 무변조). 스캔 1패스(ScanAudioTree)가
    // tags::ParseTag로 채운다(+오버라이드 호출부 0 — 재능력 재실행 금지 계약).
    std::string title;   // 곡명 원문(부재 = 빈값 → 표기 폴백 = 파일명 확장자 제거)
    std::string artist;  // 아티스트 원문(부재 = 빈값 → 표기 폴백 = "-")
    std::string album;   // 앨범 원문(부재 = 빈값 → 표기 폴백 = "-")
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
// 청구된다(진입 1회 + 엔트리 kScanAuxEntryCheck개마다 보조 1회) — 경계당
// 최대 1회 원문(2p-② 호출 상한 단정).
using ScanCancelFn = std::function<bool()>;

// 보조 취소 체크 간격 — 상한 계약 1행(#99 T2 fix r1 — 리뷰 I-1): 취소에 대한
// 응답 지연 = 간격×(파일당 파싱 비용)의 최악 경로. 간격 64×파일당 앞 1MB
// read(kTagHeadCap — 실사용 태그는 수십 KB) — 메모리 대기열에서 64×~1MB
// read는 수십 ms급(저속 저장소에서도 경계 단위·전체 트리 스캔에 몰리지
// 않는다 — 태그 파싱이 같은 패스에 얹힌 T2 이후 간격이 취소 대기의 지배항).
// 래치(ListAudioFiles)가 해소 후 재청구를 막으므로 호출 수 상한(2p-②)은
// 간격 유무와 무관 — 간격을 줄여도 호출 수 계약은 무변조.
inline constexpr int kScanAuxEntryCheck = 64;

// Recursive scan worker — ListAudioFiles의 재귀 leg. ec 중립형(throwing
// 오버로드 금지 — 갤러리/shot 원문 계약): dir 열기 실패 = 독립 실패(빈 목록,
// 스펙 §2 "각 dir 독립 — 폴백 0건이어도 목록은 그린다"), 열거 중 소명 성분은
// 스킵. 하위 디렉터리 재귀는 순환 가드(fix r1) 이후에만.
//
// 취소(#93 T1): 진입 체크가 "자식 재귀 직전" 경계를 겸한다 — 호출자가
// 내려보내는 지점에서 callee 진입이 먼저 판정하므로 경계당 콜백 청구는
// 정확히 1회(2p-② 상한 원리). 취소 시 즉시 복귀(이 브랜치만 절단 — 형제와
// 상위의 나머지 열거는 계속되고, 최종 폐기는 ListAudioFiles 래치가 소관).
// 엔트리 kScanAuxEntryCheck(64)개마다 보조 체크 — 단일 디렉터리 폭주(수천
// 엔트리)에서도 취소에 응답한다(경계가 디렉터리마다뿐이면 폭주 leg를 끝까지
// 견딘다 — #99 T2 fix r1: T2 이후 같은 패스의 태그 파싱(파일당 앞 1MB read)
// 이 취소 응답 지연의 지배항이 되어 간격 64로 조정 — 간격 상수만·2p 호출 수
// 계약 무변조).
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
        if (++aux == kScanAuxEntryCheck) {  // 엔트리 64개마다 보조 체크(#93 T1)
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
        // 태그 접합 (#99 T2 — 스펙 D2 스캔 1패스): 같은 열거 패스에서 머리
        // 영역만 읽어 채운다(tags::ParseTag — kTagHeadCap 상한 + mp3 말미 128B
        // v1 폴백 seek = **헤더만 읽는 것 계약**, 전체 파일 열기 아님). 실패 =
        // 빈 TagSet(파일명 폴백 정상 경로 — D2: 오류 표기 없음), err는 스캔
        // 계약상 폐기(표기 leg가 없다). 접합은 이 지점 1곳 — 호출부 재실행
        // 금지 계약(재파싱 0). 취소 경계에 영향 없음(파싱은 콜백을 청구하지
        // 않는다 — 2p-② 호출 상한 무변조).
        std::string terr;
        const tags::TagSet tg = tags::ParseTag(tr.full, terr);
        tr.title = tg.title;
        tr.artist = tg.artist;
        tr.album = tg.album;
        out.push_back(std::move(tr));
    }
}

// Enumerate one root's audio files across the whole tree, newest first
// (selftest 2m-c — D3 재귀 + mtime desc 정렬). 확장자 필터는
// IsAudioExtension(mp3/flac/wav/ogg/opus/m4a/m4b/mp4/aac/wma, 대소문자
// 무시 — Stricmp).
// Sort tie(같은 mtime) = rel 오름차순 — 재귀 열거 순서가 축(Win/WSL)마다
// 다르게 흔들릴 수 있어 tie를 rel로 못 매 둔다(결정론 보존; 갤러리 원문의
// "동점 tiebreak" 자리 계약 승계 — 갤러리는 이름 desc, 유저 트랙 폴더에는
// 어휘 오름차순이 읽기 순서라 asc를 선택 — 구현 재량, 리포트 부기).
// 없는 루트/빈 루트 = 빈 목록(독립 스캔 실패 — ok 플래그 없이 0건으로 표현,
// 스펙 §2).
//
// 취소판(#93 T1): cancel 참이 되는 즉시(래치) 스캔을 절단하고 **부분 수집을
// 폐기한 빈 목록**을 반환한다(부분 결과 유출 금지 — 2p-①/② 원문). 콜백 청구
// 경계는 ScanAudioTree 원문(진입 + 엔트리 kScanAuxEntryCheck 보조) 그대로 —
// 래치는 해소 후 재청구를 막아 경계당 1회를 보장한다(2p-② 호출 상한).
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

// 행 필터 확장 (#99 T2 — 스펙 D5): 파일명 매치 **원존 유지**(위 수형)에 곡명
//·아티스트 매치를 얹는 1겹 오버로드 — OR 결합(열외 순서 보존 계약은 UI 몫).
// 앨범은 D5 스코프 밖(곡명/아티스트만 — 필터가 앨범까지 접으면 스코프 팽창).
// title은 빈값(태그 부재)이어도 원존 파일명 수형이 name을 이미 매치하므로
// 폴백 복합 없이 성립(2s-⑨가 string 수형 원문 재용 — 본 오버로드는 신설
// 어설션 없음 — 브리프 "Test: selftest 어설션 없음(2s 소유)").
inline bool MatchFilter(const Track& t, const std::string& filter) {
    return MatchFilter(t.name, filter) || MatchFilter(t.title, filter) ||
           MatchFilter(t.artist, filter);
}

// ---- 행 라벨 폴백 (T2 — 스펙 D3 규약의 표기 leg, 순수 부품) ----

// 곡명 라벨: 태그 title 있음 = "곡명 (아티스트)" · 부재 = 파일명(확장자 제거)
// — D3 폴백 규약. 아티스트 부재 행은 괄호를 생략한다(가독 우선 — D4 원문;
// "-" 폴백 표기는 단독 열 원존인 앨범이 소유 — 합성 열의 "(-)" 잡음 회피
// 재량, 리포트 부기). 확장자 제거 = fs::path::stem 원문 수형("가을 노래.mp3"
// → "가을 노래" — 이중 도트 파일명은 마지막 확장자만 제거 — fs 계약).
inline std::string TrackRowLabel(const Track& t) {
    std::string label = t.title.empty()
                            ? std::filesystem::path(t.name).stem().string()
                            : t.title;
    if (!t.artist.empty()) {
        label += " (";
        label += t.artist;
        label += ')';
    }
    return label;
}

// 앨범 라벨: 부재 = "-" 표기(D3 폴백 규약 — 단독 열 원존).
inline std::string TrackAlbumLabel(const Track& t) {
    return t.album.empty() ? std::string("-") : t.album;
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

// TagSet 구조와 ParseTag 선언(스캔 1패스 접점)은 namespace 머리(#99 T2)로
// 올라가 있다 — ScanAudioTree 열거 leg가 정의 앞에서 접점을 소비하는 순서
// 계약(헤더 단일 TU — 전부 inline, 링크 무변조). 파서 본체는 이 아래 원문
// 그대로(정의 위치만 그 자리).

// 스캔 1패스 접점이 읽는 머리 영역 상한(D2 — "필요 범위만 read"). FLAC 메타
// 사슬·ID3v2 태그(커버 아트 포함)는 통상 수십 KB — 1MB 컷은 실사용 여유이고
// 폭주(대형 사진 프레임)를 절단한다(잠복: 사진이 코멘트 앞인 드문 정렬은
// 태그 누락 — 리포트 부기).
inline constexpr size_t kTagHeadCap = 1u << 20;

// moov 후행 2차 read의 창 상한(#115 원장행 — 2026-10-27 batch2 실환 소각).
// 근거: ffmpeg 기본 mux m4a = mdat 선두+moov 말미 — moov가 머리 kTagHeadCap
// 밖(일상 형태)이라 구 ParseTag는 "-" 폴백. 수형 = 첫 mdat 끝 오프부터 이
// 창만 seek+read해서 box 보행 재용. moov(udta/meta/ilst+태그)는 수십 KB
// 규모라 2MB는 여유 컷 — 창 밖 moov(거대 커버 포함 등) = 기존 폴백 그대로
// (정직 거부 — 부분 태그 0 계약 승계).
inline constexpr size_t kMoovLocateMax = 2u << 20;

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

// ---- opus — OggS 연쇄의 OpusTags 패킷 (own — #115 T2, 스펙 D1/D3) ----
//
// opus 컨테이너 = vorbis와 같은 Ogg 페이지 연쇄지만 패킷 매직이 다르다:
// 패킷 1 = "OpusHead"(식별 패킷 — 태그 계열 부재, 매직 미일치로 자연
// 스킵), 패킷 2 = "OpusTags" + 그 뒤가 vorbis 코멘트 구조 그대로(마커 바이트
// 없는 8바이트 매직 — vorbis의 \x03vorbis 7B와 동형 접점). 코멘트 해석은
// ParseVorbisComments를 그대로 재용한다(D1 조달 0).
//
// 패킷 지목 = lacing 인지 보행("OggS" 27B 헤더+세그 표·255 = 패킷 연속 규약
// — OpusTags 매직을 패킷 경계에서만 본다 — 오디오 패킷 내 우연 일치를
// 매기지 않는 정직 판정). 잘린 페이지 = 사슬 절단(발견 실패 = 빈 폴백 —
// 무태그 정상 경로).
inline bool OpusTagsPacket(const unsigned char* p, size_t n,
                           std::vector<unsigned char>& comments) {
    comments.clear();
    if (n < 4 || std::memcmp(p, "OggS", 4) != 0) return false;  // OggS 스니프
    std::vector<unsigned char> pay;    // 페이지 페이로드 연접(머리 영역 한정)
    std::vector<size_t> lace;          // 세그(lacing 값) 열 — 페이지 경계 무관
    size_t off = 0;
    size_t guard = 0;
    while (off + 27 <= n && guard++ < 4096) {
        if (std::memcmp(p + off, "OggS", 4) != 0) break;  // 사슬 절단
        const size_t segs = p[off + 26];
        if (off + 27 + segs > n) break;  // 잘린 헤더
        size_t plen = 0;
        for (size_t s = 0; s < segs; ++s) plen += p[off + 27 + s];
        if (off + 27 + segs + plen > n) break;  // 잘린 페이로드
        pay.insert(pay.end(), p + off + 27 + segs,
                   p + off + 27 + segs + plen);
        for (size_t s = 0; s < segs; ++s) lace.push_back(p[off + 27 + s]);
        off += 27 + segs + plen;
    }
    // lacing 연접 위에서 패킷 완성 지점마다 매직 판정(255 = 연속 — 원문
    // 규약; 마지막 세그먼트 255 런의 미종결 패킷 = 파산 절단).
    size_t i = 0, at = 0;
    while (i < lace.size()) {
        size_t len = 0;
        bool done = false;
        while (i < lace.size() && !done) {
            len += lace[i++];
            done = (lace[i - 1] != 255);
        }
        if (!done || at + len > pay.size()) break;  // 미종결/잘린 패킷
        if (len >= 8 &&
            std::memcmp(pay.data() + at, "OpusTags", 8) == 0) {
            comments.assign(pay.data() + at + 8, pay.data() + at + len);
            return true;
        }
        at += len;
    }
    return false;  // OpusTags 부재(무태그 opus — 정상 경로)
}

inline TagSet ParseOpusTags(const unsigned char* p, size_t n) {
    TagSet tags;
    std::vector<unsigned char> comments;
    if (!OpusTagsPacket(p, n, comments)) return tags;
    return ParseVorbisComments(comments.data(), comments.size());
}

// OggS 연쇄의 첫 패킷 매직 스니프 — vorbis(vorbis)와 opus("OpusHead")의
// 구별 접점(ParseTag·ReadCover 분기). 식별 패킷은 항상 첫 페이지 페이로드
// 선두(27B 헤더+세그 표 뒤)에서 개시한다 — 정적 컷 290B(27 헤더+255 세그
// 표+8B 매직 이상 — 식별 패킷은 19B라 실측 여유 큼). 잘린 헤더 = false
// (미판정 — vorbis leg 착지, 무해).
inline bool OggPacket1IsOpus(const unsigned char* p, size_t n) {
    if (n < 35) return false;  // 27B 헤더+세그 표 최소+8B 매직
    const size_t segs = p[26];
    if (27 + segs + 8 > n) return false;
    return std::memcmp(p + 27 + segs, "OpusHead", 8) == 0;
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

// ---- MP4/ISO-BMFF box 보행 (own 파서 — m4a 계열 태그, #115 T1) ----

// MP4 박스 = [4B size(BE)][4B type] + payload. size=1 = 뒤 8B largesize(64BE
// — 원문 규약)·size=0 = 컨테이너 말미까지(마지막 박스 규약). 잘린/배신적
// size(선언 < 헤더·선언 > 남은 영역) = false — 선언을 믿지 않는 정직 거부
// (부분 태그 0 — 2x-③ 계약). off는 다음 박스 선두로 전진(엄밀 증가 — 폭주
// 루프는 구조적으로 불가하다).
struct Mp4BoxWalk {
    char type[4];
    size_t body;   // payload 선두
    size_t bodyN;  // payload 길이
};
inline bool Mp4BoxAt(const unsigned char* p, size_t n, size_t& off,
                     Mp4BoxWalk& b) {
    if (off + 8 > n) return false;
    std::memcpy(b.type, p + off + 4, 4);
    unsigned long long total = RdBE32(p + off);
    size_t head = 8;
    if (total == 1) {  // 64bit largesize — size=1 규약
        if (off + 16 > n) return false;
        total = 0;
        for (int i = 0; i < 8; ++i)
            total = (total << 8) | p[off + 8 + i];
        head = 16;
    }
    size_t end;
    if (total == 0) {
        end = n;  // size=0 — 남은 영역 말미(마지막 박스)
    } else {
        if (total < head) return false;  // 배신적 size(선언 < 헤더)
        end = off + static_cast<size_t>(total);
    }
    if (end > n) return false;  // 잘린 box — 정직 거부
    b.body = off + head;
    b.bodyN = end - off - head;
    off = end;
    return true;
}

// 형제 박스 사슬에서 type 일치(원문 바이트 — 박스 type은 대소 규약 없음)
// 첫 박스의 payload를 돌려준다. 파산(잘린 box) = 전체 정직 거부와 같은
// false — 부재(무태그 m4a의 정상 경로)와 같은 폴백 착지다(부분 필드 0).
// 보행 상한 4096(폭주 절단 — FLAC 블록 256/Ogg 페이지 표 255 같은 계보).
inline bool Mp4FindBox(const unsigned char* p, size_t base, size_t span,
                       const char* type4, size_t& body, size_t& bodyN) {
    size_t off = base;
    const size_t end = base + span;
    Mp4BoxWalk b;
    size_t guard = 0;
    while (off + 8 <= end && guard++ < 4096) {
        if (!Mp4BoxAt(p, end, off, b)) return false;  // 파산 — 정직 거부
        if (std::memcmp(b.type, type4, 4) == 0) {
            body = b.body;
            bodyN = b.bodyN;
            return true;
        }
    }
    return false;  // 부재 — 보행 소진(정상 경로)
}

// 첫 mdat 박스의 끝(다음 상자 선두 절대 오프) — moov 후행 2차 read의 창
// 기점(#115 원장행 수형). 머리 버퍼에 mdat 헤더(8B)만 실려도 선언 size를
// 진실원으로 쓴다(mdat가 머리 cap을 넘는 것이 이 leg의 전제 — 실측 잘림
// 과는 무관). size=0(EOF까지) = 뒤에 상자가 없다는 규약 → false, largesize
// (size=1, 64BE) 재용, mdat 도달 전 보행 파산(잘린/배신적 size·오프 오버
// 플로) = false — Mp4BoxAt의 정직 거부 계약 승계(선언을 믿지 않는다 —
// 대신 여기서는 "mdat 끝"만 선언에서 읽고 창 유효성은 호출부가 파일 크기와
// 재판정한다). 보행 상한 64(ftyp/free 등 머리 형제 몇 개 — 폭주 절단).
inline bool Mp4MdatNext(const unsigned char* p, size_t n, size_t& nextOff) {
    size_t off = 0;
    size_t guard = 0;
    while (off + 8 <= n && guard++ < 64) {
        char type[4];
        std::memcpy(type, p + off + 4, 4);
        unsigned long long total = RdBE32(p + off);
        size_t head = 8;
        if (total == 1) {  // 64bit largesize — size=1 규약(Mp4BoxAt 원문)
            if (off + 16 > n) return false;
            total = 0;
            for (int i = 0; i < 8; ++i)
                total = (total << 8) | p[off + 8 + i];
            head = 16;
        }
        if (total == 0) return false;  // EOF까지 mdat — 뒤 상자 없음
        const unsigned long long next =
            static_cast<unsigned long long>(off) + total;
        if (next > 0xffffffffull) return false;  // 32bit 크기계계 밖 — 거부
        if (std::memcmp(type, "mdat", 4) == 0) {
            nextOff = static_cast<size_t>(next);
            return true;
        }
        if (next > n) return false;  // 머리 밖 형제 보행 불가 — 정직 거부
        off = static_cast<size_t>(next);
    }
    return false;  // mdat 부재/보행 소진 — 정직 거부
}

// ilst 항목의 data atom 본문 → UTF-8 ([4B version/flags][4B locale][값] —
// 타입 코드 = flags 하위 바이트: 1=UTF-8 통과·2=UTF-16(BOM 우선 —
// DecodeId3Text enc 1 원문 leg 재용: FE FF=BE·FF FE=LE·무BOM = LE — #99
// 인코딩 계약 승계)). 0=binary(covr 등)/기타 코드 = 빈 값 스킵(텍스트 오독
// 금지 — 정직).
inline std::string DecodeM4aValue(const unsigned char* p, size_t n) {
    if (n < 8) return std::string();
    const unsigned int code = p[3];
    if (code == 1)
        return PostprocessText(
            std::string(reinterpret_cast<const char*>(p + 8), n - 8));
    if (code == 2) {
        // Apple data 타입 2 = UTF-16BE 명시 — 무BOM = BE(톤 리뷰 r1 수형:
        // 원본 leg는 enc 1 고정이라 무BOM UTF-16을 LE로 오독했다). BOM 존재
        // = BOM이 이긴다(BOM 우선 leg — FE FF=BE·FF FE=LE·DecodeId3Text
        // enc 1 재용 — #99 인코딩 계약 승계).
        const unsigned char* q = p + 8;
        const size_t v = n - 8;
        const bool bom = v >= 2 && ((q[0] == 0xFE && q[1] == 0xFF) ||
                                    (q[0] == 0xFF && q[1] == 0xFE));
        return PostprocessText(DecodeId3Text(q, v, bom ? 1 : 2));
    }
    return std::string();
}

// ilst 지목 공용 보행(ftyp→moov→udta→meta(hdlr 'mdir' 확인)→ilst) — 태그
// ParseM4aTags와 커버 ParseMp4Covr가 같은 트리 보행을 소비한다(#115 T1/T2
// 사슬 단일 출구). moov는 보통 머리에 있으나 mdat 뒤에도 가능 — 보행은
// 호출부가 읽은 머리 영역 전체를 건다(태그 = kTagHeadCap·커버 = kTagWriteMax).
// moov 후행(머리 밖)·파산·hdlr 부재·비-'mdir' 핸들러 = false(정직 거부 —
// 부분 필드 0·2x-③ 계약).
//
// T3(쓰기 leg) 확장 — 지목 실패의 단계를 돌려주는 판정으로 원문을 뽑아냈다:
// 쓰기 leg의 신설 폴백(부재 부모 box = udta/meta/ilst 순차 신설 — D2)이 어느
// 단계에서 신설해야 하는지 계보가 필요하다. 읽기 계약(전 단계 = 동일 false)
// 은 래퍼 Mp4FindIlst 원문 유지 — 거동 불변(2x ①②③ 회귀 원존·T3 스폰 0).
enum Mp4TagStage {
    kMp4TagOk = 0,    // 전 트리 성립 — ilst 재작성(in place)
    kMp4TagNoMoov,    // moov 부재 — 신설 불가(트리 루트 자의 신설 봉인 —
                      // 재생 트리 파괴 위험 fail-closed)
    kMp4TagNoUdta,    // udta 부재 — moov children 말미 신설
    kMp4TagNoMeta,    // meta 부재 — udta children 말미 신설
    kMp4TagNoHdlr,    // hdlr 부재 — meta children 말미에 hdlr(mdir)+ilst
                      // 동봉 신설
    kMp4TagBadHdlr,   // hdlr 성립하나 비-'mdir'('mdta' 등 타 핸들러 표기 —
                      // 재기입 봉인 — 타 핸들러 의미 침해 금지 fail-closed)
    kMp4TagNoIlst     // hdlr 'mdir' 성립·ilst 부재 — meta children 말미 신설
};

// Mp4LocateIlst의 지목 결과 — 쓰기 leg의 재작성 좌표(모두 p 기준 offset
// 진실원 — 자식 box payload 선두). ilst 좌표 = reader 계약 원존.
struct Mp4TagLoc {
    size_t udtaBody = 0, udtaN = 0;
    size_t metaBody = 0, metaN = 0;
    size_t ilstBody = 0, ilstN = 0;
};

// moov payload(span = moovBody..+moovN — p 절대 좌표)의 태그 트리 지목 —
// 쓰기 leg가 moov 본문 버퍼(mb)에서 직접 소비한다(ftyp 스니프 없는 코어 —
// 버퍼 = moov payload라 ftyp가 없다).
inline Mp4TagStage Mp4LocateTagTree(const unsigned char* p, size_t moovBody,
                                    size_t moovN, Mp4TagLoc& out) {
    size_t body = 0, bodyN = 0;
    if (!Mp4FindBox(p, moovBody, moovN, "udta", body, bodyN))
        return kMp4TagNoUdta;
    out.udtaBody = body;
    out.udtaN = bodyN;
    if (!Mp4FindBox(p, body, bodyN, "meta", body, bodyN))
        return kMp4TagNoMeta;
    out.metaBody = body;
    out.metaN = bodyN;
    // meta = full box — payload 머리 4B version/flags, 그 뒤 자식 박스
    // (hdlr·ilst).
    if (bodyN < 4) return kMp4TagNoMeta;
    body += 4;
    bodyN -= 4;
    // hdlr 'mdir' 확인 — handler_type = hdlr payload [8..12)(version/flags
    // 4B + pre_defined 4B 뒤). 비-metadata ilst(moov 밖의 meta 등) 걷기 봉인.
    size_t hBody = 0, hN = 0;
    if (!Mp4FindBox(p, body, bodyN, "hdlr", hBody, hN))
        return kMp4TagNoHdlr;
    if (hN < 12 || std::memcmp(p + hBody + 8, "mdir", 4) != 0)
        return kMp4TagBadHdlr;
    if (!Mp4FindBox(p, body, bodyN, "ilst", body, bodyN))
        return kMp4TagNoIlst;
    out.ilstBody = body;
    out.ilstN = bodyN;
    return kMp4TagOk;
}

// 파일 머리 버퍼(p = ftyp 이후 전 트리)의 ilst 지목 — 읽기 계약 원존(전 단계
// = 동일 false).
inline Mp4TagStage Mp4LocateIlst(const unsigned char* p, size_t n,
                                 Mp4TagLoc& out) {
    if (n < 8 || std::memcmp(p + 4, "ftyp", 4) != 0)
        return kMp4TagNoMoov;  // ftyp 스니프 — 컨테이너 매직(확장자 진실원과
                               // 독립)·ftyp 부재 = 트리 미성립
    size_t moovBody = 0, moovN = 0;
    if (!Mp4FindBox(p, 0, n, "moov", moovBody, moovN)) return kMp4TagNoMoov;
    return Mp4LocateTagTree(p, moovBody, moovN, out);
}
inline bool Mp4FindIlst(const unsigned char* p, size_t n, size_t& body,
                        size_t& bodyN) {
    Mp4TagLoc loc;
    if (Mp4LocateIlst(p, n, loc) != kMp4TagOk) return false;
    body = loc.ilstBody;
    bodyN = loc.ilstN;
    return true;
}

// ilst 본문(p 상대 좌표)의 항목 보행 — 태그 3필드 추출 공용 후반부. ParseTag
// (머리 창)와 moov 후행 2차 read(뒤 창)가 이 보행 한 곳을 소비한다(계보
// 단일 출구 — #115 원장행 수형의 등호 전제). 항목 = 본문의 첫 data atom
// (첫 값 계약 — v1 계보). 파산(잘린/배신적 box) = 빈 TagSet(정직 거부 —
// 부분 필드 소각·2x-③ 계약).
inline TagSet ParseM4aIlstItems(const unsigned char* p, size_t body,
                                size_t bodyN) {
    TagSet tags;
    // ilst 자식 연쇄 — 항목 atom(type = ©nam/©ART/©alb). 미지목 항목(covr/
    // trkn/tmpo…) = 통과만(본문은 불투명 — data 존재 요구 금지).
    size_t off = body;
    const size_t end = body + bodyN;
    Mp4BoxWalk item;
    size_t guard = 0;
    while (off + 8 <= end && guard++ < 4096) {
        if (!Mp4BoxAt(p, end, off, item)) return TagSet();  // 파산 — 부분 소각
        const int field =
            std::memcmp(item.type, "\xA9" "nam", 4) == 0
                ? 1
                : (std::memcmp(item.type, "\xA9" "ART", 4) == 0
                       ? 2
                       : (std::memcmp(item.type, "\xA9" "alb", 4) == 0 ? 3
                                                                       : 0));
        if (field != 0) {
            size_t dBody = 0, dN = 0;
            if (!Mp4FindBox(p, item.body, item.bodyN, "data", dBody, dN))
                return TagSet();  // 항목 data 파산 — 정직 거부(부분 소각)
            const std::string value = DecodeM4aValue(p + dBody, dN);
            if (field == 1) {
                if (tags.title.empty()) tags.title = value;
            } else if (field == 2) {
                if (tags.artist.empty()) tags.artist = value;
            } else {
                if (tags.album.empty()) tags.album = value;
            }
        }
    }
    return tags;
}

// ftyp→moov→udta→meta('mdir')→ilst→©nam/©ART/©alb. 아무 파산(잘린/배신적
// box·hdlr 부재·비-'mdir' 핸들러) = 빈 TagSet(정직 거부 — 부분 필드 소각·
// 2x-③ 계약). moov가 창 밖(kMp4TagNoMoov)이면 후행 2차 read가 이어준다 —
// ParseTag의 moov 후행 leg(#115 원장행).
inline TagSet ParseM4aTags(const unsigned char* p, size_t n) {
    size_t body = 0, bodyN = 0;
    if (!Mp4FindIlst(p, n, body, bodyN)) return TagSet();
    return ParseM4aIlstItems(p, body, bodyN);
}

// ---- m4a covr atom (own — ReadCover leg 부품, #115 T2 — 스펙 D4) ----
//
// ilst의 covr 항목(data atom 연쇄) → 그림 바이트+mime. data 타입 코드:
// 13=JPEG·14=PNG(iTunes 규약 — mime flag 계약 원문), 0 = binary 암규약
// 일부 태거의 표기 — 이미지 매agic 스니프(FF D8 FF=JPEG·89 "PNG"=PNG,
// ParseTag 원문 진실원과 동형). 그 외 코드 = 다음 data 후보(텍스트 오독
// 금지 — DecodeM4aValue 원문 정직 계약 승계). 항목 data 파산(잘린 box) =
// false — 커버 부재(무커버 정상 경로)와 같은 거부 착지지만 트리 파산은
// 후보 통과 금지(부분 그림 0 — "잘린 그림 = 거부" 계약).
inline bool ParseMp4Covr(const unsigned char* p, size_t n, std::string& mime,
                         std::vector<unsigned char>& bytes) {
    mime.clear();
    bytes.clear();
    if (n < 8 || std::memcmp(p + 4, "ftyp", 4) != 0) return false;
    size_t body = 0, bodyN = 0;
    if (!Mp4FindIlst(p, n, body, bodyN)) return false;
    size_t off = body;
    const size_t end = body + bodyN;
    Mp4BoxWalk item;
    size_t guard = 0;
    while (off + 8 <= end && guard++ < 4096) {
        if (!Mp4BoxAt(p, end, off, item)) return false;  // 트리 파산 — 정직
        if (std::memcmp(item.type, "covr", 4) != 0) continue;  // 항목 통과만
        // covr 자식 연쇄 — data atom(첫 유효 그림). 미지 자식 = 통과만.
        size_t dOff = item.body;
        const size_t dEnd = item.body + item.bodyN;
        Mp4BoxWalk d;
        size_t dGuard = 0;
        while (dOff + 8 <= dEnd && dGuard++ < 4096) {
            if (!Mp4BoxAt(p, dEnd, dOff, d)) return false;  // 파산 — 정직
            if (std::memcmp(d.type, "data", 4) != 0) continue;
            if (d.bodyN < 8) return false;  // 잘린 data 헤더 — 거부
            const unsigned int code = p[d.body + 3];
            const unsigned char* img = p + d.body + 8;
            const size_t imgN = d.bodyN - 8;
            const auto jpg =
                imgN >= 3 && img[0] == 0xFF && img[1] == 0xD8 && img[2] == 0xFF;
            const auto png =
                imgN >= 8 && std::memcmp(img, "\x89PNG\r\n\x1a\n", 8) == 0;
            if (code == 13 && jpg)
                mime = "image/jpeg";  // iTunes 규약 코드 13
            else if (code == 14 && png)
                mime = "image/png";  // iTunes 규약 코드 14
            else if (code == 0 && (jpg || png))
                mime = jpg ? "image/jpeg" : "image/png";  // binary 암규약
            else
                continue;  // 코드/스니프 불일치 — 다음 data 후보
            if (imgN == 0) continue;  // 빈 그림 — 거부(호출부 no_cover 계약)
            bytes.assign(img, img + imgN);
            return true;
        }
    }
    return false;  // covr 부재 — no_cover(정상 경로)
}

// ---- 파일 접점 (D2 — 필요 범위만 read) ----

// 매직 분기(ID3/fLaC/OggS/RIFF/ftyp — 첨자 무관·확장자 밀표 파일도 파싱) +
// mp3의 ID3v1 폴백(태그 계열 — v2가 비면 말미 128B). 실패 = 빈 TagSet(폴백 규약
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
        // Ogg 컨테이너 vorbis/opus 구별 — 첫 패킷 매agic("OpusHead" = opus
        // — #115 T2). vorbis 기존 leg 원존 무접촉. opus 파산/부재 = 빈
        // TagSet 폴백(무태그 정상 경로 — Ogg leg 원문 계약 음성).
        if (OggPacket1IsOpus(buf, static_cast<size_t>(headN)))
            tags = ParseOpusTags(buf, static_cast<size_t>(headN));
        else
            tags = ParseOggTags(buf, static_cast<size_t>(headN));
    } else if (headN >= 12 && std::memcmp(buf, "RIFF", 4) == 0 &&
               std::memcmp(buf + 8, "WAVE", 4) == 0) {
        tags = ParseWavTags(buf, static_cast<size_t>(headN));
    } else if (headN >= 8 && std::memcmp(buf + 4, "ftyp", 4) == 0) {
        // MP4/ISO-BMFF(m4a 계열 — 박스 트리 보행 — #115 T1). ftyp가 첫
        // 박스 = 컨테이너 매agic 진실원 — 확장자 밀표 파일도 같은 파싱.
        tags = ParseM4aTags(buf, static_cast<size_t>(headN));
        // moov 후행 2차 read(#115 원장행 — 실환 흔한 형태 소각 —
        // kMoovLocateMax 근거 주석): ffmpeg 기본 mux m4a = mdat 선두+moov
        // 말미 — moov가 머리 cap 밖 = 구 ParseTag "-" 폴백. 수형: ①머리
        // 스니프로 moov 미성립(kMp4TagNoMoov 한정 — udta/meta 깊이 미성립
        // 은 이미 머리 트리 판정 소관) 확정 ②머리 상자 연쇄에서 첫 mdat
        // 끝 오프(선언 size 진실원 — Mp4MdatNext) ③그 오프부터
        // kMoovLocateMax 창 seek+read ④창 보행 = moov 지목→트리·ilst 항목
        // 보행 재용 → 태그 승계. 창 밖 moov·창 파산 = 빈 TagSet 폴백 그대
        // 로(정직 — 부분 태그 0·2x-③ 계약 승계).
        Mp4TagLoc loc;
        if (TagEmpty(tags) &&
            Mp4LocateIlst(buf, static_cast<size_t>(headN), loc) ==
                kMp4TagNoMoov) {
            size_t mdatNext = 0;
            if (Mp4MdatNext(buf, static_cast<size_t>(headN), mdatNext) &&
                mdatNext > 8 &&
                static_cast<unsigned long long>(mdatNext) <
                    static_cast<unsigned long long>(total)) {
                std::streamoff span =
                    static_cast<std::streamoff>(kMoovLocateMax);
                const std::streamoff rest =
                    static_cast<std::streamoff>(total) -
                    static_cast<std::streamoff>(mdatNext);
                if (rest < span) span = rest;
                std::vector<unsigned char> tail(static_cast<size_t>(span));
                f.clear();
                f.seekg(static_cast<std::streamoff>(mdatNext),
                        std::ios::beg);
                f.read(reinterpret_cast<char*>(tail.data()), span);
                tail.resize(static_cast<size_t>(f.gcount()));
                // 창 보행 — 창 상대 좌표(보행 부품은 버퍼 밖을 모른다).
                // hdlr 'mdir' 트리 계약 동형 — 비-'mdir' = 폴백 그대로.
                size_t moovBody = 0, moovN = 0;
                Mp4TagLoc late;
                if (tail.size() >= 8 &&
                    Mp4FindBox(tail.data(), 0, tail.size(), "moov",
                               moovBody, moovN) &&
                    Mp4LocateTagTree(tail.data(), moovBody, moovN, late) ==
                        kMp4TagOk) {
                    tags = ParseM4aIlstItems(tail.data(), late.ilstBody,
                                             late.ilstN);
                }
            }
        }
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

// ---- 태그 쓰기 + 커버아트 (T1 mp3 leg·T2 FLAC/Ogg/WAV leg·T3 opus/m4a leg
// — 스펙
// 2026-10-25-music-tagwrite-design) ----
//
// 읽기 사슬(위의 ParseId3v2/ParseTag·#99)의 역방향 부품. D4 조달 0 승계 —
// own 쓰기 부품(third_party 메타 API는 읽기 사슬 원문 주석의 실측 그대로 재용
// 불가). T1 = mp3 leg(ID3v2.3 정규화+APIC), T2 = FLAC/Ogg/WAV leg와 ReadCover
// 전형식(아래 T2 공용 부품 그룹). 계약 시그니처(CoverArt·ReadCover·
// WriteTag)는 이후 태스크 전부가 소비하는 확정 원문 — 바꾸면 사슬 전체가
// 다시 검토된다.
//
// D5 정규화(mp3): 쓰기는 ID3v2.3으로 통일(폰 호환성 — 일부 폰 플레이어
// v2.4 미지원). 기존 태그 유지 계약: v2 존재 = 정규화 재작성(TIT2/TPE1/
// TALB/APIC 교체 + TCON 복제(장르 보존) + **그 외 기존 프레임은 본문 원문
// 바이트 그대로** 복사 보존 — 해석 불가 프레임 drop 금지)·v2 부재 = 신설
// (패딩 1KB)·v1 존재 = v1 유지(말미 128B는 [태그끝, EOF) 스트림 복사에
// 원문 흡수 — 별도 접촉 0).
//
// 크기 필드 계약(모호성 해소 원문 + 읽기 사슬 round-trip 계약이 함께
// 결정): 프레임 헤더 10B의 크기 = **32bit BE(syncsafe 아님 — v2.3 규약)**
// — v2.4 syncsafe 크기는 재기입에서 BE32로 역변환한다. 태그 헤더의 크기
// 필드(패딩 포합)도 32bit폭이지만 syncsafe 인코딩으로 기입한다 — 읽기
// 사슬(ParseId3v2 — SyncSafe32 원문)과 round-trip이 성립하는 유일 형식이고
// (plain BE32는 바이트 하나라 0x80 이상이 되는 128바이트 이상 태그에서
// 읽기 원문과 어긋난다 — 2u-① round-trip 봉인), ID3v2.3 규약 원문
// ("most significant bit in every byte is always set to zero")와도 같다.
// v2.4 syncsafe로의 재작성 금지(D5 정규화 — 출력은 항상 v2.3).
//
// 커버 계약: cover=null = 기존 APIC 유지 — 브리프 원문("ReadCover 재용 후
// 그대로 재삽입")은 지향을 말하고, 구현 leg로 APIC 프레임 **원문 바이트
// 보존**을 택했다(재합성 대비 인코딩 바이트까지 그대로 — 기존 APIC 유지
// 계약의 더 엄격 충족, 2u-⑥ 봉인). APIC 재기입(새 커버) = 인코딩 0
// (mime/desc Latin-1)+mime 정규화 "image/jpeg"/"image/png".
//
// 텍스트 인코딩 = 1 (UTF-16LE + 0xFF 0xFE BOM) 고정 — v2.3 규약이 정의하는
// 유니코드 인코딩이 1뿐(인코딩 3 UTF-8은 v2.4 전용 — D5 정규화가 금한다).
// 빈 필드 = 프레임 생략(사슬 재구성이 기존 프레임을 교체하므로 빈값 = 비움
// 쓰기와 등치 — 2u-① 봉인).
//
// D6 원자적 쓰기: 동일 dir `.<pid>.tagtmp` 완성본 → **fsync**(Win
// FlushFileBuffers / posix ::fsync — std::ofstream은 fd 접점이 없어 fsync가
// 성립하지 않아 네이티브 저수준 쓰기 leg: Win = CreateFileW 선례 수형
// kernel32 단일 선언, posix = ::open) → fs::rename(MusicDirStore
// WriteSettingsAtomic 선례 승계 — 1세대 교체 rename, 실패만 .bak 사다리+
// 복원). 실패 = 정직 거부+tmp 소각+원본 무손상. 원본 ifstream은 rename 전에
// 닫는다(Windows에서 대상 파일이 열려 있으면 MoveFileEx 교체가 거부될 수
// 있다 — 순서 계약).
//
// 스니프(쓰기·커버 공용): 선단 매직이 진실원, 확장자는 보조 분기만("fLaC
// 파일이 .mp3로 불리는" 역포장도 매직가 이긴다). "ID3" = mp3 v2 leg,
// fLaC/OggS/RIFF-WAVE = T2 leg(각 형식별 구조 파산 = 정직 거부), 매직 없음 = .mp3 확장자만 mp3(v1-raw
// leg — 신설), 그 외 = "unsupported_format". 0바이트 = "empty" 정직 거부
// (원본 무변경 자명).

// 쓰기/커버 leg의 태그 상한 — kTagHeadCap(스캔 1패스 상한·D7 "스캔 1패스
// 원존 무변조"와 무접촉)와 별개 재량 상수: 쓰기 leg는 편집 지점 1곳 파일만
// 다루므로(전체 트리 스캔 아님) 커버 프레임 포함 태그 전체를 버퍼에 두는
// 폭주 절단 16MiB. 초과 = 정직 거부("tag too large" — 실사용 커버는 수백
// KB~수 MB).
inline constexpr size_t kTagWriteMax = 16u << 20u;

// 커버아트 꾸러미 — mime = 정규화 표기("image/jpeg"/"image/png"), bytes =
// 인코딩된 이미지 원문 스트림(디코드는 표기 leg — T2/stb_image 승계).
struct CoverArt {
    std::string mime;
    std::vector<unsigned char> bytes;
};

// pid 접근(원자적 tmp 접미 — MusicDirStore store::ThisProcessId 선례 수형).
// store는 MusicModel.h를 include하므로 역방향 include로 못 끌어온다
// (단방향 include 계약) — 같은 kernel32 선례 선언의 tags 쪽 복제
// (MusicDirStore.h 원문 부기 승계, 시그니처 동일 — 중복 선언 compatible).
#if defined(_WIN32)
extern "C" __declspec(dllimport) unsigned long __stdcall
    GetCurrentProcessId(void);
inline unsigned long ThisProcessId() { return GetCurrentProcessId(); }
#else
inline unsigned long ThisProcessId() { return static_cast<unsigned long>(::getpid()); }
#endif

// ---- 인코딩 역방향 부품 (읽기 DecodeId3Text/Utf16BytesToUtf8의 쌍둥이) ----

// UTF-16 리틀 유닛 1개 출력(LE).
inline void AppendUtf16LeUnit(std::vector<unsigned char>& out, unsigned int u) {
    out.push_back(static_cast<unsigned char>(u & 0xFF));
    out.push_back(static_cast<unsigned char>((u >> 8) & 0xFF));
}

// UTF-8(#94 규약) → UTF-16 유닛열(LE·서로게 포함 — BOM/종결은 호출부 계약).
// 느슨 규약(읽기 Utf16BytesToUtf8 원문 수형): 비정형 선단 바이트와 잘린
// 연속은 Latin-1 유닛으로 통과(0x00-0xFF 한 유닛 — 데이터 전멸 회피).
inline void Utf8ToUtf16Le(const std::string& s, std::vector<unsigned char>& out) {
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {  // ASCII — 유닛 1개
            AppendUtf16LeUnit(out, c);
            ++i;
            continue;
        }
        unsigned int cp = 0;
        size_t len = 0;
        if (c >= 0xC2 && c <= 0xDF) {        // 2바이트(0xC0/0xC1 overlong 배제)
            len = 2; cp = c & 0x1F;
        } else if (c >= 0xE0 && c <= 0xEF) { // 3바이트
            len = 3; cp = c & 0x0F;
        } else if (c >= 0xF0 && c <= 0xF4) { // 4바이트(≤U+10FFFF)
            len = 4; cp = c & 0x07;
        } else {  // 비정형 선단(0xC0/0xC1/0xF5-0xFF) — Latin-1 유닛 통과
            AppendUtf16LeUnit(out, c);
            ++i;
            continue;
        }
        bool ok = i + len <= s.size();
        for (size_t k = 1; ok && k < len; ++k) {
            const unsigned char cc = static_cast<unsigned char>(s[i + k]);
            if ((cc & 0xC0) != 0x80) { ok = false; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok) {  // 잘린 연속 — 선단만 Latin-1 유닛(느슨 규약)
            AppendUtf16LeUnit(out, c);
            ++i;
            continue;
        }
        i += len;
        if (cp >= 0x10000) {  // 서로게 쌍 출력
            const unsigned int v = cp - 0x10000;
            AppendUtf16LeUnit(out, 0xD800 | (v >> 10));
            AppendUtf16LeUnit(out, 0xDC00 | (v & 0x3FF));
        } else {
            AppendUtf16LeUnit(out, cp);
        }
    }
}

// 쓰기 역방향 수치 리터 — 읽기 RdBE32/SyncSafe32 쌍둥이. WrSyncSafe32는
// 2^28-1 초과 = false(폭주 절단 — 호출부 정직 거부 "tag too large").
inline void WrBE32(std::vector<unsigned char>& out, unsigned int v) {
    for (int sh = 24; sh >= 0; sh -= 8)
        out.push_back(static_cast<unsigned char>((v >> sh) & 0xFF));
}

inline bool WrSyncSafe32(std::vector<unsigned char>& out,
                         unsigned long long v) {
    if (v > 0x0FFFFFFFull) return false;
    out.push_back(static_cast<unsigned char>((v >> 21) & 0x7F));
    out.push_back(static_cast<unsigned char>((v >> 14) & 0x7F));
    out.push_back(static_cast<unsigned char>((v >> 7) & 0x7F));
    out.push_back(static_cast<unsigned char>(v & 0x7F));
    return true;
}

// ---- ID3v2 프레임 사슬 보행 (D5 보존 leg — "drop 금지"의 원문 회수) ----

// 보행 1건 재료 — 쓰기 leg가 본문 **원문 바이트**를 그대로 새 v2.3 사슬에
// 옮긴다(D5 — 그룹 ID·v2.4 데이터 길이 표지(dli) 등 프레임이 첨부한 바이트도
// 본문 원문의 일부 — 재해석 변조 금지·원문 보존 우선).
struct Id3FrameRec {
    std::string id;                   // 4바이트 프레임 ID(A-Z·0-9 — 파서 규약)
    std::vector<unsigned char> body;  // 본문 원문(10B 헤더 아래 선언 크기 전체)
    unsigned char v23flags = 0;       // 재기입용 v2.3 매핑 플래그(0x80/0x40/0x20)
};

// v2.4 프레임-level unsync 전개(0xFF 0x00 → 0xFF — 읽기 사슬 v2.3 전역
// unsync 전개 원문 걸음 재용). v2.3에는 프레임 unsync 플래그가 없어 데이터
// 원문 전개가 유일한 손실 없는 정규화(재기입 시 플래그 소각 ·원문 부기).
inline void DeunsyncBytes(std::vector<unsigned char>& b) {
    std::vector<unsigned char> out;
    out.reserve(b.size());
    for (size_t i = 0; i < b.size(); ++i) {
        out.push_back(b[i]);
        if (b[i] == 0xFF && i + 1 < b.size() && b[i + 1] == 0x00) ++i;
    }
    b.swap(out);
}

// v2.3/2.4 프레임 사슬 보행 — 읽기 ParseId3v2 사슬과 같은 걸음(패딩 선단
// 0x00 종료·탈자 파산·v2.4 syncsafe 크기(0x7F 초과 바이트 = 구형 리터 BE32
// 관용 폴백))에 쓰기 leg의 정직 판정을 얹는다: 선언 크기가 본문을 넘는
// 프레임은 클램프 없이 거부("frame truncated") — 보존 대상이라 읽기의
// 클램프(선언을 믿지 않는 경계 클램프 2s-⑤)와 다리가 갈라진다(정직
// 거부 = 원본 보존 우선 — 부서진 태그를 손대지 않는다). 압축/암호화/그룹
// 플래그는 v2.3 비트로 매핑(압축 0x80·암호화 0x40·그룹 0x20 — 그룹 ID
// 바이트는 본문 원문 선두에 이미 있다. v2.4 unsync(0x02)는 전개 후 소각,
// dli(0x01)는 본문 원문에 남긴다(v2.3 구조에 없는 표지 — 재기입에서 비트
// 소각·본문 원문 유지 — 정직 한계 부기: 실사용 드문 프레임이며 관리
// 프레임(TIT2/TPE1/TALB/APIC)은 전부 재작성이라 3필드 해독에 영향 없다).
inline bool WalkId3v2Frames(const unsigned char* body, size_t bodyN,
                            int ver, std::vector<Id3FrameRec>& out,
                            std::string& err) {
    out.clear();
    err.clear();
    const auto fidByte = [](unsigned char c) {
        return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    };
    size_t off = 0;
    while (off + 10 <= bodyN) {
        const unsigned char* fh = body + off;
        if (fh[0] == 0) break;  // 패딩 — 남은 영역은 기존 패딩(pad 산출이 흡수)
        if (!fidByte(fh[0]) || !fidByte(fh[1]) || !fidByte(fh[2]) ||
            !fidByte(fh[3])) {
            err = "frame id broken";  // 읽기는 파산 종료 — 쓰기는 정직 거부
            out.clear();
            return false;
        }
        size_t fsz;
        if (ver == 4 && fh[4] <= 0x7F && fh[5] <= 0x7F && fh[6] <= 0x7F &&
            fh[7] <= 0x7F) {
            fsz = static_cast<size_t>(SyncSafe32(fh + 4));
        } else {
            fsz = static_cast<size_t>(RdBE32(fh + 4));
        }
        const size_t rest = bodyN - (off + 10);  // off+10 ≤ bodyN 보장
        if (fsz > rest) {
            err = "frame truncated";  // 읽기는 클램프 — 쓰기는 정직 거부
            out.clear();
            return false;
        }
        const unsigned char fmt = fh[9];
        // 플래그 분류 — 읽기 ParseId3v2 원문 걸음(버전별 비트 정의) 재사용.
        const bool compressed = ver == 3 ? (fmt & 0x80) : (fmt & 0x08);
        const bool encrypted = ver == 3 ? (fmt & 0x40) : (fmt & 0x04);
        const bool grouped = ver == 3 ? (fmt & 0x20) : (fmt & 0x40);
        const bool frameUnsync = ver == 4 && (fmt & 0x02);
        // 본문 원문 수취 — 선언 크기 전체(그룹 ID·dli 선두 바이트 포함 원문).
        Id3FrameRec rec;
        rec.id.assign(reinterpret_cast<const char*>(fh), 4);
        rec.body.assign(fh + 10, fh + 10 + fsz);
        if (frameUnsync) DeunsyncBytes(rec.body);  // v2.4 프레임 unsync 전개
        unsigned char mapped = 0;
        if (compressed) mapped |= 0x80;
        if (encrypted) mapped |= 0x40;
        if (grouped) mapped |= 0x20;
        rec.v23flags = mapped;
        out.push_back(std::move(rec));
        off += 10 + fsz;
    }
    return true;
}

// ID3v2 태그 1건의 전체 구조 — 쓰기(WriteTag 기존 태그 유지 사슬)와 커버
// (ReadCover APIC 지목)가 공용으로 소비하는 보행 결과.
struct Id3TagInfo {
    int ver = 0;
    size_t end = 0;        // 태그 전체 끝(푸터 포함) — 오디오 데이터 시작
    size_t bodySize = 0;   // 선언 본문(기존 패딩 포함 — D5 패딩 조절 산출)
    std::vector<Id3FrameRec> frames;
};

// ID3v2 태그 헤더+프레임 사슬 판독 — 버전/전역 플래그 게이트(정직 거부)+
// 사슬 보행(WalkId3v2Frames). v2.3 전역 unsync(0x80)는 읽기 사슬 원문의
// 전개 규약이 있어 사본 전개 후 평문 프레임으로 수용한다(재작성 시 플래그
// 소각 — 정규화). 확장 헤더(0x40·v2.3/2.4 공용)와 v2.4 전역 unsync(0x80)
// 는 정직 거부 — 프레임 사슬 해석이 어긋나는 구조다(원본 보존 우선).
inline bool ReadId3Tag(const unsigned char* p, size_t n, Id3TagInfo& out,
                       std::string& err) {
    err.clear();
    out = Id3TagInfo();
    if (n < 10) { err = "tag truncated"; return false; }
    if (p[0] != 'I' || p[1] != 'D' || p[2] != '3') { err = "no id3"; return false; }
    if (p[3] < 3 || p[3] > 4) { err = "unsupported_id3_version"; return false; }
    const unsigned char flags = p[5];
    if (flags & 0x40) { err = "extended header unsupported"; return false; }
    if (p[3] == 4 && (flags & 0x80)) { err = "global unsync unsupported"; return false; }
    // 미계약 비트 — 정직 거부(해석을 보장할 수 없는 원문은 손대지 않는다).
    // v2.3 = 0x80(unsync·수용)|0x20(experimental — 읽기 무시 규약)만, v2.4 =
    // 0x10(푸터)|0x20만.
    const unsigned char allowed = p[3] == 3
                                      ? static_cast<unsigned char>(0x80 | 0x20)
                                      : static_cast<unsigned char>(0x10 | 0x20);
    if (flags & ~allowed) { err = "unsupported flags"; return false; }
    const unsigned long long tagSize = SyncSafe32(p + 6);
    const size_t footer = (p[3] == 4 && (flags & 0x10)) ? 10u : 0u;
    const unsigned long long endU = 10 + tagSize + footer;
    if (endU > n) { err = "tag truncated"; return false; }
    const unsigned char* body = p + 10;
    size_t bodyN = static_cast<size_t>(tagSize);
    std::vector<unsigned char> unsync;
    if (p[3] == 3 && (flags & 0x80)) {  // v2.3 전역 unsync — 읽기 사슬 원문 수형
        unsync.reserve(bodyN);
        for (size_t i = 0; i < bodyN; ++i) {
            unsync.push_back(body[i]);
            if (body[i] == 0xFF && i + 1 < bodyN && body[i + 1] == 0x00) ++i;
        }
        body = unsync.data();
        bodyN = unsync.size();
    }
    if (!WalkId3v2Frames(body, bodyN, p[3], out.frames, err)) return false;
    out.ver = p[3];
    out.end = static_cast<size_t>(endU);
    out.bodySize = static_cast<size_t>(tagSize);
    return true;
}

// ---- ID3v2.3 프레임 합성 (D5 — 정규화 출력) ----

// v2.3 프레임 10B 헤더+본문 — 크기 = **32bit BE(syncsafe 아님 — D5 규약)**,
// 플래그 = 상태바이트 0x00+포맷바이트(호출부 매핑 — 0 = 압축/암호화/그룹 없음).
inline void AppendId3Frame(std::vector<unsigned char>& out, const char* id,
                           const unsigned char* body, size_t bodyN,
                           unsigned char formatFlags) {
    for (int k = 0; k < 4; ++k) out.push_back(static_cast<unsigned char>(id[k]));
    WrBE32(out, static_cast<unsigned int>(bodyN));
    out.push_back(0);  // 플래그 상위 바이트(상태 — 무기입)
    out.push_back(formatFlags);
    out.insert(out.end(), body, body + bodyN);
}

// 텍스트 프레임 — 인코딩 1(UTF-16LE + 0xFF 0xFE BOM) 고정+2바이트 NUL 종결
// (유닛 정렬 보존 — BOM 이후 유닛수가 짝수).
inline void AppendId3TextFrame(std::vector<unsigned char>& out, const char* id,
                               const std::string& utf8) {
    std::vector<unsigned char> body;
    body.reserve(5 + utf8.size() * 2);
    body.push_back(1);   // enc 1 — UTF-16 w/ BOM(v2.3 규약의 유니코드 인코딩)
    body.push_back(0xFF);
    body.push_back(0xFE);
    Utf8ToUtf16Le(utf8, body);
    body.push_back(0);
    body.push_back(0);   // UTF-16 NUL 종결
    AppendId3Frame(out, id, body.data(), body.size(), 0);
}

// APIC 프레임 — 인코딩 0(브리프 계약: mime/desc = Latin-1)+mime 1종+빈 desc
// (1바이트 NUL이 종결을 겸한다)+그림 원문. 그림 데이터의 0xFF 0x00 시퀀스는
// 평문 그대로 기입한다(전역 unsync 플래그를 셋하지 않는 한 리더의 프레이밍
// 시퀀스와만 접촉하는 데이터 영역 — 읽기 사슬도 unsync 가공 없이 읽는다).
inline void AppendId3ApicFrame(std::vector<unsigned char>& out,
                               const std::string& mime,
                               const std::vector<unsigned char>& bytes) {
    std::vector<unsigned char> body;
    body.reserve(3 + mime.size() + bytes.size());
    body.push_back(0);  // enc 0 — Latin-1 mime/desc
    for (const char c : mime) body.push_back(static_cast<unsigned char>(c));
    body.push_back(0);  // mime 종결
    body.push_back(0);  // desc = 빈 + 종결
    body.insert(body.end(), bytes.begin(), bytes.end());
    AppendId3Frame(out, "APIC", body.data(), body.size(), 0);
}

// mime 정규화(브리프 계약 — "image/jpeg"/"image/png" 1표기): 공백 절제+
// 소문자화+jpg/jpeg → image/jpeg·png → image/png, "image/…" 원철자 원통과.
// 미지 형식 = 공백 절제+소문자 원문(정직 — 무음 변조 없음).
inline std::string NormalizeCoverMime(const std::string& raw) {
    std::string s;
    s.reserve(raw.size());
    for (const char ch : raw) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        s.push_back(FoldAscii(static_cast<char>(c)));
    }
    if (s == "jpg" || s == "jpeg" || s == "image/jpg" || s == "image/pjpeg")
        return "image/jpeg";
    if (s == "png" || s == "image/png") return "image/png";
    return s;
}

// ---- ID3 텍스트 종결 지목 (ReadCover — APIC 내부 필드 슬라이스) ----

// enc 0/3 = 1바이트 NUL, enc 1/2 = 2바이트 유닛(짝 정렬) NUL. 종결 부재 =
// false(파손 — 호출부가 정직 거부/다음 후보로 간다).
inline bool FindId3TextTerm(const unsigned char* p, size_t n, int enc,
                            size_t& termPos, size_t& unit) {
    if (enc == 1 || enc == 2) {
        unit = 2;
        for (size_t i = 0; i + 2 <= n; i += 2) {
            if (p[i] == 0 && p[i + 1] == 0) { termPos = i; return true; }
        }
        return false;
    }
    unit = 1;
    for (size_t i = 0; i < n; ++i) {
        if (p[i] == 0) { termPos = i; return true; }
    }
    return false;
}

// ---- T2 공용 부품 (FLAC/Ogg/WAV leg — 스펙 2026-10-25 §1) ----
//
// 읽기 사슬의 원문 계약을 그대로 거꾸로 걷는 부품 모음:
// ①vorbis comment 원문 사슬(벤더 유지 — D9) ②base64(inline — 표준 RFC4648,
// third_party 접촉 금지 계약) ③FLAC PICTURE 구조(FLAC PICTURE 블록 본문 =
// Ogg METADATA_BLOCK_PICTURE 값 동형 — 1파서 2소비) ④Ogg 페이지 하부(CRC·
// 수독 — D10 재기입 leg).

// 코멘트 엔트리 1건 — KEY 원철자 보존(재구성에서 자리를 지키며 값만 교체).
struct VorbisEntry {
    std::string key;
    std::string value;
};

// vorbis comment 전체 구조 수취(벤더+엔트리 열 — ParseVorbisComments의
// 원문 보존판: KEY 원철자·값 원문을 그대로 돌려주는 쓰기 leg용). 구조
// 파산(부족한 길이 선언·폭주 개수) = 정직 false. '=' 누락·빈 키 엔트리 =
// 스킵(읽기 ParseVorbisComments 원문 규약 수형).
inline bool ParseVorbisCommentsRaw(const unsigned char* p, size_t n,
                                   std::string& vendor,
                                   std::vector<VorbisEntry>& entries) {
    size_t off = 0;
    if (n < 4) return false;
    const unsigned int vlen = RdLE32(p + off);
    off += 4;
    if (vlen > n - off) return false;
    vendor.assign(reinterpret_cast<const char*>(p + off), vlen);
    off += vlen;
    if (off + 4 > n) return false;
    const unsigned int count = RdLE32(p + off);
    off += 4;
    if (count > 16384) return false;
    entries.reserve(count);
    for (unsigned int i = 0; i < count; ++i) {
        if (off + 4 > n) return false;
        const unsigned int clen = RdLE32(p + off);
        off += 4;
        if (clen > n - off) return false;
        const char* entry = reinterpret_cast<const char*>(p + off);
        off += clen;
        size_t eq = 0;
        while (eq < clen && entry[eq] != '=') ++eq;
        if (eq == 0 || eq >= clen) continue;
        VorbisEntry e;
        e.key.assign(entry, eq);
        e.value.assign(entry + eq + 1, clen - eq - 1);
        entries.push_back(std::move(e));
    }
    return true;
}

// 코멘트 키 ASCII 폴드(FoldAscii 재용 — 읽기 사슬 first-value 판정과 동형).
inline std::string FoldVorbisKey(const std::string& k) {
    std::string s;
    s.reserve(k.size());
    for (const char c : k) s.push_back(FoldAscii(c));
    return s;
}

// 코멘트 사슬 재구성(T2 D8 코멘트 부분 — FLAC 코멘트 블록 본문과 Ogg 코멘트
// 패킷 본문 공용): 기존 엔트리 원문 보존 + 관리 키(TITLE/ARTIST/ALBUM —
// ASCII 폴드)의 첫 위치에서 값 교체, 이후 동일 관리 키 = 소각(첫 값 계약 —
// 읽기 사슬과 동형), 부재 관리 키는 관리 순서로 신설. 빈 새 값 = 해당 키
// 소각(빈값 쓰기 = 비움과 등치 — T1 결정 3 수형). oggPicture 지정 = 기존
// METADATA_BLOCK_PICTURE 엔트리 전부 소각+1건 신설(D8 완전 교체 — 사슬 끝),
// null = 기존 엔트리 원문 유지.
inline void RebuildVorbisEntries(const std::vector<VorbisEntry>& old,
                                 const TagSet& t,
                                 const std::string* oggPicture,
                                 std::vector<VorbisEntry>& out) {
    out.clear();
    bool saw[3] = {false, false, false};
    const std::string* vals[3] = {&t.title, &t.artist, &t.album};
    static const char* namesLower[3] = {"title", "artist", "album"};
    static const char* namesCanon[3] = {"TITLE", "ARTIST", "ALBUM"};
    for (const VorbisEntry& e : old) {
        const std::string k = FoldVorbisKey(e.key);
        int slot = -1;
        for (int i = 0; i < 3 && slot < 0; ++i)
            if (k == namesLower[i]) slot = i;  // 폴드 키 = 소문자 관용 비교
        if (slot >= 0) {
            if (saw[slot] || vals[slot]->empty()) continue;
            saw[slot] = true;
            out.push_back(VorbisEntry{e.key, *vals[slot]});
            continue;
        }
        if (oggPicture != nullptr && k == "metadata_block_picture")
            continue;
        out.push_back(e);
    }
    for (int i = 0; i < 3; ++i)
        if (!saw[i] && !vals[i]->empty())
            out.push_back(VorbisEntry{namesCanon[i], *vals[i]});
    if (oggPicture != nullptr && !oggPicture->empty())
        out.push_back(VorbisEntry{"METADATA_BLOCK_PICTURE", *oggPicture});
}

// vorbis comment 본문 serialization(패킷 마커 제외 — FLAC 블록 본문과 동일
// 형식): [32LE vlen][vendor][32LE count][count × [32LE]"KEY=value"].
inline void AppendVorbisCommentBody(std::vector<unsigned char>& out,
                                    const std::string& vendor,
                                    const std::vector<VorbisEntry>& entries) {
    const auto le32 = [&out](unsigned int v) {
        out.push_back(static_cast<unsigned char>(v & 0xFF));
        out.push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
        out.push_back(static_cast<unsigned char>((v >> 16) & 0xFF));
        out.push_back(static_cast<unsigned char>((v >> 24) & 0xFF));
    };
    le32(static_cast<unsigned int>(vendor.size()));
    out.insert(out.end(), vendor.begin(), vendor.end());
    le32(static_cast<unsigned int>(entries.size()));
    for (const VorbisEntry& e : entries) {
        le32(static_cast<unsigned int>(e.key.size() + 1 + e.value.size()));
        out.insert(out.end(), e.key.begin(), e.key.end());
        out.push_back('=');
        out.insert(out.end(), e.value.begin(), e.value.end());
    }
}

// LE64(리온 — Ogg 페이지 granule 8바이트 재기입용. 읽기 RdLE32 쌍둥이).
inline unsigned long long RdLE64(const unsigned char* b) {
    return static_cast<unsigned long long>(b[0]) |
           (static_cast<unsigned long long>(b[1]) << 8) |
           (static_cast<unsigned long long>(b[2]) << 16) |
           (static_cast<unsigned long long>(b[3]) << 24) |
           (static_cast<unsigned long long>(b[4]) << 32) |
           (static_cast<unsigned long long>(b[5]) << 40) |
           (static_cast<unsigned long long>(b[6]) << 48) |
           (static_cast<unsigned long long>(b[7]) << 56);
}

inline void WrLE32(std::vector<unsigned char>& out, unsigned int v) {
    out.push_back(static_cast<unsigned char>(v & 0xFF));
    out.push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
    out.push_back(static_cast<unsigned char>((v >> 16) & 0xFF));
    out.push_back(static_cast<unsigned char>((v >> 24) & 0xFF));
}

// ---- base64 (inline — 표준 RFC4648 알파벳, third_party 접촉 금지 계약) ----

// Ogg METADATA_BLOCK_PICTURE 값 = base64(FLAC PICTURE 구조). 느슨 규약:
// 공백류는 건너 뛰고(코멘트 값의 개행 관용), '='의 위치 무정렬·알파벳 밖
// 바이트·'=' 이후 데이터 = 정직 거부(false — 부분 출력 없음·out 소각).
inline bool Base64Decode(const std::string& in,
                         std::vector<unsigned char>& out) {
    out.clear();
    const auto dec = [](unsigned char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    unsigned int buf = 0;
    int bits = 0;
    int pad = 0;
    for (const char ch : in) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        if (c == '=') {
            ++pad;
            continue;
        }
        if (pad > 0) return false;  // 패딩 이후 데이터 = 파산
        const int v = dec(c);
        if (v < 0) return false;
        buf = (buf << 6) | static_cast<unsigned int>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<unsigned char>((buf >> bits) & 0xFF));
            buf &= (1u << bits) - 1;
        }
    }
    return true;  // 잔여 2/4비트 = 패딩 비트(느슨 규약 — 값 검증 생략: 구조
                  // 파산은 ParseFlacPictureStruct가 걷는다)
}

// base64 인코딩(Ogg 그림 필드 기입용 — '=' 패딩 규약 포함).
inline void Base64Encode(const unsigned char* p, size_t n, std::string& out) {
    static const char* kTb =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned int buf = 0;
    int bits = 0;
    for (size_t i = 0; i < n; ++i) {
        buf = ((buf << 8) | p[i]) & 0xFFFFu;  // 유효 12비트 — 잔산 소각
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            out.push_back(kTb[(buf >> bits) & 0x3F]);
        }
    }
    if (bits == 2) {
        out.push_back(kTb[(buf << 4) & 0x3F]);
        out += "==";
    } else if (bits == 4) {
        out.push_back(kTb[(buf << 2) & 0x3F]);
        out += '=';
    }
}

// ---- FLAC PICTURE 구조 (PICTURE 블록 본문 = METADATA_BLOCK_PICTURE 값) ----

// [32BE 그림형식][32BE mime len][mime][32BE desc len][desc][32BE w][32BE h]
// [32BE depth][32BE colors][32BE data len][data] — 그림형식 3 = front cover
// (표준 계약 — ReadCover는 형식 값을 판정하지 않고 mime만 정규화한다).
// 파산(길이 선언 초과) = false — 호출부가 다음 후보(첫 유효 계약)로 간다.
inline bool ParseFlacPictureStruct(const unsigned char* p, size_t n,
                                   std::string& mime,
                                   std::vector<unsigned char>& bytes) {
    size_t off = 0;
    const auto take = [&](unsigned int& v) -> bool {
        if (off + 4 > n) return false;
        v = RdBE32(p + off);
        off += 4;
        return true;
    };
    unsigned int mimeLen = 0, descLen = 0, dataLen = 0, skip = 0;
    if (!take(skip)) return false;  // 그림형식 — 스킵(판정 필드 아님)
    if (!take(mimeLen) || mimeLen > n - off) return false;
    mime.assign(reinterpret_cast<const char*>(p + off), mimeLen);
    off += mimeLen;
    if (!take(descLen) || descLen > n - off) return false;
    off += descLen;
    for (int i = 0; i < 4; ++i) {
        if (!take(skip)) return false;  // w·h·depth·colors — 표기 계약 무접촉
    }
    if (!take(dataLen) || dataLen > n - off) return false;
    bytes.assign(p + off, p + off + dataLen);
    return true;
}

// METADATA_BLOCK_PICTURE 엔트리 열 → 커버 아트(첫 유효 — 2u 원문 계약):
// 베이스64 → FLAC PICTURE 구조 → mime 정규화. 복호화 파산·빈 바이트·mime
// 미지 = 다음 후보(읽기 사슬 관용 기각). Ogg(vorbis) leg와 opus leg가 같은
// 사슬을 소비한다(#115 T2 — METADATA_BLOCK_PICTURE 재용).
inline bool CoverFromMbpEntries(const std::vector<VorbisEntry>& entries,
                                CoverArt& out) {
    for (const VorbisEntry& e : entries) {
        if (FoldVorbisKey(e.key) != "metadata_block_picture") continue;
        std::vector<unsigned char> raw;
        if (!Base64Decode(e.value, raw)) continue;
        std::string mime;
        std::vector<unsigned char> bytes;
        if (!ParseFlacPictureStruct(raw.data(), raw.size(), mime, bytes) ||
            bytes.empty())
            continue;
        const std::string norm = NormalizeCoverMime(mime);
        if (norm.empty()) continue;
        out.mime = norm;
        out.bytes = std::move(bytes);
        return true;
    }
    return false;
}

inline std::vector<unsigned char> BuildFlacPictureStruct(
        const std::string& mime, const std::vector<unsigned char>& bytes) {
    std::vector<unsigned char> o;
    o.reserve(32 + mime.size() + bytes.size());
    WrBE32(o, 3);  // 그림형식 3 = front cover(표준 계약)
    WrBE32(o, static_cast<unsigned int>(mime.size()));
    o.insert(o.end(), mime.begin(), mime.end());
    WrBE32(o, 0);  // desc 빈(종결 역할)
    for (int i = 0; i < 4; ++i) WrBE32(o, 0);  // w·h·depth·colors 미지정
    WrBE32(o, static_cast<unsigned int>(bytes.size()));
    o.insert(o.end(), bytes.begin(), bytes.end());
    return o;
}

// FLAC 메타 블록 1건 헤더([1비트 is-last][7비트 type][24BE size]) 기입.
inline void AppendFlacBlockHeader(std::vector<unsigned char>& out, bool last,
                                  unsigned type, size_t sz) {
    out.push_back(static_cast<unsigned char>((last ? 0x80 : 0) | (type & 0x7F)));
    out.push_back(static_cast<unsigned char>((sz >> 16) & 0xFF));
    out.push_back(static_cast<unsigned char>((sz >> 8) & 0xFF));
    out.push_back(static_cast<unsigned char>(sz & 0xFF));
}

// ---- Ogg 페이지 하부 (D10 — 코멘트 페이지 교체+뒤페이지 재기입 leg) ----

// Ogg CRC32(poly 0x04C11DB7·init 0·무반사·무종결 XOR — Ogg 규약 표준).
// 표는 처음 1회 완성(inline 함수의 함수 지역 static — 단일 TU 계약).
inline unsigned int OggCrc(const unsigned char* p, size_t n) {
    static unsigned int tbl[256];
    static bool filled = false;
    if (!filled) {
        for (unsigned int i = 0; i < 256; ++i) {
            unsigned int r = i << 24;
            for (int b = 0; b < 8; ++b)
                r = (r & 0x80000000u) ? ((r << 1) ^ 0x04C11DB7u) : (r << 1);
            tbl[i] = r;
        }
        filled = true;
    }
    unsigned int crc = 0;
    for (size_t i = 0; i < n; ++i)
        crc = (crc << 8) ^ tbl[((crc >> 24) & 0xFF) ^ p[i]];
    return crc;
}

// Ogg 페이지 1건 수취(스트림 현재 위치에서 — 27B 헤더+세그 표+페이로드).
// 반환: 1 = 성립("OggS" 매직 포함), 0 = 페이지 경계에서 깨끗한 EOF,
// -1 = 파산(잘린 헤더/페이로드·비페이지 도달 — 호출부가 정직 거부/walk 종료).
inline int OggReadPage(std::ifstream& f, std::vector<unsigned char>& page) {
    unsigned char h[27];
    f.read(reinterpret_cast<char*>(h), 27);
    if (f.gcount() == 0) return 0;  // 깨끗한 종료(페이지 경계)
    if (f.gcount() != 27) return -1;
    if (std::memcmp(h, "OggS", 4) != 0) return -1;
    page.assign(h, h + 27);
    const size_t segs = h[26];
    if (segs > 0) {
        const size_t tblAt = page.size();
        page.resize(tblAt + segs);
        f.read(reinterpret_cast<char*>(&page[tblAt]),
               static_cast<std::streamsize>(segs));
        if (f.gcount() != static_cast<std::streamsize>(segs)) return -1;
        size_t plen = 0;
        for (size_t s = 0; s < segs; ++s) plen += page[27 + s];
        if (plen > 0) {
            const size_t payAt = page.size();
            page.resize(payAt + plen);
            f.read(reinterpret_cast<char*>(&page[payAt]),
                   static_cast<std::streamsize>(plen));
            if (f.gcount() != static_cast<std::streamsize>(plen)) return -1;
        }
    }
    return 1;
}

// ---- 네이티브 쓰기 leg (D6 — fsync 성립 조건) ----

// std::ofstream은 fd 접점이 없어 fsync가 성립하지 않는다(실측 제약 — 원장
// 부기). 네이티브 저수준 쓰기(Win = CreateFileW+FlushFileBuffers, posix =
// ::open+::fsync)로 같은 결과 — 경로는 fs::path 원문(wide/native — narrow
// fopen ACP 함정 회피, ParseTag 개방 수형 동형 원문).
#if defined(_WIN32)
// kernel32 단일 함수 선언 — MusicModel.h GetFileAttributesW 원문 선례 수형
// (windows.h 전개 금지 계약).
extern "C" __declspec(dllimport) void* __stdcall CreateFileW(
    const wchar_t* fileName, unsigned long desiredAccess,
    unsigned long shareMode, void* securityAttributes,
    unsigned long creationDisposition, unsigned long flagsAndAttributes,
    void* templateFile);
extern "C" __declspec(dllimport) int __stdcall WriteFile(
    void* handle, const void* buffer, unsigned long bytesToWrite,
    unsigned long* bytesWritten, void* overlapped);
extern "C" __declspec(dllimport) int __stdcall FlushFileBuffers(void* handle);
extern "C" __declspec(dllimport) int __stdcall CloseHandle(void* handle);

// winnt/winbase 상수 로컬 복제(원문 형식 주석 — GetFileAttributesW 선례 수형).
inline constexpr unsigned long kFileGenericWrite = 0x40000000ul;  // GENERIC_WRITE
inline constexpr unsigned long kFileShareReadWrite = 0x3ul;      // FILE_SHARE_READ|WRITE
inline constexpr unsigned long kFileCreateAlways = 0x2ul;        // CREATE_ALWAYS
inline constexpr unsigned long kFileAttributeNormal = 0x80ul;    // FILE_ATTRIBUTE_NORMAL

struct TagFileWriter {
    void* h = reinterpret_cast<void*>(-1);  // INVALID_HANDLE_VALUE
    bool Open(const std::filesystem::path& p) {
        h = CreateFileW(p.c_str(), kFileGenericWrite, kFileShareReadWrite,
                        nullptr, kFileCreateAlways, kFileAttributeNormal,
                        nullptr);
        return h != reinterpret_cast<void*>(-1);
    }
    bool WriteAll(const unsigned char* data, size_t n) {
        while (n > 0) {
            const unsigned long chunk =
                n > 0x40000000ul ? 0x40000000ul : static_cast<unsigned long>(n);
            unsigned long written = 0;
            if (!WriteFile(h, data, chunk, &written, nullptr) ||
                written != chunk)
                return false;
            data += chunk;
            n -= chunk;
        }
        return true;
    }
    bool FlushToDisk() { return FlushFileBuffers(h) != 0; }  // D6 fsync leg
    void Close() {
        if (h != reinterpret_cast<void*>(-1)) CloseHandle(h);
        h = reinterpret_cast<void*>(-1);
    }
};
#else
struct TagFileWriter {
    int fd = -1;
    bool Open(const std::filesystem::path& p) {
        fd = ::open(p.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        return fd >= 0;
    }
    bool WriteAll(const unsigned char* data, size_t n) {
        while (n > 0) {
            const size_t want = n > (1u << 20) ? (1u << 20) : n;
            const ssize_t w = ::write(fd, data, want);
            if (w < 0 && errno == EINTR) continue;
            if (w <= 0) return false;
            data += static_cast<size_t>(w);
            n -= static_cast<size_t>(w);
        }
        return true;
    }
    bool FlushToDisk() { return ::fsync(fd) == 0; }  // D6 fsync leg
    void Close() {
        if (fd >= 0) ::close(fd);
        fd = -1;
    }
};
#endif

// ---- 파일 사다리 공용 (T2 — 4형식 leg가 하나의 원자적 사다리를 쓴다) ----

// 스트림 구간 복사(T1 mp3 leg 원문 수형 — 64KB 분절). 원본 read 실패·부분
// read(경합 절단) = "audio read failed", tmp 기입 실패 = "tmp write failed".
inline bool CopyStreamRange(std::ifstream& f, TagFileWriter& w,
                            std::streamoff from, std::streamoff count,
                            std::string& err) {
    f.clear();
    f.seekg(from, std::ios::beg);
    std::vector<char> chunk(64 * 1024);
    while (count > 0) {
        const std::streamsize want =
            count < static_cast<std::streamoff>(chunk.size())
                ? static_cast<std::streamsize>(count)
                : static_cast<std::streamsize>(chunk.size());
        f.read(chunk.data(), want);
        const std::streamsize got = f.gcount();
        if (got != want) {
            err = "audio read failed";
            return false;
        }
        if (!w.WriteAll(reinterpret_cast<const unsigned char*>(chunk.data()),
                        static_cast<size_t>(got))) {
            err = "tmp write failed";
            return false;
        }
        count -= got;
    }
    return true;
}

// 교체 rename 사다리(T1 mp3 leg 원문 수형 — MusicDirStore WriteSettingsAtomic
// 선례: 1세대 교체 rename, 실패만 .bak 사다리+복원).
inline bool TagReplaceRename(const std::string& tmp, const std::string& path,
                             std::string& err) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::rename(fs::path(tmp), fs::path(path), ec);
    if (!ec) return true;
    const std::string bak = path + ".bak";
    fs::remove(fs::path(bak), ec);
    if (ec) {
        std::remove(tmp.c_str());
        err = "bak remove failed";
        return false;
    }
    ec.clear();
    fs::rename(fs::path(path), fs::path(bak), ec);
    if (ec) {
        std::remove(tmp.c_str());
        err = "original backup failed";
        return false;
    }
    ec.clear();
    fs::rename(fs::path(tmp), fs::path(path), ec);
    if (ec) {
        std::error_code rec;
        fs::rename(fs::path(bak), fs::path(path), rec);  // 원본 복원 시도
        std::remove(tmp.c_str());
        err = "rename failed";
        return false;
    }
    std::error_code bc;
    fs::remove(fs::path(bak), bc);  // 성공 세대의 이주 잔산 소각
    return true;
}

// 원자적 tmp 사다리(D6 — 4형식 공용): head1 버퍼 기입 → tail leg(원본 스트림
// 에서 이어 읽어 형식별 재기입이 필요한 페이지/청크를 걸며 기입 — null이면
// 스트림 접촉 없음) → head2 버퍼 기입 → fsync → 원본 close → rename 사다리.
// 실패 = 정직 거부+tmp 소각+원본 무손상(D6 계약 — T1 mp3 leg 원문 수형).
// 원본 ifstream은 rename 전에 닫는다(T1 트랩 10 — Windows에서 대상 파일이
// 공유 핸들로 열려 있으면 교체 rename이 거부된다).
inline bool TagAtomicWrite(
        const std::string& path, std::ifstream& f,
        const std::vector<unsigned char>& head1,
        const std::function<bool(TagFileWriter&)>& tail,
        const std::vector<unsigned char>& head2, std::string& err) {
    namespace fs = std::filesystem;
    const std::string tmp =
        path + "." + std::to_string(ThisProcessId()) + ".tagtmp";
    {
        TagFileWriter w;
        if (!w.Open(fs::path(tmp))) {
            err = "tmp open failed";
            return false;
        }
        const auto fail = [&](const char* e) {
            w.Close();
            std::error_code rec;
            fs::remove(fs::path(tmp), rec);  // tmp 소각 — 잔산 방지
            err = e;
            return false;
        };
        if (!w.WriteAll(head1.data(), head1.size()))
            return fail("tmp write failed");
        if (tail) {
            if (!tail(w)) {
                if (err.empty()) err = "tmp write failed";
                w.Close();
                std::error_code rec;
                fs::remove(fs::path(tmp), rec);
                return false;
            }
        }
        if (!w.WriteAll(head2.data(), head2.size()))
            return fail("tmp write failed");
        if (!w.FlushToDisk()) return fail("fsync failed");  // D6 fsync 계약
        w.Close();
    }
    f.close();  // 원본 핸들 해제 — Windows rename 교체의 공유 핸들 충돌 소각
    return TagReplaceRename(tmp, path, err);
}

// 커버 게이트(4형식 공용 — mp3 leg 원문 계약 그대로): null = 정의로 통과,
// 빈 그림/빈 mime/비지원 mime = 정직 거부. 성립 시 mime = 정규화 표기.
inline bool ValidateCover(const CoverArt* cover, std::string& mime,
                          std::string& err) {
    if (cover == nullptr) return true;
    if (cover->bytes.empty()) {
        err = "cover empty";
        return false;
    }
    mime = NormalizeCoverMime(cover->mime);
    if (mime.empty()) {
        err = "cover mime missing";
        return false;
    }
    if (mime != "image/jpeg" && mime != "image/png" &&
        mime.rfind("image/", 0) != 0) {
        err = "cover mime unsupported";
        return false;
    }
    return true;
}

// ID3v2.3 본문 조립(D5 관리 프레임 교체+원문 보존 — T1 mp3 leg의 신설/재작성
// 사슬을 그대로 뽑아낸 공용판: WAV id3 청크 leg가 재용한다).
inline bool BuildId3V23Body(const Id3TagInfo* old, const TagSet& t,
                            const CoverArt* cover,
                            std::vector<unsigned char>& body,
                            std::string& err) {
    body.clear();
    err.clear();
    // 관리 텍스트 프레임 — 인코딩 1(UTF-16LE+BOM) 고정 통일. 빈 필드 = 생략
    // (사슬 재구성이 기존 프레임을 교체하므로 빈값 = 비움 쓰기와 등치).
    if (!t.title.empty()) AppendId3TextFrame(body, "TIT2", t.title);
    if (!t.artist.empty()) AppendId3TextFrame(body, "TPE1", t.artist);
    if (!t.album.empty()) AppendId3TextFrame(body, "TALB", t.album);
    if (cover != nullptr) {
        std::string mime;
        if (!ValidateCover(cover, mime, err)) return false;
        AppendId3ApicFrame(body, mime, cover->bytes);
    }
    // 기존 프레임 원문 복제(D5 — TCON 장르 복제+해석 불가 프레임 drop 금지).
    // 관리 프레임은 새 프레임이 교체하고, cover 지정 시 기존 APIC도 소각
    // (D8 완전 교체 — T1 mp3 leg 원문 수형).
    if (old != nullptr) {
        for (const Id3FrameRec& rec : old->frames) {
            if (rec.id == "TIT2" || rec.id == "TPE1" || rec.id == "TALB")
                continue;
            if (rec.id == "APIC" && cover != nullptr) continue;
            AppendId3Frame(body, rec.id.c_str(), rec.body.data(),
                           rec.body.size(), rec.v23flags);
        }
    }
    return true;
}

// v2.3 태그 헤더+본문+패딩 조립 — 태그 크기 = syncsafe(읽기 사슬 round-trip
// 계약 — T1 결정 1 원문 수형). 2^28 초과 = 정직 거부.
inline bool AssembleId3V23Tag(const std::vector<unsigned char>& body,
                              size_t pad, std::vector<unsigned char>& tag,
                              std::string& err) {
    tag.clear();
    tag.reserve(10 + body.size() + pad);
    tag.push_back('I');
    tag.push_back('D');
    tag.push_back('3');
    tag.push_back(0x03);  // v2.3 (D5 정규화 — v2.4 syncsafe 재작성 금지)
    tag.push_back(0x00);
    tag.push_back(0x00);  // 플래그 0 — 평문 사슬
    if (!WrSyncSafe32(tag,
                      static_cast<unsigned long long>(body.size()) + pad)) {
        err = "tag too large";
        return false;
    }
    tag.insert(tag.end(), body.begin(), body.end());
    tag.resize(10 + body.size() + pad);  // 패딩 = 0x00(resize 채움)
    return true;
}

// APIC 추출(프레임 사슬에서 첫 유효 — T1 mp3 leg APIC 보행을 뽑아낸 공용판:
// WAV leg가 "id3 " 청크 페이로드에서 재용한다). 파손 APIC = 다음 후보.
inline bool ApicFromFrames(const std::vector<Id3FrameRec>& frames,
                           CoverArt& out) {
    for (const Id3FrameRec& rec : frames) {
        if (rec.id != "APIC" || rec.body.empty()) continue;
        // APIC 본문: [enc][mime][NUL][desc][NUL][picture] — 인코딩 0/1
        // 모두 지원(mime 해독은 enc에 따라 Latin-1/UTF-16 — 브리프 계약).
        const int enc = rec.body[0];
        size_t i = 1;
        size_t mimeEnd = 0, unit = 1;
        if (!FindId3TextTerm(rec.body.data() + i, rec.body.size() - i, enc,
                             mimeEnd, unit))
            continue;
        const std::string rawMime =
            DecodeId3Text(rec.body.data() + i, mimeEnd, enc);
        const size_t di = i + mimeEnd + unit;  // desc 시작
        if (di > rec.body.size()) continue;
        size_t descEnd = 0, descUnit = 1;
        if (!FindId3TextTerm(rec.body.data() + di, rec.body.size() - di, enc,
                             descEnd, descUnit))
            continue;
        const size_t picOff = di + descEnd + descUnit;  // 그림 원문 시작
        if (picOff > rec.body.size()) continue;
        const std::string mime = NormalizeCoverMime(rawMime);
        if (mime.empty()) continue;
        // 빈 그림 APIC = 빈 out 성립 금지(ReadCover 거부 계약 "거부 = 빈 out"
        // — T1 deferred ④): 그림 본문 0B면 다음 후보(파손 APIC 관용 원문).
        if (picOff == rec.body.size()) continue;
        out.mime = mime;
        out.bytes.assign(rec.body.data() + picOff,
                         rec.body.data() + rec.body.size());
        return true;
    }
    return false;
}

// ---- FLAC 쓰기 leg (D8 커버 교체·D9 재조립 — 메타 블록 연쇄) ----

// 사슬 스캔(원문 보존 — [0, chainEnd) 버퍼): STREAMINFO 첫 블록 필수(34B)·
// SEEKTABLE 보존 검증(bsz ≡ 1 mod 18 — D9 판정)·VORBIS_COMMENT 원문 수취
// (벤더 유지)·PICTURE 목록 수취(D8 교체 대상). 사슬 종결(is-last) 미발견·
// 폭주 블록 수 = 정직 거부.
inline bool FlacScanRegion(std::ifstream& f, std::streamoff total,
                           std::vector<unsigned char>& region,
                           std::string& vendor,
                           std::vector<VorbisEntry>& oldEntries,
                           std::vector<size_t>& picOffsets, size_t& chainEnd,
                           int& guard, std::string& err) {
    namespace fs = std::filesystem;
    region.clear();
    picOffsets.clear();
    vendor.clear();
    oldEntries.clear();
    chainEnd = 0;
    f.clear();
    f.seekg(0, std::ios::beg);
    region.resize(4);
    f.read(reinterpret_cast<char*>(region.data()), 4);
    if (f.gcount() != 4) {
        err = "flac structure";
        return false;
    }
    guard = 0;
    bool sawComment = false;
    while (true) {
        if (++guard > 256) {
            err = "flac structure";  // 폭주 절단(읽기 사슬 guard 원문 수형)
            return false;
        }
        unsigned char hdr[4];
        f.read(reinterpret_cast<char*>(hdr), 4);
        if (f.gcount() != 4) {
            err = "flac structure";
            return false;
        }
        region.insert(region.end(), hdr, hdr + 4);
        const bool last = (hdr[0] & 0x80) != 0;
        const unsigned blockType = static_cast<unsigned>(hdr[0] & 0x7F);
        const size_t bsz = (static_cast<size_t>(hdr[1]) << 16) |
                           (static_cast<size_t>(hdr[2]) << 8) |
                           static_cast<size_t>(hdr[3]);
        if (region.size() + bsz >
            static_cast<size_t>(kTagWriteMax)) {
            err = "tag too large";
            return false;
        }
        if (region.size() + bsz > static_cast<size_t>(total)) {
            err = "flac truncated";  // 선언 블록이 파일을 넘는다 — 정직 거부
            return false;
        }
        const size_t payAt = region.size();
        region.resize(region.size() + bsz);
        f.read(reinterpret_cast<char*>(&region[payAt]),
               static_cast<std::streamsize>(bsz));
        if (f.gcount() != static_cast<std::streamsize>(bsz)) {
            err = "flac truncated";
            return false;
        }
        if (region.size() == 4 + 4 + bsz) {  // 첫 블록 = STREAMINFO 필수
            if (blockType != 0) {
                err = "flac streaminfo missing";  // D9 — 구성 계약
                return false;
            }
            if (bsz != 34) {
                err = "flac streaminfo missing";  // STREAMINFO = 항상 34B
                return false;
            }
        }
        if (blockType == 3 && (bsz < 1 || (bsz - 1) % 18 != 0)) {
            err = "seektable broken";  // D9 — 보존 판정 실패 = 정직 거부
            return false;
        }
        if (blockType == 4 && !sawComment) {
            sawComment = true;  // 첫 코멘트 블록만 소취(복수 = 비규약 — 첫이 진실원)
            const unsigned char* p = region.data() + payAt;
            if (!ParseVorbisCommentsRaw(p, bsz, vendor, oldEntries)) {
                err = "vorbis comment broken";
                return false;
            }
        } else if (blockType == 6 && picOffsets.empty()) {
            picOffsets.push_back(payAt);  // 첫 유효 PICTURE만(D8 — 교체 자리)
        }
        if (last) {
            chainEnd = region.size();
            return true;
        }
    }
}

// FLAC 쓰기 leg — 메타 블록 연쇄 재조립(스펙 D8·D9): 구역 원문을 걷며 블록을
// 다시 쓴다. STREAMINFO(첫 블록·34B)와 그 밖 보존 블록(SEQUENCE 표·APPLICATION
// ·PADDING·CUESHEET)은 원문 그대로, VORBIS_COMMENT(4)는 신설 코멘트(벤더 유지
// +관리 키 교체)로 교체(복수 = 첫 것 교체·나머지 소각), PICTURE(6)는 cover 지정
// 시 첫 유효 자리에서 완전 교체+복수 소각(D8 — 보유 블록 보존 없음), cover 지정
// 이 없으면 원문 유지. 신설 블록은 고정 순서 그룹(type ≤ 4) 뒤에 끼운다(FLAC
// 규약 — 5 이상은 자유 순서). is-last는 마지막 블록에만 재마킹한다. 그 뒤는
// 오디오 데이터 원문([chainEnd, EOF) — 원자적 사다리 스트림 복사). cover 지정
// 시 기존 METADATA_BLOCK_PICTURE 코멘트 엔트리도 전부 소각한다(구 아트 b64
// 잔존 방지 — F1).
inline bool WriteFlacTag(const std::string& path, std::ifstream& f,
                         std::streamoff total, const TagSet& t,
                         const CoverArt* cover, std::string& err) {
    std::vector<unsigned char> region;
    std::vector<size_t> picOffsets;
    std::string vendor;
    std::vector<VorbisEntry> oldEntries;
    size_t chainEnd = 0;
    int guard = 0;
    if (!FlacScanRegion(f, total, region, vendor, oldEntries, picOffsets,
                        chainEnd, guard, err))
        return false;
    std::vector<VorbisEntry> news;
    // F1(최final 리뷰 — D8 정신): cover 지정 = 기존 METADATA_BLOCK_PICTURE
    // 코멘트 엔트리 전부 소각(구 아트 b64 코멘트 잔존 방지 — PICTURE 블록이
    // 진실원). 빈 문자열 포인터 = 소각 전용+신설 없음(RebuildVorbisEntries
    // 빈값 가드), null = 기존 엔트리 원문 유지(cover=null 계약 — 2u-⑫).
    std::string picDrop;
    RebuildVorbisEntries(oldEntries, t,
                         cover != nullptr ? &picDrop : nullptr, news);
    std::vector<unsigned char> newComment;
    AppendVorbisCommentBody(newComment, vendor, news);
    std::string picMime;
    std::vector<unsigned char> newPicture;
    if (cover != nullptr) {
        if (!ValidateCover(cover, picMime, err)) return false;
        newPicture = BuildFlacPictureStruct(picMime, cover->bytes);
    }
    // 신설 블록 크기 가드(블록 24비트 크기 필드 한계 — 16MiB 안).
    if (newComment.size() >= (1u << 24) || newPicture.size() >= (1u << 24)) {
        err = "tag too large";
        return false;
    }
    // ---- 재조립 ----
    std::vector<unsigned char> out;
    out.insert(out.end(), "fLaC", "fLaC" + 4);
    size_t off = 4;
    bool sawComment = false, replacedPicture = false;
    size_t fixedEnd = 4;
    for (int g = 0; off + 4 <= region.size() && g < 256; ++g) {
        const unsigned char* hdr = region.data() + off;
        const unsigned blockType = static_cast<unsigned>(hdr[0] & 0x7F);
        const size_t bsz = (static_cast<size_t>(hdr[1]) << 16) |
                           (static_cast<size_t>(hdr[2]) << 8) |
                           static_cast<size_t>(hdr[3]);
        const size_t payAt = off + 4;
        if (payAt + bsz > region.size()) break;  // 스캔 leg와 동일 — 도달 불가
        if (blockType == 4 && !sawComment) {
            sawComment = true;
            AppendFlacBlockHeader(out, false, 4, newComment.size());
            out.insert(out.end(), newComment.begin(), newComment.end());
        } else if (blockType == 6 && cover != nullptr && !replacedPicture) {
            replacedPicture = true;
            if (newPicture.empty()) {
                // 빈 신설 그림 — 첫 PICTURE 자리만 소각(교체 실패 아님 —
                // cover 계약상 교체가 항상 비어 있지 않다 — 도달 불가 방어선)
                AppendFlacBlockHeader(out, false, 6, 0);
            } else {
                AppendFlacBlockHeader(out, false, 6, newPicture.size());
                out.insert(out.end(), newPicture.begin(), newPicture.end());
            }
        } else if (blockType == 6 && cover != nullptr) {
            // 복수 PICTURE — 소각(D8 완전 교체)
        } else if (blockType == 4) {
            // 복수 VORBIS_COMMENT — 소각(FLAC 규약 = 스트림당 1건 — docs/90
            // "VORBIS_COMMENT 첫 것만 교체(복수 소각)" 원장행과 정합 — F2).
            // continue 대신 공과 분기 유지: off 전위+is-last 원문 계속(사슬
            // 보행 원문 — 종결 표지 소각 방지).
        } else {
            // 보존 — 헤더(플래그 소각 — is-last 재마킹 leg)+페이로드 원문.
            AppendFlacBlockHeader(out, false, blockType, bsz);
            out.insert(out.end(), region.begin() + payAt,
                       region.begin() + payAt + bsz);
        }
        if (blockType <= 4) fixedEnd = out.size();  // 고정 순서 그룹 끝
        off = payAt + bsz;
        if (hdr[0] & 0x80) break;  // 사슬 종결 — chainEnd와 동일(스캔 leg 원문)
    }
    if (!sawComment) {
        // 신설 코멘트 — 고정 순서 그룹 뒤에 끼운다(FLAC 규약·T2 재량 leg).
        constexpr size_t kCommentType = 4;
        std::vector<unsigned char> ins;
        AppendFlacBlockHeader(ins, false, kCommentType, newComment.size());
        ins.insert(ins.end(), newComment.begin(), newComment.end());
        out.insert(out.begin() + static_cast<long>(fixedEnd), ins.begin(),
                   ins.end());
    }
    if (cover != nullptr && !replacedPicture) {
        // 신설 PICTURE — 사슬 말미(자유 순서 그룹 — FLAC 규약).
        AppendFlacBlockHeader(out, false, 6, newPicture.size());
        out.insert(out.end(), newPicture.begin(), newPicture.end());
    }
    if (out.size() < 8) {
        err = "flac structure";  // 방어선(빈 재조립 — 도달 불가)
        return false;
    }
    // is-last 재마킹 — 마지막 블록 헤더(첫 바이트 0x80|type).
    bool marked = false;
    for (size_t pos = 4; pos + 4 <= out.size();) {
        const size_t bsz = (static_cast<size_t>(out[pos + 1]) << 16) |
                           (static_cast<size_t>(out[pos + 2]) << 8) |
                           static_cast<size_t>(out[pos + 3]);
        out[pos] &= 0x7F;  // 이전 표지 소각(재마킹 leg)
        if (pos + 4 + bsz >= out.size()) {
            out[pos] |= 0x80;  // 마지막 블록 표지
            marked = true;
            break;
        }
        pos += 4 + bsz;
    }
    if (!marked) {
        err = "flac structure";  // 방어선(블록 산술 파산 — 도달 불가)
        return false;
    }
    // ---- 원자적 쓰기(D6) — 신설 사슬 + 오디오 원문([chainEnd, EOF)) ----
    const std::streamoff chainEndOff = static_cast<std::streamoff>(chainEnd);
    const std::streamoff copyCount = total - chainEndOff;
    return TagAtomicWrite(
        path, f, out,
        [&](TagFileWriter& w) -> bool {
            return CopyStreamRange(f, w, chainEndOff, copyCount, err);
        },
        {}, err);
}

// ---- Ogg 쓰기 leg (D10 — 코멘트 페이지 교체+뒤페이지 재기입) ----

// 세그먼트 런 lacing 합성: 완성 패킷 = [255]*(S/255)[+(S%255 또는 0)].
inline void PushPacketLacing(std::vector<unsigned char>& lace, size_t sz) {
    const size_t full = sz / 255;
    for (size_t k = 0; k < full; ++k) lace.push_back(255);
    if (full == 0 || sz % 255 != 0)
        lace.push_back(static_cast<unsigned char>(sz % 255));
    else
        lace.push_back(0);  // 정크 크기%255 == 0 — 빈 종결 세그먼트로 마감
}

// Ogg 페이지 1건 조립(27B 헤더+표+페이로드 — CRC 계산 포함).
inline void BuildOggPage(std::vector<unsigned char>& out, unsigned version,
                         unsigned char htype, unsigned long long granule,
                         unsigned int serial, unsigned int seq,
                         const unsigned char* lace, size_t segs,
                         const unsigned char* payload, size_t payloadN) {
    std::vector<unsigned char> page;
    page.reserve(27 + segs + payloadN);
    page.insert(page.end(), "OggS", "OggS" + 4);
    page.push_back(static_cast<unsigned char>(version));
    page.push_back(htype);
    for (int b = 0; b < 8; ++b)
        page.push_back(static_cast<unsigned char>((granule >> (8 * b)) & 0xFF));
    const auto le4 = [&page](unsigned int v) {
        page.push_back(static_cast<unsigned char>(v & 0xFF));
        page.push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
        page.push_back(static_cast<unsigned char>((v >> 16) & 0xFF));
        page.push_back(static_cast<unsigned char>((v >> 24) & 0xFF));
    };
    le4(serial);  // serial — 스트림 동일성 원문
    le4(seq);
    le4(0);  // CRC 자리(0 — 계산 후 기입)
    page.push_back(static_cast<unsigned char>(segs));
    page.insert(page.end(), lace, lace + segs);
    if (payloadN > 0) page.insert(page.end(), payload, payload + payloadN);
    const unsigned int crc = OggCrc(page.data(), page.size());
    page[22] = static_cast<unsigned char>(crc & 0xFF);
    page[23] = static_cast<unsigned char>((crc >> 8) & 0xFF);
    page[24] = static_cast<unsigned char>((crc >> 16) & 0xFF);
    page[25] = static_cast<unsigned char>((crc >> 24) & 0xFF);
    out.insert(out.end(), page.begin(), page.end());
}

// ---- Ogg 재조립 설계(스펙 D10 + 형식 계약): 코멘트 패킷(0x03"vorbis" —
// vorbis 규약의 패킷 2)을 새 패킷으로 교체하고, 코멘트 개시 페이지(C0)부터
// 코멘트 종결 페이지(Ck)까지를 세그먼트 런 단위로 재조립한다 — 패킷 경계는
// lacing value 원문에서 복원(형식 해소 (d): 패킷 경계 불변 재사용). Ck의
// 잔여 세그먼트(트레일 — 셋업 헤더 등 다음 패킷의 본문 원문)는 lacing 원문
// 그대로 신설 뒤페이지에 옮겨 심는다(경계 연속성 — 다음 원본 페이지의
// continuation 플래그와 정합). 그 이후 모든 페이지는 시퀀스 재기입+CRC 재계산
// 으로 원문 복사(granule·lacing 불변 — D10). 배치 계약(libvorbis 표준):
// 코멘트 패킷은 페이지 페이로드 선두에서 새 패킷으로 시작 — 그 외 배치 =
// 정직 거부("ogg structure")·원본 무변경.
//
// T3(#115 D3) — 코멘트 패킷 매agic만 다른 opus("OpusTags" 8B — 마커 바이트
// 없음·코멘트 본문 = vorbis 동형)가 같은 재조립 사슬을 재용한다: 공용 코어가
// leadN/lead(7B/8B)로 갈리고, vorbis leg = 래퍼 원존(시그니처 불변 — #112
// 계약). 벤더 유지·MBP 완전 교체(D8·RebuildVorbisEntries 재용)·코멘트 페이지
// 교체+뒤페이지 재번호+CRC 규약이 opus에도 그대로 성립(조달 0).
inline bool WriteOggCommentTag(const std::string& path, std::ifstream& f,
                               std::streamoff total, const TagSet& t,
                               const CoverArt* cover, bool isOpus,
                               std::string& err) {
    namespace fs = std::filesystem;
    // 코멘트 패킷 매agic — vorbis = [0x03"vorbis"] 7B(패킷 2 규약)·opus =
    // ["OpusTags"] 8B(패킷 2 규약 — RFC 7845, 마커 바이트 없음). 패킷 지목/
    // 수취 오프셋/합성 전부 leadN 계약으로 갈린다.
    const size_t leadN = isOpus ? 8u : 7u;
    const unsigned char* lead =
        isOpus ? reinterpret_cast<const unsigned char*>("OpusTags")
               : reinterpret_cast<const unsigned char*>("\x03" "vorbis");
    f.clear();
    f.seekg(0, std::ios::beg);
    // 코멘트 패킷 보행 — C0 기록(페이로드 선두 개시 계약)+패킷 원문 수취.
    size_t c0Off = 0;
    unsigned char c0Htype = 0, c0Version = 0;
    unsigned long long c0Granule = 0;
    unsigned int c0Serial = 0, c0Seq = 0;
    std::vector<unsigned char> comment;   // 코멘트 패킷 원문(매agic 포함)
    std::vector<unsigned char> trail;     // Ck 페이로드 잔여 원문
    std::vector<unsigned char> trailLace; // 잔여 세그먼트 런 원문
    bool midPacket = false;               // 세그먼트 런이 페이지를 넘는가
    bool found = false;
    size_t walkBytes = 0;
    {
        std::vector<unsigned char> page;
        std::vector<unsigned char> pkt;
        bool pktAligned = false;  // 진행 패킷이 페이지 페이로드 선두 개시?
        while (true) {
            const std::streamoff pgOff = f.tellg();
            if (pgOff < 0) {
                err = "ogg structure";
                return false;
            }
            int r = OggReadPage(f, page);
            if (r == 0) break;  // 코멘트 없이 종료 — 보행 실패
            if (r < 0) {
                err = "ogg structure";
                return false;
            }
            walkBytes += page.size();
            if (walkBytes > kTagWriteMax) {
                err = "tag too large";  // 코멘트 발견 실패(폭주) — 정직 거부
                return false;
            }
            const size_t segs = page[26];
            const unsigned char* lace = page.data() + 27;
            const unsigned char* pay = page.data() + 27 + segs;
            size_t payOff = 0;
            for (size_t s = 0; s < segs; ++s) {
                const size_t lv = lace[s];
                if (!midPacket) {
                    pktAligned = (payOff == 0);
                    pkt.clear();
                }
                if (!found) {
                    pkt.insert(pkt.end(), pay + payOff, pay + payOff + lv);
                    payOff += lv;
                    midPacket = (lv == 255);
                    if (!midPacket) {
                        // 패킷 완성 — 코멘트 패킷 매agic(페이로드 선두 개시
                        // 계약)인가.
                        if (pktAligned && pkt.size() >= leadN &&
                            std::memcmp(pkt.data(), lead, leadN) == 0) {
                            found = true;
                            comment = pkt;  // 코멘트 패킷 원문 수취
                            c0Off = static_cast<size_t>(pgOff);
                            c0Htype = page[5];
                            c0Version = page[4];
                            c0Granule = RdLE64(page.data() + 6);
                            c0Serial = RdLE32(page.data() + 14);
                            c0Seq = RdLE32(page.data() + 18);
                        }
                        pkt.clear();
                    }
                } else {
                    // 코멘트 종결 이후 Ck 잔여 — 트레일(세그먼트 원문+lacing
                    // 값 원문 — 형식 해소 (d)의 패킷 경계 불변 재사용 재료).
                    trail.insert(trail.end(), pay + payOff, pay + payOff + lv);
                    trailLace.push_back(static_cast<unsigned char>(lv));
                    payOff += lv;
                    midPacket = (lv == 255);
                }
            }
            if (found) break;  // Ck — 코멘트 종결 페이지(트레일 수집 완료)
        }
        if (!found) {
            err = "ogg structure";  // 코멘트 패킷 부재/비정렬 — 정직 거부
            return false;
        }
    }
    const std::streamoff ckEnd = f.tellg();  // 보행 종료 위치 = Ck 페이지 끝
    // ---- 신설 코멘트 패킷(벤더 유지 D9 + 관리 키 교체 + D8 그림 필드) ----
    std::string vendor;
    std::vector<VorbisEntry> olds, news;
    if (!ParseVorbisCommentsRaw(comment.data() + leadN, comment.size() - leadN,
                                vendor, olds)) {
        err = "vorbis comment broken";
        return false;
    }
    std::string picMime, oggPicB64;
    const std::string* oggPic = nullptr;
    if (cover != nullptr) {
        if (!ValidateCover(cover, picMime, err)) return false;
        const std::vector<unsigned char> picStruct =
            BuildFlacPictureStruct(picMime, cover->bytes);
        Base64Encode(picStruct.data(), picStruct.size(), oggPicB64);
        oggPic = &oggPicB64;  // 기존 METADATA_BLOCK_PICTURE 전부 소각+신설(D8)
    }
    RebuildVorbisEntries(olds, t, oggPic, news);
    std::vector<unsigned char> newPkt;
    newPkt.reserve(leadN + 8 + vendor.size() + oggPicB64.size());
    newPkt.insert(newPkt.end(), lead, lead + leadN);  // 매agic 원문(8B/7B)
    AppendVorbisCommentBody(newPkt, vendor, news);
    if (newPkt.size() > kTagWriteMax) {
        err = "tag too large";  // 코멘트 폭주(그림 포함 — 정직 거부)
        return false;
    }
    // ---- 재조립 — 신설 페이지 세그먼트 사슬 ----
    // 머리 영역 원문([0, C0) — id/코멘트이전 페이지 보존 복사)+신설 코멘트
    // 페이지+트레일 페이지(lacing 원문).
    std::vector<unsigned char> head1;
    {
        std::vector<unsigned char> prefixHead;
        prefixHead.resize(c0Off);
        f.clear();
        f.seekg(0, std::ios::beg);
        f.read(reinterpret_cast<char*>(prefixHead.data()),
               static_cast<std::streamsize>(prefixHead.size()));
        if (f.gcount() != static_cast<std::streamsize>(prefixHead.size())) {
            err = "audio read failed";  // 재독 실패 — 정직 거부
            return false;
        }
        head1.swap(prefixHead);  // [0, C0) 원문 선행
    }
    // [신설 코멘트 lacing(완성 런)] ++ [트레일 lacing 원문(패킷 경계 계약 (d))].
    std::vector<unsigned char> laceAll, payloadAll;
    PushPacketLacing(laceAll, newPkt.size());
    laceAll.insert(laceAll.end(), trailLace.begin(), trailLace.end());
    payloadAll = newPkt;
    payloadAll.insert(payloadAll.end(), trail.begin(), trail.end());
    // 페이지 포장(255 세그/페이지 — 런이 페이지를 넘으면 다음 페이지 htype에
    // continuation 비트 0x01을 세운다; Ogg 세그먼트 런 규약).
    size_t segI = 0, byteOff = 0;
    unsigned int pageCnt = 0;
    while (segI < laceAll.size()) {
        const size_t take = laceAll.size() - segI > 255 ? 255
                                                        : laceAll.size() - segI;
        size_t plen = 0;
        for (size_t k = 0; k < take; ++k) plen += laceAll[segI + k];
        unsigned char htype = 0;
        if (segI == 0) {
            htype = static_cast<unsigned char>(c0Htype & ~0x01u);  // 신설 개시
        } else if (laceAll[segI - 1] == 255) {
            htype = 0x01;  // 런이 페이지를 넘었다 — continuation
        }
        BuildOggPage(head1, c0Version, htype, c0Granule, c0Serial,
                     c0Seq + pageCnt, laceAll.data() + segI, take,
                     payloadAll.data() + byteOff, plen);
        ++pageCnt;
        segI += take;
        byteOff += plen;
    }
    // ---- 원자적 쓰기(D6) + 뒤페이지 재기입(D10) ----
    return TagAtomicWrite(
        path, f, head1,
        [&](TagFileWriter& w) -> bool {
            f.clear();
            f.seekg(ckEnd, std::ios::beg);  // 보행 종료 지점부터 재기입 복사
            // Ck 이후 모든 페이지 — 시퀀스 재기입+CRC 재계산, granule·
            // lacing·payload 원문(granule 불변 — D10). 파산 도달 = 잔여
            // 원문 복사(최소 파괴 원칙 — 알 수 없는 꼬리는 건드리지 않는다).
            std::vector<unsigned char> pg;
            unsigned int pageJ = 0;
            while (true) {
                const std::streamoff pgOff = f.tellg();
                const int r = OggReadPage(f, pg);
                if (r == 0) return true;  // 깨끗한 종료
                if (r < 0) {  // 비페이지 잔여 — 원문 복사(건드리지 않는다)
                    return CopyStreamRange(f, w, pgOff,
                                           static_cast<std::streamoff>(total) -
                                               pgOff,
                                           err);
                }
                const unsigned int sq =
                    c0Seq + pageCnt + static_cast<unsigned int>(pageJ++);
                pg[18] = static_cast<unsigned char>(sq & 0xFF);
                pg[19] = static_cast<unsigned char>((sq >> 8) & 0xFF);
                pg[20] = static_cast<unsigned char>((sq >> 16) & 0xFF);
                pg[21] = static_cast<unsigned char>((sq >> 24) & 0xFF);
                pg[22] = pg[23] = pg[24] = pg[25] = 0;  // CRC 자리 소각(규약)
                const unsigned int crc = OggCrc(pg.data(), pg.size());
                pg[22] = static_cast<unsigned char>(crc & 0xFF);
                pg[23] = static_cast<unsigned char>((crc >> 8) & 0xFF);
                pg[24] = static_cast<unsigned char>((crc >> 16) & 0xFF);
                pg[25] = static_cast<unsigned char>((crc >> 24) & 0xFF);
                if (!w.WriteAll(pg.data(), pg.size())) {
                    err = "tmp write failed";
                    return false;
                }
            }
        },
        {}, err);
}

inline bool WriteOggTag(const std::string& path, std::ifstream& f,
                        std::streamoff total, const TagSet& t,
                        const CoverArt* cover, std::string& err) {
    return WriteOggCommentTag(path, f, total, t, cover, false, err);
}

// ---- opus 쓰기 leg (D3 — OpusTags 패킷 교체·D10 재조립 사슬 재용) ----

// 코멘트 패킷 매agic = "OpusTags" 8B인 Ogg 재조립 — 그 밖 계약 전부 vorbis
// leg 원존(코멘트 페이지 교체+재번호 — D10 원존 계약·벤더 유지 D9·MBP = D8
// 원존 완전 교체 흡수 — RebuildVorbisEntries 재용). 페이지 페이로드 선두
// 개시 계약 = RFC 7845(식별/코멘트 헤더는 각각 자기 페이지 — 코멘트 페이지
// htype의 EOS 표지 = c0Htype 원문 유지 — 비선두 배치 = 정직 거부).
inline bool WriteOpusTag(const std::string& path, std::ifstream& f,
                         std::streamoff total, const TagSet& t,
                         const CoverArt* cover, std::string& err) {
    return WriteOggCommentTag(path, f, total, t, cover, true, err);
}

// ---- WAV 쓰기 leg ("id3 " 청크 교체/신설 + RIFF 크기 재계산) ----

// RIFF("RIFF"+32LE 크기+"WAVE")의 청크 연쇄에서 "id3 " 청크(대소문자 무시 —
// 읽기 ParseWavTags 규약 그대로)를 교체(부재 = 말미 신설 — 형식 해소 (c):
// 기존 청크 순서 보존·RIFF/청크 크기 동시 재계산). 기존 id3 청크의 태그는
// ReadId3Tag → BuildId3V23Body 재용(mp3 leg 동형 — 관리 프레임 교체+그 외
// 원문 보존), 복수 id3 청크 = 첫 것만 교체·나머지 소각(D8 완전 교체 정신).
// 청크 패드(홀수)·payload는 원문 그대로 복사한다(청크 페이로드 변조 금지).
// 패딩 = 0(임베디드 청크 — 정확 크기 재계산이 진실원, mp3의 1KB 패딩과 다른
// 사정: RIFF 크기·다음 청크 오프셋 계산이 정확해야 한다).
inline bool WriteWavTag(const std::string& path, std::ifstream& f,
                        std::streamoff total, const TagSet& t,
                        const CoverArt* cover, std::string& err) {
    namespace fs = std::filesystem;
    struct ChunkRec {
        std::streamoff off;
        unsigned int csz;
        unsigned char id[4];
    };
    std::vector<ChunkRec> chunks;
    int id3Idx = -1;  // 첫 "id3 " 청크 자리(대소문자 무시 — 읽기 규약 그대로)
    {
        std::streamoff pos = 12;
        while (pos + 8 <= total) {
            if (chunks.size() > 1024) {
                err = "riff structure";  // 폭주 절단 — 정직 거부
                return false;
            }
            unsigned char ch[8];
            f.clear();
            f.seekg(pos, std::ios::beg);
            f.read(reinterpret_cast<char*>(ch), 8);
            if (f.gcount() != 8) {
                err = "riff structure";
                return false;
            }
            const unsigned int csz = RdLE32(ch + 4);
            if (pos + 8 + csz > total) {
                err = "chunk truncated";  // 선언이 파일을 넘는다 — 정직 거부
                return false;
            }
            const bool isId3 =
                FoldAscii(static_cast<char>(ch[0])) == 'i' &&
                FoldAscii(static_cast<char>(ch[1])) == 'd' &&
                FoldAscii(static_cast<char>(ch[2])) == '3';
            const int idx = static_cast<int>(chunks.size());
            if (isId3 && id3Idx < 0) id3Idx = idx;
            ChunkRec c;
            c.off = pos;
            c.csz = csz;
            std::memcpy(c.id, ch, 4);
            chunks.push_back(c);
            pos += 8 + static_cast<std::streamoff>(csz) + (csz & 1);
        }
        if (pos != total) {
            err = "riff structure";  // 사슬 종료 후 온전한 청크 헤더 미만 잔여
            return false;
        }
    }
    // 새 "id3 " 청크 조립 — 기존 태그 유지(mp3 leg 동형 사슬 재용).
    Id3TagInfo old;
    bool hasOld = false;
    // 기존 id3(첫 것) 페이로드 수취(조건 read — ≤ kTagWriteMax). 부재 =
    // 신설(기존 nullptr leg — 말미 신설).
    if (id3Idx >= 0) {
        {
            const ChunkRec& c = chunks[id3Idx];
            if (c.csz > kTagWriteMax) {
                err = "tag too large";
                return false;
            }
            std::vector<unsigned char> pay(c.csz);
            f.clear();
            f.seekg(c.off + 8, std::ios::beg);
            f.read(reinterpret_cast<char*>(pay.data()),
                   static_cast<std::streamsize>(c.csz));
            if (f.gcount() != static_cast<std::streamsize>(c.csz)) {
                err = "chunk truncated";
                return false;
            }
            if (!ReadId3Tag(pay.data(), pay.size(), old, err)) return false;
            hasOld = true;
        }
    }
    std::vector<unsigned char> body;
    if (!BuildId3V23Body(hasOld ? &old : nullptr, t, cover, body, err))
        return false;
    std::vector<unsigned char> tag;
    if (!AssembleId3V23Tag(body, 0, tag, err)) return false;
    std::vector<unsigned char> id3Chunk;
    id3Chunk.insert(id3Chunk.end(), "id3 ", "id3 " + 4);
    WrLE32(id3Chunk, static_cast<unsigned int>(tag.size()));
    id3Chunk.insert(id3Chunk.end(), tag.begin(), tag.end());
    if (tag.size() & 1) id3Chunk.push_back(0);  // 홀수 패드(청크 총계 산출용)
    // RIFF 크기 재계산(신설/교체·복수 id3 소각 반영 — 산출이 기입 leg와
    // 같은 걸음: 보존 청크+첫 id3 자리의 신설 청크·그 밖 id3 소각).
    unsigned long long riffSize = 4;
    unsigned long long id3ChunkTotal = id3Chunk.size();  // 패드 포함 총계
    const bool id3InPlace = id3Idx >= 0;  // 기존 자리 교체(true) vs 말미 신설
    for (size_t i = 0; i < chunks.size(); ++i) {
        const ChunkRec& c = chunks[i];
        const bool isId3 = FoldAscii(static_cast<char>(c.id[0])) == 'i' &&
                           FoldAscii(static_cast<char>(c.id[1])) == 'd' &&
                           FoldAscii(static_cast<char>(c.id[2])) == '3';
        if (isId3) {
            if (static_cast<int>(i) == id3Idx)
                riffSize += id3ChunkTotal;  // 첫 id3 — 신설 청크로 교체(헤더 포함 총계)
            continue;  // 복수 id3 = 소각
        }
        riffSize += 8 + c.csz + (c.csz & 1);
    }
    if (!id3InPlace) riffSize += id3ChunkTotal;  // 말미 신설(헤더 포함 총계)
    if (riffSize > 0xFFFFFFFEull) {
        err = "tag too large";  // RIFX 도달 — 32비트 RIFF 한계 정직 거부
        return false;
    }
    // ---- 원자적 쓰기(D6) — 헤더 재기입+청크 보존 복사+신설 청크 ----
    const std::string tmp =
        path + "." + std::to_string(ThisProcessId()) + ".tagtmp";
    {
        TagFileWriter w;
        if (!w.Open(fs::path(tmp))) {
            err = "tmp open failed";
            return false;
        }
        const auto fail = [&](const char* e) {
            w.Close();
            std::error_code rec;
            fs::remove(fs::path(tmp), rec);
            err = e;
            return false;
        };
        unsigned char riffHdr[12];
        riffHdr[0] = 'R';
        riffHdr[1] = 'I';
        riffHdr[2] = 'F';
        riffHdr[3] = 'F';
        const unsigned int rs = static_cast<unsigned int>(riffSize);
        riffHdr[4] = static_cast<unsigned char>(rs & 0xFF);
        riffHdr[5] = static_cast<unsigned char>((rs >> 8) & 0xFF);
        riffHdr[6] = static_cast<unsigned char>((rs >> 16) & 0xFF);
        riffHdr[7] = static_cast<unsigned char>((rs >> 24) & 0xFF);
        riffHdr[8] = 'W';
        riffHdr[9] = 'A';
        riffHdr[10] = 'V';
        riffHdr[11] = 'E';
        if (!w.WriteAll(riffHdr, 12)) return fail("tmp write failed");
        for (size_t i = 0; i < chunks.size(); ++i) {
            const ChunkRec& c = chunks[i];
            const bool isId3 = FoldAscii(static_cast<char>(c.id[0])) == 'i' &&
                               FoldAscii(static_cast<char>(c.id[1])) == 'd' &&
                               FoldAscii(static_cast<char>(c.id[2])) == '3';
            if (isId3) {
                if (static_cast<int>(i) == id3Idx &&
                    !w.WriteAll(id3Chunk.data(), id3Chunk.size()))
                    return fail("tmp write failed");  // 신설 청크 교체 기입
                continue;  // 그 밖 id3 = 소각
            }
            std::string cerr;
            if (!CopyStreamRange(f, w, c.off,
                                 8 + static_cast<std::streamoff>(c.csz) +
                                     (c.csz & 1), cerr))
                return fail(cerr.c_str());
        }
        if (!id3InPlace && !w.WriteAll(id3Chunk.data(), id3Chunk.size()))
            return fail("tmp write failed");  // 말미 신설
        if (!w.FlushToDisk()) return fail("fsync failed");  // D6 fsync 계약
        w.Close();
    }
    f.close();  // 원본 핸들 해제 — Windows rename 교체의 공유 핸들 충돌 소각
    return TagReplaceRename(tmp, path, err);
}

// ---- MP4/M4A 쓰기 leg (D2 — ilst 재작성+box 사이즈 전파 재계산·mdat 원문
// 바이트 무이동) ----

// MP4 box 조립(읽기 Mp4BoxWalk의 쌍둥이: [4B size(BE) = 8+body][4B type]
// +body). 32bit size — 태그 leg 상한 kTagWriteMax(16MiB) 안쪽 귀속이라
// largesize 불요(도달 불가 — 폭주 방어선 = 호출부 "tag too large").
inline void PushMp4Box(std::vector<unsigned char>& out, const char* type4,
                       const std::vector<unsigned char>& body) {
    const unsigned int sz = static_cast<unsigned int>(8 + body.size());
    for (int sh = 24; sh >= 0; sh -= 8)
        out.push_back(static_cast<unsigned char>((sz >> sh) & 0xFF));
    out.insert(out.end(), type4, type4 + 4);
    out.insert(out.end(), body.begin(), body.end());
}

// ilst 항목의 data atom 합성 — [4B version/flags][4B locale 0][값] — flags
// 하위 바이트 = 타입 코드(읽기 DecodeM4aValue 접점 원존): 1 = UTF-8(관리
// 텍스트 3필드의 쓰기 정규 표기 — re-독 = DecodeM4aValue 통과 원존)·
// 13 = JPEG·14 = PNG·0 = binary(그 밖 image/*의 covr — 읽기 매agic 스니프
// 원존 재용).
inline std::vector<unsigned char> BuildM4aDataBox(unsigned int code,
                                                   const unsigned char* v,
                                                   size_t vN) {
    std::vector<unsigned char> b;
    b.push_back(0);  // version 0
    b.push_back(0);  // flags
    b.push_back(0);
    b.push_back(static_cast<unsigned char>(code));  // flags 하위 바이트
    b.insert(b.end(), 4, 0);  // locale reserved
    b.insert(b.end(), v, v + vN);
    std::vector<unsigned char> box;
    PushMp4Box(box, "data", b);
    return box;
}

// hdlr 합성(mdir — 25B: ver/flags 4 + pre_defined 4 + 'mdir' + reserved 12
// + name NUL 1) — 신설 폴백 leg 부품(읽기 접점 = payload [8..12) 'mdir'
// 원존 — ReadCover 계열과 같은 좌표).
inline std::vector<unsigned char> BuildM4aHdlrBody() {
    std::vector<unsigned char> b(25, 0);
    const unsigned char mdir[4] = {'m', 'd', 'i', 'r'};
    std::memcpy(b.data() + 8, mdir, 4);
    return b;
}

// covr data 타입 코드(mime 진실원 — iTunes 규약 원존 승계): jpeg = 13·
// png = 14·그 밖 image/* = 0(binary — 읽기가 이미지 매agic 스니프로 되갚는다
// — ParseMp4Covr 계약 원존).
inline unsigned int M4aCovrCode(const std::string& mime) {
    if (mime == "image/jpeg") return 13;
    if (mime == "image/png") return 14;
    return 0;
}

// 새 ilst 본문 조립 — 구 ilst(p 기준 ilstBase..+ilstN — 신설 = p nullptr)
// 자식 보행: 관리 항목(©nam/©ART/©alb) = 첫 자리 교체(자리 유지 — vorbis leg
// RebuildVorbisEntries 동형 계약), 빈 새 값 = 항목 소각(비움 쓰기 — T1 결정
// 3 수형), 이후 동일 관리 항목 = 소각(첫 값 계약 — 읽기 사슬 동형). 미관리
// 항목(trkn/gnre/cprt…) = box 원문 보존 — covr = cover 지정 시 전부 소각+
// 관리 항목 뒤 신설(D4/D8 완전 교체), null = 원문 유지. 관리 항목 미성립 =
// 관리 순서(©nam/©ART/©alb) 신설. data atom = 타입 코드 1(UTF-8).
// 파산(잘린 ilst 자식) = false — 원본 무변경 정직 거부.
inline bool BuildM4aIlst(const unsigned char* p, size_t ilstBase,
                         size_t ilstN, const TagSet& t, const CoverArt* cover,
                         const std::string& covrMime,
                         std::vector<unsigned char>& out, std::string& err) {
    static const char* kManaged[3] = {"\xA9" "nam", "\xA9" "ART",
                                      "\xA9" "alb"};
    const std::string* vals[3] = {&t.title, &t.artist, &t.album};
    bool saw[3] = {false, false, false};
    out.clear();
    if (p != nullptr) {
        size_t off = ilstBase;
        const size_t end = ilstBase + ilstN;
        Mp4BoxWalk item;
        while (off + 8 <= end) {
            const size_t itemAt = off;
            if (!Mp4BoxAt(p, end, off, item)) {
                err = "mp4 structure";  // ilst 자식 파산 — 정직 거부
                return false;
            }
            int slot = -1;
            for (int i = 0; i < 3 && slot < 0; ++i)
                if (std::memcmp(item.type, kManaged[i], 4) == 0) slot = i;
            if (slot >= 0) {
                if (saw[slot] || vals[slot]->empty()) continue;  // 소각
                saw[slot] = true;
                PushMp4Box(out, kManaged[slot],
                           BuildM4aDataBox(
                               1,
                               reinterpret_cast<const unsigned char*>(
                                   vals[slot]->data()),
                               vals[slot]->size()));
                continue;
            }
            if (std::memcmp(item.type, "covr", 4) == 0 && cover != nullptr)
                continue;  // D8 — covr 전부 소각(말미 신설로 대체)
            out.insert(out.end(), p + itemAt, p + off);  // 원문 보존(covr
                                                         // 유지 포함)
        }
    }
    for (int i = 0; i < 3; ++i) {
        if (saw[i] || vals[i]->empty()) continue;
        PushMp4Box(out, kManaged[i],
                   BuildM4aDataBox(
                       1, reinterpret_cast<const unsigned char*>(
                              vals[i]->data()),
                       vals[i]->size()));
    }
    if (cover != nullptr) {
        PushMp4Box(out, "covr",
                   BuildM4aDataBox(M4aCovrCode(covrMime), cover->bytes.data(),
                                   cover->bytes.size()));
    }
    return true;
}

// 컨테이너(box) 자식 사슬 재조립(offset 진실원): 원문 자식 box 복사에서
// matchBody(payload 선두 offset — Mp4TagLoc 좌표와 같은 buffer) 일치 첫 자리
// = newChild 교체(자리 유지 — RebuildVorbisEntries 동형). matchBody = 0 =
// 교체 없음 — newChild = 말미 신설 부품(부재 부모 순차 신설 규약 — D2).
// 복수 일치 = 첫 것만 교체·나머지 원문(복수 ilst/udta의 규약 부재 — 원존
// 보존). 파산(잘린 자식 box) = false — 원본 무변경 정직 거부.
inline bool Mp4ReshuffleChildren(const unsigned char* p, size_t base,
                                 size_t span, size_t matchBody,
                                 const std::vector<unsigned char>& newChild,
                                 std::vector<unsigned char>& out,
                                 std::string& err) {
    size_t off = base;
    const size_t end = base + span;
    bool replaced = false;
    Mp4BoxWalk b;
    while (off + 8 <= end) {
        const size_t itemAt = off;
        if (!Mp4BoxAt(p, end, off, b)) {
            err = "mp4 structure";  // 자식 파산 — 정직 거부(원본 무변경)
            return false;
        }
        if (!replaced && matchBody != 0 && b.body == matchBody) {
            out.insert(out.end(), newChild.begin(), newChild.end());
            replaced = true;
            continue;  // 구 자식 소각(신설 box로 교체)
        }
        out.insert(out.end(), p + itemAt, p + off);
    }
    if (matchBody != 0 && !replaced) {
        err = "mp4 structure";  // 지목 좌표 소실(도달 불가 방어선 — 좌표 원존)
        return false;
    }
    if (matchBody == 0 && !newChild.empty())
        out.insert(out.end(), newChild.begin(), newChild.end());  // 말미 신설
    return true;
}

// ---- stco/co64 청크 오프셋 패치 (T3 fix r1 — moov Δ 이동·C1 수형) ----
//
// moov 재조립이 사이즈를 바꾸면(Δ = 신구 moov 차) moov 뒤 box(mdat —
// faststart 계열)가 Δ 이동하고, moov 내 stco(co64) 청크 오프셋 = mdat 내
// 절대 파일 오프셋이라 Δ 만큼 같이 이동해야 한다(외부 플레이어 재생 인덱스
// 계약 — 태그 자체 재독의 블라인드 스팟 — C1 원문). 표준 수형 = stco
// (32BE)/co64(64BE) 항목 Δ 가산 패치. 보행 = 표준 컨테이너 사슬 한정
// (trak→mdia→minf→stbl — depth 0-3 재귀) — ilst payload 등 임의 데이터를
// 컨테이너로 오독하지 않는 좁은 사슬(covr binary 내 우연 매agic 봉인).
// entries가 전부 패치 가능(0 ≤ 항목+Δ ≤ 상한)이면 true·하나라도 넘치면
// false — 호출부 = free 흡수 보조가 없으면 "free 부족" 정직 거부(③ — 원본
// 무변경·수정 없음 원존). stco/co64 부재 = found false+true(청크 오프셋
// 참조 부재 moov — 최소 파괴 원칙 진행 — 재량 원장).
inline bool PatchMp4ChunkOffsetsIn(std::vector<unsigned char>& b, size_t base,
                                   size_t span, long long delta, int depth,
                                   bool& found) {
    static const char* kChain[4] = {"trak", "mdia", "minf", "stbl"};
    size_t off = base;
    const size_t end = base + span;
    int guard = 0;
    while (off + 8 <= end && guard++ < 4096) {
        const unsigned char* q = b.data() + off;
        unsigned long long total = RdBE32(q);
        size_t head = 8;
        if (total == 1) {  // 64bit largesize — 읽기 규약 원존
            if (off + 16 > end) return false;
            total = 0;
            for (int i = 0; i < 8; ++i) total = (total << 8) | q[8 + i];
            head = 16;
        }
        if (total < head || off + total > end)
            return false;  // 파산 방어선(출처 = 검증된 재조립 — 도달 불가)
        const size_t bodyAt = off + head;
        const size_t bodyN = static_cast<size_t>(total - head);
        const bool isStco = std::memcmp(q + 4, "stco", 4) == 0;
        const bool isCo64 = std::memcmp(q + 4, "co64", 4) == 0;
        if (depth < 4 &&
            std::memcmp(q + 4, kChain[depth], 4) == 0) {
            // trak→mdia→minf→stbl 사슬 재귀 — stco/co64는 최심(depth 4) 아래.
            if (!PatchMp4ChunkOffsetsIn(b, bodyAt, bodyN, delta, depth + 1,
                                        found))
                return false;
        } else if (depth == 4 && (isStco || isCo64)) {
            found = true;
            if (bodyN < 8) return false;
            const unsigned int cnt = RdBE32(b.data() + bodyAt + 4);
            const size_t esz = isStco ? 4 : 8;  // stco = 4BE·co64 = 8BE
            if (8 + static_cast<unsigned long long>(esz) * cnt > bodyN)
                return false;  // entries 파산(선언 > box) — 방어선
            for (unsigned int i = 0; i < cnt; ++i) {
                const size_t at = bodyAt + 8 + static_cast<size_t>(esz) * i;
                unsigned long long v = 0;
                for (size_t k = 0; k < esz; ++k) v = (v << 8) | b[at + k];
                // Δ = moov 크기 차(±수 MB 귀속)·v ≤ 파일 크기(2^63 실역) —
                // signed 산술 안전. 32bit는 상한 0xFFFFFFFF 추가 계약.
                const long long s = static_cast<long long>(v) + delta;
                if (s < 0 || (isStco && s > 0xFFFFFFFFll))
                    return false;  // 넘침 — 호출부 정직 거부(③)
                const unsigned long long nv =
                    static_cast<unsigned long long>(s);
                for (size_t k = 0; k < esz; ++k)
                    b[at + k] =
                        static_cast<unsigned char>(nv >> (8 * (esz - 1 - k)));
            }
        }
        off += static_cast<size_t>(total);
    }
    return true;
}

inline bool PatchMp4ChunkOffsets(std::vector<unsigned char>& moovBody,
                                 long long delta, bool& found) {
    found = false;
    return PatchMp4ChunkOffsetsIn(moovBody, 0, moovBody.size(), delta, 0,
                                  found);
}
// ---- m4a 쓰기 leg (D2 — moov/udta/meta/ilst 재작성) ----
//
// 최상위 box 연쇄에서 moov만 재조립하고 그 밖 box(ftyp/free/mdat…)는 원문
// 그대로 스트림 복사한다 — mdat(오디오 raw) 데이터는 바이트 원존(무이동·
// 변조 0 — moov 위치가 mdat 앞이면 mdat 헤더만 앞당겨진다 = 데이터 무이동·
// 뒤면 무변경 — D2 계약). box 사이즈 전파 = 재조립 트리(moov/udta/meta/
// ilst)의 실측 사이즈 재기입 — 자식→부모 순서로 box 조립이라 자동 성립.
// free atom = 원문 보존(흡수 — 재조립 트리가 자체 사이즈 전파로 성립하므로
// 패드 재계산 불요 원존 계약). 신설 폴백 = 부재 부모 box 순차 신설(udta
// 부재 → moov children 말미·meta 부재 → udta 말미·ilst 부재 → meta 말미·
// hdlr 부재 = hdlr(mdir)+ilst 동봉 신설 — Mp4TagStage 계보). moov 부재(트리
// 루트 자의 신설 봉인)·비-'mdir' hdlr('mdta' 등 타 핸들러 표기 침해 봉인) =
// "mp4 structure" 정직 거부·원본 무변경. 커버 = covr 항목 교체(D4 — null =
// 기존 유지·지정 = 전부 소각+신설 — 읽기 ParseMp4Covr round-trip 계약).
// 원자적 쓰기 = TagAtomicWrite 사다리 재용(D6 — 원존 무손상·fsync·rename).
inline bool WriteM4aTag(const std::string& path, std::ifstream& f,
                        std::streamoff total, const TagSet& t,
                        const CoverArt* cover, std::string& err) {
    namespace fs = std::filesystem;
    std::string covrMime;
    if (!ValidateCover(cover, covrMime, err)) return false;
    if (cover != nullptr && cover->bytes.size() > kTagWriteMax) {
        err = "tag too large";  // covr 상한 = 16MiB 원존(kTagWriteMax 재용)
        return false;
    }
    // ---- 최상위 box 보행(선언을 믿지 않는 정직 계약 — Mp4BoxAt 원존 수형) ----
    std::streamoff moovStart = 0, moovEnd = 0;
    size_t moovHead = 8;
    bool sawMoov = false;
    // moov 뒤 구조 수취(fix r1 — Δ 이동 계약): moov 다음 box(free/skip —
    // 흡수 후보)와 moov 뒤 mdat(Δ 절대 이동 대상).
    bool nextSeen = false, nextIsFree = false;
    char nextType[4] = {0, 0, 0, 0};
    unsigned long long nextTotal = 0;
    bool mdatAfter = false;
    {
        std::streamoff off = 0;
        int guard = 0;
        while (off + 8 <= total) {
            if (++guard > 4096) {
                err = "mp4 structure";  // 폭주 절단(읽기 보행 상한 같은 계보)
                return false;
            }
            unsigned char h[16];
            f.clear();
            f.seekg(off, std::ios::beg);
            f.read(reinterpret_cast<char*>(h), 8);
            if (f.gcount() != 8) {
                err = "mp4 structure";  // 잘린 box 헤더 — 정직 거부
                return false;
            }
            unsigned long long decl = RdBE32(h);
            size_t head = 8;
            if (decl == 1) {  // 64bit largesize — 읽기 규약 원존
                f.read(reinterpret_cast<char*>(h + 8), 8);
                if (f.gcount() != 8) {
                    err = "mp4 structure";
                    return false;
                }
                decl = 0;
                for (int i = 0; i < 8; ++i)
                    decl = (decl << 8) | h[8 + i];
                head = 16;
            }
            std::streamoff end;
            if (decl == 0) {
                end = total;  // size=0 — 남은 영역 말미(마지막 박스 규약)
            } else {
                if (decl < head) {
                    err = "mp4 structure";  // 배신적 size(선언 < 헤더)
                    return false;
                }
                end = off + static_cast<std::streamoff>(decl);
            }
            if (end > total) {
                err = "mp4 truncated";  // 잘린 box — 정직 거부
                return false;
            }
            if (!sawMoov && std::memcmp(h + 4, "moov", 4) == 0) {
                sawMoov = true;  // 첫 moov만(재조립 대상 — Mp4FindBox 원존)
                moovStart = off;
                moovEnd = end;
                moovHead = head;
            }
            if (sawMoov && off >= moovEnd) {  // moov 뒤 box 보행 — fix r1
                if (!nextSeen) {
                    nextSeen = true;
                    nextIsFree = std::memcmp(h + 4, "free", 4) == 0 ||
                                 std::memcmp(h + 4, "skip", 4) == 0;
                    std::memcpy(nextType, h + 4, 4);
                    nextTotal = static_cast<unsigned long long>(end - off);
                }
                if (std::memcmp(h + 4, "mdat", 4) == 0) mdatAfter = true;
            }
            if (end <= off) {
                err = "mp4 structure";  // 방어선(0 길이 box 사슬 — 폭주 봉인)
                return false;
            }
            off = end;
        }
        if (off != total) {
            err = "mp4 truncated";  // 사슬 종료 후 온전한 box 헤더 미만 잔여
            return false;
        }
        if (!sawMoov) {
            err = "mp4 structure";  // moov 부재 — 신설 불가(D2 fail-closed)
            return false;
        }
    }
    // ---- moov 본문 수취(상한 kTagWriteMax — 커버 블롭 포함 재조립 버퍼) ----
    const unsigned long long moovBodyN =
        static_cast<unsigned long long>(moovEnd - moovStart) - moovHead;
    if (moovBodyN > kTagWriteMax) {
        err = "tag too large";  // 폭주 절단 — 정직 거부
        return false;
    }
    std::vector<unsigned char> mb(static_cast<size_t>(moovBodyN));
    f.clear();
    f.seekg(moovStart + static_cast<std::streamoff>(moovHead), std::ios::beg);
    f.read(reinterpret_cast<char*>(mb.data()),
           static_cast<std::streamsize>(mb.size()));
    if (f.gcount() != static_cast<std::streamsize>(mb.size())) {
        err = "mp4 truncated";  // 재독 실패 — 정직 거부
        return false;
    }
    // ---- 트리 지목(읽기 사슬 원존 재용 — 신설 폴백 계보 판정) ----
    Mp4TagLoc loc;
    const Mp4TagStage stage =
        Mp4LocateTagTree(mb.data(), 0, mb.size(), loc);
    if (stage == kMp4TagBadHdlr) {
        err = "mp4 structure";  // 비-metadata hdlr 표기 침해 봉인 — fail-closed
        return false;
    }
    // ---- 신설 ilst(구 항목 원존 보존+관리 교체 — 성립 경로만) ----
    std::vector<unsigned char> newIlst;
    if (!BuildM4aIlst(stage == kMp4TagOk ? mb.data() : nullptr,
                      stage == kMp4TagOk ? loc.ilstBody : 0,
                      stage == kMp4TagOk ? loc.ilstN : 0, t, cover, covrMime,
                      newIlst, err))
        return false;
    std::vector<unsigned char> ilstBox;
    PushMp4Box(ilstBox, "ilst", newIlst);
    // ---- meta/udta 재조립·신설(단계별 규약 — 위 설계 주석 계보) ----
    std::vector<unsigned char> metaBox, newUdta;
    if (stage == kMp4TagOk || stage == kMp4TagNoIlst || stage == kMp4TagNoHdlr) {
        // 재조립 — meta ver/flags 원문 + 자식 사슬(ilst 교체·부재 = 말미 신설).
        std::vector<unsigned char> blob;
        if (stage == kMp4TagNoHdlr) {
            PushMp4Box(blob, "hdlr", BuildM4aHdlrBody());  // hdlr 동봉 신설
            blob.insert(blob.end(), ilstBox.begin(), ilstBox.end());
        } else {
            blob = ilstBox;
        }
        std::vector<unsigned char> metaBody, udtaBody;
        if (!Mp4ReshuffleChildren(mb.data(), loc.metaBody + 4, loc.metaN - 4,
                                  stage == kMp4TagOk ? loc.ilstBody : 0, blob,
                                  metaBody, err))
            return false;
        metaBody.insert(metaBody.begin(), mb.begin() + static_cast<std::ptrdiff_t>(loc.metaBody),
                        mb.begin() + static_cast<std::ptrdiff_t>(loc.metaBody + 4));
        PushMp4Box(metaBox, "meta", metaBody);
        if (!Mp4ReshuffleChildren(mb.data(), loc.udtaBody, loc.udtaN,
                                  loc.metaBody, metaBox, udtaBody, err))
            return false;
        PushMp4Box(newUdta, "udta", udtaBody);
    } else {
        // 신설(NoMeta/NoUdta) — meta full box 신설: [ver/flags 0][hdlr mdir]
        // +[ilst]·udta 부재 = 메타 box 단일 자식 udta.
        std::vector<unsigned char> metaBody(4, 0);  // version/flags 0 — 신설
        PushMp4Box(metaBody, "hdlr", BuildM4aHdlrBody());
        PushMp4Box(metaBody, "ilst", newIlst);
        PushMp4Box(metaBox, "meta", metaBody);
        if (stage == kMp4TagNoMeta) {
            std::vector<unsigned char> udtaBody;
            if (!Mp4ReshuffleChildren(mb.data(), loc.udtaBody, loc.udtaN, 0,
                                      metaBox, udtaBody, err))
                return false;
            PushMp4Box(newUdta, "udta", udtaBody);
        } else {
            PushMp4Box(newUdta, "udta", metaBox);
        }
    }
    // ---- moov 재조립 — box 사이즈 전파(udta 교체/말미 신설) ----
    std::vector<unsigned char> moovBody;
    if (!Mp4ReshuffleChildren(mb.data(), 0, mb.size(),
                              stage != kMp4TagNoUdta ? loc.udtaBody : 0,
                              newUdta, moovBody, err))
        return false;
    // ---- mdat 절대 위치 계약(fix r1 — C1 수형) — moov Δ 이동 정합 ----
    // moov 재조립 Δ = 신구 moov box 크기 차. moov 뒤 mdat가 있으면(faststart
    // 계열) 그 절대 위치가 Δ 이동 — ①free 흡수(moov 다음 free/skip box가 Δ를
    // 삼키면 mdat 자리 고정 — 파일 바이트 총량 불변·절대 오프셋 무패치·
    // Δ>0 = payload 축소(부족 = 거부 leg로), Δ<0 = 확장)·②표준 패치(stco/
    // co64 항목 Δ 가산 — 넘침 = "free 부족" 정직 거부 — ③ 원존 무손상 계약).
    // moov가 마지막 box(mdat 뒤 배치) = mdat 무이동 — 패치 불요(조건부 경로
    // — 리포트 근거 원존). 청크 오프셋 참조 부재(stco/co64 부재) = 무패치
    // 진행(최소 파괴 — 재량 원장).
    const long long delta =
        static_cast<long long>(moovBody.size() + 8) -
        static_cast<long long>(moovEnd - moovStart);
    bool freeAbsorb = false;
    if (delta != 0 && mdatAfter) {
        if (nextSeen && nextIsFree && nextTotal >= 8 &&
            (delta > 0
                 ? static_cast<unsigned long long>(delta) <= nextTotal - 8
                 : static_cast<long long>(nextTotal) - delta <= 0xFFFFFFFFll)) {
            freeAbsorb = true;  // ① — free/skip가 Δ를 삼켜 mdat 고정
        } else {
            bool found = false;
            if (!PatchMp4ChunkOffsets(moovBody, delta, found)) {
                err = "free 부족";  // ② 넘침+① 불가 — 정직 거부(③ — 수정 없음)
                return false;
            }
        }
    }
    std::vector<unsigned char> newMoov;
    PushMp4Box(newMoov, "moov", moovBody);
    if (newMoov.size() > kTagWriteMax) {
        err = "tag too large";  // 재조립 moov 폭주 — 정직 거부
        return false;
    }
    // ---- 원자적 쓰기(D6 사다리 재용) — moov 교체+그 밖 box 원문 복사 ----
    std::vector<unsigned char> head1;
    if (moovStart <= static_cast<std::streamoff>(kTagHeadCap)) {
        // 선행 box(ftyp/free — 실사용 소형) 원문 선두 기입 버퍼(비-faststart
        // 대형 mdat 선두 = 스트림 복사 회피 leg — 하단).
        head1.resize(static_cast<size_t>(moovStart));
        f.clear();
        f.seekg(0, std::ios::beg);
        f.read(reinterpret_cast<char*>(head1.data()),
               static_cast<std::streamsize>(head1.size()));
        if (f.gcount() != static_cast<std::streamsize>(head1.size())) {
            err = "audio read failed";  // 재독 실패 — 정직 거부
            return false;
        }
    }
    return TagAtomicWrite(
        path, f, head1,
        [&](TagFileWriter& w) -> bool {
            if (head1.empty()) {
                // 비-faststart — 선행 box를 스트림 복사(버퍼 대기열 회피).
                if (!CopyStreamRange(f, w, 0, moovStart, err)) return false;
            }
            if (!w.WriteAll(newMoov.data(), newMoov.size())) {
                err = "tmp write failed";
                return false;
            }
            if (freeAbsorb) {
                // free/skip 재기입(사이즈 필드만 — payload = 원문 스트림 복사)
                // — Δ 흡수로 mdat 자리 고정(① — 이동 0).
                unsigned char fh[8];
                const unsigned long long nf =
                    static_cast<unsigned long long>(
                        static_cast<long long>(nextTotal) - delta);
                fh[0] = static_cast<unsigned char>(nf >> 24);
                fh[1] = static_cast<unsigned char>(nf >> 16);
                fh[2] = static_cast<unsigned char>(nf >> 8);
                fh[3] = static_cast<unsigned char>(nf);
                fh[4] = static_cast<unsigned char>(nextType[0]);
                fh[5] = static_cast<unsigned char>(nextType[1]);
                fh[6] = static_cast<unsigned char>(nextType[2]);
                fh[7] = static_cast<unsigned char>(nextType[3]);
                if (!w.WriteAll(fh, 8)) {
                    err = "tmp write failed";
                    return false;
                }
                if (delta > 0) {
                    // payload 축소 — 흡수된 Δ 바이트 = 말미 절단(payload 원문
                    // 선두 복사·자르는 바이트 = 흡수분).
                    if (!CopyStreamRange(f, w, moovEnd + 8,
                                         static_cast<std::streamoff>(
                                             nextTotal) -
                                             8 - delta,
                                         err))
                        return false;
                } else {
                    // payload 확장 — 흡수 |Δ| = zero pad(free 규약 잔산)
                    // 선두 기입+payload 원문 복사.
                    const std::vector<unsigned char> pad(
                        static_cast<size_t>(-delta), 0);
                    if (!w.WriteAll(pad.data(), pad.size())) {
                        err = "tmp write failed";
                        return false;
                    }
                    if (!CopyStreamRange(f, w, moovEnd + 8,
                                         static_cast<std::streamoff>(
                                             nextTotal) -
                                             8,
                                         err))
                        return false;
                }
                const std::streamoff skipAt =
                    moovEnd + static_cast<std::streamoff>(nextTotal);
                return CopyStreamRange(f, w, skipAt, total - skipAt, err);
            }
            // moov 뒤 box(mdat 포함) 원문 복사 — mdat 데이터 원문 바이트
            // 무이동(D2 — header 자리만 앞당겨진다).
            return CopyStreamRange(f, w, moovEnd,
                                   static_cast<std::streamoff>(total) -
                                       moovEnd,
                                   err);
        },
        {}, err);
}

// mp3: APIC 프레임(첫 유효 — 인코딩 0/1 모두 지원: mime Latin-1/UTF-16 해독
// (DecodeId3Text 재용)·desc 스킵·그림 데이터 원문). FLAC/Ogg/WAV leg — 아래
// 접점이 형식별로 갈라진다(읽기 사슬의 원문 계약을 거꾸로 걷는 leg). 커버
// 부재 = "no_cover"·개방 실패 = "open"·총계 실패 = "size"(ParseTag 원문
// 사유 계열). 성립 = mime 정규화 표기(NormalizeCoverMime)+그림 바이트 원문.
// 거부 = 빈 out(부재·실패 전부 — 브리프 계약).
inline bool ReadCover(const std::string& path, CoverArt& out, std::string& err) {
    err.clear();
    out = CoverArt{};  // 거부 계약 — out 소각(브리프 원문)
    namespace fs = std::filesystem;
    std::ifstream f(fs::path(path), std::ios::binary);
    if (!f.is_open()) { err = "open"; return false; }
    f.seekg(0, std::ios::end);
    const std::streamoff total = f.tellg();
    if (total < 0) { err = "size"; return false; }
    // 머리 12B 프리읽 — 매직 스니프+ID3 선언 크기(SyncSafe32에 버퍼 10B 필요).
    std::vector<unsigned char> pre;
    {
        const std::streamoff preN = total < 12 ? total : 12;
        pre.resize(static_cast<size_t>(preN));
        f.seekg(0, std::ios::beg);
        f.read(reinterpret_cast<char*>(pre.data()), preN);
        pre.resize(static_cast<size_t>(f.gcount()));
    }
    if (pre.size() >= 3 && std::memcmp(pre.data(), "ID3", 3) == 0) {
        if (pre.size() < 10) { err = "tag truncated"; return false; }
        // 선언 태그 끝까지 read(D2 "필요 범위만 read" 원칙 — 커버 프레임이
        // 필요한 범위. kTagHeadCap는 스캔 1패스 상한 — 편집 지점 단발 read는
        // 이 안쪽에 갇히지 않는다 · 재량 상한 kTagWriteMax 16MiB).
        const unsigned long long declared =
            10 + SyncSafe32(pre.data() + 6);
        const size_t footer =
            (pre[3] == 4 && (pre[5] & 0x10)) ? 10u : 0u;
        const unsigned long long needU = declared + footer;
        if (needU > static_cast<unsigned long long>(total)) {
            err = "tag truncated";
            return false;
        }
        if (needU > kTagWriteMax) { err = "tag too large"; return false; }
        std::vector<unsigned char> head(pre.begin(), pre.end());
        head.resize(static_cast<size_t>(needU));
        f.seekg(0, std::ios::beg);
        f.read(reinterpret_cast<char*>(head.data()),
               static_cast<std::streamsize>(head.size()));
        if (f.gcount() != static_cast<std::streamsize>(head.size())) {
            err = "read";
            return false;
        }
        Id3TagInfo tag;
        if (!ReadId3Tag(head.data(), head.size(), tag, err)) return false;
        if (ApicFromFrames(tag.frames, out)) return true;
        err = "no_cover";
        return false;
    }
    // ---- FLAC PICTURE 블록 leg (T2) ----
    // 메타 사슬 조건 read(4B 헤더 단위) → PICTURE 블록(첫 유효 — 2u 원문
    // 계약) → ParseFlacPictureStruct → mime 정규화. 사슬 파산·파손 블록 =
    // 다음 후보/종료(읽기 사슬 관용 — 부재 취급 "no_cover").
    if (pre.size() >= 4 && std::memcmp(pre.data(), "fLaC", 4) == 0) {
        f.clear();
        f.seekg(4, std::ios::beg);
        for (int guard = 0; guard < 256; ++guard) {
            unsigned char hdr[4];
            f.read(reinterpret_cast<char*>(hdr), 4);
            if (f.gcount() != 4) break;
            const bool last = (hdr[0] & 0x80) != 0;
            const unsigned blockType = static_cast<unsigned>(hdr[0] & 0x7F);
            const size_t bsz = (static_cast<size_t>(hdr[1]) << 16) |
                               (static_cast<size_t>(hdr[2]) << 8) |
                               static_cast<size_t>(hdr[3]);
            if (bsz > kTagWriteMax) break;  // 폭주 컷(정사 관용)
            if (blockType == 6) {
                std::vector<unsigned char> pay(bsz);
                if (bsz > 0) {
                    f.read(reinterpret_cast<char*>(pay.data()),
                           static_cast<std::streamsize>(bsz));
                    if (f.gcount() != static_cast<std::streamsize>(bsz)) break;
                }
                std::string mime;
                std::vector<unsigned char> bytes;
                if (!ParseFlacPictureStruct(pay.data(), pay.size(), mime,
                                            bytes) ||
                    bytes.empty())
                    continue;
                const std::string norm = NormalizeCoverMime(mime);
                if (norm.empty()) continue;
                out.mime = norm;
                out.bytes = std::move(bytes);
                return true;
            }
            // PICTURE 이외 블록 — 페이로드 스킵(선언 크기만큼 seek)
            f.seekg(static_cast<std::streamoff>(bsz), std::ios::cur);
            if (last) break;
        }
        err = "no_cover";
        return false;
    }
    // ---- m4a covr atom leg (T2 — #115 D4) ----
    // moov/udta/meta('mdir')/ilst 트리 보행(ParseMp4Covr — 태그 leg의
    // Mp4FindIlst 공용 원존) → covr 항목 data atom(코드 13=JPEG·14=PNG·
    // 0=스니프). 커버 블롭이 moov 안에 있으므로 머리 read 상한 =
    // kTagWriteMax(쓰기 leg 재용 — kTagHeadCap는 스캔 1패스 상한). moov
    // 후행(머리 밖)·covr 부재·트리 파산 = "no_cover"(부재 취급 — 읽기
    // 사슬 관용·잘린 그림 전시 금지 원문).
    if (pre.size() >= 8 && std::memcmp(pre.data() + 4, "ftyp", 4) == 0) {
        const std::streamoff headN =
            total < static_cast<std::streamoff>(kTagWriteMax)
                ? total
                : static_cast<std::streamoff>(kTagWriteMax);
        std::vector<unsigned char> head(pre.begin(), pre.end());
        head.resize(static_cast<size_t>(headN));
        f.clear();
        f.seekg(0, std::ios::beg);
        f.read(reinterpret_cast<char*>(head.data()),
               static_cast<std::streamsize>(head.size()));
        if (f.gcount() != static_cast<std::streamsize>(head.size())) {
            err = "read";
            return false;
        }
        std::string mime;
        std::vector<unsigned char> bytes;
        if (ParseMp4Covr(head.data(), head.size(), mime, bytes)) {
            out.mime = NormalizeCoverMime(mime);
            out.bytes = std::move(bytes);
            return true;
        }
        err = "no_cover";
        return false;
    }
    // ---- opus OpusTags METADATA_BLOCK_PICTURE leg (T2 — #115 D3/D4) ----
    // vorbis leg와 같은 MBP 사슬이지만 코멘트 패킷 매agic = "OpusTags" 8B
    // (마커 바이트 없음). 구별 스니프(첫 페이지 최초 패킷 "OpusHead" —
    // OggPacket1IsOpus) → 머리 kTagHeadCap read → lacing 패킷 지목
    // (OpusTagsPacket 재용 — ParseOpusTags 원문) → ParseVorbisCommentsRaw →
    // CoverFromMbpEntries(vorbis leg와 공용 사슬 — 첫 유효 MBP). vorbis는
    // 아래 기존 스트리밍 leg 원존(무접촉).
    if (pre.size() >= 4 && std::memcmp(pre.data(), "OggS", 4) == 0) {
        constexpr std::streamoff kOggSniff = 290;  // 27헤더+255세그+8매agic
        const std::streamoff snN =
            total < kOggSniff ? total : kOggSniff;
        std::vector<unsigned char> sn;
        f.clear();
        f.seekg(0, std::ios::beg);
        sn.resize(static_cast<size_t>(snN));
        f.read(reinterpret_cast<char*>(sn.data()), snN);
        sn.resize(static_cast<size_t>(f.gcount()));
        if (OggPacket1IsOpus(sn.data(), sn.size())) {
            f.clear();
            const std::streamoff headN =
                total < static_cast<std::streamoff>(kTagHeadCap)
                    ? total
                    : static_cast<std::streamoff>(kTagHeadCap);
            std::vector<unsigned char> head(pre.begin(), pre.end());
            head.resize(static_cast<size_t>(headN));
            f.seekg(0, std::ios::beg);
            f.read(reinterpret_cast<char*>(head.data()),
                   static_cast<std::streamsize>(head.size()));
            if (f.gcount() != static_cast<std::streamsize>(head.size())) {
                err = "read";
                return false;
            }
            std::vector<unsigned char> comments;
            std::string vendor;
            std::vector<VorbisEntry> entries;
            if (OpusTagsPacket(head.data(), head.size(), comments) &&
                ParseVorbisCommentsRaw(comments.data(), comments.size(),
                                       vendor, entries) &&
                CoverFromMbpEntries(entries, out))
                return true;  // 첫 유효 MBP — 2u 원문 계약
            err = "no_cover";
            return false;
        }
    }
    // ---- Ogg METADATA_BLOCK_PICTURE leg (T2) ----
    // 페이지 연쇄 보행 → "\x03vorbis" 패킷(패킷 2 — lacing 원문 보행) →
    // ParseVorbisCommentsRaw → METADATA_BLOCK_PICTURE(베이스64) →
    // ParseFlacPictureStruct. 머리 컷 kTagHeadCap(읽기 ParseOggTags 원문
    // 수형)·부재·파산·복호화 실패 = 부재 취급 "no_cover"(읽기 사슬 관용).
    if (pre.size() >= 4 && std::memcmp(pre.data(), "OggS", 4) == 0) {
        f.clear();
        f.seekg(0, std::ios::beg);
        std::vector<unsigned char> page, pkt;
        bool midPacket = false, done = false, aligned = false;
        size_t walked = 0;
        while (!done) {
            const int r = OggReadPage(f, page);
            if (r <= 0) break;  // 종료/파산 — 부재 취급
            walked += page.size();
            if (walked > kTagHeadCap) break;  // 머리 컷(원문 수형)
            const size_t segs = page[26];
            const unsigned char* lace = page.data() + 27;
            const unsigned char* pay = page.data() + 27 + segs;
            size_t payOff = 0;
            for (size_t s = 0; s < segs; ++s) {
                const size_t lv = lace[s];
                if (!midPacket) {
                    aligned = (payOff == 0);
                    pkt.clear();
                }
                pkt.insert(pkt.end(), pay + payOff, pay + payOff + lv);
                payOff += lv;
                midPacket = (lv == 255);
                if (midPacket) continue;
                // 패킷 완성 — 코멘트 패킷(선두 마커) 여부
                if (aligned && pkt.size() >= 7 && pkt[0] == 0x03 &&
                    std::memcmp(pkt.data() + 1, "vorbis", 6) == 0) {
                    done = true;
                    break;
                }
                pkt.clear();
            }
        }
        if (done && !midPacket && pkt.size() >= 7) {
            std::string vendor;
            std::vector<VorbisEntry> entries;
            if (ParseVorbisCommentsRaw(pkt.data() + 7, pkt.size() - 7,
                                       vendor, entries) &&
                CoverFromMbpEntries(entries, out))
                return true;  // 첫 유효 MBP — 원문 사슬(CoverFromMbpEntries
                              // 공용 — opus leg 동형 소비)
        }
        err = "no_cover";
        return false;
    }
    // ---- WAV "id3 " 청크 APIC leg (T2) ----
    // RIFF 청크 연쇄에서 "id3 " 청크(대소문자 무시 — 읽기 ParseWavTags 규약
    // 그대로) → ReadId3Tag → ApicFromFrames(T1 mp3 APIC 파싱 재용 — 브리프
    // 원문). id3 부재·청크 파산 = "no_cover"(읽기 사슬 관용).
    if (pre.size() >= 12 && std::memcmp(pre.data(), "RIFF", 4) == 0 &&
        std::memcmp(pre.data() + 8, "WAVE", 4) == 0) {
        f.clear();
        std::streamoff pos = 12;
        for (int guard = 0; pos + 8 <= total && guard < 1024; ++guard) {
            unsigned char ch[8];
            f.seekg(pos, std::ios::beg);
            f.read(reinterpret_cast<char*>(ch), 8);
            if (f.gcount() != 8) break;
            const unsigned int csz = RdLE32(ch + 4);
            const bool id3 =
                FoldAscii(static_cast<char>(ch[0])) == 'i' &&
                FoldAscii(static_cast<char>(ch[1])) == 'd' &&
                FoldAscii(static_cast<char>(ch[2])) == '3';
            if (id3) {
                if (pos + 8 + csz > total) break;  // 잘린 청크 — 부재 취급
                if (csz < 10) break;  // 태그 헤더 미만 — 부재 취급
                if (csz > kTagWriteMax) {
                    err = "tag too large";
                    return false;
                }
                std::vector<unsigned char> pay(csz);
                f.read(reinterpret_cast<char*>(pay.data()),
                       static_cast<std::streamsize>(csz));
                if (f.gcount() != static_cast<std::streamsize>(csz)) break;
                Id3TagInfo tag;
                if (!ReadId3Tag(pay.data(), pay.size(), tag, err))
                    return false;  // 구조 파산 — 정직 전파(원본 보존 우선)
                if (ApicFromFrames(tag.frames, out)) return true;
                break;  // 첫 id3 청크만(v1 계약 — 읽기 ParseWavTags 원문)
            }
            pos += 8 + static_cast<std::streamoff>(csz) + (csz & 1);
        }
        err = "no_cover";
        return false;
    }
    // 매직 없는 mp3 — 커버는 v2 APIC의 몫(v1에는 그림 필드가 없다).
    err = "no_cover";
    return false;
}

// ---- 태그 쓰기 (6형식 분기 — mp3 T1·FLAC/Ogg/WAV T2·opus/m4a T3, D5·D6·
// D8·D9·D10·#115 D2/D3) ----

// mp3: WriteId3v2.3 — 기존 v2 크기 산출 → 새 태그 크기 계산 → 패딩 조절
// (기존 태그 = 선언 본문 크기 유지 — 오디오 데이터 제자리, content 초과분만
// 재배치; 신설 = 패딩 1KB) → [기존 태그 끝, EOF) 스트림 복사(전체 파일
// rewrite·D6 원자적 — 64KB 분절, 오디오+ID3v1 말미 원문). cover != null =
// APIC 교체(기존 APIC 보존에서 제외), null = 기존 APIC 원문 유지 FLAC/
// Ogg/WAV = T2 leg(WriteFlacTag/WriteOggTag/WriteWavTag — 그 밖 형식 = 정직
// 거부 "unsupported_format"). 확장자/
// 스니프 불일치·0바이트("empty")·구조 파산 = 정직 거부+원본 무변경.
inline bool WriteTag(const std::string& path, const TagSet& t,
                     const CoverArt* cover /* null = 기존 APIC 유지 — err 전
                                            참조라 기본값 없음(비후행 계약) */,
                     std::string& err) {
    err.clear();
    namespace fs = std::filesystem;
    std::ifstream f(fs::path(path), std::ios::binary);
    if (!f.is_open()) { err = "open"; return false; }
    f.seekg(0, std::ios::end);
    const std::streamoff total = f.tellg();
    if (total < 0) { err = "size"; return false; }
    if (total == 0) { err = "empty"; return false; }
    // 머리 12B 프리읽 — 스니프(선단 매직이 진실원, 확장자 보조 분기).
    std::vector<unsigned char> pre;
    {
        const std::streamoff preN = total < 12 ? total : 12;
        pre.resize(static_cast<size_t>(preN));
        f.seekg(0, std::ios::beg);
        f.read(reinterpret_cast<char*>(pre.data()), preN);
        pre.resize(static_cast<size_t>(f.gcount()));
    }
    bool hasV2 = false;
    Id3TagInfo old;
    if (pre.size() >= 3 && std::memcmp(pre.data(), "ID3", 3) == 0) {
        if (pre.size() < 10) { err = "tag truncated"; return false; }
        // 선언 태그 끝(+푸터)까지 프리읽 — 커버 포함 대형 태그도 온전히 다룬다
        // (kTagHeadCap는 스캔 1패스 상한 — 쓰기 leg의 구속 아님·kTagWriteMax).
        const unsigned long long declared = 10 + SyncSafe32(pre.data() + 6);
        const size_t footer = (pre[3] == 4 && (pre[5] & 0x10)) ? 10u : 0u;
        const unsigned long long needU = declared + footer;
        if (needU > static_cast<unsigned long long>(total)) {
            err = "tag truncated";
            return false;
        }
        if (needU > kTagWriteMax) { err = "tag too large"; return false; }
        std::vector<unsigned char> head(pre.begin(), pre.end());
        head.resize(static_cast<size_t>(needU));
        f.seekg(0, std::ios::beg);
        f.read(reinterpret_cast<char*>(head.data()),
               static_cast<std::streamsize>(head.size()));
        if (f.gcount() != static_cast<std::streamsize>(head.size())) {
            err = "read";
            return false;
        }
        if (!ReadId3Tag(head.data(), head.size(), old, err)) return false;
        hasV2 = true;
    } else if (pre.size() >= 4 &&
               std::memcmp(pre.data(), "fLaC", 4) == 0) {
        // FLAC — 메타 블록 연쇄 재조립(D8 PICTURE 교체·D9 STREAMINFO/SEEK
        // TABLE 보존·벤더 유지 — T2 leg).
        return WriteFlacTag(path, f, total, t, cover, err);
    } else if (pre.size() >= 8 &&
               std::memcmp(pre.data() + 4, "ftyp", 4) == 0) {
        // m4a/mp4 — moov/udta/meta/ilst 재작성(D2 — mdat 원문 바이트 무이동·
        // T3 leg — 읽기 Mp4LocateIlst 원존 사슬의 역방향).
        return WriteM4aTag(path, f, total, t, cover, err);
    } else if (pre.size() >= 4 &&
               std::memcmp(pre.data(), "OggS", 4) == 0) {
        // Ogg — vorbis/opus 구별 스니프(T2 읽기 leg 원존 재용) → opus =
        // OpusTags 패킷 교체(D3 — T3 leg)·vorbis = 코멘트 패킷 교체+뒤페이지
        // 재기입(D10 — granule 불변·T2 leg). 290 = 27헤더+255세그+8매agic
        // 상수 — ReadCover opus leg 원존(정적 컷 — 식별 패킷 19B 실측 여유).
        constexpr std::streamoff kOggSniff = 290;
        const std::streamoff snN = total < kOggSniff ? total : kOggSniff;
        std::vector<unsigned char> sn(static_cast<size_t>(snN));
        f.clear();
        f.seekg(0, std::ios::beg);
        f.read(reinterpret_cast<char*>(sn.data()), snN);
        sn.resize(static_cast<size_t>(f.gcount()));
        if (OggPacket1IsOpus(sn.data(), sn.size()))
            return WriteOpusTag(path, f, total, t, cover, err);
        return WriteOggTag(path, f, total, t, cover, err);
    } else if (pre.size() >= 12 &&
               std::memcmp(pre.data(), "RIFF", 4) == 0 &&
               std::memcmp(pre.data() + 8, "WAVE", 4) == 0) {
        // WAV — "id3 " 청크 교체/말미 신설+RIFF 크기 재계산(청크 순서 보존
        // — 형식 해소 (c)·T2 leg).
        return WriteWavTag(path, f, total, t, cover, err);
    } else {
        // 매직 없음 — mp3 원존(v1-raw leg)만 신설 대상. 그 외 = 정직 거부.
        const std::string ext =
            std::filesystem::path(path).extension().string();
        if (jk::crt::Stricmp(ext.c_str(), ".mp3") != 0) {
            err = "unsupported_format";
            return false;
        }
    }
    const size_t oldEnd = hasV2 ? old.end : 0;
    if (oldEnd > static_cast<size_t>(total)) {  // 방어선(프리읽 게이트 중복)
        err = "tag truncated";
        return false;
    }
    // ---- 새 사슬 조립 (D5 교체/보존 — T2에서 공용 부품 추출: BuildId3V23
    // Body — WAV leg가 같은 사슬을 재용한다) ----
    std::vector<unsigned char> body;
    if (!BuildId3V23Body(hasV2 ? &old : nullptr, t, cover, body, err))
        return false;
    // 패딩 조절(D5) — 기존 태그: 선언 본문 크기 유지(오디오 제자리 — content
    // 초과분만 재배치·대형 커버 소멸 시 잔여 패딩은 무해 잔산, 리포트 부기).
    // 신설: 패딩 1KB.
    const size_t pad =
        hasV2 ? (old.bodySize > body.size() ? old.bodySize - body.size() : 0)
              : 1024u;
    std::vector<unsigned char> tag;
    if (!AssembleId3V23Tag(body, pad, tag, err)) return false;
    // ---- 원자적 쓰기 (D6) — .tagtmp → fsync → rename(선례 승계) ----
    const std::streamoff oldEndOff = static_cast<std::streamoff>(oldEnd);
    const std::streamoff copyCount = total - oldEndOff;
    return TagAtomicWrite(
        path, f, tag,
        [&](TagFileWriter& w) -> bool {
            // 오디오 데이터(말미 v1 포함) 스트림 복사 — [oldEnd, total) 원문.
            return CopyStreamRange(f, w, oldEndOff, copyCount, err);
        },
        {}, err);
}

} // namespace tags

// ---- music_tag_write 도구 경계 (T4 — 스펙 2026-10-25-music-tagwrite §4) ----
// agent 도구 hub의 music 앱 4종째: UI [저장](SaveTagEdit)과 **동일 사슬**
// (tags::WriteTag 재용 — 원자적 tmp+rename·4형식 분기 전부 내장)의 도구
// 경계. 순수 몫(args 판독+커버 수취+사슬 1콜)은 이 헤더가 소유 — selftest
// 2u-⑮와 앱 OnAgentToolCall이 같은 함수를 소비한다(DRY 최소·사슬 단일 출구).
namespace tool {

// 도구 args 계약(브리프 원문): {path, title, artist, album, cover(path|null)}.
// cover의 빈값 = null 취급(기존 아트 유지 — WriteTag의 cover=null 계약).
struct TagWriteArgs {
    std::string path, title, artist, album, cover;
};

// args 판독 — AgentJson(도구 허브 원존 파서 — quickjs 원문 회수·이스케이프
// 복호·UTF-8 계약). path 필수(M-3 원문 가드의 도구 경계 수형); title/
// artist/album = 기본 공문자(빈 필드 = 프레임 생략 쓰기 — WriteTag 계약과
// 등치); cover = null/생략도 합법(판독 실패 = 빈값 유지 = null).
inline bool ParseTagWriteArgs(const std::string& argsJson, TagWriteArgs& a) {
    a = TagWriteArgs{};
    const agent::AgentJson args(argsJson);
    if (!args.ok() || !args.GetStr("path", a.path) || a.path.empty())
        return false;
    args.GetStr("title", a.title);
    args.GetStr("artist", a.artist);
    args.GetStr("album", a.album);
    args.GetStr("cover", a.cover);  // null/생략 = 판독 실패 = 빈값(null)
    return true;
}

// 커버 파일 수취(도구 경계 — UI SaveTagEdit의 확장자 판정과 달리 브리프
// 원문의 **마법수** 판정: 0xFF 0xD8 = image/jpeg·0x89 0x50 = image/png,
// 그 외 = 정직 거부). bytes = 파일 원문 수취·상한 = kTagWriteMax 재용
// (WriteTag와 같은 폭주 절단 계약). 파일 접점 = fs::path 개방(2q 수형 —
// CP949 바이트 경로도 native 변환 성공 — 인코딩 사슬 함정 회피).
inline bool LoadCoverByMagic(const std::string& path, tags::CoverArt& out,
                             std::string& err) {
    out = tags::CoverArt{};
    namespace fs = std::filesystem;
    const fs::path p(path);
    std::error_code ec;
    const auto sz = fs::file_size(p, ec);
    if (ec || sz <= 0) { err = "cover open"; return false; }
    if (static_cast<unsigned long long>(sz) > tags::kTagWriteMax) {
        err = "cover too large";
        return false;
    }
    std::ifstream f(p, std::ios::binary);
    if (!f.is_open()) { err = "cover open"; return false; }
    out.bytes.resize(static_cast<size_t>(sz));
    f.read(reinterpret_cast<char*>(out.bytes.data()),
           static_cast<std::streamsize>(sz));
    if (f.gcount() != static_cast<std::streamsize>(sz)) {
        out = tags::CoverArt{};
        err = "cover read";
        return false;
    }
    if (out.bytes.size() >= 2 && out.bytes[0] == 0xFF &&
        out.bytes[1] == 0xD8) {
        out.mime = "image/jpeg";  // SOI 마법수
    } else if (out.bytes.size() >= 4 && out.bytes[0] == 0x89 &&
               out.bytes[1] == 0x50 && out.bytes[2] == 0x4E &&
               out.bytes[3] == 0x47) {
        out.mime = "image/png";  // 89 50 4E 47 마법수
    } else {
        out = tags::CoverArt{};
        err = "cover magic unsupported";  // 정직 거부 — 판정 불가 mime
        return false;
    }
    return true;
}

// 도구 경계 쓰기 — UI와 동일 사슬 1콜(cover 빈값 = null = 기존 유지·지정 =
// 교체 D8). 실패 = 정직 거부(WriteTag 내부 원본 무손상 계약 원문 전파).
inline bool TagWriteTool(const TagWriteArgs& a, std::string& err) {
    tags::TagSet t;
    t.title = a.title;
    t.artist = a.artist;
    t.album = a.album;
    tags::CoverArt cover;
    const tags::CoverArt* coverPtr = nullptr;
    if (!a.cover.empty()) {
        if (!LoadCoverByMagic(a.cover, cover, err)) return false;
        coverPtr = &cover;  // 지정 = 교체(D8), 빈값 = 유지(WriteTag 계약)
    }
    return tags::WriteTag(a.path, t, coverPtr, err);
}

} // namespace tool

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