#include <JKVectorFont.h>

#include <wancode.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace jk {

namespace {

constexpr uint16_t FONT_FIXED    = 0;
constexpr uint16_t FONT_VARIABLE = 1;

constexpr uint16_t FONT_ENG    = 0;
constexpr uint16_t FONT_KSCODE = 100;

constexpr uint8_t MAGIC_PROP = 0xAA;
constexpr uint8_t MOVETO     = 0x10;
constexpr uint8_t LINETO     = 0x20;
constexpr uint8_t CURVETO    = 0x40;
constexpr uint8_t ENDCHAR    = 0x80;

constexpr int kMaxFontId = 4;
constexpr double kPi = 3.14159265358979323846;

#pragma pack(push, 1)
struct FontInfoHeader {
    char     name[32];
    uint16_t version;
    uint16_t charSet;
    uint16_t codingSystem;
    uint16_t pitch;
    uint16_t startCode;
    uint16_t charNum;
    uint16_t rowNum;
    uint16_t emSize;
    uint16_t defaultWidth;
    int32_t  mapOffset;
    int32_t  widthTblOffset;
    int32_t  glyphOffset;
    char     fileName[128];
};

static_assert(sizeof(FontInfoHeader) == 190,
              "VFT FONTINFO header must be 190 bytes");

struct MapEntry {
    int16_t glyphSize;
    int32_t offset;
};

static_assert(sizeof(MapEntry) == 6, "VFT MAPENTRY must be 6 bytes");
#pragma pack(pop)

struct CTM {
    double a  = 1.0;
    double b  = 0.0;
    double c  = 0.0;
    double d  = 1.0;
    double tx = 0.0;
    double ty = 0.0;
};

struct Glyph {
    int16_t width = 0;
    int16_t height = 0;
    int16_t xOffset = 0;
    int16_t yOffset = 0;
    // Each contour is a list of points in design units. Curves were flattened
    // into line segments at parse time to keep rasterization simple.
    std::vector<std::vector<JKPoint>> contours;
};

struct FontSlot {
    std::string fileName;
    FontInfoHeader header;
    std::vector<int16_t> widthTable;
    std::vector<uint8_t> fileData;   // entire font file loaded contiguously
};

int16_t ReadWord(const uint8_t*& p) {
    uint16_t v = static_cast<uint16_t>((p[0] << 8) | p[1]);
    p += 2;
    return static_cast<int16_t>(v);
}

} // anonymous namespace

