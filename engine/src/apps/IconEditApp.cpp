#include <apps/IconEditApp.h>

#include <JKApplication.h>
#include <JKButton.h>
#include <JKDC.h>
#include <JKDialog.h>
#include <JKEdit.h>
#include <JKEdit.h>
#include <JKEvent.h>
#include <JKListBox.h>
#include <JKStatic.h>
#include <JKWindow.h>
#include <SDL.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace jk {

namespace {

constexpr uint16_t ID_EDIT_FILENAME   = 101;
constexpr uint16_t ID_EDIT_IMAGENAME  = 102;
constexpr uint16_t ID_BTN_CLEAR       = 103;
constexpr uint16_t ID_BTN_SAVE        = 104;
constexpr uint16_t ID_BTN_LOAD        = 105;
constexpr uint16_t ID_BTN_COLOR       = 106;

struct Sprite {
    uint16_t width = 24;
    uint16_t height = 24;
    std::vector<uint8_t> image; // 4-byte header (w-1,h-1) + pixels
    std::vector<uint8_t> mask;  // same layout
};

struct ColorEntry {
    const char* name;
    uint8_t index;
};

constexpr ColorEntry kColors[] = {
    { "Black",       0 },
    { "White",       1 },
    { "Blue",        2 },
    { "Green",       3 },
    { "Cyan",        4 },
    { "Red",         5 },
    { "Magenta",     6 },
    { "Brown",       7 },
    { "LtBlue",      8 },
    { "LtGreen",     9 },
    { "LtCyan",     10 },
    { "LtRed",      11 },
    { "LtMagenta",  12 },
    { "Yellow",     13 },
    { "LtGray",     14 },
    { "Gray",       15 },
    { "Eraser",   0xFF },
};
constexpr size_t kColorCount = sizeof(kColors) / sizeof(kColors[0]);

// Approximate 16-color EGA/VGA palette mapped to 8-bit RGB.
constexpr uint32_t kPalette[16] = {
    0x000000, // Black
    0xFFFFFF, // White
    0x0000AA, // Blue
    0x00AA00, // Green
    0x00AAAA, // Cyan
    0xAA0000, // Red
    0xAA00AA, // Magenta
    0xAA5500, // Brown
    0x5555FF, // LtBlue
    0x55FF55, // LtGreen
    0x55FFFF, // LtCyan
    0xFF5555, // LtRed
    0xFF55FF, // LtMagenta
    0xFFFF00, // Yellow
    0xC0C0C0, // LtGray
    0x808080, // Gray
};

uint32_t PaletteColor(uint8_t idx) {
    if (idx < 16) return kPalette[idx];
    return 0xC0C0C0;
}

void UnpackColor(uint32_t c, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = static_cast<uint8_t>((c >> 16) & 0xFF);
    g = static_cast<uint8_t>((c >> 8) & 0xFF);
    b = static_cast<uint8_t>(c & 0xFF);
}

void MakeSprite(Sprite& s, uint16_t w, uint16_t h, uint8_t bgColor) {
    uint32_t size = 4 + static_cast<uint32_t>(w) * h;
    s.width = w;
    s.height = h;
    s.image.assign(size, 0);
    s.mask.assign(size, 0xFF);
    s.image[0] = static_cast<uint8_t>(w - 1);
    s.image[1] = static_cast<uint8_t>((w - 1) >> 8);
    s.image[2] = static_cast<uint8_t>(h - 1);
    s.image[3] = static_cast<uint8_t>((h - 1) >> 8);
    s.mask[0] = s.image[0];
    s.mask[1] = s.image[1];
    s.mask[2] = s.image[2];
    s.mask[3] = s.image[3];
    for (uint16_t y = 0; y < h; ++y) {
        for (uint16_t x = 0; x < w; ++x) {
            s.image[4 + y * w + x] = bgColor;
        }
    }
}

bool LoadSprite(const std::string& path, Sprite& out) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;

    uint32_t totalsize = 0;
    if (std::fread(&totalsize, sizeof(totalsize), 1, fp) != 1) {
        std::fclose(fp);
        return false;
    }
    uint32_t size = totalsize / 2;
    if (size < 4) {
        std::fclose(fp);
        return false;
    }
    uint16_t w1 = 0, h1 = 0;
    if (std::fread(&w1, sizeof(w1), 1, fp) != 1 ||
        std::fread(&h1, sizeof(h1), 1, fp) != 1) {
        std::fclose(fp);
        return false;
    }
    uint16_t w = w1 + 1;
    uint16_t h = h1 + 1;
    if (size != 4 + static_cast<uint32_t>(w) * h) {
        std::fclose(fp);
        return false;
    }

