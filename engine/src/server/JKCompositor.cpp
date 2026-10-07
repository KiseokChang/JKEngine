#include <server/JKCompositor.h>

#include "theme/JKTheme.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>  // std::getenv (T2 — JK_PRESENT_FORCE_FULL)
#include <cstring>  // std::strcmp / std::memmove (T2 — SW 판정·행 복사)

namespace jk {
namespace server {

JKCompositor::JKCompositor(SDL_Renderer* renderer, SDL_Window* window)
    : renderer_(renderer), window_(window) {
}

JKCompositor::~JKCompositor() {
    std::lock_guard<std::mutex> lock(layersMutex_);
    for (auto& layer : layers_) {
        if (layer && layer->Texture()) {
            SDL_DestroyTexture(layer->Texture());
            layer->SetTexture(nullptr);
        }
    }
    layers_.clear();
}

JKCompositorLayer* JKCompositor::AddLayer(uint32_t id,
                                            int width, int height,
                                            const std::string& title,
                                            uint8_t* pixels) {
    if (!renderer_ || width <= 0 || height <= 0) {
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer_,
                                             SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             width, height);
    if (!texture) {
        std::fprintf(stderr, "JKCompositor::AddLayer: SDL_CreateTexture failed: %s\n",
                     SDL_GetError());
        return nullptr;
    }
    // Fit-scale 레이어(1920x1080 surface를 0.63배 축소 표시)는 nearest 샘플링으로는
    // 축소 과정에서 픽셀 행이 통째로 버려져 surface에 래스터화된 글자가 깨진다.
    // 선형 필터로 샘플링해 축소 텍스트가 부드럽게 보이도록 한다. 1:1 레이어는
    // 리샘플링이 일어나지 않아 영향 없음.
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
    // Per-pixel alpha must blend over the desktop (the docs/35 capture
    // overlay is a whole-screen (0,0,0,70) dim). This SDL build does not
    // default STREAMING textures to BLEND — without this the dim renders
    // as an opaque black screen (실측: region readback = (0,0,0,255)).
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    JKCompositorLayer* raw = nullptr;
    {
        std::lock_guard<std::mutex> lock(layersMutex_);
        auto layer = std::make_unique<JKCompositorLayer>(id, width, height, title);
        layer->SetTexture(texture);
        layer->SetPixels(pixels);
        layer->MarkDirty();
        raw = layer.get();
        layers_.push_back(std::move(layer));
    }
    // Focus is applied by the caller to avoid a nested layersMutex_ lock.
    return raw;
}

void JKCompositor::RemoveLayer(uint32_t id) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    // Plain erase loop, NOT remove_if+erase: remove_if moves kept unique_ptrs
    // forward, leaving MOVED-FROM (null) unique_ptrs in the tail — the old
    // code then dereferenced (*it)->Texture() on a null element whenever the
    // removed layer was not the vector's last element (killing a non-focused
    // client, e.g. three at once from Task Manager, crashed the server).
    for (auto it = layers_.begin(); it != layers_.end(); ++it) {
        if (*it && (*it)->Id() == id) {
            // (T2) 보수 사건: 제거돼 없어진 자리 = 데스크톱 배경이 다시
            // 보이는 rect — 커밋 없는 변화의 dst 전체 봉합(배선 불변의 반쪽).
            frameDirty_.AddDirtyLayerRect(
                id, ComputeLayerDst(**it, output_.Scale()));
            lastPresentedDst_.erase(id);
            if ((*it)->Texture()) {
                SDL_DestroyTexture((*it)->Texture());
            }
            layers_.erase(it);
            break;
        }
    }
    if (focusedId_ == id) {
        focusedId_ = 0;
    }
}

void JKCompositor::SetLayerPosition(uint32_t id, int x, int y) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    if (JKCompositorLayer* layer = FindLayer(id)) {
        layer->SetPosition(x, y);
        // (T2) 이동은 사건 rect 없이는 소각되지 않는다 — dst 추적(Composite
        // 루프의 lastPresentedDst_ 대조)이 이전∪새 dst를 봉합한다. 여기서는
        // 중복 두지 않는다(같은 프레임에 두 사건이 겹치면 병합만 낭비).
    }
}

void JKCompositor::SetLayerScale(uint32_t id, float sx, float sy) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    if (JKCompositorLayer* layer = FindLayer(id)) {
        layer->SetScale(sx, sy);
        // (T2) 스케일 변화도 커밋 없는 변화 — dst 크기 변화로 dst 추적이
        // 봉합한다(Composite 루프).
    }
}

