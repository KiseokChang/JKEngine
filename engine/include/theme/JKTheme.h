// include/theme/JKTheme.h — P2 테마 단계 1 유일 진실원 (스펙 §1)
// 값 근거: Win11 모던 다크 + 액센트 #0078D4. 태스크바 필드는 기존 값 유지
// (값 불변 토큰화 — docs/superpowers/plans/2026-09-13-theme-system-phase1.md).
#ifndef JK_THEME_H
#define JK_THEME_H

#include <SDL.h>

#include <string>

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
    // 위젯 라이브러리 — JKControl 계열 (단계 2)
    SDL_Color widgetFace;       // 버튼/스크롤/메뉴/콤보버튼/스태틱/체크박스 면
    SDL_Color widgetText;       // 위젯 텍스트/글리프/캐럿
    SDL_Color fieldBg;          // 입력 필드/리스트/콤보 배경
    SDL_Color selectionBg;      // 선택/하이라이트 배경 (액센트)
    SDL_Color selectionText;    // 선택/하이라이트 텍스트
    SDL_Color bevelLight;       // 3D 가장자리 (밝음)
    SDL_Color bevelDark;        // 3D 가장자리 (어두움)
    SDL_Color bevelMid;         // 썸 그림자/중간 톤
    SDL_Color scrollbarTrack;   // 스크롤바 트랙
    SDL_Color scrollbarThumb;   // 스크롤바 썸 면
    SDL_Color focusRing;        // 포커스 링
    SDL_Color imeCompositionBg; // IME 조합 배경 (알파 64 보존 필수)
    SDL_Color imeCaret;         // IME 조합 캐럿/에러 (값 유지 토큰)
    // 표면 — 클라이언트 영역/앱 클리어 (단계 2)
    SDL_Color windowClientBg;   // JKWindow 클라 영역 배경
    SDL_Color appClearBg;       // JKClientApplication 표면 클리어
    // 터미널 — 기본 전경/배경 (단계 3). terminal.json의 themeBg/Fg 키가
    // 없을 때만 소비(시딩) — 사용자 지정 키가 있으면 그 값이 우선한다.
    SDL_Color terminalBg;       // 터미널 배경 (kDefault/kClassic = 구값 #0C0C0C 불변)
    SDL_Color terminalFg;       // 터미널 전경 (kDefault/kClassic = 구값 #CCCCCC 불변)
};

inline constexpr JKTheme kDefault = {
    .chromeTitleBg         = {32, 32, 32, 255},     // #202020 (구: 실버 192,192,192 계열)
    .chromeTitleText       = {240, 240, 240, 255},
    .chromeBorder          = {68, 68, 68, 255},     // #444444
    .chromeActiveBorder    = {0, 120, 212, 255},    // #0078D4
    .chromeButtonFace      = {38, 38, 38, 255},
    .chromeButtonGlyph     = {240, 240, 240, 255},
    .chromeCloseHover      = {196, 43, 28, 255},    // Win11 close red
    .desktopBgFallback     = {28, 28, 30, 255},     // #1C1C1E (구: 96,96,96)
    .launcherCellFace      = {44, 44, 46, 255},     // (구: 100,100,100)
    .launcherCellOutline   = {255, 255, 255, 255},  // 불변
    .launcherCellPlaceholder = {80, 80, 84, 255},   // 제너릭 폴백 (구: 128,128,128)
    .taskbarBg             = {24, 26, 32, 255},
    .taskbarFaceNormal     = {56, 58, 68, 255},
    .taskbarFaceActive     = {92, 98, 122, 255},
    .taskbarFaceMinimized  = {38, 40, 46, 255},
    .taskbarTextNormal     = {224, 224, 224, 255},
    .taskbarTextActive     = {255, 255, 255, 255},
    .taskbarTextMinimized  = {120, 120, 120, 255},
    .taskbarBorderActive   = {255, 255, 255, 255},
    .taskbarBorderInactive = {70, 72, 84, 255},
    .widgetFace       = {43, 43, 43, 255},
    .widgetText       = {240, 240, 240, 255},
    .fieldBg          = {26, 26, 26, 255},
    .selectionBg      = {0, 120, 212, 255},
    .selectionText    = {255, 255, 255, 255},
    .bevelLight       = {70, 70, 70, 255},
    .bevelDark        = {16, 16, 16, 255},
    .bevelMid         = {40, 40, 40, 255},
    .scrollbarTrack   = {56, 56, 56, 255},
    .scrollbarThumb   = {86, 86, 86, 255},
    .focusRing        = {0, 120, 212, 255},
    .imeCompositionBg = {0, 120, 212, 64},   // 알파 64 보존
    .imeCaret         = {255, 0, 0, 255},    // 값 유지
    .windowClientBg   = {32, 32, 32, 255},
    .appClearBg       = {32, 32, 32, 255},
    .terminalBg       = {12, 12, 12, 255},       // #0C0C0C (구값 불변)
    .terminalFg       = {204, 204, 204, 255},    // #CCCCCC (구값 불변)
};

