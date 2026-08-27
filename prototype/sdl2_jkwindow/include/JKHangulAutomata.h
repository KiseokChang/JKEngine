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
};

} // namespace jk

#endif // JKHANGULAUTOMATA_H
