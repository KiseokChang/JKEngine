#include <apps/TetrisApp.h>

#include <apps/AppUtil.h>
#include <JKApplication.h>
#include <JKButton.h>
#include <JKMessageBox.h>
#include <JKSoundManager.h>
#include <JKStatic.h>
#include <JKWindow.h>
#include <JKTypes.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <random>

namespace jk {

namespace {

constexpr int kButtonAreaHeight = 36;
constexpr int kButtonTopY = 4;
constexpr int kMargin = 10;

// Background panel for toolbars.
class JKPanel : public JKControl {
public:
    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetScreenClientRect();
        if (!client.IsEmpty()) {
            dc.SetColor(backR_, backG_, backB_, 255);
            dc.FillRect(client);
        }
        JKControl::OnPaintClient(dc);
    }
};

// Piece shapes: [type][rotation][row][col].
constexpr bool kShapes[7][4][4][4] = {
    // I
    {
        {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
        {{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}},
        {{0,0,0,0},{0,0,0,0},{1,1,1,1},{0,0,0,0}},
        {{0,1,0,0},{0,1,0,0},{0,1,0,0},{0,1,0,0}}
    },
    // O
    {
        {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}
    },
    // T
    {
        {{0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,0,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}},
        {{0,0,0,0},{1,1,1,0},{0,1,0,0},{0,0,0,0}},
        {{0,1,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}}
    },
    // S
    {
        {{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,0,0},{0,1,1,0},{0,0,1,0},{0,0,0,0}},
        {{0,0,0,0},{0,1,1,0},{1,1,0,0},{0,0,0,0}},
        {{1,0,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}}
    },
    // Z
    {
        {{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,0,1,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}},
        {{0,0,0,0},{1,1,0,0},{0,1,1,0},{0,0,0,0}},
        {{0,1,0,0},{1,1,0,0},{1,0,0,0},{0,0,0,0}}
    },
    // J
    {
        {{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,1,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}},
        {{0,0,0,0},{1,1,1,0},{0,0,1,0},{0,0,0,0}},
        {{0,1,0,0},{0,1,0,0},{1,1,0,0},{0,0,0,0}}
    },
    // L
    {
        {{0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,0,0},{0,1,0,0},{0,1,1,0},{0,0,0,0}},
        {{0,0,0,0},{1,1,1,0},{1,0,0,0},{0,0,0,0}},
        {{1,1,0,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}}
    }
};

int ShapeIndex(TetrisGame::PieceType type) {
    return static_cast<int>(type);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// TetrisGame
// ---------------------------------------------------------------------------

TetrisGame::TetrisGame() {
    NewGame();
}

void TetrisGame::NewGame() {
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            board_[r][c] = Cell{};
        }
    }
    score_ = 0;
    lines_ = 0;
    level_ = 1;
    gameOver_ = false;
    nextType_ = RandomType();
    SpawnPiece();
}

void TetrisGame::GetPieceColor(PieceType type, uint8_t& r, uint8_t& g, uint8_t& b) {
    switch (type) {
        case PieceType::I: r = 0;   g = 255; b = 255; break;
        case PieceType::O: r = 255; g = 255; b = 0;   break;
        case PieceType::T: r = 128; g = 0;   b = 128; break;
        case PieceType::S: r = 0;   g = 255; b = 0;   break;
        case PieceType::Z: r = 255; g = 0;   b = 0;   break;
        case PieceType::J: r = 0;   g = 0;   b = 255; break;
        case PieceType::L: r = 255; g = 165; b = 0;   break;
    }
}

TetrisGame::PieceType TetrisGame::RandomType() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 6);
    return static_cast<PieceType>(dist(gen));
}

void TetrisGame::SpawnPiece() {
    current_.type = nextType_;
    current_.rotation = 0;
    current_.row = 0;
    current_.col = 3;
    nextType_ = RandomType();

    if (!Fits(current_.type, current_.rotation, current_.row, current_.col)) {
        gameOver_ = true;
    }
}

