// terminal_hangul_probe — 터미널 내부 한글 조합(docs/61 §22) 순수 로직 단위
// 시험. TerminalHangulInput은 창 의존 없는 클래스라 서버/pty 없이 직접 운전
// 한다. send = pty로 나갈 바이트, preEdit = 커서 오버레이. 기대치는 한글
// 리터럴로 비교 — 수기 UTF-8 이스케이프는 오탐 3연쇄의 전례(docs/57 §11).
#include <apps/TerminalHangulInput.h>
#include <SDL.h>
#ifdef main
#undef main   // SDL.h defines main to SDL_main; we use plain main()
#endif
#include <cstdio>
#include <string>

using namespace jk;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg)                                                   \
    do {                                                                   \
        if (cond) { ++g_pass; }                                            \
        else { ++g_fail; std::printf("FAIL: %s\n", msg); }                 \
    } while (0)

// 여러 키를 차례로 누르고 누적 send를 되돌린다(디버그 출력용).
static std::string Type(TerminalHangulInput& h, const char* keys,
                        std::string* flushOut = nullptr) {
    std::string sent;
    for (const char* p = keys; *p; ++p) {
        auto r = h.Letter(static_cast<SDL_Keycode>(*p), (SDL_Keymod)0);
        sent += r.send;
        if (flushOut) *flushOut += "  [k=" + std::string(1, *p) +
                                   "] send=\"" + r.send + "\" preEdit=\"" +
                                   r.preEdit + "\"\n";
    }
    return sent;
}

