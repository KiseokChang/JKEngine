// terminal_hangul_view_probe — 터미널 한글 배선 통합 시험(docs/61 §22.2).
// 순수 클래스 프로브(terminal_hangul_probe)는 오토마타만 보고, 뷰 배선
// (HandleKeyDown의 확정/수용 순서, Char/TextEditing 단일 소유, LANG1 토글)
// 은 못 본다 — 첫 실기기 보고 "자소 조합안되고 하나씩 찍힘"이 배선 결함
// (문자 키에도 무조건 확정)이었다. TerminalView를 콘솔에서 직접 운전해
// onInput_(pty 바이트)을 수집·단정한다. 아틀라스/캐시 없음 — 그리기 불요.
#include <apps/TerminalView.h>
#include <terminal/JKVtParser.h>
#include <terminal/JKTerminalGrid.h>
#include <JKHangulUtil.h>
#include <JKHanjaDict.h>
#ifdef main
#undef main
#endif
#include <SDL.h>
#include <cstdio>
#include <string>
#include <vector>

using namespace jk;

static int g_pass = 0, g_fail = 0;
static void PrintFail(const char* s) { std::printf("FAIL: %s\n", s); }
static void PrintFail(const std::string& s) { std::printf("FAIL: %s\n", s.c_str()); }
#define CHECK(cond, msg)                                                   \
    do {                                                                   \
        if (cond) { ++g_pass; }                                            \
        else { ++g_fail; PrintFail(msg); }                                 \
    } while (0)

static std::string KeyEv(int key, uint32_t mod, bool down = true) {
    JKEvent ev{};
    ev.type = down ? JKEventType::KeyDown : JKEventType::KeyUp;
    ev.keyCode = key;
    ev.option = mod;
    return std::string();   // unused; helper below feeds directly
}

struct Feed {
    JKVtParser parser;
    JKTerminalGrid grid;
    TerminalView view;
    std::string sent;
    explicit Feed() : view(&parser, &grid, nullptr, nullptr) {
        parser.Attach(&grid);
        view.SetOnInput([this](const char* d, size_t n) {
            sent.append(d, n);
        });
    }
    void Key(int key, uint32_t mod = 0) {
        JKEvent ev{};
        ev.type = JKEventType::KeyDown;
        ev.keyCode = key;
        ev.option = mod;
        view.RespondMessage(ev);
    }
    void Toggle() {
        JKEvent ev{};
        ev.type = JKEventType::ImeToggle;
        view.RespondMessage(ev);
    }
    void Hanja() {
        JKEvent ev{};
        ev.type = JKEventType::ImeHanja;
        view.RespondMessage(ev);
    }
    void Char(const char* utf8) {
        JKEvent ev{};
        ev.type = JKEventType::Char;
        std::snprintf(ev.text, sizeof(ev.text), "%s", utf8);
        view.RespondMessage(ev);
    }
    void Editing(const char* utf8) {
        JKEvent ev{};
        ev.type = JKEventType::TextEditing;
        std::snprintf(ev.text, sizeof(ev.text), "%s", utf8);
        view.RespondMessage(ev);
    }
};

