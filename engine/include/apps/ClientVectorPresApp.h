#ifndef APPS_CLIENTVECTORPRESAPP_H
#define APPS_CLIENTVECTORPRESAPP_H

#include <client/JKClientApplication.h>
#include <JKVectorFont.h>
#include <memory>

namespace jk {

// Separate-process Vector Presentation client. Reuses the shared
// PresentWindow (chrome-less, dock-filled under the root window) and pumps
// the same 500 ms timer the single-process VectorPresApp uses.
class ClientVectorPresApp : public JKClientApplication {
public:
    ClientVectorPresApp();
    ~ClientVectorPresApp() override;

protected:
    void OnInit() override;

private:
    std::unique_ptr<JKVectorFont> vfont_;
};

} // namespace jk

#endif // APPS_CLIENTVECTORPRESAPP_H