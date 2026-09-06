#include <apps/VectorPresApp.h>

#include <JKDC.h>
#include <JKEvent.h>
#include <JKWindow.h>
#include <cmath>
#include <memory>
#include <string>

namespace jk {

namespace {

constexpr double kPi = 3.14159265358979323846;

// "I92Run" followed by three Hangul syllables in KSSM encoding.
const char kPresString[] = "I92Run\xb8\x77\x8b\xa1\xac\xe2";

// EGA/VGA-style 16-color palette for the cycling text color.
void ColorFromIndex(uint8_t idx, uint8_t& r, uint8_t& g, uint8_t& b) {
    static const uint8_t pal[16][3] = {
        {0,   0,   0  },  // 0 black
        {0,   0,   128},  // 1 dark blue
        {0,   128, 0  },  // 2 dark green
        {0,   128, 128},  // 3 dark cyan
        {128, 0,   0  },  // 4 dark red
        {128, 0,   128},  // 5 dark magenta
        {128, 128, 0  },  // 6 brown
        {192, 192, 192},  // 7 light gray
        {128, 128, 128},  // 8 dark gray
        {0,   0,   255},  // 9 light blue
        {0,   255, 0  },  // 10 light green
        {0,   255, 255},  // 11 light cyan
        {255, 0,   0  },  // 12 light red
        {255, 0,   255},  // 13 light magenta
        {255, 255, 0  },  // 14 yellow
        {255, 255, 255},  // 15 white
    };
    r = pal[idx][0];
    g = pal[idx][1];
    b = pal[idx][2];
}

class PresentWindow : public JKWindow {
public:
    explicit PresentWindow(JKVectorFont* vfont) : vfont_(vfont) {
        SetBackColor(255, 255, 255);
        SetAttrFlags(WA_TITLEMOVEABLE | WA_BORDERRESIZABLE);
    }

    void OnPaintClient(JKDC& dc) override {
        JKWindow::OnPaintClient(dc);
        if (!vfont_) return;

        const JKRect client = GetScreenClientRect();
        const JKPoint center{ client.x + client.w / 2,
                              client.y + client.h / 2 };

        const int size = 4 + txtCount_ / 12;
        const int radius = 4 + txtCount_ / 4;

        vfont_->ResetCTM();
        vfont_->SetFont(JKVectorFont::Hangul, 0);
        vfont_->SetSize(size, size);
        vfont_->Rotate(txtCount_);

        uint8_t r, g, b;
        ColorFromIndex(txtColor_, r, g, b);
        dc.SetTextColor(r, g, b);

        const double rad = txtCount_ * kPi / 180.0;
        const JKPoint pos{
            center.x + static_cast<int32_t>(radius * std::cos(rad)),
            center.y + static_cast<int32_t>(radius * std::sin(rad))
        };

        vfont_->DrawString(dc, pos, kPresString);
    }

    void RespondMessage(const JKEvent& ev) override {
        if (ev.type == JKEventType::Timer) {
            txtCount_ += 10;
            if (txtCount_ >= 720) {
                txtCount_ = 0;
            }
            // Cycle through colors, skipping white (background color).
            txtColor_ = (txtColor_ + 1) % 16;
            if (txtColor_ == 15) {
                txtColor_ = 0;
            }
            return;
        }
        JKWindow::RespondMessage(ev);
    }

private:
    JKVectorFont* vfont_ = nullptr;
    int txtCount_ = 0;
    uint8_t txtColor_ = 14; // start with yellow
};

} // anonymous namespace

class VectorPresApp::Impl {
public:
    std::unique_ptr<JKVectorFont> vfont;
    PresentWindow* mainWindow = nullptr;
};

VectorPresApp::VectorPresApp() : impl_(std::make_unique<Impl>()) {
}

VectorPresApp::~VectorPresApp() = default;

void VectorPresApp::OnInit() {
    impl_->vfont = std::make_unique<JKVectorFont>(
#ifdef JKENGINE_FONT_DIR
        JKENGINE_FONT_DIR
#else
        "."
#endif
    );

    if (!impl_->vfont->LoadFont("english.vft", JKVectorFont::English, 0) ||
        !impl_->vfont->LoadFont("hanmoon.vft", JKVectorFont::Hangul, 0)) {
        std::fprintf(stderr, "VectorPresApp: failed to load one or more fonts\n");
    }

    auto main = std::make_unique<PresentWindow>(impl_->vfont.get());
    main->SetTitle("Vector Font Window - SDL2 Port");
    main->SetWindowRect(JKRect{ 0, 0, 1920, 1080 });
    impl_->mainWindow = main.get();

    SetMainWindow(std::move(main));

    // Match the original PRESWIN timer period (500 ms).
    SetTimerInterval(500);
}

} // namespace jk
