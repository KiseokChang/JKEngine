// Image Viewer (pcx app, 2026-09-06 rewrite).
// PCX(8-bit RLE + 256-color palette)와 PNG/JPEG/BMP(stb_image)를 하나의
// RGBA 뷰어로 통합했다. 렌더는 ResourceCache 텍스처를 BlitTexture로 스케일
// 블릿 — 구 픽셀 루프(DrawPixel)는 사진 크기에서 너무 느리고 축소 시
// aliasing이 심했다. 줌/팬/파일 다이얼로그는 문서 없이 WINDBASE 관례를 따른다.
#include <apps/PcxViews.h>

#include <JKApplicationHost.h>
#include <JKButton.h>
#include <JKDC.h>
#include <JKFileDialog.h>
#include <JKHangulUtil.h>
#include <JKImageLoader.h>
#include <JKStatic.h>

#include <apps/PcxApp.h>

#include <algorithm>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace jk {

namespace {

// ---------------------------------------------------------------------------
// PCX loading (8-bit, RLE, 256-color — the WINDBASE asset format).

#pragma pack(push, 1)
struct PcxHeader {
    uint8_t manufacturer;
    uint8_t version;
    uint8_t encoding;
    uint8_t bitsPerPixel;
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
    uint16_t horizDPI;
    uint16_t vertDPI;
    uint8_t egaPalette[48];
    uint8_t reserved1;
    uint8_t numPlanes;
    uint16_t bytesPerLine;
    uint16_t paletteInfo;
    uint16_t hScreenSize;
    uint16_t vScreenSize;
    uint8_t filler[54];
};
#pragma pack(pop)

static_assert(sizeof(PcxHeader) == 128, "PCX header size mismatch");

bool ReadPcxRleLine(FILE* fp, uint8_t* out, int bytes) {
    int n = 0;
    while (n < bytes) {
        int c = fgetc(fp);
        if (c == EOF) return false;
        uint8_t b = static_cast<uint8_t>(c);
        if ((b & 0xC0) == 0xC0) {
            int count = b & 0x3F;
            c = fgetc(fp);
            if (c == EOF) return false;
            uint8_t value = static_cast<uint8_t>(c);
            while (count-- > 0 && n < bytes) {
                out[n++] = value;
            }
        } else {
            out[n++] = b;
        }
    }
    return true;
}

// Decodes a 256-color PCX straight into ViewerImage RGBA (palette expanded).
bool LoadPcxRgba(const std::string& path, ViewerImage& out) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;

    PcxHeader header{};
    if (std::fread(&header, sizeof(header), 1, fp) != 1) {
        std::fclose(fp);
        return false;
    }

    // Must be ZSoft PCX, RLE, 256-color, single plane.
    if (header.manufacturer != 0x0A || header.encoding != 1 ||
        header.bitsPerPixel != 8 || header.numPlanes != 1) {
        std::fclose(fp);
        return false;
    }

    int width = header.x2 - header.x1 + 1;
    int height = header.y2 - header.y1 + 1;
    int bytesPerLine = header.bytesPerLine;
    if (width <= 0 || height <= 0 || bytesPerLine <= 0) {
        std::fclose(fp);
        return false;
    }

    // Read 256-color palette at end of file.
    if (std::fseek(fp, -769, SEEK_END) != 0) {
        std::fclose(fp);
        return false;
    }
    int marker = fgetc(fp);
    if (marker != 0x0C) {
        std::fclose(fp);
        return false;
    }
    uint8_t palette[256][3];
    for (int i = 0; i < 256; ++i) {
        int r = fgetc(fp);
        int g = fgetc(fp);
        int b = fgetc(fp);
        if (r == EOF || g == EOF || b == EOF) {
            std::fclose(fp);
            return false;
        }
        palette[i][0] = static_cast<uint8_t>(r);
        palette[i][1] = static_cast<uint8_t>(g);
        palette[i][2] = static_cast<uint8_t>(b);
    }

    out.w = width;
    out.h = height;
    out.rgba.resize(static_cast<size_t>(width) * height * 4);