void JKCompositor::SetLayerAlpha(uint32_t id, uint8_t alpha) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    if (JKCompositorLayer* layer = FindLayer(id)) {
        if (layer->Alpha() == alpha) {
            return;  // 무변화 사건은 rect를 부채질하지 않는다
        }
        layer->SetAlpha(alpha);
        // (T2) alpha 변화 = dst 전체의 블렌드 변화(커밋 없는 변화) → dst 전체.
        frameDirty_.AddDirtyLayerRect(id, ComputeLayerDst(*layer, output_.Scale()));
    }
}

void JKCompositor::SetLayerShell(uint32_t id, bool shell) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    if (JKCompositorLayer* layer = FindLayer(id)) {
        if (layer->IsShell() == shell) {
            return;
        }
        layer->SetShell(shell);
        // Keep the shell topmost right away (full sort policy in SortLayers).
        SortLayers();
        // (T2) 스펙 결정 2 보수: Z 순서 재정렬 = rect 사건 없이 시각 상태가
        // 바뀐다 — 전체 프레젠트.
        frameDirty_.ForceFull();
    }
}

int JKCompositor::ShellReserveHeight() {
    std::lock_guard<std::mutex> lock(layersMutex_);
    for (auto& layer : layers_) {
        if (layer && layer->IsShell() && layer->IsVisible()) {
            return static_cast<int>(layer->Height() * layer->ScaleY());
        }
    }
    return 0;
}

void JKCompositor::SetLayerVisible(uint32_t id, bool visible) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    JKCompositorLayer* layer = FindLayer(id);
    if (!layer || layer->IsVisible() == visible) {
        return;  // 무변화 표시 — rect 부채질 없음(기존 관행 유지)
    }
    layer->SetVisible(visible);
    if (!visible) {
        // (T2) 보수 사건: 감춰진 자리 = 배경이 다시 보인다 — dst 전체 봉합 +
        // dst 추적 소각. 다시 보일 때(추적 부재 레이어)는 Composite 루프의
        // dst 추적이 dst 전체를 봉합한다(배선 불변 — 커밋 없는 변화도 rect로).
        frameDirty_.AddDirtyLayerRect(id, ComputeLayerDst(*layer, output_.Scale()));
        lastPresentedDst_.erase(id);
    }
}

bool JKCompositor::IsLayerVisible(uint32_t id) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    JKCompositorLayer* layer = FindLayer(id);
    return layer ? layer->IsVisible() : false;
}

bool JKCompositor::ResizeLayer(uint32_t id, int width, int height, uint8_t* pixels) {
    if (!renderer_ || width <= 0 || height <= 0) {
        return false;
    }
    SDL_Texture* texture = SDL_CreateTexture(renderer_,
                                             SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             width, height);
    if (!texture) {
        std::fprintf(stderr, "JKCompositor::ResizeLayer: SDL_CreateTexture failed: %s\n",
                     SDL_GetError());
        return false;
    }
    // AddLayer와 동일한 이유(축소 레이어 글자 깨짐 방지 + per-pixel alpha 블렌드).
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    std::lock_guard<std::mutex> lock(layersMutex_);
    JKCompositorLayer* layer = FindLayer(id);
    if (!layer) {
        SDL_DestroyTexture(texture);
        return false;
    }
    if (layer->Texture()) {
        SDL_DestroyTexture(layer->Texture());
    }
    layer->SetTexture(texture);
    layer->SetPixels(pixels);
    layer->SetSize(width, height);
    // The stretch preview during a resize drag uses Scale; reset it here so
    // the new texture is drawn at its native size.
    layer->SetScale(1.0f, 1.0f);
    layer->MarkDirty();
    return true;
}

void JKCompositor::MarkDirty(uint32_t id) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    if (JKCompositorLayer* layer = FindLayer(id)) {
        layer->MarkDirty();
    }
}

