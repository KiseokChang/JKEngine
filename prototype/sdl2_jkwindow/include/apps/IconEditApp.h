#ifndef APPS_ICONEDITAPP_H
#define APPS_ICONEDITAPP_H

#include <JKApplication.h>
#include <memory>

namespace jk {

class IconEditApp : public JKApplication {
public:
    IconEditApp();
    ~IconEditApp() override;

protected:
    void OnInit() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace jk

#endif // APPS_ICONEDITAPP_H
