#include <apps/VectorFontApp.h>

#include <apps/VectorViews.h>

#include <JKDC.h>
#include <JKEvent.h>
#include <JKStatic.h>
#include <JKWindow.h>
#include <SDL.h>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>

namespace jk {

namespace {

// Raw KSSM byte sequence used by the original VFONTWIN demo.
// "I92Run" followed by three Hangul syllables in KSSM encoding.
const char kDemoString[] = "I92Run\xb8\x77\x8b\xa1\xac\xe2";

// VectorFontWindow는 서버 모드 ClientVectorFontApp에서도 재사용하므로
// 클래스 선언이 include/apps/VectorViews.h로 옮겨갔다. 구현만 여기에 둔다.

} // anonymous namespace

// --- VectorFontWindow (선언: include/apps/VectorViews.h) ---

VectorFontWindow::VectorFontWindow(JKVectorFont* vfont) : vfont_(vfont) {
    SetBackColor(255, 255, 255);
    SetAttrFlags(WA_TITLEMOVEABLE | WA_BORDERRESIZABLE);
}

void VectorFontWindow::OnPaintClient(JKDC& dc) {
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

void VectorFontWindow::RespondMessage(const JKEvent& ev) {
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

class VectorFontApp::Impl {
public:
    std::unique_ptr<JKVectorFont> vfont;
    VectorFontWindow* mainWindow = nullptr;
};

VectorFontApp::VectorFontApp() : impl_(std::make_unique<Impl>()) {
}

VectorFontApp::~VectorFontApp() = default;

void VectorFontApp::OnInit() {
    impl_->vfont = LoadVectorAppFonts("VectorFontApp");

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