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

    static constexpr int kRows = 9;
    static constexpr int kCols = 9;
    static constexpr int kMineCount = 10;

    MineSweeperGame();

    // Start a new game. If firstRow/firstCol are valid, generate mines so that
    // cell and its immediate neighbors are guaranteed safe.
    void NewGame(int firstRow = -1, int firstCol = -1);

    // Test helper: place mines at the given deterministic positions.
    void NewGameWithMines(const std::vector<std::pair<int, int>>& mines);

    // Open a cell. Returns true if the cell was opened. On the first open with
    // an ungenerated board, mines are generated around the excluded cell.
    bool OpenCell(int row, int col);

    // Cycle cell mark: None -> Flag -> Question -> None.
    void CycleMark(int row, int col);

    // If the cell is revealed and its number equals the count of adjacent
    // flags, open all non-flagged covered neighbors. Returns true if any
    // neighbor was opened.
    bool ChordReveal(int row, int col);

    bool IsValid(int row, int col) const;
    bool IsMine(int row, int col) const;
    bool IsRevealed(int row, int col) const;
    Mark GetMark(int row, int col) const;
    int GetAdjacent(int row, int col) const;

    bool IsGameOver() const { return gameOver_; }
    bool IsWon() const { return won_; }
    int GetRevealedCount() const { return revealedCount_; }
    int GetFlagCount() const { return flagCount_; }
    int GetRemainingMines() const { return kMineCount - flagCount_; }

private:
    std::array<std::array<bool, kCols>, kRows> mines_ = {};
    std::array<std::array<bool, kCols>, kRows> revealed_ = {};
    std::array<std::array<Mark, kCols>, kRows> marks_ = {};
    std::array<std::array<uint8_t, kCols>, kRows> adjacent_ = {};

    bool gameOver_ = false;
    bool won_ = false;
    bool minesGenerated_ = false;
    int revealedCount_ = 0;
    int flagCount_ = 0;

    void Clear();
    void GenerateMines(int excludeRow, int excludeCol);
    void ComputeAdjacent();
    void RevealCell(int row, int col);
    void CheckWin();
};

class MineGrid : public JKControl {
public:
    MineGrid(const JKRect& rect, MineSweeperGame& game,
             std::function<void()> onChanged,
             std::function<void(bool)> onGameOver);

    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;

private:
    MineSweeperGame& game_;
    std::function<void()> onChanged_;
    std::function<void(bool)> onGameOver_;
    int cellSize_ = 20;

    bool HitTestCell(int x, int y, int& row, int& col) const;
    void DrawCell(JKDC& dc, int row, int col, const JKRect& cell) const;
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
