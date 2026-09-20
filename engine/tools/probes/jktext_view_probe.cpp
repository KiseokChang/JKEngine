// jktext_view_probe — 데스크탑 벡터 폰트(docs/63) 뷰 레벨 배선 잠금
// (레슨 30: 로직 클래스가 생기면 호출부 배선을 뷰 레벨 프로브로 잠근다).
// 실 SDL 백엔드(software renderer)+ReadPixels로 "가나ABC"를 찍어
// ①잉크 ②안티에일리어싱(중간 농도 픽셀) ③8/16px 전진을 픽셀로 단정한다.
// 비트맵 폰트는 0/255 이진 픽셀뿐 — 중간 농도 존재 자체가 벡터 경로의 증거.
// TDD: 아틀라스 미장착(=비트맵 경로)이면 AA 체크가 FAIL — 배선의 부정 증거.
// 2페이즈(텍스트 3단계): 페이즈 2는 아틀라스를 **탈장착**하고 같은 문장을 다시
// 찍어 aa==0(비트맵 계단 — 기존 수동 음성 통제의 자동화)+ink>0(비트맵 글리프는
// 여전히 그려진다)을 단정한다. 페이즈 1 단정은 전부 유지.
// 글리프 span 단정: 글리프 하나씩 따로 찍어 잉크 사각 폭이 영문 ≤ engW(8),
// 한글 ≤ hanW(16) — 셀 전진 폭을 넘는 글리프(이웃 셀 침범)를 픽셀로 봉쇄.
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