int main() {
    // T1: 토글 → rk는 아무 바이트도 pty로 안 간다(§22.2 결함 회귀 — 결함
    // 버전은 'k' 시점에 "ㄱ"이 흘렀다). Enter에서 "가\r" 한번에.
    {
        Feed f;
        f.Toggle();
        f.Key('r');
        f.Key('k');
        CHECK(f.sent.empty(), "T1 composing keys send nothing");
        f.Key(SDLK_RETURN);
        CHECK(f.sent == "가\r", "T1 enter commits 가 + CR, got=" + f.sent);
    }

    // T2: 연속 음절 조합 — rkskek + Enter = "가나다\r". 조합 중 나+ㅏ 트리거
    // 시점에 End2 캐리가 기저 "가"를 먼저 송출하는 것은 정상(오토마타가
    // 받침을 새 초성으로 넘길 때 기저를 내보낸다) — 무송출이 아니다.
    {
        Feed f;
        f.Toggle();
        for (char c : std::string("rkskek")) f.Key(c);
        // 캐리 송출은 각 음절 넘어갈 때마다 — 가+나가 나간다.
        CHECK(f.sent == "가나", "T2 carry flushes ga+na mid-sentence, got=" + f.sent);
        f.Key(SDLK_RETURN);
        CHECK(f.sent == "가나다\r", "T2 sentence, got=" + f.sent);
    }

    // T3: 받침 조합 — rkr(각) 조합 중 Enter → "각\r".
    {
        Feed f;
        f.Toggle();
        f.Key('r'); f.Key('k'); f.Key('r');
        f.Key(SDLK_RETURN);
        CHECK(f.sent == "각\r", "T3 jongsung syllable, got=" + f.sent);
    }

    // T4: 조합 중 ctrl+C = 확정 후 SIGINT(가 + 0x03).
    {
        Feed f;
        f.Toggle();
        f.Key('r'); f.Key('k');
        f.Key('c', KMOD_CTRL);
        CHECK(f.sent == "가\x03", "T4 ctrl+C commits then SIGINT, got=" + f.sent);
    }

    // T5: 조합 중 Char(원시 TEXTINPUT) — 영문은 버리고, 공백은 확정+전송.
    {
        Feed f;
        f.Toggle();
        f.Key('r'); f.Key('k');
        f.Char("r");                       // KeyDown이 이미 소비한 영문 재유입
        CHECK(f.sent.empty(), "T5 alpha Char ignored in hangul mode");
        f.Char(" ");
        CHECK(f.sent == "가 ", "T5 space commits ga + space, got=" + f.sent);
    }

    // T6: 조합 중 TextEditing 선조합은 버린다(단일 소유).
    {
        Feed f;
        f.Toggle();
        f.Key('r'); f.Key('k');
        f.Editing("나");
        f.Key(SDLK_RETURN);
        CHECK(f.sent == "가\r", "T6 editing ignored, got=" + f.sent);
    }

    // T7: 조합 중 백스페이스 — pty에 바이트 없음(로컬 팝), 소진 후 DEL.
    {
        Feed f;
        f.Toggle();
        f.Key('r'); f.Key('k');
        f.Key(SDLK_BACKSPACE);
        CHECK(f.sent.empty(), "T7 composing backspace sends nothing");
        f.Key(SDLK_BACKSPACE);
        CHECK(f.sent.empty(), "T7 jamo backspace sends nothing");
        f.Key(SDLK_BACKSPACE);
        CHECK(f.sent == "\x7f", "T7 empty backspace = DEL, got=" + f.sent);
        // 팝 소진(2 pop으로 자소 전부 제거) 뒤 'k'는 ㅏ 단독 조합이 맞다.
        f.Key('k');
        f.Key(SDLK_RETURN);
        CHECK(f.sent == "\x7fㅏ\r", "T7 solo vowel after drain, got=" + f.sent);
    }

    // T8: 영문 모드(기본) — 문자는 Char 경로 그대로, 오토마타 개입 없음.
    {
        Feed f;
        f.Key('r');
        CHECK(f.sent.empty(), "T8 keydown alone sends nothing (Char path)");
        f.Char("r");
        CHECK(f.sent == "r", "T8 english via Char, got=" + f.sent);
    }

    // T9: LANG1 스캔코드 토글(ImeToggle 없는 환경).
    {
        Feed f;
        f.Key(static_cast<int>(SDLK_SCANCODE_MASK | SDL_SCANCODE_LANG1));
        f.Key('r'); f.Key('k');
        CHECK(f.sent.empty(), "T9 composing after LANG1 toggle");
        f.Key(SDLK_RETURN);
        CHECK(f.sent == "가\r", "T9 LANG1 toggled in, got=" + f.sent);
        f.Key(static_cast<int>(SDLK_SCANCODE_MASK | SDL_SCANCODE_LANG1));
        f.Char("r");
        CHECK(f.sent == "가\r" + std::string("r"),
              "T9 LANG1 toggled out to english, got=" + f.sent);
    }

    // T10: 한자 후보 모드 배선 (docs/66 B4). 사전은 가짜 프로바이더 주입 —
    // 배선(게이트 순서/커밋 단일점/흡수)만 본다. 후보[0]=韓 고정.
    jk::hanja::SetProviderForTest([](uint16_t, std::vector<uint16_t>* out) {
        const std::string k = Utf8ToKssm("韓");
        out->clear();
        out->push_back(static_cast<uint16_t>(
            (static_cast<uint16_t>(static_cast<unsigned char>(k[0])) << 8) |
            static_cast<unsigned char>(k[1])));
        return true;
    });
    {
        // a: 비조합 ImeHanja no-op — 팝업이 열리지 않으면 Char 숫자는 본래
        // 경로("1")로 흐른다(열렸다면 커밋으로 "韓"이 나간다).
        Feed f;
        f.Toggle();
        f.Hanja();
        f.Char("1");
        CHECK(f.sent == "1", "T10a non-composing hanja no-op, got=" + f.sent);
    }
    {
        // b: 조합 중 진입 + KeyDown 숫자 흡수. KeyDown 숫자는 본래 경로에서도
        // 무송출이지만, 흡수(취소 아님)여야 c의 Char 커밋이 "韓"이 된다 —
        // 게이트가 취소+통과였다면 c는 "가1"이 된다.
        Feed f;
        f.Toggle();
        f.Key('r'); f.Key('k');   // 가 조합 중
        f.Hanja();
        f.Key(SDLK_1);
        CHECK(f.sent.empty(), "T10b composing open absorbs digit, got=" + f.sent);
        // c: Char 숫자 커밋 단일점 — 후보[0]=韓 pty 송출.
        f.Char("1");
        CHECK(f.sent == "韓", "T10c char digit commits hanja, got=" + f.sent);
    }
    {
        // d: Esc 취소 → 팝업 닫힘, 이후 Char 숫자는 본래 경로(조합 확정+숫자).
        Feed f;
        f.Toggle();
        f.Key('r'); f.Key('k');
        f.Hanja();
        f.Key(SDLK_ESCAPE);
        f.Char("1");
        CHECK(f.sent == "가1", "T10d esc closes, digit passes, got=" + f.sent);
    }
    {
        // e: Backspace 취소+흡수 — 조합(가)이 훼손되지 않는다. 통과라면 자소
        // 팝으로 ㄱ만 남아 Enter가 "ㄱ\r"을 보낸다.
        Feed f;
        f.Toggle();
        f.Key('r'); f.Key('k');
        f.Hanja();
        f.Key(SDLK_BACKSPACE);
        CHECK(f.sent.empty(), "T10e backspace absorbed, got=" + f.sent);
        f.Key(SDLK_RETURN);
        CHECK(f.sent == "가\r", "T10e composition intact after absorb, got=" + f.sent);
    }
    {
        // f: Enter 커밋 + Char '\r' 흡수(1회) — 셸 엔터 유출 차단.
        Feed f;
        f.Toggle();
        f.Key('r'); f.Key('k');
        f.Hanja();
        f.Key(SDLK_RETURN);
        CHECK(f.sent == "韓", "T10f enter commits hanja, got=" + f.sent);
        f.Char("\r");
        CHECK(f.sent == "韓", "T10f trailing CR swallowed, got=" + f.sent);
        f.Char("\r");
        CHECK(f.sent == "韓\r", "T10f second CR passes, got=" + f.sent);
    }
    jk::hanja::SetProviderForTest(nullptr);   // 시스템 사전 복귀

    std::printf("%d/%d checks passed\n", g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}