#include <apps/MineSweeperApp.h>

#include <apps/AppLauncherItem.h>
#include <apps/AppUtil.h>
#include <apps/TetrisApp.h>
#include <JKApplication.h>
#include <JKButton.h>
#include <JKMessageBox.h>
#include <JKResourceCache.h>
#include <JKSoundManager.h>
#include <JKStatic.h>
#include <JKWindow.h>
#include <JKTypes.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <queue>
#include <random>
#include <utility>
#include <vector>

namespace jk {

namespace {

constexpr int kButtonAreaHeight = 36;
constexpr int kButtonTopY = 4;
constexpr int kMargin = 10;
constexpr int kCellSize = 24;
constexpr int kMinWindowWidth = 200;
constexpr int kIconSize = 16;

// Simple container control that fills its client area with its back color.
// Used as the Minesweeper toolbar so the docked top band is visually distinct.
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

// Simple 16x16 RGBA icon generators (no external files needed).
std::vector<uint8_t> CreateMineIcon() {
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
    // Black mine body with grey highlight.
    drawCircle(8, 8, 5, 0, 0, 0);
    drawCircle(6, 6, 1, 192, 192, 192);
    // Spikes.
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
    std::vector<uint8_t> data(kIconSize * kIconSize * 4, 0);
    auto set = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        if (x < 0 || x >= kIconSize || y < 0 || y >= kIconSize) return;
        int idx = (y * kIconSize + x) * 4;
        data[idx + 0] = r;
        data[idx + 1] = g;
        data[idx + 2] = b;
        data[idx + 3] = a;
    };
    // Pole.
    for (int y = 2; y < 14; ++y) set(5, y, 0, 0, 0, 255);
    // Base.
    for (int x = 3; x < 8; ++x) set(x, 13, 0, 0, 0, 255);
    // Red flag.
    for (int y = 2; y < 7; ++y) {
        int width = 6 - (y - 2);
        for (int x = 6; x < 6 + width; ++x) set(x, y, 255, 0, 0, 255);
    }
    return data;
}

std::vector<uint8_t> CreateQuestionIcon() {
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
    // Question mark shape (black).
    setRect(5, 3, 6, 2, 0, 0, 0);   // top bar
    setRect(9, 3, 2, 6, 0, 0, 0);   // right vertical
    setRect(5, 7, 6, 2, 0, 0, 0);   // middle bar
    setRect(5, 7, 2, 4, 0, 0, 0);   // left tail
    setRect(6, 12, 3, 2, 0, 0, 0);  // dot
    return data;
}

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
        JKSoundManager::GetInstance().PlaySFX("mine_explosion", kAudioBusMine);
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

bool MineGrid::HitTestCell(int x, int y, int& row, int& col, JKRect* outCellRect) const {
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
    if (col >= cols || row >= rows) return false;

    if (outCellRect) {
        outCellRect->x = client.x + offsetX + col * cellSize;
        outCellRect->y = client.y + offsetY + row * cellSize;
        outCellRect->w = cellSize;
        outCellRect->h = cellSize;
    }
    return true;
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

    JKResourceCache* cache = g_currentJKApp ? g_currentJKApp->GetResourceCache() : nullptr;

    if (revealed && isMine) {
        // The mine that ended the game.
        dc.SetColor(255, 0, 0, 255);
        dc.FillRect(cell);
        if (cache) {
            auto tex = cache->GetImage("mine");
            dc.DrawSpriteX(cell, tex, kIconSize, kIconSize, ADJ_XYCENTER);
        } else {
            dc.SetTextColor(0, 0, 0);
            dc.TextOutX(cell, "*", ADJ_XYCENTER, false);
        }
        return;
    }

    if (!revealed) {
        // Covered cell with raised border.
        dc.Box3D(cell, 2, 192, 192, 192, 255, 255, 255, 0, 0, 0);
        if (mark == MineSweeperGame::Mark::Flag) {
            if (cache) {
                auto tex = cache->GetImage("flag");
                dc.DrawSpriteX(cell, tex, kIconSize, kIconSize, ADJ_XYCENTER);
            } else {
                dc.SetTextColor(255, 0, 0);
                dc.TextOutX(cell, "F", ADJ_XYCENTER, false);
            }
        } else if (mark == MineSweeperGame::Mark::Question) {
            if (cache) {
                auto tex = cache->GetImage("question");
                dc.DrawSpriteX(cell, tex, kIconSize, kIconSize, ADJ_XYCENTER);
            } else {
                dc.SetTextColor(0, 0, 0);
                dc.TextOutX(cell, "?", ADJ_XYCENTER, false);
            }
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
        if (cache) {
            auto tex = cache->GetImage("mine");
            dc.DrawSpriteX(cell, tex, kIconSize, kIconSize, ADJ_XYCENTER);
        } else {
            dc.SetTextColor(0, 0, 0);
            dc.TextOutX(cell, "*", ADJ_XYCENTER, false);
        }
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

void MineGrid::ResetChordState() {
    leftDown_ = false;
    rightDown_ = false;
    chordRow_ = -1;
    chordCol_ = -1;
}

void MineGrid::OnKillFocus() {
    ResetChordState();
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
        } else {
            // Chord may open multiple cells; invalidate the entire board region.
            Invalidate();
        }
    }
    return changed;
}

void MineGrid::RespondMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::MouseDown) {
        SetFocus();

        // If the game has ended, clear any stuck chord state and ignore input.
        if (game_.IsGameOver()) {
            ResetChordState();
            return;
        }

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
                    if (!game_.IsGameOver()) {
                        JKSoundManager::GetInstance().PlaySFX("mine_open", kAudioBusMine);
                    }
                    if (!wasStarted && game_.IsStarted()) {
                        onFirstOpen_();
                    }
                    onChanged_();
                    if (game_.IsGameOver()) {
                        onGameOver_(game_.IsWon());
                    } else {
                        // Opening a zero cell flood-fills a region; invalidate the board.
                        Invalidate();
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
                JKSoundManager::GetInstance().PlaySFX("button_click", kAudioBusMine);
                onChanged_();
                JKRect cell;
                HitTestCell(ev.x, ev.y, row, col, &cell);
                // The raised border of a cell touches its neighbours, so expand
                // the invalidation rectangle by the border depth (2) to avoid
                // leaving visual artifacts on adjacent cells.
                cell.x -= 2;
                cell.y -= 2;
                cell.w += 4;
                cell.h += 4;
                InvalidateRect(cell);
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
// MineWindow: floating game window
// ---------------------------------------------------------------------------

class MineWindow : public JKWindow {
public:
    MineWindow() : JKWindow("Minesweeper") {
        SetAttrFlags(WA_TITLEMOVEABLE | WA_BORDERRESIZABLE);
    }

    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetScreenClientRect();
        dc.SetColor(192, 192, 192, 255);
        dc.FillRect(client);
        JKWindow::OnPaintClient(dc);
    }
};

// ---------------------------------------------------------------------------
// MineGameWindow
// ---------------------------------------------------------------------------

class MineGameWindow::Impl {
public:
    MineSweeperGame game;
    MineWindow* window = nullptr;
    MineGrid* grid = nullptr;
    JKStatic* mineLabel = nullptr;
    JKStatic* timeLabel = nullptr;
    std::unique_ptr<JKMessageBox> gameOverBox;
    bool gameOverShown = false;
    bool timerRunning = false;
    int elapsedSeconds = 0;
    int mineTickCounter = 0;

    void NewGame() {
        game.NewGame();
        elapsedSeconds = 0;
        mineTickCounter = 0;
        timerRunning = false;
        gameOverShown = false;
        if (grid) grid->ResetChordState();
        UpdateLabels();
        if (grid) grid->SetFocus();
    }

    void SetDifficulty(MineSweeperGame::Difficulty diff) {
        game.SetDifficulty(diff);
        ResizeMineWindow();
        NewGame();
    }

    void ResizeMineWindow() {
        if (!window) return;
        int w = std::max(kMinWindowWidth, kMargin * 2 + game.GetCols() * kCellSize);
        int clientH = kButtonAreaHeight + kMargin + game.GetRows() * kCellSize + kMargin;
        int h = clientH + 24 + 2;
        JKRect r = window->GetRect();
        r.w = w;
        r.h = h;
        window->SetWindowRect(r);
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
        if (grid) grid->ResetChordState();
        std::string title = won ? "Victory" : "Game Over";
        std::string message = won ? "You cleared the minefield!" : "BOOM! You hit a mine.";

        gameOverBox.reset();
        apputil::ShowModalMessage(window, gameOverBox, title, message,
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

    void OnTimer(uint32_t deltaMs) {
        (void)deltaMs;
        if (timerRunning) {
            ++mineTickCounter;
            if (mineTickCounter >= 10) {
                mineTickCounter = 0;
                ++elapsedSeconds;
                UpdateLabels();
            }
        }
    }
};

MineGameWindow::MineGameWindow() : impl_(std::make_unique<Impl>()) {}
MineGameWindow::~MineGameWindow() = default;

void MineGameWindow::Build(JKControl* parent, const JKRect& rect) {
    auto win = std::make_unique<MineWindow>();
    win->SetWindowRect(rect);
    impl_->window = win.get();

    auto g = std::make_unique<MineGrid>(
        JKRect{ 0, 0, 100, 100 }, impl_->game,
        [this]() { impl_->OnChanged(); },
        [this](bool won) { impl_->OnGameOver(won); },
        [this]() { impl_->OnFirstOpen(); });
    g->SetDock(DOCK_FILL);
    g->SetMargins(kMargin, kMargin, kMargin, kMargin);
    impl_->grid = g.get();
    win->AddControl(std::move(g));

    auto toolbar = std::make_unique<JKPanel>();
    toolbar->SetRect(JKRect{ 0, 0, 320, kButtonAreaHeight });
    toolbar->SetDock(DOCK_TOP);
    toolbar->SetPadding(2, 2, 2, 2);
    toolbar->SetBackColor(210, 210, 210);

    auto newGameBtn = std::make_unique<JKButton>(JKRect{ 10, kButtonTopY, 50, 26 }, 101);
    newGameBtn->SetText("New");
    newGameBtn->SetOnClick([this]() { impl_->NewGame(); });
    toolbar->AddControl(std::move(newGameBtn));

    auto beginnerBtn = std::make_unique<JKButton>(JKRect{ 70, kButtonTopY, 28, 26 }, 102);
    beginnerBtn->SetText("B");
    beginnerBtn->SetOnClick([this]() { impl_->SetDifficulty(MineSweeperGame::Difficulty::Beginner); });
    toolbar->AddControl(std::move(beginnerBtn));

    auto intermediateBtn = std::make_unique<JKButton>(JKRect{ 102, kButtonTopY, 28, 26 }, 103);
    intermediateBtn->SetText("I");
    intermediateBtn->SetOnClick([this]() { impl_->SetDifficulty(MineSweeperGame::Difficulty::Intermediate); });
    toolbar->AddControl(std::move(intermediateBtn));

    auto expertBtn = std::make_unique<JKButton>(JKRect{ 134, kButtonTopY, 28, 26 }, 104);
    expertBtn->SetText("E");
    expertBtn->SetOnClick([this]() { impl_->SetDifficulty(MineSweeperGame::Difficulty::Expert); });
    toolbar->AddControl(std::move(expertBtn));

    auto mineLabel = std::make_unique<JKStatic>(JKRect{ 170, kButtonTopY, 70, 24 }, 105);
    mineLabel->SetText("Mines: 10");
    mineLabel->SetTextColor(0, 0, 0);
    impl_->mineLabel = mineLabel.get();
    toolbar->AddControl(std::move(mineLabel));

    auto timeLabel = std::make_unique<JKStatic>(JKRect{ 240, kButtonTopY, 70, 24 }, 106);
    timeLabel->SetText("Time: 0");
    timeLabel->SetTextColor(0, 0, 0);
    impl_->timeLabel = timeLabel.get();
    toolbar->AddControl(std::move(timeLabel));

    win->AddControl(std::move(toolbar));

    if (parent) {
        parent->AddControl(std::move(win));
    }
}

JKWindow* MineGameWindow::GetWindow() const {
    return impl_->window;
}

MineSweeperGame& MineGameWindow::Game() {
    return impl_->game;
}

void MineGameWindow::NewGame() {
    impl_->mineTickCounter = 0;
    impl_->NewGame();
}

void MineGameWindow::OnTimer(uint32_t deltaMs) {
    impl_->OnTimer(deltaMs);
}

// ---------------------------------------------------------------------------
// MineSweeperApp
// ---------------------------------------------------------------------------

class MineSweeperApp::Impl {
public:
    MineSweeperApp* app = nullptr;
    std::unique_ptr<MineGameWindow> mineWindow;
    std::unique_ptr<TetrisGameWindow> tetrisWindow;
    bool iconsLoaded = false;

    // The launcher drives a shared 100 ms timer.
    static constexpr int kLauncherTimerMs = 100;

    void LoadIcons() {
        if (iconsLoaded) return;
        JKResourceCache* cache = app ? app->GetResourceCache() : nullptr;
        if (!cache) return;
        cache->CreateImageFromRGBA("mine", kIconSize, kIconSize, CreateMineIcon());
        cache->CreateImageFromRGBA("flag", kIconSize, kIconSize, CreateFlagIcon());
        cache->CreateImageFromRGBA("question", kIconSize, kIconSize, CreateQuestionIcon());
        cache->CreateImageFromRGBA("launcher_mine", 32, 32, CreateMineLauncherIcon());
        cache->CreateImageFromRGBA("launcher_tetris", 32, 32, CreateTetrisLauncherIcon());
        iconsLoaded = true;
    }

    void OpenMineWindow(JKWindow* desktop) {
        if (mineWindow && mineWindow->GetWindow() && !mineWindow->GetWindow()->IsCloseRequested()) {
            mineWindow->GetWindow()->FocusFirstChild();
            return;
        }
        if (mineWindow && mineWindow->GetWindow() && mineWindow->GetWindow()->IsCloseRequested()) {
            mineWindow.reset();
        }

        mineWindow = std::make_unique<MineGameWindow>();
        mineWindow->Build(desktop, JKRect{ 100, 100, 320, 380 });
        mineWindow->NewGame();
    }

    void OpenTetrisWindow(JKWindow* desktop) {
        if (tetrisWindow) {
            auto* w = tetrisWindow->GetWindow();
            if (w && !w->IsCloseRequested()) {
                w->FocusFirstChild();
                return;
            }
            tetrisWindow.reset();
        }

        tetrisWindow = std::make_unique<TetrisGameWindow>();
        tetrisWindow->Build(desktop, JKRect{ 140, 140, 320, 520 });
        tetrisWindow->NewGame();
    }

    void CleanupClosedWindows() {
        if (mineWindow && mineWindow->GetWindow() && mineWindow->GetWindow()->IsCloseRequested()) {
            mineWindow.reset();
        }
        if (tetrisWindow) {
            auto* w = tetrisWindow->GetWindow();
            if (!w || w->IsCloseRequested()) {
                tetrisWindow.reset();
            }
        }
    }
};

MineSweeperApp::MineSweeperApp() : impl_(std::make_unique<Impl>()) {}

MineSweeperApp::~MineSweeperApp() = default;

void MineSweeperApp::OnInit() {
    impl_->app = this;

    auto main = std::make_unique<JKWindow>("App Launcher");
    main->SetWindowRect(JKRect{ 0, 0, 1920, 1080 });

    auto mineItem = std::make_unique<AppLauncherItem>(
        JKRect{ 50, 50, 100, 90 }, "Minesweeper",
        [this, mainRaw = main.get()]() { impl_->OpenMineWindow(mainRaw); });
    mineItem->SetIconKey("launcher_mine");
    main->AddControl(std::move(mineItem));

    auto tetrisItem = std::make_unique<AppLauncherItem>(
        JKRect{ 170, 50, 100, 90 }, "Tetris",
        [this, mainRaw = main.get()]() { impl_->OpenTetrisWindow(mainRaw); });
    tetrisItem->SetIconKey("launcher_tetris");
    main->AddControl(std::move(tetrisItem));

    SetMainWindow(std::move(main));

    SetTimerInterval(Impl::kLauncherTimerMs);
    impl_->LoadIcons();
}

bool MineSweeperApp::PreProcessMessage(const JKEvent& ev) {
    impl_->CleanupClosedWindows();

    if (ev.type == JKEventType::Timer) {
        if (impl_->mineWindow) {
            impl_->mineWindow->OnTimer(Impl::kLauncherTimerMs);
        }
        if (impl_->tetrisWindow) {
            impl_->tetrisWindow->OnTimer(Impl::kLauncherTimerMs);
        }
    }
    return JKApplication::PreProcessMessage(ev);
}

} // namespace jk
