#include <apps/ClientMineSweeperApp.h>

#include <agent/JKAgentJson.h>
#include <apps/MineSweeperApp.h>
#include <JKResourceCache.h>
#include <JKWindow.h>
#include <client/JKClientSurface.h>
#include <cstdio>

namespace jk {

namespace {

// 16x16 RGBA icons identical to the ones used by the single-process app.
std::vector<uint8_t> CreateMineIcon() {
    constexpr int kIconSize = 16;
    std::vector<uint8_t> data(kIconSize * kIconSize * 4, 0);
    auto set = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        if (x < 0 || x >= kIconSize || y < 0 || y >= kIconSize) return;
        int idx = (y * kIconSize + x) * 4;
        data[idx + 0] = r;
        data[idx + 1] = g;
        data[idx + 2] = b;
        data[idx + 3] = a;
    };
    auto drawCircle = [&](int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b) {
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                if (x * x + y * y <= radius * radius + radius / 2) {
                    set(cx + x, cy + y, r, g, b, 255);
                }
            }
        }
    };
    auto drawLine = [&](int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b) {
        int dx = std::abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
        int dy = -std::abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            set(x1, y1, r, g, b, 255);
            if (x1 == x2 && y1 == y2) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x1 += sx; }
            if (e2 <= dx) { err += dx; y1 += sy; }
        }
    };
    drawCircle(8, 8, 5, 0, 0, 0);
    drawCircle(6, 6, 1, 192, 192, 192);
    drawLine(8, 1, 8, 4, 0, 0, 0);
    drawLine(8, 11, 8, 14, 0, 0, 0);
    drawLine(1, 8, 4, 8, 0, 0, 0);
    drawLine(11, 8, 14, 8, 0, 0, 0);
    drawLine(3, 3, 5, 5, 0, 0, 0);
    drawLine(11, 3, 13, 5, 0, 0, 0);
    drawLine(3, 13, 5, 11, 0, 0, 0);
    drawLine(11, 13, 13, 11, 0, 0, 0);
    return data;
}

std::vector<uint8_t> CreateFlagIcon() {
    constexpr int kIconSize = 16;
    std::vector<uint8_t> data(kIconSize * kIconSize * 4, 0);
    auto set = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        if (x < 0 || x >= kIconSize || y < 0 || y >= kIconSize) return;
        int idx = (y * kIconSize + x) * 4;
        data[idx + 0] = r;
        data[idx + 1] = g;
        data[idx + 2] = b;
        data[idx + 3] = a;
    };
    for (int y = 2; y < 14; ++y) set(5, y, 0, 0, 0, 255);
    for (int x = 3; x < 8; ++x) set(x, 13, 0, 0, 0, 255);
    for (int y = 2; y < 7; ++y) {
        int width = 6 - (y - 2);
        for (int x = 6; x < 6 + width; ++x) set(x, y, 255, 0, 0, 255);
    }
    return data;
}

std::vector<uint8_t> CreateQuestionIcon() {
    constexpr int kIconSize = 16;
    std::vector<uint8_t> data(kIconSize * kIconSize * 4, 0);
    auto set = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        if (x < 0 || x >= kIconSize || y < 0 || y >= kIconSize) return;
        int idx = (y * kIconSize + x) * 4;
        data[idx + 0] = r;
        data[idx + 1] = g;
        data[idx + 2] = b;
        data[idx + 3] = a;
    };
    auto setRect = [&](int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
        for (int yy = y; yy < y + h; ++yy)
            for (int xx = x; xx < x + w; ++xx)
                set(xx, yy, r, g, b, 255);
    };
    setRect(5, 3, 6, 2, 0, 0, 0);
    setRect(9, 3, 2, 6, 0, 0, 0);
    setRect(5, 7, 6, 2, 0, 0, 0);
    setRect(5, 7, 2, 4, 0, 0, 0);
    setRect(6, 12, 3, 2, 0, 0, 0);
    return data;
}

