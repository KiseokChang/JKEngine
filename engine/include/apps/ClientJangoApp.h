#ifndef CLIENTJANGOAPP_H
#define CLIENTJANGOAPP_H

#include <client/JKClientApplication.h>
#include <memory>

namespace jk {

class JangoUI;

// Server-mode client module wrapper for the JANGO launcher. Renders into a
// server-managed surface and reuses the single-process JangoUI.
class ClientJangoApp : public JKClientApplication {
public:
    ClientJangoApp();
    ~ClientJangoApp() override;

protected:
    void OnInit() override;

private:
    std::unique_ptr<JangoUI> ui_;
};

} // namespace jk

#endif // CLIENTJANGOAPP_H