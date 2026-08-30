#include <apps/MineSweeperApp.h>

#include <apps/AppUtil.h>
#include <JKApplication.h>
#include <JKButton.h>
#include <JKMessageBox.h>
#include <JKStatic.h>
#include <JKWindow.h>
#include <JKTypes.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <queue>
#include <random>
#include <utility>

namespace jk {

namespace {

constexpr int kButtonAreaHeight = 44;
constexpr int kMargin = 10;
constexpr int kCellSize = 24;
constexpr int kMinWindowWidth = 320;

struct NumberColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

constexpr NumberColor kNumberColors[9] = {
    {  0,   0,   0}, // 0 unused
    {  0,   0, 255}, // 1 blue
    {  0, 128,   0}, // 2 green
    {255,   0,   0}, // 3 red
    {  0,   0, 128}, // 4 dark blue
    {128,   0,   0}, // 5 maroon
    {  0, 128, 128}, // 6 cyan
    {  0,   0,   0}, // 7 black
    {128, 128, 128}, // 8 gray
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// MineSweeperGame
// ---------------------------------------------------------------------------

MineSweeperGame::MineSweeperGame() {
    Resize(rows_, cols_);
}

MineSweeperGame::Settings MineSweeperGame::GetSettings(Difficulty diff) {
    switch (diff) {
        case Difficulty::Beginner:      return { 9,  9, 10};
        case Difficulty::Intermediate:  return {16, 16, 40};
        case Difficulty::Expert:        return {16, 30, 99};
    }
    return {9, 9, 10};
}

void MineSweeperGame::SetDifficulty(Difficulty diff) {
    difficulty_ = diff;
    Settings s = GetSettings(diff);
    rows_ = s.rows;
    cols_ = s.cols;
    mineCount_ = s.mines;
    Resize(rows_, cols_);
}

void MineSweeperGame::Resize(int rows, int cols) {
    rows_ = rows;
    cols_ = cols;
    mines_.assign(rows_, std::vector<bool>(cols_, false));
    revealed_.assign(rows_, std::vector<bool>(cols_, false));
    marks_.assign(rows_, std::vector<Mark>(cols_, Mark::None));
    adjacent_.assign(rows_, std::vector<uint8_t>(cols_, 0));
}

void MineSweeperGame::Clear() {
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            mines_[r][c] = false;
            revealed_[r][c] = false;
            marks_[r][c] = Mark::None;
            adjacent_[r][c] = 0;
        }
    }
    gameOver_ = false;
    won_ = false;
    minesGenerated_ = false;
    revealedCount_ = 0;
    flagCount_ = 0;
}

bool MineSweeperGame::IsValid(int row, int col) const {
    return row >= 0 && row < rows_ && col >= 0 && col < cols_;
}

void MineSweeperGame::NewGame(int firstRow, int firstCol) {
    Clear();
    if (IsValid(firstRow, firstCol)) {
        GenerateMines(firstRow, firstCol);
    }
}

void MineSweeperGame::NewGameWithMines(int rows, int cols,
                                       const std::vector<std::pair<int, int>>& mines) {
    SetDifficulty(Difficulty::Beginner);
    Resize(rows, cols);
    rows_ = rows;
    cols_ = cols;
    mineCount_ = static_cast<int>(mines.size());
    Clear();
    for (const auto& m : mines) {
        if (IsValid(m.first, m.second)) {
            mines_[m.first][m.second] = true;
        }
    }
    ComputeAdjacent();
    minesGenerated_ = true;
}

void MineSweeperGame::GenerateMines(int excludeRow, int excludeCol) {
    std::vector<std::pair<int, int>> candidates;
    candidates.reserve(rows_ * cols_);
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            // Exclude the first-clicked cell and its 8 neighbors.
            if (std::abs(r - excludeRow) <= 1 && std::abs(c - excludeCol) <= 1) {
                continue;
            }
            candidates.emplace_back(r, c);
        }
    }

    int count = std::min(mineCount_, static_cast<int>(candidates.size()));
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(candidates.begin(), candidates.end(), gen);

    for (int i = 0; i < count; ++i) {
        mines_[candidates[i].first][candidates[i].second] = true;
    }
    ComputeAdjacent();
    minesGenerated_ = true;
}