// NIT-5 — JKClientSurface의 JsonEsc는 TU 로컬이라 재사용 불가. unsupported_tool
// 에코에 외래 토큰을 실을 때 JSON 구조 파괴/주입 봉쇄용 최소 이스케이퍼.
std::string JsonEsc(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char ch : s) {
        switch (ch) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
                    out += buf;
                } else {
                    out += ch;
                }
        }
    }
    return out;
}

} // anonymous namespace

class ClientMineSweeperApp::Impl {
public:
    std::unique_ptr<MineGameWindow> mineWindow;
    bool iconsLoaded = false;
    ClientMineSweeperApp* app = nullptr;

    static constexpr int kTimerMs = 100;

    // 의미 커서 등록 (스펙 2026-09-22-semantic-cursor §2): 자기 도구(act/
    // snapshot) + 실측 격자 선언서를 AgentToolRegister로 등록한다 — vplayer
    // 선례 (ClientVPlayerApp.cpp :1687). MINOR-4 — 난이도 변경이 격자
    // 지오메트리(rows/cols)를 바꾸므로 MineGameWindow::SetCursorDeclChangedCb
    // 콜백으로 재등록한다(서버 upsert = cursorState (0,0) 리셋 — 새 격자의
    // 좌상단이므로 정확한 정의 전이). 선언서는 렌더 코드의 실측 보고
    // (MineGrid::GetBoardGeometry — HitTestCell/OnPaintClient와 동일 산식)다.
    // 레이아웃 미완으로 선언이 비면 등록을 아예 건너뛴다 — 커서 없는 act는
    // 게이트 ask 보장(cursorOwner 계약)이 깨지므로 도구만 등록하는 혼종을
    // 만들지 않는다(fail-closed).
    void RegisterAgentTools(jk::client::JKClientSurface* surface) {
        if (!app || !mineWindow || !surface) return;
        const std::string cursorDecl = mineWindow->CursorDeclJson();
        if (!surface || cursorDecl.empty()) return;
        using Decl = jk::client::JKClientSurface::AgentToolDecl;
        std::vector<Decl> tools = {
            {"act",
             "Minesweeper semantic act on the cursor cell: reveal (flood-fill "
             "open, echoes opened), flag/question/clear (cell mark), chord "
             "(open neighbors around a flagged number cell), reset "
             "(new board; reset ignores row/col - send 0,0). Echoes "
             "kind/row/col/opened/status; invalid transitions return "
             "error=bad_state.",
             "{\"type\":\"object\",\"properties\":{\"kind\":{\"type\":"
             "\"string\",\"enum\":[\"reveal\",\"flag\",\"question\",\"clear\","
             "\"chord\",\"reset\"]},\"row\":{\"type\":\"integer\"},\"col\":"
             "{\"type\":\"integer\"}},\"required\":[\"kind\",\"row\",\"col\"]}"},
            {"snapshot",
             "Serialize the minesweeper board: 9 text lines (one per row, "
             "'#' closed / 'F' flag / '?' question / digit opened / '*' mine "
             "exposed after game over) plus status/mine/flag/opened counts. "
             "Cursor-independent (the cursor header is added by the server).",
             "{\"type\":\"object\",\"properties\":{}}"},
        };
        surface->SendAgentToolRegister("minesweeper", tools, false, cursorDecl);
    }

    void LoadIcons(JKResourceCache* cache) {
        if (iconsLoaded || !cache) return;
        // PNG assets take precedence; the procedural fallback keeps the game
        // working without an assets/ tree (asset spec: ARCHITECTURE_DOCS/20).
        if (!cache->LoadImagePNG("mine", "assets/icons/mine@1x.png"))
            cache->CreateImageFromRGBA("mine", 16, 16, CreateMineIcon());
        if (!cache->LoadImagePNG("flag", "assets/icons/flag@1x.png"))
            cache->CreateImageFromRGBA("flag", 16, 16, CreateFlagIcon());
        if (!cache->LoadImagePNG("question", "assets/icons/question@1x.png"))
            cache->CreateImageFromRGBA("question", 16, 16, CreateQuestionIcon());
        iconsLoaded = true;
    }
};

