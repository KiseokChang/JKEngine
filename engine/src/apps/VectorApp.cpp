#include <apps/VectorApp.h>

#include <apps/VectorViews.h>

#include <JKApplication.h>
#include <JKButton.h>
#include <JKDC.h>
#include <JKEvent.h>
#include <JKWindow.h>
#include <SDL.h>
#include <algorithm>
#include <cstring>

namespace jk {

namespace {

constexpr uint16_t ID_RESET   = 101;
constexpr uint16_t ID_TOGGLE  = 102;
constexpr uint16_t ID_CONVERT = 103;

constexpr uint8_t COL_LTBLUE_R = 0;
constexpr uint8_t COL_LTBLUE_G = 128;
constexpr uint8_t COL_LTBLUE_B = 255;
constexpr uint8_t COL_LTRED_R  = 255;
constexpr uint8_t COL_LTRED_G  = 96;
constexpr uint8_t COL_LTRED_B  = 96;
constexpr uint8_t COL_YELLOW_R = 255;
constexpr uint8_t COL_YELLOW_G = 255;
constexpr uint8_t COL_YELLOW_B = 0;

constexpr int32_t CTRLPOINTXSIZE = 8;
constexpr int32_t CTRLPOINTYSIZE = 8;

// 기저 변환 행렬(B2K/K2B)은 BezierPlane(VectorViews.h)의 static constexpr
// 멤버 kB2K/kK2B로 옮겨갔다. VectorView는 서버 모드 ClientVectorApp에서도
// 재사용하므로 클래스 선언이 헤더에 있고 구현만 여기에 둔다.

} // anonymous namespace

// --- VectorView (선언: include/apps/VectorViews.h) ---

VectorView::VectorView(const JKRect& rect) {
    SetRect(rect);
    SetFocusable(true);
    SetBackColor(240, 240, 240);
}

void VectorView::Reset() {
    const JKRect client = GetScreenClientRect();
    // NormalizeControlPoint는 포인트를 rect 가장자리에 정확히 놓는다. 뷰 전체를
    // 격자로 쓰면 모서리 포인트가 절반 잘려 히트 박스(16×16)의 절반만 잡히므로
    // 평면을 안쪽으로 들여 여백을 둔다.
    constexpr int32_t kMargin = 40;
    JKRect grid = client;
    grid.x += kMargin;
    grid.w -= kMargin * 2;
    grid.y += kMargin;
    grid.h -= kMargin * 2;
    grid.w = std::max<int32_t>(grid.w, 64);
    grid.h = std::max<int32_t>(grid.h, 64);
    BezierPlane engine;
    engine.NormalizeControlPoint(grid, ctrl_);
}

void VectorView::ToggleMode() {
    isKBez_ = !isKBez_;
}

void VectorView::Convert() {
    BezierPlane engine;
    JKPoint temp[4][4];
    bool targetMode = !isKBez_;
    engine.ChangeBezMode(targetMode, ctrl_, temp);
    std::memcpy(ctrl_, temp, sizeof(ctrl_));
    isKBez_ = targetMode;
}

void VectorView::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    dc.SetColor(backR_, backG_, backB_, 255);
    dc.FillRect(client);

    JKPoint temp[4];
    dc.SetColor(COL_LTBLUE_R, COL_LTBLUE_G, COL_LTBLUE_B, 255);
    for (double v = 0.0; v <= 1.0; v += 0.03) {
        engine_.GetCurvePoint(isKBez_, true, v, ctrl_, temp);
        dc.Bezier(temp, isKBez_);
    }
    for (double u = 0.0; u <= 1.0; u += 0.03) {
        engine_.GetCurvePoint(isKBez_, false, u, ctrl_, temp);
        dc.Bezier(temp, isKBez_);
    }