void MineSweeperGame::ComputeAdjacent() {
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            if (mines_[r][c]) {
                adjacent_[r][c] = 0;
                continue;
            }
            int count = 0;
            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    if (dr == 0 && dc == 0) continue;
                    int nr = r + dr;
                    int nc = c + dc;
                    if (IsValid(nr, nc) && mines_[nr][nc]) ++count;
                }
            }
            adjacent_[r][c] = static_cast<uint8_t>(count);
        }
    }
}

bool MineSweeperGame::OpenCell(int row, int col) {
    if (gameOver_ || !IsValid(row, col)) return false;
    if (revealed_[row][col] || marks_[row][col] != Mark::None) return false;

    if (!minesGenerated_) {
        GenerateMines(row, col);
    }

    if (mines_[row][col]) {
        revealed_[row][col] = true;
        ++revealedCount_;
        gameOver_ = true;
        won_ = false;
        return true;
    }

    // Iterative flood fill: reveal the clicked cell and any connected zeros.
    std::queue<std::pair<int, int>> q;
    q.emplace(row, col);
    while (!q.empty()) {
        auto [r, c] = q.front();
        q.pop();
        if (revealed_[r][c] || marks_[r][c] != Mark::None) continue;
        revealed_[r][c] = true;
        ++revealedCount_;
        if (adjacent_[r][c] == 0) {
            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    if (dr == 0 && dc == 0) continue;
                    int nr = r + dr;
                    int nc = c + dc;
                    if (IsValid(nr, nc) && !revealed_[nr][nc] && marks_[nr][nc] == Mark::None) {
                        q.emplace(nr, nc);
                    }
                }
            }
        }
    }

    CheckWin();
    return true;
}

void MineSweeperGame::CycleMark(int row, int col) {
    if (gameOver_ || !IsValid(row, col) || revealed_[row][col]) return;

    Mark oldMark = marks_[row][col];
    Mark newMark;
    switch (oldMark) {
        case Mark::None:    newMark = Mark::Flag; break;
        case Mark::Flag:    newMark = Mark::Question; break;
        case Mark::Question:
        default:            newMark = Mark::None; break;
    }
    marks_[row][col] = newMark;
    if (oldMark == Mark::Flag) --flagCount_;
    if (newMark == Mark::Flag) ++flagCount_;
}

bool MineSweeperGame::ChordReveal(int row, int col) {
    if (gameOver_ || !IsValid(row, col) || !revealed_[row][col]) return false;

    int adj = adjacent_[row][col];
    if (adj == 0) return false;

    int flagCountAround = 0;
    std::vector<std::pair<int, int>> neighbors;
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) continue;
            int nr = row + dr;
            int nc = col + dc;
            if (!IsValid(nr, nc)) continue;
            if (marks_[nr][nc] == Mark::Flag) ++flagCountAround;
            else if (!revealed_[nr][nc]) neighbors.emplace_back(nr, nc);
        }
    }

    if (flagCountAround != adj) return false;

    bool openedAny = false;
    for (const auto& n : neighbors) {
        if (OpenCell(n.first, n.second)) openedAny = true;
    }
    return openedAny;
}

void MineSweeperGame::CheckWin() {
    if (gameOver_) return;
    int safeCells = rows_ * cols_ - mineCount_;
    if (revealedCount_ >= safeCells) {
        gameOver_ = true;
        won_ = true;
    }
}

bool MineSweeperGame::IsMine(int row, int col) const {
    return IsValid(row, col) && mines_[row][col];
}

bool MineSweeperGame::IsRevealed(int row, int col) const {
    return IsValid(row, col) && revealed_[row][col];
}

MineSweeperGame::Mark MineSweeperGame::GetMark(int row, int col) const {
    if (!IsValid(row, col)) return Mark::None;
    return marks_[row][col];
}

int MineSweeperGame::GetAdjacent(int row, int col) const {
    if (!IsValid(row, col)) return 0;
    return adjacent_[row][col];
}

// ---------------------------------------------------------------------------
// MineGrid
// ---------------------------------------------------------------------------

MineGrid::MineGrid(const JKRect& rect, MineSweeperGame& game,
                   std::function<void()> onChanged,
                   std::function<void(bool)> onGameOver,
                   std::function<void()> onFirstOpen)
    : game_(game), onChanged_(std::move(onChanged)),
      onGameOver_(std::move(onGameOver)),
      onFirstOpen_(std::move(onFirstOpen)) {
    SetRect(rect);
    SetFocusable(true);
}

