// Desktop shell / taskbar client (docs/28). Registers the shell role right
// after connecting, renders one button per window from the server's
// WindowList snapshots, and sends WindowActivate on click.
#include <apps/ClientTaskbarApp.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif

#include "theme/JKTheme.h"

#include <JKHangulManager.h>
#include <JKHangulUtil.h>
#include <JKResourceCache.h>
#include <JKSDLRenderBackend.h>
#include <JKTextAtlas.h>
#include <net/JKNet.h>
#include <cstdio>
#include <algorithm>
#include <map>
#include <cmath>
#include <thread>

namespace jk {

namespace {
// Bar/button geometry (logical points).
constexpr int kButtonHeight = 44;   // 셸 2.0 S2 — 바 56px(SendShellRegister 갱신).
constexpr int kTrayW = 90;          // [트레이 시계] 셀(우측 고정).
constexpr int kButtonGap = 4;
constexpr int kBarMargin = 4;
constexpr int kButtonMaxWidth = 180;   // Windows-style cap
constexpr int kButtonMinWidth = 40;    // overflow floor (docs/28): 축소 하한
constexpr int kChipSize = 10;
constexpr int kChipPad = 6;

// [트레이·라벨 텍스처 미니 실장 — 2026-11-09 실측 원존: 자식 TextOutX가
// 전부 불발(빈 셀 원문) → 셸의 TextTexture 정확한 수형을 자식에 미니 실장.
// 벡터 폰트(ResolveDesktopFontPath — JKENGINE_FONT_DIR compile leg 원존)
// + 비트맵 폴백 — 문구별 1회 렌더 캐시(캐시는 프로세스 수명 소유 —
// 승인 배너 선례). 투명 배경.
struct MiniRenderKit {
    SDL_Renderer* r = nullptr;   // GetSurfaceRenderer() — OnInit 봉입.
    std::unique_ptr<jk::HangulManager> font;
    std::unique_ptr<jk::JKSDLRenderBackend> backend;
    std::unique_ptr<jk::JKTextAtlas> atlas;
    std::unique_ptr<jk::JKResourceCache> cache;
};
MiniRenderKit& MiniKit() {
    static MiniRenderKit k;
    return k;
}

// [수형 2026-11-11 — KSSM 부분집합(2350음절) 한계 우회] 아틀라스 = 유니코드
// 직행 — 자식 dc에 아틀라스를 장착해 DrawGlyph(cp) 로 KSSM 밖 한글(홈·최·정 등)
// 까지 렌더. 실패(폰트 부재) = 아틀라스 미장착 상태로 통과(평태 비트맵 경로 보존).
bool EnsureMiniAtlas(SDL_Renderer* r) {
    MiniRenderKit& k = MiniKit();
    if (k.atlas) return k.atlas->IsLoaded();
    if (!r) return false;
    if (!k.font) k.font = std::make_unique<jk::HangulManager>();
    if (!k.backend) k.backend = std::make_unique<jk::JKSDLRenderBackend>(r);
    if (!k.cache) k.cache = std::make_unique<jk::JKResourceCache>(k.backend.get());
    k.atlas = std::make_unique<jk::JKTextAtlas>();
    const jk::text::CellMetrics m = jk::text::GetCellMetrics();
    const std::string fp = jk::text::ResolveDesktopFontPath();
    if (!fp.empty()) {
        (void)k.atlas->Init(fp, m.engW, m.cellH, m.hanW);
        const std::string fb = jk::text::ResolveDesktopFallbackPath();
        if (k.atlas->IsLoaded() && !fb.empty())
            (void)k.atlas->InitFallback(fb);
    }
    return k.atlas->IsLoaded();
}

// UTF-8 문자열 → 아틀라스 직행(압축 평활자)·중앙 정렬.
void DrawUnicodeCentered(JKDC& dc, const JKRect& rect, const std::string& utf8) {
    const jk::text::CellMetrics m = jk::text::GetCellMetrics();
    // 폭 계산(추정 — ASCII engW·그 외 hanW)
    int w = 0;
    for (size_t i = 0; i < utf8.size(); ) {
        const unsigned char c = static_cast<unsigned char>(utf8[i]);
        int len = (c < 0x80) ? 1 : (c >= 0xF0 ? 4 : (c >= 0xE0 ? 3 : 2));
        w += (len == 1) ? m.engW : m.hanW;
        i += static_cast<size_t>(len);
    }
    int x = rect.x + (rect.w - w) / 2;
    const int y = rect.y + (rect.h - static_cast<int>(m.cellH)) / 2;
    for (size_t i = 0; i < utf8.size(); ) {
        const unsigned char c = static_cast<unsigned char>(utf8[i]);
        uint32_t cp = c;
        int len = 1;
        if (c >= 0xF0) { cp = c & 0x07; len = 4; }
        else if (c >= 0xE0) { cp = c & 0x0F; len = 3; }
        else if (c >= 0xC0) { cp = c & 0x1F; len = 2; }
        for (int k = 1; k < len; ++k) cp = (cp << 6) | (utf8[i + k] & 0x3F);
        (void)dc.DrawUnicodeGlyph(JKPoint{ x, y }, cp, (len == 1) ? m.engW : m.hanW);
        x += (len == 1) ? m.engW : m.hanW;
        i += static_cast<size_t>(len);
    }
}
SDL_Texture* MiniTextTexture(SDL_Renderer* r, const std::string& utf8,
                             int* w, int* h) {
    static std::map<std::string, std::pair<SDL_Texture*, SDL_Point>> texs;
    r = r ? r : MiniKit().r;
    *w = 0; *h = 0;
    auto it = texs.find(utf8);
    if (it != texs.end()) {
        *w = it->second.second.x;
        *h = it->second.second.y;
        return it->second.first;
    }
    MiniRenderKit& k = MiniKit();
    k.r = r;     // 재부팅(닫았다 다시 열기) 대비 — r의 최신 봉입.
    if (!r) return nullptr;
    if (!k.font) k.font = std::make_unique<jk::HangulManager>();
    if (!k.backend) k.backend =
        std::make_unique<jk::JKSDLRenderBackend>(r);
    if (!k.cache) k.cache = std::make_unique<jk::JKResourceCache>(
        k.backend.get());
    if (!k.atlas) {
        k.atlas = std::make_unique<jk::JKTextAtlas>();
        const jk::text::CellMetrics m = jk::text::GetCellMetrics();
        const std::string fp = jk::text::ResolveDesktopFontPath();
        const bool inited = k.atlas->Init(fp, m.engW, m.cellH, m.hanW);
        std::fprintf(stderr, "[taskbar] mini fp init=%d", inited ? 1 : 0);
        const std::string fb = jk::text::ResolveDesktopFallbackPath();
        if (k.atlas->IsLoaded() && !fb.empty())
            (void)k.atlas->InitFallback(fb);
        std::fprintf(stderr, "[taskbar] mini tex create done");
    }
    std::string kssm = jk::Utf8ToKssm(utf8.c_str());
    if (kssm.empty()) kssm = utf8;
    const JKPoint m = jk::JKDC::MeasureText(kssm.c_str());
    constexpr int pad = 3;
    const int texW = m.x + pad * 2;
    const int texH = (m.y > 0 ? m.y : 16) + pad * 2;
    if (!SDL_RenderTargetSupported(r)) return nullptr;
    SDL_Texture* tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888,
                                         SDL_TEXTUREACCESS_TARGET,
                                         texW, texH);
    if (!tex) {
        std::fprintf(stderr, "[taskbar] mini texture create failed");
        return nullptr;
    }
    SDL_Texture* prev = SDL_GetRenderTarget(r);
    SDL_SetRenderTarget(r, tex);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
    SDL_RenderClear(r);
    jk::JKSDLRenderBackend backend(r);
    jk::JKDC dc(&backend);
    if (k.font) dc.SetHangulManager(k.font.get());
    if (k.atlas && k.atlas->IsLoaded()) {
        dc.SetTextAtlas(k.atlas.get(), k.cache.get());
    }
    dc.SetTextColor(235, 235, 240);
    dc.TextOut(JKPoint{pad, pad}, kssm.c_str());
    // [봉합 — 2026-11-09 실측] GetImage가 업로드 플러시 이전에 null을 주는
    // 셸 TextTexture의 재시도 패턴 승계(docs/63 §8) — 플러시 누락 = 빈
    // 텍스처(라벨 미표기 원존).
    if (k.cache) k.cache->FlushUploads(k.backend.get());
    SDL_SetRenderTarget(r, prev);
    texs[utf8] = {tex, SDL_Point{texW, texH}};
    *w = texW; *h = texH;
    return tex;
}

// [S3 — 시계 셀 텍스트] HH:MM 24시(로컬).
std::string CurrentClockText() {
    time_t now = time(nullptr);
    tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &now);
#else
    tm* pp = std::localtime(&now);
    if (pp) lt = *pp;
#endif
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", lt.tm_hour, lt.tm_min);
    return buf;
}

