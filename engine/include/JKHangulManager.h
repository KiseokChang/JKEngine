#ifndef JKHANGULMANAGER_H
#define JKHANGULMANAGER_H

#include <JKTypes.h>
#include <cstdint>
#include <cstdio>

namespace jk {

// Bitmap Hangul/English/Hanja/Special font manager ported from the original JKENGINE.
// Requires external font files in the working directory:
//   - hangul.fnt  (Hangul jamo glyph pieces)
//   - english.fnt (256 8x16 English glyphs)
//   - hanja.fnt   (4888 16x16 Hanja glyphs, optional)
//   - special.fnt (256 16x16 special glyphs, optional)
// If hangul.fnt or english.fnt are missing, CreationError is set and only the
// built-in ASCII fallback in JKDC will be available.
class HangulManager {
public:
    HangulManager();
    virtual ~HangulManager();

    bool CreationError = false;

    uint8_t* HanFirstFont  = nullptr;  // 8*20*32 bytes
    uint8_t* HanMiddleFont = nullptr;  // 4*22*32 bytes
    uint8_t* HanLastFont   = nullptr;  // 4*28*32 bytes
    uint8_t* EnglishFont   = nullptr;  // 256*16 bytes
    uint8_t* HanjaFont     = nullptr;  // 4888*32 bytes
    uint8_t* SpecialFont   = nullptr;  // 256*32 bytes

    bool GetWORDImage(uint8_t* buffer, uint8_t ch1, uint8_t ch2, bool onlyeng = false);
    bool GetEnglishImage(uint8_t* buffer, uint8_t ch);
    bool GetSpecialImage(uint8_t* buffer, uint8_t ch);
    bool GetHanjaImage(uint8_t* buffer, uint8_t ch1, uint8_t ch2);
    bool GetHangulImage(uint8_t* buffer, uint8_t ch1, uint8_t ch2);

    static bool IsHanja(uint8_t ch1, uint8_t ch2);
    static bool IsSpecial(uint8_t ch1, uint8_t ch2);
    static int16_t HanjaCode2HanjaPos(uint16_t hanjacode);

private:
    void OrHangulImage(int8_t* dest, int8_t* src);

    static const char* HangulFontFile;
    static const char* EngFontFile;
    static const char* HanjaFontFile;
    static const char* SpcFontFile;

    static int8_t IndexHF[3][32];
    static int8_t DLast[22];
    static int8_t DMiddle[20 * 2];
    static int8_t DFirst[22 * 2];
};

extern HangulManager* HanMan;

} // namespace jk

#endif // JKHANGULMANAGER_H