bool TetrisGame::Fits(PieceType type, int rotation, int row, int col) const {
    int t = ShapeIndex(type);
    int r = ((rotation % 4) + 4) % 4;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if (!kShapes[t][r][y][x]) continue;
            int br = row + y;
            int bc = col + x;
            if (bc < 0 || bc >= kCols) return false;
            if (br >= kRows) return false;
            if (br >= 0 && board_[br][bc].filled) return false;
        }
    }
    return true;
}

bool TetrisGame::TryRotate(int delta) {
    if (gameOver_) return false;
    int newRot = ((current_.rotation + delta) % 4 + 4) % 4;
    // Simple wall kicks: try the requested rotation, then small horizontal
    // offsets, then a tiny upward offset for the I piece near the top.
    const int kicks[][2] = { {0,0}, {-1,0}, {1,0}, {-2,0}, {2,0}, {0,-1} };
    for (const auto& k : kicks) {
        int nr = current_.row + k[1];
        int nc = current_.col + k[0];
        if (Fits(current_.type, newRot, nr, nc)) {
            current_.rotation = newRot;
            current_.row = nr;
            current_.col = nc;
            return true;
        }
    }
    return false;
}

bool TetrisGame::RotateClockwise() {
    bool ok = TryRotate(1);
    if (ok) JKSoundManager::GetInstance().PlaySFX("tetris_rotate", kAudioBusTetris);
    return ok;
}

bool TetrisGame::RotateCounterClockwise() {
    bool ok = TryRotate(-1);
    if (ok) JKSoundManager::GetInstance().PlaySFX("tetris_rotate", kAudioBusTetris);
    return ok;
}

bool TetrisGame::MoveLeft() {
    if (gameOver_) return false;
    if (Fits(current_.type, current_.rotation, current_.row, current_.col - 1)) {
        --current_.col;
        JKSoundManager::GetInstance().PlaySFX("tetris_move", kAudioBusTetris);
        return true;
    }
    return false;
}

bool TetrisGame::MoveRight() {
    if (gameOver_) return false;
    if (Fits(current_.type, current_.rotation, current_.row, current_.col + 1)) {
        ++current_.col;
        JKSoundManager::GetInstance().PlaySFX("tetris_move", kAudioBusTetris);
        return true;
    }
    return false;
}

bool TetrisGame::SoftDrop() {
    if (gameOver_) return false;
    if (Fits(current_.type, current_.rotation, current_.row + 1, current_.col)) {
        ++current_.row;
        ++score_;
        return true;
    }
    LockPiece();
    return true;
}

bool TetrisGame::HardDrop() {
    if (gameOver_) return false;
    int dropped = 0;
    while (Fits(current_.type, current_.rotation, current_.row + 1, current_.col)) {
        ++current_.row;
        ++dropped;
    }
    score_ += dropped * 2;
    JKSoundManager::GetInstance().PlaySFX("tetris_drop", kAudioBusTetris);
    LockPiece();
    return true;
}

bool TetrisGame::Tick() {
    if (gameOver_) return false;
    return SoftDrop();
}

void TetrisGame::LockPiece() {
    int t = ShapeIndex(current_.type);
    int r = current_.rotation;
    uint8_t pr, pg, pb;
    GetPieceColor(current_.type, pr, pg, pb);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if (!kShapes[t][r][y][x]) continue;
            int br = current_.row + y;
            int bc = current_.col + x;
            if (br >= 0 && br < kRows && bc >= 0 && bc < kCols) {
                board_[br][bc] = Cell{ pr, pg, pb, true };
            }
        }
    }

    int cleared = ClearLines();
    if (cleared > 0) {
        JKSoundManager::GetInstance().PlaySFX("tetris_clear", kAudioBusTetris);
        lines_ += cleared;
        score_ += 100 * level_ * cleared * cleared;
        level_ = 1 + lines_ / 10;
    }

    SpawnPiece();
    if (gameOver_) {
        JKSoundManager::GetInstance().PlaySFX("tetris_gameover", kAudioBusTetris);
        JKSoundManager::GetInstance().StopBGM();
    }
}