// (T2) 와이어 DirtyRect 수집(스펙 결정 2 — 와이어 원용): CommitSurface
// 핸들러(JKWindowServer.cpp ProcessClientMessage)가 폐기하던 DirtyRect[]를
// 표면 좌표 원문 그대로 보류 큐에 적립한다. 매핑은 Composite 소비 시점의
// AddSurfaceRect가 맡는다(헤더 주석 — 이동 부채질 근거 포함). 큐만의
// 뮤텍스(brief 함정 ①) — layersMutex_와의 중첩은 한 번도 없다.
// 풍량 가드: rect가 수천 단위로 밀리면(프레임당 사건 수십~수백 선 원장의
// 이탈) 수집 실패 취급 — 보류 큐를 비우고 다음 제시를 전체로 접는다.
void JKCompositor::QueueCommitRects(uint32_t layerId,
                                    const ipc::DirtyRect* rects,
                                    size_t count) {
    if (!rects || count == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(commitRectsMutex_);
    if (pendingCommitRects_.size() + count > kMaxPendingCommitRects) {
        pendingCommitRects_.clear();
        frameDirty_.ForceFull();
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        pendingCommitRects_.push_back(PendingCommitRect{layerId, rects[i]});
    }
}

// (T2) 보수적 전체 프레젠트 강제 — 오버레이 훅이 실제로 그린 프레임 등이
// 한 점으로 부른다(헤더 주석).
void JKCompositor::RequestFullPresent() {
    frameDirty_.ForceFull();
}

void JKCompositor::SetOutput(const JKCompositorOutput& output) {
    output_ = output;
}

void JKCompositor::FocusLayer(uint32_t id) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    if (!FindLayer(id)) {
        return;
    }
    if (focusedId_ == id) {
        return;  // 무변화 재포커스 — 전체 프레젠트 반복 낭비 없음
    }
    focusedId_ = id;
    SortLayers();
    // (T2) 스펙 결정 2 보수: 포커스 재정렬(SortLayers 결과 변화) = rect 사건
    // 없이 Z 순서 시각 상태가 바뀐다 — 전체 프레젠트.
    frameDirty_.ForceFull();
}

uint32_t JKCompositor::TopmostLayerId() {
    std::lock_guard<std::mutex> lock(layersMutex_);
    for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
        // The shell never receives focus fallback (docs/28) — keyboard input
        // must stay with app windows.
        if (*it && (*it)->IsVisible() && !(*it)->IsShell()) {
            return (*it)->Id();
        }
    }
    return 0;
}

JKCompositorLayer* JKCompositor::FindLayer(uint32_t id) {
    for (auto& layer : layers_) {
        if (layer && layer->Id() == id) {
            return layer.get();
        }
    }
    return nullptr;
}

JKCompositorLayer* JKCompositor::FindLayerById(uint32_t id) {
    std::lock_guard<std::mutex> lock(layersMutex_);
    return FindLayer(id);
}

void JKCompositor::UpdateLayerTexture(JKCompositorLayer& layer) {
    if (!layer.Texture() || !layer.Pixels()) {
        return;
    }
    SDL_UpdateTexture(layer.Texture(), nullptr, layer.Pixels(),
                      layer.Width() * 4);
    layer.ClearDirty();
}

void JKCompositor::SortLayers() {
    // Stable sort: non-focused layers come before the focused layer so it is
    // rendered last (on top). Null entries are sorted to the back. Shell
    // layers (docs/28) outrank everything — the focused window never covers
    // the taskbar.
    std::stable_sort(layers_.begin(), layers_.end(),
        [this](const std::unique_ptr<JKCompositorLayer>& a,
               const std::unique_ptr<JKCompositorLayer>& b) {
            const bool aValid = a != nullptr;
            const bool bValid = b != nullptr;
            if (!aValid || !bValid) {
                return !aValid < bValid;
            }
            if (a->IsShell() != b->IsShell()) {
                return !a->IsShell() && b->IsShell();
            }
            const bool aFocused = (a->Id() == focusedId_);
            const bool bFocused = (b->Id() == focusedId_);
            return !aFocused && bFocused;
        });
}

void JKCompositor::Composite() {
    Composite(true);
}

