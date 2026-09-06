#ifndef APPS_VECTORVIEWS_H
#define APPS_VECTORVIEWS_H

// vector/vfont/vpres 계열 앱이 단일 프로세스 모드와 윈도우 서버 클라이언트
// 모드에서 공유하는 뷰/윈도우 클래스 모음.
//
// 원래는 각 App .cpp(VectorApp.cpp/VectorFontApp.cpp/VectorPresApp.cpp)의 익명
// 네임스페이스 안에 정의되어 internal linkage라서 클라이언트 래퍼 TU에서 볼 수
// 없었다. 클라이언트 모듈(ClientVectorApp/ClientVectorFontApp/
// ClientVectorPresApp)이 같은 클래스를 재사용할 수 있도록 선언만 헤더로 옮기고,
// 메서드 구현은 원래 .cpp에 그대로 둔다(단일 프로세스 동작 불변). 구현은
// jkdesktop 실행파일과 각 jkapp_<name> 모듈 DLL 양쪽에 링크된다.

#include <JKControl.h>
#include <JKWindow.h>
#include <JKVectorFont.h>

#include <cstdio>
#include <memory>

namespace jk {

// Bezier 기저 변환/평가 엔진. 원본 JKWINDOW/VECTOR/BEZPLANE.CPP에서 유래.
// 순수 수학 헬퍼라 구현은 헤더 안에 인라인으로 둔다.
class BezierPlane {
public:
    // Cubic Bezier <-> K-Bezier 제어점 기저 변환 행렬.
    static constexpr double kB2K[16][16] = {
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

    static constexpr double kK2B[16][16] = {
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
                    doublex += kB2K[i][j] * points[j / 4][j % 4].x;
                    doubley += kB2K[i][j] * points[j / 4][j % 4].y;
                }
            } else {
                for (int32_t j = 0; j < 16; ++j) {
                    doublex += kK2B[i][j] * points[j / 4][j % 4].x;
                    doubley += kK2B[i][j] * points[j / 4][j % 4].y;
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

// Bezier 곡선 편집 뷰. 단일 프로세스 VectorApp과 서버 모드 ClientVectorApp이
// 함께 쓴다. 구현은 src/apps/VectorApp.cpp.
class VectorView : public JKControl {
public:
    explicit VectorView(const JKRect& rect);

    void Reset();
    void ToggleMode();
    void Convert();

    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;

private:
    int32_t CheckControlPoint(int32_t x, int32_t y);
    void ShiftPoints(int32_t dx, int32_t dy);

    JKPoint ctrl_[4][4];
    bool isKBez_ = false;
    int32_t dragIdx_ = -1;
    BezierPlane engine_;
};

// KSSM 벡터 폰트 글리프 뷰어 윈도우(vfont). 방향키로 글리프 크기 조절.
// 단일 프로세스에서는 떠 있는 메인 윈도우, 서버 모드에서는 루트 아래
// WA_CHROMELESS + DOCK_FILL 자식으로 쓰인다(ClientTetrisApp 패턴).
// 구현은 src/apps/VectorFontApp.cpp.
class VectorFontWindow : public JKWindow {
public:
    explicit VectorFontWindow(JKVectorFont* vfont);

    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;

    JKPoint fontSize_{ 32, 32 };

private:
    JKVectorFont* vfont_ = nullptr;
};

// 벡터 폰트 프레젠테이션 윈도우(vpres). 타이머(500ms)마다 텍스트가 공전하고
// 색상이 순환한다. 구현은 src/apps/VectorPresApp.cpp.
class PresentWindow : public JKWindow {
public:
    explicit PresentWindow(JKVectorFont* vfont);

    void OnPaintClient(JKDC& dc) override;
    void RespondMessage(const JKEvent& ev) override;

private:
    JKVectorFont* vfont_ = nullptr;
    int txtCount_ = 0;
    uint8_t txtColor_ = 14; // start with yellow
};

// vfont/vpres 공통: 영문/한글 .VFT 로딩. 로드 실패는 치명적이지 않다
// (텍스트만 그려지지 않는다). 단일 프로세스와 클라이언트 모듈이 같은 경로를
// 쓰도록 인라인으로 제공(JKENGINE_FONT_DIR은 jkcore의 PUBLIC 컴파일 정의).
inline std::unique_ptr<JKVectorFont> LoadVectorAppFonts(const char* logTag) {
    auto vfont = std::make_unique<JKVectorFont>(
#ifdef JKENGINE_FONT_DIR
        JKENGINE_FONT_DIR
#else
        "."
#endif
    );
    if (!vfont->LoadFont("english.vft", JKVectorFont::English, 0) ||
        !vfont->LoadFont("hanmoon.vft", JKVectorFont::Hangul, 0)) {
        std::fprintf(stderr, "%s: failed to load one or more fonts\n", logTag);
    }
    return vfont;
}

// Vector 편집기 UI(VectorView + Reset/K-Bez/Convert 버튼 3개)를 `main`의
// 클라이언트 영역에 구성하고 Reset/SetFocus까지 수행한다. 단일 프로세스
// VectorApp::OnInit과 서버 모드 ClientVectorApp::OnInit이 함께 호출한다.
// 구현은 src/apps/VectorApp.cpp.
VectorView* BuildVectorEditorUi(JKWindow* main);

} // namespace jk

#endif // APPS_VECTORVIEWS_H