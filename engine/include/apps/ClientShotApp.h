#ifndef CLIENTSHOTAPP_H
#define CLIENTSHOTAPP_H

// Screenshot viewer (agent platform, docs/35 Task 4): lists <exeDir>\state
// \screenshots\*.png newest-first, decodes the selection with LoadImageFile
// into an SDL streaming texture, and draws it via ImGui::Image. The 영역
// 캡처 button spawns the rubber-band overlay (launch_app snap). Texture
// lifetime: one per selection, destroyed on reselect/refresh/close.

#include <client/JKClientApplication.h>
#include <JKImageLoader.h>
#include <string>
#include <vector>

struct SDL_Texture;

namespace jk {

class ClientShotApp : public JKClientApplication {
public:
    ClientShotApp() = default;
    ~ClientShotApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    bool PreProcessMessage(const JKEvent& ev) override;
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;

private:
    void BuildUi(int w, int h);
    // Re-scan <exeDir>\state\screenshots for *.png, newest name first (the
    // server's shot_<ts>_<...>.png prefix sorts lexically by time). Invalidates
    // the current selection if its file vanished.
    void RefreshList();
    // Decode + upload the selected file (deferred to RenderOverlay, where the
    // SDL renderer lives). A failed decode clears the texture, not the app.
    void SelectFile(const std::string& fullPath);
    void DropTexture();
    static std::string ExeDir();

    bool frameDirty_ = true;
    bool imguiReady_ = false;

    std::vector<std::string> files_;   // file names, newest first
    std::string selectedPath_;         // full path of the shown image
    LoadedImage current_;              // decoded pixels of selectedPath_
    SDL_Texture* texture_ = nullptr;   // GPU copy of current_ (RGBA32)
    int texW_ = 0;
    int texH_ = 0;
    SDL_Renderer* renderer_ = nullptr; // RenderOverlay's renderer, stashed
                                       // for the deferred texture upload
};

} // namespace jk

#endif // CLIENTSHOTAPP_H