// FNV-1a title hash -> stable per-window chip color.
void TitleChipColor(const std::string& title, uint8_t& r, uint8_t& g, uint8_t& b) {
    uint32_t h = 2166136261u;
    for (char c : title) {
        h ^= static_cast<uint8_t>(c);
        h *= 16777619u;
    }
    r = static_cast<uint8_t>(96 + ((h >> 16) & 63));
    g = static_cast<uint8_t>(96 + ((h >> 8) & 63));
    b = static_cast<uint8_t>(96 + (h & 63));
}

// 네비 셀 지시 채널(스펙 §3 — 수단 통일 계약): :8091/talk POST 단발 —
// 라우터 exact 행("브라우저 뒤로"·"바탕화면으로"·"창 목록")이 소비한다.
// 워커 스레드 단발(detach) — UI 스레드 블록 금지. 실패 = 조용한 포기
// (콘솔 행 없음 — 컨트롤 앱/구두 폴백이 남는다).
void PostTalkAsync(const std::string& text) {
    std::thread([text] {
        if (!jk::net::Startup()) return;
        jk::net::Socket s = jk::net::ConnectTcp("127.0.0.1", 8091);
        if (s == jk::net::kInvalidSocket) return;
        jk::net::SetTimeouts(s, 5000);
        const std::string body = "{\"text\":\"" + text + "\"}";
        std::string req =
            "POST /talk HTTP/1.1\r\n"
            "Host: 127.0.0.1:8091\r\n"
            "Content-Type: application/json\r\n"
            "Connection: close\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
        if (jk::net::Send(s, req.data(), static_cast<int>(req.size())) <= 0) {
            jk::net::Close(s);
            return;
        }
        char buf[256];
        (void)jk::net::Recv(s, buf, sizeof(buf));  // 1회 회신 폐기(단발 소화)
        jk::net::Close(s);
    }).detach();
}
} // namespace

