#ifndef APPS_CLIENTVECTORFONTAPP_H
#define APPS_CLIENTVECTORFONTAPP_H

#include <client/JKClientApplication.h>
#include <JKVectorFont.h>
#include <memory>

namespace jk {

// Separate-process Vector Font (KSSM glyph viewer) client. Reuses the shared
// VectorFontWindow (chrome-less, dock-filled under the root window) so the
// arrow-key glyph resizing behaves exactly like the single-process app.
class ClientVectorFontApp : public JKClientApplication {
public:
    ClientVectorFontApp();
    ~ClientVectorFontApp() override;

protected:
    void OnInit() override;

private:
    std::unique_ptr<JKVectorFont> vfont_;
};

} // namespace jk

#endif // APPS_CLIENTVECTORFONTAPP_H