void JKCompositor::Composite(bool present) {
    if (!renderer_) {
        return;
    }

    // The caller is responsible for clearing the framebuffer (e.g. with the
    // launcher/desktop background) before calling Composite(). We only draw
    // client layers on top so the background is preserved.
    //
    // All drawing is in physical pixels. Layer positions/sizes are stored in
    // SDL logical points, so we multiply by the output scale (physW/logW) to
    // get the physical-pixel destination rect. The mouse hit-test uses the
    // same physical-pixel space, so what you see is what you click.

    const float outputScale = output_.Scale();

    // 합성 분해 계측 (docs/78 TX7 잔여 — 폰 합성 성질 규명: 레이어 blit 0.5ms
    // vs present 140ms — 폰 present가 전부, 폰 SW 전환 후 70ms):
    // 단계 소요를 JK_CPU_TRACE=1일 때만 stderr로. 벗기려면 이 블록만.
    static const bool s_trace = std::getenv("JK_CPU_TRACE") != nullptr;
    const auto ph0 = std::chrono::steady_clock::now();

    // (T2) 화면 원료 — 역치/ForceFull의 전체 rect 원료에 프레임마다 갱신한다
    // (T1 계약 handoff §6: SetScreenSize 매 Frame — 출력 리사이즈 대응).
    int screenW = 0, screenH = 0;
    SDL_GetRendererOutputSize(renderer_, &screenW, &screenH);
    frameDirty_.SetScreenSize(screenW, screenH);

    // (T2) 와이어 DirtyRect 수수 — 커밋 수신점(QueueCommitRects · 커밋 핸들러
    // 와 같은 서버 Run 스레드)이 쌓아둔 보류 큐를 drain한다. 수집→소비 스레드
    // 판정은 위 클래스 주석(같은 Run 스레드 — 계산기 단일 스레드 전제 성립).
    std::vector<PendingCommitRect> pending;
    {
        std::lock_guard<std::mutex> lock(commitRectsMutex_);
        pending.swap(pendingCommitRects_);
    }

    {
        std::lock_guard<std::mutex> lock(layersMutex_);
        commitRectSeen_.clear();

        // (T2) 커밋 rect 매핑(표면→화면) — T1 계산기 AddSurfaceRect 원용
        // (오버플레이/화면 경계 클램프 내장). 비가시 레이어의 커밋은 그릴 곳이
        // 없으므로 사건 소각 — 다시 보일 때 dst 추적이 봉합한다.
        for (const PendingCommitRect& pc : pending) {
            JKCompositorLayer* layer = FindLayer(pc.layerId);
            if (!layer || !layer->IsVisible() || !layer->Texture()) {
                continue;
            }
            frameDirty_.AddSurfaceRect(
                pc.layerId, layer->Width(), layer->Height(),
                layer->ScaleX(), layer->ScaleY(),
                static_cast<int>(layer->X() * outputScale),
                static_cast<int>(layer->Y() * outputScale), pc.rect);
            commitRectSeen_[pc.layerId] = true;
        }

        for (auto& layer : layers_) {
            if (!layer || !layer->IsVisible() || !layer->Texture()) {
                continue;
            }

            const SDL_Rect dst = ComputeLayerDst(*layer, outputScale);
            // (T2) dst 추적 — 마지막 "제시된" 화면의 dst와 달라지면 이전∪새
            // dst가 제시를 끈다(이동·리사이즈·fit 변화 = 커밋 없는 변화).
            // skip 프레임(제시 스킵)에는 화면이 마지막 제시 상태 그대로므로
            // 추적을 갱신하지 않는다 — 화면 진실원(lastPresentedDst_) 유지.
            if (present) {
                auto seen = lastPresentedDst_.find(layer->Id());
                if (seen == lastPresentedDst_.end()) {
                    // 새 (또는 재현) 레이어 — 이전 화면에 없던 rect 한 통.
                    frameDirty_.AddDirtyLayerRect(layer->Id(), dst);
                    lastPresentedDst_.emplace(layer->Id(), dst);
                } else if (seen->second.x != dst.x || seen->second.y != dst.y ||
                           seen->second.w != dst.w || seen->second.h != dst.h) {
                    frameDirty_.AddLayerMove(layer->Id(), seen->second, dst);
                    seen->second = dst;
                }
            }

            if (layer->IsDirty()) {
                UpdateLayerTexture(*layer);
                if (commitRectSeen_.find(layer->Id()) == commitRectSeen_.end()) {
                    // (T2) 배선 불변: 레이어가 dirty면 rect가 반드시 채워진다 —
                    // 커밋 rect(위) 또는 dst 전체(여기, 커밋 없는 변화 —
                    // 이동·alpha·resize·MarkDirty 원용 등) 둘 중 하나.
                    frameDirty_.AddDirtyLayerRect(layer->Id(), dst);
                }
            }

            uint8_t alpha = layer->Alpha();
            SDL_SetTextureAlphaMod(layer->Texture(), alpha);

            SDL_RenderCopy(renderer_, layer->Texture(), nullptr, &dst);

            // The client paints the title bar inside its surface, but a
            // parentless main window paints no close button — the server
            // draws the close overlay at the top-right of every layer.
            // Shell layers have no chrome at all (docs/28); neither does the
            // capture overlay (docs/35) — a grey X would end up in the
            // user's face for the lifetime of the drag. Fullscreen layers
            // (vplayer spec §2.1) have no chrome either — the app owns the
            // whole surface including the top strip.
            if (!layer->IsShell() && layer->Title() != kCaptureOverlayTitle &&
                !layer->IsFullscreen()) {
                DrawCloseOverlay(*layer, outputScale);
                // docs/39: the maximize/restore button shares the close
                // overlay's guard — shell layers and the capture overlay get
                // neither button.
                DrawMaximizeButton(*layer, outputScale);
            }
        }
    }

    // 승인 대상 시각화 훅 (스펙 2026-09-19-app-tool-hub §5 1단): 반드시 레이어
    // 루프 밖(위의 layersMutex_ 스코프 해제 후)에서 부른다 — 훅이 레이어를
    // FindLayerById로 다시 찾으므로(layersMutex_ 재획득) 락 보유 중 호출이면
    // std::mutex 교착. present 전이라서 이번 프레임에 바로 화면에 오른다.
    const auto ph1 = std::chrono::steady_clock::now();
    if (overlayHook_) {
        overlayHook_(outputScale);
    }
    const auto ph2 = std::chrono::steady_clock::now();

    // (T2) 제시 사다리(스펙 결정 3 — fail-safe, 기본 회귀 없음):
    //   더티 0 = 제시 스킵(변화 0 프레임의 업로드 0)
    //   SW 밖 렌더러 / FORCE_FULL env = 전체(GL RenderReadPixels는 전체 전송
    //     stall — 봉합 ④)
    //   부분(SW 전용) = RenderReadPixels → 창 표면 행 복사 →
    //     SDL_UpdateWindowSurfaceRects 일괄 업로드 — 실패 = 전체 프레젠트
    // TakeDirty는 ForceFull/역치 40%를 이미 화면 전체 rect 단건으로 접어 놓는다
    // (T1 계약) — 사다리에서는 env·SW 판정·전체 단건 3검이 남는다. 안전 근거
    // (스펙 결정 3): 컴포지터는 합성마다 배경+전 레이어를 다시 그리므로
    // 백버퍼가 언제나 "완전한 새 프레임"이고, 제시 안 한 부분은 마지막 제시
    // 내용과 결합해도 idempotent하다.
    size_t nRects = 0;
    bool fullPath = false;
    if (present) {
        // 첫 제시(renderedOnce) = 전체 — 이전 제시 상태가 없는 프레임(부트).
        if (!presentedOnce_) {
            frameDirty_.ForceFull();
        }
        std::vector<SDL_Rect> dirty;
        if (frameDirty_.TakeDirty(dirty)) {
            presentedOnce_ = true;
            nRects = dirty.size();
            static const bool s_forceFullEnv = []() {
                const char* v = std::getenv("JK_PRESENT_FORCE_FULL");
                return v && v[0] != '\0' && std::strcmp(v, "0") != 0;
            }();
            const bool whole =
                nRects == 1 && dirty[0].x <= 0 && dirty[0].y <= 0 &&
                dirty[0].w >= screenW && dirty[0].h >= screenH;
            if (whole || s_forceFullEnv || !IsSoftwareRenderer()) {
                fullPath = true;  // 전체(기존 단일 SDL_RenderPresent)
                SDL_RenderPresent(renderer_);
            } else if (PresentPartialSurface(dirty)) {
                fullPath = false;  // 부분(SW 전용 경로 — 봉합 ④ 게이트 통과)
            } else {
                fullPath = true;   // 부분 실패 = fail-safe 전체 프레젠트
                SDL_RenderPresent(renderer_);
            }
        }
    }
    const auto ph3 = std::chrono::steady_clock::now();

    // [tmp] docs/78 잔여 합성 분해 — 레이어 blit/오버레이/present 소요(ms).
    // 레이어 스코프 진입(ph0) 이후 기준. shell draw는 서버 Composite() 측에서.
    // (T2 스펙 결정 5) present 표기 확장: full | dirty(N) | skip(제시 스킵 —
    // 결정 5의 full|dirty(N)은 실제 제시 표기; skip은 "변화 0 프레임 = 업로드
    // 0"의 영수증, A/B 실측(T3)이 이 3종을 구분해 센다).
    if (s_trace) {
        const auto ms = [](const auto& a, const auto& b) {
            return std::chrono::duration<double, std::milli>(b - a).count();
        };
        static int s_compCount = 0;
        if (++s_compCount % 8 == 1) {
            if (nRects == 0) {
                std::fprintf(stderr,
                             "[compst] layers=%.1f overlay=%.1f present=skip\n",
                             ms(ph0, ph1), ms(ph1, ph2));
            } else if (fullPath) {
                std::fprintf(stderr,
                             "[compst] layers=%.1f overlay=%.1f "
                             "present=full=%.1f\n",
                             ms(ph0, ph1), ms(ph1, ph2), ms(ph2, ph3));
            } else {
                std::fprintf(stderr,
                             "[compst] layers=%.1f overlay=%.1f "
                             "present=dirty(%zu)=%.1f\n",
                             ms(ph0, ph1), ms(ph1, ph2), nRects, ms(ph2, ph3));
            }
            std::fflush(stderr);
        }
    }
}

