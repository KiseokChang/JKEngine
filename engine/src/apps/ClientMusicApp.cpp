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
// extra dirty sources, a retry resend never dirties.
#include <apps/ClientMusicApp.h>

#include <imgui_impl_jkwindow.h>
#include <JKTextAtlas.h>
#include <JKWindow.h>
#include <agent/JKAgentJson.h>  // open 답신 error 판독(서버 app_tool 릴레이 표기)
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
// 부정 mtime(1970 이전·플랫폼 epoch 차액)은 셀 "-"(표기 부재 — 스탬프 부재와
// 같은 열외 수형, WorkshopStore의 0 클램프 계열).
// 재청구 폴백 상수(T3 — 콜드 부팅 경기 흡수): launch_app의 스폰은 비동기라
// vplayer의 도구 등록(SendAgentToolRegister — 프로세스 기동+연결 뒤)이 open
// 릴레이보다 뒤져 unknown_app_tool이 떨어진다. 서버 부품 신설 금지(폴백
// 원존 원칙)라 클라가 기존 채널로 재청구한다 — 20 × 250ms = 5s 예산(기동
// 실측 대비 큰 여유). 시간 기반 pacing이라 무더기 재청구가 없다.
constexpr int kOpenRetryMax = 20;
constexpr std::chrono::milliseconds kOpenRetryDelayMs{250};

void MtimeLabel(long long mtime, char* buf, size_t bufBytes) {
    std::snprintf(buf, bufBytes, "-");
    if (mtime <= 0) return;
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
} // namespace

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

    ResolveDirs();
    RequestScan();
}

void ClientMusicApp::OnClose() {
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

void ClientMusicApp::ResolveDirs() {
    // exe-dir settings.json 직독 → MusicDirList에 **원문 텍스트**로 넘긴다:
    // 리졸버는 순수 함수(디스크 I/O 없음)라 셀프테스트 쌍둥이가 같은 경로를
    // 직단정한다(gallery ResolveDirs 수형 — settings.json 대상만 music으로).
    std::string text;
    {
        const std::string kvPath =
            (std::filesystem::path(ExeDir()) / "state" / "settings.json")
                .string();
        std::FILE* f = std::fopen(kvPath.c_str(), "rb");
        if (f) {
            char chunk[2048];
            size_t n;
            while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0)
                text.append(chunk, n);
            std::fclose(f);
        }
        // 부재/읽기 실패 = 빈 텍스트 → 리졸버가 기본 1건(fail-safe)으로 떨어진다.
    }
    dirs_ = music::MusicDirList(ExeDir(), text);
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
        std::vector<music::Track> out = music::ListAudioFiles(root);
        {
            std::lock_guard<std::mutex> lk(scanM_);
            scanOut_ = std::move(out);
            scanOutGen_ = gen;
        }
        scanDone_.store(true, std::memory_order_release);
    }
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
        if (reply.queryId == launchId_) {
            launchId_ = 0;
            // ① 답신 — ok면 open이 이미 나가 있고(재청구 pacing 계속), 거부면
            // 런치 검증 실패(unknown_app — jkapp_vplayer 모듈 부재) 표기+
            // 폴백 종료(open 재청구가 런치 성립을 전제한다 — 원문 쌍 계약,
            // fix r1 M-2: 사후 open 답신 겹침 가드도 여기서 세운다).
            if (!reply.ok) {
                status_ = "[!] vplayer 실행 거부";
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
            jk::agent::AgentJson body(reply.json);
            std::string err;
            const bool hasErr = body.ok() && body.GetStr("error", err);
            // ② 답신 판정 (fix r1 I-2 — 서버 HandleToolResult 조립 정합):
            // ok=false의 error 객체는 앱-수준 실패다 — 성공 분류는 **reply.ok
            // 원문만**이 소유한다(구판의 !body.ok()/error 부재 성공 폴은
            // 삭제). ok=false인데판독 실패(파손 표기)도 실패 몫.
            if (reply.ok) {
                status_ = "vplayer 재생 요청됨";   // 오픈은 비동기 — 진행은
                                                  //   vplayer 표면(get_status)
                openPath_.clear();
                openRetries_ = 0;
                frameDirty_ = true;
            } else if (hasErr && err == "unknown_app_tool") {
                // 콜드 부팅 경기 — 등록 전 릴레이(재청구 유일 대상 — 도구
                // 등록 경기 흡수 계약). 소진 크레딧(openRetries_)은 **답신
                // 수취에서만 소모**하고(발사인 OnIdle pace는 감법을 만들지
                // 않는다), 수취에서 0 도달 = 즉시 소진 전이(fix r2 I-1
                // 재수형 — 감법이 OnIdle 분기에 의존하면 마지막 답신 뒤
                // 영구 실패·표기 dead code가 된다): 소진 표기 1행+이번 틱
                // 더티 1회(사용자 안내 = 상태 변칙 몫)를 **수취 시점에**.
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
            } else if (hasErr && err == "ambiguous") {
                // 복수 후보(자기교정 원문 재용) — 추측 없이 표기로만.
                status_ = "[!] vplayer 창이 복수 — 하나 닫고 다시 시도";
                openPath_.clear();
                openRetries_ = 0;
                frameDirty_ = true;
            } else {
                // ok=false+error(도구 실패 — 폴백 흡수 대상 아님)·판독 실패 —
                // 재청구 대상 아님, 즉시 종착(리트라이 무의미 — fix r1 I-2).
                status_ = hasErr ? "[!] 재생 위임 실패 — " + err
                                 : "[!] 재생 위임 실패";
                openPath_.clear();
                openRetries_ = 0;
                frameDirty_ = true;
            }
        }
    }
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
            std::string tabLabel =
                std::filesystem::path(dirs_[i]).filename().string();
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

        ImGui::Separator();
        if (ImGui::BeginChild("tracks", ImVec2(0, 0),
                              ImGuiChildFlags_Borders)) {
            if (scanBusy_) {
                // 진행 표시 — 비동기 스캔의 도착 대기(비운 목록 위 1행).
                // 도착은 OnIdle 수취(도착 더티)로 다음 프레임을 유발한다.
                ImGui::TextUnformatted("스캔 중...");
            } else if (tracks_.empty()) {
                // 빈 상태(브리프 4) — 스캔 0건 계약 문구(독립 실패·빈 루트
                // 구분 없음 — ListAudioFiles의 ok 플래그 불요 계약 승계).
                ImGui::TextUnformatted(
                    "트랙 없음 — settings music.dirs에 폴더를 추가하세요");
            } else {
                BuildTrackTable();
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();
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
    if (ImGui::BeginTable("tracks", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("경로");  // rel(상대경로) — Stretch
        ImGui::TableSetupColumn("크기", ImGuiTableColumnFlags_WidthFixed,
                                84.0f);
        ImGui::TableSetupColumn("수정", ImGuiTableColumnFlags_WidthFixed,
                                140.0f);
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
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

// P3 theme hot-swap (docs/52): the palette was snapshotted into ImGuiStyle
// at OnInit - re-apply it after a preset swap.
void ClientMusicApp::OnThemeChanged() { jk::theme::ApplyImGuiTheme(); }

} // namespace jk