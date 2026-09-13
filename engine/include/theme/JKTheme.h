// include/theme/JKTheme.h — P2 테마 단계 1 유일 진실원 (스펙 §1)
// 값 근거: Win11 모던 다크 + 액센트 #0078D4. 태스크바 필드는 기존 값 유지
// (값 불변 토큰화 — docs/superpowers/plans/2026-09-13-theme-system-phase1.md).
#ifndef JK_THEME_H
#define JK_THEME_H

#include <SDL.h>

namespace jk { namespace theme {

struct JKTheme {
    // 창 크롬 — JKCompositor(컴포지터)와 JKWindow(단일 프로세스) 공유
    SDL_Color chromeTitleBg;        // 타이틀바 배경
    SDL_Color chromeTitleText;      // 타이틀 텍스트
    SDL_Color chromeBorder;         // 창 테두리/버튼 윤곽
    SDL_Color chromeActiveBorder;   // 활성 창 액센트 경계 (예약 — 현재 미사용, 값만 확정)
    SDL_Color chromeButtonFace;     // 닫기/최대화 버튼 면
    SDL_Color chromeButtonGlyph;    // 버튼 글리프(X/사각형)
    SDL_Color chromeCloseHover;     // 닫기 버튼 호버 (Win11 빨강)
    // 데스크탑/런처 — JKDesktopShell
    SDL_Color desktopBgFallback;    // 배경 애셔 부재 시 클리어색
    SDL_Color launcherCellFace;     // 런처 셀 면 (폴백)
    SDL_Color launcherCellOutline;  // 런처 셀 윤곽
    SDL_Color launcherCellPlaceholder; // 앱별 플레이스홀더 폴백(제너릭)
    // 태스크바 — ClientTaskbarApp (값 = 기존 리터럴 그대로)
    SDL_Color taskbarBg;
    SDL_Color taskbarFaceNormal, taskbarFaceActive, taskbarFaceMinimized;
    SDL_Color taskbarTextNormal, taskbarTextActive, taskbarTextMinimized;
    SDL_Color taskbarBorderActive, taskbarBorderInactive;
};

inline constexpr JKTheme kDefault = {
    /*chromeTitleBg*/        {32, 32, 32, 255},     // #202020 (구: 실버 192,192,192 계열)
    /*chromeTitleText*/      {240, 240, 240, 255},
    /*chromeBorder*/         {68, 68, 68, 255},     // #444444
    /*chromeActiveBorder*/   {0, 120, 212, 255},    // #0078D4
    /*chromeButtonFace*/     {38, 38, 38, 255},
    /*chromeButtonGlyph*/    {240, 240, 240, 255},
    /*chromeCloseHover*/     {196, 43, 28, 255},    // Win11 close red
    /*desktopBgFallback*/    {28, 28, 30, 255},     // #1C1C1E (구: 96,96,96)
    /*launcherCellFace*/     {44, 44, 46, 255},     // (구: 100,100,100)
    /*launcherCellOutline*/  {255, 255, 255, 255},  // 불변
    /*launcherCellPlaceholder*/ {80, 80, 84, 255},  // 제너릭 폴백 (구: 128,128,128)
    /*taskbarBg*/            {24, 26, 32, 255},
    /*taskbarFaceNormal*/    {56, 58, 68, 255},
    /*taskbarFaceActive*/    {92, 98, 122, 255},
    /*taskbarFaceMinimized*/ {38, 40, 46, 255},
    /*taskbarTextNormal*/    {224, 224, 224, 255},
    /*taskbarTextActive*/    {255, 255, 255, 255},
    /*taskbarTextMinimized*/ {120, 120, 120, 255},
    /*taskbarBorderActive*/  {255, 255, 255, 255},
    /*taskbarBorderInactive*/{70, 72, 84, 255},
};

} } // namespace jk::theme

#endif // JK_THEME_H