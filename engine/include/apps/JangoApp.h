#ifndef JANGOAPP_H_SDL2
#define JANGOAPP_H_SDL2

#include <JKApplication.h>
#include <memory>
#include <string>

namespace jk {

class JangoApp : public JKApplication {
public:
    JangoApp();
    ~JangoApp() override;

protected:
    void OnInit() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace jk

#endif // JANGOAPP_H_SDL2
