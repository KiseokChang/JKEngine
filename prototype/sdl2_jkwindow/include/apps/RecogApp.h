#ifndef APPS_RECOGAPP_H
#define APPS_RECOGAPP_H

#include <JKApplication.h>
#include <memory>

namespace jk {

class RecogApp : public JKApplication {
public:
    RecogApp();
    ~RecogApp() override;

protected:
    void OnInit() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace jk

#endif // APPS_RECOGAPP_H
