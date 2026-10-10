// Music hub client (spec 2026-10-09-music-library-design, T2) — the gallery
// twin: dark root window, 16 ms timer as a delivery channel, ImGui over the
// surface, Korean font via the desktop resolver, AppContentTopOffset band
// math, theme hot-swap re-apply. T2 adds the async scan worker (single
// parked std::thread, one-slot latest-wins request mailbox, destruction
// join) and the track table (rel / size / mtime) + the name filter. The
// displayed vector is the scan worker's ListAudioFiles output verbatim —
// sorted (mtime desc, rel asc tie) and consumed without re-sort or derivation
// (selftest 2m-f asserts that isomorphism). Arrival of a scan result is the
// ONLY frameDirty_ source (#89 idle contract, docs/88). T3 adds the playback
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
#include <apps/ClientMusicApp.h>

#include <imgui_impl_jkwindow.h>
#include <JKTextAtlas.h>
#include <JKWindow.h>
#include <fs/JKFs.h>  // FileTimeToSys — file_clock 수형 → 시간 표기(플랫폼 epoch 몫)
#include "theme/JKThemeImGui.h"
#include <SDL.h>

#include <chrono>
#include <cstdio>
#include <ctime>
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
} // namespace

ClientMusicApp::ClientMusicApp() = default;  // 원밖 — leg unique_ptr의 완전형
                                             //   가시점(cpp 가드 include 뒤)

