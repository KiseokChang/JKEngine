// include/theme/JKThemeImGui.h — P2 단계 3: JKTheme→ImGui 팔레트 봉합 (스펙 §1a)
//
// 봉합(seam)의 의미: Dear ImGui 앱은 이 헤더 하나로 JKTheme 단일 진실원을 따른다.
// - 팔레트만 봉합한다. 지오메트리(라운딩/간격/프레임 패딩)는 ImGui 고유 스타일을
//   그대로 둔다 — PushStyleVar 계열은 이 헤더가 절대 건드리지 않는다.
// - 모든 항목은 JKTheme 토큰에서 온다. hover/active 등 상태 변주는 같은 토큰의
//   Lighten() 선형 변형이나 bevel/focusRing 토큰으로 만든다 — 새 리터럴 색 없음.
//   유일한 예외는 ModalWindowDimBg (디밍은 테마 토큰이 아니라 반투명 검정 기능색).
// - 헤더 온리 — CMake 변경 없음. 앱은 ImGui::CreateContext() 직후
//   jk::theme::ApplyImGuiTheme()를 1회 호출하면 된다 (단계 3 T3 시딩).
// - implot 계열 enum(플롯 히스토그램/축 등)은 implot 자체 스타일 소관 — 여기서
//   설정하지 않는다 (범위 밖).
#ifndef JK_THEME_IMGUI_H
#define JK_THEME_IMGUI_H

#include "theme/JKTheme.h"
#include <imgui.h>

