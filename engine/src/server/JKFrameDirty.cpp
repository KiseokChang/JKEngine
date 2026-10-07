// JKFrameDirty.cpp — FrameDirtyAccumulator(더티프레젠트 순수 계산 계층 T1).
// 스펙 2026-10-08-dirty-present 설계 결정 4: 합집합/매핑/역치 로직을
// 렌더러 없이 단정 가능한 순수 로직으로 분리(selftest 1p 계열 대상). SDL 접촉
//은 SDL_Rect 타입뿐이다 — SDL API 본체(부분 업로드)는 T2 배선 몫.
#include <server/JKFrameDirty.h>

#include <algorithm>  // std::min, std::max
#include <cmath>  // std::lround
#include <utility>  // std::swap

namespace jk {
namespace server {

void FrameDirtyAccumulator::MergeRects(
    std::vector<SDL_Rect>& out) const {
    out = rects_;
    if (out.size() < 2) {
        return;
    }
    // 반복 쌍 병합: 사건 개수가 프레임당 수십 개 선이라 O(n^2) 재검사로 충분.
    // 접촉 판정은 closed-interval 반쪽 비교 — rect {x,y,w,h}는 [x, x+w) 원문
    // 반이고 서로의 경계에 딱 붙는(1px 접촉) rect도 하나로 뭉친다(계약:
    // 인접/중첩 정리). 병합 결과는 무중첩이므로 면적 총합 = 합집합 면적.
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < out.size(); ++i) {
            for (size_t j = i + 1; j < out.size(); ++j) {
                const SDL_Rect& a = out[i];
                const SDL_Rect& b = out[j];
                const bool touch =
                    a.x <= b.x + b.w && b.x <= a.x + a.w &&
                    a.y <= b.y + b.h && b.y <= a.y + a.h;
                if (!touch) {
                    continue;
                }
                const int x = a.x < b.x ? a.x : b.x;
                const int y = a.y < b.y ? a.y : b.y;
                const int right = (a.x + a.w > b.x + b.w) ? a.x + a.w
                                                          : b.x + b.w;
                const int bottom =
                    (a.y + a.h > b.y + b.h) ? a.y + a.h : b.y + b.h;
                out[i] = SDL_Rect{x, y, right - x, bottom - y};
                out[j] = out.back();
                out.pop_back();
                --j;  // 같은 j에서 다음 후보를 다시 잡는다
                changed = true;
            }
        }
    }
}

void FrameDirtyAccumulator::AddScreenRect(const SDL_Rect& screenRect) {
    if (screenRect.w <= 0 || screenRect.h <= 0) {
        return;
    }
    SDL_Rect r = screenRect;
    if (screenW_ > 0 && screenH_ > 0) {
        const int right = std::min(r.x + r.w, screenW_);
        const int bottom = std::min(r.y + r.h, screenH_);
        r.x = std::max(r.x, 0);
        r.y = std::max(r.y, 0);
        r.w = right - r.x;
        r.h = bottom - r.y;
        if (r.w <= 0 || r.h <= 0) {
            return;  // 화면 밖 사건 — 적립할 게 없다
        }
    }
    rects_.push_back(r);
}

