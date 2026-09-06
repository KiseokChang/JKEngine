#ifndef APPS_PCXAPP_H
#define APPS_PCXAPP_H

#include <JKApplication.h>
#include <string>

namespace jk {

class PcxApp : public JKApplication {
public:
    explicit PcxApp(const std::string& filePath);

protected:
    void OnInit() override;

private:
    std::string filePath_;
};

} // namespace jk

#endif // APPS_PCXAPP_H