ClientMineSweeperApp::ClientMineSweeperApp() : impl_(std::make_unique<Impl>()) {}
ClientMineSweeperApp::~ClientMineSweeperApp() = default;

void ClientMineSweeperApp::OnInit() {
    impl_->app = this;
    auto main = std::make_unique<JKWindow>("Minesweeper");
    main->SetWindowRect(JKRect{ 0, 0, 320, 380 });

    // The game window must exactly fill the main window's client area (the
    // root paints the title bar/border chrome that the server overlays), and
    // dock-fill so server-initiated resizes propagate into the game layout.
    const JKRect clientArea = main->GetClientRect();
    impl_->mineWindow = std::make_unique<MineGameWindow>();
    impl_->mineWindow->Build(main.get(), JKRect{ 0, 0, clientArea.w, clientArea.h });
    // MineWindow is built as a floating window (own title bar + move/resize
    // attrs) for the single-process path. In server mode the window server
    // owns all chrome, so strip it: chrome-less and fixed in place. Re-setting
    // the rect recomputes the client area (full rect) and relayouts children.
    auto* gameWin = impl_->mineWindow->GetWindow();
    gameWin->SetAttrFlags(WA_CHROMELESS);
    gameWin->SetWindowRect(JKRect{ 0, 0, clientArea.w, clientArea.h });
    gameWin->SetDock(DOCK_FILL);
    impl_->mineWindow->NewGame();
    // MINOR-4 — 난이도 변경(메뉴 클릭) 후 재등록. OnInit은 JKClientApplication
    // ::Init의 surface_->Connect() 이후에 불린다 — 연결 전 "조용한 false" 경로
    // 회피.
    impl_->mineWindow->SetCursorDeclChangedCb(
        [this]() { impl_->RegisterAgentTools(Surface()); });

    SetMainWindow(std::move(main));
    SetTimerInterval(Impl::kTimerMs);

    impl_->LoadIcons(GetResourceCache());

    impl_->RegisterAgentTools(Surface());
}

bool ClientMineSweeperApp::OnAgentToolCall(const std::string& tool,
                                           const std::string& argsJson,
                                           std::string& resultJson) {
    // 의미 커서 act/snapshot (스펙 §3) — 서버가 move/read를 플랫폼 구현으로
    // 흡수하므로 앱이 받는 중계는 act(파킹 승인 후)와 snapshot뿐이다.
    if (!impl_->mineWindow) {
        resultJson = "{\"ok\":true,\"error\":\"window_gone\"}";
        return true;
    }
    if (tool == "act") {
        const agent::AgentJson args(argsJson.empty() ? "{}" : argsJson);
        std::string kind;
        int row = 0, col = 0;
        if (!args.ok() || !args.GetStr("kind", kind) ||
            !args.GetInt("row", row) || !args.GetInt("col", col)) {
            resultJson = "{\"ok\":true,\"error\":\"bad_args\"}";
            return true;
        }
        impl_->mineWindow->Act(kind, row, col, resultJson);
        return true;
    }
    if (tool == "snapshot") {
        impl_->mineWindow->Snapshot(resultJson);
        return true;
    }
    resultJson = "{\"ok\":true,\"error\":\"unsupported_tool\",\"tool\":\"" +
                 JsonEsc(tool) + "\"}";
    return true;
}

bool ClientMineSweeperApp::PreProcessMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::Timer && impl_->mineWindow) {
        impl_->mineWindow->OnTimer(Impl::kTimerMs);
    }
    return JKClientApplication::PreProcessMessage(ev);
}

} // namespace jk