    out.width = w;
    out.height = h;
    out.image.resize(size);
    out.mask.resize(size);
    out.image[0] = static_cast<uint8_t>(w1);
    out.image[1] = static_cast<uint8_t>(w1 >> 8);
    out.image[2] = static_cast<uint8_t>(h1);
    out.image[3] = static_cast<uint8_t>(h1 >> 8);
    out.mask[0] = out.image[0];
    out.mask[1] = out.image[1];
    out.mask[2] = out.image[2];
    out.mask[3] = out.image[3];

    if (std::fread(out.image.data() + 4, 1, static_cast<size_t>(w) * h, fp) != static_cast<size_t>(w) * h) {
        std::fclose(fp);
        return false;
    }
    if (std::fread(out.mask.data(), 1, size, fp) != size) {
        std::fclose(fp);
        return false;
    }
    std::fclose(fp);
    return true;
}

bool SaveSprite(const std::string& path, const Sprite& s) {
    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) return false;
    uint32_t size = 4 + static_cast<uint32_t>(s.width) * s.height;
    uint32_t totalsize = size * 2;
    uint16_t w1 = s.width - 1;
    uint16_t h1 = s.height - 1;
    std::fwrite(&totalsize, sizeof(totalsize), 1, fp);
    std::fwrite(&w1, sizeof(w1), 1, fp);
    std::fwrite(&h1, sizeof(h1), 1, fp);
    std::fwrite(s.image.data() + 4, 1, static_cast<size_t>(s.width) * s.height, fp);
    std::fwrite(s.mask.data(), 1, size, fp);
    std::fclose(fp);
    return true;
}

std::string EnsureSprExtension(const std::string& name) {
    if (name.size() > 4 && name.substr(name.size() - 4) == ".spr") return name;
    return name + ".spr";
}

class ColorDialog : public JKDialog {
public:
    ColorDialog(const JKRect& rect, uint8_t initialColor,
                std::function<void(uint8_t)> onSelect)
        : JKDialog("Select Color"), selected_(initialColor),
          onSelect_(std::move(onSelect)) {
        SetWindowRect(rect);

        auto list = std::make_unique<JKListBox>(JKRect{ 10, 30, 280, 280 }, 0);
        list_ = list.get();
        for (size_t i = 0; i < kColorCount; ++i) {
            list_->AddString(kColors[i].name);
        }
        int32_t startIdx = FindColorIndex(initialColor);
        list_->SetSelectedIndex(startIdx);
        list_->SetOnSelect([this](int32_t idx) {
            if (idx >= 0 && static_cast<size_t>(idx) < kColorCount) {
                selected_ = kColors[idx].index;
                Close(ResultOk);
            }
        });
        AddControl(std::move(list));

        auto cancel = std::make_unique<JKButton>(JKRect{ 10, 300, 100, 330 }, 1);
        cancel->SetText("Cancel");
        cancel->SetOnClick([this]() { Close(ResultCancel); });
        AddControl(std::move(cancel));

        SetOnClose([this](int result) {
            if (result == ResultOk && onSelect_) {
                onSelect_(selected_);
            }
        });
    }

private:
    uint8_t selected_;
    JKListBox* list_ = nullptr;
    std::function<void(uint8_t)> onSelect_;

    static int32_t FindColorIndex(uint8_t idx) {
        for (size_t i = 0; i < kColorCount; ++i) {
            if (kColors[i].index == idx) return static_cast<int32_t>(i);
        }
        return 0;
    }
};

class PreviewBoard : public JKControl {
public:
    PreviewBoard(const JKRect& rect, Sprite* sprite, int32_t scale)
        : sprite_(sprite), scale_(scale) {
        SetRect(rect);
    }

    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetScreenClientRect();
        dc.SetColor(160, 160, 160, 255);
        dc.FillRect(client);

        if (!sprite_) return;
        int32_t offX = client.x + (client.w - sprite_->width * scale_) / 2;
        int32_t offY = client.y + (client.h - sprite_->height * scale_) / 2;
        for (uint16_t y = 0; y < sprite_->height; ++y) {
            for (uint16_t x = 0; x < sprite_->width; ++x) {
                uint8_t idx = sprite_->image[4 + y * sprite_->width + x];
                uint8_t mask = sprite_->mask[4 + y * sprite_->width + x];
                JKRect r{ offX + x * scale_, offY + y * scale_, scale_, scale_ };
                if (mask == 0xFF) {
                    dc.SetColor(192, 192, 192, 255);
                    dc.FillRect(r);
                } else {
                    uint32_t c = PaletteColor(idx);
                    uint8_t r_, g_, b_;
                    UnpackColor(c, r_, g_, b_);
                    dc.SetColor(r_, g_, b_, 255);
                    dc.FillRect(r);
                }
                dc.SetColor(128, 128, 128, 255);
                dc.DrawRect(r);
            }
        }
        JKControl::OnPaintClient(dc);
    }