// (T2) 레이어 dst 산식 원문의 한 점 추출(JKCompositor dst 산식 — Composite
// 본체와 T2 보수 사건이 같은 산식을 쓴다: 논리 포인트 × 레이어 스케일 ×
// outputScale = 물리 화면 픽셀).
SDL_Rect JKCompositor::ComputeLayerDst(const JKCompositorLayer& layer,
                                       float outputScale) const {
    return SDL_Rect{
        static_cast<int>(layer.X() * outputScale),
        static_cast<int>(layer.Y() * outputScale),
        static_cast<int>(layer.Width() * layer.ScaleX() * outputScale),
        static_cast<int>(layer.Height() * layer.ScaleY() * outputScale)};
}

// (T2) SW 렌더러 판정 — 창 표면 경로와 RenderReadPixels가 값싸게 유효한
// 백엔드가 "software"(SDL_GetRendererInfo)다. 판정 실패는 SW 밖 취급 = 사다리
// 전체 프레젠트(fail-safe 방향).
bool JKCompositor::IsSoftwareRenderer() const {
    SDL_RendererInfo info{};
    if (SDL_GetRendererInfo(renderer_, &info) != 0 || info.name == nullptr) {
        return false;
    }
    return std::strcmp(info.name, "software") == 0;
}

// (T2) 부분 업로드 경로 — SW 렌더러 전용(스펙 결정 3). 더 dirty rect마다
// RenderReadPixels(백버퍼 → rect 크기 버퍼) → 창 표면(SDL_GetWindowSurface)에
// 행 복사 → 마지막에 SDL_UpdateWindowSurfaceRects 일괄 업로드(bbox 병합 목록의
// 무중첩 rect — 서로 겹치지 않으므로 rect별 업로드 중복 없음). 소프트웨어
// 렌더러의 목표는 곧 창 표면이므로(렌더가 이미 표면에 찍혔다) 행 복사는
// self-copy에 가깝고, 유효 작업은 rect 업로드다 — 그래도 스펙 파이프라인
// 원문(ReadPixels→행 복사→UpdateWindowSurfaceRects)을 그대로 밟는다: 백버퍼와
// 표면이 분리된 백엔드에서도 동일하게 유효한 형태(결정 3의 안전 근거).
bool JKCompositor::PresentPartialSurface(const std::vector<SDL_Rect>& dirty) {
    if (!window_ || dirty.empty()) {
        return false;  // fail-safe — 사다리가 전체 프레젠트로 폴백한다
    }
    // SW 게이트(봉합 ④): GL에서 RenderReadPixels는 전체 전송 stall — SW만 통과.
    if (!IsSoftwareRenderer()) {
        return false;
    }
    SDL_Surface* surface = SDL_GetWindowSurface(window_);
    if (!surface || !surface->pixels || surface->format == nullptr ||
        surface->format->BytesPerPixel <= 0) {
        return false;  // 서피스 획득 실패 = 전체 프레젠트(fail-safe)
    }
    const int bpp = surface->format->BytesPerPixel;
    // rect 절단(창 표면 경계) — 화면 경계 클램프는 T1 계산기가 이미 했지만
    // 표면이 렌더러 출력보다 작은 백엔드 변동을 여기서도 막아둔다.
    std::vector<SDL_Rect> rects;
    rects.reserve(dirty.size());
    SDL_Rect bounds{0, 0, surface->w, surface->h};
    for (const SDL_Rect& r : dirty) {
        SDL_Rect clipped{};
        if (SDL_IntersectRect(&bounds, &r, &clipped) &&
            clipped.w > 0 && clipped.h > 0) {
            rects.push_back(clipped);
        }
    }
    if (rects.empty()) {
        return false;
    }
    // rect마다 백버퍼 판독 → 표면 행 복사(일괄 업로드를 위해 읽기를 먼저
    // 몰아서 하지 않는 이유: rect별 버퍼가 커질 뿐이라 — 최대 rect 버퍼 1개를
    // 재사용한다). 행 복사는 memmove — SW 렌더러에선 읽기 원본과 복사 대상이
    // 같은 메모리일 수 있다(오버랩·자기 복사 무해화).
    const bool lockNeeded = SDL_MUSTLOCK(surface) != 0;
    if (lockNeeded && SDL_LockSurface(surface) != 0) {
        return false;
    }
    size_t maxBytes = 0;
    for (const SDL_Rect& c : rects) {
        maxBytes = std::max(maxBytes, static_cast<size_t>(c.w) * bpp *
                                          static_cast<size_t>(c.h));
    }
    std::vector<uint8_t> buf(maxBytes);
    bool ok = true;
    for (const SDL_Rect& c : rects) {
        const size_t rowBytes = static_cast<size_t>(c.w) * bpp;
        if (SDL_RenderReadPixels(renderer_, &c, surface->format->format,
                                 buf.data(), static_cast<int>(rowBytes)) != 0) {
            ok = false;
            break;
        }
        uint8_t* dstRow = static_cast<uint8_t*>(surface->pixels) +
                          static_cast<size_t>(c.y) * surface->pitch +
                          static_cast<size_t>(c.x) * bpp;
        if (dstRow == buf.data()) {
            continue;  // self-copy — 표면이 곧 판독지(SW 렌더러 상용 모양)
        }
        for (int y = 0; y < c.h; ++y) {
            std::memmove(dstRow + static_cast<size_t>(y) * surface->pitch,
                         buf.data() + static_cast<size_t>(y) * rowBytes,
                         rowBytes);
        }
    }
    if (lockNeeded) {
        SDL_UnlockSurface(surface);
    }
    if (!ok) {
        return false;  // 판독 실패 = 전체 프레젠트(fail-safe)
    }
    if (SDL_UpdateWindowSurfaceRects(window_, rects.data(),
                                     static_cast<int>(rects.size())) != 0) {
        return false;  // 업로드 실패 = 전체 프레젠트(fail-safe)
    }
    return true;
}