    std::fseek(fp, sizeof(header), SEEK_SET);
    std::vector<uint8_t> line(bytesPerLine);
    for (int y = 0; y < height; ++y) {
        if (!ReadPcxRleLine(fp, line.data(), bytesPerLine)) {
            std::fclose(fp);
            return false;
        }
        for (int x = 0; x < width; ++x) {
            uint8_t idx = line[x];
            uint8_t* px = &out.rgba[(static_cast<size_t>(y) * width + x) * 4];
            px[0] = palette[idx][0];
            px[1] = palette[idx][1];
            px[2] = palette[idx][2];
            px[3] = 255;
        }
    }

    std::fclose(fp);
    return true;
}

// Dispatch on extension: .pcx uses the internal decoder, everything else
// goes through stb_image (PNG/JPEG/BMP).
std::string LowerExt(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos ||
        (slash != std::string::npos && dot < slash)) {
        return std::string();
    }
    std::string ext = path.substr(dot);
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext;
}

bool LoadViewerImageRgba(const std::string& path, ViewerImage& out) {
    if (LowerExt(path) == ".pcx") {
        return LoadPcxRgba(path, out);
    }
    LoadedImage img;
    if (!LoadImageFile(path, img) || img.w <= 0 || img.h <= 0 ||
        img.rgba.size() != static_cast<size_t>(img.w) * img.h * 4) {
        return false;
    }
    out.w = img.w;
    out.h = img.h;
    out.rgba = std::move(img.rgba);
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// ImageViewerWindow

namespace {

// Zoom ladder in percent. kIndex100 points at the 100% entry.
constexpr int kZoomSteps[] = { 25, 33, 50, 66, 75, 100,
                               125, 150, 200, 250, 300, 400, 500, 600, 800 };
constexpr int kZoomCount = static_cast<int>(sizeof(kZoomSteps) / sizeof(int));
constexpr int kIndex100 = 5;

constexpr int kToolbarH = 28;
constexpr uint16_t kIdOpen = 1;
constexpr uint16_t kIdFit = 2;
constexpr uint16_t kIdActual = 3;
constexpr uint16_t kIdZoomOut = 4;
constexpr uint16_t kIdZoomIn = 5;

// ResourceCache key for the displayed image (single slot — reloading
// unloads the previous texture; the queue destroys it on the render side).
constexpr const char* kImageKey = "pcx_image";

} // namespace

class ImageViewerWindow::Canvas : public JKControl {
public:
    void SetContent(JKResourceCache* cache, ViewerImage* image,
                    const std::string& path) {
        cache_ = cache;
        image_ = image;
        path_ = path;
        tex_ = JKRenderBackend::InvalidTexture;
        texWait_ = 0;
        fit_ = true;
        zoomIndex_ = kIndex100;
        pan_.x = 0;
        pan_.y = 0;
        Invalidate();
    }

    void ClearContent() {
        image_ = nullptr;
        tex_ = JKRenderBackend::InvalidTexture;
        Invalidate();
    }

    bool HasImage() const { return image_ && !image_->rgba.empty(); }

    void ZoomIn() { StepZoom(1); }
    void ZoomOut() { StepZoom(-1); }
    void SetFit() {
        fit_ = true;
        pan_.x = 0;
        pan_.y = 0;
        NotifyViewChanged();
        Invalidate();
    }
    void SetActualSize() {
        fit_ = false;
        zoomIndex_ = kIndex100;
        pan_.x = 0;
        pan_.y = 0;
        NotifyViewChanged();
        Invalidate();
    }
    bool IsFit() const { return fit_; }
    int ZoomPercent() const {
        return fit_ ? 0 : kZoomSteps[zoomIndex_];
    }

    void SetOnViewChanged(std::function<void()> cb) {
        onViewChanged_ = std::move(cb);
    }

    // Wheel events arrive without a hit target (app forwards them).
    void HandleWheel(int dy) {
        if (!HasImage()) return;
        StepZoom(dy > 0 ? 1 : -1);
    }

    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetScreenClientRect();
        dc.SetColor(24, 24, 28, 255);
        dc.FillRect(client);
        if (!HasImage()) {
            if (path_.empty()) {
                dc.SetTextColor(160, 160, 160);
                dc.TextOutX(client, "Open an image file  (PCX / PNG / JPG / BMP)",
                            ADJ_XYCENTER, false);
            }
            return;
        }

        if (tex_ == JKRenderBackend::InvalidTexture) {
            UploadTexture();
            if (tex_ == JKRenderBackend::InvalidTexture) {
                // Uploads are queued; the render side flushes them before or
                // during the next frames. Retry a few times, then give up.
                if (++texWait_ <= 8) {
                    Invalidate();
                } else {
                    dc.SetTextColor(220, 120, 120);
                    dc.TextOutX(client, "Texture upload failed.",
                                ADJ_XYCENTER, false);
                }
                return;
            }
        }

        const int iw = image_->w;
        const int ih = image_->h;
        double scale;
        if (fit_) {
            // Fit: shrink oversized images, upscale small ones — same as
            // common viewers.
            scale = std::min(static_cast<double>(client.w) / iw,
                             static_cast<double>(client.h) / ih);
        } else {
            scale = kZoomSteps[zoomIndex_] / 100.0;
        }
        const int drawW = std::max(1, static_cast<int>(iw * scale + 0.5));
        const int drawH = std::max(1, static_cast<int>(ih * scale + 0.5));
        int offX = client.x + (client.w - drawW) / 2 + pan_.x;
        int offY = client.y + (client.h - drawH) / 2 + pan_.y;

        JKRenderBackend* backend = dc.GetBackend();
        if (!backend) return;
        backend->SetClipRect(&client);
        backend->BlitTexture(tex_, nullptr, JKRect{ offX, offY, drawW, drawH }, 255);
        backend->SetClipRect(nullptr);
    }

    void RespondMessage(const JKEvent& ev) override {
        if (ev.type == JKEventType::MouseDown) {
            dragging_ = true;
            dragStart_.x = ev.x;
            dragStart_.y = ev.y;
            panStart_ = pan_;
            if (g_jkAppHost) g_jkAppHost->SetCapture(this);
        } else if (ev.type == JKEventType::MouseMove && dragging_) {
            if (g_jkAppHost && g_jkAppHost->GetCapture() == this) {
                pan_.x = panStart_.x + (ev.x - dragStart_.x);
                pan_.y = panStart_.y + (ev.y - dragStart_.y);
                ClampPan();
                Invalidate();
            }
        } else if (ev.type == JKEventType::MouseUp) {
            if (dragging_) {
                dragging_ = false;
                if (g_jkAppHost) g_jkAppHost->ReleaseCapture();
            }
        } else {
            JKControl::RespondMessage(ev);
        }
    }

private:
    void UploadTexture() {
        if (!cache_ || !HasImage()) return;
        // Queue the upload exactly once per load: CreateImageFromRGBA calls
        // UnloadImage internally, which would destroy the texture the render
        // side just flushed and reset the entry to Invalid — re-calling it on
        // every retry frame loops forever without ever sampling a texture.
        if (!cache_->HasImage(kImageKey)) {
            cache_->CreateImageFromRGBA(kImageKey, image_->w, image_->h,
                                        image_->rgba);
        }
        tex_ = cache_->GetImage(kImageKey);
        texWait_ = 0;
    }

    void StepZoom(int dir) {
        if (!HasImage()) return;
        fit_ = false;
        const int next = zoomIndex_ + dir;
        if (next < 0 || next >= kZoomCount) return;
        zoomIndex_ = next;
        ClampPan();
        NotifyViewChanged();
        Invalidate();
    }

    void ClampPan() {
        if (!HasImage() || fit_) {
            if (fit_) {
                pan_.x = 0;
                pan_.y = 0;
            }
            return;
        }
        const JKRect client = GetScreenClientRect();
        const double scale = kZoomSteps[zoomIndex_] / 100.0;
        const int drawW = std::max(1, static_cast<int>(image_->w * scale + 0.5));
        const int drawH = std::max(1, static_cast<int>(image_->h * scale + 0.5));
        // Keep the image loosely on screen: center can wander at most half
        // the drawn size plus one canvas — no hard edge-lock.
        const int maxX = client.w / 2 + drawW / 2;
        const int maxY = client.h / 2 + drawH / 2;
        pan_.x = std::max(-maxX, std::min(maxX, pan_.x));
        pan_.y = std::max(-maxY, std::min(maxY, pan_.y));
    }

    void NotifyViewChanged() {
        if (onViewChanged_) onViewChanged_();
    }

    JKResourceCache* cache_ = nullptr;
    ViewerImage* image_ = nullptr;
    JKRenderBackend::TextureHandle tex_ = JKRenderBackend::InvalidTexture;
    int texWait_ = 0;
    bool fit_ = true;
    int zoomIndex_ = kIndex100;
    JKPoint pan_{ 0, 0 };
    bool dragging_ = false;
    JKPoint dragStart_{ 0, 0 };
    JKPoint panStart_{ 0, 0 };
    std::string path_;
    std::function<void()> onViewChanged_;
};

