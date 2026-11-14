// include/text/JKImGuiFont.h — 앱 ImGui 한글 폰트 등록 정본 (docs/106 §2-5)
//
// 왜: "ResolveDesktopFontPath → AddFontFromFileTTF(16px, Korean ranges) →
// 성공 시 koreanFont_=true" 5줄이 앱 TU 14곳에 복사돼 있었다. 한 곳만
// GlyphRanges/크기를 빠뜨리면 그 앱에서만 한글이 네모로 나오는 식의
// 조용한 드리프트가 생긴다. 계약(폰트 부재 = 커스텀 폰트 스킵, 하드코딩
// malgun 금지 — 리눅스 AddFont assert 원존)을 한 곳에 둔다.
#ifndef JK_IMGUI_FONT_H
#define JK_IMGUI_FONT_H

#include <imgui.h>

#include <JKTextAtlas.h>   // jk::text::ResolveDesktopFontPath

#include <string>

namespace jk {
namespace text {

// ImGui 기본 폰트로 데스크톱 한글 폰트를 등록한다.
// 반환 = 등록 성공(폰트 부재/로드 실패 = false — 호출자는 기본 글리프로 계속).
inline bool AddImGuiKoreanFont(ImFontAtlas* atlas, float sizePx = 16.0f) {
    if (!atlas) return false;
    const std::string fontPath = ResolveDesktopFontPath();
    if (fontPath.empty()) return false;
    return atlas->AddFontFromFileTTF(fontPath.c_str(), sizePx, nullptr,
                                     atlas->GetGlyphRangesKorean()) != nullptr;
}

// 커스텀 글리프 범위(예: 브라우저의 ★☆ 북마크 토글) — 범위 계약은 호출자 소관,
// 폰트 해석/실패 처리만 공유한다.
inline bool AddImGuiKoreanFont(ImFontAtlas* atlas, float sizePx,
                               const ImWchar* ranges) {
    if (!atlas || !ranges) return false;
    const std::string fontPath = ResolveDesktopFontPath();
    if (fontPath.empty()) return false;
    return atlas->AddFontFromFileTTF(fontPath.c_str(), sizePx, nullptr, ranges) !=
           nullptr;
}

}  // namespace text
}  // namespace jk

#endif  // JK_IMGUI_FONT_H
