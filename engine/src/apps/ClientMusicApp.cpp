// Music hub client (spec 2026-10-09-music-library-design, T2) — the gallery
// twin: dark root window, 16 ms timer as a delivery channel, ImGui over the
// surface, Korean font via the desktop resolver, AppContentTopOffset band
// math, theme hot-swap re-apply. T2 adds the async scan worker (single
// parked std::thread, one-slot latest-wins request mailbox, destruction
// join) and the track table (rel / size / mtime) + the name filter. The
// displayed vector is the scan worker's ListAudioFiles output verbatim —
// sorted (mtime desc, rel asc tie) and consumed without re-sort or derivation
// (selftest 2m-f asserts that isomorphism). Arrival of a scan result is the
// ONLY frameDirty_ source (#89 idle contract, docs/88). #99 T2 (spec
// 2026-10-11-music-tags-design): the scan pass itself parses the tag head
// (tags::ParseTag — 헤더만 읽는 D2 원문, 워커 몫 — 도착 표기만 더디) and the
// row columns re-cut to [곡명(아티스트)] [앨범] [크기] [수정] [spatial]
// (full 절대경로 → row tooltip — fix r1 I-3 문언); the filter widens to
// filename+title+artist (D5).
// T3 adds the playback
// delegation — a track-row double-click sends the spec §4 query pair
// (① launch_app vplayer → ② app_tool open) through the library app's
// SendQuery/PollReplies idiom; replies and exhausted retries are the only
// extra dirty sources, a retry resend never dirties. T2 (spatial leg line,
// spec 2026-10-10-music-spatial-leg-design) adds the embedded spatial
// playback channel behind JK_MUSIC_SPATIAL_LEG: audio_core (spatial-player
// 소비 — StreamPlayer[brief: BufferQueueStreamPlayer]+SpatialEngine) is polled
// through pump() in OnIdle, the row [spatial] button + position sliders form
// the notation layer (T1 pure parts — renders in every build) and the app
// tool trio spatial_play/stop/status is registered for the agent tool hub.
// The delegation channel (double click → vplayer) is untouched (D3 공존).
// #97 T2 (plan 2026-10-11-music-dir-browser) adds the in-app folder browser
// under the [폴더 관리] panel — the jk::music::browse pure parts (ListSubdirs/
// JoinDir/Parent, T1) consumed by a type-free picker (cwd row + ".." up row +
// subdir click-enter list + [이 폴더 추가] via the DirAddManaged single path +
// [닫기]); only the add success/failure status line dirties (no new frameDirty
// sources — navigation is an input event's natural consequence, #89).
// #111 T3 (spec 2026-10-25-music-tagwrite-design §2·§3) adds the tag edit
// panel + the cover art display. Row [편집] (beside [spatial]) opens the tag
// edit panel — the [폴더 관리] panel's 동형 전환 pattern (same window·tab,
// no new window/child outside the flow). The save is exactly one
// tags::WriteTag call (the atomic tmp+rename lives inside — the UI adds no
// further file handling); success = status line + re-scan re-request (the
// existing RequestScan lane) + panel close; failure = the honest err line
// inside the panel (the session stays open, the input keeps its value). The
// [커버 선택] picker re-uses the #97 browser parts (cwd / ".." rows /
// click-enter subdirs) plus a local click-to-select image row list — image
// filter .jpg/.jpeg/.png only (mime 판정 정합 — 타이핑 0, phone contract).
// Cover decode is the stb 조달품 via jk::LoadImageMemory/LoadImageFile
// (JKImageLoader — no TU may redefine the stb implementation, so the app
// never includes stb_image itself); row thumbnails are a memory LRU pool of
// 96 keyed (path, mtime) with nearest downscale to the 32px box (갤러리
// 썸네일 선례 — the pool/lru/shrink parts live in this file). D7 (지연 읽기
// 원존): ReadCover never mixes into the scan pass — cache misses are read
// from the render thread at ≤2 per frame, and the pool-starved frames
// continue via the #89 T2 조건화 idiom (pendingCovers_ → OnIdle re-asserts
// frameDirty_, 도착 완료 = 마지막 프레임). A failed read is terminal until
// the next re-scan generation (failedGen — placeholder keeps the gray face).
#include <apps/ClientMusicApp.h>

#include <imgui_impl_jkwindow.h>
#include <JKTextAtlas.h>
#include <JKWindow.h>
#include <fs/JKFs.h>  // FileTimeToSys — file_clock 수형 → 시간 표기(플랫폼 epoch 몫)
#include "theme/JKThemeImGui.h"
#include <SDL.h>

#include <algorithm>  // std::find — 도착 멤버십 필터(#93 T2)의 dirs_ 열외 순회
#include <chrono>
#include <cstdio>    // stderr 진단 1행(도착 폐기 — 정직 계약)
#include <cstring>   // std::memcpy/strncpy — 커버 픽셀 축소·프리필 사본(T3)
#include <ctime>
#include <fstream>   // SettingsText 통독 수형(fs::path 개방 — #94 잠복 수리)
// 폴더 스캔은 jk::music::ListAudioFiles(원문 규약 — ec 중립형, 재귀 가드
// fix r1)이 소유한다. 여기선 fs::path 합성만 쓴다.
#include <filesystem>
#include <port/JKCrtShim.h>  // LocaltimeS — Win/posix 공용 시간 표기 수형
#include <agent/JKAgentJson.h>  // OnAgentToolCall 인자 판독(도구 허브 §8.2 원문)
                                // 폴더 관리(T2): MusicDirStore.h는 헤더 선두의
                                //   include가 소유(스캐너 read leg·DirWriteResult)
                                // UTF-8 유효성 가드(Utf8ToUtf16 fail-closed 계약 —
                                //   스캔 leg 진입 검사)의 소유 헤더:
#include <text/JKTextConv.h>

#ifdef JK_MUSIC_SPATIAL_LEG
// spatial leg — audio_core 소비(T1 CMake env 분기: SPATIAL_PLAYER_ROOT 설정
// 빌드에서만 컴파일). 완전형 include는 이 가드 안에만 산다(header는 불완전형).
// **spatial-player 경로는 어디에도 기입하지 않는다(env-only 규약)**.
#include <spatial.h>         // SpatialEngine — ALC 디바이스+HRTF 컨텍스트 래퍼
#include <stream_player.h>   // StreamPlayer — open/play/pump/set_position 계약
#endif

namespace jk {
namespace {
// Root window paints the dark clear color so the surface never flashes white
// behind ImGui's rounded windows (gallery/palette/notify idiom — 쌍둥이 원문).
class MusicRoot : public JKWindow {
public:
    explicit MusicRoot(const std::string& title) : JKWindow(title) {}
    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetClientRect();
        const auto& t = jk::theme::current();
        dc.SetColor(t.appClearBg.r, t.appClearBg.g, t.appClearBg.b, 255);
        dc.FillRect(client);
    }
};

// 사람단위 크기 — bytes는 원수, 그 밖은 1자리 고정(ClientFilesApp FmtSize
// 원문 수형: 1024 경계 B/KB/MB). 오디오파은 MB 상위가 현실이라 GB 경계는
// 미접촉(FmtSize 원문도 없다 — 동형 유지).
void HumanSize(long long bytes, char* buf, size_t bufBytes) {
    if (bytes >= 1024 * 1024) {
        std::snprintf(buf, bufBytes, "%.1f MB",
                      static_cast<double>(bytes) / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        std::snprintf(buf, bufBytes, "%.1f KB",
                      static_cast<double>(bytes) / 1024.0);
    } else {
        std::snprintf(buf, bufBytes, "%lld B", bytes);
    }
}

// mtime(Track.mtime — file_clock 원문 수형) → "YYYY-MM-DD HH:MM" 로컬 표기.
// file_clock epoch가 플랫폼마다 다르므로 jk::fs::FileTimeToSys 단일 수형으로
// system_clock을 풀고(WorkshopStore::EntryMtimeSecs 동형) LocaltimeS shim으로
// 현지 시각(strftime 세부 — ClientSettingsApp::BuildUi 3 블록 원문 수형).
// **숫자 가드는 0만**(fix r3 부수 원장 ① — T4 WSL 실측): libstdc++(GCC 13)
// file_clock epoch=2174라 last_write_time().time_since_epoch().count()가
// **음수**로 나오고(실측 count=-4646113461279417845), 구판의 mtime<=0 가드는
// 그 전행을 "-"로 오렸다. 음수는 FileTimeToSys 재구성(clock_cast)에서 정상
// 시각(2026-10-09)으로 풀린다(같은 실측 — 변환 후 표기 부합). 0(스탬프 부재
// — stat 실패 열외 수형)만 "-" 유지. 변환 실패(지지 시대 미만 등)는
// LocaltimeS/strftime 실패 분기가 "-" 폴백을 소유한다(무음 계약 원문).
// Windows·phone은 양수 epoch라 증상 없음(T4 실측 — 가드 정정의 관측 몫).
// 재청구 폴백 상수(T3 — 콜드 부팅 경기 흡수): launch_app의 스폰은 비동기라
// vplayer의 도구 등록(SendAgentToolRegister — 프로세스 기동+연결 뒤)이 open
// 릴레이보다 뒤져 unknown_app_tool이 떨어진다. 서버 부품 신설 금지(폴백
// 원존 원칙)라 클라가 기존 채널로 재청구한다 — 20 × 250ms = 5s 예산(기동
// 실측 대비 큰 여유). 시간 기반 pacing이라 무더기 재청구가 없다. 크레딧은
// 답신 수취에서만 소모한다(fix r2/r3 원칙 — 감법 소재는 PollReplies 1곳).
constexpr int kOpenRetryMax = 20;
constexpr std::chrono::milliseconds kOpenRetryDelayMs{250};

// tracks child의 압착 최소 높이(T2 fix r1 — 압착 가드의 상수): 헤더 행+데이터
// 1행(16px 폰트 × 2 + 행폭 여유). 남는 높이가 "공간 패널 예약 200+이 상수"보다
// 작으면 표를 이 높이로 봉한다(음수 child 높이 봉인 소각 — BuildUi 원문).
constexpr float kTrackMinHeight = 56.0f;

void MtimeLabel(long long mtime, char* buf, size_t bufBytes) {
    std::snprintf(buf, bufBytes, "-");
    if (mtime == 0) return;
    const std::chrono::system_clock::time_point sys =
        jk::fs::FileTimeToSys(
            std::filesystem::file_time_type::clock::time_point(
                std::filesystem::file_time_type::duration{mtime}));
    const std::time_t tt = std::chrono::system_clock::to_time_t(sys);
    std::tm lt{};
    if (jk::crt::LocaltimeS(&lt, &tt) != 0) return;  // 실패 = "-" 유지(무음 계약)
    const size_t n = std::strftime(buf, bufBytes, "%Y-%m-%d %H:%M", &lt);
    if (n == 0) std::snprintf(buf, bufBytes, "-");  // 버퍼 부족 등 = "-" 폴백
}

// spatial 진행 표기(정수초 — SpatialPump의 더티 근거와 같은 분해): "1:23" 계열.
// 표기 분해가 정수초인 이유: 오디오 leg에는 프레임 이미지가 없어 1초당 1프레임
// 갱신이면 충분(#89 — 진행 표기 갱신만 더티의 실측 수형).
void FmtClock(long long secs, char* buf, size_t bufBytes) {
    if (secs < 0) secs = 0;
    std::snprintf(buf, bufBytes, "%lld:%02lld", secs / 60, secs % 60);
}

// JSON 문자열 이스케이프(D4 표기의 도구 결과 원문 — vplayer의 EscapeJson 원문
// 수형: 따옴표·역슬래시·제어 바이트; spatial_status의 path/error 필드용).
std::string EscapeJson(const std::string& in) {
    std::string out;
    for (char c : in) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (static_cast<unsigned char>(c) < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", c);
            out += buf;
        } else {
            out += c;
        }
    }
    return out;
}

// ---- 폴더 관리 (T2) 공용 헬퍼 ----

// 폴더 관리 도구 3종 계약명(브리프) — 앱 도구 허브 등록·핸들 공용 상수.
constexpr const char* kToolDirAdd = "music_dir_add";
constexpr const char* kToolDirRemove = "music_dir_remove";
constexpr const char* kToolDirList = "music_dir_list";

// 말단 경로 성분 — 바이트 스캔 수형(dir strip 라벨 공용). fs::path의 narrow
// 계층은 이 툴체인에서 UTF-8 기수라 CP949 바이트에 던진다(2o-f 실측+프루브
// 재확인 — filesystem_error "Illegal byte sequence") — dirs_ 표기에는
// fs::path를 못 쓴다. '/'·'\' 전원을 성분 경계로 친다(UTF-8 연속 바이트는
// 0x5C가 없어 무충돌; CP949 확장 한글의 후행 0x5C 파일명 문자가 성분말단으로
// 오인될 수 있다 — 라벨 한계, 전문은 툴힌트/패널 행이 소유 — 표기 한계 원장).
std::string LastPathSegment(const std::string& p) {
    const size_t pos = p.find_last_of("/\\");
    return pos == std::string::npos ? p : p.substr(pos + 1);
}

// 뒤 구분자 정리(호출부 '/' 정리 규약 — T1 fix r1 권고 상쇄): Slashize는
// 고바이트 바로 뒤의 '\'를 리터럴로 두어(데이터 무손상 규약) 뒤 구분자가
// '\'이고 그 앞이 고바이트면 접히지 않는다 — NormalizeDirsPure는 '/'만 접기
// 때문에 호출부(이 앱)가 양쪽 형식을 벗겨 보내야 한다. 몸통의 '\'는 손대지
// 않는다(Slashize의 0x5C 규약이 소유 — 호출부에서 더 접으면 데이터 손상).
std::string TrimTrailSeparators(const std::string& p) {
    std::string s = p;
    while (!s.empty() && (s.back() == '/' || s.back() == '\\')) s.pop_back();
    return s;
}

// ---- 태그 편집+커버아트 (T3 — 스펙 2026-10-25 §2·§3) 순수 관용 부품 ----
// 갤러리 썸네일 파이프라인(GalleryModel.h)의 이 앱 국지 수형 — cross-app
// include 회피(구현 위치 = ClientMusicApp 내 — 브리프 계약). 셀프테스트
// 접점 없음(2g는 갤러리 원문 소유 — 신설 어셜션 0 계약).

// 커버아트 메모리 LRU 상한(브리프 — 갤러리 kThumbPoolMax 96 선례 승계).
inline constexpr size_t kCoverPoolMax = 96;
// 행 썸네일 박스(px — 브리프 32px)+패널 미리보기 박스(px — 모호성 해소 (b):
// 4KB+ 원본도 전체 디코드 뒤 텍스처 축소).
inline constexpr int kCoverThumbBox = 32;
inline constexpr int kCoverPreviewBox = 96;
// 프레임당 ReadCover+디코드 상한(D7 지연 채움 — 렌더 프리즈 절단; 스타된 요청
//은 pendingCovers_로 다음 틱 계속).
inline constexpr int kCoverFillPerFrame = 2;

// 커버아트 파일 확장자 필터(브리프 — .jpg/.jpeg/.png만; WriteTag의 mime 판정
// ValidateCover 승인 표기와 정합 — Stricmp 원존 수형).
inline bool IsCoverImageExtension(const std::string& ext) {
    return jk::crt::Stricmp(ext.c_str(), ".jpg") == 0 ||
           jk::crt::Stricmp(ext.c_str(), ".jpeg") == 0 ||
           jk::crt::Stricmp(ext.c_str(), ".png") == 0;
}

// 확장자 → mime 정규화(커버 선택 파일 — WriteTag의 NormalizeCoverMime 승인
// 표기에 들어가는 원문 형식 "image/jpeg"/"image/png"; 그밖 확장자는 필터가
// 거부하므로 도달 계약 밖 — jpeg 폴백).
inline std::string CoverMimeOfExt(const std::string& ext) {
    return jk::crt::Stricmp(ext.c_str(), ".png") == 0 ? "image/png"
                                                      : "image/jpeg";
}

// 태그 쓰기 지원 확장자(행 [편집] 버튼의 정직 표기 게이트 — D4 표기 수형과
// 같은 성질: 쓰기 leg는 매agic 스니프가 진실원이라 확장자는 **가능성** 게이트
// — 매agic 불일치·매agic 없는 비-mp3는 [저장]의 정직 거부("unsupported_format")
// 로 흡수된다. m4a/aac/wma = v1 스코프 밖 — 버튼 그리지 않는다).
inline bool TagWritableExt(const std::string& full) {
    const std::string ext =
        std::filesystem::path(full).extension().string();
    return jk::crt::Stricmp(ext.c_str(), ".mp3") == 0 ||
           jk::crt::Stricmp(ext.c_str(), ".flac") == 0 ||
           jk::crt::Stricmp(ext.c_str(), ".ogg") == 0 ||
           jk::crt::Stricmp(ext.c_str(), ".wav") == 0;
}

// 박스 핏 승계 상수 — nearest 축소(gallery::MakeThumb 원문 수형 — nearest
// 역사상 y*src.h/dstH)를 박스 상한 안으로만(원본이 박스보다 작으면 원본
// 유지 — 확대 금지). 행 썸네일 32px·미리보기 96px 공용.
inline LoadedImage ShrinkToBox(const LoadedImage& src, int maxBox) {
    if (src.w <= 0 || src.h <= 0 || maxBox <= 0 || src.rgba.empty())
        return LoadedImage();
    const float s = std::min(
        {std::min(static_cast<float>(maxBox) / static_cast<float>(src.w),
                  static_cast<float>(maxBox) / static_cast<float>(src.h)),
         1.0f});
    const int dstW = std::max(1, static_cast<int>(src.w * s));
    const int dstH = std::max(1, static_cast<int>(src.h * s));
    if (dstW == src.w && dstH == src.h) return src;
    LoadedImage out;
    out.w = dstW;
    out.h = dstH;
    out.rgba.assign(static_cast<size_t>(dstW) * dstH * 4, 0);
    for (int y = 0; y < dstH; ++y) {
        const int sy = std::min(src.h - 1, y * src.h / dstH);
        const uint8_t* srcRow = src.rgba.data() + static_cast<size_t>(sy) * src.w * 4;
        uint8_t* dstRow = out.rgba.data() + static_cast<size_t>(y) * dstW * 4;
        for (int x = 0; x < dstW; ++x) {
            const int sx = std::min(src.w - 1, x * src.w / dstW);
            std::memcpy(dstRow + static_cast<size_t>(x) * 4,
                        srcRow + static_cast<size_t>(sx) * 4, 4);
        }
    }
    return out;
}

// 박스 표기 승수(gallery::FitThumb 원문 수형 — 비율 유지·박스 밖 출혈 없음;
// 행 셀과 미리보기 상자의 중앙 배치가 소비).
struct FitBox {
    float w = 0, h = 0;
};
inline FitBox FitImageBox(int w, int h, float maxW, float maxH) {
    if (w <= 0 || h <= 0 || maxW <= 0.f || maxH <= 0.f) return FitBox();
    const float s = std::min(maxW / static_cast<float>(w),
                             maxH / static_cast<float>(h));
    return FitBox{ static_cast<float>(w) * s, static_cast<float>(h) * s };
}

// LRU 퇴출 산치(gallery::PickLruVictim 원문 수형 — 이번 프레임 접촉분은
// 전부 후보 제외: 이미 draw list에 기록된 텍스처의 동프레임 파괴 금지 — C1
// 원장; 후보 없음 = -1 — placeholder 유지).
inline int PickCoverVictim(const std::vector<long long>& useFrames,
                           long long currentFrame) {
    int victim = -1;
    for (size_t i = 0; i < useFrames.size(); ++i) {
        if (useFrames[i] >= currentFrame) continue;  // 이번 프레임 접촉 제외
        if (victim < 0 || useFrames[i] < useFrames[static_cast<size_t>(victim)])
            victim = static_cast<int>(i);
    }
    return victim;
}

// 커버 선택 브라우저의 이미지 파일 열거(ec 중립형 — browse::ListSubdirs
// 원문 수형의 파일판: directory_iterator+ec·도트 스킵·확장자 필터·바이트
// 오름차순 결정론). 없는 cwd/빈 cwd = 빈 목록. 하위 디렉터리는 ListSubdirs가
// 별도 소유(브라우저가 둘을 조립한다).
inline std::vector<std::string> ListCoverFiles(const std::string& cwd) {
    std::vector<std::string> files;
    if (cwd.empty()) return files;  // 방어선(호출부 무접촉)
    std::error_code ec;
    const std::filesystem::directory_iterator it(
        std::filesystem::path(cwd), ec);
    if (ec) return files;  // 열기 실패(부재 cwd 포함) = 빈 목록(독립 실패)
    for (const std::filesystem::directory_entry& entry : it) {
        std::error_code entryEc;
        const bool isDir = entry.is_directory(entryEc);
        if (entryEc || isDir) continue;  // 소멸 성분·하위 폴더 = 스킵
        const std::string name = entry.path().filename().string();
        if (name.empty() || name.front() == '.') continue;  // 숨김 도트 스킵
        if (!IsCoverImageExtension(entry.path().extension().string()))
            continue;
        files.push_back(std::move(name));
    }
    std::sort(files.begin(), files.end());  // 바이트 오름차순 결정론(ListSubdirs
                                            //   tie 수형)
    return files;
}
} // namespace

