#ifndef APPS_VECTORPRESAPP_H
#define APPS_VECTORPRESAPP_H

#include <JKApplication.h>
#include <JKVectorFont.h>
#include <memory>

namespace jk {

class VectorPresApp : public JKApplication {
public:
    VectorPresApp();
    ~VectorPresApp() override;

protected:
    void OnInit() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace jk

#endif // APPS_VECTORPRESAPP_H