bool MineGrid::HitTestCell(int x, int y, int& row, int& col) const {
    const JKRect client = GetScreenClientRect();
    int cols = game_.GetCols();
    int rows = game_.GetRows();
    int cellSize = std::min(client.w / cols, client.h / rows);
    if (cellSize <= 0) return false;
    int boardW = cellSize * cols;
    int boardH = cellSize * rows;
    int offsetX = (client.w - boardW) / 2;
    int offsetY = (client.h - boardH) / 2;

    int lx = x - (client.x + offsetX);
    int ly = y - (client.y + offsetY);
    if (lx < 0 || ly < 0) return false;

    col = lx / cellSize;
    row = ly / cellSize;
    return col < cols && row < rows;
}

void MineGrid::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    dc.SetColor(192, 192, 192, 255);
    dc.FillRect(client);

    int cols = game_.GetCols();
    int rows = game_.GetRows();
    int cellSize = std::min(client.w / cols, client.h / rows);
    if (cellSize <= 0) return;
    int boardW = cellSize * cols;
    int boardH = cellSize * rows;
    int offsetX = (client.w - boardW) / 2;
    int offsetY = (client.h - boardH) / 2;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            JKRect cell{
                client.x + offsetX + c * cellSize,
                client.y + offsetY + r * cellSize,
                cellSize,
                cellSize
            };
            DrawCell(dc, r, c, cell);
        }
    }

    JKControl::OnPaintClient(dc);
}

void MineGrid::DrawCell(JKDC& dc, int row, int col, const JKRect& cell) const {
    bool revealed = game_.IsRevealed(row, col);
    bool isMine = game_.IsMine(row, col);
    auto mark = game_.GetMark(row, col);

    if (revealed && isMine) {
        // The mine that ended the game.
        dc.SetColor(255, 0, 0, 255);
        dc.FillRect(cell);
        dc.SetTextColor(0, 0, 0);
        dc.TextOutX(cell, "*", ADJ_XYCENTER, false);
        return;
    }

    if (!revealed) {
        // Covered cell with raised border.
        dc.Box3D(cell, 2, 192, 192, 192, 255, 255, 255, 0, 0, 0);
        if (mark == MineSweeperGame::Mark::Flag) {
            dc.SetTextColor(255, 0, 0);
            dc.TextOutX(cell, "F", ADJ_XYCENTER, false);
        } else if (mark == MineSweeperGame::Mark::Question) {
            dc.SetTextColor(0, 0, 0);
            dc.TextOutX(cell, "?", ADJ_XYCENTER, false);
        }
        return;
    }

    // Revealed empty/safe cell.
    dc.SetColor(224, 224, 224, 255);
    dc.FillRect(cell);
    dc.SetColor(128, 128, 128, 255);
    dc.DrawRect(cell);

    if (game_.IsGameOver() && isMine) {
        // Reveal all mines when the game is over.
        dc.SetTextColor(0, 0, 0);
        dc.TextOutX(cell, "*", ADJ_XYCENTER, false);
        return;
    }

    int adj = game_.GetAdjacent(row, col);
    if (adj > 0) {
        const auto& color = kNumberColors[adj];
        dc.SetTextColor(color.r, color.g, color.b);
        char buf[2] = { static_cast<char>('0' + adj), '\0' };
        dc.TextOutX(cell, buf, ADJ_XYCENTER, false);
    }
}

bool MineGrid::TryChordAt(int x, int y) {
    int row = 0, col = 0;
    if (!HitTestCell(x, y, row, col)) return false;

    bool wasStarted = game_.IsStarted();
    bool changed = game_.ChordReveal(row, col);
    if (changed) {
        if (!wasStarted && game_.IsStarted()) {
            onFirstOpen_();
        }
        onChanged_();
        if (game_.IsGameOver()) {
            onGameOver_(game_.IsWon());
        }
    }
    return changed;
}

