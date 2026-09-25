#ifndef JKHANGULAUTOMATA_H
#define JKHANGULAUTOMATA_H

#include <cctype>
#include <cstdint>

namespace jk {

enum class HanInpResults : uint16_t {
    NoKey,
    Asc,
    HanStart,
    HanIn,
    HanEnd,
    HanBackspace,
    NoHan
};

enum class HanStatus : uint16_t {
    Start,
    Chosung,
    Joongsung,
    DJoongsung,
    Jongsung,
    DJongsung,
    End1,
    End2
};

enum class HanChKind { Consonant, Vowel };

// BackspaceJamo의 결과(docs/61 §19 + docs/65 O3). Empty: 조합이 비었다 —
// 호출자가 조합 쌍을 지운다. Jamo: 자소 1개 pop — 호출자가 조합 쌍을
// restoredCode로 재기록한다. Reattach: 받침 넘김 재부착 — 넘김 직전에
// 플러시된 음절 쌍 + 조합 쌍 4바이트를 restoredCode 2바이트로 치환한다
// (JKEdit: 버퍼 replace / 터미널: pty DEL + 오버레이).
enum class BackspaceResult { Empty = 0, Jamo = 1, Reattach = 2 };

// 두벌식 한글 조합 오토마타 (원본 JKENGINE AUTOMATA.CPP 포팅).
// 입력은 SDL/ASCII 키 코드, 출력은 KSSM 조합형 문자 코드입니다.
class HangulAutomata {
public:
    explicit HangulAutomata(bool hangulKeyboard = true);
    ~HangulAutomata() = default;

    void InitAutomata();
    uint16_t ConvertKey(uint16_t key, uint16_t modifier);
    uint16_t JoongsungPair(uint16_t& key);
    uint16_t JongsungPair(uint16_t& key);
    bool Automata(uint16_t key);

    // 조합 중 백스페이스: 마지막 키 하나를 되돌린다(docs/61 §19). inpStack은
    // 키마다 적용 후 상태 스냅샷을 쌓으므로 한 칸 pop = 자소 1개 제거.
    // 결과는 BackspaceResult — Jamo면 조합 쌍을 restoredCode로 재기록,
    // Reattach면 넘김 플러시 쌍+조합 쌍 4바이트를 restoredCode로 치환
    // (받침 넘김 핸드오버 복원: 학+ㅗ → 하+고 → 백스페이스 → 학, MS IME
    // 동일, docs/65 O3), Empty면 호출자가 조합 쌍을 버퍼에서 지운다.
    BackspaceResult BackspaceJamo(uint16_t& restoredCode);

    // 8비트 자모 슬롯 코드(ConvertKey 출력, 0x82-0xB4)를 독립 KSSM 2바이트
    // 코드로 변환한다. 자동사 End1/End2 플러시와 InsertKssmChar 방어선이 쓴다.
    // outStack에 원본 슬롯 코드를 넣으면 {0x00,XX} NUL 쌍이 버퍼에 기록돼
    // 렌더가 c_str() 절단으로 통째로 사라졌다(docs/61 §8).
    static uint16_t ToStandaloneKssm(uint16_t key8);

    struct InpStack {
        uint16_t curHanState = 0;
        uint16_t key = 0;
        uint16_t charCode = 0;
    };

    InpStack inpStack[10];
    uint16_t outStack[5];
    uint16_t inpSP = 0;
    uint16_t outSP = 0;
    uint16_t curHanState = 0;
    uint16_t charCode = 0;
    uint16_t oldKey = 0;
    bool hanKbdState = true;

    // 받침 넘김 핸드오버(docs/65 O3): End2가 받침을 다음 글자의 초성으로 넘길
    // 때(학+ㅗ → 하+고) 플러시된 음절의 조합 이력(inpStack 슬라이스)을 보관한다.
    // 씨앗만 남은 채 백스페이스가 오면 이력을 복원해 받침을 재부착한다(학).
    // End1(넘김 목표 음절이 아닌 새 플러시)과 InitAutomata에서 무효화 —
    // MS IME도 즉시 재부착만 지원한다.
    struct Handover {
        bool active = false;
        uint16_t count = 0;
        InpStack history[6];
    };
    Handover handover;
};

} // namespace jk

#endif // JKHANGULAUTOMATA_H