ClientMusicApp::ClientMusicApp() = default;  // 원밖 — leg unique_ptr의 완전형
                                             //   가시점(cpp 가드 include 뒤)

ClientMusicApp::~ClientMusicApp() {
    // 스캔 워커 join 수형(갤러리 썸네일 원문 계약 — 파괴 join). quit 경련+
    // cv 기상으로 wait를 깨고, 진행 중 스캔이 끝나기를 기다린다(ListAudioFiles
    //는 유한 — 2m-e 순환 가드 계약). 잔여 결과 박스는 소멸자 원의 소각.
    // #93 T2(결정 ② — 파괴 join 무한 대기 소각): join **전에 취소 요청 1발** —
    // 진행 중 스캔이 T1 경계 콜백(스캔CancelFn)에서 절단되고 부분 수집은 취소판
    // 폐기 계약으로 버려져 join이 대략 ms급으로 돌아온다. 워커의 취소 리셋(요청
    // 수취 시점 — ScanWorker)과의 경기는 콜백의 scanQuit_ 동시 체크가 봉합한다:
    // 리셋이 이 지연 순간 이겨도 quit가 이미 참이라 다음 경계 1회 내 절단한다.
    scanQuit_.store(true, std::memory_order_release);
    scanCancel_.store(true, std::memory_order_release);
    scanCv_.notify_all();
    if (scan_.joinable()) scan_.join();
}

std::string ClientMusicApp::ExeDir() {
    char base[1024] = {};
    char* p = SDL_GetBasePath();
    if (p) {
        std::snprintf(base, sizeof(base), "%s", p);
        SDL_free(p);
    }
    return base;
}

void ClientMusicApp::OnInit() {
    auto main = std::make_unique<MusicRoot>("Music");
    main->SetWindowRect(JKRect{ 0, 0, 560, 520 });
    main->SetAttrFlags(WA_CHROMELESS);
    SetMainWindow(std::move(main));

    SetTimerInterval(16); // 틱 = 배송 채널(#89 T1/T2 — 활동·더티 아님)

    ImGui::CreateContext();
    jk::theme::ApplyImGuiTheme(); // JKTheme 팔레트 봉합 (P2 단계 3)
    ImGui::GetIO().IniFilename = nullptr;
    // Korean UI (필터 힌트·빈 상태 문구) — desktop font resolver
    // (docs/63 §4.1, gallery 동일): 빈 해석은 커스텀 폰트 스킵.
    ImGuiIO& io = ImGui::GetIO();
    const std::string fontPath = jk::text::ResolveDesktopFontPath();
    if (!fontPath.empty())
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f,
                                     nullptr,
                                     io.Fonts->GetGlyphRangesKorean());

    // 스캔 워커 — 단일 상시 스레드(생성 주기 원문 계약). 이후의 스캔은
    // RequestScan의 우편함 1발만 만든다(스레드 재창설 없음).
    scan_ = std::thread([this] { ScanWorker(); });

    // spatial leg — audio_core 소비는 매크로 가드(env 설정 빌드만)라 초기화는
    // 첫 [spatial] 발사 시에 한다(무재생 부팅이 ALC 디바이스를 열지 않는다 —
    // vplayer가 열기 전에 오디오 백엔드를 만들지 않는 것과 같은 지연 계약).
    // 부팅 시점의 leg 부재(env 미설정)는 발사 시 DeviceFailed 표기로 흡수된다.

    // 앱 도구 허브 등록 6종(스펙 2026-10-10-music-spatial-leg §2의 3종+폴더
    // 관리 3종 — 플랜 2026-10-10-music-dirs-ui T2; T1 kTool* 원문 상수 소비 —
    // vplayer OnInit 등록 원문 수형). 배치 근거(vplayer 원문 주석): OnInit은
    // JKClientApplication::Init의 Connect 이후에 불린다 —
    // SendAgentToolRegister의 "연결 전 조용한 false" 경로에 걸리지 않는다.
    // inputSchema는 MCP 그대로(broker tools/list 원문 계약). **6종을 1회
    // 발송** — 서버의 등록은 연결 단위 upsert(JKWindowServer
    // HandleToolRegister: appToolManifests_[connId]=m — 제2호 호출이 매니페스트
    // 전체를 치환한다)라 이전 3종이 2호에서 소각된다.
    if (jk::client::JKClientSurface* surface = Surface()) {
        using Decl = jk::client::JKClientSurface::AgentToolDecl;
        std::vector<Decl> tools = {
            {music::leg::kToolPlay,
             "Start spatial (HRTF positional) playback of a music file",
             "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":"
             "\"string\"}},\"required\":[\"path\"]}"},
            {music::leg::kToolStop, "Stop spatial playback", "{}"},
            {music::leg::kToolStatus,
             "Spatial playback status (active/positional/deviceOk/pos/dur/"
             "path/error)",
             "{}"},
            // 폴더 관리 3종(T2) — path 인자는 **UTF-8**(앱 도구 허브 계약 T3
            // 원문) — 저장소는 바이트 투명(std::filesystem native형 = UTF-8).
            {kToolDirAdd,
             "Add a user music directory (settings music.dirs; rescan "
             "follows)",
             "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":"
             "\"string\",\"description\":\"UTF-8 directory path, slash "
             "(/) separators recommended\"}},\"required\":[\"path\"]}"},
            {kToolDirRemove, "Remove a user music directory (settings "
                             "music.dirs; rescan follows)",
             "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":"
             "\"string\"}},\"required\":[\"path\"]}"},
            {kToolDirList,
             "List user music directories (settings music.dirs only — the "
             "default state/music dir is resolver-owned and excluded)",
             "{\"type\":\"object\",\"properties\":{}}"},
        };
        surface->SendAgentToolRegister("music", tools);
    }

    // 재런치 활성 탭 = 유저 폴더 우선(park 배치 4 — 활성 탭 UX 원장행):
    // 기본 폴더(AudioDirFallback — 리졸버가 슬롯 0에 선두 고정)는 쓸 일이
    // 드문 state/music 루트라 탭 스트립 첫 머무름 라벨이 언제나 그 폴더면
    // 재런치마다 [파일/목록 스캔 → 탭 스위치] 2동작이 강제된다 — userDirs가
    // 존재하면 활성 탭을 기본 폴더(1개) 뒤 = 첫 유저 dirs 항목 슬롯으로 연다.
    RefreshUserDirs();
    ResolveDirs();
    if (!userDirs_.empty() && dirs_.size() >= 2)
        dirIndex_ = 1;  // 기본 폴더 뒤 첫 유저 dirs 항목(탭 슬롯 — 캐논 3축
                        // 등호 승계: Win 671/WSL 648/posix 304·폰 648)
    RequestScan();
}

void ClientMusicApp::OnClose() {
#ifdef JK_MUSIC_SPATIAL_LEG
    // spatial leg 즉시 절단 — 소멸자(unique_ptr 멤버, ~ClientMusicApp)가 실제
    // 철거를 소유하지만 소리는 창이 닫히는 이 프레임에 끊는다(vplayer
    // ClosePlayer 수형에서 정지 측만 소비). 플레이어가 엔진보다 먼저
    // 정리된다(선언 순서 파괴 계약 — StreamPlayer가 SpatialEngine& 유지).
    legPlayer_.reset();
    legEngine_.reset();
#endif
    // 워커 join은 ~ClientMusicApp이 소유한다(Init 짝 원문 계약 — 생성 주기).
    // 커버 텍스처 정리(T3 — 갤러리 ReleaseThumbs 원문 수형): 행 풀+미리보기+
    // 스윕 대기 전부 — 텍스처는 renderer 소유가 아니라 SDL 소유라 명시 파괴.
    ReleaseSweepTextures();
    if (previewTex_) {
        SDL_DestroyTexture(previewTex_);
        previewTex_ = nullptr;
    }
    previewImg_ = LoadedImage();
    previewDirty_ = false;
    for (CoverSlot& s : covers_) ReleaseCoverTexture(s);
    covers_.clear();
    if (imguiReady_) {
        ImGui_ImplJKWindow_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
}

bool ClientMusicApp::PreProcessMessage(const JKEvent& ev) {
    ImGui_ImplJKWindow_ProcessJKEvent(ev);
    // #89 T2 — Timer 무조건 더티 관용구 없음(gallery/library 동형 수리):
    // 타이머 틱은 활동이 아니고 렌더 원 = 입력·테마(게이트 활동)+스캔 결과
    // 도착(OnIdle 도착 더티 — 이 앱의 유일 자발 더티).
    return true;
}

void ClientMusicApp::OnFrameCommitted() { frameDirty_ = false; }

void ClientMusicApp::OnIdle() {
    // 답신 펌프 — Run 루프의 매 이터레이션(vplayer/라이브러리 "응답 펌프의
    // 렌더 분리" 수형): T3 재생 위임 답신(스캔 도착과 독립 폴링 — 남의 답신은
    // 흘려보낸다). 무도착 즉귀라 무비용.
    PollReplies();
    // spatial leg pump 폴링 — 매 이터레이션(vplayer 응답 펌프의 렌더 분리
    // 수형): 폴링 자체는 무더티고, 재생 중(active — 유일 근거) 진행 표기의
    // 정수초가 넘어갈 때만 더티를 낸다(#89 진행 표기 갱신만 더티 — vplayer의
    // "재생 중 프레임마다 더티"[영상 예외 조항] 수형과의 차이는 오디오 leg에
    // 프레임 이미지가 없는 것 — 표기 분해 = 정수초로 승계). 무재생 leg는
    // 접촉도 없다. 단말 전이(Stop/Eos)의 더티는 SpatialStart/SpatialStop과
    // Eos 판정 지점이 소유한다.
    SpatialPump();
    // open 재청구 pacing(콜드 부팅 경기 폴백) — 발사 1발만 만들고 더티를
    // 금한다(표기 변화는 답신 수령에만; 시평형 전이면 분기 원문만).
    if (openRetries_ > 0 && openId_ == 0 && !openPath_.empty() &&
        std::chrono::steady_clock::now() >= openRetryAt_) {
        openId_ = SendQuery("app_tool", music::OpenRequestJsonPath(openPath_));
        if (!openId_) {  // 전송 실패(미연결 등) = 폴백 종료 — 사유 표기가 생기므로
            openRetries_ = 0;  //   이번 틱 더티(수취/내용 변화 계약의 사유 표기 몫)
            openPath_.clear();
            frameDirty_ = true;
        }
    }
    // 행 썸네일 채움 진행 중 틱(T3 — #89 T2 조건화 수형 — 갤러리
    // pendingThumbs_ 선례): pendingCovers_는 마지막 렌더 프레임에서 예산·풀
    // 상한으로 못 채운 행 요청 표식 — 채움 진행(도착 예정 내용)인 동안만
    // 프레임을 계속하고, 비어 있으면 접촉도 없다(도착 완료 프레임이 곧 마지막).
    if (pendingCovers_) frameDirty_ = true;
    // 스캔 결과 수취 — 워커의 도착 표식만 읽는다(뮤텍스 접촉은 수취 시 1회 —
    // 무도착 무비용). **도착+답신 수취 = 유일 더티**(#89 계약): 수취한 결과가
    // 현재 세대면 표기 갱신+frameDirty_=true, 세대 불일치(재청구가 이겼다)
    // 면 폐기만.
    // 면 폐기만.
    if (!scanDone_.load(std::memory_order_acquire)) return;
    bool adopted = false;
    {
        std::lock_guard<std::mutex> lk(scanM_);
        // 도착 멤버십 필터(#93 T2 — 결정 ④, 세대 게이트 앞 단): 도착 스냅샷
        // 루트가 현재 dirs_에 없으면 **제거된 폴더의 지연 도착** — 폐기+stderr
        // 진단 1행(정직 계약 — UI는 무변화·더티 없음). 세대 게이트(아래)는
        // 재청구 최신식의 가드지만 [제거]의 재청구가 세대를 이미 소진한 뒤
        // 도착하는 지연 결과는 세대 불일치로 무음 지나가는 길이 있어, 제거
        // 경계의 정직 진단은 이 루트 멤버십이 세대에 앞서 소유한다(도착하는
        // 모든 결과에 대해 검사 — 폐기 대상은 결과 박스 소각만, 채택 금지).
        if (std::find(dirs_.begin(), dirs_.end(), scanOutRoot_) ==
            dirs_.end()) {
            std::fprintf(stderr,
                         "[music] 도착 폐기 — 제거된 폴더의 스캔 결과(root=%s)\n",
                         scanOutRoot_.c_str());
            std::fflush(stderr);  // probe 파이프 원문 수급(버퍼 잔류 금지)
            scanOut_.clear();
        } else if (scanOutGen_ == scanSeq_) {
            tracks_ = std::move(scanOut_);
            scanOut_.clear();
            scanBusy_ = false;
            adopted = true;
        }
    }
    scanDone_.store(false, std::memory_order_release);
    if (adopted) frameDirty_ = true;  // 도착 프레임 1회 — 도착 외 더티 금지
}

void ClientMusicApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
    if (!imguiReady_) {
        if (!ImGui_ImplJKWindow_Init(renderer))
            return;
        imguiReady_ = true;
    }
    // T3 — 이번 프레임 세대 초기화: 커버 채움 예산·도착 표식 소각+프레임
    // 세대 증가(퇴출 산치의 동프레임 접촉 제외 — fix r1 수형). renderer_ 스태시
    // (shot 전체 보기·갤러리 EnsureThumbTexture 수형 — 텍스처 업로드가 SDL
    // 렌더러를 요구하는 곳은 여기뿐).
    pendingCovers_ = false;
    coverFillsThisFrame_ = 0;
    ++coverFrame_;
    renderer_ = renderer;
    ImGui_ImplJKWindow_NewFrame(1.0f / 60.0f, w, h);
    ImGui::NewFrame();
    BuildUi(w, h);
    ImGui::Render();
    ImGui_ImplJKWindow_RenderDrawData(ImGui::GetDrawData(), renderer);
    // 프레임말 텍스처 스윕(T3 — 갤러리 C1 원장의 예방 파견): 패널 닫힘·미리
    // 보기 재구성이 이 프레임 draw list가 이미 기록한 텍스처를 파괴하면
    // RenderDrawData가 해방된 SDL 텍스처로 돈다 — 파괴를 프레임말로 미룬다.
    ReleaseSweepTextures();
    // 다음 프레임 유발 — 갤러리 pendingThumbs_ 수형(채움 진행 중만): 선례의
    // 이 대입은 RenderAndCommit 뒤 OnFrameCommitted가 소각하기에 단독으로는
    // 다음 프레임을 만들지 못한다(게이트는 OnIdle 이후 판정) — 실제 동작
    // 경로는 OnIdle의 재청구(pendingCovers_ → frameDirty_ 배송 #89 조건화
    // 수형, 위)다. 이 대입은 선례 원문 유지(정직 표기 — 이원화 원장행).
    frameDirty_ = pendingCovers_;
}