void MineGrid::RespondMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::MouseDown) {
        SetFocus();
        int row = 0, col = 0;
        bool inside = HitTestCell(ev.x, ev.y, row, col);

        if (ev.detail == SDL_BUTTON_LEFT) {
            leftDown_ = true;
            // If right button is already held, trigger chord on this cell.
            if (rightDown_ && inside) {
                chordRow_ = row;
                chordCol_ = col;
                TryChordAt(ev.x, ev.y);
                return;
            }
            // Normal left-click open.
            if (inside) {
                bool wasStarted = game_.IsStarted();
                bool changed = game_.OpenCell(row, col);
                if (changed) {
                    if (!wasStarted && game_.IsStarted()) {
                        onFirstOpen_();
                    }
                    onChanged_();
                    if (game_.IsGameOver()) {
                        onGameOver_(game_.IsWon());
                    }
                }
            }
            return;
        }

        if (ev.detail == SDL_BUTTON_RIGHT) {
            rightDown_ = true;
            // If left button is already held, trigger chord on this cell.
            if (leftDown_ && inside) {
                chordRow_ = row;
                chordCol_ = col;
                TryChordAt(ev.x, ev.y);
                return;
            }
            // Normal right-click mark cycle.
            if (inside) {
                game_.CycleMark(row, col);
                onChanged_();
            }
            return;
        }

        if (ev.detail == SDL_BUTTON_MIDDLE) {
            if (inside) {
                TryChordAt(ev.x, ev.y);
            }
            return;
        }

        JKControl::RespondMessage(ev);
        return;
    }

    if (ev.type == JKEventType::MouseUp) {
        if (ev.detail == SDL_BUTTON_LEFT) {
            leftDown_ = false;
            chordRow_ = -1;
            chordCol_ = -1;
        } else if (ev.detail == SDL_BUTTON_RIGHT) {
            rightDown_ = false;
            chordRow_ = -1;
            chordCol_ = -1;
        }
        return;
    }

    JKControl::RespondMessage(ev);
}

// ---------------------------------------------------------------------------
// MineSweeperApp
// ---------------------------------------------------------------------------

class MineSweeperApp::Impl {
public:
    MineSweeperGame game;
    MineGrid* grid = nullptr;
    JKStatic* mineLabel = nullptr;
    JKStatic* timeLabel = nullptr;
    JKButton* beginnerBtn = nullptr;
    JKButton* intermediateBtn = nullptr;
    JKButton* expertBtn = nullptr;
    MineSweeperApp* app = nullptr;
    std::unique_ptr<JKMessageBox> gameOverBox;

    int elapsedSeconds = 0;
    bool timerRunning = false;
    bool gameOverShown = false;

    void NewGame() {
        game.NewGame();
        elapsedSeconds = 0;
        timerRunning = false;
        gameOverShown = false;
        UpdateLabels();
        if (grid) grid->SetFocus();
    }

    void SetDifficulty(MineSweeperGame::Difficulty diff) {
        game.SetDifficulty(diff);
        ResizeWindowForDifficulty();
        NewGame();
    }

    void ResizeWindowForDifficulty() {
        if (!app) return;
        JKWindow* main = app->GetMainWindow();
        if (!main) return;

        int w = std::max(kMinWindowWidth, kMargin * 2 + game.GetCols() * kCellSize);
        int h = kButtonAreaHeight + kMargin + game.GetRows() * kCellSize + kMargin;
        main->SetWindowRect(JKRect{ 0, 0, w, h });

        // The grid rect must fill the new client area below the button area.
        const JKRect& client = main->GetClientRect();
        if (grid) {
            grid->SetRect(JKRect{ kMargin, kButtonAreaHeight,
                                  client.w - kMargin * 2,
                                  client.h - kButtonAreaHeight - kMargin });
        }
    }

    void OnFirstOpen() {
        if (!timerRunning) {
            timerRunning = true;
        }
    }

    void OnChanged() {
        UpdateLabels();
        if (game.IsGameOver() && !gameOverShown) {
            OnGameOver(game.IsWon());
        }
    }

    void OnGameOver(bool won) {
        timerRunning = false;
        gameOverShown = true;
        std::string title = won ? "Victory" : "Game Over";
        std::string message = won ? "You cleared the minefield!" : "BOOM! You hit a mine.";

        // Discard the previous (already closed) message box before opening a new one.
        gameOverBox.reset();
        apputil::ShowModalMessage(nullptr, gameOverBox, title, message,
                                  JKMessageBox::Buttons::Ok,
                                  [this](int) { NewGame(); });
    }

