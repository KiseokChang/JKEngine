// JKEdit defect-fix probe (docs/60 §10). Unit-level: drives JKEdit with
// simulated events and asserts the six fixes. Console probe — no window
// server; GetScreenRect falls back to the control's own rect.
#include <JKEdit.h>
#include <JKEvent.h>
#include <JKPlatform.h>
#include <JKHangulUtil.h>
#include <JKHanjaDict.h>
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

// T24 한자 모드용 가짜 프로바이더(docs/66 O1): 앞 5개는 실측 '한' 후보(docs/66
// §3.5)를 Utf8ToKssm로 만든 결정론 목록, 뒤 7개는 페이지 내비 테스트용 필러
// KSSM 쌍. 후보 12 = 2페이지.
static uint16_t kssmPairOf(const char* utf8) {
    const std::string k = Utf8ToKssm(utf8);
    return k.size() == 2
               ? static_cast<uint16_t>((static_cast<uint16_t>(
                                            static_cast<unsigned char>(k[0]))
                                        << 8) |
                                       static_cast<unsigned char>(k[1]))
               : 0;
}
static bool FakeProvider(uint16_t, std::vector<uint16_t>* out) {
    const uint16_t real[] = { kssmPairOf("韓"), kssmPairOf("漢"),
                              kssmPairOf("限"), kssmPairOf("寒"),
                              kssmPairOf("翰") };
    out->clear();
    for (uint16_t v : real)
        if (v) out->push_back(v);
    for (uint16_t v = 0xB1A1; v <= 0xB1A7; ++v) out->push_back(v);
    return true;
}

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
        // 받침 넘김 재부착(docs/65 O3): 학+ㅗ → 하+고 뒤 백스페이스는 고를 지우는
        // 대신 넘어간 받침 ㄱ을 재부착해 학으로 되돌린다(MS IME 동일). 옛 코드는
        // 고 씨앗만 pop해 "하"가 남았다(docs/61 §19 한계 → O3로 수술). 재부착
        // 뒤 이력 스택도 복원돼 연속 백스페이스가 학→하→(빈)으로 이어진다.
        for (int k : { SDLK_g, SDLK_k, SDLK_r, SDLK_h }) SendKey(e, k);
        SendKey(e, SDLK_BACKSPACE);
        Check("T20h-carry-reattach",
              KssmToUtf8(e.GetText().c_str()) == "학",
              "got=" + KssmToUtf8(e.GetText().c_str()));
        SendKey(e, SDLK_BACKSPACE);
        Check("T20i-reattach-strip-jong",
              KssmToUtf8(e.GetText().c_str()) == "하",
              "got=" + KssmToUtf8(e.GetText().c_str()));
        SendKey(e, SDLK_BACKSPACE);
        // 재부착 뒤 마지막 ㅏ pop은 조합 쌍을 ㅎ 단독으로 남긴다(T20f 전례 동일).
        Check("T20j-reattach-strip-vowel",
              KssmToUtf8(e.GetText().c_str()) == "ㅎ",
              "got=" + KssmToUtf8(e.GetText().c_str()));
        SendKey(e, SDLK_BACKSPACE);
        Check("T20j2-reattach-chain-empty", e.GetText().empty(),
              "len=" + std::to_string(e.GetText().size()));
        // 겹받침 넘김 재부착: 닭+ㅗ → 달+고 뒤 백스페이스 → 닭 복원(둘째 받침
        // ㄺ 통째로 재부착 — 플러시는 첫 받침 ㄹ만 남긴 달이었어도 이력은
        // 닭 전체). 완성형 표 내 글자로 검증(KSSM 왕복 가능).
        for (int k : { SDLK_e, SDLK_k, SDLK_f, SDLK_r, SDLK_h }) SendKey(e, k);
        SendKey(e, SDLK_BACKSPACE);
        Check("T20k-djongsung-carry-reattach",
              KssmToUtf8(e.GetText().c_str()) == "닭",
              "got=" + KssmToUtf8(e.GetText().c_str()));
        SendKey(e, SDLK_BACKSPACE);
        Check("T20l-djongsung-strip-second-jong",
              KssmToUtf8(e.GetText().c_str()) == "달",
              "got=" + KssmToUtf8(e.GetText().c_str()));
        SendKey(e, SDLK_BACKSPACE);
        Check("T20m-djongsung-strip-first-jong",
              KssmToUtf8(e.GetText().c_str()) == "다",
              "got=" + KssmToUtf8(e.GetText().c_str()));
        SendKey(e, SDLK_BACKSPACE);
        Check("T20m2-djongsung-strip-vowel",
              KssmToUtf8(e.GetText().c_str()) == "ㄷ",
              "got=" + KssmToUtf8(e.GetText().c_str()));
        SendKey(e, SDLK_BACKSPACE);
        Check("T20n-djongsung-chain-empty", e.GetText().empty(),
              "len=" + std::to_string(e.GetText().size()));
        // 핸드오버 무효: 넘김 뒤 새 플러시(End1)가 이력을 갈아치우면 재부착
        // 대신 일반 pop — 하고+ㅔ → 하고애 뒤 백스페이스 ×2 → 하. 쌍모음
        // (ㅗ+ㅏ=ㅘ, ㅗ+ㅣ=ㅢ)은 End1을 안 일으키므로 쌍 아닌 모음 ㅔ 사용.
        for (int k : { SDLK_g, SDLK_k, SDLK_r, SDLK_h, SDLK_p }) SendKey(e, k);
        SendKey(e, SDLK_BACKSPACE);
        SendKey(e, SDLK_BACKSPACE);
        Check("T20o-stale-handover-invalidated",
              KssmToUtf8(e.GetText().c_str()) == "하",
              "got=" + KssmToUtf8(e.GetText().c_str()));
    }

    // T21: 캡스락 상태의 한글 조합(docs/61 §21) — 캡스락은 쌍자음 의도가
    // 아니다. 옛 shift XOR caps는 캡스락 상태에서 평자모 키를 겹자모 행으로
    // 보냈다(유저 보고 "캡스락에서 한글 입력시 쌍자음"). 겹자모 플래그는
    // 물리 Shift만 따르고, 캡스락의 대소문자는 ConvertKey의 tolower
    // 정규화가 흡수한다.
    {
        JKEdit e(JKRect{ 0, 0, 400, 24 });
        e.SetHangulMode(true);
        SendKey(e, SDLK_r, KMOD_CAPS);         // 소문자 키코드+캡스 → 평자모
        SendKey(e, SDLK_k);
        Check("T21a-caps-lower",
              KssmToUtf8(e.GetText().c_str()) == "가",
              "got=" + KssmToUtf8(e.GetText().c_str()));
        JKEdit e2(JKRect{ 0, 0, 400, 24 });
        e2.SetHangulMode(true);
        SendKey(e2, 'R', KMOD_CAPS);           // 대문자 키코드+캡스 → 평자모
        Check("T21b-caps-upper-keycode",
              KssmToUtf8(e2.GetText().c_str()) == "ㄱ",
              "got=" + KssmToUtf8(e2.GetText().c_str()));
        JKEdit e3(JKRect{ 0, 0, 400, 24 });
        e3.SetHangulMode(true);
        SendKey(e3, 'R', static_cast<SDL_Keymod>(KMOD_SHIFT | KMOD_CAPS));
        Check("T21c-shift-wins",               // Shift+캡스는 겹자모
              KssmToUtf8(e3.GetText().c_str()) == "ㄲ",
              "got=" + KssmToUtf8(e3.GetText().c_str()));
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

    // T23 (docs/61 §23): KSSM 쌍 경계 판정 — "바이트 >= 0x80 = 첫 바이트"
    // 휴리스틱은 둘째 바이트가 0x80+인 실제 코드(wCodeTable 0x88a1류)에서
    // 오판한다. 둘째 바이트 >= 0x80인 음절 X를 자동 선별해 증상 3종을 잠근다:
    // ① 백스페이스가 X+ASCII를 통째로 지움 ② Left가 쌍 중간에 진입(제자리→
    // 2셀 점프) ③ Up/Down이 바이트 컬럼을 건네 쌍 중간에 떨어진다.
    {
        // 선별: U+AC00..부터 처음 만나는 "KSSM 둘째 바이트 >= 0x80" 음절.
        std::string hi;   // 둘째 바이트 >= 0x80 음절
        for (uint32_t u = 0xAC00; u <= 0xD7A3 && hi.empty(); ++u) {
            char utf8[5] = { static_cast<char>(0xE0 | (u >> 12)),
                             static_cast<char>(0x80 | ((u >> 6) & 0x3F)),
                             static_cast<char>(0x80 | (u & 0x3F)), 0 };
            const std::string k = Utf8ToKssm(utf8);
            if (k.size() == 2 &&
                static_cast<unsigned char>(k[1]) >= 0x80)
                hi = utf8;
        }
        Check("T23a-pick", !hi.empty(),
              "둘째 바이트>=0x80 음절 선별 실패 — Utf8ToKssm 표 점검");
        if (!hi.empty()) {
            const std::string kssmHi = Utf8ToKssm(hi.c_str());
            // ② Left: "X a Y"(한글 ASCII 한글) 끝에서 Left 2회 = 'a' 시작 경계.
            //   옛 코드는 buffer_[cursor-2]를 무조건 쌍 첫 바이트로 가정 —
            //   이전 글자가 ASCII면 2칸 뒤는 Y-1 ASCII가 아니라 X.second(>=0x80)
            //   이 되어 X 둘째 바이트(쌍 중간)로 진입한다.
            {
                JKEdit e(JKRect{ 0, 0, 400, 24 });
                e.SetText(kssmHi + "a" + kssmHi);
                SendKey(e, SDLK_END);
                SendKey(e, SDLK_LEFT, KMOD_SHIFT);   // 선택 확장으로 커서 관측
                SendKey(e, SDLK_LEFT, KMOD_SHIFT);
                Check("T23b-left-boundary", e.GetSelectedText() == "a" + kssmHi,
                      "sel len=" + std::to_string(e.GetSelectedText().size()) +
                      " want 3 (쌍 중간 진입이면 4)");
            }
            // ① Backspace: "X 1" 끝에서 백스페이스 = '1'만 삭제.
            //   옛 코드: buffer_[1]=X.second(>=0x80)를 쌍으로 오판 →
            //   X 둘째 바이트+1 같이 삭제(2글자 삭제).
            {
                JKEdit e(JKRect{ 0, 0, 400, 24 });
                e.SetText(kssmHi + "1");
                SendKey(e, SDLK_END);
                SendKey(e, SDLK_BACKSPACE);
                Check("T23c-backspace-ascii", e.GetText() == kssmHi,
                      "len=" + std::to_string(e.GetText().size()) + " want 2");
                // 이어서 타이핑 — 쌍 중간 커서에 삽입되면 쌍이 쪼개진다.
                SendChar(e, "2");
                Check("T23d-type-after-backspace",
                      KssmToUtf8(e.GetText().c_str()) == hi + "2",
                      "got=" + KssmToUtf8(e.GetText().c_str()));
            }
            // ③ Up/Down: 1행=X(2셀), 2행="abc". 2행 'a' 뒤(byte col 1)에서 Up
            //   — 옛 코드는 바이트 컬럼 1을 그대로 1행에 적용해 쌍 중간에
            //   떨어진다. 올바른 것은 표시 셀 컬럼(1셀 → 쌍 통째 전진 = 2바이트).
            {
                JKEdit e(JKRect{ 0, 0, 400, 120 }, 0, 256, true);
                e.SetText(kssmHi + "\nabc");
                e.SetSelection(0, 4);            // 2행 'a' 뒤
                SendKey(e, SDLK_UP, KMOD_SHIFT); // 선택 확장으로 커서 관측
                Check("T23e-up-cellcol", e.GetSelectedText() == kssmHi,
                      "sel len=" + std::to_string(e.GetSelectedText().size()) +
                      " want 2 (쌍 중간이면 1)");
            }
            // 방어선: Delete도 경계 스캔 — 쌍 시작의 Forward 삭제는 2바이트 통째.
            {
                JKEdit e(JKRect{ 0, 0, 400, 24 });
                e.SetText(kssmHi + "1");
                SendKey(e, SDLK_HOME);
                SendKey(e, SDLK_DELETE);
                Check("T23f-deletefwd-boundary", e.GetText() == "1",
                      "len=" + std::to_string(e.GetText().size()) + " want 1");
            }
        }
    }

    // T24: 한자 후보 모드 (docs/66 O1). 모드 로직은 가짜 프로바이더 주입으로
    // 결정론 돈다(docs/65 O2 양성 대조군 철학) — 실제 사전 채널은 별도 구조
    // 프로브(probe_hanja_dict5)가 측정했다. 후보 12개(2페이지) 주입.
    {
        jk::hanja::SetProviderForTest(&FakeProvider);
        Check("T24m1-injected-available", jk::hanja::Available(),
              "주입 중 Available()=true여야 한다");
        {
            std::vector<uint16_t> a, b;
            const bool ok = jk::hanja::Candidates(0xB0A1 /*한*/, &a) &&
                            jk::hanja::Candidates(0xB0A1, &b) && a == b &&
                            a.size() == 12 && a[0] == kssmPairOf("韓");
            Check("T24m2-injected-candidates", ok,
                  "주입 리스트 왕복+결정론+첫 후보=韓");
        }

        const std::string kssmHan  = Utf8ToKssm("한");
        const std::string kssmMa   = Utf8ToKssm("마");
        const std::string kssmHanja0 = Utf8ToKssm("韓");   // 후보 [0]
        const std::string kssmHanja1 = Utf8ToKssm("漢");   // 후보 [1]
        const std::string kssmHanja2 = Utf8ToKssm("限");   // 후보 [2]
        const std::string kssmHanja4 = Utf8ToKssm("寒");   // 후보 [4]
        const std::string kssmHanja9 = { static_cast<char>(0xB1),
                                         static_cast<char>(0xA5) }; // 후보 [9] 필러

        auto openHanja = [&](JKEdit& e) {   // 진입 헬퍼: ImeHanja 이벤트
            JKEvent ev{};
            ev.type = JKEventType::ImeHanja;
            e.RespondMessage(ev);
        };

        // a 진입 — 조합 중 대상(조합은 먼저 확정) + 숫자 이중 게이트 + 커서.
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.OnSetFocus();
            e.SetHangulMode(true);
            SendKey(e, SDLK_a);        // ㅁ 조합
            SendKey(e, SDLK_k);        // 마 조합 중(버퍼에 쌍 유지)
            openHanja(e);
            SendKey(e, SDLK_1);        // KeyDown 숫자 = 흡수만
            Check("T24a1-enter-open-absorb",
                  KssmToUtf8(e.GetText().c_str()) == "마",
                  "got=" + KssmToUtf8(e.GetText().c_str()) +
                  " (팝업 미오픈이면 '1'이 삽입된다)");
            SendChar(e, "1");          // Char 경유 커밋 단일점 — 후보[0]=韓
            Check("T24a2-composing-commit",
                  e.GetText() == kssmHanja0,
                  "got len=" + std::to_string(e.GetText().size()));
            SendChar(e, "8");          // ASCII 후행 삽입 — 커서는 쌍 뒤
            Check("T24a3-cursor-after-pair",
                  KssmToUtf8(e.GetText().c_str()) == "韓8",
                  "got=" + KssmToUtf8(e.GetText().c_str()));
        }
        // b 완성 음절 대상 + 대상 부재 가드.
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.OnSetFocus();
            e.SetHangulMode(true);
            e.SetText(Utf8ToKssm("ab한"));
            SendKey(e, SDLK_END);
            openHanja(e);
            SendChar(e, "2");
            Check("T24b1-complete-syllable",
                  e.GetText() == "ab" + kssmHanja1,
                  "got len=" + std::to_string(e.GetText().size()));
        }
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.OnSetFocus();
            e.SetHangulMode(true);
            e.SetText(kssmHan + "ab");
            SendKey(e, SDLK_HOME);     // 커서 앞 — 직전 쌍 없음
            openHanja(e);
            SendChar(e, "5");          // 팝업 미오픈 — 숫자는 본래 삽입 경로
            Check("T24b2-no-target-noop",
                  e.GetText() == "5" + kssmHan + "ab",
                  "got len=" + std::to_string(e.GetText().size()));
        }
        // c 숫자 커밋 — 치환(길이 불변)+흡수 순서.
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.OnSetFocus();
            e.SetHangulMode(true);
            e.SetText(kssmHan);
            SendKey(e, SDLK_END);
            openHanja(e);
            SendKey(e, SDLK_3);        // KeyDown 흡수
            Check("T24c1-digit-absorbed", e.GetText() == kssmHan,
                  "got len=" + std::to_string(e.GetText().size()));
            SendChar(e, "3");          // Char 커밋 — 후보[2]=限
            Check("T24c2-char-commit", e.GetText() == kssmHanja2,
                  "got len=" + std::to_string(e.GetText().size()));
            Check("T24c3-inplace-2byte", e.GetText().size() == 2,
                  "len=" + std::to_string(e.GetText().size()) +
                  " want 2 (삽입이면 3)");
            SendChar(e, "8");
            Check("T24c4-caret-after",
                  KssmToUtf8(e.GetText().c_str()) == "限8",
                  "got=" + KssmToUtf8(e.GetText().c_str()));
        }
        // d Esc 취소 무변.
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.OnSetFocus();
            e.SetHangulMode(true);
            e.SetText(kssmHan);
            SendKey(e, SDLK_END);
            openHanja(e);
            SendKey(e, SDLK_ESCAPE);
            Check("T24d1-esc-unchanged", e.GetText() == kssmHan,
                  "got len=" + std::to_string(e.GetText().size()));
            SendChar(e, "1");          // 팝업 닫힘 — 이제 숫자가 삽입된다
            Check("T24d2-esc-closed",
                  KssmToUtf8(e.GetText().c_str()) == "한1",
                  "got=" + KssmToUtf8(e.GetText().c_str()));
        }
        // e 내비게이션 — Right/Left ±1, PageDown/Up 페이지(12 후보=2페이지).
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.OnSetFocus();
            e.SetHangulMode(true);
            e.SetText(kssmHan);
            SendKey(e, SDLK_END);
            openHanja(e);
            SendKey(e, SDLK_RIGHT);    // 전역 선택 1
            SendKey(e, SDLK_RETURN);   // Enter 커밋 — 후보[1]=漢
            Check("T24e1-right-enter",
                  e.GetText() == kssmHanja1,
                  "got len=" + std::to_string(e.GetText().size()));
            SendChar(e, "\r");         // swallow 플래그 — 개행 무출
            Check("T24e2-enter-swallow-cr", e.GetText() == kssmHanja1,
                  "got len=" + std::to_string(e.GetText().size()));
        }
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.OnSetFocus();
            e.SetHangulMode(true);
            e.SetText(kssmHan);
            SendKey(e, SDLK_END);
            openHanja(e);
            SendKey(e, SDLK_LEFT);     // 전역 0에서 Left — 고정
            SendKey(e, SDLK_PAGEDOWN); // 2페이지 — 전역 9
            SendKey(e, SDLK_RETURN);
            {
                std::string hex;
                for (unsigned char b : e.GetText())
                    hex += std::to_string(b) + " ";
                Check("T24e3-pagedown-commit",
                      e.GetText() == kssmHanja9,
                      "got bytes=" + hex);
            }
        }
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.OnSetFocus();
            e.SetHangulMode(true);
            e.SetText(kssmHan);
            SendKey(e, SDLK_END);
            openHanja(e);
            SendKey(e, SDLK_PAGEUP);   // 0페이지에서 Up — 고정
            SendKey(e, SDLK_DOWN);     // 2페이지
            SendKey(e, SDLK_PAGEUP);   // 되돌아 0페이지 head — 전역 0=韓
            SendKey(e, SDLK_RETURN);
            Check("T24e4-pageup-commit",
                  e.GetText() == kssmHanja0,
                  "got len=" + std::to_string(e.GetText().size()));
        }
        // f Backspace 취소+흡수 — 두 번째 Backspace만 실제 삭제.
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.OnSetFocus();
            e.SetHangulMode(true);
            e.SetText(kssmHan);
            SendKey(e, SDLK_END);
            openHanja(e);
            SendKey(e, SDLK_BACKSPACE);
            Check("T24f1-bs-absorbed", e.GetText() == kssmHan,
                  "got len=" + std::to_string(e.GetText().size()));
            SendKey(e, SDLK_BACKSPACE);   // 팝업 닫힌 뒤 — 실제 삭제
            Check("T24f2-bs-real-after", e.GetText().empty(),
                  "len=" + std::to_string(e.GetText().size()));
        }
        // g 기타 키 = 취소 후 본래 경로 통과.
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.OnSetFocus();
            e.SetHangulMode(true);
            e.SetText(kssmHan);
            SendKey(e, SDLK_END);
            openHanja(e);
            SendKey(e, SDLK_F3);       // 그 밖 키 = 취소+통과(HOME/END/DELETE는
                                       // 커밋 대상 보호 흡수 — docs/66 §B3)
            SendChar(e, "5");          // 커서 END 삽입 — 통과 증명
            Check("T24g1-otherkey-passthrough",
                  KssmToUtf8(e.GetText().c_str()) == "한5",
                  "got=" + KssmToUtf8(e.GetText().c_str()));
        }
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.OnSetFocus();
            e.SetHangulMode(true);
            e.SetText(kssmHan);
            SendKey(e, SDLK_END);
            openHanja(e);
            SendKey(e, SDLK_F1);       // 취소만 — F1 자체는 무동작
            SendChar(e, "1");
            Check("T24g2-otherkey-cancel",
                  KssmToUtf8(e.GetText().c_str()) == "한1",
                  "got=" + KssmToUtf8(e.GetText().c_str()));
        }
        // h Char 게이트 — 숫자 커밋 + 기타 문자 취소 후 삽입.
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.OnSetFocus();
            e.SetHangulMode(true);
            e.SetText(kssmHan);
            SendKey(e, SDLK_END);
            openHanja(e);
            SendChar(e, "4");          // 후보[3]=寒
            Check("T24h1-char-digit-commit", e.GetText() == kssmHanja4,
                  "got len=" + std::to_string(e.GetText().size()));
        }
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.OnSetFocus();
            e.SetHangulMode(true);
            e.SetText(kssmHan);
            SendKey(e, SDLK_END);
            openHanja(e);
            SendChar(e, "q");          // 취소 — 알파벳 Char는 한글 모드에서
                                       // 자동소유라 삽입되지 않는다(엔진 모델)
            Check("T24h2-char-other-cancel", e.GetText() == kssmHan,
                  "got len=" + std::to_string(e.GetText().size()));
            SendChar(e, "1");          // 팝업 닫힘 증명 — 이제 숫자 삽입
            Check("T24h2b-cancel-closed",
                  KssmToUtf8(e.GetText().c_str()) == "한1",
                  "got=" + KssmToUtf8(e.GetText().c_str()));
        }
        // i 모드 가드 — ImeHangul(OS IME 소유)·Ascii no-op.
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.OnSetFocus();
            e.SetInputMode(JKEdit::InputMode::ImeHangul);
            e.SetText(kssmHan);
            SendKey(e, SDLK_END);
            openHanja(e);
            SendChar(e, "1");
            Check("T24i1-imehangul-noop",
                  KssmToUtf8(e.GetText().c_str()) == "한1",
                  "got=" + KssmToUtf8(e.GetText().c_str()));
        }
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.OnSetFocus();            // 기본 Ascii
            e.SetText(kssmHan);
            SendKey(e, SDLK_END);
            openHanja(e);
            SendChar(e, "1");
            Check("T24i2-ascii-noop",
                  KssmToUtf8(e.GetText().c_str()) == "한1",
                  "got=" + KssmToUtf8(e.GetText().c_str()));
        }
        // j 대상 가드 — 이미 한자·ASCII no-op.
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.OnSetFocus();
            e.SetHangulMode(true);
            e.SetText(kssmHanja1);     // 漢 — 역방향 사전 미지원 v1 no-op
            SendKey(e, SDLK_END);
            openHanja(e);
            SendChar(e, "1");
            Check("T24j1-hanja-target-noop",
                  e.GetText() == kssmHanja1 + "1",
                  "got len=" + std::to_string(e.GetText().size()));
        }
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.OnSetFocus();
            e.SetHangulMode(true);
            e.SetText("ab");
            SendKey(e, SDLK_END);
            openHanja(e);
            SendChar(e, "1");
            Check("T24j2-ascii-target-noop", e.GetText() == "ab1",
                  "got=" + e.GetText());
        }
        // k readOnly.
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.OnSetFocus();
            e.SetHangulMode(true);
            e.SetReadOnly(true);
            e.SetText(kssmHan);
            SendKey(e, SDLK_END);
            openHanja(e);
            SendKey(e, SDLK_1);
            Check("T24k-readonly-noop", e.GetText() == kssmHan,
                  "got len=" + std::to_string(e.GetText().size()));
        }
        // l 마우스다운 취소.
        {
            JKEdit e(JKRect{ 0, 0, 400, 24 });
            e.OnSetFocus();
            e.SetHangulMode(true);
            e.SetText(kssmHan);
            SendKey(e, SDLK_END);
            openHanja(e);
            JKEvent md{};
            md.type = JKEventType::MouseDown;
            md.x = 20; md.y = 12;
            e.RespondMessage(md);
            SendChar(e, "1");
            Check("T24l-mousedown-cancel",
                  KssmToUtf8(e.GetText().c_str()) == "한1",
                  "got=" + KssmToUtf8(e.GetText().c_str()));
        }
        jk::hanja::SetProviderForTest(nullptr);   // 시스템 사전 복귀
    }

    std::printf(g_fail == 0 ? "RESULT: ALL PASS\n" : "RESULT: %d FAIL\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}