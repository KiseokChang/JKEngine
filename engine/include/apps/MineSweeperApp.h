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
    // 의미 커서 act 어휘 (스펙 2026-09-22-semantic-cursor §2/§8) — 문자열은
    // 선언 kinds enum과 정확히 일치한다("reset" 포함 — 상태 머신 리셋 전이).
    enum class ActKind { Reveal, Flag, Question, Clear, Reset };
    // act 결과: ok=false면 error에 유효 전이 토큰, opened = 이 행위가 연 칸수
    // (reveal = 플러드 필 확산 보고, flag/question/clear = 1, reset = 0).
    struct ActOutcome {
        bool ok = false;
        const char* error = nullptr;  // "bad_state" | "bad_grid" | "bad_args"
        int opened = 0;
    };

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

    // 의미 커서 act (스펙 §3): 순수 게임 전이 — 뷰(사운드/무효화/라벨/모달)는
    // MineGameWindow::Act 배선이 한다. 유효 전이 규약:
    //   reveal  — 닫힘+무마크 칸만(열림/마크 칸 = bad_state; 플러드 필 내포,
    //             첫 개방이면 지뢰 생성 — 네이티브 클릭 경로와 동일). 폭발 =
    //             status lost(지뢰 전체 공개 — 직렬화는 SnapshotLines).
    //   flag/question — 닫힘 칸의 마크 설정(멱등 — 이미 같은 마크면 무변화
    //             ok). 열린 칸 = bad_state.
    //   clear   — 마크 제거(멱등). 열린 칸 = bad_state.
    //   reset   — 언제든 유효(게임 오버 포함) — 새 판 + status playing.
    //             row/col은 무시(서버 계약상 act는 row/col 필수라 (0,0)로 온다).
    //             커서는 플랫폼 소유 — 게임은 커서를 모른다(스펙 §4).
    ActOutcome Act(const std::string& kind, int row, int col);
    static bool ParseActKind(const std::string& kind, ActKind& out);
    // "playing" | "lost" | "won" — 게임 오버 전은 항상 playing.
    const char* Status() const;
    // 보드 직렬화 (스펙 §3 read): 위→아래 각 행 하나의 문자열. 닫힘 '#',
    // 깃발 'F', 물음표 '?', 열린 칸 = 인접 숫자('0' 포함), 패배 후 지뢰 전체
    // 공개 '*'. 커서와 무관(커서 헤더는 서버가 조립).
    std::vector<std::string> SnapshotLines() const;

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

    // 의미 커서 (스펙 §2): 렌더 산식(OnPaintClient/HitTestCell과 동일)으로
    // 실측한 보드 격자 기하를 client 표면 좌표로 보고한다 — origin = (0,0)칸의
    // 표면 좌표, cellW/cellH = 렌더 칸 크기. 레이아웃 미완(칸 0)이면 false.
    bool GetBoardGeometry(int& originX, int& originY, int& cellW, int& cellH) const;

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

    // 의미 커서 (스펙 §3): 앱 도구 act/snapshot 핸들러 — 게임 전이(MineSweeperGame::Act)
    // + 뷰 배선(사운드/무효화/라벨/첫 개방 타이머/게임오버 모달)을 한 곳에서.
    // resultJson = 에코 {"ok":true,"kind","row","col","opened","status"} 또는
    // {"ok":true,"error":...,"kind","row","col","opened","status"}(filedlg
    // docs/58 레슨 f 선례 — 앱 실패도 ok:true, 에러는 필드).
    bool Act(const std::string& kind, int row, int col, std::string& resultJson);
    // snapshot 결과: {"ok":true,"status","rows","cols","mines","flags",
    // "opened","lines":[행 문자열...],"board":"\\n 조인"}.
    bool Snapshot(std::string& resultJson);
    // 의미 커서 선언서 (스펙 §2) — GetBoardGeometry 실측값을 담은 cursor 블록
    // 원문 JSON. 레이아웃 미완이면 빈 문자열(선언 생략 = 미선언 앱 동작).
    std::string CursorDeclJson() const;
    // MINOR-4 — 난이도 변경은 rows/cols(=격자 지오메트리)를 바꾼다. 선언서가
    // 변하면 등록 앱(ClientMineSweeperApp)이 AgentToolRegister를 재전송해
    // 서버 캐시를 upsert한다(upsert가 cursorState를 (0,0)으로 리셋 — 새 격자
    // 좌상단이므로 정확한 정의 전이). 레이아웃 변경 후 콜백 발화.
    void SetCursorDeclChangedCb(std::function<void()> cb);

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