ClientMusicApp::~ClientMusicApp() {
    // 스캔 워커 join 수형(갤러리 썸네일 원문 계약 — 파괴 join). quit 경련+
    // cv 기상으로 wait를 깨고, 진행 중 스캔이 끝나기를 기다린다(ListAudioFiles
    //는 유한 — 2m-e 순환 가드 계약). 잔여 결과 박스는 소멸자 원의 소각.
    scanQuit_.store(true);
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

    ResolveDirs();
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
    // 스캔 결과 수취 — 워커의 도착 표식만 읽는다(뮤텍스 접촉은 수취 시 1회 —
    // 무도착 무비용). **도착+답신 수취 = 유일 더티**(#89 계약): 수취한 결과가
    // 현재 세대면 표기 갱신+frameDirty_=true, 세대 불일치(재청구가 이겼다)
    // 면 폐기만.
    if (!scanDone_.load(std::memory_order_acquire)) return;
    bool adopted = false;
    {
        std::lock_guard<std::mutex> lk(scanM_);
        if (scanOutGen_ == scanSeq_) {
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
    ImGui_ImplJKWindow_NewFrame(1.0f / 60.0f, w, h);
    ImGui::NewFrame();
    BuildUi(w, h);
    ImGui::Render();
    ImGui_ImplJKWindow_RenderDrawData(ImGui::GetDrawData(), renderer);
    // 자기유지 더티 소멸 없음(#89 T2 — gallery 수형의 조건 잔존도 없다): 이
    // 앱은 프레임 내 비동기 도착이 없고(워커 도착은 scanDone_→OnIdle) 남는
    // 요청(placeholder 재요청)이 없다 — 다음 프레임은 도착·입력·테마만 유발.
}

std::string ClientMusicApp::SettingsText() const {
    // exe-dir state/settings.json 직독(ResolveDirs·RefreshUserDirs 공용 leg —
    // 원문 텍스트를 순수 스캐너/리졸버에 건넨다 — gallery ResolveDirs 수형).
    // 부재/읽기 실패 = 빈 텍스트 → 스캐너/리졸버가 기본 1건(fail-safe)으로
    // 떨어진다.
    const std::string kvPath =
        (std::filesystem::path(ExeDir()) / "state" / "settings.json").string();
    std::string text;
    std::FILE* f = std::fopen(kvPath.c_str(), "rb");
    if (f) {
        char chunk[2048];
        size_t n;
        while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0)
            text.append(chunk, n);
        std::fclose(f);
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
        // 스캔 leg — throw 보호선(std::terminate 방어): narrow fs::path는
        // UTF-8 기수(CP949 바이트에 던진다 — 2o-f 실측)라 원칙적으로 진입은
        // RequestScan 가드가 막지만, 그 밖의 filesystem_error류(파일명 변질
        // 등 플랫폼 엣지)에서도 워커 스레드 사망 전체 앱 terminate가 되지
        // 않게 0건 표현(스캔 계약 "독립 실패 = 빈 목록")으로 흡수한다.
        std::vector<music::Track> out;
        try {
            out = music::ListAudioFiles(root);
        } catch (...) {
            out.clear();
        }
        {
            std::lock_guard<std::mutex> lk(scanM_);
            scanOut_ = std::move(out);
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
        RefreshUserDirs();
        ResolveDirs();          // dirIndex_ 클램프 포함(삭제 탭이 활성이면 0)
        RequestScan();
    } else {
        status_ = "[!] 폴더 제거 실패 — " + r.err;
    }
    frameDirty_ = true;
    return r;
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
    if (!legPlayer_->open(t.full, err)) {
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
        // 빈 필터 = 전부 참(2m-d 계약)이라 그대로 소비.
        ImGui::SetNextItemWidth(170.0f);
        ImGui::InputTextWithHint("##filter", "이름 필터", filterBuf_,
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
        }

        ImGui::Separator();
        // 2분할 — 표(남는 높이)+spatial leg 패널(하단 고정 200px). 패널은
        // env 무관하게 늘 그려진다(버튼=표기 계층 T1 순수 부품 — leg 불가
        // 빌드의 발사는 DeviceFailed 고장 종착으로 끝나며 안내 원문이 그
        // 차이를 말해준다 — 정직 계약, header 원문).
        if (ImGui::BeginChild("tracks", ImVec2(0, -200.0f),
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
        if (ImGui::BeginChild("spatial", ImVec2(0, 0),
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

int ClientMusicApp::VisibleTrackCount() const {
    const std::string filter(filterBuf_);
    if (filter.empty()) return static_cast<int>(tracks_.size());
    int visible = 0;
    for (const music::Track& t : tracks_)
        if (music::MatchFilter(t.name, filter)) ++visible;
    return visible;
}

void ClientMusicApp::BuildTrackTable() {
    // 표기 동형 계약: 행 데이터는 tracks_(ListAudioFiles 결과) 그대로 — 재정렬
    // 없음(최신순 유지), rel 열은 Track.rel 원문, 필터는 **열외만**(순서 보존 —
    // 흡수 2m-f "정렬·rel 동형"의 UI측 절반). 행 더블클릭 = 재생 위임(T3 —
    // 파일 dialogs의 IsItemHovered+IsMouseDoubleClicked 원문 수형): 경로 셀
    // 기준(행 내부 아이템 1개 — 셀별 수형, 행 래퍼 Selectable 신설 금지).
    // 대상은 행 참조 t 그 자체라 필터 열외 순회와 무관하게 full이 정확하다.
    const std::string filter(filterBuf_);
    if (ImGui::BeginTable("tracks", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("경로");  // rel(상대경로) — Stretch
        ImGui::TableSetupColumn("크기", ImGuiTableColumnFlags_WidthFixed,
                                84.0f);
        ImGui::TableSetupColumn("수정", ImGuiTableColumnFlags_WidthFixed,
                                140.0f);
        // spatial(T2 — 행별 [spatial] 버튼 열): 지원 행만 발사 가능. 버튼은
        // 표기 계층(T1 순수 부품)이라 env 무관 그려진다.
        ImGui::TableSetupColumn("spatial", ImGuiTableColumnFlags_WidthFixed,
                                74.0f);
        ImGui::TableHeadersRow();
        for (size_t r = 0; r < tracks_.size(); ++r) {
            const music::Track& t = tracks_[r];
            if (!music::MatchFilter(t.name, filter)) continue;  // 열외만
            ImGui::PushID(static_cast<int>(r));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(t.rel.c_str());
            if (ImGui::IsItemHovered() &&
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                PlaybackDelegate(t);  // 더블클릭 → vplayer 위임(T3 계약)
            char sizeBuf[32];
            HumanSize(t.size, sizeBuf, sizeof(sizeBuf));
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(sizeBuf);
            char mtBuf[32];
            MtimeLabel(t.mtime, mtBuf, sizeof(mtBuf));
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(mtBuf);
            ImGui::TableSetColumnIndex(3);
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