// --- 프리셋 (단계 2 스펙 §1b) ---
// kLight: Win11 라이트. kClassic: 단계 1 이전 값 전부 (픽셀 회귀 도구).
// taskbar는 단계 1에서 값 불변 토큰화였으므로 kClassic의 taskbar = kDefault와 동일.
inline constexpr JKTheme kLight = {
    .chromeTitleBg         = {243, 243, 243, 255},
    .chromeTitleText       = {0, 0, 0, 255},
    .chromeBorder          = {204, 204, 204, 255},
    .chromeActiveBorder    = {0, 120, 212, 255},
    .chromeButtonFace      = {229, 229, 229, 255},
    .chromeButtonGlyph     = {0, 0, 0, 255},
    .chromeCloseHover      = {196, 43, 28, 255},
    .desktopBgFallback     = {243, 243, 243, 255},
    .launcherCellFace      = {249, 249, 249, 255},
    .launcherCellOutline   = {200, 200, 200, 255},
    .launcherCellPlaceholder = {204, 204, 208, 255},
    .taskbarBg             = {243, 243, 243, 255},
    .taskbarFaceNormal     = {229, 229, 229, 255},
    .taskbarFaceActive     = {204, 204, 204, 255},
    .taskbarFaceMinimized  = {236, 236, 236, 255},
    .taskbarTextNormal     = {0, 0, 0, 255},
    .taskbarTextActive     = {0, 0, 0, 255},
    .taskbarTextMinimized  = {96, 96, 96, 255},
    .taskbarBorderActive   = {0, 0, 0, 255},
    .taskbarBorderInactive = {200, 200, 200, 255},
    .widgetFace       = {240, 240, 240, 255},
    .widgetText       = {0, 0, 0, 255},
    .fieldBg          = {255, 255, 255, 255},
    .selectionBg      = {0, 120, 212, 255},
    .selectionText    = {255, 255, 255, 255},
    .bevelLight       = {255, 255, 255, 255},
    .bevelDark        = {160, 160, 160, 255},
    .bevelMid         = {160, 160, 160, 255},
    .scrollbarTrack   = {229, 229, 229, 255},
    .scrollbarThumb   = {205, 205, 205, 255},
    .focusRing        = {0, 120, 212, 255},
    .imeCompositionBg = {0, 120, 212, 64},
    .imeCaret         = {255, 0, 0, 255},
    .windowClientBg   = {249, 249, 249, 255},
    .appClearBg       = {240, 240, 240, 255},
    .terminalBg       = {250, 250, 250, 255},
    .terminalFg       = {31, 31, 31, 255},
};

inline constexpr JKTheme kClassic = {
    .chromeTitleBg         = {0, 0, 128, 255},      // 구 네이비
    .chromeTitleText       = {255, 255, 255, 255},
    .chromeBorder          = {192, 192, 192, 255},  // 구 실버
    .chromeActiveBorder    = {0, 0, 128, 255},      // 단계 1 전에는 미존재 — 구 테두리색으로 근사
    .chromeButtonFace      = {192, 192, 192, 255},
    .chromeButtonGlyph     = {255, 255, 255, 255},
    .chromeCloseHover      = {196, 43, 28, 255},    // 호버 페인트 미구현이라 무영향
    .desktopBgFallback     = {96, 96, 96, 255},
    .launcherCellFace      = {100, 100, 100, 255},
    .launcherCellOutline   = {255, 255, 255, 255},
    .launcherCellPlaceholder = {128, 128, 128, 255},
    .taskbarBg             = {24, 26, 32, 255},     // 이하 taskbar 9필드 = kDefault와 동일 (값 불변 토큰화)
    .taskbarFaceNormal     = {56, 58, 68, 255},
    .taskbarFaceActive     = {92, 98, 122, 255},
    .taskbarFaceMinimized  = {38, 40, 46, 255},
    .taskbarTextNormal     = {224, 224, 224, 255},
    .taskbarTextActive     = {255, 255, 255, 255},
    .taskbarTextMinimized  = {120, 120, 120, 255},
    .taskbarBorderActive   = {255, 255, 255, 255},
    .taskbarBorderInactive = {70, 72, 84, 255},
    .widgetFace       = {192, 192, 192, 255},  // 구 실버 면 (스태틱 240과의 차이는 Global Constraints 마지막 항목)
    .widgetText       = {0, 0, 0, 255},
    .fieldBg          = {255, 255, 255, 255},
    .selectionBg      = {0, 0, 128, 255},      // 구 네이비 선택
    .selectionText    = {255, 255, 255, 255},
    .bevelLight       = {255, 255, 255, 255},
    .bevelDark        = {0, 0, 0, 255},
    .bevelMid         = {128, 128, 128, 255},
    .scrollbarTrack   = {220, 220, 220, 255},
    .scrollbarThumb   = {255, 255, 255, 255},
    .focusRing        = {0, 0, 255, 255},
    .imeCompositionBg = {0, 0, 255, 64},       // 구 파랑 조합 — 알파 64
    .imeCaret         = {255, 0, 0, 255},
    .windowClientBg   = {240, 240, 240, 255},
    .appClearBg       = {192, 192, 192, 255},
    .terminalBg       = {12, 12, 12, 255},       // 구값 불변 (kDefault와 동일)
    .terminalFg       = {204, 204, 204, 255},    // 구값 불변 (kDefault와 동일)
};

// --- 스위칭 봉합 ---
// 기동 시 1회 setTheme가 정석 — 렌더 중 스왑은 P3(핫스왑)에서 재검토.
inline const JKTheme* activeTheme_ = &kDefault;
inline JKTheme const& current() { return *activeTheme_; }
inline void setTheme(const JKTheme* t) { if (t) activeTheme_ = t; }

// theme.json {"preset": "dark"|"light"|"classic"} 1키 로더. 파일 없음/깨짐
// → false, 상태 불변 (기본값=다크가 곧 정답).
bool loadPresetFromFile(const std::string& path);
std::string DefaultThemePath();  // exe 옆 theme.json (terminal.json 패턴 준용)
// P3 핫스왑: theme.json mtime 폴링 (호출자가 주기 소유, 500ms). 바뀜 감지 시
// 로더 재실행하고 true. theme_set 도구의 기록용 — 셀프 폴링 오탐 방지 포함.
bool PollPresetFile();
void WriteThemePresetFile(const std::string& preset);  // {"preset":"<p>"} 기록

} } // namespace jk::theme

#endif // JK_THEME_H
