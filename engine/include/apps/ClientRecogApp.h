#ifndef APPS_CLIENTRECOGAPP_H
#define APPS_CLIENTRECOGAPP_H

#include <client/JKClientApplication.h>
#include <memory>

namespace jk {

class RecogUI;

// Separate-process Stroke Recognition client. Renders into a server-managed
// surface via JKClientApplication instead of owning a visible SDL window.
// Reuses the single-process UI through RecogUI.
class ClientRecogApp : public JKClientApplication {
public:
    // Out-of-line: ui_ is a unique_ptr to a forward-declared pimpl type.
    ClientRecogApp();
    ~ClientRecogApp() override;

protected:
    void OnInit() override;

private:
    // Outlives the main window: the input board / list callbacks captured in
    // RecogUI::Build reference its state for the app's whole lifetime.
    std::unique_ptr<RecogUI> ui_;
};

} // namespace jk

#endif // APPS_CLIENTRECOGAPP_H