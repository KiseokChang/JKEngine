#ifndef APPS_CLIENTTESTWINDOWAPP_H
#define APPS_CLIENTTESTWINDOWAPP_H

#include <client/JKClientApplication.h>
#include <memory>

namespace jk {

// Separate-process Test Window client. A roomy control showcase (button,
// clock, checkbox, edit, memo, listbox, combobox, message boxes, KSSM text +
// pieslice painting) rendered into a server-managed surface. This is the
// server-mode counterpart of main.cpp's cramped 250x250 TestWindow: same
// control set, but full size and generously spaced.
class ClientTestWindowApp : public JKClientApplication {
public:
    ClientTestWindowApp();
    ~ClientTestWindowApp() override;

protected:
    void OnInit() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace jk

#endif // APPS_CLIENTTESTWINDOWAPP_H