#include <apps/PcxApp.h>

#include <JKWindow.h>
#include <JKStatic.h>
#include <JKDC.h>
#include <JKApplication.h>

#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

namespace jk {

namespace {

#pragma pack(push, 1)
struct PcxHeader {
    uint8_t manufacturer;
    uint8_t version;
    uint8_t encoding;
    uint8_t bitsPerPixel;
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
    uint16_t horizDPI;
    uint16_t vertDPI;
    uint8_t egaPalette[48];
    uint8_t reserved1;
    uint8_t numPlanes;
    uint16_t bytesPerLine;
    uint16_t paletteInfo;
    uint16_t hScreenSize;
    uint16_t vScreenSize;
    uint8_t filler[54];
};
#pragma pack(pop)

static_assert(sizeof(PcxHeader) == 128, "PCX header size mismatch");

bool ReadPcxRleLine(FILE* fp, uint8_t* out, int bytes) {
    int n = 0;
    while (n < bytes) {
        int c = fgetc(fp);
        if (c == EOF) return false;
        uint8_t b = static_cast<uint8_t>(c);
        if ((b & 0xC0) == 0xC0) {
            int count = b & 0x3F;
            c = fgetc(fp);
            if (c == EOF) return false;
            uint8_t value = static_cast<uint8_t>(c);
            while (count-- > 0 && n < bytes) {
                out[n++] = value;
            }
        } else {
            out[n++] = b;
        }
    }
    return true;
}

bool SkipPcxRleLine(FILE* fp, int bytes) {
    int n = 0;
    while (n < bytes) {
        int c = fgetc(fp);
        if (c == EOF) return false;
        uint8_t b = static_cast<uint8_t>(c);
        if ((b & 0xC0) == 0xC0) {
            int count = b & 0x3F;
            c = fgetc(fp);
            if (c == EOF) return false;
            n += count;
        } else {
            ++n;
        }
    }
    return true;
}

struct PcxImage {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels; // row-major 8-bit indices
    std::vector<uint32_t> palette; // 256 RGBA entries
};

bool LoadPcx(const std::string& path, PcxImage& out) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;

    PcxHeader header{};
    if (std::fread(&header, sizeof(header), 1, fp) != 1) {
        std::fclose(fp);
        return false;
    }

    // Must be ZSoft PCX, RLE, 256-color, single plane.
    if (header.manufacturer != 0x0A || header.encoding != 1 ||
        header.bitsPerPixel != 8 || header.numPlanes != 1) {
        std::fclose(fp);
        return false;
    }

    int width = header.x2 - header.x1 + 1;
    int height = header.y2 - header.y1 + 1;
    int bytesPerLine = header.bytesPerLine;
    if (width <= 0 || height <= 0 || bytesPerLine <= 0) {
        std::fclose(fp);
        return false;
    }

    // Read 256-color palette at end of file.
    if (std::fseek(fp, -769, SEEK_END) != 0) {
        std::fclose(fp);
        return false;
    }
    int marker = fgetc(fp);
    if (marker != 0x0C) {
        std::fclose(fp);
        return false;
    }
    out.palette.resize(256);
    for (int i = 0; i < 256; ++i) {
        int r = fgetc(fp);
        int g = fgetc(fp);
        int b = fgetc(fp);
        if (r == EOF || g == EOF || b == EOF) {
            std::fclose(fp);
            return false;
        }
        out.palette[i] = (0xFFu << 24) |
                         ((static_cast<uint32_t>(r & 0xFF) << 16)) |
                         ((static_cast<uint32_t>(g & 0xFF) << 8)) |
                         (static_cast<uint32_t>(b & 0xFF));
    }

    out.width = width;
    out.height = height;
    out.pixels.resize(static_cast<size_t>(width) * height);

    std::fseek(fp, sizeof(header), SEEK_SET);
    std::vector<uint8_t> line(bytesPerLine);
    for (int y = 0; y < height; ++y) {
        if (!ReadPcxRleLine(fp, line.data(), bytesPerLine)) {
            std::fclose(fp);
            return false;
        }
        for (int x = 0; x < width; ++x) {
            out.pixels[static_cast<size_t>(y) * width + x] = line[x];
        }
    }

    std::fclose(fp);
    return true;
}

class PcxViewControl : public JKControl {
public:
    explicit PcxViewControl(const PcxImage& image) : image_(image) {}

    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetScreenClientRect();
        dc.SetColor(0, 0, 0, 255);
        dc.FillRect(client);
        if (image_.pixels.empty() || image_.width <= 0 || image_.height <= 0) return;

        // Center the image in the client area, preserving aspect ratio.
        float fit = std::min(static_cast<float>(client.w) / image_.width,
                             static_cast<float>(client.h) / image_.height);
        int drawW = static_cast<int>(image_.width * fit);
        int drawH = static_cast<int>(image_.height * fit);
        int offX = client.x + (client.w - drawW) / 2;
        int offY = client.y + (client.h - drawH) / 2;

        JKRenderBackend* backend = dc.GetBackend();
        if (!backend) return;

        // Render pixel-by-pixel. A future optimization could upload as a texture.
        for (int y = 0; y < drawH; ++y) {
            int srcY = static_cast<int>((y + 0.5f) / fit);
            if (srcY < 0) srcY = 0;
            if (srcY >= image_.height) srcY = image_.height - 1;
            for (int x = 0; x < drawW; ++x) {
                int srcX = static_cast<int>((x + 0.5f) / fit);
                if (srcX < 0) srcX = 0;
                if (srcX >= image_.width) srcX = image_.width - 1;
                uint8_t idx = image_.pixels[static_cast<size_t>(srcY) * image_.width + srcX];
                uint32_t c = image_.palette[idx];
                uint8_t r = static_cast<uint8_t>((c >> 16) & 0xFF);
                uint8_t g = static_cast<uint8_t>((c >> 8) & 0xFF);
                uint8_t b = static_cast<uint8_t>(c & 0xFF);
                dc.SetColor(r, g, b, 255);
                dc.DrawPixel(offX + x, offY + y);
            }
        }
    }

private:
    const PcxImage& image_;
};

} // anonymous namespace

PcxApp::PcxApp(const std::string& filePath) : filePath_(filePath) {
}

void PcxApp::OnInit() {
    static PcxImage sImage;
    bool loaded = LoadPcx(filePath_, sImage);

    auto main = std::make_unique<JKWindow>("PCX Viewer - SDL2 Port");
    main->SetWindowRect(JKRect{ 0, 0, 1920, 1080 });

    if (!loaded) {
        auto msg = std::make_unique<JKStatic>(JKRect{ 0, 0, 1920, 1080 }, 0);
        msg->SetText("Failed to load PCX file.");
        msg->SetBackColor(0, 0, 0);
        msg->SetTextColor(255, 255, 255);
        main->AddControl(std::move(msg));
    } else {
        auto view = std::make_unique<PcxViewControl>(sImage);
        view->SetRect(JKRect{ 0, 0, 1920, 1080 });
        main->AddControl(std::move(view));

        char info[256];
        std::snprintf(info, sizeof(info), "%s  (%dx%d)",
                      filePath_.c_str(), sImage.width, sImage.height);
        auto label = std::make_unique<JKStatic>(JKRect{ 0, 0, 1920, 24 }, 0);
        label->SetText(info);
        label->SetBackColor(0, 0, 128);
        label->SetTextColor(255, 255, 255);
        main->AddControl(std::move(label));
    }

    SetMainWindow(std::move(main));
}

} // namespace jk
