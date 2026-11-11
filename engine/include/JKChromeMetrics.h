// include/JKChromeMetrics.h — 창 크롬 기하 단일 진실원 (docs/106 §2-6, docs/109)
//
// 종전: 테두리 두께·타이틀 높이가 세 곳(클라 페인트 JKWindow.cpp, 서버 페인트,
// 히트테스트)에 각자 상수로 있었고 주석으로만 동기됐다. 한 곳이 바뀌면
// 히트 테스트가 조용히 어긋난다(창을 잡을 수 없는 자리/그림과 다른 판정).
//
// 규약:
//  - kBorder = 창 가장자리 두께(논리 px, 불변).
//  - TitleHeight() = 셀 메트릭 연동(플랫폼 베이크 상수 아님 — JKWindow 주석
//    원존: Windows s=1.0 = 24, posix s=1.5 = 32).
//  - ClientRect(win) = JKWindow::GetClientRect 계약과 등호(셀프테스트 단정).
#ifndef JK_CHROME_METRICS_H
#define JK_CHROME_METRICS_H

#include <JKTypes.h>
#include <JKTextAtlas.h>  // jk::text::ChromeTitleBarHeight (셀 메트릭 연동)

namespace jk {
namespace chrome {

inline constexpr int kBorder = 2;

// 타이틀 바 높이 — 셀 메트릭 연동(JKWindow.cpp와 같은 산식: s=1.0에서 24).
inline int TitleHeight() { return jk::text::ChromeTitleBarHeight(); }

// 창 rect(로컬) → 클라 rect 계약: {kBorder, TitleHeight(), w-2*kBorder,
// h-kBorder-TitleHeight()}.
inline JKRect ClientRect(const JKRect& win) {
    JKRect c{ win.x + kBorder, win.y + TitleHeight(),
              win.w - kBorder * 2, win.h - kBorder - TitleHeight() };
    // JKWindow::OnRectChanged와 같은 하한 클램프(작은 창에서 음수 크기 금지).
    if (c.w < 0) c.w = 0;
    if (c.h < 0) c.h = 0;
    return c;
}

}  // namespace chrome
}  // namespace jk

#endif  // JK_CHROME_METRICS_H
