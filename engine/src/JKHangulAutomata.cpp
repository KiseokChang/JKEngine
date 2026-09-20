#include <JKHangulAutomata.h>

namespace jk {

HangulAutomata::HangulAutomata(bool hangulKeyboard) {
    hanKbdState = hangulKeyboard;
    InitAutomata();
}

void HangulAutomata::InitAutomata() {
    inpSP = 0;
    outSP = 0;
    curHanState = 0;
}

uint16_t HangulAutomata::JoongsungPair(uint16_t& key) {
    static const uint8_t JoongTable[7][3] = {
        { 0xad, 0xa3, 0xae },
        { 0xad, 0xa4, 0xaf },
        { 0xad, 0xbd, 0xb2 },
        { 0xb4, 0xa7, 0xb5 },
        { 0xb4, 0xaa, 0xb6 },
        { 0xb4, 0xbd, 0xb7 },
        { 0xbb, 0xbd, 0xbc }
    };
    for (uint16_t i = 0; i < 7; ++i) {
        if (JoongTable[i][0] == oldKey && JoongTable[i][1] == key)
            return (key = JoongTable[i][2]);
    }
    return 0;
}

uint16_t HangulAutomata::JongsungPair(uint16_t& key) {
    static const uint8_t dJongTable[11][3] = {
        { 0x82, 0x8b, 0xc4 },
        { 0x84, 0x8e, 0xc6 },
        { 0x84, 0x94, 0xc7 },
        { 0x87, 0x82, 0xca },
        { 0x87, 0x88, 0xcb },
        { 0x87, 0x89, 0xcc },
        { 0x87, 0x8b, 0xcd },
        { 0x87, 0x92, 0xce },
        { 0x87, 0x93, 0xcf },
        { 0x87, 0x94, 0xd0 },
        { 0x89, 0x8b, 0xd4 }
    };
    for (uint16_t i = 0; i < 11; ++i) {
        if (dJongTable[i][0] == oldKey && dJongTable[i][1] == key)
            return (key = dJongTable[i][2]);
    }
    return 0;
}


bool HangulAutomata::Automata(uint16_t key) {
    int16_t chKind;
    bool canBeJongsung = false;
    static const uint8_t Cho2Jong[] = {
        0xc2, 0xc3, 0xc5, 0xc8, 0x00, 0xc9, 0xd1, 0xd3, 0x00,
        0xd5, 0xd6, 0xd7, 0xd8, 0x00, 0xd9, 0xda, 0xdb, 0xdc, 0xdd
    };

    if ((key & 0x60) == 0x20) {
        chKind = static_cast<int16_t>(HanChKind::Vowel);
    } else {
        chKind = static_cast<int16_t>(HanChKind::Consonant);
        if (!(key == 0x86 || key == 0x8A || key == 0x8F))
            canBeJongsung = true;
    }

    if (curHanState) {
        charCode = inpStack[inpSP - 1].charCode;
        oldKey   = inpStack[inpSP - 1].key;
    } else {
        charCode = 0x8441;
        oldKey   = 0;
    }
    // 종료 상태(End1/End2) 판정에 쓰는 "이번 키 직전"의 상태 — 첫 번째
    // switch가 curHanState를 End1/End2로 덮기 전에 저장해야 한다.
    uint16_t prevState = curHanState;

    uint16_t keyCode = key;
    switch (curHanState) {
        case static_cast<uint16_t>(HanStatus::Start):
            if (chKind == static_cast<int16_t>(HanChKind::Consonant))
                curHanState = static_cast<uint16_t>(HanStatus::Chosung);
            else
                curHanState = static_cast<uint16_t>(HanStatus::Joongsung);
            break;
        case static_cast<uint16_t>(HanStatus::Chosung):
            if (chKind == static_cast<int16_t>(HanChKind::Vowel))
                curHanState = static_cast<uint16_t>(HanStatus::Joongsung);
            else
                curHanState = static_cast<uint16_t>(HanStatus::End1);
            break;
        case static_cast<uint16_t>(HanStatus::Joongsung):
            if (canBeJongsung) {
                // 단독 모음(초성 없음, 상위 바이트 < 0x88)은 받침을 못 붙인다 —
                // 모음을 플러시하고 자음이 새 글자의 초성이 된다. 옛 동작은
                // 채움 초성 음절로 조합해 "강아지"의 ㅈ이 받침에 붙었다.
                if ((charCode >> 8) < 0x88)
                    curHanState = static_cast<uint16_t>(HanStatus::End1);
                else
                    curHanState = static_cast<uint16_t>(HanStatus::Jongsung);
            } else if (JoongsungPair(keyCode)) {
                curHanState = static_cast<uint16_t>(HanStatus::DJoongsung);
            } else {
                curHanState = static_cast<uint16_t>(HanStatus::End1);
            }
            break;
        case static_cast<uint16_t>(HanStatus::DJoongsung):
            if (canBeJongsung) {
                if ((charCode >> 8) < 0x88)
                    curHanState = static_cast<uint16_t>(HanStatus::End1);
                else
                    curHanState = static_cast<uint16_t>(HanStatus::Jongsung);
            } else {
                curHanState = static_cast<uint16_t>(HanStatus::End1);
            }
            break;
        case static_cast<uint16_t>(HanStatus::Jongsung):
            if (chKind == static_cast<int16_t>(HanChKind::Consonant) && JongsungPair(keyCode))
                curHanState = static_cast<uint16_t>(HanStatus::DJongsung);
            else if (chKind == static_cast<int16_t>(HanChKind::Vowel))
                curHanState = static_cast<uint16_t>(HanStatus::End2);
            else
                curHanState = static_cast<uint16_t>(HanStatus::End1);
            break;
        case static_cast<uint16_t>(HanStatus::DJongsung):
            if (chKind == static_cast<int16_t>(HanChKind::Vowel))
                curHanState = static_cast<uint16_t>(HanStatus::End2);
            else
                curHanState = static_cast<uint16_t>(HanStatus::End1);
            break;
    }

    switch (curHanState) {
        case static_cast<uint16_t>(HanStatus::Chosung):
            charCode = (charCode & 0x83FF) | ((keyCode - 0x80) << 10);
            break;
        case static_cast<uint16_t>(HanStatus::Joongsung):
        case static_cast<uint16_t>(HanStatus::DJoongsung):
            charCode = (charCode & 0xFC1F) | ((keyCode - 0xA0) << 5);
            break;
        case static_cast<uint16_t>(HanStatus::Jongsung):
            keyCode = Cho2Jong[keyCode - 0x82];
            // fallthrough
        case static_cast<uint16_t>(HanStatus::DJongsung):
            charCode = (charCode & 0xFFE0) | (keyCode - 0xC0);
            break;
        case static_cast<uint16_t>(HanStatus::End1):
            // 종료: 지금까지 조합된 글자를 플러시하고 트리거 키로 새 조합을
            // 시드한다(Start 진입과 동일하게 inpStack에 적재). 옛 코드는
            // key(8비트 슬롯 코드)를 outStack에 그대로 넣어 {0x00,XX} NUL 쌍이
            // 됐고(docs/61 §8), 1차 픽스는 완성 글자+새 자모를 독립 배출해
            // 받침 뒤 조합이 끊겼다 — 새 자모는 다음 글자의 시작이므로 씨앗.
            outStack[outSP++] = charCode;
            if (chKind == static_cast<int16_t>(HanChKind::Consonant)) {
                curHanState = static_cast<uint16_t>(HanStatus::Chosung);
                charCode    = static_cast<uint16_t>(0x8041 | ((keyCode - 0x80) << 10));
            } else {
                curHanState = static_cast<uint16_t>(HanStatus::Joongsung);
                charCode    = static_cast<uint16_t>(0x8401 | ((keyCode - 0xA0) << 5));
            }
            inpStack[0].curHanState = curHanState;
            inpStack[0].charCode    = charCode;
            inpStack[0].key         = key;
            inpSP = 1;
            return true;
        case static_cast<uint16_t>(HanStatus::End2):
            // 종성+모음: 받침을 다음 글자의 초성으로 넘긴다(학+ㅗ → 하+고).
            // 플러시는 종성을 뗀 음절(겹받침이면 첫 받침만 남긴다), 씨앗은
            // 받침 자음(oldKey)의 초성 코드. inpSP--는 원본 잔해 — 제거.
            if (prevState == static_cast<uint16_t>(HanStatus::DJongsung) && inpSP >= 2) {
                uint16_t firstJong = inpStack[inpSP - 2].key;
                outStack[outSP++] = static_cast<uint16_t>(
                    (charCode & 0xFFE0) | (Cho2Jong[firstJong - 0x82] - 0xC0));
            } else {
                outStack[outSP++] = static_cast<uint16_t>((charCode & 0xFFE0) | 0x01);
            }
            curHanState = static_cast<uint16_t>(HanStatus::Joongsung);
            charCode    = static_cast<uint16_t>(0x8041 | ((oldKey - 0x80) << 10));
            // 트리거 모음을 새 글자의 중성으로 즉시 조합한다(학+ㅗ → 하+고).
            charCode = static_cast<uint16_t>((charCode & 0xFC1F) | ((keyCode - 0xA0) << 5));
            // 새 조합은 0번부터 다시 적재한다 — 옛 inpStack은 폐기(연쇄 End1에서
            // 경계 초과 방지).
            inpStack[0].curHanState = curHanState;
            inpStack[0].charCode    = charCode;
            inpStack[0].key         = keyCode;
            inpSP = 1;
            return true;
    }

    inpStack[inpSP].curHanState = curHanState;
    inpStack[inpSP].charCode    = charCode;
    inpStack[inpSP++].key       = key;
    return false;
}

// 8비트 슬롯 코드 → 독립 KSSM 2바이트 코드. 슬롯 배치는 ConvertKey의 역:
// 자음 = 0x8041|(slot<<10), 모음 = 0x8401|(slot<<5), 겹받침 = 0x8440|jong.
// 검증: ㄱ(0x82)→0x8841, ㅏ(0xA3)→0x8461, ㄳ(0xC4)→0x8444 — SingleHan과 일치.
uint16_t HangulAutomata::ToStandaloneKssm(uint16_t key8) {
    if ((key8 & 0x60) == 0x20) {
        return static_cast<uint16_t>(0x8401 | ((key8 - 0xA0) << 5));
    }
    if (key8 >= 0xC0) {
        return static_cast<uint16_t>(0x8440 | (key8 - 0xC0));
    }
    return static_cast<uint16_t>(0x8041 | ((key8 - 0x80) << 10));
}

uint16_t HangulAutomata::ConvertKey(uint16_t key, uint16_t modifier) {
    static const uint8_t HanKbrdTable[] = {
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
        0x40, 0x88, 0xBA, 0x90, 0x8D, 0x86, 0x87, 0x94,
        0xAD, 0xA5, 0xA7, 0xA3, 0xBD, 0xBB, 0xB4, 0xA6,
        0xAC, 0x8A, 0x83, 0x84, 0x8C, 0xAB, 0x93, 0x8F,
        0x92, 0xB3, 0x91, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
        0x60, 0x88, 0xBA, 0x90, 0x8D, 0x85, 0x87, 0x94,
        0xAD, 0xA5, 0xA7, 0xA3, 0xBD, 0xBB, 0xB4, 0xA4,
        0xAA, 0x89, 0x82, 0x84, 0x8B, 0xAB, 0x93, 0x8E,
        0x92, 0xB3, 0x91, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F
    };

    if (hanKbdState && (key > 32 && key < 127)) {
        if (std::isalpha(static_cast<int>(key))) {
            // keycode 대소문자와 shift 플래그를 정규화한다(docs/61 §13).
            // 라이브 SDL은 shift를 이미 키코드에 반영해 'R'(대문자)로 오고,
            // 하니스는 소문자 'r'+shift 플래그로 온다 — 둘 다 먼저 소문자로
            // 통일한 뒤 shift 플래그로 겹자모 행(대문자 행)을 조회한다.
            // 옛 XOR-only는 'R'+shift를 'r'로 되돌려 평자모 ㄱ을 냈다.
            key = static_cast<uint16_t>(std::tolower(static_cast<int>(key)));
            if (modifier & 0x0040)
                key = key ^ 0x20;
        }
        key = HanKbrdTable[key - 32] & 0xFF;
    }
    return key;
}

} // namespace jk

