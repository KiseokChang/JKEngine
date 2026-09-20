// jktext_view_probe — 데스크탑 벡터 폰트(docs/63) 뷰 레벨 배선 잠금
// (레슨 30: 로직 클래스가 생기면 호출부 배선을 뷰 레벨 프로브로 잠근다).
// 실 SDL 백엔드(software renderer)+ReadPixels로 "가나ABC"를 찍어
// ①잉크 ②안티에일리어싱(중간 농도 픽셀) ③8/16px 전진을 픽셀로 단정한다.
// 비트맵 폰트는 0/255 이진 픽셀뿐 — 중간 농도 존재 자체가 벡터 경로의 증거.
// TDD: 아틀라스 미장착(=비트맵 경로)이면 AA 체크가 FAIL — 배선의 부정 증거.
#include <JKDC.h>
#include <JKTextAtlas.h>
#include <JKResourceCache.h>
#include <JKSDLRenderBackend.h>
#include <JKHangulUtil.h>
#include <SDL.h>
#ifdef main
#undef main   // SDL_main 치환 방지(docs/61 레슨 27)
#endif
#include <cstdint>
#include <cstdio>
#include <string>

using namespace jk;

static int main2() {
    int rc = 1;
    SDL_Window* w = nullptr;
    SDL_Renderer* r = nullptr;
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::printf("SKIP: SDL_Init failed: %s\n", SDL_GetError());
        return 77;
    }
    w = SDL_CreateWindow("jktext_view_probe", SDL_WINDOWPOS_CENTERED,
                         SDL_WINDOWPOS_CENTERED, 320, 64, SDL_WINDOW_HIDDEN);
    if (!w) {
        std::printf("SKIP: SDL_CreateWindow failed: %s\n", SDL_GetError());
        rc = 77;
        goto out;
    }
    r = SDL_CreateRenderer(w, -1, SDL_RENDERER_SOFTWARE);
    if (!r) {
        std::printf("SKIP: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        rc = 77;
        goto out;
    }

    {
        JKSDLRenderBackend be(r);
        JKDC dc(&be);
        JKResourceCache cache(&be);
        JKTextAtlas atlas;
        const std::string fontPath = text::ResolveDesktopFontPath();
        if (fontPath.empty() || !atlas.Init(fontPath, 8, 16, 16)) {
            std::printf("SKIP: vector font init failed (%s)\n", fontPath.c_str());
            rc = 77;
            goto out;
        }
        dc.SetTextAtlas(&atlas, &cache);

        // 흰 바탕 + 검은 글자. "가나" = 완성형 16px 전진, "ABC" = 8px 전진.
        be.SetDrawColor(255, 255, 255, 255);
        be.FillRect(JKRect{ 0, 0, 320, 64 });
        dc.SetTextColor(0, 0, 0);
        dc.TextOut(JKPoint{ 8, 24 }, Utf8ToKssm("가나ABC").c_str());
        cache.FlushUploads(&be);   // JKResourceCache가 업로드를 소유한다(레슨: be가 아님)
        be.Present();

        int pw = 0, ph = 0;
        SDL_GetRendererOutputSize(r, &pw, &ph);
        // ReadPixels 포맷은 표면 포맷과 동일해야 한다 — RGBA8888 요청을 RGBA32
        // 표면에 쓰면 SDL이 무음으로 엉터리를 쓴다(진단 1시간: 픽셀은 정상,
        // 카운트만 0 — jkedit_render_probe의 ARGB8888+ARGB8888 관례와 동일).
        SDL_Surface* srf = SDL_CreateRGBSurfaceWithFormat(0, pw, ph, 32,
                                                          SDL_PIXELFORMAT_RGBA32);
        if (!srf || SDL_RenderReadPixels(r, nullptr, SDL_PIXELFORMAT_RGBA32,
                                         srf->pixels, srf->pitch) != 0) {
            std::printf("FAIL: ReadPixels failed: %s\n", SDL_GetError());
            if (srf) SDL_FreeSurface(srf);
            goto out;
        }

        int ink = 0, aa = 0;
        int firstInkX = -1, lastInkX = -1;
        for (int y = 0; y < ph; ++y) {
            for (int x = 0; x < pw; ++x) {
                const uint8_t* p = (const uint8_t*)(srf->pixels) + y * srf->pitch + x * 4;
                const uint8_t lum = p[0];   // 흑백 텍스트 — R 채널
                if (lum < 250) {
                    ++ink;
                    // 행 우선 스캔이라 최초/최후 발견이 아닌 최소/최대 x를 추적한다.
                    if (firstInkX < 0 || x < firstInkX) firstInkX = x;
                    if (x > lastInkX) lastInkX = x;
                }
                if (lum > 20 && lum < 235) ++aa;   // 중간 농도 = 안티에일리어싱
            }
        }
        int fail = 0;
        if (ink < 50) { ++fail; std::printf("FAIL: ink pixels %d too few\n", ink); }
        if (aa < 20) { ++fail; std::printf("FAIL: AA pixels %d — bitmap path suspected\n", aa); }
        // 전진 검증: 가(16)+나(16)+A(8)+B(8) = 48px 폭 → 마지막 잉크 x가 8+48=56 근처.
        if (firstInkX >= 0 && (lastInkX < 40 || lastInkX > 75)) {
            ++fail;
            std::printf("FAIL: ink span [%d..%d] — advance not 8/16px\n",
                        firstInkX, lastInkX);
        }
        std::printf("%s: ink=%d aa=%d span=[%d..%d]\n", fail == 0 ? "PASS" : "FAIL",
                    ink, aa, firstInkX, lastInkX);
        SDL_FreeSurface(srf);
        rc = fail == 0 ? 0 : 1;
    }

out:
    if (r) SDL_DestroyRenderer(r);
    if (w) SDL_DestroyWindow(w);
    SDL_Quit();
    return rc;
}

int main() {
    // 위 계층에서 goto+스코프 객체가 섞이면 소멸 순서가 어수선하다 — 본체 분리.
    return main2();
}