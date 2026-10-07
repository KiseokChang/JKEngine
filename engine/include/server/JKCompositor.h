#ifndef JKCOMPOSITOR_H
#define JKCOMPOSITOR_H

#include <server/JKCompositorLayer.h>
#include <server/JKCompositorOutput.h>
#include <ipc/JKWireProtocol.h>  // (T2) 와이어 DirtyRect 원용 — 신규 와이어 0
#include <server/JKFrameDirty.h>  // (T2) 프레임 더티 계산기(T1 착지물)
#include <SDL.h>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace jk {
namespace server {

// Server-side window chrome geometry (logical points, surface-local).
// MUST stay in sync with the frame painted by the client inside its surface:
// src/JKWindow.cpp OnRectChanged (kBorder=2, kTitle=24) and
// GetCloseButtonRect (20x20 at 2px inset from the top-right).
constexpr int kChromeTitleBar = 24;
constexpr int kChromeBorder = 2;
constexpr int kChromeCloseSize = 20;
constexpr int kChromeCloseMargin = 2;
// docs/39: maximize/restore button — same 20x20 box as the close X, sitting
// kChromeMaximizeGap px to its left in the title bar.
constexpr int kChromeMaximizeSize = 20;
constexpr int kChromeMaximizeGap = 2;
constexpr int kResizeHotspot = 6;

// docs/35: the rubber-band capture overlay's surface title. The server treats
// a client with this title as chromeless fullscreen: no close overlay, no
// title-bar drag, and the spawn placement resizes it to the whole desktop.
// Must match ClientSnapApp's JKAppMeta title.
constexpr const char* kCaptureOverlayTitle = "Region Capture";

// Server compositor: owns SDL textures for client surfaces and draws them into
// a single renderer output. For Phase 2 there is exactly one output.
// 스레드 규약(T2 배선 — 서버 Run 루프 스레드 전용): 실측 원문 — 커밋 수신점
// (MarkDirty/QueueCommitRects를 부르는 CommitSurface 핸들러,
// JKWindowServer.cpp ProcessClientMessage)과 Composite는 모두
// JKWindowServer::Run의 같은 스레드 위에 있다(ProcessPendingMessages 호출은
// Run:911, Composite는 Run:824/932 — 클라 리드 스레드는 PopMessage 큐까지만
// 채운다). 그래서 FrameDirtyAccumulator(T1 계약 — 내부 뮤텍스 없음, 단일
// 스레드 전제)를 그대로 쓴다. 예외는 커밋 rect 보류 큐 하나뿐 — 합정 원장
// (brief 함정 ①)대로 전용 뮤텍스로 방어한다.
class JKCompositor {
public:
    // window: 부분 업로드 경로(SW 렌더러)가 SDL_GetWindowSurface할 때 쓴다.
    // 서버는 하나의 윈도우를 소유하고 있다.
    JKCompositor(SDL_Renderer* renderer, SDL_Window* window);
    ~JKCompositor();

    // Add or remove layers.
    JKCompositorLayer* AddLayer(uint32_t id,
                                int width, int height,
                                const std::string& title,
                                uint8_t* pixels);
    void RemoveLayer(uint32_t id);

    // Move/resize a layer.
    void SetLayerPosition(uint32_t id, int x, int y);
    void SetLayerScale(uint32_t id, float sx, float sy);
    void SetLayerAlpha(uint32_t id, uint8_t alpha);
    // Shell role (docs/28): re-sorts so the shell stays topmost.
    void SetLayerShell(uint32_t id, bool shell);

    // Minimize support (docs/28 단계 4): hide/show a layer server-side —
    // the client keeps rendering and committing; only the draw is skipped.
    void SetLayerVisible(uint32_t id, bool visible);
    bool IsLayerVisible(uint32_t id);

    // Display height (logical points) of the shell layer, or 0 when no shell
    // is active. The window server reserves this much of the desktop as the
    // work area (window placement clamps + drag clamps).
    int ShellReserveHeight();

    // Mark a layer dirty and request texture update.
    void MarkDirty(uint32_t id);