void JKCompositor::DrawCloseOverlay(const JKCompositorLayer& layer, float scale) {
    if (!renderer_) {
        return;
    }
    // Mirrors JKWindow::GetCloseButtonRect / close-button painting
    // (src/JKWindow.cpp): 20x20 at 2px inset from the top-right, 5px X pad.
    // colors: jk::theme::current() (P2)
    // The rect lives in SURFACE px like the chrome hit-test zones
    // (JKWindowServer::TryChromeGrab), so it shrinks proportionally on
    // fit-scaled layers — Width()*ScaleX() is the on-screen width.
    const SDL_Rect btn{
        static_cast<int>((layer.X() + (layer.Width() - kChromeCloseSize - kChromeCloseMargin) * layer.ScaleX()) * scale),
        static_cast<int>((layer.Y() + kChromeCloseMargin * layer.ScaleY()) * scale),
        static_cast<int>(kChromeCloseSize * layer.ScaleX() * scale),
        static_cast<int>(kChromeCloseSize * layer.ScaleY() * scale)
    };
    const int pad = static_cast<int>(5 * layer.ScaleX() * scale);
    const auto& t = jk::theme::current();

    // 호버면 chromeCloseHover로 치환(JKWindow.cpp 클라 페인트와 동일 계약 —
    // 글리프 X 색은 유지). 상태 판정은 JKWindowServer::UpdateCloseHover.
    const auto& face = (layer.Id() == closeHoverLayerId_)
                           ? t.chromeCloseHover : t.chromeButtonFace;
    SDL_SetRenderDrawColor(renderer_, face.r, face.g, face.b, 255);
    SDL_RenderFillRect(renderer_, &btn);
    SDL_SetRenderDrawColor(renderer_, t.chromeBorder.r, t.chromeBorder.g, t.chromeBorder.b, 255);
    SDL_RenderDrawRect(renderer_, &btn);
    SDL_SetRenderDrawColor(renderer_, t.chromeButtonGlyph.r, t.chromeButtonGlyph.g, t.chromeButtonGlyph.b, 255);
    SDL_RenderDrawLine(renderer_, btn.x + pad, btn.y + pad,
                       btn.x + btn.w - pad - 1, btn.y + btn.h - pad - 1);
    SDL_RenderDrawLine(renderer_, btn.x + btn.w - pad - 1, btn.y + pad,
                       btn.x + pad, btn.y + btn.h - pad - 1);
}

