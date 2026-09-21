#include <server/JKSemanticCursor.h>

#include <algorithm>

namespace jk {
namespace server {
namespace {
// 스펙 §3 move: 다중 스텝 상한 32 / 과대 델타 봉쇄(bad_args) — 격자 최대
// 1024칸(서버 선언 검증 상한) 대비 1000배 여유.
constexpr size_t kMaxSteps = 32;
constexpr int kMaxDelta = 1 << 20;
} // namespace

void JKSemanticCursor::Reset(int rows, int cols) {
    if (rows < 1 || cols < 1) return;   // 무효 격자 무시 — 기존 상태 유지
    rows_ = rows;
    cols_ = cols;
    row_ = 0;   // 커서는 항상 (0,0) 좌상단(정의 초기/리셋 위치)으로 리셋
    col_ = 0;
}

JKSemanticCursor::Result JKSemanticCursor::MoveTo(int row, int col) {
    if (rows_ < 1 || cols_ < 1) return Result{row_, col_, "bad_grid"};
    if (row < 0 || col < 0) return Result{row_, col_, "bad_args"};
    if (row >= rows_ || col >= cols_) return Result{row_, col_, "bad_grid"};
    row_ = row;
    col_ = col;
    return Result{row_, col_, nullptr};
}

JKSemanticCursor::Result JKSemanticCursor::MoveRelative(int dr, int dc) {
    if (rows_ < 1 || cols_ < 1) return Result{row_, col_, "bad_grid"};
    if (dr > kMaxDelta || dr < -kMaxDelta || dc > kMaxDelta ||
        dc < -kMaxDelta) {
        return Result{row_, col_, "bad_args"};
    }
    // 격자 경계 클램프 — cursorOwner: platform일 때 서버가 격자 밖을 불가하게
    // 한다(스펙 §3). 도달 위치를 에코한다(클램프된 최종 칸).
    const int nr = std::max(0, std::min(rows_ - 1, row_ + dr));
    const int nc = std::max(0, std::min(cols_ - 1, col_ + dc));
    row_ = nr;
    col_ = nc;
    return Result{row_, col_, nullptr};
}

JKSemanticCursor::Result JKSemanticCursor::RunSteps(
    const std::vector<std::pair<int, int>>& steps) {
    if (rows_ < 1 || cols_ < 1) return Result{row_, col_, "bad_grid"};
    if (steps.empty() || steps.size() > kMaxSteps) {
        return Result{row_, col_, "bad_args"};
    }
    Result r{row_, col_, nullptr};
    for (const auto& s : steps) {
        if (s.first > kMaxDelta || s.first < -kMaxDelta ||
            s.second > kMaxDelta || s.second < -kMaxDelta) {
            return Result{row_, col_, "bad_args"};   // 앞 스텝은 이미 적용됨
        }
        const int targetRow = row_ + s.first;
        const int targetCol = col_ + s.second;
        const int nr = std::max(0, std::min(rows_ - 1, targetRow));
        const int nc = std::max(0, std::min(cols_ - 1, targetCol));
        row_ = nr;
        col_ = nc;
        r.row = nr;
        r.col = nc;
        // 첫 경계에서 중단 — 실제 도달 위치(클램프된 칸)를 에코한다.
        if (nr != targetRow || nc != targetCol) break;
    }
    return r;
}

} // namespace server
} // namespace jk