private:
    Sprite* sprite_ = nullptr;
    int32_t scale_ = 4;
};

class PixelBoard : public JKControl {
public:
    PixelBoard(const JKRect& rect, Sprite* sprite, uint8_t* currentColor)
        : sprite_(sprite), currentColor_(currentColor) {
        SetRect(rect);
        SetFocusable(true);
        SetBackColor(64, 64, 64);
    }

    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetScreenClientRect();
        dc.SetColor(backR_, backG_, backB_, 255);
        dc.FillRect(client);

        if (!sprite_ || sprite_->width == 0 || sprite_->height == 0) {
            JKControl::OnPaintClient(dc);
            return;
        }

        int32_t zoomW = client.w / static_cast<int32_t>(sprite_->width);
        int32_t zoomH = client.h / static_cast<int32_t>(sprite_->height);
        int32_t zoom = std::min(zoomW, zoomH);
        if (zoom < 2) zoom = 2;
        zoom_ = zoom;

        int32_t boardW = static_cast<int32_t>(sprite_->width) * zoom;
        int32_t boardH = static_cast<int32_t>(sprite_->height) * zoom;
        originX_ = client.x + (client.w - boardW) / 2;
        originY_ = client.y + (client.h - boardH) / 2;

        for (uint16_t y = 0; y < sprite_->height; ++y) {
            for (uint16_t x = 0; x < sprite_->width; ++x) {
                uint8_t idx = sprite_->image[4 + y * sprite_->width + x];
                uint8_t mask = sprite_->mask[4 + y * sprite_->width + x];
                JKRect r{ originX_ + x * zoom, originY_ + y * zoom, zoom, zoom };
                if (mask == 0xFF) {
                    // Transparent: checkerboard using the editor background.
                    dc.SetColor(backR_, backG_, backB_, 255);
                    dc.FillRect(r);
                    if ((x + y) % 2 == 0) {
                        dc.SetColor(80, 80, 80, 255);
                        dc.FillRect(JKRect{ r.x, r.y, zoom / 2, zoom / 2 });
                    }
                } else {
                    uint32_t c = PaletteColor(idx);
                    uint8_t r_, g_, b_;
                    UnpackColor(c, r_, g_, b_);
                    dc.SetColor(r_, g_, b_, 255);
                    dc.FillRect(r);
                }
                dc.SetColor(128, 128, 128, 255);
                dc.DrawRect(r);
            }
        }

        JKControl::OnPaintClient(dc);
    }

    void OpenColorPicker() {
        OpenColorDialog();
    }

    void RespondMessage(const JKEvent& ev) override {
        if (ev.type == JKEventType::MouseDown) {
            SetFocus();
            if (ev.detail == SDL_BUTTON_RIGHT) {
                OpenColorDialog();
                return;
            }
            PutPixelAt(ev.x, ev.y);
            if (g_currentJKApp) g_currentJKApp->SetCapture(this);
        } else if (ev.type == JKEventType::MouseMove) {
            if (g_currentJKApp && g_currentJKApp->GetCapture() == this) {
                PutPixelAt(ev.x, ev.y);
            } else {
                JKControl::RespondMessage(ev);
            }
        } else if (ev.type == JKEventType::MouseUp) {
            if (g_currentJKApp && g_currentJKApp->GetCapture() == this) {
                g_currentJKApp->ReleaseCapture();
            } else {
                JKControl::RespondMessage(ev);
            }
        } else if (ev.type == JKEventType::KeyDown) {
            if ((SDL_GetModState() & KMOD_ALT) && ev.keyCode == SDLK_x) {
                JKControl* p = GetParent();
                if (p) p->RequestClose();
                return;
            }
            JKControl::RespondMessage(ev);
        } else {
            JKControl::RespondMessage(ev);
        }
    }

