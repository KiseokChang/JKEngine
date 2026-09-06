#include <apps/RecogApp.h>

#include <JKApplication.h>
#include <JKButton.h>
#include <JKDC.h>
#include <JKEvent.h>
#include <JKListBox.h>
#include <JKStatic.h>
#include <JKWindow.h>
#include <SDL.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace jk {

namespace {

constexpr uint16_t ID_BTN_CLEAR   = 101;
constexpr uint16_t ID_BTN_RECOG   = 102;
constexpr uint16_t ID_LIST_STROKE = 201;
constexpr uint16_t ID_LIST_CHAR   = 202;

// A lightweight modern replacement for the original NEWTECH chain classes.
// We keep only the point list and a few derived metrics.
struct Stroke {
    int32_t minX = 0, minY = 0, maxX = 0, maxY = 0;
    std::vector<JKPoint> points;
    int32_t code = -1;

    void Clear() {
        points.clear();
        code = -1;
        minX = minY = maxX = maxY = 0;
    }

    void Add(int32_t x, int32_t y) {
        if (points.empty()) {
            minX = maxX = x;
            minY = maxY = y;
        } else {
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
        }
        points.push_back(JKPoint{ x, y });
    }

    bool Empty() const { return points.empty(); }

    void Normalize(int32_t left, int32_t top, int32_t size) {
        if (points.empty()) return;
        int32_t w = maxX - minX;
        int32_t h = maxY - minY;
        int32_t extent = std::max(w, h);
        if (extent == 0) extent = 1;
        for (auto& p : points) {
            p.x = left + (p.x - minX) * size / extent;
            p.y = top + (p.y - minY) * size / extent;
        }
        minX = left;
        minY = top;
        maxX = left + size;
        maxY = top + size;
    }

    void Filter(int32_t threshold) {
        if (points.empty()) return;
        std::vector<JKPoint> filtered;
        filtered.push_back(points.front());
        for (size_t i = 1; i < points.size(); ++i) {
            int32_t dx = points[i].x - filtered.back().x;
            int32_t dy = points[i].y - filtered.back().y;
            if (std::abs(dx) >= threshold || std::abs(dy) >= threshold) {
                filtered.push_back(points[i]);
            }
        }
        points = std::move(filtered);
    }

    // Very simple rule-based recognizer used by the original prototype.
    // Returns the number of retained points as the "code", matching the
    // original ProcessInput/FindStroke semantics.
    int32_t Recognize() {
        if (points.empty()) return -1;
        return static_cast<int32_t>(points.size());
    }
};

class InputBoard : public JKControl {
public:
    InputBoard(const JKRect& rect, std::function<void()> onFinishStroke,
               std::function<void()> onFinishChar)
        : onFinishStroke_(std::move(onFinishStroke)),
          onFinishChar_(std::move(onFinishChar)) {
        SetRect(rect);
        SetBackColor(255, 255, 255);
        SetFocusable(true);
    }

    void Clear() {
        current_.Clear();
        strokes_.clear();
        drawing_ = false;
    }

    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetScreenClientRect();
        dc.SetColor(backR_, backG_, backB_, 255);
        dc.FillRect(client);
        dc.SetColor(0, 0, 0, 255);
        dc.DrawRect(client);

        dc.SetColor(0, 0, 0, 255);
        for (const auto& s : strokes_) {
            DrawStroke(dc, s);
        }
        DrawStroke(dc, current_);

        JKControl::OnPaintClient(dc);
    }

    void RespondMessage(const JKEvent& ev) override {
        if (ev.type == JKEventType::MouseDown) {
            SetFocus();
            if (ev.detail == SDL_BUTTON_RIGHT) {
                if (!current_.Empty()) {
                    current_.Clear();
                    drawing_ = false;
                } else {
                    strokes_.clear();
                    if (onFinishChar_) onFinishChar_();
                }
                return;
            }
            drawing_ = true;
            current_.Clear();
            current_.Add(ev.x, ev.y);
        } else if (ev.type == JKEventType::MouseMove) {
            if (drawing_) {
                current_.Add(ev.x, ev.y);
            } else {
                JKControl::RespondMessage(ev);
            }
        } else if (ev.type == JKEventType::MouseUp) {
            if (drawing_ && !current_.Empty()) {
                strokes_.push_back(std::move(current_));
                current_.Clear();
                drawing_ = false;
                if (onFinishStroke_) onFinishStroke_();
            } else {
                JKControl::RespondMessage(ev);
            }
        } else if (ev.type == JKEventType::KeyDown) {
            if ((SDL_GetModState() & KMOD_ALT) && ev.keyCode == SDLK_x) {
                JKControl* p = GetParent();
                if (p) p->RequestClose();
                return;
            }
            JKControl::RespondMessage(ev);
        } else {
            JKControl::RespondMessage(ev);
        }
    }

