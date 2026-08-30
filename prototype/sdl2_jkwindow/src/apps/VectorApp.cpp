#include <apps/VectorApp.h>

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

// Basis conversion matrices between cubic Bezier and K-Bezier control points.
// Copied from the original JKWINDOW/VECTOR/BEZPLANE.CPP.
constexpr double B2K[16][16] = {
    { 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
    { 8.0 / 27.0, 4.0 / 9.0, 2.0 / 9.0, 1.0 / 27.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
    { 1.0 / 27.0, 2.0 / 9.0, 4.0 / 9.0, 8.0 / 27.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
    { 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
    { 8.0 / 27.0, 0.0, 0.0, 0.0, 4.0 / 9.0, 0.0, 0.0, 0.0, 2.0 / 9.0, 0.0, 0.0, 0.0, 1.0 / 27.0, 0.0, 0.0, 0.0 },
    { 64.0 / 729.0, 32.0 / 243.0, 16.0 / 243.0, 8.0 / 729.0, 32.0 / 243.0, 16.0 / 81.0, 8.0 / 81.0, 4.0 / 243.0, 16.0 / 243.0, 8.0 / 81.0, 4.0 / 81.0, 2.0 / 243.0, 8.0 / 729.0, 4.0 / 243.0, 2.0 / 243.0, 1.0 / 729.0 },
    { 8.0 / 729.0, 16.0 / 243.0, 32.0 / 243.0, 64.0 / 729.0, 4.0 / 243.0, 8.0 / 81.0, 16.0 / 81.0, 32.0 / 243.0, 2.0 / 243.0, 4.0 / 81.0, 8.0 / 81.0, 16.0 / 243.0, 1.0 / 729.0, 2.0 / 243.0, 4.0 / 243.0, 8.0 / 729.0 },
    { 0.0, 0.0, 0.0, 8.0 / 27.0, 0.0, 0.0, 0.0, 4.0 / 9.0, 0.0, 0.0, 0.0, 2.0 / 9.0, 0.0, 0.0, 0.0, 1.0 / 27.0 },
    { 1.0 / 27.0, 0.0, 0.0, 0.0, 2.0 / 9.0, 0.0, 0.0, 0.0, 4.0 / 9.0, 0.0, 0.0, 0.0, 8.0 / 27.0, 0.0, 0.0, 0.0 },
    { 8.0 / 729.0, 4.0 / 243.0, 2.0 / 243.0, 1.0 / 729.0, 16.0 / 243.0, 8.0 / 81.0, 4.0 / 81.0, 2.0 / 243.0, 32.0 / 243.0, 16.0 / 81.0, 8.0 / 81.0, 4.0 / 243.0, 64.0 / 729.0, 32.0 / 243.0, 16.0 / 243.0, 8.0 / 729.0 },
    { 1.0 / 729.0, 2.0 / 243.0, 4.0 / 243.0, 8.0 / 729.0, 2.0 / 243.0, 4.0 / 81.0, 8.0 / 81.0, 16.0 / 243.0, 4.0 / 243.0, 8.0 / 81.0, 16.0 / 81.0, 32.0 / 243.0, 8.0 / 729.0, 16.0 / 243.0, 32.0 / 243.0, 64.0 / 729.0 },
    { 0.0, 0.0, 0.0, 1.0 / 27.0, 0.0, 0.0, 0.0, 2.0 / 9.0, 0.0, 0.0, 0.0, 4.0 / 9.0, 0.0, 0.0, 0.0, 8.0 / 27.0 },
    { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0 },
    { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 8.0 / 27.0, 4.0 / 9.0, 2.0 / 9.0, 1.0 / 27.0 },
    { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0 / 27.0, 2.0 / 9.0, 4.0 / 9.0, 8.0 / 27.0 },
    { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0 }
};

constexpr double K2B[16][16] = {
    { 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
    { -5.0 / 6.0, 3.0, -3.0 / 2.0, 1.0 / 3.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
    { 1.0 / 3.0, -3.0 / 2.0, 3.0, -5.0 / 6.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
    { 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
    { -5.0 / 6.0, 0.0, 0.0, 0.0, 3.0, 0.0, 0.0, 0.0, -3.0 / 2.0, 0.0, 0.0, 0.0, 1.0 / 3.0, 0.0, 0.0, 0.0 },
    { 25.0 / 36.0, -5.0 / 2.0, 5.0 / 4.0, -5.0 / 18.0, -5.0 / 2.0, 9.0, -9.0 / 2.0, 1.0, 5.0 / 4.0, -9.0 / 2.0, 9.0 / 4.0, -1.0 / 2.0, -5.0 / 18.0, 1.0, -1.0 / 2.0, 1.0 / 9.0 },
    { -5.0 / 18.0, 5.0 / 4.0, -5.0 / 2.0, 25.0 / 36.0, 1.0, -9.0 / 2.0, 9.0, -5.0 / 2.0, -1.0 / 2.0, 9.0 / 4.0, -9.0 / 2.0, 5.0 / 4.0, 1.0 / 9.0, -1.0 / 2.0, 1.0, -5.0 / 18.0 },
    { 0.0, 0.0, 0.0, -5.0 / 6.0, 0.0, 0.0, 0.0, 3.0, 0.0, 0.0, 0.0, -3.0 / 2.0, 0.0, 0.0, 0.0, 1.0 / 3.0 },
    { 1.0 / 3.0, 0.0, 0.0, 0.0, -3.0 / 2.0, 0.0, 0.0, 0.0, 3.0, 0.0, 0.0, 0.0, -5.0 / 6.0, 0.0, 0.0, 0.0 },
    { -5.0 / 18.0, 1.0, -1.0 / 2.0, 1.0 / 9.0, 5.0 / 4.0, -9.0 / 2.0, 9.0 / 4.0, -1.0 / 2.0, -5.0 / 2.0, 9.0, -9.0 / 2.0, 1.0, 25.0 / 36.0, -5.0 / 2.0, 5.0 / 4.0, -5.0 / 18.0 },
    { 1.0 / 9.0, -1.0 / 2.0, 1.0, -5.0 / 18.0, -1.0 / 2.0, 9.0 / 4.0, -9.0 / 2.0, 5.0 / 4.0, 1.0, -9.0 / 2.0, 9.0, -5.0 / 2.0, -5.0 / 18.0, 5.0 / 4.0, -5.0 / 2.0, 25.0 / 36.0 },
    { 0.0, 0.0, 0.0, 1.0 / 3.0, 0.0, 0.0, 0.0, -3.0 / 2.0, 0.0, 0.0, 0.0, 3.0, 0.0, 0.0, 0.0, -5.0 / 6.0 },
    { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0 },
    { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, -5.0 / 6.0, 3.0, -3.0 / 2.0, 1.0 / 3.0 },
    { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0 / 3.0, -3.0 / 2.0, 3.0, -5.0 / 6.0 },
    { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0 }
};

class BezierPlane {
public:
    void NormalizeControlPoint(const JKRect& rect, JKPoint points[4][4]) {
        int32_t width = rect.w;
        int32_t height = rect.h;
        for (int32_t i = 0; i < 4; ++i) {
            for (int32_t j = 0; j < 4; ++j) {
                points[i][j].Set(rect.x + width * i / 3,
                                   rect.y + height * j / 3);
            }
        }
    }

    JKPoint CalcBezPoint(double u, double v, JKPoint points[4][4], bool iskbez) {
        double table[2][4];
        if (iskbez) {
            table[0][0] = 0.5 * (1.0 - 3.0 * u) * (2.0 - 3.0 * u) * (1.0 - u);
            table[0][1] = 4.5 * (2.0 - 3.0 * u) * (1.0 - u) * u;
            table[0][2] = -4.5 * (1.0 - u) * u * (1.0 - 3.0 * u);
            table[0][3] = 0.5 * u * (1.0 - 3.0 * u) * (2.0 - 3.0 * u);

            table[1][0] = 0.5 * (1.0 - 3.0 * v) * (2.0 - 3.0 * v) * (1.0 - v);
            table[1][1] = 4.5 * (2.0 - 3.0 * v) * (1.0 - v) * v;
            table[1][2] = -4.5 * (1.0 - v) * v * (1.0 - 3.0 * v);
            table[1][3] = 0.5 * v * (1.0 - 3.0 * v) * (2.0 - 3.0 * v);
        } else {
            table[0][0] = (1.0 - u) * (1.0 - u) * (1.0 - u);
            table[0][1] = 3.0 * u * (1.0 - u) * (1.0 - u);
            table[0][2] = 3.0 * u * u * (1.0 - u);
            table[0][3] = u * u * u;

            table[1][0] = (1.0 - v) * (1.0 - v) * (1.0 - v);
            table[1][1] = 3.0 * v * (1.0 - v) * (1.0 - v);
            table[1][2] = 3.0 * v * v * (1.0 - v);
            table[1][3] = v * v * v;
        }
        double doublex = 0.0;
        double doubley = 0.0;
        for (int32_t i = 0; i < 4; ++i) {
            for (int32_t j = 0; j < 4; ++j) {
                doublex += table[0][i] * table[1][j] * points[i][j].x;
                doubley += table[0][i] * table[1][j] * points[i][j].y;
            }
        }
        return JKPoint{ static_cast<int32_t>(doublex + 0.5),
                        static_cast<int32_t>(doubley + 0.5) };
    }

    void ChangeBezMode(bool mode, JKPoint points[4][4], JKPoint pointBuf[4][4]) {
        for (int32_t i = 0; i < 16; ++i) {
            double doublex = 0.0;
            double doubley = 0.0;
            if (mode) {
                for (int32_t j = 0; j < 16; ++j) {
                    doublex += B2K[i][j] * points[j / 4][j % 4].x;
                    doubley += B2K[i][j] * points[j / 4][j % 4].y;
                }
            } else {
                for (int32_t j = 0; j < 16; ++j) {
                    doublex += K2B[i][j] * points[j / 4][j % 4].x;
                    doubley += K2B[i][j] * points[j / 4][j % 4].y;
                }
            }
            pointBuf[i / 4][i % 4].x = static_cast<int32_t>(doublex + 0.5);
            pointBuf[i / 4][i % 4].y = static_cast<int32_t>(doubley + 0.5);
        }
    }

    void GetCurvePoint(bool iskbez, bool isu, double uv,
                       JKPoint points[4][4], JKPoint pointBuf[4]) {
        double table[4];
        if (iskbez) {
            table[0] = 0.5 * (1.0 - 3.0 * uv) * (2.0 - 3.0 * uv) * (1.0 - uv);
            table[1] = 4.5 * (2.0 - 3.0 * uv) * (1.0 - uv) * uv;
            table[2] = -4.5 * (1.0 - uv) * uv * (1.0 - 3.0 * uv);
            table[3] = 0.5 * uv * (1.0 - 3.0 * uv) * (2.0 - 3.0 * uv);
        } else {
            table[0] = (1.0 - uv) * (1.0 - uv) * (1.0 - uv);
            table[1] = 3.0 * uv * (1.0 - uv) * (1.0 - uv);
            table[2] = 3.0 * uv * uv * (1.0 - uv);
            table[3] = uv * uv * uv;
        }
        for (int32_t i = 0; i < 4; ++i) {
            double doublex = 0.0;
            double doubley = 0.0;
            if (isu) {
                for (int32_t j = 0; j < 4; ++j) {
                    doublex += table[j] * points[i][j].x;
                    doubley += table[j] * points[i][j].y;
                }
            } else {
                for (int32_t j = 0; j < 4; ++j) {
                    doublex += table[j] * points[j][i].x;
                    doubley += table[j] * points[j][i].y;
                }
            }
            pointBuf[i].x = static_cast<int32_t>(doublex + 0.5);
            pointBuf[i].y = static_cast<int32_t>(doubley + 0.5);
        }
    }
};

class VectorView : public JKControl {
public:
    explicit VectorView(const JKRect& rect) {
        SetRect(rect);
        SetFocusable(true);
        SetBackColor(240, 240, 240);
    }

    void Reset() {
        const JKRect client = GetScreenClientRect();
        BezierPlane engine;
        engine.NormalizeControlPoint(client, ctrl_);
    }

    void ToggleMode() {
        isKBez_ = !isKBez_;
    }

    void Convert() {
        BezierPlane engine;
        JKPoint temp[4][4];
        bool targetMode = !isKBez_;
        engine.ChangeBezMode(targetMode, ctrl_, temp);
        std::memcpy(ctrl_, temp, sizeof(ctrl_));
        isKBez_ = targetMode;
    }

    void OnPaintClient(JKDC& dc) override {
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
                dc.SetColor(COL_YELLOW_R, COL_YELLOW_G, COL_YELLOW_B, 255);
                dc.FillRect(rect);
                dc.SetColor(0, 0, 0, 255);
                dc.DrawRect(rect);
                dc.TextOutX(rect, pname, ADJ_XYCENTER);
            }
        }

        JKControl::OnPaintClient(dc);
    }

    void RespondMessage(const JKEvent& ev) override {
        if (ev.type == JKEventType::MouseDown) {
            SetFocus();
            int32_t index = CheckControlPoint(ev.x, ev.y);
            if (index >= 0 && index < 16) {
                dragIdx_ = index;
                if (g_currentJKApp) {
                    g_currentJKApp->SetCapture(this);
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
                if (g_currentJKApp) {
                    g_currentJKApp->ReleaseCapture();
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

private:
    JKPoint ctrl_[4][4];
    bool isKBez_ = false;
    int32_t dragIdx_ = -1;
    BezierPlane engine_;

    int32_t CheckControlPoint(int32_t x, int32_t y) {
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

    void ShiftPoints(int32_t dx, int32_t dy) {
        for (int32_t i = 0; i < 4; ++i) {
            for (int32_t j = 0; j < 4; ++j) {
                ctrl_[i][j].x += dx;
                ctrl_[i][j].y += dy;
            }
        }
    }
};

} // anonymous namespace

VectorApp::VectorApp() = default;

void VectorApp::OnInit() {
    auto main = std::make_unique<JKWindow>("Vector Bezier Editor - SDL2 Port");
    main->SetWindowRect(JKRect{ 0, 0, 1920, 1080 });

    const JKRect client = main->GetClientRect();
    auto view = std::make_unique<VectorView>(JKRect{ 0, 0, client.w, client.h });
    VectorView* viewPtr = view.get();

    auto reset = std::make_unique<JKButton>(JKRect{ 10, 20, 60, 50 }, ID_RESET);
    reset->SetText("Reset");
    reset->SetOnClick([viewPtr]() { viewPtr->Reset(); });

    auto toggle = std::make_unique<JKButton>(JKRect{ 10, 60, 60, 90 }, ID_TOGGLE);
    toggle->SetText("K-Bez");
    toggle->SetOnClick([viewPtr]() { viewPtr->ToggleMode(); });

    auto convert = std::make_unique<JKButton>(JKRect{ 10, 100, 60, 130 }, ID_CONVERT);
    convert->SetText("Convert");
    convert->SetOnClick([viewPtr]() { viewPtr->Convert(); });

    main->AddControl(std::move(view));
    main->AddControl(std::move(reset));
    main->AddControl(std::move(toggle));
    main->AddControl(std::move(convert));

    viewPtr->Reset();
    viewPtr->SetFocus();

    SetMainWindow(std::move(main));
}

} // namespace jk
