#ifndef JKSEMANTICCURSOR_H
#define JKSEMANTICCURSOR_H

#include <utility>
#include <vector>

namespace jk {
namespace server {

// 의미 커서 상태 머신 (스펙 specs/2026-09-22-semantic-cursor §4).
//
// 선언 격자(rows×cols) 위의 커서 {row, col} — 절대/상대/다중 스텝 이동과
// 에코(도달 위치) 산출만 담당하는 순수 로직. 뷰/서버 의존 0
// (TerminalHangulInput 선례 — 로직 클래스 + 뷰 배선 분리 + 2층 프로브).
//
// 규약 (스펙 §3 move):
//  - 절대 이동(MoveTo): 음수 = bad_args, 선언 격자 밖 = bad_grid(에코 없음 —
//    상태 불변).
//  - 상대 이동(MoveRelative): |dr|/|dc| 과대 = bad_args(상태 불변); 결과는
//    격자 경계에 클램프(격자 밖 불가 — cursorOwner: platform 서버 규약).
//  - 다중 스텝(RunSteps): 개수 상한 32, 첫 경계에서 중단하고 실제 도달 위치를
//    에코("끝까지"의 의미를 고정). 음수 델타는 정상 이동(위/왼쪽 — 경계
//    클램프). 정상 경로는 서버가 전 스텝 델타를 사전 검증(|델타| > 1<<20 →
//    bad_args, 무적용)해 넘겨준다 — 클래스 자체 가드는 이중 방어로 유지
//    (도중 과대 델타가 오면 bad_args — 그 앞 스텝까지는 이미 적용된 상태).
//  - Reset(rows, cols): 격자 (재)설정 — 커서는 항상 (0,0) 좌상단으로 리셋
//    (게임 리셋=정의 전이, 재선언 안전 규약).
class JKSemanticCursor {
public:
    // 에코 = 상태 직렬화. error == nullptr 이면 성공, 아니면 에러 코드
    // (bad_args / bad_grid) — row/col은 에러 시 "이동 전" 위치(불변 에코).
    struct Result {
        int row = 0;
        int col = 0;
        const char* error = nullptr;
        bool ok() const { return error == nullptr; }
    };

    JKSemanticCursor() = default;

    // 격자 설정(선언 적용). rows/cols < 1 무시(기존 상태 유지).
    void Reset(int rows, int cols);

    int Rows() const { return rows_; }
    int Cols() const { return cols_; }
    int Row() const { return row_; }
    int Col() const { return col_; }

    Result MoveTo(int row, int col);
    Result MoveRelative(int dr, int dc);
    Result RunSteps(const std::vector<std::pair<int, int>>& steps);

private:
    int rows_ = 0;
    int cols_ = 0;
    int row_ = 0;   // (0,0) = 좌상단 (코디네이터 룰링 — 초기/리셋 위치)
    int col_ = 0;
};

} // namespace server
} // namespace jk

#endif // JKSEMANTICCURSOR_H