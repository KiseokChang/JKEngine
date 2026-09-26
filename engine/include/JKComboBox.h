#ifndef JKCOMBOBOX_H
#define JKCOMBOBOX_H

#include <JKControl.h>
#include <JKListBox.h>
#include <functional>
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

    void SetReadOnly(bool readOnly) { readOnly_ = readOnly; }
    bool IsReadOnly() const { return readOnly_; }

    // 선택 변경 콜백 (additive, docs/67 단 1 — 워크숍 슬롯 스트립): 팝업
    // 선택과 키보드 이동 모두에서 발화한다. 직접 SetSelectedIndex는 발화하지
    // 않는다(프로그램적 갱신 = 전환 루프 금지). 기존 사용처(콜백 미설정)는
    // 무영향.
    void SetOnSelectionChanged(std::function<void(int32_t)> cb) {
        onSelectionChanged_ = std::move(cb);
    }

    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;
    void ApplyTheme() override;  // fieldBg 재캡처 (docs/52)

private:
    std::vector<std::string> items_;
    int32_t selectedIndex_ = -1;
    bool dropped_ = false;
    bool readOnly_ = false;
    JKListBox* popup_ = nullptr;
    std::function<void(int32_t)> onSelectionChanged_;

    // 선택 확정 단일 경로 — 팝업 선택과 키보드 이동이 모두 여기를 지난다.
    void SelectItem(int32_t index);
    void ToggleDropDown();
    void CloseDropDown();
};

} // namespace jk

#endif // JKCOMBOBOX_H
