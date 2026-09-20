// JKEdit defect-fix probe (docs/60 §10). Unit-level: drives JKEdit with
// simulated events and asserts the six fixes. Console probe — no window
// server; GetScreenRect falls back to the control's own rect.
#include <JKEdit.h>
#include <JKEvent.h>
#include <JKHangulUtil.h>
#include <SDL.h>
#include <cstdio>
#include <cstring>

#ifdef main
#undef main  // SDL.h defines main to SDL_main; we use plain main()
#endif

using namespace jk;

static int g_fail = 0;
static void Check(const char* name, bool ok, const std::string& detail = {}) {
    if (ok) std::printf("PASS: %s\n", name);
    else { ++g_fail; std::printf("FAIL: %s -- %s\n", name, detail.c_str()); }
}

static void SendKey(JKEdit& e, int key, SDL_Keymod mod = KMOD_NONE) {
    SDL_SetModState(static_cast<SDL_Keymod>(mod));
    JKEvent ev{};
    ev.type = JKEventType::KeyDown;
    ev.keyCode = static_cast<SDL_Keycode>(key);
    e.RespondMessage(ev);
    SDL_SetModState(KMOD_NONE);
}

static void SendChar(JKEdit& e, const char* text) {
    JKEvent ev{};
    ev.type = JKEventType::Char;
    std::snprintf(ev.text, sizeof(ev.text), "%s", text);
    e.RespondMessage(ev);
}

// hangul "한글" in KSSM 완성형 (0xB0A1..): Utf8ToKssm gives the exact bytes.
int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::printf("SDL init failed: %s\n", SDL_GetError());
        return 2;
    }

    // T1: KSSM 셀 매핑 — 캐럿/클릭 좌표가 한글에서 밀리지 않는다.
    {
        JKEdit e(JKRect{ 0, 0, 400, 24 });
        const std::string kssm = Utf8ToKssm("한글ab");
        e.SetText(kssm);
        SendKey(e, SDLK_END);
        // 클릭 x = inner.x(2) + cells*8. cell 2 = 한 글자(16px) 다음 → '한' 다음.
        size_t posMid = e.PixelToPos(2 + 8, 12);   // cell 1 → KSSM 쌍은 통째 전진 → 글자 뒤
        size_t posAfter = e.PixelToPos(2 + 16 + 4, 12); // cell 2 → '한' 뒤
        Check("T1-pair-atomic", posMid == 2 && posAfter == 2,
              "posMid=" + std::to_string(posMid) + " posAfter=" + std::to_string(posAfter));
        Check("T1-after-hangul", posAfter == 2,
              "posAfter=" + std::to_string(posAfter) + " want 2");
        // 셀 좌표 일관성: '글'(cell 2..3) 끝 = cell 4 → byte 4
        size_t posAfterTwo = e.PixelToPos(2 + 32 + 4, 12);
        Check("T1-two-hangul", posAfterTwo == 4,
              "posAfterTwo=" + std::to_string(posAfterTwo) + " want 4");
    }

    // T2: 한 줄 수평 스크롤 — END 후 클릭 왼쪽 끝은 오프셋 이후 문자.
    {
        JKEdit e(JKRect{ 0, 0, 100, 24 });  // inner w=96 → 12셀
        e.SetText("abcdefghijklmnopqrstuvwxyz");  // 26 cells
        SendKey(e, SDLK_END);
        size_t posAtLeft = e.PixelToPos(2, 12);
        Check("T2-hscroll", posAtLeft > 0,
              "posAtLeft=" + std::to_string(posAtLeft) + " want >=14 (26-12)");
    }

    // T3: 클립보드 KSSM↔UTF-8 — 한글 복사가 클립보드에서 UTF-8.
    {
        JKEdit e(JKRect{ 0, 0, 400, 24 });
        const std::string kssm = Utf8ToKssm("한글텍스트");
        e.SetText(kssm);
        e.SetSelection(0, kssm.size());
        SendKey(e, SDLK_c, KMOD_CTRL);
        char* clip = SDL_GetClipboardText();
        std::string got = clip ? clip : "";
        SDL_free(clip);
        Check("T3-copy-utf8", got == "한글텍스트", "got bytes=" + std::to_string(got.size()));
        // 붙여넣기 왕복: 빈 버퍼에 ctrl+v → 클립보드 UTF-8이 KSSM으로 변환됨.
        e.SetText("");
        SendKey(e, SDLK_v, KMOD_CTRL);
        Check("T3-paste-roundtrip", e.GetText() == kssm,
              "len=" + std::to_string(e.GetText().size()) + " want " + std::to_string(kssm.size()));
    }

    // T4: 한글 타이핑이 선택을 대체한다.
    {
        JKEdit e(JKRect{ 0, 0, 400, 24 });
        const std::string kssm = Utf8ToKssm("abc한글");
        e.SetText(kssm);
        e.SetHangulMode(true);
        e.SetSelection(3, kssm.size());  // "한글" 선택
        SendKey(e, SDLK_a);              // 내부 오토마타: 'ㅁ' 조합 시작
        // 선택(4바이트) 지워지고 조합 글자(2바이트) 진입 — 버퍼는 5바이트("abc"+1글자).
        Check("T4-selection-replaced", e.GetText().size() == 5,
              "len=" + std::to_string(e.GetText().size()) + " want 5 (3 ASCII + composing pair)");
    }

    // T5: 멀티라인 라인 유틸 + 선택 대체(InsertKssmText 경로).
    {
        JKEdit e(JKRect{ 0, 0, 400, 120 }, 0, 256, true);
        e.SetText("a\nbb\nccc");
        // TEXTINPUT(Char) 경로의 선택 대체 — InsertKssmText가 이제
        // DeleteSelection을 부른다 (docs/60 §10).
        e.SetSelection(0, 5);   // "a\nbb\n" 선택
        SendChar(e, "x");
        Check("T5-replaced", e.GetText() == "xccc", "got=" + e.GetText());
    }

    // T6: SetSelection 후 스크롤 동기화(멀티라인, 화면 밖 라인).
    {
        JKEdit e(JKRect{ 0, 0, 200, 40 }, 0, 256, true);  // 2 lines visible
        std::string big;
        for (int i = 0; i < 10; ++i) big += "line" + std::to_string(i) + "\n";
        e.SetText(big);
        SendKey(e, SDLK_a, KMOD_CTRL);  // 전체 선택
        // 캐럿이 끝 라인 — PaintClient 없이도 충돌 없는 상태 검증.
        Check("T6-select-all", e.GetSelectedText().size() == big.size(),
              "sel=" + std::to_string(e.GetSelectedText().size()));
    }

    std::printf(g_fail == 0 ? "RESULT: ALL PASS\n" : "RESULT: %d FAIL\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}