namespace jk { namespace theme {

// SDL_Color → ImVec4 변환. alphaScale로 상태 변주(비활성/흐림)의 알파만 조정.
inline ImVec4 ToImVec4(const SDL_Color& c, float alphaScale = 1.0f) {
    return ImVec4(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f,
                  (c.a / 255.0f) * alphaScale);
}

// 선형 밝기 헬퍼 — 채널별로 amount*255 만큼 더하고 0..255로 클램프.
// amount>0 밝게, amount<0 어둡게. (hover ≈ +0.08~+0.12, ±20% 상한선)
inline SDL_Color Lighten(const SDL_Color& c, float amount) {
    auto ch = [amount](unsigned char v) -> unsigned char {
        float f = static_cast<float>(v) + amount * 255.0f;
        if (f < 0.0f)   f = 0.0f;
        if (f > 255.0f) f = 255.0f;
        return static_cast<unsigned char>(f + 0.5f);
    };
    return SDL_Color{ch(c.r), ch(c.g), ch(c.b), c.a};
}

// 현재 테마(current())를 ImGuiStyle 팔레트에 반영. 지오메트리 불변.
inline void ApplyToImGui(ImGuiStyle& style) {
    const JKTheme& t = current();
    // 표면 — windowClientBg로 수렴 (단계 2 토큰 재사용)
    style.Colors[ImGuiCol_WindowBg]         = ToImVec4(t.windowClientBg);
    style.Colors[ImGuiCol_ChildBg]          = ToImVec4(t.windowClientBg);
    style.Colors[ImGuiCol_PopupBg]          = ToImVec4(t.windowClientBg);
    // 창 크롬 — 타이틀 계열은 chromeTitleBg 단일 토큰
    style.Colors[ImGuiCol_MenuBarBg]        = ToImVec4(t.chromeTitleBg);
    style.Colors[ImGuiCol_TitleBg]          = ToImVec4(t.chromeTitleBg);
    style.Colors[ImGuiCol_TitleBgActive]    = ToImVec4(t.chromeTitleBg);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ToImVec4(t.chromeTitleBg);
    // 텍스트/경계
    style.Colors[ImGuiCol_Text]             = ToImVec4(t.widgetText);
    style.Colors[ImGuiCol_TextDisabled]     = ToImVec4(t.widgetText, 0.5f);
    style.Colors[ImGuiCol_Border]           = ToImVec4(t.chromeBorder);
    style.Colors[ImGuiCol_BorderShadow]     = ToImVec4(t.bevelDark);
    style.Colors[ImGuiCol_Separator]        = ToImVec4(t.bevelLight);
    style.Colors[ImGuiCol_SeparatorHovered] = ToImVec4(t.bevelMid);
    style.Colors[ImGuiCol_SeparatorActive]  = ToImVec4(t.focusRing);
    // 입력 필드 — fieldBg + 밝기 변주
    style.Colors[ImGuiCol_FrameBg]          = ToImVec4(t.fieldBg);
    style.Colors[ImGuiCol_FrameBgHovered]   = ToImVec4(Lighten(t.fieldBg, 0.08f));
    style.Colors[ImGuiCol_FrameBgActive]    = ToImVec4(Lighten(t.fieldBg, 0.16f));
    // 버튼 — widgetFace + 밝기 변주 / 눌림은 bevelMid
    style.Colors[ImGuiCol_Button]           = ToImVec4(t.widgetFace);
    style.Colors[ImGuiCol_ButtonHovered]    = ToImVec4(Lighten(t.widgetFace, 0.08f));
    style.Colors[ImGuiCol_ButtonActive]     = ToImVec4(t.bevelMid);
    // 헤더(트리/콜랩싱/셀렉터블) — selectionBg + 밝기 변주
    style.Colors[ImGuiCol_Header]           = ToImVec4(t.selectionBg);
    style.Colors[ImGuiCol_HeaderHovered]    = ToImVec4(Lighten(t.selectionBg, 0.12f));
    style.Colors[ImGuiCol_HeaderActive]     = ToImVec4(t.bevelMid);
    // 체크마크/라디오 글리프 — 위젯 텍스트 토큰
    style.Colors[ImGuiCol_CheckMark]        = ToImVec4(t.widgetText);
    // 스크롤바
    style.Colors[ImGuiCol_ScrollbarBg]      = ToImVec4(t.scrollbarTrack);
    style.Colors[ImGuiCol_ScrollbarGrab]    = ToImVec4(t.scrollbarThumb);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ToImVec4(Lighten(t.scrollbarThumb, 0.10f));
    style.Colors[ImGuiCol_ScrollbarGrabActive]  = ToImVec4(t.focusRing);
    // 슬라이더 그랩 — selectionBg 계열 (ImGui에 SliderGrabHovered는 미존재)
    style.Colors[ImGuiCol_SliderGrab]       = ToImVec4(t.selectionBg);
    style.Colors[ImGuiCol_SliderGrabActive] = ToImVec4(t.focusRing);
    // 선택/네비 하이라이트 — selectionBg 알파 변주
    style.Colors[ImGuiCol_TextSelectedBg]   = ToImVec4(t.selectionBg, 0.6f);
    // 1.92에서 정식 명칭은 NavCursor (NavHighlight는 폐기 예정 별칭) — 동일 토큰
    style.Colors[ImGuiCol_NavCursor]        = ToImVec4(t.selectionBg, 0.5f);
    // 리사이즈 그립 — bevelLight/bevelMid/focusRing 패턴
    style.Colors[ImGuiCol_ResizeGrip]       = ToImVec4(t.bevelLight);
    style.Colors[ImGuiCol_ResizeGripHovered]= ToImVec4(t.bevelMid);
    style.Colors[ImGuiCol_ResizeGripActive] = ToImVec4(t.focusRing);
    // 드래그앤드롭 대상 윤곽 — 포커스 링 토큰
    style.Colors[ImGuiCol_DragDropTarget]   = ToImVec4(t.focusRing);
    // 모달 뒤 디밍 — 반투명 검정 기능색 (테마 토큰 아님; 유일한 리터럴)
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 96.0f / 255.0f);
    // 테이블/탭 — 게이트 실측에서 기본값 잔존 확인 (docs/47) 표면/베벨 토큰으로 확장
    // 테이블 — 헤더는 위젯 면, 테두리는 bevel 쌍, 행 배경은 클라 영역 토큰
    style.Colors[ImGuiCol_TableHeaderBg]    = ToImVec4(t.widgetFace);
    style.Colors[ImGuiCol_TableBorderStrong]= ToImVec4(t.bevelDark);
    style.Colors[ImGuiCol_TableBorderLight] = ToImVec4(t.bevelLight);
    style.Colors[ImGuiCol_TableRowBg]       = ToImVec4(t.windowClientBg);
    style.Colors[ImGuiCol_TableRowBgAlt]    = ToImVec4(Lighten(t.windowClientBg, 0.05f)); // 얼룩말 줄 — 표면 살짝 밝게
    // 탭 — 비선택은 위젯 면 변주, 선택은 selectionBg (kClassic 회귀: selectionBg가 액센트 — 의도)
    style.Colors[ImGuiCol_Tab]              = ToImVec4(t.widgetFace);
    style.Colors[ImGuiCol_TabHovered]       = ToImVec4(Lighten(t.widgetFace, 0.12f));
    style.Colors[ImGuiCol_TabActive]        = ToImVec4(Lighten(t.widgetFace, 0.08f)); // 1.90.9+ TabSelected 별칭 슬롯 — 아래 TabSelected가 최종값
    style.Colors[ImGuiCol_TabSelected]      = ToImVec4(t.selectionBg);
    style.Colors[ImGuiCol_TabSelectedOverline] = ToImVec4(t.selectionBg);
    style.Colors[ImGuiCol_TabDimmed]        = ToImVec4(Lighten(t.windowClientBg, 0.04f));
    style.Colors[ImGuiCol_TabDimmedSelected]= ToImVec4(Lighten(t.windowClientBg, 0.10f));
}

// 편의 래퍼 — CreateContext() 직후 1회 호출용.
inline void ApplyImGuiTheme() { ApplyToImGui(ImGui::GetStyle()); }

} } // namespace jk::theme

#endif // JK_THEME_IMGUI_H
