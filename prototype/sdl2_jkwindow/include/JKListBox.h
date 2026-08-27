#ifndef JKLISTBOX_H
#define JKLISTBOX_H

#include <JKControl.h>
#include <JKScrollBar.h>
#include <functional>
#include <string>
#include <vector>
#include <memory>

namespace jk {

class JKListBox : public JKControl {
public:
    JKListBox();
    explicit JKListBox(const JKRect& rect, uint16_t controlId = 0);

    void AddString(const std::string& str);
    void DeleteString(size_t index);
    void Clear();
    size_t GetCount() const;
    const std::string& GetString(size_t index) const;

    int32_t GetSelectedIndex() const { return selectedIndex_; }
    void SetSelectedIndex(int32_t index);

    void SetOnSelect(std::function<void(int32_t)> cb) { onSelect_ = std::move(cb); }

    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;

private:
    std::vector<std::string> items_;
    int32_t selectedIndex_ = -1;
    int32_t topIndex_ = 0;
    int32_t itemHeight_ = 16;

    JKScrollBar* vScroll_ = nullptr;
    std::function<void(int32_t)> onSelect_;

    void UpdateScrollRange();
};

} // namespace jk

#endif // JKLISTBOX_H