ClientTaskbarApp::~ClientTaskbarApp() = default;

void ClientTaskbarApp::OnInit() {
    auto main = std::make_unique<TaskbarWindow>("Taskbar");
    main->SetWindowRect(JKRect{ 0, 0, 1280, 40 });
    main->SetAttrFlags(WA_CHROMELESS);

    // 네비 셀 3종(스펙 §4 — DeX 하단바 관례 순서: 뒤·홈·최근 — 좌측 고정).
    // 창 버튼 pool과 별개 — Relayout에서 스트립 폭만큼 창 버튼을 미끼친다.
    struct NavCell { const char* label; const char* say; TaskbarButton** dst; };
    const NavCell nav[] = {
        { "뒤",  "브라우저 뒤로",   &navBack_ },
        { "홈",  "바탕화면으로",    &navHome_ },
        { "최근", "창 목록",        &navRecents_ },
    };
    for (const NavCell& n : nav) {
        auto btn = std::make_unique<TaskbarButton>();
        btn->SetNavMode(true);
        btn->SetText(n.label);
        TaskbarButton* raw = btn.get();
        const char* say = n.say;
        raw->SetOnClickWindow([say](uint32_t, bool) {
            PostTalkAsync(say);
        });
        main->AddControl(std::move(btn));
        *n.dst = raw;
    }
    {
        // [S5 정리 셀] 창 배치 저장 서버 도구 위임(save_layout 라우터 재용).
        auto btn = std::make_unique<TaskbarButton>();
        btn->SetNavMode(true);
        btn->SetText("정리");
        cleanupCell_ = btn.get();
        cleanupCell_->SetOnClickWindow([](uint32_t, bool) {
            PostTalkAsync("창 정리해줘");
        });
        main->AddControl(std::move(btn));
    }
    {
        // [트레이 시계 셀 — 사용자 결정 원존] HH:MM — Timer 틱마다 갱신.
        auto btn = std::make_unique<TaskbarButton>();
        btn->SetNavMode(true);
        btn->SetText(CurrentClockText());
        clockCell_ = btn.get();
        main->AddControl(std::move(btn));
    }
    SetTimerInterval(10000);  // [트레이] 시계 갱신(1분 해상도·10초 틱).
    MiniKit().r = GetSurfaceRenderer();  // 자식 텍스처 대상(표면 렌더러).
    // [2026-11-09 정산] 중복 정리 셀·폐기 주석 소각(파이썬 편집 적중 2회 원존
    // — 첫 적용은 커밋 미포함 상태로 파일에 남았던 실측).

    // Button pool (WindowListPayload caps at 32 windows).
    for (size_t i = 0; i < 32; ++i) {
        auto btn = std::make_unique<TaskbarButton>();
        btn->SetOnClickWindow([this](uint32_t surfaceId, bool wasActive) {
            jk::client::JKClientSurface* surface = Surface();
            if (!surface) return;
            if (wasActive) {
                surface->SendWindowMinimizeToggle(surfaceId);
            } else {
                surface->SendWindowActivate(surfaceId);
            }
        });
        btn->Hide();
        TaskbarButton* raw = btn.get();
        main->AddControl(std::move(btn));
        buttons_.push_back(raw);
    }

    SetMainWindow(std::move(main));

    // Claim the shell role: bottom edge, 40pt requested thickness. The
    // server acks (accepted/denied) and replies with an initial WindowList
    // snapshot; the shell layer goes topmost from here on.
    jk::client::JKClientSurface* surface = Surface();
    if (!surface || !surface->SendShellRegister(0 /*bottom*/, 56)) {
        std::fprintf(stderr, "[taskbar] SendShellRegister failed\n");
        std::fflush(stderr);
    }
}