    void UpdateLabels() {
        char buf[64];
        if (mineLabel) {
            std::snprintf(buf, sizeof(buf), "Mines: %d", game.GetRemainingMines());
            mineLabel->SetText(buf);
        }
        if (timeLabel) {
            std::snprintf(buf, sizeof(buf), "Time: %d", elapsedSeconds);
            timeLabel->SetText(buf);
        }
    }

    void UpdateDifficultyButtonState() {
        if (!beginnerBtn || !intermediateBtn || !expertBtn) return;
        auto diff = game.GetDifficulty();
        // Visual feedback only; disabled/pressed states are not supported yet.
        (void)diff;
    }
};

MineSweeperApp::MineSweeperApp() : impl_(std::make_unique<Impl>()) {}

MineSweeperApp::~MineSweeperApp() = default;

void MineSweeperApp::OnInit() {
    impl_->app = this;

    auto main = std::make_unique<JKWindow>("Minesweeper");
    main->SetWindowRect(JKRect{ 0, 0, 320, 380 });
    main->SetAttrFlags(WA_TITLEMOVEABLE | WA_BORDERRESIZABLE);

    // Top bar controls.
    auto newGameBtn = std::make_unique<JKButton>(JKRect{ 10, 8, 50, 28 }, 101);
    newGameBtn->SetText("New");
    newGameBtn->SetOnClick([this]() { impl_->NewGame(); });
    main->AddControl(std::move(newGameBtn));

    auto beginnerBtn = std::make_unique<JKButton>(JKRect{ 70, 8, 28, 28 }, 102);
    beginnerBtn->SetText("B");
    beginnerBtn->SetOnClick([this]() { impl_->SetDifficulty(MineSweeperGame::Difficulty::Beginner); });
    impl_->beginnerBtn = beginnerBtn.get();
    main->AddControl(std::move(beginnerBtn));

    auto intermediateBtn = std::make_unique<JKButton>(JKRect{ 102, 8, 28, 28 }, 103);
    intermediateBtn->SetText("I");
    intermediateBtn->SetOnClick([this]() { impl_->SetDifficulty(MineSweeperGame::Difficulty::Intermediate); });
    impl_->intermediateBtn = intermediateBtn.get();
    main->AddControl(std::move(intermediateBtn));

    auto expertBtn = std::make_unique<JKButton>(JKRect{ 134, 8, 28, 28 }, 104);
    expertBtn->SetText("E");
    expertBtn->SetOnClick([this]() { impl_->SetDifficulty(MineSweeperGame::Difficulty::Expert); });
    impl_->expertBtn = expertBtn.get();
    main->AddControl(std::move(expertBtn));

    auto mineLabel = std::make_unique<JKStatic>(JKRect{ 170, 10, 70, 24 }, 105);
    mineLabel->SetText("Mines: 10");
    mineLabel->SetTextColor(0, 0, 0);
    impl_->mineLabel = mineLabel.get();
    main->AddControl(std::move(mineLabel));

    auto timeLabel = std::make_unique<JKStatic>(JKRect{ 170, 32, 70, 24 }, 106);
    timeLabel->SetText("Time: 0");
    timeLabel->SetTextColor(0, 0, 0);
    impl_->timeLabel = timeLabel.get();
    main->AddControl(std::move(timeLabel));

    // The mine grid occupies the rest of the client area.
    JKRect gridRect{ kMargin, kButtonAreaHeight,
                     320 - kMargin * 2,
                     380 - kButtonAreaHeight - kMargin };
    auto grid = std::make_unique<MineGrid>(
        gridRect, impl_->game,
        [this]() { impl_->OnChanged(); },
        [this](bool won) { impl_->OnGameOver(won); },
        [this]() { impl_->OnFirstOpen(); });
    impl_->grid = grid.get();
    main->AddControl(std::move(grid));

    SetMainWindow(std::move(main));
    SetTimerInterval(1000);
    impl_->NewGame();
}

bool MineSweeperApp::PreProcessMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::Timer && impl_->timerRunning) {
        ++impl_->elapsedSeconds;
        impl_->UpdateLabels();
    }
    return JKApplication::PreProcessMessage(ev);
}

} // namespace jk