int main() {
    // T1: 기본은 영문 모드 — Letter는 무시, Backspace는 pty 삭제.
    {
        TerminalHangulInput t;
        auto r = t.Letter('r', (SDL_Keymod)0);
        CHECK(r.send.empty() && r.preEdit.empty(), "T1 default-off letter ignored");
        auto b = t.Backspace();
        CHECK(b.send == "\x7f" && b.preEdit.empty(), "T1 default-off backspace = DEL");
        CHECK(!t.HangulMode(), "T1 mode off");
    }

    // T2: 토글 → 한글 모드, 조합 없음, 바이트 없음.
    {
        TerminalHangulInput t;
        auto r = t.Toggle();
        CHECK(r.send.empty() && r.preEdit.empty(), "T2 toggle-on empty");
        CHECK(t.HangulMode(), "T2 mode on");
    }

    // T3: 2-set r=ㄱ — 조합 시작 → preEdit "ㄱ", pty 미송출.
    {
        TerminalHangulInput t;
        t.Toggle();
        auto r = t.Letter('r', (SDL_Keymod)0);
        std::printf("T3 send=\"%s\" preEdit=\"%s\"\n", r.send.c_str(),
                    r.preEdit.c_str());
        CHECK(r.send.empty() && r.preEdit == "ㄱ", "T3 r -> jamo preEdit");
        CHECK(t.Composing(), "T3 composing");
    }

    // T4/T5: rk=가, rkr=각 — 조합 중엔 preEdit만 갱신.
    {
        TerminalHangulInput t;
        t.Toggle();
        auto r1 = t.Letter('r', (SDL_Keymod)0);   // ㄱ
        auto r2 = t.Letter('k', (SDL_Keymod)0);   // 가
        std::printf("T4 send=\"%s\" preEdit=\"%s\"\n", r2.send.c_str(),
                    r2.preEdit.c_str());
        CHECK(r2.send.empty() && r2.preEdit == "가", "T4 rk -> ga preEdit");
        auto r3 = t.Letter('r', (SDL_Keymod)0);   // 각
        CHECK(r3.send.empty() && r3.preEdit == "각", "T5 rkr -> gak preEdit");
    }

    // T6: 각+ㄹ(받침 전이 불가) → End 캐리: "각" 송출, 씨앗 ㄹ가 preEdit.
    {
        TerminalHangulInput t;
        t.Toggle();
        Type(t, "rkr");
        auto r = t.Letter('f', (SDL_Keymod)0);
        std::printf("T6 send=\"%s\" preEdit=\"%s\"\n", r.send.c_str(),
                    r.preEdit.c_str());
        CHECK(r.send == "각", "T6 carry flushes gak");
        CHECK(r.preEdit == "ㄹ", "T6 seed riul preEdit");
    }

    // T7: 조합 중 토글 → 확정 송출 + 모드 해제. 재진입 후 조합 살아있음.
    {
        TerminalHangulInput t;
        t.Toggle();
        Type(t, "rk");
        auto r = t.Toggle();
        std::printf("T7 send=\"%s\"\n", r.send.c_str());
        CHECK(r.send == "가", "T7 toggle-off commits ga");
        CHECK(!t.HangulMode() && !t.Composing(), "T7 mode off, idle");
        auto r2 = t.Toggle();
        CHECK(r2.send.empty(), "T7 toggle-on empty");
        t.Letter('q', (SDL_Keymod)0);   // ㅂ
        t.Letter('k', (SDL_Keymod)0);   // ㅏ
        auto r3 = t.Toggle();
        CHECK(r3.send == "바", "T7 compose after reentry");
    }

    // T8: 자소 백스페이스(docs/61 §19 이식) — 가→ㄱ→(빔), 빔 이후는 pty DEL.
    {
        TerminalHangulInput t;
        t.Toggle();
        Type(t, "rk");
        auto b1 = t.Backspace();
        std::printf("T8 b1 preEdit=\"%s\" send=\"%s\"\n", b1.preEdit.c_str(),
                    b1.send.c_str());
        CHECK(b1.send.empty() && b1.preEdit == "ㄱ", "T8 ga -> gaok");
        auto b2 = t.Backspace();
        CHECK(b2.send.empty() && b2.preEdit.empty(), "T8 gaok -> empty");
        auto b3 = t.Backspace();
        CHECK(b3.send == "\x7f", "T8 empty -> pty DEL");
        // 되돌린 자소에서 재조합: ㄱ에 ㅏ → 가.
        t.Letter('k', (SDL_Keymod)0);
        CHECK(t.Composing(), "T8 recompose after pop");
    }

    // T9: 랄(fkf) 자소 역순 팝 — 랄→라→ㄹ→(빔) (jkedit T20 전례 동일).
    {
        TerminalHangulInput t;
        t.Toggle();
        Type(t, "fkf");
        auto b1 = t.Backspace();
        auto b2 = t.Backspace();
        auto b3 = t.Backspace();
        std::printf("T9 b1=\"%s\" b2=\"%s\" b3(send)=\"%s\"\n",
                    b1.preEdit.c_str(), b2.preEdit.c_str(), b3.send.c_str());
        CHECK(b1.preEdit == "라", "T9 lal -> ra");
        CHECK(b2.preEdit == "ㄹ", "T9 ra -> riul");
        CHECK(b3.preEdit.empty() && b3.send.empty(), "T9 riul -> empty");
        auto b4 = t.Backspace();
        CHECK(b4.send == "\x7f", "T9 empty -> pty DEL");
    }

    // T11: 받침 넘김 재부착(docs/65 O3 이식) — 학+ㅗ → "하" 송출+고 씨앗 뒤
    // 백스페이스는 고를 지우는 대신 넘어간 받침 ㄱ을 재부착해 학으로 되돌린다
    // (MS IME 동일). 연속: 학→하→(빔), 빔 이후는 pty DEL.
    {
        TerminalHangulInput t;
        t.Toggle();
        Type(t, "gkrh");                 // 학+ㅗ → "하" 송출, 고 조합 중
        auto b1 = t.Backspace();
        std::printf("T11 b1 preEdit=\"%s\" send-len=\"%zu\"\n",
                    b1.preEdit.c_str(), b1.send.size());
        // 재부착: pty에서 넘김 직전에 송출된 "하"를 DEL로 지우고 오버레이는 학.
        CHECK(b1.send == "\x7f" && b1.preEdit == "학",
              "T11 carry reattach hak");
        auto b2 = t.Backspace();
        CHECK(b2.send.empty() && b2.preEdit == "하", "T11 strip jong -> ha");
        auto b3 = t.Backspace();
        CHECK(b3.send.empty() && b3.preEdit == "ㅎ", "T11 strip vowel -> jamo");
        auto b4 = t.Backspace();
        CHECK(b4.send.empty() && b4.preEdit.empty(), "T11 jamo -> empty");
        auto b5 = t.Backspace();
        CHECK(b5.send == "\x7f", "T11 empty -> pty DEL");
    }

    // T10: Shift → 겹자모(docs/61 §21 — 물리 Shift만), Caps는 평자모.
    {
        TerminalHangulInput t;
        t.Toggle();
        auto r1 = t.Letter('R', KMOD_SHIFT);
        std::printf("T10 shift-R preEdit=\"%s\"\n", r1.preEdit.c_str());
        CHECK(r1.preEdit == "ㄲ", "T10 shift-R -> ssang-giok");
        TerminalHangulInput t2;
        t2.Toggle();
        auto r2 = t2.Letter('R', (SDL_Keymod)KMOD_CAPS);
        CHECK(r2.preEdit == "ㄱ", "T10 caps-R -> plain giok");
    }

    // T11: ctrl+letter 거부(호출자가 SIGINT 경로 계속하도록 빈 Result),
    // 거부돼도 진행 조합은 살아 있어야 한다.
    {
        TerminalHangulInput t;
        t.Toggle();
        Type(t, "rk");
        auto r = t.Letter('c', KMOD_CTRL);
        CHECK(r.send.empty() && r.preEdit.empty(), "T11 ctrl rejected");
        CHECK(t.Composing(), "T11 composition survives ctrl");
    }

    // T12: Enter 앞 확정 = Commit이 send 반환(조합 없으면 빈 send).
    {
        TerminalHangulInput t;
        t.Toggle();
        Type(t, "rk");
        auto r = t.Commit();
        CHECK(r.send == "가", "T12 commit ga");
        CHECK(!t.Composing(), "T12 idle after commit");
        auto r2 = t.Commit();
        CHECK(r2.send.empty(), "T12 commit empty when idle");
    }

    // T13: 문장 "가나다" — rk sk ek. End2 캐리가 받침을 새 음절 초성으로
    // 옮기며 기저 음절을 송출한다(간+ㅏ → 가+나).
    {
        TerminalHangulInput t;
        t.Toggle();
        std::string sent = Type(t, "rkskek");
        sent += t.Commit().send;
        std::printf("T13 sent=\"%s\"\n", sent.c_str());
        CHECK(sent == "가나다", "T13 ganada sentence");
    }

    // T14: Reset — 조합 상태 파기(오버레이 동기용), Commit은 빈 send.
    {
        TerminalHangulInput t;
        t.Toggle();
        Type(t, "rk");
        t.Reset();
        CHECK(!t.Composing(), "T14 reset clears state");
        CHECK(t.Commit().send.empty(), "T14 commit empty after reset");
    }

    std::printf("%d/%d checks passed\n", g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}