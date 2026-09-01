#ifndef APPS_APPLAUNCHERITEM_H
#define APPS_APPLAUNCHERITEM_H

#include <JKControl.h>
#include <JKDC.h>
#include <JKTypes.h>
#include <functional>
#include <string>

namespace jk {

// Desktop-style launcher icon: raised 3D box with a generated icon and a text
// label. Invokes a callback when clicked.
class AppLauncherItem : public JKControl {
public:
    AppLauncherItem(const JKRect& rect, const std::string& label,
                    std::function<void()> onClick);

    void SetIconKey(const std::string& key) { iconKey_ = key; }
    const std::string& GetIconKey() const { return iconKey_; }

    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;
    JKPoint MeasureContent() const override;

private:
    std::string label_;
    std::string iconKey_;
    std::function<void()> onClick_;
    bool pressed_ = false;
};

// Generate simple 32x32 RGBA icons and register them in the resource cache.
std::vector<uint8_t> CreateMineLauncherIcon();
std::vector<uint8_t> CreateTetrisLauncherIcon();

} // namespace jk

#endif // APPS_APPLAUNCHERITEM_H
