// include/text/JKTextParts.h — 벡터 텍스트 렌더 지연 부품 번들 (docs/106 §3)
//
// 왜: "한글 매니저 + 아틀라스 + 리소스 캐시 + SDL 백엔드"를 같은 순서로
// 지연 초기화하는 코드가 셸(툴팁/라벨)·태스크바(미니)·승인 배너·앱들에
// 복제돼 있었고, 초기화 파라미터(셀 메트릭)나 폴백 체인이 한 곳만 어긋나도
// 그 표면의 한글만 조용히 비트맵으로 퇴화한다. 번들 하나로 계약을 고정한다.
//
// 사용:
//   jk::text::FontParts parts;
//   if (parts.Ensure(renderer)) parts.AttachTo(dc);   // 실패 = 비트맵 폴백
#ifndef JK_TEXT_PARTS_H
#define JK_TEXT_PARTS_H

#include <memory>

struct SDL_Renderer;

namespace jk {

class JKDC;               // include/JKDC.h
class JKTextAtlas;        // include/JKTextAtlas.h
class HangulManager;      // include/JKHangulManager.h
class JKResourceCache;    // include/JKResourceCache.h
class JKSDLRenderBackend; // include/JKSDLRenderBackend.h

namespace text {

struct FontParts {
    FontParts();
    ~FontParts();                       // 소멸은 .cpp(불완전 형 완결)
    FontParts(const FontParts&) = delete;
    FontParts& operator=(const FontParts&) = delete;

    std::unique_ptr<HangulManager> font;
    std::unique_ptr<JKTextAtlas> atlas;
    std::unique_ptr<JKResourceCache> cache;
    std::unique_ptr<JKSDLRenderBackend> backend;

    // 1회 지연 초기화. 반환 = 아틀라스 장착 가능 여부(폰트 부재/초기화 실패
    // = false — 호출자는 비트맵 경로로 계속).
    bool Ensure(SDL_Renderer* renderer);

    // dc에 부품 장착(아틀라스가 살아 있을 때만). dc는 호출자 수명 소유.
    void AttachTo(JKDC& dc) const;

    bool Ready() const;

    // 아틀라스/캐시의 큐된 업로드를 지금 반영. **텍스처 타겟에 글리프를 그린
    // 직후 반드시 호출**(업로드 전에 타겟을 읽으면 빈 글리프가 나온다 —
    // 태스크바 미니 텍스처 실측 원존 "GetImage가 업로드 플러시 이전에 null").
    void FlushUploads();

    // 텍스처 파괴 전에 캐시 업로드/이미지 정리 — 소유 백엔드가 사라지기 전.
    void Reset();
};

}  // namespace text
}  // namespace jk

#endif  // JK_TEXT_PARTS_H
