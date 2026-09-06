#ifndef APPS_PCXVIEWS_H
#define APPS_PCXVIEWS_H

#include <JKWindow.h>
#include <memory>
#include <string>
#include <vector>

namespace jk {

class JKFileDialog;
class JKStatic;
class JKResourceCache;

// Decoded RGBA8 image shared by the viewer window and its canvas.
struct ViewerImage {
    int w = 0;
    int h = 0;
    std::vector<uint8_t> rgba; // row-major, w*h*4 bytes (R,G,B,A)
};

// 이미지 뷰어 윈도우(2026-09-06 재작성). 구 PCX 전용 뷰어를 범용화했다:
// PCX(8비트 RLE+팔레트)와 PNG/JPEG/BMP(stb_image)를 같은 RGBA 모델로 통합하고
// ResourceCache 텍스처 블릿, fit/1:1/줌(25~800%), 드래그 팬, 휠 줌, 시작 시
// 파일 열기 다이얼로그를 제공한다. 단일 프로세스 PcxApp과 클라이언트 모듈
// (ClientPcxApp)이 같은 빌더를 공유한다(테트리스 Build 패턴).
class ImageViewerWindow : public JKWindow {
public:
    ImageViewerWindow();

    // Load a file into the canvas. Failure keeps the previous content and
    // shows the reason in the status label.
    void OpenPath(const std::string& path);
    void ShowOpenDialog();
    // Show the open dialog once when no file was given (timer/idle hook —
    // must run after the app registered the main window).
    void MaybeShowStartupDialog();
    // Wheel events have no hit target — the app forwards them from
    // PreProcessMessage (ClientTerminalApp 패턴).
    void ForwardWheel(int dy);

    // View state accessors (self-test + potential automation API).
    bool HasImage() const;
    const ViewerImage* Image() const;
    bool IsFit() const;
    int ZoomPercent() const;

protected:
    void OnPaintClient(JKDC& dc) override;
    void PerformLayout(const JKRect& parentClient) override;

private:
    class Canvas;
    friend class Canvas;

    void UpdateLabels();
    void UpdateStatus(const std::string& text);
    void LayoutChildren();

    std::unique_ptr<ViewerImage> image_;
    std::unique_ptr<JKFileDialog> dialog_;
    Canvas* canvas_ = nullptr;
    JKStatic* status_ = nullptr;
    JKStatic* zoomLabel_ = nullptr;
    std::string path_;
    bool startupDialogShown_ = false;
};

// 이전 시그니처 유지: filePath가 비어 있으면 빈 캔버스(Open 안내) 상태로
// 만든다. 반환된 윈도우(1280x680, 제목 "Image Viewer")는 호출자가
// 소유한다 — SetMainWindow로 루트 윈도우로 지정한다.
std::unique_ptr<JKWindow> CreatePcxViewerWindow(const std::string& filePath);

} // namespace jk

#endif // APPS_PCXVIEWS_H