void ClientTaskbarApp::OnClose() {
}

bool ClientTaskbarApp::PreProcessMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::WindowListChanged) {
        RefreshWindowList();
        return true;
    }
    if (ev.type == JKEventType::Timer) {
        // [트레이] 시계 텍스트 갱신(10초 틱 — 1분 해상도). UI 스레드 전처리.
        if (clockCell_ && clockCell_->IsVisible()) {
            clockCell_->SetText(CurrentClockText());
            JKWindow* win = GetMainWindow();
            if (win) win->Invalidate();
        }
        return true;
    }

    // Docking resize: the server reshapes the bar to the desktop width —
    // re-flow the buttons into the new client rect.
    if (ev.type == JKEventType::SizeChanged) {
        Relayout();
        return true;
    }
    return true;
}

void ClientTaskbarApp::RefreshWindowList() {
    jk::client::JKClientSurface* surface = Surface();
    if (!surface || !surface->GetWindowList(windows_)) {
        return;
    }

    const size_t count = std::min(windows_.size(), buttons_.size());
    for (size_t i = 0; i < buttons_.size(); ++i) {
        TaskbarButton* btn = buttons_[i];
        if (i < count) {
            const auto& w = windows_[i];
            btn->Bind(w.surfaceId, w.title,
                      (w.flags & 0x1u) != 0,   // ipc::kShellWindowActive
                      (w.flags & 0x2u) != 0);  // ipc::kShellWindowMinimized
            btn->Show();
        } else {
            btn->Hide();
        }
    }
    Relayout();
}

