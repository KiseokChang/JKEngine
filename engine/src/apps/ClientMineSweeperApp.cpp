#include <apps/ClientMineSweeperApp.h>

#include <apps/MineSweeperApp.h>
#include <JKResourceCache.h>
#include <JKWindow.h>

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

} // anonymous namespace

class ClientMineSweeperApp::Impl {
public:
    std::unique_ptr<MineGameWindow> mineWindow;
    bool iconsLoaded = false;

    static constexpr int kTimerMs = 100;

    void LoadIcons(JKResourceCache* cache) {
        if (iconsLoaded || !cache) return;
        cache->CreateImageFromRGBA("mine", 16, 16, CreateMineIcon());
        cache->CreateImageFromRGBA("flag", 16, 16, CreateFlagIcon());
        cache->CreateImageFromRGBA("question", 16, 16, CreateQuestionIcon());
        iconsLoaded = true;
    }
};

ClientMineSweeperApp::ClientMineSweeperApp() : impl_(std::make_unique<Impl>()) {}
ClientMineSweeperApp::~ClientMineSweeperApp() = default;

void ClientMineSweeperApp::OnInit() {
    auto main = std::make_unique<JKWindow>("Minesweeper");
    main->SetWindowRect(JKRect{ 0, 0, 320, 380 });

    impl_->mineWindow = std::make_unique<MineGameWindow>();
    impl_->mineWindow->Build(main.get(), JKRect{ 0, 0, 320, 380 });
    impl_->mineWindow->NewGame();

    SetMainWindow(std::move(main));
    SetTimerInterval(Impl::kTimerMs);

    impl_->LoadIcons(GetResourceCache());
}

bool ClientMineSweeperApp::PreProcessMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::Timer && impl_->mineWindow) {
        impl_->mineWindow->OnTimer(Impl::kTimerMs);
    }
    return JKClientApplication::PreProcessMessage(ev);
}

} // namespace jk