int TetrisGame::ClearLines() {
    int cleared = 0;
    for (int r = kRows - 1; r >= 0; --r) {
        bool full = true;
        for (int c = 0; c < kCols; ++c) {
            if (!board_[r][c].filled) { full = false; break; }
        }
        if (full) {
            // Shift everything above down one row.
            for (int rr = r; rr > 0; --rr) {
                for (int c = 0; c < kCols; ++c) {
                    board_[rr][c] = board_[rr - 1][c];
                }
            }
            for (int c = 0; c < kCols; ++c) {
                board_[0][c] = Cell{};
            }
            ++cleared;
            ++r; // recheck this row after shift.
        }
    }
    return cleared;
}

uint32_t TetrisGame::GetTimerInterval() const {
    int ms = 500 - (level_ - 1) * 50;
    return static_cast<uint32_t>(std::max(ms, 100));
}

// ---------------------------------------------------------------------------
// TetrisGrid
// ---------------------------------------------------------------------------

TetrisGrid::TetrisGrid(const JKRect& rect, TetrisGame& game,
                       std::function<void()> onChanged)
    : game_(game), onChanged_(std::move(onChanged)) {
    SetRect(rect);
    SetFocusable(true);
}

void TetrisGrid::DrawBlock(JKDC& dc, const JKRect& cell,
                           uint8_t r, uint8_t g, uint8_t b) const {
    dc.SetColor(r, g, b, 255);
    dc.FillRect(cell);
    dc.Box3D(cell, 1, r, g, b, 255, 255, 255, 0, 0, 0);
}

void TetrisGrid::DrawPiece(JKDC& dc, const TetrisGame::Piece& piece,
                           int cellSize, int offsetX, int offsetY) const {
    int t = ShapeIndex(piece.type);
    int r = piece.rotation;
    uint8_t pr, pg, pb;
    TetrisGame::GetPieceColor(piece.type, pr, pg, pb);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if (!kShapes[t][r][y][x]) continue;
            int br = piece.row + y;
            int bc = piece.col + x;
            if (br < 0 || br >= TetrisGame::kRows || bc < 0 || bc >= TetrisGame::kCols) continue;
            JKRect cell{
                offsetX + bc * cellSize,
                offsetY + br * cellSize,
                cellSize,
                cellSize
            };
            DrawBlock(dc, cell, pr, pg, pb);
        }
    }
}

void TetrisGrid::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    dc.SetColor(0, 0, 0, 255);
    dc.FillRect(client);

    int cellSize = std::min(client.w / TetrisGame::kCols, client.h / TetrisGame::kRows);
    if (cellSize <= 0) return;
    int boardW = cellSize * TetrisGame::kCols;
    int boardH = cellSize * TetrisGame::kRows;
    int offsetX = client.x + (client.w - boardW) / 2;
    int offsetY = client.y + (client.h - boardH) / 2;

    // Locked cells.
    for (int r = 0; r < TetrisGame::kRows; ++r) {
        for (int c = 0; c < TetrisGame::kCols; ++c) {
            const auto& cell = game_.GetBoard()[r][c];
            if (!cell.filled) continue;
            JKRect rc{ offsetX + c * cellSize, offsetY + r * cellSize, cellSize, cellSize };
            DrawBlock(dc, rc, cell.r, cell.g, cell.b);
        }
    }

    // Current piece.
    DrawPiece(dc, game_.GetCurrentPiece(), cellSize, offsetX, offsetY);

    // Grid lines.
    dc.SetColor(64, 64, 64, 255);
    for (int c = 0; c <= TetrisGame::kCols; ++c) {
        int x = offsetX + c * cellSize;
        dc.DrawLine(x, offsetY, x, offsetY + boardH);
    }
    for (int r = 0; r <= TetrisGame::kRows; ++r) {
        int y = offsetY + r * cellSize;
        dc.DrawLine(offsetX, y, offsetX + boardW, y);
    }

    JKControl::OnPaintClient(dc);
}

