#ifndef JKMENU_H
#define JKMENU_H

#include <JKControl.h>
#include <JKWindow.h>
#include <JKListBox.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace jk {

struct JKMenuItem {
    std::string label;
    uint16_t id = 0;
    std::function<void()> onClick;
};

class JKMenu : public JKControl {
public:
    JKMenu();
    explicit JKMenu(const JKRect& rect, uint16_t controlId = 0);

    void AddMenu(const std::string& label, const std::vector<JKMenuItem>& items);

    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;

private:
    struct MenuDef {
        std::string label;
        std::vector<JKMenuItem> items;
    };

    std::vector<MenuDef> menus_;
    int32_t activeIndex_ = -1;

    class Popup : public JKWindow {
    public:
        Popup(const JKRect& rect, const std::vector<JKMenuItem>& items,
              std::function<void()> onClose);

        void SetRect(const JKRect& rect) override;
        void PaintWindow(JKDC& dc) override;
        void OnPaintClient(JKDC& dc) override;
        void RespondMessage(const JKEvent& ev) override;

    private:
        std::vector<JKMenuItem> items_;
        std::function<void()> onClose_;
        JKListBox* list_ = nullptr;
    };

    std::unique_ptr<Popup> popup_;

    int32_t HitItem(int32_t x) const;
    int32_t ItemX(int32_t index) const;
    int32_t ItemWidth(int32_t index) const;
    void OpenPopup(int32_t index);
    void ClosePopup();
};

} // namespace jk

#endif // JKMENU_H