ImageViewerWindow::ImageViewerWindow()
    : JKWindow("Image Viewer") {
    SetWindowRect(JKRect{ 0, 0, 1280, 680 });

    auto canvas = std::make_unique<Canvas>();
    canvas_ = canvas.get();
    canvas_->SetOnViewChanged([this]() { UpdateLabels(); });

    auto status = std::make_unique<JKStatic>(JKRect{ 0, 0, 10, 10 }, 0);
    status_ = status.get();
    status_->SetBackColor(0, 0, 128);
    status_->SetTextColor(255, 255, 255);

    auto zoomLabel = std::make_unique<JKStatic>(JKRect{ 0, 0, 10, 10 }, 0);
    zoomLabel_ = zoomLabel.get();
    zoomLabel_->SetBackColor(64, 64, 72);
    zoomLabel_->SetTextColor(255, 255, 255);
    zoomLabel_->SetText("Fit");

    struct BtnDef { uint16_t id; const char* label; int w; };
    const BtnDef defs[] = {
        { kIdOpen, "Open", 64 },
        { kIdFit, "Fit", 48 },
        { kIdActual, "1:1", 48 },
        { kIdZoomOut, "-", 32 },
        { kIdZoomIn, "+", 32 },
    };
    for (const BtnDef& d : defs) {
        auto btn = std::make_unique<JKButton>(JKRect{ 0, 0, d.w, 22 }, d.id);
        btn->SetText(d.label);
        btn->SetBackColor(192, 192, 192);
        btn->SetTextColor(0, 0, 0);
        btn->SetDepth(2);
        const uint16_t id = d.id;
        btn->SetOnClick([this, id]() {
            switch (id) {
                case kIdOpen: ShowOpenDialog(); break;
                case kIdFit: canvas_->SetFit(); break;
                case kIdActual: canvas_->SetActualSize(); break;
                case kIdZoomOut: canvas_->ZoomOut(); break;
                case kIdZoomIn: canvas_->ZoomIn(); break;
            }
        });
        AddControl(std::move(btn));
    }

    AddControl(std::move(status));
    AddControl(std::move(zoomLabel));
    AddControl(std::move(canvas));

    dialog_ = std::make_unique<JKFileDialog>("Open Image");
    dialog_->SetFilter("*.pcx;*.png;*.jpg;*.jpeg;*.bmp");
    dialog_->SetOnOk([this](const std::string& path) { OpenPath(path); });
    dialog_->SetOnCancel([]() { /* keep current content */ });

    LayoutChildren();
    UpdateLabels();
}

