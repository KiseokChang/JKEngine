#ifndef APPS_VECTORFONTAPP_H
#define APPS_VECTORFONTAPP_H

#include <JKApplication.h>
#include <JKVectorFont.h>
#include <memory>

namespace jk {

class VectorFontApp : public JKApplication {
public:
    VectorFontApp();
    ~VectorFontApp() override;

protected:
    void OnInit() override;
    bool PreProcessMessage(const JKEvent& ev) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace jk

#endif // APPS_VECTORFONTAPP_H