// docs/39: maximize/restore button — the same 20x20 grey box as the close X
// (DrawCloseOverlay), sitting kChromeMaximizeGap px to its left. Hit zone is
// JKWindowServer::TryChromeGrab zone 1b, mirrored surface-px like the close
// zone so both shrink proportionally on fit-scaled layers.
void JKCompositor::DrawMaximizeButton(const JKCompositorLayer& layer, float scale) {
    if (!renderer_) {
        return;
    }
    const SDL_Rect btn{
        static_cast<int>((layer.X() +
            (layer.Width() - kChromeCloseSize - kChromeCloseMargin -
             kChromeMaximizeGap - kChromeMaximizeSize) * layer.ScaleX()) * scale),
        static_cast<int>((layer.Y() + kChromeCloseMargin * layer.ScaleY()) * scale),
        static_cast<int>(kChromeMaximizeSize * layer.ScaleX() * scale),
        static_cast<int>(kChromeMaximizeSize * layer.ScaleY() * scale)
    };
    const auto& t = jk::theme::current();

    SDL_SetRenderDrawColor(renderer_, t.chromeButtonFace.r, t.chromeButtonFace.g, t.chromeButtonFace.b, 255);
    SDL_RenderFillRect(renderer_, &btn);
    SDL_SetRenderDrawColor(renderer_, t.chromeBorder.r, t.chromeBorder.g, t.chromeBorder.b, 255);
    SDL_RenderDrawRect(renderer_, &btn);
    SDL_SetRenderDrawColor(renderer_, t.chromeButtonGlyph.r, t.chromeButtonGlyph.g, t.chromeButtonGlyph.b, 255);
    if (layer.Maximized()) {
        // Restore glyph: two overlapping white outlines — a big square pad 7
        // with a smaller square pad 3 in front (the window + its shadow).
        const int padOuter = static_cast<int>(7 * layer.ScaleX() * scale);
        SDL_Rect r{btn.x + padOuter, btn.y + padOuter,
                   btn.w - 2 * padOuter, btn.h - 2 * padOuter};
        SDL_RenderDrawRect(renderer_, &r);
        const int padInner = static_cast<int>(3 * layer.ScaleX() * scale);
        r.x = btn.x + padInner;
        r.y = btn.y + padInner;
        r.w = btn.w - 2 * padInner;
        r.h = btn.h - 2 * padInner;
        SDL_RenderDrawRect(renderer_, &r);
    } else {
        // Maximize glyph: one white outline with a 5px pad (like the X pad).
        const int pad = static_cast<int>(5 * layer.ScaleX() * scale);
        SDL_Rect r{btn.x + pad, btn.y + pad, btn.w - 2 * pad, btn.h - 2 * pad};
        SDL_RenderDrawRect(renderer_, &r);
    }
}

JKCompositorLayer* JKCompositor::HitTest(int x, int y) {
    // (x, y) are physical client pixels. Layer positions/sizes are stored in
    // SDL logical points, so multiply by the output scale before comparing.
    const float s = output_.Scale();
    std::lock_guard<std::mutex> lock(layersMutex_);
    for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
        JKCompositorLayer* layer = it->get();
        if (!layer || !layer->IsVisible()) {
            continue;
        }
        const int lx = static_cast<int>(layer->X() * s);
        const int ly = static_cast<int>(layer->Y() * s);
        const int w = static_cast<int>(layer->Width() * layer->ScaleX() * s);
        const int h = static_cast<int>(layer->Height() * layer->ScaleY() * s);
        if (x >= lx && x < lx + w &&
            y >= ly && y < ly + h) {
            return layer;
        }
    }
    return nullptr;
}

} // namespace server
} // namespace jk