    // (T2) 와이어 DirtyRect 수집 — CommitSurface 핸들러가 와이어 원문
    // (ipc::CommitSurfaceHeader.dirtyCount + DirtyRect[], 표면 좌표)을 버리지
    // 말고 여기로 전달한다(함정 봉합 ①: 전용 뮤텍스의 rect 큐). 표면 좌표→
    // 화면 좌표 매핑은 Composite 소비 시점에 T1 계산기 AddSurfaceRect가
    // 레이어 dst 산식(JKCompositor.cpp dst 산식 원용)으로 맡는다 — 그래서
    // 표면 rect 원문 그대로 큐에 적립한다(제시 시점 이전의 레이어 이동은
    // dst 추적 사건이 이전∪새 dst로 부채질하므로 매핑 시점 지연이 안전).
    // rect 수백 개 단위로 밀리면 fail-safe로 전체 프레젠트로 접고 큐를 비운다.
    void QueueCommitRects(uint32_t layerId, const ipc::DirtyRect* rects,
                          size_t count);

    // (T2) 보수적 전체 프레젠트 강제 — 오버레이 훅(스펙 결정 2: 훅이 실제로
    // 그린 프레임은 부분 rect가 부채질하는 회귀를 막는 안전망)·진단 회귀 등
    // 바깥 사건이 한 점으로 부른다. 다음 TakeDirty가 화면 전체 rect 단건을
    // 낸다(사건 유무 무관 — T1 ForceFull 계약).
    void RequestFullPresent();

    // Update output bounds (for now a single output covering the SDL window).
    void SetOutput(const JKCompositorOutput& output);
    float OutputScale() const { return output_.Scale(); }
    // Logical size of the single output (docs/35 capture_region clamping).
    int OutputWidth() const { return output_.Bounds().w; }
    int OutputHeight() const { return output_.Bounds().h; }

    // Sort layers by a z-order policy. Phase 2: focused client on top.
    void FocusLayer(uint32_t id);

    // Draw all visible layers into the framebuffer; present when asked.
    // capture_region (docs/35) draws without presenting to read pixels back.
    void Composite(bool present);

    // Id of the topmost visible layer (the focused one, or the last sorted
    // layer when focus is unset) — used to re-focus keyboard input after the
    // focused client disconnects. Returns 0 when no layers exist.
    uint32_t TopmostLayerId();

    // Replace a layer's texture and pixel source with a resized one. All
    // fields (texture, w/h, pixels, scale reset) swap atomically under the
    // layers mutex so Composite/HitTest never observe a mismatched pitch.
    // Main-thread only; returns false if the layer does not exist.
    bool ResizeLayer(uint32_t id, int width, int height, uint8_t* pixels);

    // Composite all layers to the renderer.
    void Composite();

    // Hit test in output coordinates returns the topmost layer at (x,y).
    JKCompositorLayer* HitTest(int x, int y);

    // Public lookup for the window server's chrome drag state (layer
    // pointers die on RemoveLayer, so callers store ids and re-resolve).
    JKCompositorLayer* FindLayerById(uint32_t id);

    // 승인 대상 시각화 훅 (스펙 2026-09-19-app-tool-hub §5 1단): Composite가
    // 모든 레이어(크롬 오버레이 포함)를 그린 뒤 present 직전에 한 번 부른다 —
    // DrawCloseOverlay와 같은 컴포지트 패스의 "레이어 위" 드로잉 단계. 서버만
    // pendingApprovals_를 알므로 컴포지터는 함수만 받는다(의존성 역전 —
    // DrawCloseOverlay의 면제 규칙은 훅 구현자가 동일 적용한다). Composite(false)
    // (capture_region 리드백)에서도 그려진다 — 화면에 보이는 것이 곧 캡처다.
    using OverlayHook = std::function<void(float outputScale)>;
    void SetOverlayHook(OverlayHook hook) { overlayHook_ = std::move(hook); }
    // (T2 스펙 결정 2): 훅이 실제로 그린 프레임은 사다리에서 전체 프레젠트로
    // 간다 — 훅 구현자가 그린 그때만 RequestFullPresent()를 부른다(매 프레임
    // 무조건 full이면 더티프레젠트의 이득 자체가 사라진다).

    // 닫기(X) 버튼 호버 (2026-10-05 사용자 눈확인 수리 — I4 원장 "컴포지터
    // 오버레이 복제" 봉합): 서버가 마우스 커서가 어느 레이어의 X 존에
    // 있는지 판정한 결과(레이어 id, 0=없음)를 그리는 쪽에 흘린다. 상태의
    // 판정은 윈도 서버(TryChromeGrab 존 1과 동일 산식), 컴포지터는 미러
    // 그리기만 — Docs 39의 maximize 플래그와 같은 방향.
    // (T2): 호버 전이는 커밋 없는 글리프 변화다 — 보수적 전체 프레젠트
    // (스펙 결정 2: 커밋 없는 변화의 안전망; 전이 시에만 Composite가
    // 실리므로 비용은 기존 관행과 동일).
    void SetCloseHoverLayer(uint32_t id) {
        if (closeHoverLayerId_ != id) {
            closeHoverLayerId_ = id;
            frameDirty_.ForceFull();
        }
    }

private:
    SDL_Renderer* renderer_ = nullptr;
    SDL_Window* window_ = nullptr;  // (T2) 부분 업로드 경로의 창 표면 원료
    JKCompositorOutput output_{0, JKRect{0, 0, 0, 0}, 1.0f};

