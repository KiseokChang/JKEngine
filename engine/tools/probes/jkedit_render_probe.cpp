// JKEdit visual probe (docs/61 §2): renders an edit offscreen and saves BMPs
// for eye inspection. Cases: (1) Korean caret/selection alignment in a
// single line, (2) multiline selection render, (3) long-line clipping,
// (4) IME composition overlay position. Run from anywhere; writes PNG/BMP
// next to the exe.
#include <JKEdit.h>
#include <JKEvent.h>
#include <JKDC.h>
#include <JKHangulUtil.h>
#include <JKHangulManager.h>
#include <JKSDLRenderBackend.h>
#include <SDL.h>
#include <cstdio>
#include <cstring>

#ifdef main
#undef main
#endif

using namespace jk;

static void SendKey(JKEdit& e, int key, SDL_Keymod mod = KMOD_NONE) {
    SDL_SetModState(static_cast<SDL_Keymod>(mod));
    JKEvent ev{};
    ev.type = JKEventType::KeyDown;
    ev.keyCode = static_cast<SDL_Keycode>(key);
    e.RespondMessage(ev);
    SDL_SetModState(KMOD_NONE);
}

static void Snap(SDL_Renderer* ren, int w, int h, const char* path) {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
    SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
    std::printf("saved %s\n", path);
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* win = SDL_CreateWindow("jkedit-render", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, 760, 420, SDL_WINDOW_HIDDEN);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { std::printf("no renderer: %s\n", SDL_GetError()); return 2; }
    JKSDLRenderBackend backend(ren);
    // 한글 글리프는 HangulManager가 로드한 비트맵 폰트에서 나온다(docs/61 §2).
    // 미주입 시 HanPutCh 폴백=빈 사각형 — 앱 결함으로 오판한 하네스 갭.
    HangulManager hangul;
    if (hangul.CreationError) {
        std::printf("WARN: HangulManager font files missing (run from exe dir with assets/fonts)\n");
    }
    auto makeDC = [&]() {
        JKDC dc(&backend);
        dc.SetHangulManager(&hangul);
        return dc;
    };

    // (1) 한글 캐럿+선택 정렬 — "한글테스트" 후 '글'과 '테' 사이 클릭.
    {
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderClear(ren);
        JKEdit e(JKRect{ 40, 40, 420, 34 });
        e.SetText(Utf8ToKssm("한글테스트"));
        // 마우스 클릭: inner.x=42 + 2글자(32px) + 4 → '테' 앞
        JKEvent md{}; md.type = JKEventType::MouseDown; md.x = 42 + 32 + 4; md.y = 55;
        e.RespondMessage(md);
        JKEvent mu{}; mu.type = JKEventType::MouseUp; mu.x = md.x; mu.y = md.y;
        e.RespondMessage(mu);
        e.OnSetFocus();
        // 캐럿 가시화를 위해 선택: shift+← 로 '테' 선택
        SendKey(e, SDLK_LEFT, KMOD_SHIFT);
        JKDC dc = makeDC();
        {
            uint8_t img[32];
            bool ok = dc.GetHangulManager() && dc.GetHangulManager()->GetWORDImage(img, 0xD0, 0x65);
            std::printf("case1 fontman=%p wordimg=%d first-bytes=%02X %02X\n",
                        (void*)dc.GetHangulManager(), (int)ok, img[0], img[1]);
        }

        e.OnPaintClient(dc);
        Snap(ren, 760, 420, "jkedit_case1_korean_caret.bmp");
    }

    // (2) 멀티라인 선택 렌더.
    {
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderClear(ren);
        JKEdit e(JKRect{ 40, 40, 420, 160 }, 0, 4096, true);
        std::string txt = Utf8ToKssm("첫 번째 줄") + "\n" + Utf8ToKssm("둘째 줄") + "\n"
                        + Utf8ToKssm("셋째 줄") + "\n" + "last line";
        e.SetText(txt);
        // "둘째 줄" 라인 전체+앞뒤 일부 선택: 라인1 시작-1 ~ 라인2 끝+2
        // 직접 계산: 라인0="첫 번째 줄"(12B) + '\n' → 라인1 시작 13B,
        // 라인1 = "둘째 줄"(8B) → 끝 21B, 라인2 시작 22B
        e.SetSelection(11, 24);
        e.OnSetFocus();
        JKDC dc = makeDC();

        e.OnPaintClient(dc);
        Snap(ren, 760, 420, "jkedit_case2_multiline_sel.bmp");
    }

    // (3) 긴 라인 클리핑.
    {
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderClear(ren);
        JKEdit e(JKRect{ 40, 40, 300, 120 }, 0, 4096, true);
        std::string long1 = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA (50)";
        std::string long2 = Utf8ToKssm("가나다라마바사아자차카타파하") +
                            " — hangul long line overflow test";
        e.SetText(long1 + "\n" + long2);
        e.OnSetFocus();
        JKDC dc = makeDC();

        e.OnPaintClient(dc);
        Snap(ren, 760, 420, "jkedit_case3_clip.bmp");
    }

    // (4) IME 조합 문자열 오버레이 — compText_ 수동 주입(TEXTEDITING 이벤트).
    {
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderClear(ren);
        JKEdit e(JKRect{ 40, 40, 420, 34 });
        e.SetText(Utf8ToKssm("한글") + "abc");
        SendKey(e, SDLK_END);
        e.OnSetFocus();
        JKEvent te{}; te.type = JKEventType::TextEditing;
        std::snprintf(te.text, sizeof(te.text), "%s", Utf8ToKssm("한글").c_str());
        te.editStart = 4;
        e.RespondMessage(te);
        JKDC dc = makeDC();

        e.OnPaintClient(dc);
        Snap(ren, 760, 420, "jkedit_case4_ime.bmp");
    }

    // (5) 사용자 시나리오 재현: 메모(멀티라인)에 한글 타이핑(IME 커밋 시뮬레이션)
    // + 마우스 드래그 선택 — testwin 보고 "멀티라인 한글 안보임/선택 안됨".
    {
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderClear(ren);
        JKEdit e(JKRect{ 40, 40, 310, 120 }, 0, 1000, true);
        e.SetText("Line 1\nLine 2\nLine 3");
        e.OnSetFocus();
        // END로 마지막 라인 끝으로, 엔터, 한글 타이핑(TEXTINPUT=SDL이 전달하는
        // UTF-8 원문 그대로 — 실제 IME 커밋과 동일)
        SendKey(e, SDLK_END);
        SendKey(e, SDLK_RETURN);
        JKEvent c1{}; c1.type = JKEventType::Char;
        std::snprintf(c1.text, sizeof(c1.text), "%s", "한글");
        e.RespondMessage(c1);
        JKEvent c2{}; c2.type = JKEventType::Char;
        std::snprintf(c2.text, sizeof(c2.text), "%s", "입력");
        e.RespondMessage(c2);
        // 마우스 드래그 선택: 첫 라인 "Line 1" 전체 (x=42..42+48, y 라인0)
        JKEvent md{}; md.type = JKEventType::MouseDown; md.x = 42; md.y = 44;
        e.RespondMessage(md);
        JKEvent mm{}; mm.type = JKEventType::MouseMove; mm.x = 42 + 48; mm.y = 44;
        e.RespondMessage(mm);
        JKEvent mu{}; mu.type = JKEventType::MouseUp; mu.x = 42 + 48; mu.y = 44;
        e.RespondMessage(mu);
        JKDC dc = makeDC();
        e.OnPaintClient(dc);
        Snap(ren, 760, 420, "jkedit_case5_memo_typing.bmp");
        std::printf("case5 buffer bytes=%zu\n", e.GetText().size());
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    std::printf("DONE\n");
    return 0;
}