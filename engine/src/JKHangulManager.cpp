#include <JKHangulManager.h>
#include <cstring>

namespace jk {

int8_t HangulManager::IndexHF[3][32] = {
    {
         0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    },
    {
         0, 0, 0, 1, 2, 3, 4, 5, 0, 0, 6, 7, 8, 9,10,11,
         0, 0,12,13,14,15,16,17, 0, 0,18,19,20,21, 0, 0
    },
    {
         0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16, 0,17,18,19,20,21,22,23,24,25,26,27, 0, 0
    }
};

int8_t HangulManager::DLast[22] = {
    0, 0,2,0,2,1,2,1,2,3,0,2,1,3,3,1,2,1,3,3,1,1
};

int8_t HangulManager::DMiddle[20 * 2] = {
    0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,
    2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,2,3,3,3
};

int8_t HangulManager::DFirst[22 * 2] = {
    0,0,0,0,0,0,0,0,0,1,3,3,3,1,2,4,4,4,2,1,3,0,
    5,5,5,5,5,5,5,5,5,6,7,7,7,6,6,7,7,7,6,6,7,5
};

const char* HangulManager::HangulFontFile = "hangul.fnt";
const char* HangulManager::EngFontFile    = "english.fnt";
const char* HangulManager::HanjaFontFile  = "hanja.fnt";
const char* HangulManager::SpcFontFile    = "special.fnt";

HangulManager::HangulManager() {
    CreationError = false;

    HanFirstFont  = nullptr;
    HanMiddleFont = nullptr;
    HanLastFont   = nullptr;
    EnglishFont   = nullptr;

    // Load Hangul jamo font pieces.
    if (!CreationError) {
        HanFirstFont  = new uint8_t[8 * 20 * 32];
        HanMiddleFont = new uint8_t[4 * 22 * 32];
        HanLastFont   = new uint8_t[4 * 28 * 32];

        if (HanFirstFont && HanMiddleFont && HanLastFont) {
            std::memset(HanFirstFont,  0, 8 * 20 * 32);
            std::memset(HanMiddleFont, 0, 4 * 22 * 32);
            std::memset(HanLastFont,   0, 4 * 28 * 32);

            FILE* fp = std::fopen(HangulFontFile, "rb");
            if (fp) {
                std::fread(HanFirstFont,  8 * 20 * 32, 1, fp);
                std::fread(HanMiddleFont, 4 * 22 * 32, 1, fp);
                std::fread(HanLastFont,   4 * 28 * 32, 1, fp);
                std::fclose(fp);
            } else {
                CreationError = true;
            }
        } else {
            CreationError = true;
        }
    }

    // Load English 8x16 bitmap font.
    if (!CreationError) {
        EnglishFont = new uint8_t[256 * 16];
        if (EnglishFont) {
            std::memset(EnglishFont, 0, 256 * 16);
            FILE* fp = std::fopen(EngFontFile, "rb");
            if (fp) {
                std::fread(EnglishFont, 256 * 16, 1, fp);
                std::fclose(fp);
            } else {
                CreationError = true;
            }
        } else {
            CreationError = true;
        }
    }

    // Load optional Hanja bitmap font (4888 16x16 glyphs).
    if (!CreationError) {
        constexpr size_t HanjaGlyphCount = 4888;
        HanjaFont = new uint8_t[HanjaGlyphCount * 32];
        if (HanjaFont) {
            std::memset(HanjaFont, 0, HanjaGlyphCount * 32);
            FILE* fp = std::fopen(HanjaFontFile, "rb");
            if (fp) {
                std::fread(HanjaFont, HanjaGlyphCount * 32, 1, fp);
                std::fclose(fp);
            }
            // Hanja/Special fonts are optional; do not set CreationError if absent.
        }
    }

    // Load optional special-character bitmap font (256 16x16 glyphs).
    if (!CreationError) {
        constexpr size_t SpecialGlyphCount = 256;
        SpecialFont = new uint8_t[SpecialGlyphCount * 32];
        if (SpecialFont) {
            std::memset(SpecialFont, 0, SpecialGlyphCount * 32);
            FILE* fp = std::fopen(SpcFontFile, "rb");
            if (fp) {
                std::fread(SpecialFont, SpecialGlyphCount * 32, 1, fp);
                std::fclose(fp);
            }
            // Optional font; missing data is rendered as blank.
        }
    }
}

HangulManager::~HangulManager() {
    if (HanFirstFont)  delete[] HanFirstFont;
    if (HanMiddleFont) delete[] HanMiddleFont;
    if (HanLastFont)   delete[] HanLastFont;
    if (EnglishFont)   delete[] EnglishFont;
    if (HanjaFont)     delete[] HanjaFont;
    if (SpecialFont)   delete[] SpecialFont;
}

bool HangulManager::IsHanja(uint8_t ch1, uint8_t ch2) {
    if (ch1 >= 0xE0 && ch1 <= 0xF9) {
        return (ch2 >= 0x31 && ch2 <= 0x7E) || (ch2 >= 0x91 && ch2 <= 0xFE);
    }
    return false;
}

bool HangulManager::IsSpecial(uint8_t ch1, uint8_t /*ch2*/) {
    return ch1 == 0xD4;
}

int16_t HangulManager::HanjaCode2HanjaPos(uint16_t hanjacode) {
    uint8_t firstbyte  = static_cast<uint8_t>(hanjacode >> 8);
    uint8_t secondbyte = static_cast<uint8_t>(hanjacode & 0x00ff);

    if (firstbyte < 0xE0 || firstbyte > 0xF9) {
        return -1;
    }
    firstbyte -= 0xE0;

    if (secondbyte >= 0x31 && secondbyte <= 0x7E) {
        secondbyte -= 0x31;
    } else if (secondbyte >= 0x91 && secondbyte <= 0xFE) {
        secondbyte -= 0x91;
        secondbyte += 0x4E;
    } else {
        return -1;
    }
    return static_cast<int16_t>(firstbyte * 0xBC + secondbyte);
}


bool HangulManager::GetWORDImage(uint8_t* buffer, uint8_t ch1, uint8_t ch2, bool onlyeng) {
    if (!buffer) return false;

    if ((ch1 & 0x80) && !onlyeng) {
        if (IsSpecial(ch1, ch2)) {
            return GetSpecialImage(buffer, ch2);
        } else if (IsHanja(ch1, ch2)) {
            return GetHanjaImage(buffer, ch1, ch2);
        } else {
            return GetHangulImage(buffer, ch1, ch2);
        }
    }

    // English / single-byte path. Fill a 16x16 buffer with one or two 8x16 glyphs.
    std::memset(buffer, 0, 32);
    for (uint16_t i = 0; i < 16; ++i) {
        buffer[i * 2]     = EnglishFont ? EnglishFont[ch1 * 16 + i] : 0;
        buffer[i * 2 + 1] = EnglishFont ? EnglishFont[ch2 * 16 + i] : 0;
    }
    return true;
}

bool HangulManager::GetEnglishImage(uint8_t* buffer, uint8_t ch) {
    if (!buffer || !EnglishFont) return false;
    std::memcpy(buffer, EnglishFont + ch * 16, 16);
    return true;
}

bool HangulManager::GetSpecialImage(uint8_t* buffer, uint8_t ch) {
    if (!buffer) return false;
    if (SpecialFont) {
        std::memcpy(buffer, SpecialFont + static_cast<size_t>(ch) * 32, 32);
    } else {
        std::memset(buffer, 0, 32);
    }
    return true;
}

bool HangulManager::GetHanjaImage(uint8_t* buffer, uint8_t ch1, uint8_t ch2) {
    if (!buffer) return false;

    int16_t pos = HanjaCode2HanjaPos((static_cast<uint16_t>(ch1) << 8) | ch2);
    constexpr size_t HanjaGlyphCount = 4888;
    if (pos >= 0 && static_cast<size_t>(pos) < HanjaGlyphCount && HanjaFont) {
        std::memcpy(buffer, HanjaFont + static_cast<size_t>(pos) * 32, 32);
    } else {
        std::memset(buffer, 0, 32);
    }
    return true;
}

bool HangulManager::GetHangulImage(uint8_t* buffer, uint8_t ch1, uint8_t ch2) {
    if (!buffer || !HanFirstFont || !HanMiddleFont || !HanLastFont) return false;

    uint8_t fcode = ((ch1 & 0x7f) >> 2) & 0x1f;
    uint8_t mcode = (((ch1 & 0x03) << 3) | (ch2 >> 5)) & 0x1f;
    uint8_t lcode = ch2 & 0x1f;

    fcode = IndexHF[0][fcode];
    mcode = IndexHF[1][mcode];
    lcode = IndexHF[2][lcode];

    int16_t f3 = DLast[mcode];
    int16_t f2, f1;
    if (lcode) {
        f2 = DMiddle[fcode + 20];
        f1 = DFirst[mcode + 22];
    } else {
        f2 = DMiddle[fcode];
        f1 = DFirst[mcode];
    }

    std::memset(buffer, 0, 32);
    if (fcode) OrHangulImage(reinterpret_cast<int8_t*>(buffer), reinterpret_cast<int8_t*>(HanFirstFont  + (f1 * 20 + fcode) * 32));
    if (mcode) OrHangulImage(reinterpret_cast<int8_t*>(buffer), reinterpret_cast<int8_t*>(HanMiddleFont + (f2 * 22 + mcode) * 32));
    if (lcode) OrHangulImage(reinterpret_cast<int8_t*>(buffer), reinterpret_cast<int8_t*>(HanLastFont   + (f3 * 28 + lcode) * 32));
    return true;
}

void HangulManager::OrHangulImage(int8_t* dest, int8_t* src) {
    if (!dest || !src) return;
    for (int16_t i = 0; i < 32; ++i) {
        dest[i] |= src[i];
    }
}

HangulManager* HanMan = nullptr;

} // namespace jk