void TetrisGrid::RespondMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::MouseDown) {
        SetFocus();
        return;
    }
    if (ev.type == JKEventType::KeyDown) {
        bool changed = false;
        switch (ev.keyCode) {
            case SDLK_LEFT:  changed = game_.MoveLeft(); break;
            case SDLK_RIGHT: changed = game_.MoveRight(); break;
            case SDLK_UP:    changed = game_.RotateClockwise(); break;
            case SDLK_DOWN:  changed = game_.SoftDrop(); break;
            case SDLK_SPACE: changed = game_.HardDrop(); break;
            default: break;
        }
        if (changed) {
            Invalidate();
            onChanged_();
        }
        return;
    }
    JKControl::RespondMessage(ev);
}

// ---------------------------------------------------------------------------
// TetrisWindow
// ---------------------------------------------------------------------------

class TetrisWindow : public JKWindow {
public:
    TetrisWindow() : JKWindow("Tetris") {
        SetAttrFlags(WA_TITLEMOVEABLE | WA_BORDERRESIZABLE);
    }

    void OnClose() override {
        // Stop the Tetris theme BGM when the floating game window closes.
        JKSoundManager::GetInstance().StopBGM();
        JKWindow::OnClose();
    }

    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetScreenClientRect();
        dc.SetColor(32, 32, 32, 255);
        dc.FillRect(client);
        JKWindow::OnPaintClient(dc);
    }
};

// ---------------------------------------------------------------------------
// TetrisGameWindow
// ---------------------------------------------------------------------------

class TetrisGameWindow::Impl {
public:
    TetrisGame game;
    TetrisWindow* window = nullptr;
    TetrisGrid* grid = nullptr;
    JKStatic* scoreLabel = nullptr;
    JKStatic* levelLabel = nullptr;
    std::unique_ptr<JKMessageBox> gameOverBox;
    bool gameOverShown = false;
    uint32_t fallAccumulator = 0;

    void UpdateLabels() {
        char buf[64];
        if (scoreLabel) {
            std::snprintf(buf, sizeof(buf), "Score: %d", game.GetScore());
            scoreLabel->SetText(buf);
        }
        if (levelLabel) {
            std::snprintf(buf, sizeof(buf), "Level: %d", game.GetLevel());
            levelLabel->SetText(buf);
        }
    }

    void OnChanged() {
        UpdateLabels();
        if (game.IsGameOver() && !gameOverShown) {
            OnGameOver();
        }
    }

    void OnGameOver() {
        gameOverShown = true;
        JKSoundManager::GetInstance().PlaySFX("tetris_gameover", kAudioBusTetris);
        JKSoundManager::GetInstance().StopBGM();
        std::string title = "Game Over";
        char message[128];
        std::snprintf(message, sizeof(message),
                      "Score: %d\nLines: %d\nLevel: %d",
                      game.GetScore(), game.GetLines(), game.GetLevel());
        gameOverBox.reset();
        apputil::ShowModalMessage(window, gameOverBox, title, message,
                                  JKMessageBox::Buttons::Ok,
                                  [this](int) { NewGame(); });
    }

    void NewGame() {
        game.NewGame();
        gameOverShown = false;
        gameOverBox.reset();
        UpdateLabels();
        if (grid) {
            grid->Invalidate();
            grid->SetFocus();
        }
        JKSoundManager::GetInstance().PlayBGM("tetris_theme", -1);
    }
};

TetrisGameWindow::TetrisGameWindow() : impl_(std::make_unique<Impl>()) {}
TetrisGameWindow::~TetrisGameWindow() = default;

