#ifndef JKCONTROL_H
#define JKCONTROL_H

#include <JKTypes.h>
#include <JKEvent.h>
#include <JKDC.h>
#include <memory>
#include <string>
#include <vector>

namespace jk {

constexpr uint32_t WA_TITLEMOVEABLE  = 0x00000001;
constexpr uint32_t WA_BORDERRESIZABLE = 0x00000002;

class JKControl {
public:
    JKControl();
    virtual ~JKControl();

    virtual void Init();
    virtual void Setup();
    virtual void Open();
    virtual void Close();

    void Show();
    void Hide();
    bool IsVisible() const;

    virtual void PaintWindow(JKDC& dc);
    void PaintClient(JKDC& dc);
    virtual void OnPaintClient(JKDC& dc);
    virtual void RespondMessage(const JKEvent& ev);

    uint32_t GetWinId() const { return winId_; }
    void SetControlId(uint16_t id) { controlId_ = id; }
    uint16_t GetControlId() const { return controlId_; }

    virtual void SetText(const std::string& text);
    virtual const std::string& GetText() const;

    void SetTextColor(uint8_t r, uint8_t g, uint8_t b);
    void GetTextColor(uint8_t& r, uint8_t& g, uint8_t& b) const;
    void SetBackColor(uint8_t r, uint8_t g, uint8_t b);
    void GetBackColor(uint8_t& r, uint8_t& g, uint8_t& b) const;

    virtual void SetRect(const JKRect& rect);
    const JKRect& GetRect() const;

    void SetClientRect(const JKRect& rect);
    const JKRect& GetClientRect() const;

    void SetAttrFlags(uint32_t flags);
    uint32_t GetAttrFlags() const;
    bool HasAttrFlag(uint32_t flag) const;

    JKRect GetScreenRect() const;
    virtual JKRect GetScreenClientRect() const;

    virtual JKControl* HitTest(int32_t screenX, int32_t screenY);

    void SetParent(JKControl* parent);
    JKControl* GetParent() const;

    void SetFocus();

    void AddControl(std::unique_ptr<JKControl> child);
    JKControl* FindControlById(uint32_t winId);
    JKControl* FindControlByControlId(uint16_t controlId);

    const std::vector<std::unique_ptr<JKControl>>& GetChildren() const;

    void RequestClose();
    bool IsCloseRequested() const;
    void RemoveClosedChildren();

protected:
    uint32_t winId_ = 0;
    uint16_t controlId_ = 0;
    JKControl* parent_ = nullptr;
    JKRect rect_;
    JKRect clientRect_;
    bool visible_ = true;
    bool closeRequested_ = false;
    uint32_t attrFlags_ = 0;
    std::string text_;
    std::vector<std::unique_ptr<JKControl>> children_;

    uint8_t textR_ = 0;
    uint8_t textG_ = 0;
    uint8_t textB_ = 0;
    uint8_t backR_ = 240;
    uint8_t backG_ = 240;
    uint8_t backB_ = 240;
};

} // namespace jk

#endif // JKCONTROL_H