std::string ClientMusicApp::SettingsText() const {
    // exe-dir state/settings.json 직독(ResolveDirs·RefreshUserDirs 공용 leg —
    // 원문 텍스트를 순수 스캐너/리졸버에 건넨다 — gallery ResolveDirs 수형).
    // 부재/읽기 실패 = 빈 텍스트 → 스캐너/리졸버가 기본 1건(fail-safe)으로
    // 떨어진다. #94 리뷰 확정 잠복 결함 수리(#93 T2 동승 흡수): 원존의
    // .string()+narrow fopen은 UTF-8 바이트를 Windows ACP로 재해석해 한글 exe
    // 설치 디렉터에서 판독이 열리지 않았다 — fs::path를 그대로 ifstream에
    // 건네는 통독 수형(#94 규약 "파일 터치점 = 네이티브": libstdc++ narrow→
    // fs::path는 UTF-8 strict — 2o-f/T2 원장 실측 — 이라 와이드 개방이 네이티브
    // 경로를 소유한다). 계약 무변조: 부재/개방 실패 = 빈 텍스트(fail-safe 동일).
    const std::filesystem::path kvPath =
        std::filesystem::path(ExeDir()) / "state" / "settings.json";
    std::string text;
    std::ifstream f(kvPath, std::ios::binary);
    if (f) {
        char chunk[2048];
        // read 실패 시 gcount는 부분 수취 바이트(EOF 직전 청크) — 0이면 종료.
        while (f.read(chunk, sizeof(chunk)) || f.gcount() > 0) {
            text.append(chunk, static_cast<size_t>(f.gcount()));
            if (!f) break;  // eof/failbit — 남은 read는 무수취(반복 무의미)
        }
    }
    return text;
}

void ClientMusicApp::RefreshUserDirs() {
    // 사용자 dirs만(기본 폴더는 리졸버가 자동 구성 — 원장 계약). T2 fix(M-2):
    // read leg는 quickjs 원독 폐기 → 저장소 스캐너 UserDirs 통일 — 기존 CP949
    // 문서의 항목도 수취된다(보존-가시; 표기는 바이트 그대로 — 폰트가 깨는
    // 것은 표기 한계 원장). 파싱 실패 = 빈 목록(뷰 전용 — 오류 비표기 계약).
    userDirs_ = music::store::UserDirs(SettingsText());
}

void ClientMusicApp::ResolveDirs() {
    // exe-dir settings.json 직독 → resolved dirs(기본 폴더 먼저+유저 dirs).
    // T2 fix(M-2 — read leg 인코딩 통일): quickjs 원독(DirsFromSettings) 대신
    // 저장소 스캐너(UserDirs)로 유저 dirs를 수취한다 — quickjs는 CP949 바이트
    // 혼입 문서를 전체 거부해 보존된 기존 항목이 무시됐다(보존-불-가시 — T1
    // fix r1 원장). MusicDirList 원문은 리졸버 원존(2m-a 계보)으로 남기고 이
    // 앱의 read leg만 쌍둥이 합성으로 통일 — 디폴트 선두+정규화 규약은
    // NormalizeDirsPure(바이트 투명 쌍둥이 — Slashize된 입력에서 동형, 2o-f
    // 실측 원문)로 승계한다. 표기는 바이트 그대로(레거시 CP949 항목 = 폰트
    // 깨짐 — 표기 한계 원장).
    const std::string text = SettingsText();
    std::vector<std::string> dirs;
    dirs.push_back(music::AudioDirFallback::Path(ExeDir()));
    for (std::string& user : music::store::UserDirs(text))
        dirs.push_back(std::move(user));
    dirs_ = music::store::NormalizeDirsPure(std::move(dirs));
    if (dirIndex_ < 0 || dirIndex_ >= static_cast<int>(dirs_.size()))
        dirIndex_ = 0;
}

void ClientMusicApp::RequestScan() {
    // 비동기 스캔 청구 — 이전 스캔이 아직 진행 중이어도 우편함은 1슬롯
    // 최신식: 이전 요청은 무시되고(단일 스레드가 최신 것을 다음에 받는다)
    // 그 사이 도착한 옛 결과 세대는 OnIdle 수취에서 폐기된다. 표시 목록은
    // 비운다 — 화면의 리스트는 언제나 **선택 루트의** 결과만 의미(탭 전환/
    // 새로고침에 잔광 방지), 진행 표시는 BuildUi의 "스캔 중..."이 맡는다.
    if (dirIndex_ < 0 || dirIndex_ >= static_cast<int>(dirs_.size())) {
        tracks_.clear();
        scanBusy_ = false;
        return;  // 방어선(탭 없음) — 빈 목록(독립 스캔 실패 계약과 동형 표기)
    }
    // fs::path 수용 가드(T2 — 표기 한계 원장의 스캔 leg 분기): narrow
    // std::filesystem 계층은 바이트를 UTF-8로 기수해 CP949 바이트에
    // filesystem_error를 던진다(2o-f 실측·프루브 재확인) — 워커 스레드
    // terminate 방지. 비유효 UTF-8 루트(레거시 CP949 항목)는 목록 표기·
    // 저장소 관리(추가/제거 — NormalizeDirsPure 왕복)는 계속되지만 스캔은
    // 못 건다 — status 1행 정직(데이터는 무손상, 스캔 leg의 표기 한계).
    // Utf8ToUtf16은 부적합 UTF-8 = 빈 문자열 fail-closed 계약이라 유효성
    // 검사로 재용한다(빈 루트는 위 방어선이 선행).
    if (jk::text::Utf8ToUtf16(dirs_[dirIndex_]).empty()) {
        status_ = "[!] 스캔 미지원 인코딩 폴더(레거시 CP949) — 표기·관리만 가능";
        tracks_.clear();
        scanBusy_ = false;
        frameDirty_ = true;  // 사유 표기 틱(내용 변화 계약 몫)
        return;
    }
    {
        std::lock_guard<std::mutex> lk(scanM_);
        scanRoot_ = dirs_[dirIndex_];
        ++scanSeq_;
        scanRequestOpen_ = true;
    }
    scanBusy_ = true;
    tracks_.clear();
    // 커버 실패 슬롯의 재시도 게이트 세대(T3): 재스캔 = 새 조명이라 부재 슬롯
    // (파일 변칙·ReadCover 사유)도 1회 재시도 간다(갤러리 failedGen scanGen_
    // 수형). 성공 슬롯은 키(path,mtime) 부정합만 재충 — 세대와 무관.
    ++coverGen_;
    scanDone_.store(false, std::memory_order_release);  // 옛 도착 표식 소각
    scanCv_.notify_all();
}

void ClientMusicApp::ScanWorker() {
    // 단일 상시 워커 — 요청 대기(cv)→ListAudioFiles(순수 파트 원문 규약:
    // 재귀 전 트리+ec 중립형+재귀 가드 fix r1)→결과 항적+도착 표식. UI/
    // imgui/SDL 무접촉(계약). 세대는 채택 시 UI측 대조 원문(워커는 세대를
    // 결과에 심기만 한다).
    for (;;) {
        std::string root;
        unsigned long long gen = 0;
        {
            std::unique_lock<std::mutex> lk(scanM_);
            scanCv_.wait(lk, [&] {
                return scanQuit_.load() || scanRequestOpen_;
            });
            if (scanQuit_.load()) return;
            root = scanRoot_;
            scanRequestOpen_ = false;
            gen = scanSeq_;
        }
        // 취소 요청 리셋 — 요청 수취 시점(#93 T2): [제거] 성공/파괴가 세운
        // scanCancel_은 이전 세대 진행분만 절단해야 한다. UI 스레드가 취소 1발+
        // 재청구를 한 흐름으로 세운다(같은 스레드 — 순서 보장)이고 워커의 리셋은
        // 수취 뒤라 경합 없다. 파괴 경기(리셋 뒤에 quit가 세워지는 순간)는 콜백이
        // scanQuit_도 함께 읽어 봉합 — 다음 경계 1회 내 절단(소멸자 주석 원문).
        scanCancel_.store(false, std::memory_order_release);
        // 스캔 leg — throw 보호선(std::terminate 방어): narrow fs::path는
        // UTF-8 기수(CP949 바이트에 던진다 — 2o-f 실측)라 원칙적으로 진입은
        // RequestScan 가드가 막지만, 그 밖의 filesystem_error류(파일명 변질
        // 등 플랫폼 엣지)에서도 워커 스레드 사망 전체 앱 terminate가 되지
        // 않게 0건 표현(스캔 계약 "독립 실패 = 빈 목록")으로 흡수한다.
        std::vector<music::Track> out;
        try {
            // 취소판 소비(#93 T2 — T1 ListAudioFiles(root, cancel) 계약):
            // 콜백 = scanCancel_ ∥ scanQuit_(참=절단) — 경계(디렉터 진입+엔트리
            // 256 보조)마다 읽히고 절단 시 부분 수집 폐기 → 빈 목록(도착 후의
            // 세대·멤버십 폐기가 후속). **외부 삭제(스캔 중 디렉터 소멸)는 ec
            // 중립형 원존 그대로**(ListAudioFiles가 이미 소유 — dir 열기 실패
            // 독립 실패·열거 중 소멸 성분 스킵 — 2p-③·2m 원존, #93 결정 ⑤
            // 접촉 없음).
            out = music::ListAudioFiles(root, [this] {
                return scanCancel_.load(std::memory_order_acquire) ||
                       scanQuit_.load(std::memory_order_acquire);
            });
        } catch (...) {
            out.clear();
        }
        {
            std::lock_guard<std::mutex> lk(scanM_);
            scanOut_ = std::move(out);
            scanOutRoot_ = root;  // 도착 필터 동봉(#93 T2 — 루트 스냅샷)
            scanOutGen_ = gen;
        }
        scanDone_.store(true, std::memory_order_release);
    }
}

// ---- 폴더 관리 (T2 — MusicDirStore 소비) ----

jk::music::DirWriteResult ClientMusicApp::DirAddManaged(const std::string& path) {
    // UI [추가] 버튼과 music_dir_add 도구의 단일 경로. 입력 인코딩 규약
    // (하드 — T2): path는 UTF-8(앱 도구 계약 T3 원문·InputText 자연형) —
    // AddDir는 바이트 투명 저장(전 축의 std::filesystem native형 = UTF-8 —
    // 헤더 원문 주석). 가드 2검은 저장소 방어선(기본 폴더·공문자 거부)의
    // 호출부 쌍둥이 — 사유 표기를 사용자 표기로 먼저 만든다.
    jk::music::DirWriteResult r;
    const std::string p = TrimTrailSeparators(path);
    const std::vector<std::string> one =
        music::store::NormalizeDirsPure({music::Slashize(p)});
    if (one.empty() || one[0].empty()) {
        r.err = "empty dir";
        status_ = "[!] 폴더 경로가 비어 있습니다";
        frameDirty_ = true;
        return r;
    }
    if (one[0] == music::AudioDirFallback::Path(ExeDir())) {
        r.err = "default dir not appendable";
        status_ = "[!] 기본 폴더(state/music)는 추가할 수 없습니다";
        frameDirty_ = true;
        return r;
    }
    r = music::store::AddDir(ExeDir(), p);
    if (r.ok) {
        status_ = "폴더 추가됨 — 목록 재스캔";
        dirPathBuf_[0] = '\0';  // 성공 입력 소각 — 동일 경로 재추가 유도 방지
        RefreshUserDirs();
        ResolveDirs();
        RequestScan();          // Store 성공 = 재스캔(브리프 계약)
    } else {
        status_ = "[!] 폴더 추가 실패 — " + r.err;  // Store err 1행 정직(무음 금지)
    }
    frameDirty_ = true;
    return r;
}

jk::music::DirWriteResult
ClientMusicApp::DirRemoveManaged(const std::string& path) {
    // UI 행 [제거] 버튼과 music_dir_remove 도구의 단일 경로(DirAddManaged
    // 쌍둥이 — 같은 가드·정직 규약). 기본 폴더는 패널 목록(UserDirs)에 애초
    // 없지만 저장소 방어선과의 대칭으로 호출부 가드도 둔다.
    jk::music::DirWriteResult r;
    const std::string p = TrimTrailSeparators(path);
    const std::vector<std::string> one =
        music::store::NormalizeDirsPure({music::Slashize(p)});
    if (one.empty() || one[0].empty()) {
        r.err = "empty dir";
        status_ = "[!] 폴더 경로가 비어 있습니다";
        frameDirty_ = true;
        return r;
    }
    if (one[0] == music::AudioDirFallback::Path(ExeDir())) {
        r.err = "default dir not removable";
        status_ = "[!] 기본 폴더(state/music)는 제거할 수 없습니다";
        frameDirty_ = true;
        return r;
    }
    r = music::store::RemoveDir(ExeDir(), p);
    if (r.ok) {
        status_ = "폴더 제거됨 — 목록 갱신";
        // 취소 1발(#93 T2 — 결정 ③): 진행 중 스캔을 경계서 절단한다. 워커가
        // 요청 수취 때 리셋하므로 바로 뒤의 재청구(기존 RequestScan 계약그대로)
        // 는 취소를 먹지 않는다. 제거된 루트의 지연 도착은 멤버십 필터(OnIdle)
        // 가 폐기한다 — 여기는 절단 요청 1발이 전부(파괴 join 전 1발과 같은
        // 수형 — 소멸자 주석 원문).
        scanCancel_.store(true, std::memory_order_release);
        RefreshUserDirs();
        ResolveDirs();          // dirIndex_ 클램프 포함(삭제 탭이 활성이면 0)
        RequestScan();
    } else {
        status_ = "[!] 폴더 제거 실패 — " + r.err;
    }
    frameDirty_ = true;
    return r;
}

// ---- 폴더 브라우저 (#97 T2 — 플랜 2026-10-11-music-dir-browser) ----

