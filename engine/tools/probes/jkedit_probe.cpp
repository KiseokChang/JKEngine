// JKEdit defect-fix probe (docs/60 §10). Unit-level: drives JKEdit with
// simulated events and asserts the six fixes. Console probe — no window
// server; GetScreenRect falls back to the control's own rect.
#include <JKEdit.h>
#include <JKEvent.h>
#include <JKPlatform.h>
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

    // T7: 자동사 End1/End2 플러시 — 8비트 슬롯 코드가 {0x00,XX} NUL 쌍으로
    // 기록돼 한글 줄이 통째로 안 보였던 결함 (docs/61 §8). 조합 중 다음 자모가
    // 못 붙으면 완성 음절+새 자모가 독립 KSSM으로 나와야 한다.
    {
        JKEdit e(JKRect{ 0, 0, 400, 24 });
        e.SetHangulMode(true);
        // "한"(g k s) 후 ㄱ(r) — ㄱ은 받침이 안 돼 End1 플러시.
        // 옛 코드: "한"이 지워지고 {0x00,0x83} 기록 → NUL.
        SendKey(e, SDLK_g);
        SendKey(e, SDLK_k);
        SendKey(e, SDLK_s);
        SendKey(e, SDLK_r);
        const std::string& buf = e.GetText();
        bool noNul = buf.find('\0') == std::string::npos;
        Check("T7-no-nul", noNul,
              "len=" + std::to_string(buf.size()));
        // "한"(D0 65) + 독립 ㄱ(88 41) → 4바이트, 전부 KSSM 쌍(첫 바이트 >= 0x80).
        Check("T7-han-kept", buf.size() == 4 &&
                  static_cast<unsigned char>(buf[0]) == 0xD0 &&
                  static_cast<unsigned char>(buf[1]) == 0x65 &&
                  static_cast<unsigned char>(buf[2]) == 0x88 &&
                  static_cast<unsigned char>(buf[3]) == 0x41,
              "first4=" + std::to_string(static_cast<unsigned char>(buf[0])) + "," +
              std::to_string(static_cast<unsigned char>(buf[1])) + "," +
              std::to_string(static_cast<unsigned char>(buf[2])) + "," +
              std::to_string(static_cast<unsigned char>(buf[3])));
        // KSSM 왕복: 버퍼(조합형) → UTF-8이 "한ㄱ"이어야 한다.
        Check("T7-roundtrip", KssmToUtf8(buf.c_str()) == "한ㄱ",
              "got bytes=" + std::to_string(KssmToUtf8(buf.c_str()).size()));
    }

    // T8: 같은 시나리오에서 NUL 유입 전면 차단 — InsertKssmChar 방어선.
    // 자음 뒤 자음(gk → "하" 조합 중 d(ㅇ)이 못 붙는 경우는 없으므로
    // ConvertKey 직접 변환값으로 독립 코드 검증: ㄱ=0x8841, ㅏ=0x8461, ㄳ=0x8444).
    {
        Check("T8-slot-cons", HangulAutomata::ToStandaloneKssm(0x82) == 0x8841 &&
                  HangulAutomata::ToStandaloneKssm(0xA3) == 0x8461 &&
                  HangulAutomata::ToStandaloneKssm(0xC4) == 0x8444,
              "gig=" + std::to_string(HangulAutomata::ToStandaloneKssm(0x82)) +
              " a=" + std::to_string(HangulAutomata::ToStandaloneKssm(0xA3)) +
              " gg=" + std::to_string(HangulAutomata::ToStandaloneKssm(0xC4)));
    }

    // T13: 겹받침 분리 — "랄가" = ㄹㅏㄹㄱㅏ (f k f r k): 랄+ㄱ이 랅(랄ㄺ)으로
    // 조합된 후 모음이 오면 첫 받침 ㄹ만 남긴 랄 플러시+ㄱ 새 초성(라가가 아니라
    // 랄가). 옛 코드는 curHanState가 End2로 덮인 뒤 DJongsung을 판정해 항상
    // ELSE 분기로 받침을 탈락시켰다 (docs/61 §12).
    {
        JKEdit e(JKRect{ 0, 0, 400, 24 });
        e.SetHangulMode(true);
        for (int k : { SDLK_f, SDLK_k, SDLK_f, SDLK_r, SDLK_k })
            SendKey(e, k);
        Check("T13-double-jong-split", KssmToUtf8(e.GetText().c_str()) == "랄가",
              "got=" + KssmToUtf8(e.GetText().c_str()));
    }

    // T14: Shift 겹자모+모음 — Shift+r + k = "까" (하니스 형태: 소문자+플래그).
    {
        JKEdit e(JKRect{ 0, 0, 400, 24 });
        e.SetHangulMode(true);
        SendKey(e, SDLK_r, KMOD_SHIFT);
        SendKey(e, SDLK_k);
        Check("T14-shift-kka", KssmToUtf8(e.GetText().c_str()) == "까",
              "got=" + KssmToUtf8(e.GetText().c_str()));
    }

    // T15: 라이브 형태 — SDL은 shift를 키코드에 반영해 대문자 'R'로 온다.
    // 옛 코드는 대문자 범위를 한글 분기에서 거부해 조합 자체가 안 시작됐고
    // (유저 보고 "쌍자음은 처음부터 입력이 안됨"), ConvertKey의 XOR-only는
    // 'R'+shift를 'r'로 되돌려 평자모 ㄱ을 냈다 (docs/61 §13).
    {
        JKEdit e(JKRect{ 0, 0, 400, 24 });
        e.SetHangulMode(true);
        SendKey(e, 'R', KMOD_SHIFT);
        SendKey(e, SDLK_k);
        Check("T15-live-uppercase-shift", KssmToUtf8(e.GetText().c_str()) == "까",
              "got=" + KssmToUtf8(e.GetText().c_str()));
    }

    // T16: 대문자 키코드 무시(shift 없음 — capslock 경로) → 평자모 ㄱ.
    {
        JKEdit e(JKRect{ 0, 0, 400, 24 });
        e.SetHangulMode(true);
        SendKey(e, 'R');
        Check("T16-uppercase-noshift", KssmToUtf8(e.GetText().c_str()) == "ㄱ",
              "got=" + KssmToUtf8(e.GetText().c_str()));
    }

    // T17: 한/영 전환키(IME LANG1 스캔코드, docs/61 §15) — 내부 모드에서 이
    // 키를 누르면 내부 오토마타에서 손을 뗀다(콘솔 프로브에는 OS IME가
    // 없으므로 DetectWindowsImeState는 no-op, 내부 모드 해제만 검증).
    {
        JKEdit e(JKRect{ 0, 0, 400, 24 });
        e.SetHangulMode(true);
        Check("T17a-internal", e.GetInputMode() == JKEdit::InputMode::InternalHangul,
              "mode=" + std::to_string(static_cast<int>(e.GetInputMode())));
        SendKey(e, static_cast<int>(SDLK_SCANCODE_MASK | SDL_SCANCODE_LANG1));
        Check("T17b-haneng-internal-off",
              e.GetInputMode() != JKEdit::InputMode::InternalHangul,
              "mode=" + std::to_string(static_cast<int>(e.GetInputMode())));
    }

    // T18: ImeChanged 핸드오버(docs/61 §16) — 서버 폴링이 감지한 OS IME
    // 변환 상태 변화. 내부 모드에서 Hangul 전환 이벤트가 오면 진행 중 조합을
    // 확정하고 OS IME 경로를 따른다(한/영 키 자체는 SDL에 도달하지 않음).
    {
        JKEdit e(JKRect{ 0, 0, 400, 24 });
        e.SetHangulMode(true);
        for (int k : { SDLK_r, SDLK_k }) SendKey(e, k);   // "가" 조합 중
        JKEvent ev{};
        ev.type = JKEventType::ImeChanged;
        ev.option = static_cast<uint32_t>(JKPlatform::ImeMode::Hangul);
        e.RespondMessage(ev);
        Check("T18a-handover-mode",
              e.GetInputMode() == JKEdit::InputMode::ImeHangul,
              "mode=" + std::to_string(static_cast<int>(e.GetInputMode())));
        Check("T18b-handover-committed",
              KssmToUtf8(e.GetText().c_str()) == "가",
              "got=" + KssmToUtf8(e.GetText().c_str()));
        // Ascii 전환 이벤트도 수용한다.
        ev.option = static_cast<uint32_t>(JKPlatform::ImeMode::Ascii);
        e.RespondMessage(ev);
        Check("T18c-handover-ascii",
              e.GetInputMode() == JKEdit::InputMode::Ascii,
              "mode=" + std::to_string(static_cast<int>(e.GetInputMode())));
    }

    // T19: ImeToggle(서버 저수준 훅 관측, docs/61 §18) — 한/영은 F2와 동일한
    // 내부 모드 양방향 토글이다. 옛 코드는 내부→ImeHangul 한 방향 핸드오버라
    // OS IME 조합이 불능인 데스크톱(라이브 실측: "한글 상태에서도 영문 코드가
    // 온다")에서 한국어를 다시 켤 방법이 없었다. T19d/e: 내부 모드는 OS IME
    // 콘텐츠(TEXTEDITING 선조합/비ASCII 커밋)를 버리는 단일 소유 규칙.
    {
        JKEdit e(JKRect{ 0, 0, 400, 24 });
        e.SetHangulMode(true);
        for (int k : { SDLK_r, SDLK_k }) SendKey(e, k);   // "가" 조합 중
        JKEvent ev{};
        ev.type = JKEventType::ImeToggle;
        e.RespondMessage(ev);
        Check("T19a-toggle-off",
              e.GetInputMode() == JKEdit::InputMode::Ascii,
              "mode=" + std::to_string(static_cast<int>(e.GetInputMode())));
        Check("T19b-toggle-committed",
              KssmToUtf8(e.GetText().c_str()) == "가",
              "got=" + KssmToUtf8(e.GetText().c_str()));
        // 두 번째 토글: Ascii → InternalHangul (한국어 재진입 — 옛 코드 사망점).
        e.RespondMessage(ev);
        Check("T19c-toggle-on",
              e.GetInputMode() == JKEdit::InputMode::InternalHangul,
              "mode=" + std::to_string(static_cast<int>(e.GetInputMode())));
        // 재진입 후 조합이 살아있는지 — ㅂ+ㅏ = "바".
        SendKey(e, SDLK_q);
        SendKey(e, SDLK_k);
        Check("T19c2-compose-after-reentry",
              KssmToUtf8(e.GetText().c_str()) == "가바",
              "got=" + KssmToUtf8(e.GetText().c_str()));
        // 단일 소유 규칙: 내부 모드에선 TEXTEDITING 선조합과 비ASCII 커밋(Char)을
        // 모두 버린다 — OS IME가 살아 있는 환경에서 이중 조합을 막는 방어선.
        JKEvent ed{};
        ed.type = JKEventType::TextEditing;
        std::snprintf(ed.text, sizeof(ed.text), "%s", "나");
        e.RespondMessage(ed);
        JKEvent ch{};
        ch.type = JKEventType::Char;
        std::snprintf(ch.text, sizeof(ch.text), "%s", "나");
        e.RespondMessage(ch);
        Check("T19d-single-owner",
              KssmToUtf8(e.GetText().c_str()) == "가바" &&
                  e.GetInputMode() == JKEdit::InputMode::InternalHangul,
              "got=" + KssmToUtf8(e.GetText().c_str()));
    }

    // T20: 조합 중 백스페이스 = 자소 단위 되돌림(docs/61 §19) — 표준 IME처럼
    // 가→ㄱ→(빈). 옛 코드는 조합 쌍 전체를 삭제했다(유저 보고 "조합중인 글자가
    // 다 날아감"). inpStack 키별 스냅샷 pop으로 구현.
    {
        JKEdit e(JKRect{ 0, 0, 400, 24 });
        e.SetHangulMode(true);
        SendKey(e, SDLK_r);
        SendKey(e, SDLK_k);                    // "가" 조합 중
        SendKey(e, SDLK_BACKSPACE);
        Check("T20a-backspace-to-jamo",
              KssmToUtf8(e.GetText().c_str()) == "ㄱ",
              "got=" + KssmToUtf8(e.GetText().c_str()));
        SendKey(e, SDLK_k);                    // ㄱ에 ㅏ 재조합 → 가
        Check("T20b-recompose-after-pop",
              KssmToUtf8(e.GetText().c_str()) == "가",
              "got=" + KssmToUtf8(e.GetText().c_str()));
        SendKey(e, SDLK_BACKSPACE);
        SendKey(e, SDLK_BACKSPACE);
        Check("T20c-backspace-empty", e.GetText().empty(),
              "len=" + std::to_string(e.GetText().size()));
        // 겹... 아니 단일 받침 음절 되돌림: 랄 → 라 → ㄹ → (빈).
        for (int k : { SDLK_f, SDLK_k, SDLK_f }) SendKey(e, k);   // 랄
        Check("T20d-lal",
              KssmToUtf8(e.GetText().c_str()) == "랄",
              "got=" + KssmToUtf8(e.GetText().c_str()));
        SendKey(e, SDLK_BACKSPACE);
        Check("T20e-lal-to-ra",
              KssmToUtf8(e.GetText().c_str()) == "라",
              "got=" + KssmToUtf8(e.GetText().c_str()));
        SendKey(e, SDLK_BACKSPACE);
        Check("T20f-ra-to-jamo",
              KssmToUtf8(e.GetText().c_str()) == "ㄹ",
              "got=" + KssmToUtf8(e.GetText().c_str()));
        SendKey(e, SDLK_BACKSPACE);
        Check("T20g-empty2", e.GetText().empty(),
              "len=" + std::to_string(e.GetText().size()));
        // 받침 넘김(학+ㅗ → 하+고) 뒤 백스페이스: 고 씨앗만 pop → 하.
        // 넘어간 받침 ㄱ의 재부착(학 복원)은 자동사가 플러시 이력을 갖지 않아
        // 불가 — 한계 문서화(docs/61 §19).
        for (int k : { SDLK_g, SDLK_k, SDLK_r, SDLK_h }) SendKey(e, k);
        SendKey(e, SDLK_BACKSPACE);
        Check("T20h-carry-limit",
              KssmToUtf8(e.GetText().c_str()) == "하",
              "got=" + KssmToUtf8(e.GetText().c_str()));
    }

    {
        // T9: "한국어" = ㅎㅏㄴ ㄱㅜ ㄹ ㅇㅓ — 받침 뒤 새 음절이 조합돼야 한다.
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.SetHangulMode(true);
            for (int k : { SDLK_g, SDLK_k, SDLK_s, SDLK_r, SDLK_n, SDLK_r, SDLK_d, SDLK_j })
                SendKey(e, k);
            Check("T9-hangul-word", KssmToUtf8(e.GetText().c_str()) == "한국어",
                  "got=" + KssmToUtf8(e.GetText().c_str()));
        }
        // T10: 종성 넘기기 — "하고" = ㅎㅏㄱㅗ: 학 조합 후 모음이 오면 받침 ㄱ이
        // 다음 글자 초성으로 넘어가 하+고가 된다.
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.SetHangulMode(true);
            for (int k : { SDLK_g, SDLK_k, SDLK_r, SDLK_h })
                SendKey(e, k);
            Check("T10-jong-carry", KssmToUtf8(e.GetText().c_str()) == "하고",
                  "got=" + KssmToUtf8(e.GetText().c_str()));
        }
        // T11: 단독 모음 후 자음 — ㅓ+ㄴ+ㅏ: 채움 초성 음절 대신 ㅓ 플러시+새 초성.
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.SetHangulMode(true);
            for (int k : { SDLK_j, SDLK_s, SDLK_k })
                SendKey(e, k);
            Check("T11-lone-vowel", KssmToUtf8(e.GetText().c_str()) == "ㅓ나",
                  "got=" + KssmToUtf8(e.GetText().c_str()));
        }
        // T12: Shift 자모 — Shift+t = ㅆ (옛 코드는 modifier=0라 대문자 표가 죽어
        // ㅅ이 나왔다).
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.SetHangulMode(true);
            SendKey(e, SDLK_t, KMOD_SHIFT);
            Check("T12-shift-jamo", KssmToUtf8(e.GetText().c_str()) == "ㅆ",
                  "got=" + KssmToUtf8(e.GetText().c_str()));
        }
    }

    std::printf(g_fail == 0 ? "RESULT: ALL PASS\n" : "RESULT: %d FAIL\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}