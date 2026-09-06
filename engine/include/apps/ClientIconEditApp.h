#ifndef APPS_CLIENTICONEDITAPP_H
#define APPS_CLIENTICONEDITAPP_H

#include <client/JKClientApplication.h>
#include <memory>

namespace jk {

class IconEditUI;

// Separate-process Icon Editor client. Renders into a server-managed surface
// via JKClientApplication instead of owning a visible SDL window. Reuses the
// single-process editor UI through IconEditUI.
class ClientIconEditApp : public JKClientApplication {
public:
    // Out-of-line: ui_ is a unique_ptr to a forward-declared pimpl type.
    ClientIconEditApp();
    ~ClientIconEditApp() override;

protected:
    void OnInit() override;

private:
    // Outlives the main window: the Save/Load/Color callbacks captured in
    // IconEditUI::Build reference its Sprite state for the app's lifetime.
    std::unique_ptr<IconEditUI> ui_;
};

} // namespace jk

#endif // APPS_CLIENTICONEDITAPP_H