#ifndef APPS_TETRISAPP_H
#define APPS_TETRISAPP_H

#include <JKApplication.h>
#include <JKControl.h>
#include <JKDC.h>
#include <JKEvent.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace jk {

// Pure Tetris model: 10x20 board, 7 tetrominoes, rotation, lock delay,
// line clearing, score/level, and game-over detection.
class TetrisGame {
public:
    enum class PieceType { I, O, T, S, Z, J, L };

    struct Piece {
        PieceType type = PieceType::I;
        int rotation = 0; // 0..3
        int row = 0;      // board row of the 4x4 top-left
        int col = 0;      // board col of the 4x4 top-left
    };

    struct Cell {
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        bool filled = false;
    };

    static constexpr int kRows = 20;
    static constexpr int kCols = 10;

    TetrisGame();

    void NewGame();

    // Advance one gravity tick. Returns true if the board changed.
    bool Tick();

    // User actions. Return true if the board changed.
    bool MoveLeft();
    bool MoveRight();
    bool SoftDrop();
    bool HardDrop();
    bool RotateClockwise();
    bool RotateCounterClockwise();

    bool IsGameOver() const { return gameOver_; }
    int GetScore() const { return score_; }
    int GetLines() const { return lines_; }
    int GetLevel() const { return level_; }

    const Piece& GetCurrentPiece() const { return current_; }
    const Cell (&GetBoard() const)[kRows][kCols] { return board_; }

    // Get the color for a piece type.
    static void GetPieceColor(PieceType type, uint8_t& r, uint8_t& g, uint8_t& b);

    // Recommended timer interval in milliseconds for the current level.
    uint32_t GetTimerInterval() const;

private:
    Cell board_[kRows][kCols]{};
    Piece current_;
    PieceType nextType_ = PieceType::I;

    int score_ = 0;
    int lines_ = 0;
    int level_ = 1;
    bool gameOver_ = false;

    void SpawnPiece();
    PieceType RandomType();
    void LockPiece();
    int ClearLines();

    // Check whether the 4x4 piece shape at (row,col,rotation) fits on the board.
    bool Fits(PieceType type, int rotation, int row, int col) const;

    // Try to rotate with simple wall kicks.
    bool TryRotate(int delta);
};

class TetrisGrid : public JKControl {
public:
    TetrisGrid(const JKRect& rect, TetrisGame& game,
               std::function<void()> onChanged);

    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;

private:
    TetrisGame& game_;
    std::function<void()> onChanged_;

    void DrawBlock(JKDC& dc, const JKRect& cell, uint8_t r, uint8_t g, uint8_t b) const;
    void DrawPiece(JKDC& dc, const TetrisGame::Piece& piece,
                   int cellSize, int offsetX, int offsetY) const;
};

// Reusable floating Tetris window. The launcher and TetrisApp both use this.
class TetrisGameWindow {
public:
    TetrisGameWindow();
    ~TetrisGameWindow();

    // Build the window and add it as a child of `parent`. The window rect
    // is in parent client coordinates.
    void Build(JKControl* parent, const JKRect& rect);

    void NewGame();
    // Advance by deltaMs. Internally accumulates time and falls the piece
    // when the current level's interval has elapsed.
    void OnTimer(uint32_t deltaMs);
    JKWindow* GetWindow() const;
    uint32_t GetTimerInterval() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

class TetrisApp : public JKApplication {
public:
    TetrisApp();
    ~TetrisApp() override;

protected:
    void OnInit() override;
    bool PreProcessMessage(const JKEvent& ev) override;

private:
    std::unique_ptr<TetrisGameWindow> gameWindow_;
};

} // namespace jk

#endif // APPS_TETRISAPP_H