    std::mutex layersMutex_;
    std::vector<std::unique_ptr<JKCompositorLayer>> layers_;
    uint32_t focusedId_ = 0;
    uint32_t closeHoverLayerId_ = 0;

    // (T2) 더티프레젠트 상태 — 스레드 규약은 위 클래스 주석(서버 Run 스레드
    // 전용, 계산기 락 없음). commitRectsMutex_는 커밋 rect 보류 큐만 본다
    // (함정 ① — 큐의 소비는 layersMutex_와 절대 중첩하지 않는다: Composite는
    // 큐를 먼저 drain해 놓고 나서 layersMutex_를 잡는다, QueueCommitRects는
    // 큐만 잡는다 — 락 순서 사이클 자체가 없다).
    struct PendingCommitRect {
        uint32_t layerId = 0;
        ipc::DirtyRect rect{};
    };
    // 커밋 rect 보류 큐 풍량 가드(spin-safe — 스펙 결정 2 원장 "사건 수십~수백
    // 선"): 이 값을 넘으면 수집 실패 취급 — 큐를 비우고 다음 제시를 전체로.
    static constexpr size_t kMaxPendingCommitRects = 2048;
    std::mutex commitRectsMutex_;
    std::vector<PendingCommitRect> pendingCommitRects_;
    // 커밋 rect가 없는 dirty 레이어(이동·alpha·resize 등 커밋 없는 변화)를
    // dst 전체로 봉합하기 위한 "이번 프레임에 커밋 rect를 본 레이어" 표지 —
    // Composite가 소비한다.
    std::map<uint32_t, bool> commitRectSeen_;
    // 프레임 더티 계산기(T1 착지물 — 단일 스레드 규약).
    FrameDirtyAccumulator frameDirty_;
    // 부트 첫 제시(renderedOnce) = 전체 — 이전 제시 상태가 없는 프레임.
    bool presentedOnce_ = false;
    // 마지막 "제시된" 화면의 레이어 dst — skip 프레임(제시 스킵)에는 화면이
    // 그 제시 상태 그대로므로 갱신하지 않는다(화면 진실원 유지).
    std::map<uint32_t, SDL_Rect> lastPresentedDst_;

    JKCompositorLayer* FindLayer(uint32_t id);
    void UpdateLayerTexture(JKCompositorLayer& layer);
    void SortLayers();
    void DrawCloseOverlay(const JKCompositorLayer& layer, float scale);
    // docs/39: maximize/restore button left of the close X. The glyph
    // (maximize vs. restore) follows the layer's Maximized() flag — the
    // server owns the state, the compositor only mirrors it for drawing.
    void DrawMaximizeButton(const JKCompositorLayer& layer, float scale);
    OverlayHook overlayHook_;

    // (T2) 레이어 dst 산식 원문의 한 점 추출 — Composite 본체와 T2 보수 사건
    // (이동/표시/alpha/제거)이 같은 산식을 쓴다.
    SDL_Rect ComputeLayerDst(const JKCompositorLayer& layer,
                             float outputScale) const;
    // (T2) SW 렌더러 판정(SDL_GetRendererInfo — "software"). 판정 실패는
    // false(SW 밖 취급) — 부분 경로로 가지 않는 fail-safe 방향.
    bool IsSoftwareRenderer() const;
    // (T2) 부분 업로드 경로(SW 전용): 더 dirty rect마다 RenderReadPixels(백
    // 버퍼→rect 버퍼) → 창 표면 행 복사 → SDL_UpdateWindowSurfaceRects 일괄
    // 업로드. 어떤 단계라 실패하면 false — 사다리가 전체 프레젠트로 폴백한다
    // (기본 회귀 없음). SW 밖 렌더러에서는 즉시 false(GL 전체 전송 stall 봉합).
    bool PresentPartialSurface(const std::vector<SDL_Rect>& dirty);
};

} // namespace server
} // namespace jk

#endif // JKCOMPOSITOR_H
