#ifndef APPS_CLIENTVECTORAPP_H
#define APPS_CLIENTVECTORAPP_H

#include <client/JKClientApplication.h>
#include <memory>

namespace jk {

// Separate-process Vector (Bezier editor) client. Builds the shared
// BuildVectorEditorUi UI (VectorView + Reset/K-Bez/Convert buttons) into a
// server-managed surface root window instead of owning a visible SDL window.
// No timer: the single-process VectorApp does not pump JKEventType::Timer.
class ClientVectorApp : public JKClientApplication {
public:
    ClientVectorApp();
    ~ClientVectorApp() override;

protected:
    void OnInit() override;
};

} // namespace jk

#endif // APPS_CLIENTVECTORAPP_H