    const std::vector<Stroke>& GetStrokes() const { return strokes_; }

private:
    std::vector<Stroke> strokes_;
    Stroke current_;
    bool drawing_ = false;
    std::function<void()> onFinishStroke_;
    std::function<void()> onFinishChar_;

    void DrawStroke(JKDC& dc, const Stroke& s) {
        if (s.points.size() < 2) return;
        for (size_t i = 1; i < s.points.size(); ++i) {
            dc.DrawLine(s.points[i - 1].x, s.points[i - 1].y,
                        s.points[i].x, s.points[i].y);
        }
    }
};

} // anonymous namespace

class RecogApp::Impl {
public:
    InputBoard* inputBoard = nullptr;
    JKListBox* strokeList = nullptr;
    JKListBox* charList = nullptr;

    void FinishStroke() {
        const auto& strokes = inputBoard->GetStrokes();
        if (strokes.empty()) return;
        const Stroke& s = strokes.back();
        Stroke filtered = s;
        filtered.Filter(4);
        int32_t code = filtered.Recognize();
        char buf[32];
        std::snprintf(buf, sizeof(buf), "stroke #%zu code=%d",
                      strokes.size(), code);
        strokeList->AddString(buf);
        strokeList->SetSelectedIndex(static_cast<int32_t>(strokeList->GetCount() - 1));
    }

    void FinishCharacter() {
        strokeList->Clear();
        const auto& strokes = inputBoard->GetStrokes();
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%zu strokes", strokes.size());
        charList->AddString(buf);
        inputBoard->Clear();
    }
};

RecogApp::RecogApp() : impl_(std::make_unique<Impl>()) {
}

RecogApp::~RecogApp() = default;

void RecogApp::OnInit() {
    auto main = std::make_unique<JKWindow>("Stroke Recognition - SDL2 Port");
    main->SetWindowRect(JKRect{ 0, 0, 1920, 1080 });

    auto input = std::make_unique<InputBoard>(
        JKRect{ 20, 320, 1880, 1060 },
        [this]() { impl_->FinishStroke(); },
        [this]() { impl_->FinishCharacter(); });
    impl_->inputBoard = input.get();

    auto clearBtn = std::make_unique<JKButton>(JKRect{ 20, 50, 120, 80 }, ID_BTN_CLEAR);
    clearBtn->SetText("Clear");
    clearBtn->SetOnClick([this]() {
        impl_->inputBoard->Clear();
        impl_->strokeList->Clear();
        impl_->charList->Clear();
    });

    auto recogBtn = std::make_unique<JKButton>(JKRect{ 140, 50, 260, 80 }, ID_BTN_RECOG);
    recogBtn->SetText("Recognize");
    recogBtn->SetOnClick([this]() { impl_->FinishCharacter(); });

    auto strokeLabel = std::make_unique<JKStatic>(JKRect{ 300, 50, 620, 66 }, 0);
    strokeLabel->SetText("Strokes:");
    auto strokeList = std::make_unique<JKListBox>(JKRect{ 300, 70, 620, 300 }, ID_LIST_STROKE);
    impl_->strokeList = strokeList.get();

    auto charLabel = std::make_unique<JKStatic>(JKRect{ 640, 50, 960, 66 }, 0);
    charLabel->SetText("Characters:");
    auto charList = std::make_unique<JKListBox>(JKRect{ 640, 70, 960, 300 }, ID_LIST_CHAR);
    impl_->charList = charList.get();

    main->AddControl(std::move(input));
    main->AddControl(std::move(clearBtn));
    main->AddControl(std::move(recogBtn));
    main->AddControl(std::move(strokeLabel));
    main->AddControl(std::move(strokeList));
    main->AddControl(std::move(charLabel));
    main->AddControl(std::move(charList));

    impl_->inputBoard->SetFocus();
    SetMainWindow(std::move(main));
}

} // namespace jk