class JKVectorFont::Impl {
public:
    explicit Impl(const std::string& fontDir) : fontDir_(fontDir) {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < kMaxFontId; ++j) {
                fonts_[i][j] = nullptr;
            }
        }
        ResetCTM();
        SetSize(1, 1);
    }

    ~Impl() {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < kMaxFontId; ++j) {
                delete fonts_[i][j];
            }
        }
    }

    bool LoadFont(const std::string& fileName, CodeArea area, int fontId) {
        if (area < 0 || area > 3 || fontId < 0 || fontId >= kMaxFontId) {
            return false;
        }
        std::string path = fontDir_ + "/" + fileName;
        FILE* fp = std::fopen(path.c_str(), "rb");
        if (!fp) return false;

        auto slot = std::make_unique<FontSlot>();
        if (std::fread(&slot->header, sizeof(FontInfoHeader), 1, fp) != 1) {
            std::fclose(fp);
            return false;
        }
        slot->fileName = path;

        if (slot->header.pitch == FONT_VARIABLE) {
            size_t count = slot->header.charNum;
            slot->widthTable.resize(count);
            std::fseek(fp, slot->header.widthTblOffset, SEEK_SET);
            if (std::fread(slot->widthTable.data(), sizeof(int16_t), count, fp) != count) {
                std::fclose(fp);
                return false;
            }
        }

        // Load the entire file so map entries and glyph data are addressable
        // by their absolute file offsets.
        std::fseek(fp, 0, SEEK_END);
        long fileSize = std::ftell(fp);
        if (fileSize < static_cast<long>(sizeof(FontInfoHeader))) {
            std::fclose(fp);
            return false;
        }
        std::rewind(fp);
        slot->fileData.resize(static_cast<size_t>(fileSize));
        if (std::fread(slot->fileData.data(), 1, slot->fileData.size(), fp) !=
            slot->fileData.size()) {
            std::fclose(fp);
            return false;
        }

        std::fclose(fp);

        delete fonts_[area][fontId];
        fonts_[area][fontId] = slot.release();
        return true;
    }

    void SetFont(CodeArea area, int fontId) {
        if (area >= 0 && area <= 3 && fontId >= 0 && fontId < kMaxFontId) {
            curArea_ = area;
            curFontId_ = fontId;
        }
    }

    void ResetCTM() {
        ctm_.a  = 1.0; ctm_.b  = 0.0;
        ctm_.c  = 0.0; ctm_.d  = 1.0;
        ctm_.tx = 0.0; ctm_.ty = 0.0;
    }

    void SetSize(int x, int y) {
        sizeX_ = x;
        sizeY_ = y;
    }

    void Rotate(int angle) {
        angle %= 360;
        if (angle < 0) angle += 360;
        if (angle == 0) return;
        double rad = angle * kPi / 180.0;
        double s = std::sin(rad);
        double c = std::cos(rad);

        double a = ctm_.a * c - ctm_.b * s;
        double b = ctm_.a * s + ctm_.b * c;
        double cc = ctm_.c * c - ctm_.d * s;
        double d = ctm_.c * s + ctm_.d * c;
        double tx = ctm_.tx * c - ctm_.ty * s;
        double ty = ctm_.tx * s + ctm_.ty * c;
        ctm_.a = a; ctm_.b = b; ctm_.c = cc; ctm_.d = d;
        ctm_.tx = tx; ctm_.ty = ty;
    }

    void Slant(int angle) {
        angle %= 360;
        if (angle >= 90 || angle <= -90) return;
        if (angle < 0) angle += 360;
        if (angle == 0) return;
        double t = std::tan(angle * kPi / 180.0);
        ctm_.a -= ctm_.c * t;
        ctm_.b -= ctm_.d * t;
        ctm_.tx -= ctm_.ty * t;
    }

    void DrawString(JKDC& dc, JKPoint p, const std::string& str) {
        size_t i = 0;
        while (i < str.size()) {
            uint8_t c = static_cast<uint8_t>(str[i]);
            uint16_t code = 0;
            size_t advance = 1;
            if (c < 0x80) {
                code = c;
            } else {
                if (i + 1 >= str.size()) break;
                code = (static_cast<uint16_t>(c) << 8) |
                       static_cast<uint8_t>(str[i + 1]);
                advance = 2;
            }

            int16_t xInc = 0, yInc = 0;
            DrawChar(dc, p, code, xInc, yInc);
            p.x += xInc;
            p.y += yInc;
            i += advance;
        }
    }

    int GetCharWidth(uint16_t ch) {
        FontSlot* fi = ResolveFont(ch);
        if (!fi) return 0;

        int index = 0;
        if (!ComputeIndex(*fi, ch, index)) return 0;

        if (fi->header.pitch == FONT_FIXED) {
            return fi->header.defaultWidth * sizeX_ / fi->header.emSize;
        }
        if (index >= 0 && index < static_cast<int>(fi->widthTable.size())) {
            return fi->widthTable[index] * sizeX_ / fi->header.emSize;
        }
        return 0;
    }

    JKPoint MeasureString(const std::string& str) {
        JKPoint size{ 0, sizeY_ };
        size_t i = 0;
        while (i < str.size()) {
            uint8_t c = static_cast<uint8_t>(str[i]);
            uint16_t code = 0;
            size_t advance = 1;
            if (c < 0x80) {
                code = c;
            } else {
                if (i + 1 >= str.size()) break;
                code = (static_cast<uint16_t>(c) << 8) |
                       static_cast<uint8_t>(str[i + 1]);
                advance = 2;
            }
            size.x += GetCharWidth(code);
            i += advance;
        }
        return size;
    }

