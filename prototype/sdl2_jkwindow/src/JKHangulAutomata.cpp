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
            if (canBeJongsung)
                curHanState = static_cast<uint16_t>(HanStatus::Jongsung);
            else if (JoongsungPair(keyCode))
                curHanState = static_cast<uint16_t>(HanStatus::DJoongsung);
            else
                curHanState = static_cast<uint16_t>(HanStatus::End1);
            break;
        case static_cast<uint16_t>(HanStatus::DJoongsung):
            if (canBeJongsung)
                curHanState = static_cast<uint16_t>(HanStatus::Jongsung);
            else
                curHanState = static_cast<uint16_t>(HanStatus::End1);
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
            outStack[outSP++] = key;
            return true;
        case static_cast<uint16_t>(HanStatus::End2):
            outStack[outSP++] = key;
            outStack[outSP++] = oldKey;
            inpSP--;
            return true;
    }

    inpStack[inpSP].curHanState = curHanState;
    inpStack[inpSP].charCode    = charCode;
    inpStack[inpSP++].key       = key;
    return false;
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
        if ((modifier & 0x0040) && std::isalpha(static_cast<int>(key)))
            key = key ^ 0x20;
        key = HanKbrdTable[key - 32] & 0xFF;
    }
    return key;
}

} // namespace jk