void TetrisGameWindow::Build(JKControl* parent, const JKRect& rect) {
    auto win = std::make_unique<TetrisWindow>();
    win->SetWindowRect(rect);
    impl_->window = win.get();

    // Grid first (painted under toolbar), toolbar second.
    auto g = std::make_unique<TetrisGrid>(
        JKRect{ 0, 0, 100, 100 }, impl_->game,
        [this]() { impl_->OnChanged(); });
    g->SetDock(DOCK_FILL);
    g->SetMargins(kMargin, kMargin, kMargin, kMargin);
    impl_->grid = g.get();
    win->AddControl(std::move(g));

    auto toolbar = std::make_unique<JKPanel>();
    toolbar->SetRect(JKRect{ 0, 0, 320, kButtonAreaHeight });
    toolbar->SetDock(DOCK_TOP);
    toolbar->SetPadding(2, 2, 2, 2);
    toolbar->SetBackColor(210, 210, 210);

    auto newBtn = std::make_unique<JKButton>(JKRect{ 10, kButtonTopY, 50, 26 }, 101);
    newBtn->SetText("New");
    newBtn->SetOnClick([this]() { impl_->NewGame(); });
    toolbar->AddControl(std::move(newBtn));

    auto score = std::make_unique<JKStatic>(JKRect{ 70, kButtonTopY, 90, 24 }, 102);
    score->SetText("Score: 0");
    score->SetTextColor(0, 0, 0);
    impl_->scoreLabel = score.get();
    toolbar->AddControl(std::move(score));

    auto level = std::make_unique<JKStatic>(JKRect{ 170, kButtonTopY, 70, 24 }, 103);
    level->SetText("Level: 1");
    level->SetTextColor(0, 0, 0);
    impl_->levelLabel = level.get();
    toolbar->AddControl(std::move(level));

    win->AddControl(std::move(toolbar));

    if (parent) {
        parent->AddControl(std::move(win));
    }
}

JKWindow* TetrisGameWindow::GetWindow() const {
    return impl_->window;
}

uint32_t TetrisGameWindow::GetTimerInterval() const {
    return impl_->game.GetTimerInterval();
}

void TetrisGameWindow::NewGame() {
    impl_->fallAccumulator = 0;
    impl_->NewGame();
}

void TetrisGameWindow::OnTimer(uint32_t deltaMs) {
    if (impl_->game.IsGameOver()) return;
    impl_->fallAccumulator += deltaMs;
    uint32_t interval = impl_->game.GetTimerInterval();
    bool changed = false;
    while (impl_->fallAccumulator >= interval) {
        impl_->fallAccumulator -= interval;
        if (!impl_->game.IsGameOver()) {
            if (impl_->game.Tick()) {
                changed = true;
                interval = impl_->game.GetTimerInterval();
            }
        }
    }
    if (changed) {
        impl_->UpdateLabels();
        if (impl_->grid) impl_->grid->Invalidate();
        if (impl_->game.IsGameOver() && !impl_->gameOverShown) {
            impl_->OnGameOver();
        }
    }
}

// ---------------------------------------------------------------------------
// TetrisApp
// ---------------------------------------------------------------------------

TetrisApp::TetrisApp() = default;
TetrisApp::~TetrisApp() = default;

void TetrisApp::OnInit() {
    auto main = std::make_unique<JKWindow>("Tetris Desktop");
    main->SetWindowRect(JKRect{ 0, 0, 1920, 1080 });

    gameWindow_ = std::make_unique<TetrisGameWindow>();
    gameWindow_->Build(main.get(), JKRect{ 100, 100, 320, 520 });

    SetMainWindow(std::move(main));

    uint32_t interval = gameWindow_->GetTimerInterval();
    SetTimerInterval(interval);
    gameWindow_->NewGame();
}

bool TetrisApp::PreProcessMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::Timer && gameWindow_) {
        uint32_t interval = gameWindow_->GetTimerInterval();
        gameWindow_->OnTimer(interval);
        SetTimerInterval(interval);
    }
    return JKApplication::PreProcessMessage(ev);
}

} // namespace jk