private:
    FontSlot* fonts_[4][kMaxFontId];
    CodeArea curArea_ = Hangul;
    int curFontId_ = 0;
    std::string fontDir_;
    CTM ctm_;
    int sizeX_ = 1;
    int sizeY_ = 1;

    FontSlot* ResolveFont(uint16_t ch) {
        if (ch < 0x80) {
            return fonts_[English][curFontId_];
        } else if (ch >= 0x8440 && ch <= 0xd3fe) {
            return fonts_[Hangul][curFontId_];
        } else if (ch >= 0xd931 && ch <= 0xdefe) {
            return fonts_[Special][curFontId_];
        } else if (ch >= 0xe031 && ch <= 0xf9fe) {
            return fonts_[Hanja][curFontId_];
        }
        return nullptr;
    }

    bool ComputeIndex(const FontSlot& slot, uint16_t ch, int& index) {
        const FontInfoHeader& h = slot.header;
        switch (h.codingSystem) {
            case FONT_ENG:
                if (ch < h.startCode || ch >= h.startCode + h.charNum) {
                    return false;
                }
                index = ch - h.startCode;
                return true;
            case FONT_KSCODE: {
                uint16_t ks = KSSM2KS(ch);
                if (ks == 0xffff) return false;
                int row = (ks >> 8) - h.startCode;
                if (row < 0 || row >= h.rowNum) return false;
                index = row * 94 + (ks & 0x00ff) - 0xa1;
                return true;
            }
            default:
                return false;
        }
    }

    const uint8_t* GetGlyphBytes(const FontSlot& slot, int index, size_t& size) {
        if (index < 0 || index >= slot.header.charNum) return nullptr;
        long mapOff = slot.header.mapOffset +
                        static_cast<long>(index) * sizeof(MapEntry);
        if (mapOff < 0 ||
            static_cast<size_t>(mapOff) + sizeof(MapEntry) > slot.fileData.size()) {
            return nullptr;
        }
        const uint8_t* p = slot.fileData.data() + mapOff;
        MapEntry me;
        me.glyphSize = static_cast<int16_t>(p[0] | (p[1] << 8));
        me.offset = static_cast<int32_t>(p[2] | (p[3] << 8) |
                                            (p[4] << 16) | (p[5] << 24));
        if (me.glyphSize == 0) {
            size = 0;
            return nullptr;
        }
        if (me.offset < 0 ||
            static_cast<size_t>(me.offset) + me.glyphSize > slot.fileData.size()) {
            return nullptr;
        }
        size = static_cast<size_t>(me.glyphSize);
        return slot.fileData.data() + me.offset;
    }

    void DrawChar(JKDC& dc, JKPoint p, uint16_t ch, int16_t& xInc, int16_t& yInc) {
        xInc = yInc = 0;
        FontSlot* slot = ResolveFont(ch);
        if (!slot) return;

        int index = 0;
        if (!ComputeIndex(*slot, ch, index)) return;

        size_t glyphSize = 0;
        const uint8_t* data = GetGlyphBytes(*slot, index, glyphSize);
        if (!data || glyphSize == 0) {
            xInc = CharAdvance(*slot, index);
            return;
        }

        Glyph glyph;
        if (!ParseGlyph(data, glyphSize, slot->header.emSize, glyph)) {
            xInc = CharAdvance(*slot, index);
            return;
        }

        p.x += glyph.xOffset;
        p.y -= glyph.yOffset + glyph.height;
        p.y += sizeY_;

        RenderGlyph(dc, p, glyph);
        xInc = CharAdvance(*slot, index);
    }

    int CharAdvance(const FontSlot& slot, int index) {
        int w = slot.header.defaultWidth;
        if (slot.header.pitch == FONT_VARIABLE &&
            index >= 0 && index < static_cast<int>(slot.widthTable.size())) {
            w = slot.widthTable[index];
        }
        double dx = static_cast<double>(w) * sizeX_ / slot.header.emSize;
        double dy = 0.0;
        double ox = dx * ctm_.a + dy * ctm_.b + ctm_.tx;
        double oy = -(dx * ctm_.c + dy * ctm_.d + ctm_.ty);
        return static_cast<int16_t>(static_cast<int32_t>(ox + 0.5));
    }

    bool ParseGlyph(const uint8_t* data, size_t size, int emSize, Glyph& out) {
        const uint8_t* p = data;
        const uint8_t* end = data + size;

        int32_t minX = 0, minY = 0, maxX = emSize, maxY = emSize;
        if (p < end && *p == MAGIC_PROP) {
            ++p;
            if (p + 8 > end) return false;
            minX = ReadWord(p) - 4;
            minY = ReadWord(p) - 4;
            maxX = ReadWord(p) + 4;
            maxY = ReadWord(p) + 4;
        }

        std::vector<std::vector<JKPoint>> contours;
        std::vector<JKPoint> current;
        bool firstMove = true;
        int32_t startX = 0, startY = 0;
        int32_t prevX = 0, prevY = 0;

        while (p < end) {
            uint8_t op = *p++;
            if (op == ENDCHAR) {
                if (!current.empty()) {
                    current.push_back(JKPoint{ startX, startY });
                    contours.push_back(std::move(current));
                    current.clear();
                }
                break;
            }
            if (op == MOVETO) {
                if (!firstMove && !current.empty()) {
                    current.push_back(JKPoint{ startX, startY });
                    contours.push_back(std::move(current));
                    current.clear();
                }
                if (p + 4 > end) return false;
                int32_t x = ReadWord(p);
                int32_t y = ReadWord(p);
                startX = prevX = x;
                startY = prevY = y;
                current.push_back(JKPoint{ x, y });
                firstMove = false;
            } else if (op == LINETO) {
                if (p + 4 > end) return false;
                int32_t x = ReadWord(p);
                int32_t y = ReadWord(p);
                current.push_back(JKPoint{ x, y });
                prevX = x;
                prevY = y;
            } else if (op == CURVETO) {
                if (p + 12 > end) return false;
                int32_t endX = ReadWord(p);
                int32_t endY = ReadWord(p);
                int32_t cp1X = ReadWord(p);
                int32_t cp1Y = ReadWord(p);
                int32_t cp2X = ReadWord(p);
                int32_t cp2Y = ReadWord(p);
                FlattenCubic(prevX, prevY, cp1X, cp1Y, cp2X, cp2Y, endX, endY,
                             current);
                current.push_back(JKPoint{ endX, endY });
                prevX = endX;
                prevY = endY;
            } else {
                return false;
            }
        }

        if (contours.empty()) {
            out.width = 0;
            out.height = 0;
            out.xOffset = 0;
            out.yOffset = 0;
            return true;
        }

        // Compute transformed bounding box.
        double xmin = 1e9, ymin = 1e9, xmax = -1e9, ymax = -1e9;
        for (const auto& contour : contours) {
            for (const auto& pt : contour) {
                double x = pt.x, y = pt.y;
                TransformPoint(x, y, emSize);
                xmin = std::min(xmin, x);
                ymin = std::min(ymin, y);
                xmax = std::max(xmax, x);
                ymax = std::max(ymax, y);
            }
        }

        int32_t xMinPix = static_cast<int32_t>(std::floor(xmin));
        int32_t yMinPix = static_cast<int32_t>(std::floor(ymin));
        int32_t xMaxPix = static_cast<int32_t>(std::ceil(xmax));
        int32_t yMaxPix = static_cast<int32_t>(std::ceil(ymax));

        out.width  = static_cast<int16_t>(xMaxPix - xMinPix + 1);
        out.height = static_cast<int16_t>(yMaxPix - yMinPix + 1);
        out.xOffset = static_cast<int16_t>(xMinPix);
        out.yOffset = static_cast<int16_t>(yMinPix);
        out.contours = std::move(contours);

        // Shift contour points so the bitmap origin is (0,0) and flip Y so
        // design-space up maps to bitmap row 0 (matching the original rasterizer).
        for (auto& contour : out.contours) {
            for (auto& pt : contour) {
                double x = pt.x, y = pt.y;
                TransformPoint(x, y, emSize);
                pt.x = static_cast<int32_t>(x) - out.xOffset;
                pt.y = (out.yOffset + out.height - 1) - static_cast<int32_t>(y);
            }
        }

        return true;
    }

    void TransformPoint(double& x, double& y, int emSize) {
        double sx = x * static_cast<double>(sizeX_) / static_cast<double>(emSize);
        double sy = y * static_cast<double>(sizeY_) / static_cast<double>(emSize);
        double ox = sx * ctm_.a + sy * ctm_.b + ctm_.tx;
        double oy = sx * ctm_.c + sy * ctm_.d + ctm_.ty;
        x = ox;
        y = oy;
    }

    void FlattenCubic(int32_t x0, int32_t y0,
                      int32_t x1, int32_t y1,
                      int32_t x2, int32_t y2,
                      int32_t x3, int32_t y3,
                      std::vector<JKPoint>& out) {
        // Flatten with fixed number of steps based on approximate curve length.
        double len = std::sqrt(static_cast<double>((x1 - x0) * (x1 - x0) +
                                                       (y1 - y0) * (y1 - y0))) +
                     std::sqrt(static_cast<double>((x2 - x1) * (x2 - x1) +
                                                       (y2 - y1) * (y2 - y1))) +
                     std::sqrt(static_cast<double>((x3 - x2) * (x3 - x2) +
                                                       (y3 - y2) * (y3 - y2)));
        int steps = std::max(4, static_cast<int>(len / 8.0));
        for (int i = 1; i < steps; ++i) {
            double t = static_cast<double>(i) / steps;
            double u = 1.0 - t;
            double c0 = u * u * u;
            double c1 = 3.0 * u * u * t;
            double c2 = 3.0 * u * t * t;
            double c3 = t * t * t;
            int32_t x = static_cast<int32_t>(c0 * x0 + c1 * x1 + c2 * x2 + c3 * x3);
            int32_t y = static_cast<int32_t>(c0 * y0 + c1 * y1 + c2 * y2 + c3 * y3);
            out.push_back(JKPoint{ x, y });
        }
    }

    void RenderGlyph(JKDC& dc, JKPoint origin, const Glyph& glyph) {
        if (glyph.width <= 0 || glyph.height <= 0) return;

        dc.SetColor(dc.GetTextColorR(), dc.GetTextColorG(), dc.GetTextColorB(), 255);

        // Collect edges from all contours.
        struct Edge { int32_t y1, y2; double x1, x2; };
        std::vector<Edge> edges;
        for (const auto& contour : glyph.contours) {
            size_t n = contour.size();
            if (n < 2) continue;
            for (size_t i = 0; i < n; ++i) {
                const JKPoint& a = contour[i];
                const JKPoint& b = contour[(i + 1) % n];
                if (a.y == b.y) continue; // ignore horizontal edges for fill
                edges.push_back({ a.y, b.y, static_cast<double>(a.x),
                                  static_cast<double>(b.x) });
            }
        }

        for (int32_t y = 0; y < glyph.height; ++y) {
            std::vector<double> xs;
            for (const auto& e : edges) {
                int32_t y1 = e.y1;
                int32_t y2 = e.y2;
                if ((y1 <= y && y2 > y) || (y2 <= y && y1 > y)) {
                    if (y1 == y2) continue;
                    double t = static_cast<double>(y - y1) / (y2 - y1);
                    double x = e.x1 + t * (e.x2 - e.x1);
                    xs.push_back(x);
                }
            }
            if (xs.size() >= 2) {
                std::sort(xs.begin(), xs.end());
                for (size_t i = 0; i + 1 < xs.size(); i += 2) {
                    int32_t x1 = static_cast<int32_t>(std::ceil(xs[i]));
                    int32_t x2 = static_cast<int32_t>(std::floor(xs[i + 1]));
                    if (x2 >= x1) {
                        dc.HLine(origin.x + x1, origin.y + y, x2 - x1 + 1);
                    }
                }
            }
        }
    }
};

JKVectorFont::JKVectorFont(const std::string& fontDir)
    : impl_(std::make_unique<Impl>(fontDir)) {
}

JKVectorFont::~JKVectorFont() = default;

bool JKVectorFont::LoadFont(const std::string& fileName, CodeArea area, int fontId) {
    return impl_->LoadFont(fileName, area, fontId);
}

void JKVectorFont::SetFont(CodeArea area, int fontId) {
    impl_->SetFont(area, fontId);
}

void JKVectorFont::ResetCTM() {
    impl_->ResetCTM();
}

void JKVectorFont::SetSize(int x, int y) {
    impl_->SetSize(x, y);
}

void JKVectorFont::Rotate(int angle) {
    impl_->Rotate(angle);
}

void JKVectorFont::Slant(int angle) {
    impl_->Slant(angle);
}

void JKVectorFont::DrawString(JKDC& dc, JKPoint p, const std::string& str) {
    impl_->DrawString(dc, p, str);
}

int JKVectorFont::GetCharWidth(uint16_t ch) {
    return impl_->GetCharWidth(ch);
}

JKPoint JKVectorFont::MeasureString(const std::string& str) {
    return impl_->MeasureString(str);
}

} // namespace jk
