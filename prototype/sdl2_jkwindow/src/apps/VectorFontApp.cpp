#include <apps/VectorFontApp.h>

#include <JKDC.h>
#include <JKEvent.h>
#include <JKStatic.h>
#include <JKWindow.h>
#include <SDL.h>
#include <functional>
#include <memory>
#include <string>

namespace jk {

namespace {

// Raw KSSM byte sequence used by the original VFONTWIN demo.
// "I92Run" followed by three Hangul syllables in KSSM encoding.
const char kDemoString[] = "I92Run\xb8\x77\x8b\xa1\xac\xe2";

class VectorFontWindow : public JKWindow {
public:
    VectorFontWindow(JKVectorFont* vfont) : vfont_(vfont) {
        SetBackColor(255, 255, 255);
        SetAttrFlags(WA_TITLEMOVEABLE | WA_BORDERRESIZABLE);
    }

    void OnPaintClient(JKDC& dc) override {
        JKWindow::OnPaintClient(dc);
        if (!vfont_) return;

        const JKRect client = GetScreenClientRect();
        vfont_->ResetCTM();
        vfont_->SetFont(JKVectorFont::Hangul, 0);
        vfont_->SetSize(fontSize_.x, fontSize_.y);
        dc.SetTextColor(0, 0, 0);
        vfont_->DrawString(dc, client.x + 20, client.y + 50, kDemoString);

        // Show current size.
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Size: %d x %d", fontSize_.x, fontSize_.y);
        dc.SetTextColor(128, 128, 128);
        dc.TextOut(JKPoint{ client.x + 20, client.y + 20 }, buf);
    }

    void RespondMessage(const JKEvent& ev) override {
        if (ev.type == JKEventType::KeyDown) {
            bool changed = false;
            switch (ev.keyCode) {
                case SDLK_LEFT:
                    if (fontSize_.x > 4) { fontSize_.x -= 4; changed = true; }
                    break;
                case SDLK_RIGHT:
                    if (fontSize_.x < 1000) { fontSize_.x += 4; changed = true; }
                    break;
                case SDLK_UP:
                    if (fontSize_.y < 1000) { fontSize_.y += 4; changed = true; }
                    break;
                case SDLK_DOWN:
                    if (fontSize_.y > 4) { fontSize_.y -= 4; changed = true; }
                    break;
                default:
                    break;
            }
            if (changed) {
                return;
            }
        }
        if (ev.type == JKEventType::MouseDown) {
            SetFocus();
        }
        JKWindow::RespondMessage(ev);
    }

    JKPoint fontSize_{ 32, 32 };
    JKVectorFont* vfont_ = nullptr;
};

} // anonymous namespace

class VectorFontApp::Impl {
public:
    std::unique_ptr<JKVectorFont> vfont;
    VectorFontWindow* mainWindow = nullptr;
};

VectorFontApp::VectorFontApp() : impl_(std::make_unique<Impl>()) {
}

VectorFontApp::~VectorFontApp() = default;

void VectorFontApp::OnInit() {
    impl_->vfont = std::make_unique<JKVectorFont>(
#ifdef JKENGINE_FONT_DIR
        JKENGINE_FONT_DIR
#else
        "."
#endif
    );

    if (!impl_->vfont->LoadFont("english.vft", JKVectorFont::English, 0) ||
        !impl_->vfont->LoadFont("hanmoon.vft", JKVectorFont::Hangul, 0)) {
        // Font load failure is non-fatal: the window will just not draw text.
        std::fprintf(stderr, "VectorFontApp: failed to load one or more fonts\n");
    }

    auto main = std::make_unique<VectorFontWindow>(impl_->vfont.get());
    main->SetTitle("Vector Font Window - SDL2 Port");
    main->SetWindowRect(JKRect{ 0, 0, 1920, 1080 });
    impl_->mainWindow = main.get();

    SetMainWindow(std::move(main));
}

bool VectorFontApp::PreProcessMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::KeyDown &&
        (SDL_GetModState() & KMOD_ALT) && ev.keyCode == SDLK_x) {
        if (GetMainWindow()) {
            GetMainWindow()->RequestClose();
        }
        return false; // exit the demo
    }
    return JKApplication::PreProcessMessage(ev);
}

} // namespace jk