void ImageViewerWindow::LayoutChildren() {
    const JKRect client = GetClientRect();
    int x = 4;
    const int y = 3;
    for (auto& child : GetChildren()) {
        if (child->GetControlId() != 0) {
            // Toolbar buttons keep their width, laid out left to right.
            const JKRect r = child->GetRect();
            child->SetRect(JKRect{ x, y + 1, r.w, 22 });
            x += r.w + 4;
        }
    }
    const int labelW = 90;
    zoomLabel_->SetRect(JKRect{ client.w - labelW - 4, y + 1, labelW, 22 });
    const int statusW = client.w - x - labelW - 12;
    status_->SetRect(JKRect{ x, y + 1, std::max(10, statusW), 22 });
    canvas_->SetRect(JKRect{ 0, kToolbarH, client.w,
                             std::max(10, client.h - kToolbarH) });
}

void ImageViewerWindow::PerformLayout(const JKRect& parentClient) {
    JKWindow::PerformLayout(parentClient);
    LayoutChildren();
}

void ImageViewerWindow::OnPaintClient(JKDC& dc) {
    const JKRect client = GetScreenClientRect();
    dc.SetColor(48, 48, 56, 255);
    dc.FillRect(client);
    for (const auto& child : GetChildren()) {
        if (child->IsVisible()) {
            child->PaintClient(dc);
        }
    }
}