    dc.SetTextColor(COL_LTRED_R, COL_LTRED_G, COL_LTRED_B);
    char pname[3] = { '0', '0', '\0' };
    for (int32_t i = 0; i < 4; ++i) {
        pname[0] = static_cast<char>('0' + i);
        for (int32_t j = 0; j < 4; ++j) {
            pname[1] = static_cast<char>('0' + j);
            const JKPoint& p = ctrl_[i][j];
            JKRect rect{ p.x - CTRLPOINTXSIZE, p.y - CTRLPOINTYSIZE,
                         CTRLPOINTXSIZE * 2, CTRLPOINTYSIZE * 2 };
            if (i * 4 + j == dragIdx_) {
                // 드래그 중인 포인트: 확대 + 빨간 면 + 흰 두꺼운 테두리로
                // "지금 이 포인트를 잡고 있다"를 표시한다. 라벨은 빨간 면 위에서
                // 안 보이므로 흰색으로 바꿔 찍는다.
                rect = rect.Expand(4);
                dc.SetColor(COL_LTRED_R, COL_LTRED_G, COL_LTRED_B, 255);
                dc.FillRect(rect);
                dc.SetColor(255, 255, 255, 255);
                dc.DrawRect(rect);
                dc.SetColor(0, 0, 0, 255);
                dc.DrawRect(rect.Expand(-3));
                dc.SetTextColor(255, 255, 255);
                dc.TextOutX(rect, pname, ADJ_XYCENTER);
                dc.SetTextColor(COL_LTRED_R, COL_LTRED_G, COL_LTRED_B);
            } else {
                dc.SetColor(COL_YELLOW_R, COL_YELLOW_G, COL_YELLOW_B, 255);
                dc.FillRect(rect);
                dc.SetColor(0, 0, 0, 255);
                dc.DrawRect(rect);
                dc.TextOutX(rect, pname, ADJ_XYCENTER);
            }
        }
    }

    JKControl::OnPaintClient(dc);
}

void VectorView::RespondMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::MouseDown) {
        SetFocus();
        int32_t index = CheckControlPoint(ev.x, ev.y);
        if (index >= 0 && index < 16) {
            dragIdx_ = index;
            if (g_jkAppHost) {
                g_jkAppHost->SetCapture(this);
            }
        }
    } else if (ev.type == JKEventType::MouseMove) {
        if (dragIdx_ >= 0) {
            ctrl_[dragIdx_ / 4][dragIdx_ % 4].Set(ev.x, ev.y);
        } else {
            JKControl::RespondMessage(ev);
        }
    } else if (ev.type == JKEventType::MouseUp) {
        if (dragIdx_ >= 0) {
            ctrl_[dragIdx_ / 4][dragIdx_ % 4].Set(ev.x, ev.y);
            dragIdx_ = -1;
            if (g_jkAppHost) {
                g_jkAppHost->ReleaseCapture();
            }
        } else {
            JKControl::RespondMessage(ev);
        }
    } else if (ev.type == JKEventType::KeyDown) {
        if (ev.keyCode == SDLK_ESCAPE ||
            (ev.keyCode == SDLK_x && (SDL_GetModState() & KMOD_ALT))) {
            JKControl* p = GetParent();
            if (p) {
                p->RequestClose();
            }
        } else if (ev.keyCode == SDLK_LEFT) {
            ShiftPoints(-20, 0);
        } else if (ev.keyCode == SDLK_RIGHT) {
            ShiftPoints(20, 0);
        } else if (ev.keyCode == SDLK_UP) {
            ShiftPoints(0, -20);
        } else if (ev.keyCode == SDLK_DOWN) {
            ShiftPoints(0, 20);
        } else {
            JKControl::RespondMessage(ev);
        }
    } else {
        JKControl::RespondMessage(ev);
    }
}