// Overflow rule (docs/28, 최소 너비 보장형 동적 축소): split the available
// width across buttons, cap at kButtonMaxWidth, never below kButtonMinWidth.
// Titles wider than the button are simply clipped by TextOutX.
// 창 개수 무관 항상 보이는 네비 스트립 폭(뒤·홈·최근 — DeX 하단바 관례).
void ClientTaskbarApp::Relayout() {
    JKWindow* win = GetMainWindow();
    if (!win) return;
    const JKRect client = win->GetClientRect();

    constexpr int kNavW = 44;
    constexpr int kNavCount = 3;
    const int navStrip = kNavW * kNavCount + kButtonGap * (kNavCount - 1);

    // 네비 셀 — 좌측 뒤·홈·최근, 그 옆 정리(S5), 우측 끝 시계(S3).
    int x = kBarMargin;
    const int y = std::max(0, (client.h - kButtonHeight) / 2);
    TaskbarButton* navs[] = { navBack_, navHome_, navRecents_ };
    for (TaskbarButton* nav : navs) {
        if (nav) {
            nav->SetRect(JKRect{ x, y, kNavW, kButtonHeight });
            nav->Show();
        }
        x += kNavW + kButtonGap;
    }
    if (cleanupCell_) {
        cleanupCell_->SetRect(JKRect{ x, y, kNavW, kButtonHeight });
        cleanupCell_->Show();
        x += kNavW + kButtonGap;
    }
    // [트레이] 우측 고정 — 시계(텍스트 중앙 — 나비 글리프 계열과 경계 분리).
    const int trayLeft = client.w - kBarMargin - kTrayW;
    if (clockCell_) {
        clockCell_->SetRect(JKRect{ trayLeft, y, kTrayW, kButtonHeight });
        clockCell_->Show();
    }
    // 창 버튼 — 시계를 피해 배치.
    // [P0 수형 — docs/106 §1-1 검증 확정] 1판 avail 산식이 정리 셀(44px)과
    // 시작 x를 누락해 겹침 = 52-r px(창 6개@1280 = 시계 앞 36px 가림 실측).
    // 계약 = 실제 커서 x에서 트레이 좌단까지의 폭만 소비 + 배치 클램프.

    size_t count = 0;
    for (TaskbarButton* btn : buttons_) {
        if (btn->IsVisible()) ++count;
    }
    if (count == 0) { win->Invalidate(); return; }

    const int avail = trayLeft - kButtonGap - x
                      - kButtonGap * static_cast<int>(count - 1);
    int btnW = avail / static_cast<int>(count);
    btnW = std::max(kButtonMinWidth, std::min(kButtonMaxWidth, btnW));

    for (TaskbarButton* btn : buttons_) {
        if (!btn->IsVisible()) continue;
        if (x + btnW > trayLeft - kButtonGap) {   // 클램프 — 트레이 침범 금지
            btn->Hide();                          // 자리 없음 = 정직 숨김
            continue;
        }
        btn->SetRect(JKRect{ x, y, btnW, kButtonHeight });
        x += btnW + kButtonGap;
    }
    win->Invalidate();
}

// --- TaskbarButton -------------------------------------------------------

void TaskbarButton::Bind(uint32_t surfaceId, const std::string& title,
                         bool active, bool minimized) {
    surfaceId_ = surfaceId;
    active_ = active;
    minimized_ = minimized;
    SetText(title);
}

void TaskbarButton::OnClick() {
    if (onClickWindow_) {
        onClickWindow_(surfaceId_, active_);
    }
}