private:
    Sprite* sprite_ = nullptr;
    uint8_t* currentColor_ = nullptr;
    int32_t zoom_ = 20;
    int32_t originX_ = 0;
    int32_t originY_ = 0;
    std::unique_ptr<ColorDialog> colorDlg_;

    void PutPixelAt(int32_t sx, int32_t sy) {
        if (!sprite_) return;
        int32_t lx = sx - originX_;
        int32_t ly = sy - originY_;
        int32_t px = lx / zoom_;
        int32_t py = ly / zoom_;
        if (px < 0 || py < 0 || px >= sprite_->width || py >= sprite_->height) return;
        size_t off = 4 + static_cast<size_t>(py) * sprite_->width + px;
        if (*currentColor_ == 0xFF) {
            sprite_->image[off] = 0;
            sprite_->mask[off] = 0xFF;
        } else {
            sprite_->image[off] = *currentColor_;
            sprite_->mask[off] = 0;
        }
    }

    void OpenColorDialog() {
        if (!colorDlg_ || colorDlg_->IsCloseRequested()) {
            colorDlg_ = std::make_unique<ColorDialog>(
                JKRect{ 200, 200, 500, 500 }, *currentColor_,
                [this](uint8_t c) { *currentColor_ = c; });
        }
        colorDlg_->Show();
    }
};

} // anonymous namespace

class IconEditApp::Impl {
public:
    Sprite sprite;
    uint8_t currentColor = 1;
};

IconEditApp::IconEditApp() : impl_(std::make_unique<Impl>()) {
}

IconEditApp::~IconEditApp() = default;

void IconEditApp::OnInit() {
    auto main = std::make_unique<JKWindow>("Icon Editor - SDL2 Port");
    main->SetWindowRect(JKRect{ 0, 0, 1920, 1080 });

    MakeSprite(impl_->sprite, 24, 24, 14);
    impl_->currentColor = 1; // White

    auto preview = std::make_unique<PreviewBoard>(JKRect{ 20, 20, 120, 120 }, &impl_->sprite, 4);

    auto labelFile = std::make_unique<JKStatic>(JKRect{ 150, 20, 200, 36 }, 0);
    labelFile->SetText("File:");
    auto editFile = std::make_unique<JKEdit>(JKRect{ 260, 20, 500, 40 }, ID_EDIT_FILENAME, 64);
    editFile->SetText("NONAME");

    auto labelName = std::make_unique<JKStatic>(JKRect{ 150, 55, 200, 71 }, 0);
    labelName->SetText("Name:");
    auto editName = std::make_unique<JKEdit>(JKRect{ 260, 55, 500, 75 }, ID_EDIT_IMAGENAME, 64);
    editName->SetText("Image_Noname");

    auto colorBtn = std::make_unique<JKButton>(JKRect{ 900, 55, 980, 85 }, ID_BTN_COLOR);
    colorBtn->SetText("Color");

    auto clearBtn = std::make_unique<JKButton>(JKRect{ 900, 20, 980, 50 }, ID_BTN_CLEAR);
    clearBtn->SetText("Clear");
    auto saveBtn = std::make_unique<JKButton>(JKRect{ 990, 20, 1070, 50 }, ID_BTN_SAVE);
    saveBtn->SetText("Save");
    auto loadBtn = std::make_unique<JKButton>(JKRect{ 1080, 20, 1160, 50 }, ID_BTN_LOAD);
    loadBtn->SetText("Load");

    JKEdit* fileEditPtr = editFile.get();

    clearBtn->SetOnClick([this]() {
        MakeSprite(impl_->sprite, impl_->sprite.width, impl_->sprite.height, 14);
    });
    saveBtn->SetOnClick([fileEditPtr, this]() {
        std::string name = fileEditPtr->GetText();
        if (name.empty()) name = "NONAME";
        SaveSprite(EnsureSprExtension(name), impl_->sprite);
    });
    loadBtn->SetOnClick([fileEditPtr, this]() {
        std::string name = fileEditPtr->GetText();
        if (name.empty()) name = "NONAME";
        LoadSprite(EnsureSprExtension(name), impl_->sprite);
    });

    auto pixelBoard = std::make_unique<PixelBoard>(JKRect{ 20, 120, 1900, 1060 }, &impl_->sprite, &impl_->currentColor);
    PixelBoard* pixelBoardPtr = pixelBoard.get();
    colorBtn->SetOnClick([pixelBoardPtr]() { pixelBoardPtr->OpenColorPicker(); });

    main->AddControl(std::move(preview));
    main->AddControl(std::move(labelFile));
    main->AddControl(std::move(editFile));
    main->AddControl(std::move(labelName));
    main->AddControl(std::move(editName));
    main->AddControl(std::move(colorBtn));
    main->AddControl(std::move(clearBtn));
    main->AddControl(std::move(saveBtn));
    main->AddControl(std::move(loadBtn));
    main->AddControl(std::move(pixelBoard));

    pixelBoardPtr->SetFocus();
    SetMainWindow(std::move(main));
}

} // namespace jk
