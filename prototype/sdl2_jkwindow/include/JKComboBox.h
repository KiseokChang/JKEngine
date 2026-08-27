#ifndef JKCOMBOBOX_H
#define JKCOMBOBOX_H

#include <JKControl.h>
#include <JKListBox.h>
#include <memory>
#include <string>
#include <vector>

namespace jk {

class JKComboBox : public JKControl {
public:
    JKComboBox();
    explicit JKComboBox(const JKRect& rect, uint16_t controlId = 0);

    void AddString(const std::string& str);
    void Clear();
    size_t GetCount() const;

    int32_t GetSelectedIndex() const { return selectedIndex_; }
    void SetSelectedIndex(int32_t index);
    const std::string& GetSelectedString() const;

    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;

private:
    std::vector<std::string> items_;
    int32_t selectedIndex_ = -1;
    bool dropped_ = false;
    JKListBox* popup_ = nullptr;

    void ToggleDropDown();
    void CloseDropDown();
};

} // namespace jk

#endif // JKCOMBOBOX_H