void ImageViewerWindow::OpenPath(const std::string& path) {
    auto img = std::make_unique<ViewerImage>();
    if (!LoadViewerImageRgba(path, *img)) {
        UpdateStatus(path + "  -  load failed");
        return;
    }
    char info[512];
    std::snprintf(info, sizeof(info), "%s  (%dx%d)", path.c_str(),
                  img->w, img->h);

    if (g_jkAppHost) {
        JKResourceCache* cache = g_jkAppHost->GetResourceCache();
        if (cache) {
            cache->UnloadImage(kImageKey);
        }
        canvas_->SetContent(cache, img.get(), path);
    } else {
        // Hostless mode (self-test): keep CPU-side state for layout checks.
        canvas_->SetContent(nullptr, img.get(), path);
    }
    image_ = std::move(img);
    path_ = path;
    UpdateStatus(info);
    UpdateLabels();
    Invalidate();
}

void ImageViewerWindow::ShowOpenDialog() {
    if (dialog_) dialog_->Show();
}

void ImageViewerWindow::MaybeShowStartupDialog() {
    if (startupDialogShown_) return;
    startupDialogShown_ = true;
    if (path_.empty() && dialog_) {
        dialog_->SetInitialDir(".");
        dialog_->Show();
    }
}

void ImageViewerWindow::ForwardWheel(int dy) {
    canvas_->HandleWheel(dy);
}

bool ImageViewerWindow::HasImage() const {
    return canvas_->HasImage();
}

const ViewerImage* ImageViewerWindow::Image() const {
    return image_.get();
}

bool ImageViewerWindow::IsFit() const {
    return canvas_->IsFit();
}

int ImageViewerWindow::ZoomPercent() const {
    return canvas_->ZoomPercent();
}

void ImageViewerWindow::UpdateLabels() {
    if (zoomLabel_) {
        if (canvas_->IsFit()) {
            zoomLabel_->SetText("Fit");
        } else {
            char z[16];
            std::snprintf(z, sizeof(z), "%d%%", canvas_->ZoomPercent());
            zoomLabel_->SetText(z);
        }
        zoomLabel_->Invalidate();
    }
}

void ImageViewerWindow::UpdateStatus(const std::string& text) {
    if (status_) {
        status_->SetText(Utf8ToKssm(text.c_str()));
        status_->Invalidate();
    }
}

std::unique_ptr<JKWindow> CreatePcxViewerWindow(const std::string& filePath) {
    auto win = std::make_unique<ImageViewerWindow>();
    if (!filePath.empty()) {
        win->OpenPath(filePath);
    }
    return win;
}

// ---------------------------------------------------------------------------
// Single-process entry app (jkdesktop pcx [FILE]).

PcxApp::PcxApp(const std::string& filePath) : filePath_(filePath) {
}

void PcxApp::OnInit() {
    SetMainWindow(CreatePcxViewerWindow(filePath_));
    // First tick shows the startup open dialog when no path was given. The
    // dialog must be shown after the window manager registered the root
    // window, which happens after OnInit — hence the timer indirection.
    SetTimerInterval(120);
}

bool PcxApp::PreProcessMessage(const JKEvent& ev) {
    auto* viewer = static_cast<ImageViewerWindow*>(GetMainWindow());
    if (viewer) {
        if (ev.type == JKEventType::Timer) {
            viewer->MaybeShowStartupDialog();
            SetTimerInterval(0);  // one-shot purpose served
        } else if (ev.type == JKEventType::MouseWheel) {
            viewer->ForwardWheel(ev.dy);
        }
    }
    return JKApplication::PreProcessMessage(ev);
}

} // namespace jk