int32_t VectorView::CheckControlPoint(int32_t x, int32_t y) {
    for (int32_t i = 0; i < 4; ++i) {
        for (int32_t j = 0; j < 4; ++j) {
            const JKPoint& p = ctrl_[i][j];
            JKRect rect{ p.x - CTRLPOINTXSIZE, p.y - CTRLPOINTYSIZE,
                         CTRLPOINTXSIZE * 2, CTRLPOINTYSIZE * 2 };
            if (rect.Contains(x, y)) {
                return i * 4 + j;
            }
        }
    }
    return -1;
}

void VectorView::ShiftPoints(int32_t dx, int32_t dy) {
    for (int32_t i = 0; i < 4; ++i) {
        for (int32_t j = 0; j < 4; ++j) {
            ctrl_[i][j].x += dx;
            ctrl_[i][j].y += dy;
        }
    }
}

// 왼쪽 버튼 패널: 버튼을 올려 놓는 전용 사이드바. 창 배경(240)과 같은 회색으로는
// 패널/캔버스가 구분되지 않으므로 어두운 회색으로 채우고 오른쪽에 구분선을 긋는다.
class VectorSidePanel : public JKControl {
public:
    explicit VectorSidePanel(const JKRect& rect) { SetRect(rect); }

    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetScreenClientRect();
        dc.SetColor(208, 208, 208, 255);
        dc.FillRect(client);
        dc.SetColor(150, 150, 150, 255);
        dc.FillRect(JKRect{ client.x + client.w - 1, client.y, 1, client.h });
    }
};

// 공유 UI 빌더: 단일 프로세스 VectorApp::OnInit과 서버 모드
// ClientVectorApp::OnInit이 함께 호출한다. VectorView는 JKControl이므로
// 서버 모드에서도 크롬 제거 없이 루트 클라이언트 영역에 직접 구성된다.
VectorView* BuildVectorEditorUi(JKWindow* main) {
    const JKRect client = main->GetClientRect();
    // 왼쪽 100px는 버튼 사이드바, 나머지는 베지어 캔버스.
    constexpr int32_t kSideBarW = 100;

    auto panel = std::make_unique<VectorSidePanel>(JKRect{ 0, 0, kSideBarW, client.h });
    auto view = std::make_unique<VectorView>(
        JKRect{ kSideBarW, 0, client.w - kSideBarW, client.h });
    VectorView* viewPtr = view.get();

    // 버튼 rect는 {x, y, w, h}. 예전 값은 원본 WINDBASE의 {l,t,r,b} 수치를 그대로
    // 넘겨 K-Bez(높이 90)·Convert(높이 130)가 서로 겹쳐 그려졌었다.
    auto reset = std::make_unique<JKButton>(JKRect{ 15, 20, 70, 30 }, ID_RESET);
    reset->SetText("Reset");
    reset->SetOnClick([viewPtr]() { viewPtr->Reset(); });

    auto toggle = std::make_unique<JKButton>(JKRect{ 15, 65, 70, 30 }, ID_TOGGLE);
    toggle->SetText("K-Bez");
    toggle->SetOnClick([viewPtr]() { viewPtr->ToggleMode(); });

    auto convert = std::make_unique<JKButton>(JKRect{ 15, 110, 70, 30 }, ID_CONVERT);
    convert->SetText("Convert");
    convert->SetOnClick([viewPtr]() { viewPtr->Convert(); });

    main->AddControl(std::move(panel));
    main->AddControl(std::move(view));
    main->AddControl(std::move(reset));
    main->AddControl(std::move(toggle));
    main->AddControl(std::move(convert));

    viewPtr->Reset();
    viewPtr->SetFocus();
    return viewPtr;
}

VectorApp::VectorApp() = default;

void VectorApp::OnInit() {
    auto main = std::make_unique<JKWindow>("Vector Bezier Editor - SDL2 Port");
    main->SetWindowRect(JKRect{ 0, 0, 1920, 1080 });

    BuildVectorEditorUi(main.get());

    SetMainWindow(std::move(main));
}

} // namespace jk