void TaskbarButton::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    if (client.IsEmpty()) return;

    // 네비 셀(스펙 §4) — 의미 그림(관례 글리프)+소형 라벨: 뒤=◀(좌향 삼각형
    // 세로 절단 스윕), 홈=◯(Android HOME 관례 동형 원), 최근=겹쳐린 두 창
    // (Recents 관례). 라벨 = 셀 하단 한글(그림 만으로 의미가 끊기지 않게 —
    // 사용자 원존 "그림도 없고 설명도 없고 → 의미 있게 그려주세요").
    if (navMode_) {
        const auto& t = jk::theme::current();
        dc.SetColor(t.taskbarBorderActive.r, t.taskbarBorderActive.g,
                    t.taskbarBorderActive.b, 255);
        dc.DrawRect(client);
        dc.SetColor(t.taskbarTextActive.r, t.taskbarTextActive.g,
                    t.taskbarTextActive.b, 255);
        dc.SetTextColor(t.taskbarTextActive.r, t.taskbarTextActive.g,
                        t.taskbarTextActive.b);
        const int cx = client.x + client.w / 2;
        const int cy = client.y + (client.h - 14) / 2 + 2;
        const std::string& label = GetText();
        if (label == "뒤") {
            // ◀ — 세로 1px 절단 스윕(왼쪽 끝=끝점·오른쪽 끝=만폭).
            for (int j = 0; j < 10; ++j) {
                const int half = 6 * j / 10;
                dc.FillRect(JKRect{ cx - 6 + j, cy - half, 1, half * 2 + 1 });
            }
        } else if (label == "홈") {
            // ◯ — Android HOME 관례 동형(수평 절단 스윕 원).
            for (int dy = -5; dy <= 5; ++dy) {
                const int w2 = static_cast<int>(
                    std::lround(std::sqrt(25.0 - dy * dy)));
                dc.FillRect(JKRect{ cx - w2, cy + dy, w2 * 2, 1 });
            }
        } else if (label == "최근") {
            // ▤ — Recents 관례 동형(겹쳐진 두 창 외곽선).
            dc.DrawRect(JKRect{ cx - 7, cy - 7, 10, 10 });
            dc.DrawRect(JKRect{ cx - 3, cy - 3, 10, 10 });
        } else if (label == "정리") {
            // ‖ — 정렬(창 배치 save_layout) 기호(두 수직 바).
            dc.FillRect(JKRect{ cx - 5, cy - 6, 3, 12 });
            dc.FillRect(JKRect{ cx + 2, cy - 6, 3, 12 });
        } else if (label.size() == 5 && label[2] == ':') {
            // [S3 시계 — 트레이 결정판] 7세그먼트 기하(FillRect만 — 자식 텍
            // 스트 전 경로 불발 실측 봉합). 세그 인덱스 A=1 B=2 C=4 D=8
            // E=16 F=32 G=64 — 표준 7세그 비트맵.
            // 비트 정의: A(위)=1 B(우상)=2 C(우하)=4 D(아래)=8 E(좌하)=16
            // F(좌상)=32 G(중)=64 — 표준 7세그 숫자 비트맵.
            static constexpr uint8_t SEG[10] = {
                63, 6, 91, 79, 102, 109, 125, 7, 127, 111 };
            (void)SEG;
            int digit[4];
            digit[0] = label[0] - '0';
            digit[1] = label[1] - '0';
            digit[2] = label[3] - '0';
            digit[3] = label[4] - '0';
            // [폭 정리 — 2026-11-09 실측: 셀 90px에 7세그가 넘쳐 우측 절단]
            // gw12·gap4·콜론폭 6 — 총폭 62(< 셀 내부 82) — 중앙 정렬·클램프.
            const int gw = 12, hh = 20, gap = 4;
            const int totalW = gw * 4 + gap * 2 + 6;
            int px = client.x + std::max(4, (client.w - totalW) / 2);
            for (int di = 0; di < 4; ++di) {
                const int d = digit[di];
                if (di == 2) {
                    dc.FillRect(JKRect{ px + 1, cy - 7, 4, 4 });
                    dc.FillRect(JKRect{ px + 1, cy + 3, 4, 4 });
                    px += gap + 6;
                    continue;
                }
                const uint8_t segs = SEG[d & 0x0f];
                // 6획: A(위) D(아래) F(좌상) E(좌하) B(우상) C(우하) G(중).
                if (segs & 1) dc.FillRect(JKRect{ px, cy - hh / 2, gw, 3 });
                if (segs & 8) dc.FillRect(JKRect{ px, cy + hh / 2 - 3, gw, 3 });
                if (segs & 32) dc.FillRect(JKRect{ px, cy - hh / 2, 3, hh / 2 });
                if (segs & 16) dc.FillRect(JKRect{ px, cy, 3, hh / 2 });
                if (segs & 2) dc.FillRect(JKRect{ px + gw - 3, cy - hh / 2, 3, hh / 2 });
                if (segs & 4) dc.FillRect(JKRect{ px + gw - 3, cy, 3, hh / 2 });
                if (segs & 64) dc.FillRect(JKRect{ px, cy - 1, gw, 3 });
                px += gw + gap;
            }
            return;
        } else {
            // ▤ — Recents 관례 동형(겹쳐진 두 창 외곽선).
            dc.DrawRect(JKRect{ cx - 7, cy - 7, 10, 10 });
            dc.DrawRect(JKRect{ cx - 3, cy - 3, 10, 10 });
        }
        // 소형 라벨(셀 하단) — 직접 dc 텍스트(TextOutX).
        // [원존 정정 2026-11-11] "미니 텍스처가 불발"로 보였던 증거는 **스텔이 APK**
        // 였다(이후 클러 프로베원으로 폐기) — 자식은 소프트웨어 렌더러라
        // 렌더 타갓 텍스처(미니 킷)가 안되는 것이고, dc 직접 그리기는
        // 상종 국소이다. 라벨 밴드 = 셀 메트릭(cellH — font_scale 승계).
        const jk::text::CellMetrics cm = jk::text::GetCellMetrics();
        const JKRect textRect{ client.x, client.y + client.h
                                   - static_cast<int>(cm.cellH) - 1,
                               client.w, static_cast<int>(cm.cellH) };
        {
            // [수형] 아틀라스 유니코드 직행 우선(KSSM 부분집합 한계 우회)·
            // 실패 시 기존 KSSM/비트맵 경로 평태 보존.
            if (MiniKit().r && EnsureMiniAtlas(MiniKit().r)) {
                dc.SetTextAtlas(MiniKit().atlas.get(), MiniKit().cache.get());
                DrawUnicodeCentered(dc, textRect, label);
            } else {
                const std::string kssm = Utf8ToKssm(label.c_str());
                dc.TextOutX(textRect,
                            kssm.empty() ? label.c_str() : kssm.c_str(),
                            ADJ_YCENTER, false);
            }
        }
        return;
    }

    // Variant faces (docs/28): active = light + white border,
    // minimized = dark + dim text, normal = mid grey.
    // colors: jk::theme::current() (P2) — 값 불변 토큰화.
    const auto& t = jk::theme::current();
    uint8_t fr, fg, fb, tr, tg, tb;
    if (active_) {
        fr = t.taskbarFaceActive.r; fg = t.taskbarFaceActive.g; fb = t.taskbarFaceActive.b;
        tr = t.taskbarTextActive.r; tg = t.taskbarTextActive.g; tb = t.taskbarTextActive.b;
    } else if (minimized_) {
        fr = t.taskbarFaceMinimized.r; fg = t.taskbarFaceMinimized.g; fb = t.taskbarFaceMinimized.b;
        tr = t.taskbarTextMinimized.r; tg = t.taskbarTextMinimized.g; tb = t.taskbarTextMinimized.b;
    } else {
        fr = t.taskbarFaceNormal.r; fg = t.taskbarFaceNormal.g; fb = t.taskbarFaceNormal.b;
        tr = t.taskbarTextNormal.r; tg = t.taskbarTextNormal.g; tb = t.taskbarTextNormal.b;
    }
    dc.SetColor(fr, fg, fb, 255);
    dc.FillRect(client);
    const SDL_Color& border = active_ ? t.taskbarBorderActive : t.taskbarBorderInactive;
    dc.SetColor(border.r, border.g, border.b, 255);
    dc.DrawRect(client);

    // Title hash-color chip.
    const std::string& title = GetText();
    uint8_t cr = 0, cg = 0, cb = 0;
    TitleChipColor(title, cr, cg, cb);
    dc.SetColor(cr, cg, cb, 255);
    const JKRect chip{ client.x + kChipPad,
                       client.y + (client.h - kChipSize) / 2,
                       kChipSize, kChipSize };
    dc.FillRect(chip);

    // Title text (UTF-8 -> KSSM for the bitmap font; clipped to the button).
    if (!title.empty()) {
        dc.SetTextColor(tr, tg, tb);
        const JKRect textRect{ client.x + kChipPad * 2 + kChipSize, client.y,
                               client.w - (kChipPad * 3 + kChipSize), client.h };
        if (textRect.w > 0) {
            dc.TextOutX(textRect, Utf8ToKssm(title.c_str()).c_str(),
                        ADJ_YCENTER, false);
        }
    }
}

// --- TaskbarWindow -------------------------------------------------------

void ClientTaskbarApp::TaskbarWindow::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    const auto& t = jk::theme::current();
    dc.SetColor(t.taskbarBg.r, t.taskbarBg.g, t.taskbarBg.b, 255);
    dc.FillRect(client);
    for (const auto& child : GetChildren()) {
        if (child->IsVisible()) {
            child->PaintClient(dc);
        }
    }
}

} // namespace jk