namespace {

struct FrameStat {
    int ink = 0, aa = 0;
    int minX = -1, maxX = -1, minY = -1, maxY = -1;
    int width() const { return minX < 0 ? 0 : maxX - minX + 1; }
};

// 흰 바탕에 text를 (8,24)에 찍고 ReadPixels로 잉크/AA/사각을 채운다.
// ReadPixels 포맷은 표면 포맷과 동일해야 한다 — RGBA8888 요청을 RGBA32
// 표면에 쓰면 SDL이 무음으로 엉터리를 쓴다(진단 1시간: 픽셀은 정상,
// 카운트만 0 — jkedit_render_probe의 ARGB8888+ARGB8888 관례와 동일).
static bool CaptureFrame(SDL_Renderer* r, JKResourceCache& cache,
                         JKSDLRenderBackend& be, JKDC& dc, const char* text,
                         FrameStat& out) {
    be.SetDrawColor(255, 255, 255, 255);
    be.FillRect(JKRect{ 0, 0, 320, 64 });
    dc.SetTextColor(0, 0, 0);
    dc.TextOut(JKPoint{ 8, 24 }, text);
    cache.FlushUploads(&be);   // JKResourceCache가 업로드를 소유한다(레슨: be가 아님)
    be.Present();

    int pw = 0, ph = 0;
    SDL_GetRendererOutputSize(r, &pw, &ph);
    SDL_Surface* srf = SDL_CreateRGBSurfaceWithFormat(0, pw, ph, 32,
                                                      SDL_PIXELFORMAT_RGBA32);
    if (!srf || SDL_RenderReadPixels(r, nullptr, SDL_PIXELFORMAT_RGBA32,
                                     srf->pixels, srf->pitch) != 0) {
        std::printf("FAIL: ReadPixels failed: %s\n", SDL_GetError());
        if (srf) SDL_FreeSurface(srf);
        return false;
    }
    out = FrameStat{};
    for (int y = 0; y < ph; ++y) {
        for (int x = 0; x < pw; ++x) {
            const uint8_t* p = (const uint8_t*)(srf->pixels) + y * srf->pitch + x * 4;
            const uint8_t lum = p[0];   // 흑백 텍스트 — R 채널
            if (lum < 250) {
                ++out.ink;
                // 행 우선 스캔이라 최초/최후 발견이 아닌 최소/최대를 추적한다.
                if (out.minX < 0 || x < out.minX) out.minX = x;
                if (x > out.maxX) out.maxX = x;
                if (out.minY < 0 || y < out.minY) out.minY = y;
                if (y > out.maxY) out.maxY = y;
            }
            if (lum > 20 && lum < 235) ++out.aa;   // 중간 농도 = 안티에일리어싱
        }
    }
    SDL_FreeSurface(srf);
    return true;
}

} // namespace

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
        const text::CellMetrics m = text::GetCellMetrics();

        int fail = 0;

        // ── 페이즈 1: 아틀라스 장착 — "가나ABC" 한 프레임 ──────────────────
        FrameStat fs;
        if (!CaptureFrame(r, cache, be, dc, Utf8ToKssm("가나ABC").c_str(), fs)) {
            goto out;
        }
        if (fs.ink < 50) { ++fail; std::printf("FAIL: ink pixels %d too few\n", fs.ink); }
        if (fs.aa < 20) { ++fail; std::printf("FAIL: AA pixels %d — bitmap path suspected\n", fs.aa); }
        // 전진 검증: 가(16)+나(16)+A(8)+B(8) = 48px 폭 → 마지막 잉크 x가 8+48=56 근처.
        if (fs.minX >= 0 && (fs.maxX < 40 || fs.maxX > 75)) {
            ++fail;
            std::printf("FAIL: ink span [%d..%d] — advance not 8/16px\n",
                        fs.minX, fs.maxX);
        }
        std::printf("phase1: ink=%d aa=%d span=[%d..%d]\n",
                    fs.ink, fs.aa, fs.minX, fs.maxX);

        // ── 글리프 span 단정: 글리프 하나씩 따로 찍어 잉크 폭 ≤ 셀 전진 ────
        // 문자셋(영문/한글)별 상한 — engW/hanW는 GetCellMetrics의 진실원.
        struct GlyphCase { const char* utf8; int limit; const char* label; };
        const GlyphCase cases[] = {
            { "가", m.hanW, "han" }, { "나", m.hanW, "han" },
            { "A", m.engW, "eng" }, { "B", m.engW, "eng" }, { "C", m.engW, "eng" },
        };
        for (const GlyphCase& gc : cases) {
            FrameStat gs;
            if (!CaptureFrame(r, cache, be, dc, Utf8ToKssm(gc.utf8).c_str(), gs)) {
                goto out;
            }
            if (gs.ink <= 0) {
                ++fail;
                std::printf("FAIL: glyph %s(%s) drew no ink\n", gc.utf8, gc.label);
                continue;
            }
            if (gs.width() > gc.limit) {
                ++fail;
                std::printf("FAIL: glyph %s(%s) ink width %d > cell %d\n",
                            gc.utf8, gc.label, gs.width(), gc.limit);
            }
            std::printf("glyph %s(%s): w=%d limit=%d ink=%d\n",
                        gc.utf8, gc.label, gs.width(), gc.limit, gs.ink);
        }

        // ── 페이즈 2: 아틀라스 탈장착 = 비트맵 경로(음성 통제 자동화) ──────
        // SetTextAtlas(nullptr)는 DrawGlyph/TextOut가 즉시 비트맵 폴백으로
        // 내려가는 유일한 공식 스위치(JKDC.cpp) — 나쁜 폰트 경로 재Init보다
        // 결정론적이다.
        dc.SetTextAtlas(nullptr, nullptr);
        FrameStat bs;
        if (!CaptureFrame(r, cache, be, dc, Utf8ToKssm("가나ABC").c_str(), bs)) {
            goto out;
        }
        if (bs.aa != 0) {
            ++fail;
            std::printf("FAIL: unmounted aa=%d — bitmap path must be binary 0/255\n",
                        bs.aa);
        }
        if (bs.ink <= 0) {
            ++fail;
            std::printf("FAIL: unmounted ink=%d — bitmap glyph must still draw\n",
                        bs.ink);
        }
        std::printf("phase2(bitmap): ink=%d aa=%d\n", bs.ink, bs.aa);

        rc = fail == 0 ? 0 : 1;
        if (fail == 0) std::printf("PASS\n");
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