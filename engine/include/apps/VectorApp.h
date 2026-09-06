#ifndef APPS_VECTORAPP_H
#define APPS_VECTORAPP_H

#include <JKApplication.h>

namespace jk {

class VectorApp : public JKApplication {
public:
    VectorApp();

protected:
    void OnInit() override;
};

} // namespace jk

#endif // APPS_VECTORAPP_H