void ClientMusicApp::BuildFolderBrowser() {
    // 브라우즈 패널 — 폴더 전용(플랜 결정 ② — 파일 목록 숨김, 확장자 필터와의
    // 혼동 방지). 목록은 **매 프레임 ListSubdirs 원문 재구성**(T1 순수 부품 —
    // 결정론 정렬·ec 중립형·.도트 스킵): 이 패널이 그려지는 프레임은 이미
    // 사유(입력·도착·진행 표기)가 있는 프레임뿐이라(#89 게이트) 프레임당 1회
    // 단일 디렉터 스캔은 비용이 유휴에 흐르지 않는다.
    //
    // 행 순서 원장(fix r1 — T3 런 E/F 실측 흡수): 원존은 cwd 1행 → ".." 행 →
    // 목록 child(128) → [이 폴더 추가]/[닫기] 행이었다 — [이 폴더 추가]가
    // 클라 y≈442-457(흐름 최하단 = tracks child 음수 높이 압착 경계)에 떨어져
    // 합성 클릭 25 후보 × 런 5회 전부 무전달(서버 relay sent=ok·클라 수취
    // frames=0의 단절 원문). **③④ 행을 목록 위로 옮긴다** — 계약 ①②③④의
    // 존재 순서는 유지하되 공간 순서가 아닌 도달성이 원장: 클릭이 관측 전달
    // 성립 지대(하위 행 y≈324 원문 = 런 E/F 성립 축)에 상시 산다. ⑤의 판정
    // 축(도구 진실원)은 변하지 않는다.
    if (browseCwd_.empty())
        return;  // 방어선(개봉 가드가 비움 — 레거시 CP949 거부 등)
    // ① cwd 표기 1행 — 전문 단일 행(개행 없음). 창폭을 넘는 긴 경로는 잘림
    //    표기 한계 행(마우스 툴힌트가 전문을 다시 소유 — 위 .. 행 수형).
    ImGui::TextUnformatted(browseCwd_.c_str());
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", browseCwd_.c_str());
    // ② 상위 ".." 행 — 클릭 = 상위 이동(T1 Parent 원문 소비). 상위 부재(cwd가
    //    루트 — 드라이브 루트·"/"·단일 성분)는 행 대신 무안내 1행(T1 Parent의
    //    부자기 금지 계약을 UI가 그대로 소비 — 루트에서 눌러도 사라지는 행을
    //    만들지 않는다).
    const std::string parentPath = music::browse::Parent(browseCwd_);
    if (parentPath.empty()) {
        ImGui::TextDisabled("(루트 — 상위 없음)");
    } else {
        ImGui::PushID(1000000000);  // ".." 행과 subdir 행의 id 공간 분리
        if (ImGui::Selectable("..")) browseCwd_ = parentPath;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("상위 — %s", parentPath.c_str());
        ImGui::PopID();
    }
    // ③④ [이 폴더 추가]/[닫기] — 목록 앞(도달성 원장 상기).
    // ③ — DirAddManaged 단일 경로([추가] 버튼·music_dir_add 도구와 동일):
    //    가드 2검→Store.AddDir→성공 = status+입력 소각+RefreshUserDirs 1연+
    //    ResolveDirs+RequestScan(기존 재스캔 계약 원존)+frameDirty(기존 계약
    //    — 성공/실패 status 표기 모두). 성공 = 브라우즈 종료(계약 ③); 실패 =
    //    status 정직 1행으로 브라우저에 남는다.
    if (ImGui::Button("이 폴더 추가")) {
        const music::DirWriteResult r = DirAddManaged(browseCwd_);
        if (r.ok) {
            browseOpen_ = false;  // 추가 성공 = 브라우즈 종료(계약 ③)
            browseCwd_.clear();
        }
    }
    ImGui::SameLine();
    // ④ [닫기] — 상태 소각만(선택 없이 종료). 닫는 프레임은 클릭 = 입력
    //    이벤트 자연 귀결 — 더티 조작 없음(#89).
    if (ImGui::Button("닫기")) {
        browseOpen_ = false;
        browseCwd_.clear();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("브라우저 닫기 — 추가하지 않고 종료");
    // 하위 폴더 목록 — 클릭 = 진입(T1 JoinDir 원문 소비). 목록 child는
    // 버튼행의 나머지 공간을 소비한다(고정 128 — 원존 높이 계약 유지: 위의
    // 행·버튼이 관측 전달 지대에 상시 산다).
    if (ImGui::BeginChild("browselist", ImVec2(0, 128.0f),
                          ImGuiChildFlags_Borders)) {
        const music::browse::DirPage page =
            music::browse::ListSubdirs(browseCwd_);
        if (page.dirs.empty())
            ImGui::TextDisabled("(하위 폴더 없음)");
        for (size_t k = 0; k < page.dirs.size(); ++k) {
            // 행 사본 선취(P1 수형 재용 — #92 move/삭제 무효화 부류 예방 원장):
            // 클릭 핸들과 툴힌트가 원본 벡터가 아닌 사본을 쓴다. ListSubdirs는
            // 매 프레임 원문 재구성이라 구조적으로 move가 없지만, 관리 경로
            // (DirAddManaged — 성공 시 dirs_·userDirs_ move-대입 교체)와 같은
            // 프레임에서 행이 살아남는 전제를 부수지 않는다(수형 통일).
            const std::string name = page.dirs[k];
            // 진행 경로 사본(I1 fix r1 — 동프레임 툴힌트 드리프트 가드 1행):
            // JoinDir의 대상 행을 버튼 판정 **전**에 계산해 모은다 — 원존은
            // Selectable의 클릭 핸들에서 browseCwd_를 바꾼 뒤
            // IsItemHovered 툴힌트가 "새 cwd의 자식"을 그렸다(동프레임
            // 드리프트 — 클릭 프레임의 툴힌트가 클릭 전 경로를 말해야 하는
            // 정직 원장). JoinDir 2패 구성도 소각(매 프레임 문자열 재계산 1벌).
            const std::string dest =
                music::browse::JoinDir(browseCwd_, name);
            ImGui::PushID(static_cast<int>(k));
            if (ImGui::Selectable(name.c_str()))
                browseCwd_ = dest;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", dest.c_str());
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

// ---- 재생 위임 (T3 — 스펙 §4 D1) ----

uint32_t ClientMusicApp::SendQuery(const char* tool, const std::string& args) {
    // 위임 쿼리 발사 — ClientLibraryApp::SendQuery 원문 쌍둥이(라이브러리는
    // launch_app 1종이라 kind/arg 메타 대신 id만 적립한다 — 동일 계약이
    // music도 1종과 같다: launch_app+app_tool 둘 다 id 적립형). 0 = 미성립
    // (미연결·전송 실패 — 사유 표기, 더티는 사유가 뜨는 이번 틱에만).
    jk::client::JKClientSurface* surface = Surface();
    if (!surface || !surface->IsConnected()) {
        status_ = "[!] 서버에 연결되어 있지 않습니다";
        return 0;
    }
    const std::string json =
        "{\"tool\":\"" + std::string(tool) + "\",\"args\":" + args + "}";
    const uint32_t id = nextQueryId_++;
    if (!surface->SendAgentQuery(id, json)) {
        status_ = "[!] 전송 실패";
        return 0;
    }
    pending_.push_back(id);
    return id;
}

void ClientMusicApp::PlaybackDelegate(const music::Track& t) {
    // 더블클릭 핸들 — 브리프 T3 쿼리 쌍(스펙 §4 원문 재용): ① launch_app
    //({"app":"vplayer"} 리터럴 — 라이브러리 LaunchSelected 원문 쌍둥이; 서버
    //가 jkapp_vplayer<접미> 존재 검증을 소유) ② 이어서 app_tool open — 꾸러미
    //는 순수 부품 jk::music::OpenRequestJson 원문(windowId 미기술 — 단일 후보
    //= 직행). 두 요청은 같은 파이프에 ①→② 순서로 적힌다(작성 순서 = 서버 처리
    //순서). 콜드 부팅 경기(vplayer 도구 등록이 스폰+연결 뒤)는 open 답신의
    //unknown_app_tool 재청구 폴백(PollReplies)이 흡수한다. 쿼리 성립 자체는
    //표기 변화 없음 — 더티는 답신 수령에만(더블클릭 자체는 입력 활동이라 게이트
    //가 이미 프레임을 낸다). 재선택(이미 vplayer 창)도 이 원문 쌍이 그대로
    //간다 — 서버 launch_app은 무제한 별도 스폰(존재 검증 원문 — toggle 계열은
    //palette 전용 별도 툴), 2 인스턴스 중복 등록이면 릴레이의 ambiguous+후보
    //목록(자기교정)이 답해 클라는 추측 없이 표기로만 흡수한다(스펙 §4.2).
    if (t.full.empty()) {
        status_ = "[!] 재생 경로 없음";   // 방어선(빈 full 행은 스캔 계약상 없다)
        frameDirty_ = true;
        return;
    }
    launchId_ = 0;
    openId_ = 0;
    launchAborted_ = false;
    openPath_ = t.full;               // 재청구 폴백에 다시 전달할 사본
    openRetries_ = kOpenRetryMax;
    openRetryAt_ = std::chrono::steady_clock::now();
    launchId_ = SendQuery("launch_app", "{\"app\":\"vplayer\"}");
    openId_ = SendQuery("app_tool", music::OpenRequestJson(t));
    if (!launchId_) {
        // launch 미성립(전송 실패·거부) = 위임 폴백 성립 전제 상실 — 재청구
        // 상태는 여기서 취소(M-2). open이 나가 있다면 그 답신은 "사후 답신"
        // 겹침 가드(launchAborted_)로 무음 흡수한다(status 납치 방지).
        launchAborted_ = true;
        openRetries_ = 0;
        openPath_.clear();
        openId_ = 0;  // 사후 답신은 pending_ 정리 소각(mine 흡수 뒤 무사항 —
                      //   launchId_ 0 대조 원문도 없다 — 겹침 가드가 소유)
    }
    if (!launchId_ || !openId_) frameDirty_ = true;  // 미성립 사유 표기 틱
}

void ClientMusicApp::PollReplies() {
    // 답신 소비 — ClientLibraryApp::PollReplies 원문 쌍둥이(남의 답신은
    // 흘려보낸다 — 드레인 계약). 수령 틱의 표기 변화에만 더티(재청구 재발사는
    // OnIdle pacing이 소유하고 무더티 — #89: 도착·내용 변화 외 더티 금지).
    jk::client::JKClientSurface* surface = Surface();
    if (!surface) return;
    jk::client::AgentReply reply;
    while (surface->PollAgentReply(reply)) {
        bool mine = false;
        for (auto it = pending_.begin(); it != pending_.end(); ++it) {
            if (*it == reply.queryId) {
                pending_.erase(it);
                mine = true;
                break;
            }
        }
        if (!mine) continue;  // 우리 쿼리가 아니다 — 무시
        // 대분법 — 봉투 ok 무신 본문 재판정(fix r3 I-1/진원 수형): 즉답 경로는
        // 봉투 ok=1 고정(본문 거부와 무관), 지연응답만 봉투=앱 결과다 — 봉투만
        // 믿는 판정(fix r2까지)은 콜드 거부를 성공으로 읽어 폴백 크레딧을
        // 즉시 소멸시켰다(T4 결함 원장 — 폴백 자기 소멸). 대분법 자체는 순수
        // 부품(music::DelegationReplyVerdict) 소비 — 2m-h 원문 실측.
        const music::DelegationVerdict dv =
            music::DelegationReplyVerdict(reply.ok, reply.json);
        if (reply.queryId == launchId_) {
            launchId_ = 0;
            // ① 답신 — 본문 ok면 open이 이미 나가 있고(재청구 pacing 계속),
            // 거부면 런치 검증 실패(unknown_app — jkapp_vplayer 모듈 부재)
            // 표기+폴백 종료(open 재청구가 런치 성립을 전제한다 — 원문 쌍
            // 계약, fix r1 M-2: 사후 open 답신 겹침 가드도 여기서 세운다).
            if (!dv.ok) {
                status_ = dv.err.empty() ? "[!] vplayer 실행 거부"
                                         : "[!] vplayer 실행 거부 — " + dv.err;
                launchAborted_ = true;
                openPath_.clear();
                openRetries_ = 0;
                frameDirty_ = true;
            }
        } else if (reply.queryId == openId_) {
            openId_ = 0;
            if (launchAborted_) {
                // M-2 겹침 가드 — 런치 거부 후 도착한 사후 답신. 폴백은 이미
                // 종착(retries==0)이고 거부 안내가 status에 있다 — 이 답신은
                // 무음 회수만(status 납치·소진 표기 오인 방지), 더티 없음.
                openPath_.clear();
                openRetries_ = 0;
                continue;
            }
            // ② 답신 판정 (fix r3 — 본문 진실원; fix r1 I-2 계약의 본문계층
            // 승격): 본문 ok=false+error = 앱-수준 실패다 — 성공 분류는 본문
            // 판정이 소유한다.
            if (dv.ok) {
                status_ = "vplayer 재생 요청됨";   // 오픈은 비동기 — 진행은
                                                  //   vplayer 표면(get_status)
                openPath_.clear();
                openRetries_ = 0;
                frameDirty_ = true;
            } else if (dv.err == "unknown_app_tool") {
                // 콜드 부팅 경기 — 등록 전 릴레이(재청구 유일 대상 — 도구
                // 등록 경기 흡수 계약). 소진 크레딧(openRetries_)은 **답신
                // 수취에서만 소모**하고(발사인 OnIdle pace는 감법을 만들지
                // 않는다), 수취에서 0 도달 = 즉시 소진 전이(fix r2 I-1
                // 재수형): 소진 표기 1행+이번 틱 더티 1회(사용자 안내 = 상태
                // 변칙 몫)를 **수취 시점에**.
                if (openRetries_ > 0) {
                    if (--openRetries_ == 0) {
                        status_ = "[!] vplayer 응답 없음 — 재시도 " +
                                  std::to_string(kOpenRetryMax) + "회 소진";
                        openPath_.clear();  // 재발사 원문이 모두 막힌다
                        frameDirty_ = true; //   (openRetries_>0·openPath_ 있음) —
                    }                       //   openRetryAt_ 절화(시독 불요)
                    else {
                        // 다음 pacing 시각 갱신 — 재청구 대기(OnIdle 재발사),
                        // 표기 변화 없음 — 더티 금지.
                        openRetryAt_ = std::chrono::steady_clock::now() +
                                       kOpenRetryDelayMs;
                    }
                }
                // 크레딧 소진 뒤의 잔여 답신(방어선 — openPath_가 비어 재발사
                // 원문이 막혀 있어 원론적으로 도착하지 않는다)은 무음 흡수.
            } else if (dv.err == "ambiguous") {
                // 복수 후보(자기교정 원문 재용) — 추측 없이 표기로만.
                status_ = "[!] vplayer 창이 복수 — 하나 닫고 다시 시도";
                openPath_.clear();
                openRetries_ = 0;
                frameDirty_ = true;
            } else {
                // 본문 ok=false+그 밖 error(도구 실패 — 폴백 흡수 대상 아님)·
                // 본문 판정 실패 — 재청구 대상 아님, 즉시 종착+err 부기
                // (리트라이 무의미 — fix r1 I-2의 본문계층).
                status_ = dv.err.empty() ? "[!] 재생 위임 실패"
                                         : "[!] 재생 위임 실패 — " + dv.err;
                openPath_.clear();
                openRetries_ = 0;
                frameDirty_ = true;
            }
        }
    }
}

// ---- spatial 재생 leg (T2 — 스펙 2026-10-10 §2, T1 MusicSpatialLeg.h 소비) ----

bool ClientMusicApp::SpatialStart(const music::Track& t) {
    // 단일 발사 경로 — 행 [spatial] 버튼과 spatial_play 도구 모두 여기로.
    // 사건의 사실원(디바이스/디코더 관측)을 LegEvent로 번역해 T1 Apply 원문에
    // 넣는다(전이는 T1이 단독 소유).
    // M-3 가드(T1 리뷰 — play path 공문자 발사 금지): 버튼은 지원 행에만
    // 그려지지만 도구 경계는 인자 검증이 이 앱의 몫이라 동일 가드가 선행한다.
    if (t.full.empty()) {
        status_ = "[!] 재생 경로 없음";
        frameDirty_ = true;
        return false;
    }
    if (music::leg::LegSupport::OfExt(t.full) !=
        music::leg::LegSupport::State::Supported) {
        // D4 정직 표기 — 경로 셀의 버튼은 표 밖 행에 그려지지 않지만, 도구
        // 경계에서 온 경로는 여기가 유일 게이트다.
        status_ = "[!] spatial 미지원 포맷 — " + t.rel;
        frameDirty_ = true;
        return false;
    }
#ifndef JK_MUSIC_SPATIAL_LEG
    // env 미설정 빌드(fail-closed 원장 — T1 배선): leg 부재는 DeviceFailed와
    // 같은 UI 귀결(정직 계약). [spatial] 버튼은 표기 계층(T1 순수 부품)이라
    // 그려지되, 발사는 고장 종착으로 끝나고 상태 행은 kDelegationHint를
    // 그대로 라벨로 소비한다(D2/D5 — 표기가 없는 leg를 있는 것처럼 말하지
    // 않는다).
    legState_ = music::leg::Apply(legState_, music::leg::LegEvent::DeviceFailed);
    status_ = legState_.err;  // kDelegationHint 원문 라벨 소비
    frameDirty_ = true;
    return false;
#else
    std::string err;
    // ALC 디바이스 — lazy 1회 성립(부팅 무재생이 디바이스를 열지 않는다).
    // 실패는 즉시 폐기(reset — 다음 클릭이 다시 연다)로 정직 재시도를 허용.
    if (!legEngine_) {
        legEngine_ = std::make_unique<SpatialEngine>();
        if (!legEngine_->init(err)) {
            // 정리 순서 = 파괴 역선언순 원문(fix r1 I-1): 플레이어가 엔진을
            // 먼저 정리한다 — StreamPlayer가 SpatialEngine&를 유지하며
            // destroy_al은 컨텍스트 소멸 전에 소스를 떼어야 한다.
            legPlayer_.reset();
            legEngine_.reset();
            legState_ = music::leg::Apply(legState_, music::leg::LegEvent::DeviceFailed);
            status_ = legState_.err;  // kDelegationHint 원문(디바이스 err 부기
                                      //   없음 — 라벨 소비 계약의 원문 유지)
            frameDirty_ = true;
            return false;
        }
        legPlayer_ = std::make_unique<StreamPlayer>(*legEngine_);
    }
    // 디코더 열기 — 확장자 4행 판별(wav/mp3/flac/ogg)이 실제 진실원: D4 표는
    // 5종을 말하지만 리터럴 .vorbis 파일은 여기서 "unsupported format"으로
    // 거부된다(I-1 원장 — 표기 문구는 "ogg(vorbis 코덱)" 1행 정리로 해소).
    // #94 경로 인코딩 사슬(규약 — 와이어/저장/표기 = UTF-8, 파일 터치점 =
    // 네이티브 변환): Track.full은 스캔 leg의 UTF-8 원문(2q 수취 단정)이지만
    // 디코더(dr_wav/dr_mp3/dr_flac·stb_vorbis 내부 narrow fopen)는 Windows에서
    // 그 바이트를 ACP로 해석한다 — Decoder::open에 네이티브 바이트로 변환해
    // 전달한다(spatial-player 무변경 계약: 변환은 이 앱 경계의 몫). 표기와
    // 전이(아래 legState_.path)는 UTF-8 원문을 유지한다. posix는 무변환 항등
    // (경로 인코딩 = 파일시스템이 소유 — Utf8ToAnsi 계약)이라 동일 소비 수형이
    // 양축 하나로 성립한다. 부적합 UTF-8 = 빈 변환(fail-closed 승계)은 열기
    // 고장과 같은 귀결로 정직 종착한다.
    const std::string nativeFull = jk::text::Utf8ToAnsi(t.full);
    bool opened = false;
    if (nativeFull.empty()) {
        err = "경로 인코딩 변환 실패";
    } else {
        opened = legPlayer_->open(nativeFull, err);
    }
    if (!opened) {
        // 고장 종착은 ALC 전용(DeviceFailed — T1 전이 명세)이라 디코더 고장은
        // 정지 종착(StopRequested — deviceOk 보존·active 해제)으로 분류하고
        // 고장 원문은 status에 부기한다(사건 분류 = 이 앱의 몫). path 클리어
        // (fix r1 I-2): open이 destroy_al로 이전 소스를 먼저 파괴하므로
        // 실패 시점에 "열려 있는" 경로는 더 이상 없다 — 이전 재생 경로의
        // 잔존 표기는 사실과 반대다(T1 Apply의 path 보존은 순수 전이 계약이고,
        // 사실원인 이 앱이 사건 뒤 fact 정리를 소유한다).
        if (legState_.active)
            legState_ =
                music::leg::Apply(legState_, music::leg::LegEvent::StopRequested);
        legState_.path.clear();
        status_ = "[!] spatial 열기 실패 — " + err;
        frameDirty_ = true;
        return false;
    }
    // Start의 전제(호출부가 미리 채워 넣는다 — T1 원문): path/dur는 전이 앞에.
    legState_.path = t.full;
    const int sr = legPlayer_->decoder().fmt().sample_rate;
    legState_.durSec =
        sr > 0
            ? static_cast<double>(legPlayer_->decoder().fmt().total_frames) /
                  static_cast<double>(sr)
            : 0.0;  // durSec=0 = 길이 미상(MP3 — M-2: 표기 가드는 BuildUi 몫)
    legState_ = music::leg::Apply(legState_, music::leg::LegEvent::Start);
    legShownSec_ = -1;  // 표기 원점 재설정 — 다음 정수초 경계로 1프레임
    legPlayer_->set_position(legAzimuth_, legDistance_, legElevation_);
    legPlayer_->play();
    status_ = "spatial 재생 — " + (t.rel.empty() ? t.full : t.rel);
    frameDirty_ = true;
    return true;
#endif
}

void ClientMusicApp::SpatialStop() {
    // 정지 성공 종착 — player stop(AL stop+큐 drain+처음 프레임 리와인드)와
    // T1 StopRequested 전이(최후 위치 표기 보존). 정지는 고장이 아니다(M-3
    // 아님 — idempotent: idle leg에 대한 stop도 성공 종착이다).
    if (!legState_.active) return;
#ifdef JK_MUSIC_SPATIAL_LEG
    if (legPlayer_) legPlayer_->stop();
#endif
    legState_ = music::leg::Apply(legState_, music::leg::LegEvent::StopRequested);
    legShownSec_ = -1;
    status_ = "spatial 정지";
    frameDirty_ = true;
}

void ClientMusicApp::SpatialPump() {
#ifdef JK_MUSIC_SPATIAL_LEG
    // 무재생 = 접촉도 없다(idle 계약 #89 — active가 pump 폴링의 유일 근거).
    if (!legState_.active || !legPlayer_) return;
    legPlayer_->pump();
    const int sr = legPlayer_->decoder().fmt().sample_rate;  // open 성립 보장
    legState_.posSec =
        sr > 0 ? static_cast<double>(legPlayer_->current_frame()) /
                     static_cast<double>(sr)
               : legState_.posSec;
    if (!legPlayer_->is_playing()) {
        // 자연 종료(디코더 EOS — prefill 고갈 뒤 AL_STOPPED). pause는 이 leg가
        // 부여하지 않아 is_playing false = Eos의 유일 사실원. 조기 종료
        // (truncated 판정 — stream_player §5 원문)는 원문 부기.
        if (legPlayer_->has_error())
            status_ = "[!] spatial 재생 조기 종료 — " + legPlayer_->error();
        legState_ = music::leg::Apply(legState_, music::leg::LegEvent::Eos);
        legShownSec_ = -1;
        frameDirty_ = true;  // 진행 표기의 마지막 변화(pos→dur 끝까지)
        return;
    }
    // 진행 표기 갱신만 더티 — 표기 분해(정수초)가 넘어갈 때 1프레임.
    const long long shown = static_cast<long long>(legState_.posSec);
    if (shown != legShownSec_) {
        legShownSec_ = shown;
        frameDirty_ = true;
    }
#endif
}

// ---- 앱 도구 허브 소비 (spatial_play/stop/status — vplayer 원문 수형) ----

bool ClientMusicApp::OnAgentToolCall(const std::string& tool,
                                     const std::string& argsJson,
                                     std::string& out) {
    // 인자 검증은 앱이 한다(서버는 패스스루 계약 — vplayer OnAgentToolCall
    // 원문 주석). fix r1 I-3 정직 정정 — vplayer 원문의 "블로킹 I/O 금지"를
    // 그대로 옮기는 것은 과대: spatial_play는 디코더 open+최초 ALC 디바이스
    // open을 프레임 스레드에서 한다(수십 ms급 실해 — 설계 의도: leg는 vplayer의
    // 워커 스레드 열기와 다른 원장·스펙 §2 [spatial] 클릭 발사 원문). 그럼에도
    // 무한 대기류(ALC 콜백·네트워크)는 없다 — 파일 I/O+디바이스 핸드셰이크만.
    // play 실패 경로는 status_ 원문으로 종착한다.
    jk::agent::AgentJson args(argsJson);
    if (tool == music::leg::kToolPlay) {
        std::string path;
        if (!args.ok() || !args.GetStr("path", path) || path.empty()) {
            out = "{\"error\":\"bad_args\",\"need\":\"path\"}";  // M-3 원문 가드
            return false;
        }
        music::Track t;
        t.full = path;  // T1 ToolJson이 '\'→'/' 정규화해 실은 원문(도구 허브
                        //   릴레이 파서 통과형 — 리뷰 §3 확증)
        // 말단 성분은 바이트 스캔(N-1 — T2 fix r1, terminate 4검 부기): 도구
        // 인자는 UI 입력과 달리 서버 릴레이 외부 원문이라 CP949 바이트 주입이
        // 가능하다 — fs::path narrow 계층은 UTF-8 기수라 CP949 바이트에
        // filesystem_error를 던져(2o-f 실측) UI 스레드 terminate가 됐다.
        // LastPathSegment는 유효 경로에서 fs::path filename과 동일 표기
        // (표기 전용 필드 — 행 상태/도구 결과의 rel 몫).
        t.rel = LastPathSegment(path);
        const bool started = SpatialStart(t);
        if (!started) {
            out = "{\"error\":\"start_failed\",\"detail\":\"" +
                  EscapeJson(status_) + "\"}";
            return false;
        }
        out = "{\"accepted\":true}";  // 비동기 — 진행은 spatial_status
        return true;
    }
    if (tool == music::leg::kToolStop) {
        if (!legState_.active) {
            // idempotent 정지(정지는 고장이 아니다 — T1 StopRequested 계약의
            // 도구측 수형: 이미 정지 상태의 stop도 성공 원문).
            out = "{\"active\":false,\"idle\":true}";
            return true;
        }
        SpatialStop();
        out = "{\"active\":false}";
        return true;
    }
    if (tool == music::leg::kToolStatus) {
        // 관측 표면(스펙 §2 — probe/구두 조작 몫): T1 LegState의 필드 원문.
        // **절단 정직 가드(M-1 — fix r1)**: snprintf의 반환값은 "버퍼가 충분
        // 했다면 쓰였을 바이트 수"(음수=포맷 고장)라 필요 크기가 산출된다 —
        // 반환값 >= 버퍼 크기면 전문이 절단된 것(긴 path+이스케이프 2배 확장이
        // 실측 침입 경로): 절단된 악형 JSON을 내보내지 않고 path 필드를
        // 생략해 다시 조립하고 "truncated":true를 싣는다(정직 축소 전문 —
        // 필드 누락 자체가 관측 원문에 남는다).
        const char* act = legState_.active ? "true" : "false";
        const char* posl = legState_.positional ? "true" : "false";
        const char* dok = legState_.deviceOk ? "true" : "false";
        const std::string ePath = EscapeJson(legState_.path);
        const std::string eErr = EscapeJson(legState_.err);
        char buf[768];
        const int need = std::snprintf(
            buf, sizeof(buf),
            "{\"active\":%s,\"positional\":%s,\"deviceOk\":%s,"
            "\"pos\":%.3f,\"dur\":%.3f,\"path\":\"%s\",\"error\":\"%s\"}",
            act, posl, dok, legState_.posSec, legState_.durSec,
            ePath.c_str(), eErr.c_str());
        if (need < 0) {  // 포맷 고장 — 악형이 아닌 실패 원문(ok=false)
            out = "{\"error\":\"status_format_failed\"}";
            return false;
        }
        if (static_cast<size_t>(need) >= sizeof(buf)) {
            // 절단 — path 생략 재조립(수치·플래그 필드만이라 상수 크기 —
            // 필요 크기 산식 원문 유지).
            char small[256];
            const int need2 =
                std::snprintf(small, sizeof(small),
                              "{\"active\":%s,\"positional\":%s,\"deviceOk\":"
                              "%s,\"pos\":%.3f,\"dur\":%.3f,\"truncated\":true}",
                              act, posl, dok, legState_.posSec,
                              legState_.durSec);
            if (need2 < 0 || static_cast<size_t>(need2) >= sizeof(small)) {
                // 이론 미도달(고정 분해 수치 2개+불리언 3개) — 방어선: 절단
                // 표기만 남는 최소 전문.
                out = "{\"error\":\"status_overflow\",\"truncated\":true}";
                return true;
            }
            out = small;
            return true;
        }
        out = buf;
        return true;
    }
    // ---- 폴더 관리 도구 3종 (T2 — 브리프 계약, 브라우저 목적지 상태까지
    // 앱이 소유한 정직 응답 규약: 성공 {"ok":true}·실패 ok=false+error 1행).
    // **도구 호출도 단일 경로(DirAddManaged/DirRemoveManaged)로 들어온다** —
    // 가드·재스캔·status 표기가 UI 버튼과 동일(표기 한계 힌트: 도구 발행
    // 결과는 사용자 status 1행에도 남는다 — 정직 표기).
    if (tool == kToolDirAdd) {
        std::string path;  // UTF-8(앱 도구 계약 T3 원문 — quickjs가 UTF-8 원문
                           //   회수, 이스케이프 복호 포함)
        if (!args.ok() || !args.GetStr("path", path) || path.empty()) {
            out = "{\"error\":\"bad_args\",\"need\":\"path\"}";
            return false;
        }
        const jk::music::DirWriteResult r = DirAddManaged(path);
        if (!r.ok) {
            out = "{\"error\":\"add_failed\",\"detail\":\"" +
                  music::store::EscapeJsonStr(r.err) + "\"}";
            return false;
        }
        out = "{\"ok\":true,\"dir\":\"" + music::store::EscapeJsonStr(path) + "\"}";
        return true;
    }
    if (tool == kToolDirRemove) {
        std::string path;  // UTF-8(앱 도구 계약 T3 원문 — quickjs가 UTF-8 원문
                           //   회수, 이스케이프 복호 포함)
        if (!args.ok() || !args.GetStr("path", path) || path.empty()) {
            out = "{\"error\":\"bad_args\",\"need\":\"path\"}";
            return false;
        }
        const jk::music::DirWriteResult r = DirRemoveManaged(path);
        if (!r.ok) {
            out = "{\"error\":\"remove_failed\",\"detail\":\"" +
                  music::store::EscapeJsonStr(r.err) + "\"}";
            return false;
        }
        out = "{\"ok\":true}";
        return true;
    }
    if (tool == kToolDirList) {
        // 사용자 dirs만(기본 폴더는 리졸버가 자동 구성 — 원장 계약). read leg
        // 은 T2 fix(M-2)로 스캐너 통일(quickjs 원독 폐기 — CP949 문서의 기존
        // 항목도 수취). 경로 바이트는 원문 규약(EscapeJsonStr — 고바이트 뒤
        // 0x5C 리터럴 — CP949 후행 바이트의 이중화 손상 없음; cpp local
        // EscapeJson은 규약 밖이라 금지).
        const std::vector<std::string> dirs =
            music::store::UserDirs(SettingsText());
        std::string reply = "{\"ok\":true,\"dirs\":[";
        for (size_t k = 0; k < dirs.size(); ++k) {
            if (k) reply += ',';
            reply += '"';
            reply += music::store::EscapeJsonStr(dirs[k]);
            reply += '"';
        }
        out = reply + "]}";
        return true;
    }
    out = "{\"error\":\"unknown_tool\"}";
    return false;
}

void ClientMusicApp::BuildUi(int w, int h) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
    if (ImGui::Begin("music", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
        // The server reserves the top title band of every surface as
        // title-bar chrome — mouse-downs there never reach the client
        // (vplayer lesson 8, gallery 동형 — 밴드 산식 진실원).
        ImGui::SetCursorPosY(static_cast<float>(jk::text::AppContentTopOffset()));

        if (ImGui::Button("새로고침")) {
            ResolveDirs();
            RequestScan();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%d개 폴더", static_cast<int>(dirs_.size()));
        ImGui::SameLine();
        // 이름 필터(브리프 3) — 바뀐 프레임은 입력 이벤트가 게이트에서
        // 이미 낸다(타이핑 = 활동 — 자연 귀결, 조작 없음). MatchFilter는
        // 빈 필터 = 전부 참(2m-d 계약)이라 그대로 소비 — D5 확장으로
        // 파일명+곡명+아티스트를 매치(#99 T2)라 힌트가 스코프를 말한다.
        ImGui::SetNextItemWidth(170.0f);
        ImGui::InputTextWithHint("##filter", "곡명·아티스트 검색", filterBuf_,
                                 sizeof(filterBuf_));
        ImGui::SameLine();
        ImGui::TextDisabled("%d곡", VisibleTrackCount());
        ImGui::SameLine();
        // 위임 결과 표기(T3 — 라이브러리 status 원문 수형 TextWrapped): 답신
        // 수령 틱의 상태 전환만 채운다(라이브러리와 같은 수형).
        if (!status_.empty()) ImGui::TextWrapped("%s", status_.c_str());

        // Dir strip — resolved dirs in contract order (default music dir
        // first, user dirs after; NormalizeDirs 중복·빈 성분 제거済).
        // 갤러리 탭 원문 계약 승계: 라벨 = 말단 이름(긴 경로 오버플로 방지),
        // 전문 경로는 툴힌트, 말단 빈 수형(루트 "/")은 전문으로.
        ImGui::Separator();
        for (int i = 0; i < static_cast<int>(dirs_.size()); ++i) {
            if (i > 0) ImGui::SameLine();
            ImGui::PushID(i);
            // 말단 성분 라벨은 바이트 스캔(LastPathSegment) — fs::path 말단
            // 성분은 narrow 계층이 UTF-8 기수라 CP949 바이트 항목에 던진다
            // (2o-f 실측 — 표기 leg와 스캔 leg의 인코딩 경계, T2 원장).
            std::string tabLabel = LastPathSegment(dirs_[i]);
            if (tabLabel.empty()) tabLabel = dirs_[i];
            const bool active = (i == dirIndex_);
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      ImGui::GetStyleColorVec4(
                                          ImGuiCol_ButtonHovered));
            if (ImGui::Button(tabLabel.c_str())) {
                if (dirIndex_ != i) {
                    dirIndex_ = i;
                    RequestScan();  // 탭 전환 = 루트 전환 = 재스캔(브리프 계약)
                }
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", dirs_[i].c_str());
            if (active) ImGui::PopStyleColor();
            ImGui::PopID();
        }
        // [폴더 관리] 토글(스트립 옆 — 브리프 계약): 열 때 목록 1회 갱신.
        // 닫아도 목록 값은 남는다(다음 열 때 갱신 — Store 성공 경로도 갱신).
        ImGui::SameLine();
        if (ImGui::Button(dirsUiOpen_ ? "폴더 관리 ▾" : "폴더 관리 ▸")) {
            dirsUiOpen_ = !dirsUiOpen_;
            if (dirsUiOpen_)
                RefreshUserDirs();  // 열림 프레임의 목록 = 파일 진실원(fix r2:
        }                           //   [제거] 행이 존재하려면 반드시 필요)
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("settings music.dirs 폴더 추가/제거 — "
                              "music_dir_add/remove/list 도구와 같은 경로");
        if (dirsUiOpen_) {
            // 패널 — user dirs 목록(T2 fix M-2: 스캐너 read leg — 레거시 CP949
            // 문서의 기존 항목도 보인다(보존-가시); 행 표기는 바이트 그대로,
            // 폰트가 깨는 것은 표기 한계 원장)+경로 Edit(공문자·기본 폴더
            // 가드 — DirAddManaged의 2검)+[추가].
            if (ImGui::BeginChild("dirmg", ImVec2(0, 128.0f),
                                  ImGuiChildFlags_Borders)) {
                ImGui::TextUnformatted(
                    "사용자 폴더 (settings music.dirs — 기본 폴더 제외)");
                if (userDirs_.empty())
                    ImGui::TextDisabled("사용자 폴더 없음 — 경로를 입력하고 "
                                        "추가하세요");
                for (size_t k = 0; k < userDirs_.size(); ++k) {
                    // 행 사본 선취(fix r2 — T2 probe 실측 P1 크래시 수리): 관리
                    // 경로가 성공 시 RefreshUserDirs()로 userDirs_를 **move-대입
                    // 교체**한다(libstdc++ operator=(vector&&) = RHS 버퍼 수취+
                    // 구버퍼 해제) — 사본 없이는 같은 프레임의 ①인자 바인딩 ②
                    // SetTooltip(userDirs_[k].c_str())이 해제된 구버퍼의 파괴
                    // 성분을 읽는다(tcache가 _M_p를 독살해 strlen 무은 주소를
                    // 걷어 SEGSEGV — 세그폴트 원인 수형 실측, 리포트 원문).
                    const std::string row = userDirs_[k];
                    ImGui::PushID(static_cast<int>(k));
                    ImGui::BulletText("%s", row.c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("제거")) {
                        DirRemoveManaged(row);
                        ImGui::PopID();
                        break;  // 제거 1회/프레임 — 남은 행은 다음 프레임의
                    }           //   갱신된 userDirs_에서 다시 그린다(목록이
                                //   관리 경로 안에서 줄어든 만큼 시프트)
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", row.c_str());
                    ImGui::PopID();
                }
            }
            ImGui::EndChild();
            ImGui::SetNextItemWidth(
                ImGui::GetContentRegionAvail().x - 64.0f);
            ImGui::InputTextWithHint("##addir", "폴더 경로 (예: D:/음악)",
                                     dirPathBuf_, sizeof(dirPathBuf_));
            ImGui::SameLine();
            if (ImGui::Button("추가"))
                DirAddManaged(dirPathBuf_);

            // ---- 폴더 브라우저 (#97 T2 — 플랜 2026-10-11-music-dir-browser) ----
            // [찾아보기] 토글 — 타이핑 없는 폴더 선택(폰 키보드 불요 — 유일
            // 동기 원장). 개봉 cwd 시작점 = dirs_[dirIndex_](현재 탭 루트 —
            // 계약 ⑤; 기본 탭에서도 자연). 레거시 CP949 루트는 RequestScan과
            // 같은 Utf8ToUtf16 가드로 개봉 거부 — status 정직(ListSubdirs의
            // fs::path narrow 기수가 UTF-8이라 CP949 바이트 cwd를 건네면
            // filesystem_error throw 경로 — 2o-f 실측 승계; 스캔 leg와 같은
            // 유효성 검사 재용 — Utf8ToUtf16 빈 반환 fail-closed 계약).
            if (ImGui::Button(browseOpen_ ? "찾아보기 ▾" : "찾아보기 ▸")) {
                browseOpen_ = !browseOpen_;
                if (browseOpen_) {
                    // 개봉 가드 2검(fix r1 — 스캔 leg 동형): ①인코딩(레거시
                    // CP949 바이트 cwd는 fs::path narrow UTF-8 기수의
                    // filesystem_error throw 경로 — 2o-f 실측 승계,
                    // Utf8ToUtf16 빈 반환 fail-closed 계약 재용) ②광 드라이브
                    // 루트 형식(I3 fix r1 — "C:"는 ListSubdirs("C:")가 드라이브
                    // 현재 디렉터리를 건드리고 JoinDir이 "C:name" 드라이브-
                    // 상대 표기를 만드는 T1 함정 원문의 cwd 폼 — 슬래시
                    // 동반("C:/") 형식으로 재등록하라는 정직 1행).
                    bool valid =
                        dirIndex_ >= 0 &&
                        dirIndex_ < static_cast<int>(dirs_.size());
                    std::string deny;  // 개봉 거부 사유(빈값 = 합격)
                    if (valid) {
                        const std::string& root = dirs_[dirIndex_];
                        if (root.size() == 2 && root[1] == ':') {
                            // 광 드라이브 루트(I3 fix r1) — 개봉 거부
                            deny = "[!] 브라우저 개봉 불가 — 탭 루트가 광 "
                                   "드라이브 형식(C:)이라 X:/ 형식으로 "
                                   "재등록하세요";
                        } else if (jk::text::Utf8ToUtf16(root).empty()) {
                            // 부적합 UTF-8(CP949) — 스캔 leg 동형 거부
                            deny = "[!] 브라우저 미지원 인코딩 폴더(레거시 "
                                   "CP949) — 표기·관리만 가능";
                        } else {
                            browseCwd_ = root;
                        }
                    }
                    if (browseCwd_.empty() || !deny.empty()) {
                        browseOpen_ = false;  // 개봉 거부 — 브라우저는 닫힌 채
                        browseCwd_.clear();   // 거부 시 소각(fix r2 M1 — v1
                                              //   거부 계약 복원): 토글 폐쇄
                                              //   후 재개봉의 stale cwd(이전
                                              //   탭 경로)로 열렸던 회귀 소각
                        status_ = deny;
                        // 더티의 성격(M1 fix r1 문어 정정 — 동작 무변경):
                        // **부하성** — status_는 BuildUi 선두의 상태행에
                        // 그려진다(폴더 패널 코드보다 앞선 위치라 이 프레임의
                        // 선두 행은 **이전** status를 이미 그렸다) — 이번 렌더
                        // 중의 대입은 다음 프레임의 선두 행 표기 계약이고,
                        // frameDirty는 그 다음 프레임을 유발하는 하중이다
                        // (입력 프레임 뒤의 1프레임 지연 표기 원장).
                        frameDirty_ = true;
                    }
                }
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "현재 탭 하위를 클릭으로 탐색해 추가 — 상위 \"..\" 행은 "
                    "등록 루트 위로도 이동(T1 Parent 계약)");
            if (browseOpen_) BuildFolderBrowser();
        }

        // 태그 편집 패널(T3 — 스펙 §3): 행 [편집] 버튼의 전환 패널 — [폴더
        // 관리] 패널과 같은 동형 전환(같은 창·탭 — 클릭 = 입력 이벤트 자연
        // 귀결, 더티 조작 없음). [커버 선택] 브라우저가 열리면 그 아래의
        // 목록 child(128 — 브라우저 원존 높이)도 흐름에 이어진다. 패널이
        // 위를 압착하면 아래의 압착 가드(reserve)가 표 최소를 봉한다(원존).
        if (editOpen_) {
            BuildTagEditPanel();
            if (editPickOpen_) BuildCoverPicker();
        }

        ImGui::Separator();
        // 2분할 — 표(남는 높이)+spatial leg 패널(하단 고정 200px). 패널은
        // env 무관하게 늘 그려진다(버튼=표기 계층 T1 순수 부품 — leg 불가
        // 빌드의 발사는 DeviceFailed 고장 종착으로 끝나며 안내 원문이 그
        // 차이를 말해준다 — 정직 계약, header 원문).
        // 압착 가드(T2 fix r1 — T3 런 E/F 클릭 무전달의 진원 후보 (a)):
        // tracks child의 -200 고정폭 = "남는 높이 − 공간 패널 예약 200"인데
        // 위의 패널(폴더 관리+브라우저)이 열리면 남는 높이가 예약보다 작아진다
        // — CalcItemSize는 음수 child 높이를 ImMax(4, 남는 높이+음수)로 봉해
        // 4px 잔본을 만든다(원문 실측 — third_party/imgui/imgui.cpp
        // CalcItemSize 1.92.9b) — 그 경계의 행/버튼 배치가 관측 비도달 영역에
        // 떨어진다. 부족하면 표의 최소(헤더 1행+데이터 1행)를 봉하고 부족분은
        // spatial child가 흡수한다 — 예약 = clamp(남는 높이 − 표 최소, 4, 200):
        //   남는 500 → 예약 200(원존 동형·표 300)  남는 300 → 200·표 100
        //   남는 100 → 예약 44·표 56(최소 봉합)    남는 58 → 예약 4·표 54
        // 여유 상태(패널 폐쇄 표준)는 -200 원존과 완전 동형.
        const float availDown = ImGui::GetContentRegionAvail().y;
        const float reserve = std::min(
            200.0f, std::max(4.0f, availDown - kTrackMinHeight));
        if (ImGui::BeginChild("tracks", ImVec2(0, -reserve),
                              ImGuiChildFlags_Borders)) {
            if (scanBusy_) {
                // 진행 표시 — 비동기 스캔의 도착 대기(비운 목록 위 1행).
                // 도착은 OnIdle 수취(도착 더티)로 다음 프레임을 유발한다.
                ImGui::TextUnformatted("스캔 중...");
            } else if (tracks_.empty()) {
                // 빈 상태(브리프 4) — 스캔 0건 계약 문구(독립 실패·빈 루트
                // 구분 없음 — ListAudioFiles의 ok 플래그 불요 계약 승계).
                ImGui::TextUnformatted(
                    "트랙 없음 — 위 [폴더 관리]에서 폴더를 추가하세요");
            } else {
                BuildTrackTable();
            }
        }
        ImGui::EndChild();
        // spatial leg 패널(T2 — 아래쪽 200px): 표의 남는 높이가 위 표 소유.
        // **압착 경계 2차 가드(fix r1 — 1차 가드의 누설 봉합)**: (0,0)의
        // 잔여 의미는 "GetContentRegionAvail()"과 같다 — tracks child 54px+
        // 행 간격 9가 남는 높이(58)를 초과하면 잔여가 **음수**로 떨어진다
        // (CalcItemSize의 (0,0) → default_h = 음수 잔여 — 음수 높이 child =
        // T3 런에서의 클라 freeze·서버 relay 봉합·agentctl empty 전멸 원문의
        // 유일 신설 구조) — 남는 높이 4px 미만을 4px로 봉한다. 여유 상태는
        // (0,0) 원존과 완전 동형(잔여 = avail 그대로).
        const float availSpatial = ImGui::GetContentRegionAvail().y;
        if (ImGui::BeginChild("spatial",
                              ImVec2(0, availSpatial > 4.0f ? availSpatial
                                                            : 4.0f),
                              ImGuiChildFlags_Borders)) {
            BuildSpatialPanel();
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

void ClientMusicApp::BuildSpatialPanel() {
    // [spatial] leg 패널 — 두 재생 채널의 표기 계층(스펙 §2 정직 귀속). 표기
    // 데이터는 T1 LegState(유일 진실원)와 슬라이더 UI 상태뿐 — leg는 여기서
    // 아무것도 주장하지 않는다(표밖 확장자·고장·env 미설정 전부 정직 분기).
    ImGui::TextUnformatted("spatial leg");
    // 미사용 leg의 상시 안내 1행(공존 계약 표기 — D3: 더블클릭=vplayer 위임,
    // [spatial]=내장 leg).
    if (!legState_.active && !legState_.deviceOk && legState_.err.empty() &&
        legState_.posSec == 0.0) {
        ImGui::TextWrapped("%s",
            "행 더블클릭 = vplayer 위임 · 행 spatial 버튼 = 내장 spatial(HRTF 위치)");
    }
    // 고장 안내(DeviceFailed — kDelegationHint 원문 라벨 소비).
    if (!legState_.err.empty())
        ImGui::TextWrapped("%s", legState_.err.c_str());
    // 진행 행 — M-2 가드(durSec=0 = 길이 미상, MP3 등: pos/0 표기를 만들지
    // 않는다). 진행 막대는 dur 참값 있는 포맷에만(wav/flac/ogg) 그린다.
    char posBuf[16];
    FmtClock(static_cast<long long>(legState_.posSec), posBuf, sizeof(posBuf));
    if (legState_.active && legState_.durSec > 0) {
        char durBuf[16];
        FmtClock(static_cast<long long>(legState_.durSec), durBuf, sizeof(durBuf));
        ImGui::ProgressBar(
            static_cast<float>(legState_.posSec / legState_.durSec),
            ImVec2(-1.0f, 0.0f), "");
        ImGui::TextDisabled("진행 %s / %s", posBuf, durBuf);
    } else if (legState_.active) {
        ImGui::TextDisabled("진행 %s · 길이 미상", posBuf);
    } else if (legState_.posSec > 0.0 || legState_.durSec > 0.0) {
        ImGui::TextDisabled("정지 — 최후 위치 %s", posBuf);
    }
    // 조작 행 — 정지(재생 중만)+모드 토글(POSITIONAL/STEREO — legState_.
    // positional 소비; leg 성립 이력(deviceOk) 있어야).
    ImGui::BeginDisabled(!legState_.active);
    if (ImGui::Button("정지")) SpatialStop();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!legState_.deviceOk);
    bool wantPositional = legState_.positional;
    if (ImGui::Checkbox("POSITIONAL", &wantPositional) &&
        wantPositional != legState_.positional) {
        legState_.positional = wantPositional;
#ifdef JK_MUSIC_SPATIAL_LEG
        if (legPlayer_) {
            std::string merr;
            legPlayer_->set_mode(wantPositional
                                     ? PlaybackMode::Positional
                                     : PlaybackMode::Stereo,
                                 merr);
            if (!merr.empty()) status_ = "[!] 모드 전환 실패 — " + merr;
        }
#endif
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("끄면 STEREO(위치 무시 통과) — 위치 슬라이더가 무시된다");
    // 위치 3조(스펙 §2 — 재생 중 활성) — 값 변화 시 set_position 1회(드래그 =
    // 입력 이벤트 자연 귀결, 더티 조작 없음).
    ImGui::BeginDisabled(!legState_.active);
    ImGui::PushItemWidth(200.0f);
    if (ImGui::SliderFloat("방위각", &legAzimuth_, -180.0f, 180.0f, "%.0f°"))
        SpatialPositionChanged();
    if (ImGui::SliderFloat("거리", &legDistance_, 0.5f, 20.0f, "%.1f m"))
        SpatialPositionChanged();
    if (ImGui::SliderFloat("고도", &legElevation_, -45.0f, 45.0f, "%.0f°"))
        SpatialPositionChanged();
    ImGui::EndDisabled();
    ImGui::PopItemWidth();
    // D4 표기 1행 정리(I-1 원장 — 리터럴 .vorbis는 decoder 4행 판별에서
    // 거부되므로 표기가 코덱 소속을 정직하게 말한다).
    ImGui::TextDisabled("%s",
        "지원: mp3 · flac · ogg(vorbis 코덱) · wav — m4a·aac·wma는 vplayer 위임");
}

void ClientMusicApp::SpatialPositionChanged() {
#ifdef JK_MUSIC_SPATIAL_LEG
    if (legPlayer_)
        legPlayer_->set_position(legAzimuth_, legDistance_, legElevation_);
#endif
    // 드래그 자체는 입력 이벤트(게이트가 이미 프레임을 낸다 — 더티 조작 없음).
}

// ---- 태그 편집 패널+커버아트 표시 (T3 — 스펙 2026-10-25 §2·§3) ----

void ClientMusicApp::SweepPreview() {
    // 미리보기 텍스처의 프레임말 파괴 적립 — 동프레임 AddImage 원장(파괴를
    // RenderDrawData 뒤로 미룬다 — C1 원장의 예방 파견). 픽셀도 같이 소각.
    if (previewTex_) texSweep_.push_back(previewTex_);
    previewTex_ = nullptr;
    previewImg_ = LoadedImage();
}

void ClientMusicApp::ReleaseSweepTextures() {
    // 프레임말 파괴(RenderOverlay 끝 경계 — draw list가 기록을 마친 뒤):
    // 스윕 목록 전부. 미리보기가 낡은/닫힌 세션의 텍스처라 이 시점에 살아
    // 있으면(addImage 없는 파괴 경계) 함께 파괴한다.
    for (SDL_Texture* tex : texSweep_) SDL_DestroyTexture(tex);
    texSweep_.clear();
}

ClientMusicApp::CoverSlot* ClientMusicApp::FindCoverSlot(
    const std::string& full) {
    for (CoverSlot& s : covers_)
        if (s.full == full) return &s;
    return nullptr;
}

ClientMusicApp::CoverSlot* ClientMusicApp::AcquireCoverSlot(
    const std::string& full, long long mtime, long long size) {
    // 남는 슬롯(픽셀·텍스처 없음+실패 아님) 재용 — 상한 접촉 없이 소개.
    for (CoverSlot& s : covers_) {
        if (!s.tex && s.img.rgba.empty() && !s.failed) {
            ReleaseCoverTexture(s);
            s.full = full;
            return &s;
        }
    }
    if (covers_.size() < kCoverPoolMax) {
        covers_.emplace_back();
        CoverSlot& fresh = covers_.back();
        fresh.full = full;
        fresh.mtime = mtime;
        fresh.size = size;
        return &fresh;
    }
    // LRU 퇴출(gallery::PickLruVictim 원문 수형 — 이번 프레임 접촉분 전부
    // 제외: 동프레임 draw list의 텍스처 파괴 금지 — C1 원장). 후보 없음 =
    // 요청 유보 — placeholder 유지(파괴는 접촉 프레임 이후 틱).
    std::vector<long long> useFrames;
    useFrames.reserve(covers_.size());
    for (const CoverSlot& s : covers_) useFrames.push_back(s.useFrame);
    const int victimIx = PickCoverVictim(useFrames, coverFrame_);
    if (victimIx < 0) return nullptr;
    CoverSlot& victim = covers_[static_cast<size_t>(victimIx)];
    ReleaseCoverTexture(victim);
    victim = CoverSlot();
    victim.full = full;
    victim.mtime = mtime;
    victim.size = size;
    return &victim;
}

void ClientMusicApp::ReleaseCoverTexture(CoverSlot& slot) {
    if (slot.tex) {
        SDL_DestroyTexture(slot.tex);
        slot.tex = nullptr;
    }
}

void ClientMusicApp::FillCoverSlot(CoverSlot& slot, const music::Track& t) {
    // D7 — 지연 채움의 실제 몫: ReadCover(파일 접점 1건)+stb 디코드+nearest
    // 32px 축소. 키 성분도 여기가 단일 진실원(경계 재용이 슬롯 키를 완성한다).
    slot.mtime = t.mtime;
    slot.size = t.size;
    // 실패 = failed 슬롯(부재 표기 — placeholder·정직, 갤러리
    // FillThumbSlot 원문 수형) — 재시도는 새 재스캔 세대(coverGen_)의 조명에서
    // 1회. ReadCover가 던지지 않는다(fs::path 개방 — 2q 수형; CP949 바이트
    // 경로도 native 변환 성공) — 읽기 사슬 관용, catch 불요.
    ReleaseCoverTexture(slot);
    slot.img = LoadedImage();
    music::tags::CoverArt art;
    std::string err;
    if (!music::tags::ReadCover(t.full, art, err) || art.bytes.empty()) {
        slot.failed = true;
        slot.failedGen = coverGen_;
        return;  // 커버 부재 = placeholder(행 라벨 무변조 — 브리프)
    }
    // 디코드 — 메모리 버퍼(stb 조달품 재용). 파일 터치 0(#94 — 인코딩 사슬
    // 함정 회피: 메모리 접점이라 Utf8ToAnsi 불요).
    LoadedImage raw;
    if (!jk::LoadImageMemory(art.bytes.data(), art.bytes.size(), raw) ||
        raw.w <= 0 || raw.h <= 0) {
        slot.failed = true;
        slot.failedGen = coverGen_;
        return;
    }
    slot.img = ShrinkToBox(raw, kCoverThumbBox);
    slot.failed = false;
}

void ClientMusicApp::EnsureCoverTexture(CoverSlot& slot) {
    // 지연 업로드(갤러리 EnsureThumbTexture 원문 수형): 픽셀 있음+텍스처
    // 없음+렌더러 가시 때만 — 실패(희귀)는 다음 프레임 재시도.
    if (slot.tex || slot.img.rgba.empty() || slot.img.w <= 0 ||
        slot.img.h <= 0 || !renderer_)
        return;
    SDL_Texture* t = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                       SDL_TEXTUREACCESS_STREAMING,
                                       slot.img.w, slot.img.h);
    if (!t) return;
    SDL_UpdateTexture(t, nullptr, slot.img.rgba.data(), slot.img.w * 4);
    slot.tex = t;
}

ClientMusicApp::CoverView ClientMusicApp::RowCover(const music::Track& t) {
    // 행 셀 1장의 커버 관측 — 캐시 적중 = 무접촉(텍스처 승계). 미스 = 이
    // 프레임 예산 안에서 D7 채움(프레임당 kCoverFillPerFrame — 초과분은
    // placeholder+pendingCovers_ → 다음 틱 프레임). key = (full, mtime,
    // size) — 재스캔 뒤 mtime/size 부정합 = 재충(태그 쓰기의 커버 교체가
    // 행 표기에 도달하는 유일 경로).
    if (t.full.empty()) return CoverView();
    CoverSlot* slot = FindCoverSlot(t.full);
    if (slot) {
        if (slot->mtime == t.mtime && slot->size == t.size) {
            slot->useFrame = coverFrame_;  // 접촉 스탬프(LRU 원문)
            if (slot->failed) {
                // 실패 슬롯 — 세대 뒤 조명에서 1회 재시도(갤러리 수형); 같은
                // 세대의 반복 조명은 무비용 원문 회수(예산 미접촉).
                if (slot->failedGen != coverGen_) goto fill;
                return CoverView();
            }
            if (!slot->img.rgba.empty()) {
                EnsureCoverTexture(*slot);
                CoverView v;
                v.tex = slot->tex;
                v.w = slot->img.w;
                v.h = slot->img.h;
                return v;
            }
            goto fill;  // 슬롯 선취만 된 미채움(예산 소진 프레임의 잔여)
        }
        // 키 부정합(재작성) — 슬롯 리셋 후 재충(다음 흐름 = 신규와 동일 leg).
        ReleaseCoverTexture(*slot);
        *slot = CoverSlot();
        slot->full = t.full;
        slot->mtime = t.mtime;
        slot->size = t.size;
        goto fill;
    }
    slot = AcquireCoverSlot(t.full, t.mtime, t.size);
    if (!slot) {
        // 풀 상한+동프레임 — placeholder 유지(해당 행만 더티의 근거는 유지
        // — 진행 중 표식: 다음 틱에 1프레임 재요청, 갤러리 선례).
        pendingCovers_ = true;
        return CoverView();
    }
fill:
    if (coverFillsThisFrame_ >= kCoverFillPerFrame) {
        // 예산 소진 — 이번 프레임은 placeholder, 진행 표식만(Honest — 채움은
        // 이어진다 — pendingCovers_의 OnIdle 재청구가 다음 틱 프레임).
        pendingCovers_ = true;
        return CoverView();
    }
    ++coverFillsThisFrame_;
    slot->useFrame = coverFrame_;  // 채움 예정 접촉 — 동프레임 퇴출 제외 먼저
    FillCoverSlot(*slot, t);
    slot->useFrame = coverFrame_;
    if (slot->failed) return CoverView();  // 부재 = placeholder(세대 게이트)
    EnsureCoverTexture(*slot);
    CoverView v;
    v.tex = slot->tex;
    v.w = slot->img.w;
    v.h = slot->img.h;
    return v;
}

// ---- 태그 편집 패널 (스펙 §3 — [폴더 관리] 패널 동형 전환) ----

void ClientMusicApp::OpenTrackEditor(const music::Track& t) {
    // 개봉 단일 경로 — 행 [편집] 버튼(스펙 §3; 도구 경계는 쓰기만 소비 — 개봉
    // 도구는 v1 스코프 밖). 세션은 1건만(재개봉 = 이전 세션 소각 — 폴더 패널
    // 원존의 단일 상태 수형). 표기 원문 수형 — 입력 프레임 그 자체(더티 조작
    // 없음).
    CloseEditor();
    if (t.full.empty()) {
        status_ = "[!] 편집 경로 없음";
        frameDirty_ = true;
        return;
    }
    editPath_ = t.full;
    // 프리필 — 현값 원문(UTF-8 InputText 자연형). title 부재 = 파일명 폴백(D3
    // 표기 규약 승계 — fs::path stem 원문 수형); artist/album 부재 = 빈값
    // ("-" 표기는 열 원존라 편집 프리필에 심으면 원문이 죽는다 — Track 태그
    // 필드의 원문 보존 계약 승계).
    std::string pre =
        t.title.empty()
            ? std::filesystem::path(t.name).stem().string()
            : t.title;
    std::snprintf(editTitleBuf_, sizeof(editTitleBuf_), "%s", pre.c_str());
    std::snprintf(editArtistBuf_, sizeof(editArtistBuf_), "%s", t.artist.c_str());
    std::snprintf(editAlbumBuf_, sizeof(editAlbumBuf_), "%s", t.album.c_str());
    editOpen_ = true;
    editPickOpen_ = false;
    pickCwd_.clear();
    previewDirty_ = true;  // D7 — 패널 열림 1회 ReadCover(다음 렌더 프레임 몫 —
                           //   개봉 클릭 프레임의 draw list와의 충돌 없음)
}

void ClientMusicApp::CloseEditor() {
    // 세션 소각 — [취소]/[저장] 성공 공용. 텍스처 파괴는 프레임말 스윕(동프레임
    // AddImage 원장 — RenderOverlay의 ReleaseSweepTextures 소비).
    editOpen_ = false;
    editPath_.clear();
    editTitleBuf_[0] = '\0';
    editArtistBuf_[0] = '\0';
    editAlbumBuf_[0] = '\0';
    editCoverSel_.clear();
    editCoverErr_.clear();
    editSaveErr_.clear();
    editPickOpen_ = false;
    pickCwd_.clear();
    previewDirty_ = false;
    SweepPreview();
}

void ClientMusicApp::RebuildTagPreview() {
    // 미리보기 재구성 — D7 패널 열림 1회(내장 커버 = ReadCover)·커버 선택
    // 교체 1회(이미지 파일). 파괴는 스윕(위) — 재구성 자체는 픽셀/사유만
    // 채운다. 실패 = 미리보기 부재+editCoverErr_ 1행(정직 — 모호성 해소 (d)).
    // 이 함수는 **draw 전 경계**(BuildTagEditPanel 머리)에서만 불린다 —
    // previewTex_ 파괴(스윕 예약)와 이번 프레임 AddImage가 겹치지 않는다.
    SweepPreview();
    editCoverErr_.clear();
    if (editOpen_ && editPath_.empty()) return;  // 세션 상실 방어선
    LoadedImage dec;
    if (!editCoverSel_.empty()) {
        // 커버 선택 파일 — stb 파일 접점은 narrow char*(ACP 해석)라 #94 규약
        // 의 Utf8ToAnsi 수형(spatial leg의 디코더 경로 사슬 재용 — posix =
        // 항등). 실패 = 빈 변환(fail-closed) → 정직 사유.
        const std::string ansi = jk::text::Utf8ToAnsi(editCoverSel_);
        if (ansi.empty()) {
            editCoverErr_ = "이미지 경로 인코딩 변환 실패";
        } else if (!jk::LoadImageFile(ansi, dec) || dec.w <= 0 ||
                   dec.h <= 0) {
            editCoverErr_ = "이미지 열기 실패 — " + editCoverSel_;
        }
    } else {
        // 내장 커버 — ReadCover(파일 접점은 fs::path 개방 — 인코딩 자유).
        // "no_cover"(부재의 정상 경로)와 구조 사유를 같은 행에서 갈라 쓴다
        // (정직 — 모호성 해소 (d): "미리보기 부재+err 1행 표기").
        music::tags::CoverArt art;
        std::string err;
        if (music::tags::ReadCover(editPath_, art, err) && !art.bytes.empty()) {
            if (!jk::LoadImageMemory(art.bytes.data(), art.bytes.size(),
                                     dec) ||
                dec.w <= 0 || dec.h <= 0)
                editCoverErr_ = "커버 이미지 디코드 실패";
        } else {
            editCoverErr_ = err.empty() || err == "no_cover"
                                ? "커버 부재 — 삽입하려면 [커버 선택]"
                                : "커버 읽기 실패 — " + err;
        }
    }
    if (dec.w > 0 && dec.h > 0 && !dec.rgba.empty())
        previewImg_ = ShrinkToBox(dec, kCoverPreviewBox);
    if (previewImg_.rgba.empty() && editCoverErr_.empty())
        editCoverErr_ = "미리보기 없음";  // 방어선(정직 — 무음 계약 금지)
}

void ClientMusicApp::EnsurePreviewTexture() {
    // 지연 업로드(갤러리 전체 보기의 texture_ 수형): 픽셀 있음+텍스처 없음+
    // 렌더러 가시. 텍스처는 축소 완료판(96 박스) — 원본 전체 크기의 GPU 사본을
    // 만들지 않는다(폰 SW 렌더러의 대형 텍스처 비용 회피 — 모호성 해소 (b)).
    if (previewTex_ || previewImg_.rgba.empty() || previewImg_.w <= 0 ||
        previewImg_.h <= 0 || !renderer_)
        return;
    previewTex_ = SDL_CreateTexture(
        renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
        previewImg_.w, previewImg_.h);
    if (!previewTex_) return;  // 희귀 — 다음 프레임 재시도(더티는 진행 표기 몫)
    SDL_UpdateTexture(previewTex_, nullptr, previewImg_.rgba.data(),
                      previewImg_.w * 4);
}

void ClientMusicApp::SaveTagEdit() {
    // [저장] — 쓰기의 단일 경로(스펙 §3·브리프 "저장 사슬은 WriteTag 1콜로
    // 완결"): 원자적 tmp+rename은 WriteTag 내장 — UI는 추가 파일 처리를 하지
    // 않는다. 성공 = status 표기+재스캔 재청구(기존 RequestScan 배선 재용 —
    // 취소 계약 무접촉)+패널 닫힘; 실패 = 패널 err 1행(정직 — 세션 유지, 입력
    // 값 사존). 스캔 진행 중 = 호출 전 게이트(BuildUi — [저장] 비활성)인데도
    // 도구 경계성 사건(같은 프레임의 재청구 등)에 대비한 방어선 유지.
    if (editPath_.empty()) return;
    if (scanBusy_) {
        editSaveErr_ = "스캔 중 — 저장 대기";
        frameDirty_ = true;
        return;
    }
    music::tags::TagSet t;
    t.title = editTitleBuf_;
    t.artist = editArtistBuf_;
    t.album = editAlbumBuf_;
    music::tags::CoverArt cover;
    const music::tags::CoverArt* coverPtr = nullptr;
    if (!editCoverSel_.empty()) {
        // 커버 지정 — 이미지 파일 원문 수취(파일 터치 = 네이티브 개방 — #94;
        // ifstream fs::path — ParseTag 원문 수형)+mime = 확장자 정규화
        // (브리프 필터와 정합 — .jpg/.jpeg/.png만 통과가 가능한 경로).
        namespace fs = std::filesystem;
        const fs::path p(editCoverSel_);
        std::error_code ec;
        const auto sz = fs::file_size(p, ec);
        std::ifstream f(p, std::ios::binary);
        if (ec || !f.is_open() || sz <= 0) {
            editSaveErr_ = "커버 파일을 읽을 수 없습니다 — " + editCoverSel_;
            frameDirty_ = true;
            return;
        }
        if (static_cast<unsigned long long>(sz) > music::tags::kTagWriteMax) {
            editSaveErr_ = "커버 파일이 너무 큽니다(16MiB 상한)";
            frameDirty_ = true;
            return;
        }
        cover.bytes.resize(static_cast<size_t>(sz));
        f.read(reinterpret_cast<char*>(cover.bytes.data()),
               static_cast<std::streamsize>(sz));
        if (f.gcount() != static_cast<std::streamsize>(sz)) {
            editSaveErr_ = "커버 파일 read 실패";
            frameDirty_ = true;
            return;
        }
        cover.mime = CoverMimeOfExt(p.extension().string());
        coverPtr = &cover;  // 지정 = 교체(D8), null = 기존 유지(WriteTag 계약)
    }
    std::string err;
    if (music::tags::WriteTag(editPath_, t, coverPtr, err)) {
        status_ = "태그 저장 — 재스캔";
        CloseEditor();     // 성공 = 패널 닫힘(텍스처는 프레임말 스윕)
        RequestScan();     // 재스캔 재청구(기존 배선 재용 — 취소 계약 원존)
    } else {
        editSaveErr_ = err.empty() ? "태그 저장 실패" : err;  // err 원문 1행
    }
    frameDirty_ = true;  // DirAddManaged 수형 — 표기 계약(성공/실패 모두)
}

void ClientMusicApp::BuildTagEditPanel() {
    // 태그 편집 패널 — [폴더 관리] 패널의 동형 전환(같은 창·탭 유지 — 브리프;
    // 새 window/창 생성 금지). 개봉 = 행 [편집] 버튼(유일 경로).
    if (!editOpen_ || editPath_.empty()) {
        editOpen_ = false;  // 방어선(세션 사본 상실) — 표기 없음(사유 없는
        return;             //   개봉 거부는 행 클릭 경계의 사유 몫)
    }
    if (previewDirty_) {
        previewDirty_ = false;
        RebuildTagPreview();  // D7 패널 열림 1회(개봉 다음 프레임 — draw 전
                              //   경계 — 동프레임 AddImage 원장 파괴 금지)
    }
    EnsurePreviewTexture();
    // 제목 행 — 패널 라벨+대상 파일명(말단 성분은 바이트 스캔 — LastPathSegment
    // 원문 수형).
    ImGui::TextUnformatted("태그 편집");
    ImGui::SameLine();
    const std::string fileLabel = LastPathSegment(editPath_);
    ImGui::TextDisabled("%s", fileLabel.c_str());
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", editPath_.c_str());  // 전문 = 툴힌트(행 수형)
    // 좌우 분할 — 3 InputText(좌)+미리보기 박스(우 96px). 좌폭 = 남는 폭 −
    // 상자 −여유; 좊은 창(상자+입력이 못 들어가는 폭)은 미리보기 열 폐기(
    // 상자 0 — 입력이 전폭을 소유; degenerate 창의 방어선 — 압착 원장행).
    const float availX = ImGui::GetContentRegionAvail().x;
    const float box = kCoverPreviewBox + 12.0f < availX
                          ? static_cast<float>(kCoverPreviewBox)
                          : 0.0f;
    const float inputsW = availX - (box > 0 ? box + 12.0f : 0.0f);
    if (box > 0) {
        // 미리보기 상자 예약 — draw list 직접(행 썸네일과 같은 수형 — ImGui
        // Image의 커서 진행 회피). 좌표는 지금 cursor의 우측(세로 정렬).
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const ImU32 face = IM_COL32(
            jk::theme::current().launcherCellFace.r,
            jk::theme::current().launcherCellFace.g,
            jk::theme::current().launcherCellFace.b, 255);
        const ImU32 outline = IM_COL32(
            jk::theme::current().launcherCellOutline.r,
            jk::theme::current().launcherCellOutline.g,
            jk::theme::current().launcherCellOutline.b, 255);
        const ImVec2 q{p.x + availX - box, p.y + box};
        ImGui::GetWindowDrawList()->AddRectFilled(p, q, face, 6.0f);
        if (previewTex_ && previewImg_.w > 0 && previewImg_.h > 0) {
            const FitBox fit = FitImageBox(previewImg_.w, previewImg_.h,
                                           box - 4.0f, box - 4.0f);
            if (fit.w > 0.f && fit.h > 0.f) {
                const float tx = p.x + (availX - box) + (box - fit.w) * 0.5f;
                const float ty = p.y + (box - fit.h) * 0.5f;
                ImGui::GetWindowDrawList()->AddImage(
                    (ImTextureID)previewTex_, ImVec2(tx, ty),
                    ImVec2(tx + fit.w, ty + fit.h));
            }
        }
        ImGui::GetWindowDrawList()->AddRect(p, q, outline, 6.0f);
    }
    // 3 InputText — 프리필(OpenTrackEditor). 각 1행(폭 = inputsW) — 타이핑은
    // 입력 이벤트(게이트 원문 — 더티 조작 없음).
    ImGui::PushItemWidth(inputsW);
    ImGui::InputTextWithHint("##edtitle", "제목", editTitleBuf_,
                             sizeof(editTitleBuf_));
    ImGui::InputTextWithHint("##edartist", "아티스트", editArtistBuf_,
                             sizeof(editArtistBuf_));
    ImGui::InputTextWithHint("##edalbum", "앨범", editAlbumBuf_,
                             sizeof(editAlbumBuf_));
    ImGui::PopItemWidth();
    // 버튼 행 — [커버 선택](브라우저 토글)+[저장](모호성 해소 (c): 스캔 진행
    // 중 = 비활성+표기)+[취소]. 저장 성공의 CloseEditor가 클릭 프레임 안에서
    // 상태를 비우기에, 이후 코드(아래의 err 행)는 이번 프레임 무의미 흐름 —
    // editOpen_ 게이트로 방어한다(위의 방어선 — 다음 프레임 소멸).
    const bool canSave = !scanBusy_;
    if (ImGui::Button(editPickOpen_ ? "커버 선택 ▾" : "커버 선택 ▸")) {
        // 개봉 cwd = 편집 곡의 폴더(자연 개시 — #97 개봉 = 탭 루트의 수형).
        // 가드 2검(브라우저 개봉 원존 수형: 광 드라이브 루트·부적합 UTF-8).
        editPickOpen_ = !editPickOpen_;
        if (editPickOpen_) {
            editPickOpen_ = false;
            pickCwd_.clear();
            const std::string parent =
                std::filesystem::path(editPath_).parent_path()
                    .generic_string();
            if (parent.empty()) {
                // 개봉 시작점 부재(상대경로 성분 — 방어선 — 2m-b의 빈 성분
                // 거부 수형). 개봉 거부 = 토글 소각+사유 정직 1행.
                editSaveErr_ =
                    "[!] 커버 선택 개봉 불가 — 편집 곡의 폴더 경로가 없습니다";
                frameDirty_ = true;
            } else if (parent.size() == 2 && parent[1] == ':') {
                editSaveErr_ = "[!] 커버 선택 개봉 불가 — 광 드라이브 형식(C:) —"
                               " X:/ 형식 경로에서 파일을 준비하세요";
            } else if (jk::text::Utf8ToUtf16(parent).empty()) {
                editSaveErr_ = "[!] 커버 선택 미지원 인코딩 폴더(레거시 CP949)";
            } else {
                pickCwd_ = parent;
                editPickOpen_ = true;
            }
        } else {
            pickCwd_.clear();
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("커버아트 이미지 선택 (.jpg/.jpeg/.png) — 타이핑 없이"
                          " 폴더를 눌러 탐색");
    ImGui::SameLine();
    ImGui::BeginDisabled(!canSave);
    if (ImGui::Button("저장") && editOpen_) SaveTagEdit();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("파일에 직접 씁니다(원자적 tmp→rename) — 성공 시"
                          " 목록 재스캔");
    ImGui::SameLine();
    if (ImGui::Button("취소") && editOpen_) CloseEditor();
    if (scanBusy_ || !canSave) {
        ImGui::SameLine();
        ImGui::TextDisabled("스캔 중");  // 모호성 해소 (c) — 원문 표기
    } else if (!editSaveErr_.empty()) {
        ImGui::SameLine();
        ImGui::TextUnformatted(editSaveErr_.c_str());  // 정직 err 1행
    } else if (!editCoverErr_.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", editCoverErr_.c_str());  // 커버 부재/사유 1행
    } else if (!editCoverSel_.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("커버: %s", LastPathSegment(editCoverSel_).c_str());
    }
}

void ClientMusicApp::BuildCoverPicker() {
    // 커버 선택 브라우저 — #97 폴더 브라우저(BuildFolderBrowser)의 부품 재용:
    // cwd 표기 1행+상위 ".." 행+[닫기]+목록 child(하위 폴더 클릭 진입+browse::
    // ListSubdirs 원문)+이미지 행(Local ListCoverFiles 필터 — 클릭 = 선택,
    // 타이핑 0 — 폰 계약). 개봉 가드는 OpenTrackEditor/[커버 선택] 토글이
    // 소유(광 드라이브 루트·CP949 동형 2검).
    if (!editPickOpen_ || pickCwd_.empty())
        return;  // 방어선(개봉 가드가 비움 — 토글 소각 등)
    ImGui::TextUnformatted(pickCwd_.c_str());  // cwd 표기 1행(전문 — 브라우저
    if (ImGui::IsItemHovered())                //   원문 수형)
        ImGui::SetTooltip("%s", pickCwd_.c_str());
    const std::string parentPath = music::browse::Parent(pickCwd_);
    if (parentPath.empty()) {
        ImGui::TextDisabled("(루트 — 상위 없음)");
    } else {
        ImGui::PushID(2000000000);  // ".."·하위 폴더·이미지 행의 id 공간 분리
        if (ImGui::Selectable("..")) pickCwd_ = parentPath;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("상위 — %s", parentPath.c_str());
        ImGui::PopID();
    }
    if (ImGui::Button("닫기")) {  // 상태 소각만(브라우저 원문 수형 — 선택 유지)
        editPickOpen_ = false;
        pickCwd_.clear();
    }
    if (ImGui::BeginChild("coverpick", ImVec2(0, 128.0f),
                          ImGuiChildFlags_Borders)) {
        const music::browse::DirPage page = music::browse::ListSubdirs(pickCwd_);
        const std::vector<std::string> files = ListCoverFiles(pickCwd_);
        if (page.dirs.empty() && files.empty())
            ImGui::TextDisabled("(하위 폴더 없음 — 이미지 없음)");
        // 하위 폴더 행 — 클릭 = 진입(JoinDir 원문 소비; 행 사본 선취+dest
        // 선계산 — BuildFolderBrowser의 I1 fix r1 수형 통일).
        for (size_t k = 0; k < page.dirs.size(); ++k) {
            const std::string name = page.dirs[k];
            const std::string dest = music::browse::JoinDir(pickCwd_, name);
            ImGui::PushID(static_cast<int>(10000 + k));
            if (ImGui::Selectable(name.c_str())) pickCwd_ = dest;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", dest.c_str());
            ImGui::PopID();
        }
        // 이미지 행 — 클릭 = 커버 선택(브라우저 원문 수형의 [이 폴더 추가]
        // 성공 leg와 동형: 선택 성공 = 브라우저 종료+미리보기 재구성 예약).
        for (size_t k = 0; k < files.size(); ++k) {
            const std::string name = files[k];
            const std::string dest = music::browse::JoinDir(pickCwd_, name);
            ImGui::PushID(static_cast<int>(20000 + k));  // id 공간 분리
            if (ImGui::Selectable(name.c_str())) {
                editCoverSel_ = dest;
                editSaveErr_.clear();
                frameDirty_ = true;  // 수령 틱의 표기 변화(도착 더티 수형) —
                                     //   재구성 예약이 다음 프레임에 씌어지려면
                                     //   게이트가 필요(브라우저 종료는 입력
                                     //   이벤트지만 예약은 이번 프레임 뒤의
                                     //   내용 — 조건화 수형)
                previewDirty_ = true;  // 재구성 — 다음 프레임 draw 전 경계(
                                       //   동프레임 AddImage 원장 파괴 금지)
                editPickOpen_ = false;
                pickCwd_.clear();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", dest.c_str());
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

int ClientMusicApp::VisibleTrackCount() const {
    const std::string filter(filterBuf_);
    if (filter.empty()) return static_cast<int>(tracks_.size());
    int visible = 0;
    for (const music::Track& t : tracks_)
        if (music::MatchFilter(t, filter)) ++visible;  // D5 — 파일명+곡명+
                                                       //   아티스트(#99 T2)
    return visible;
}

void ClientMusicApp::BuildTrackTable() {
    // 표기 동형 계약: 행 데이터는 tracks_(ListAudioFiles 결과) 그대로 — 재정렬
    // 없음(최신순 유지), 필터는 **열외만**(순서 보존 — 흡수 2m-f "정렬·rel
    // 동형"의 UI측 절반). 행 더블클릭 = 재생 위임(T3 — 파일 dialogs의
    // IsItemHovered+IsMouseDoubleClicked 원문 수형): 곡명 데이터 셀 기준(행
    // 내부 아이템 1개 — 셀별 수형, 행 래퍼 Selectable 신설 금지). 대상은 행
    // 참조 t 그 자체라 필터 열외 순회와 무관하게 full이 정확하다.
    //
    // 열 재편 (#111 T3 — 스펙 §2 좌측 커버 아트 셀 좌측 [편집] 버튼): [커버]
    // [곡명(아티스트)] [앨범] [크기] [수정] [편집] [spatial] — 모호성 해소
    // (a): [편집] 표기 = "편집"(한글)·[spatial]과 같은 SmallButton 스타일.
    // #99 T2 열([곡명(아티스트)] [앨범] [크기] [수정] [spatial])의 배분 원문
    // 무변조(스트레치 2:1·fixed 84/140/74 그대로 — fixed 자리에 2열 신설:
    // 커버 40(thumb 32 박스+행 여유)·편집 56). 곡명 셀 = TrackRowLabel 원문
    // 수형(태그 부재 = 파일명 확장자 제거 — D3 폴백), 앨범 셀 = TrackAlbumLabel
    // (부재 = "-" — D3 단독 열 원존). rel 경로 데이터는 셀에서 빠지고 **툴팁**
    // 으로 남는다(경로 원문 소실 금지 — 툴팁은 Track.full **절대경로 원문** —
    // fix r1 I-3 문언 정정·구현 원존, 도착 스캔 Track.rel은 도구·status 표기
    // leg가 그대로 소비·2m-f 동형 무변조). spatial 열 원존 무변조(행별 버튼).
    const std::string filter(filterBuf_);
    if (ImGui::BeginTable("tracks", 7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
        // 커버(T3 — 스펙 §2 행 좌측 32px 썸네일): ReadCover 지연(D7 — 스캔
        // 무접촉, 프레임당 예산 2건)+아트 부재 = 회색 사각 자리표시. 셀은
        // draw list 직접(Dummy로 진행) — 컬은 갤러리 fix r2 수형(clip rect
        // 교차 행만 요청 — 컬 없는 전수 요청은 LRU 촬영 잠금의 진원).
        ImGui::TableSetupColumn("커버", ImGuiTableColumnFlags_WidthFixed,
                                40.0f);
        ImGui::TableSetupColumn("곡명", ImGuiTableColumnFlags_WidthStretch,
                                2.0f);  // "곡명 (아티스트)" 합성 — 주 가독 열
        ImGui::TableSetupColumn("앨범", ImGuiTableColumnFlags_WidthStretch,
                                1.0f);  // 보조 열 — 곡명의 절반 몫
        // (T2 런 실측: 앨범을 WidthFixed로 두면 560 창에서 곡명 열이 ~25px로
        // 압착(똑같은 압착 함정 — third_party/imgui의 fixed+stretch 배분 원문)
        // — 앨범을 stretch로 풀어 곡명 2/3·앨범 1/3 배분. fixed 자리는 원존
        // 크기/수정/spatial 3열+(T3) 커버·편집 2열.)
        ImGui::TableSetupColumn("크기", ImGuiTableColumnFlags_WidthFixed,
                                84.0f);
        ImGui::TableSetupColumn("수정", ImGuiTableColumnFlags_WidthFixed,
                                140.0f);
        // 편집(T3 — 스펙 §3 행 [편집] 버튼 열): 태그 쓰기 지원 확장자 행만
        // 발사 가능(정직 표기 — TagWritableExt; m4a/aac/wma = "-").
        ImGui::TableSetupColumn("편집", ImGuiTableColumnFlags_WidthFixed,
                                56.0f);
        // spatial(T2 — 행별 [spatial] 버튼 열, 원존 무변조): 지원 행만 발사
        // 가능. 버튼은 표기 계층(T1 순수 부품)이라 env 무관 그려진다.
        ImGui::TableSetupColumn("spatial", ImGuiTableColumnFlags_WidthFixed,
                                74.0f);
        ImGui::TableHeadersRow();
        ImDrawList* tableDl = ImGui::GetWindowDrawList();
        for (size_t r = 0; r < tracks_.size(); ++r) {
            const music::Track& t = tracks_[r];
            if (!music::MatchFilter(t, filter)) continue;  // 열외만(D5)
            ImGui::PushID(static_cast<int>(r));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            // ---- 커버 셀(T3) — 32px 박스(자리표시 face/outline은 테마
            // launcher 셀 토큰 — 갤러리 placeholder 원존 토큰 수형). 컬:
            // 행의 수직 스팬이 child·표 클립과 교차할 때만 요청(컬 산치는
            // 로컬 동형 — gallery::ThumbRowVisible 수형).
            const ImVec2 cellP = ImGui::GetCursorScreenPos();
            const float rowTop = cellP.y;
            const float rowBot = rowTop + 34.0f;  // 32 박스+행 간격 여유
            CoverView view;
            if (rowTop < tableDl->GetClipRectMax().y &&
                rowBot > tableDl->GetClipRectMin().y)
                view = RowCover(t);
            const auto& thm = jk::theme::current();
            const ImVec2 cellQ{ cellP.x + 32.0f, cellP.y + 32.0f };
            tableDl->AddRectFilled(cellP, cellQ,
                                   IM_COL32(thm.launcherCellFace.r,
                                            thm.launcherCellFace.g,
                                            thm.launcherCellFace.b, 255),
                                   4.0f);
            if (view.tex && view.w > 0 && view.h > 0) {
                const FitBox tf = FitImageBox(view.w, view.h, 32.0f, 32.0f);
                if (tf.w > 0.f && tf.h > 0.f) {
                    const float tx = cellP.x + (32.0f - tf.w) * 0.5f;
                    const float ty = cellP.y + (32.0f - tf.h) * 0.5f;
                    tableDl->AddImage((ImTextureID)view.tex, ImVec2(tx, ty),
                                      ImVec2(tx + tf.w, ty + tf.h));
                }
            }
            ImGui::Dummy(ImVec2(32.0f, 32.0f));  // 셀 진행 — 박스 높이 소유
            // 곡명(아티스트) 합성 셀 — 폴백 라벨 원문(D3). 툴팁 = Track.full
            // 절대경로 원문(fix r1 I-3 문언 — 경로 원문 보존·구현 원존).
            ImGui::TableSetColumnIndex(1);
            const std::string rowLabel = music::TrackRowLabel(t);
            ImGui::TextUnformatted(rowLabel.c_str());
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", t.full.c_str());
            if (ImGui::IsItemHovered() &&
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                PlaybackDelegate(t);  // 더블클릭 → vplayer 위임(T3 계약 원존)
            char sizeBuf[32];
            HumanSize(t.size, sizeBuf, sizeof(sizeBuf));
            ImGui::TableSetColumnIndex(2);
            const std::string albumLabel = music::TrackAlbumLabel(t);
            ImGui::TextUnformatted(albumLabel.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(sizeBuf);
            char mtBuf[32];
            MtimeLabel(t.mtime, mtBuf, sizeof(mtBuf));
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(mtBuf);
            // [편집] 버튼(T3 — 스펙 §3): 쓰기 지원 확장자 행만(정직 표기 —
            // m4a/aac/wma = "-"+툴팁). 클릭 = 개봉 1발 — 더티 조작 없음.
            ImGui::TableSetColumnIndex(5);
            if (TagWritableExt(t.full)) {
                if (ImGui::SmallButton("편집")) OpenTrackEditor(t);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("태그 편집 — 제목·아티스트·앨범·"
                                      "커버아트");
            } else {
                ImGui::TextDisabled("-");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("태그 쓰기 미지원 포맷(v1 — mp3·flac·"
                                      "ogg·wav)");
            }
            ImGui::TableSetColumnIndex(6);
            // [spatial] 버튼(T2 — LegSupport 표 소비): 지원 행만 발사, 미지원
            // 행 회색+툴팁(D4 정직 계약). 버튼 표기 자체는 env 무근(표기 계층
            // T1 순수 부품) — leg 불가 빌드의 발사는 SpatialStart의
            // DeviceFailed 종착(kDelegationHint 라벨)으로 끝난다(정직 계약).
            const bool spatialable =
                music::leg::LegSupport::OfExt(t.full) ==
                music::leg::LegSupport::State::Supported;
            if (spatialable) {
                if (ImGui::SmallButton("spatial")) SpatialStart(t);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s",
                                      "spatial leg로 재생(POSITIONAL HRTF)");
            } else {
                ImGui::TextDisabled("-");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("미지원 포맷 — vplayer 위임으로 재생");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

// P3 theme hot-swap (docs/52): the palette was snapshotted into ImGuiStyle
// at OnInit - re-apply it after a preset swap.
void ClientMusicApp::OnThemeChanged() { jk::theme::ApplyImGuiTheme(); }

} // namespace jk