void FrameDirtyAccumulator::AddSurfaceRect(uint32_t layerId, int sw, int sh,
                                           float scaleX, float scaleY,
                                           int lx, int ly,
                                           const ipc::DirtyRect& r) {
    (void)layerId;  // 원문 계약의 자리 표지 — 사건 귀속 레이어(T2 원장)
    if (sw <= 0 || sh <= 0 || scaleX <= 0.0f || scaleY <= 0.0f) {
        return;
    }
    if (r.w <= 0 || r.h <= 0) {
        return;
    }
    // 레이어 dst 산식 원문(JKCompositor.cpp:294-299):
    //   dst.w = Width * ScaleX * outputScale — (lx, ly)는 그 산식의
    //   X·outputScale / Y·outputScale 부분(호출자 원용).
    const SDL_Rect layerDst{
        lx,
        ly,
        static_cast<int>(std::lround(sw * scaleX)),
        static_cast<int>(std::lround(sh * scaleY))};
    if (layerDst.w <= 0 || layerDst.h <= 0) {
        return;
    }
    // 반올림 좌표 매핑(스펙: 스케일 매핑 = 반올림 좌표).
    const int mx = static_cast<int>(
        std::lround(static_cast<double>(lx) + r.x * scaleX));
    const int my = static_cast<int>(
        std::lround(static_cast<double>(ly) + r.y * scaleY));
    const int mw = static_cast<int>(std::lround(r.w * scaleX));
    const int mh = static_cast<int>(std::lround(r.h * scaleY));
    if (mw <= 0 || mh <= 0) {
        return;
    }
    // 레이어 dst 절단(오버플레이 클램프) — 표면 좌표로 넘친 dirty가 레이어
    // 바깥 화면 면적을 부채질하지 않는다.
    const int left = std::max(mx, layerDst.x);
    const int top = std::max(my, layerDst.y);
    const int right = std::min(mx + mw, layerDst.x + layerDst.w);
    const int bottom = std::min(my + mh, layerDst.y + layerDst.h);
    if (right - left <= 0 || bottom - top <= 0) {
        return;
    }
    AddScreenRect(SDL_Rect{left, top, right - left, bottom - top});
}

void FrameDirtyAccumulator::AddDirtyLayerRect(uint32_t layerId,
                                              const SDL_Rect& screenDst) {
    (void)layerId;  // 원문 계약의 자리 표지 — 사건 귀속 레이어(T2 원장)
    AddScreenRect(screenDst);
}

void FrameDirtyAccumulator::AddLayerMove(uint32_t layerId,
                                         const SDL_Rect& oldDst,
                                         const SDL_Rect& newDst) {
    AddDirtyLayerRect(layerId, oldDst);
    AddDirtyLayerRect(layerId, newDst);
}

bool FrameDirtyAccumulator::IsFull() const {
    if (forceFull_) {
        return true;
    }
    if (screenW_ <= 0 || screenH_ <= 0) {
        return false;  // 화면 크기 미지정 — 역치 판정 불성립(fail-safe 강등)
    }
    std::vector<SDL_Rect> merged;
    MergeRects(merged);
    // 무중첩 병합 목록의 면적 총합 = 합집합 면적(중첩 중복 가산 없음).
    long long area = 0;
    for (const SDL_Rect& r : merged) {
        area += static_cast<long long>(r.w) * static_cast<long long>(r.h);
    }
    const long long screenArea =
        static_cast<long long>(screenW_) * static_cast<long long>(screenH_);
    return screenArea > 0 &&
           static_cast<double>(area) >=
               kFullFrameThreshold * static_cast<double>(screenArea);
}

bool FrameDirtyAccumulator::TakeDirty(std::vector<SDL_Rect>& out) {
    out.clear();
    std::vector<SDL_Rect> merged;
    MergeRects(merged);
    // 역치 판정(ForceFull 배제판 — forceFull_는 아래에서 직접 본다).
    const bool thresholdFull = !forceFull_ && IsFull();
    if (forceFull_ || thresholdFull) {
        // full 전환: 화면 원료가 있으면 전체 rect 단건(사건 유무 무관 —
        // 포커스 재정렬 같은 사건 없는 강제도 제시를 이끈다). 화면 원료가
        // 없으면(미지정 화면) 누적 rect 원문으로 강등해 일단 제시는 한다
        // (fail-safe — 실 배선은 프레임마다 SetScreenSize하므로 미도달).
        if (screenW_ > 0 && screenH_ > 0) {
            merged.clear();
            merged.push_back(SDL_Rect{0, 0, screenW_, screenH_});
        }
    }
    rects_.clear();
    forceFull_ = false;
    std::swap(out, merged);
    return !out.empty();
}

} // namespace server
} // namespace jk