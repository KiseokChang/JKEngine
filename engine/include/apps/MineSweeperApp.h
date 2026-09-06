#ifndef APPS_MINESWEEPERAPP_H
#define APPS_MINESWEEPERAPP_H

#include <JKApplication.h>
#include <JKControl.h>
#include <JKDC.h>
#include <JKEvent.h>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace jk {

class MineSweeperGame {
public:
    enum class Mark { None, Flag, Question };
    enum class Difficulty { Beginner, Intermediate, Expert };

    struct Settings {
        int rows = 0;
        int cols = 0;
        int mines = 0;
    };

    MineSweeperGame();

    static Settings GetSettings(Difficulty diff);

    void SetDifficulty(Difficulty diff);
    Difficulty GetDifficulty() const { return difficulty_; }

    // Start a new game. If firstRow/firstCol are valid, generate mines so that
    // cell and its immediate neighbors are guaranteed safe.
    void NewGame(int firstRow = -1, int firstCol = -1);

    // Test helper: place mines at the given deterministic positions.
    // Clears any existing difficulty-based board size; resizes to rows x cols.
    void NewGameWithMines(int rows, int cols,
                          const std::vector<std::pair<int, int>>& mines);

    // Open a cell. Returns true if the cell was opened. On the first open with
    // an ungenerated board, mines are generated around the excluded cell.
    bool OpenCell(int row, int col);

    // Cycle cell mark: None -> Flag -> Question -> None.
    void CycleMark(int row, int col);

    // If the cell is revealed and its number equals the count of adjacent
    // flags, open all non-flagged covered neighbors. Returns true if any
    // neighbor was opened.
    bool ChordReveal(int row, int col);

    int GetRows() const { return rows_; }
    int GetCols() const { return cols_; }
    int GetMineCount() const { return mineCount_; }

    bool IsValid(int row, int col) const;
    bool IsMine(int row, int col) const;
    bool IsRevealed(int row, int col) const;
    Mark GetMark(int row, int col) const;
    int GetAdjacent(int row, int col) const;

    bool IsGameOver() const { return gameOver_; }
    bool IsWon() const { return won_; }
    bool IsStarted() const { return minesGenerated_; }
    int GetRevealedCount() const { return revealedCount_; }
    int GetFlagCount() const { return flagCount_; }
    int GetRemainingMines() const { return mineCount_ - flagCount_; }

private:
    int rows_ = 9;
    int cols_ = 9;
    int mineCount_ = 10;
    Difficulty difficulty_ = Difficulty::Beginner;

    std::vector<std::vector<bool>> mines_;
    std::vector<std::vector<bool>> revealed_;
    std::vector<std::vector<Mark>> marks_;
    std::vector<std::vector<uint8_t>> adjacent_;

    bool gameOver_ = false;
    bool won_ = false;
    bool minesGenerated_ = false;
    int revealedCount_ = 0;
    int flagCount_ = 0;

    void Resize(int rows, int cols);
    void Clear();
    void GenerateMines(int excludeRow, int excludeCol);
    void ComputeAdjacent();
    void CheckWin();
};

class MineGrid : public JKControl {
public:
    MineGrid(const JKRect& rect, MineSweeperGame& game,
             std::function<void()> onChanged,
             std::function<void(bool)> onGameOver,
             std::function<void()> onFirstOpen);

    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;
    void OnKillFocus() override;

    void ResetChordState();

private:
    MineSweeperGame& game_;
    std::function<void()> onChanged_;
    std::function<void(bool)> onGameOver_;
    std::function<void()> onFirstOpen_;
    int cellSize_ = 20;

    bool leftDown_ = false;
    bool rightDown_ = false;
    int chordRow_ = -1;
    int chordCol_ = -1;

    bool HitTestCell(int x, int y, int& row, int& col,
                     JKRect* outCellRect = nullptr) const;
    void DrawCell(JKDC& dc, int row, int col, const JKRect& cell) const;

    bool TryChordAt(int x, int y);
};

// Reusable floating Minesweeper game window. Used by MineSweeperApp and by
// the separate-process client application.
class MineGameWindow {
public:
    MineGameWindow();
    ~MineGameWindow();

    // Build the window and add it as a child of `parent`. The window rect is in
    // parent client coordinates.
    void Build(JKControl* parent, const JKRect& rect);

    void NewGame();
    // Advance the clock by deltaMs. The caller should fire this on its timer.
    void OnTimer(uint32_t deltaMs);
    JKWindow* GetWindow() const;
    MineSweeperGame& Game();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

class MineSweeperApp : public JKApplication {
public:
    MineSweeperApp();
    ~MineSweeperApp() override;

protected:
    void OnInit() override;
    bool PreProcessMessage(const JKEvent& ev) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace jk

#endif // APPS_MINESWEEPERAPP_H
