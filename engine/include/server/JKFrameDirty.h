#ifndef JKFRAMEDIRTY_H
#define JKFRAMEDIRTY_H

#include <SDL.h>

#include <cstdint>
#include <vector>

#include <ipc/JKWireProtocol.h>  // ipc::DirtyRect — 와이어 원료(스펙 결정 2)

namespace jk {
namespace server {

// FrameDirtyAccumulator — 더티프레젠트의 순수 계산 계층(T1).
//
// 근거: 스펙 2026-10-08-dirty-present 설계 결정 4("더티 rect 계산은 순수
// 함수화 — SDL 렌더러 없이 단정 가능한 합집합/매핑/역치 로직(selftest 케이스
// 대상)과 SDL API 호출 본체를 분리")·결정 2(더티 rect 출처 = 와이어 원용).
// 결함 대상은 docs/78 §5.7 [compst] 실측 — 폰 서버 합성 1회에서 present
// ~70ms(SW 렌더러 전체 프레임 업로드)가 93%를 차지해 커서 블링크 8x16px
// 변화 1건이 1920x1080 전체를 올린다. 이 계층은 그 업로드를 더 dirty rect에
// 비례시키기 위한 판정판이다: SDL_RenderReadPixels/
// SDL_UpdateWindowSurfaceRects 등의 SDL API 본체는 T2 배선 몫이고, 여기는
// 렌더러 생성 없이 단정 가능한 사건 적립/합집합/역치만 갖는다.
//
// 스레드 전제(T1 계약): 내부 뮤텍스 없음 — 단일 스레드 사용 전제. T2 배선에서
// 서버 합성 경로(Composite를 부르는 스레드)만 이 계산기를 만지는 것을 전제로
// 한다. 사건 수집이 다른 스레드에서 필요해지면 T2 원장에서 사건 큐로 승격한다
// (여기서 락을 봉하지 않는다 — 순수 로직 유지).
//
// 수집 시점 함정: Composite 내부 UpdateLayerTexture가 ClearDirty 후 제시되므로,
// 수집은 "이전 프레임 제시 이후 쌓인 사건" 원용이다(T2 배선과 정합).
class FrameDirtyAccumulator {
public:
    static constexpr double kFullFrameThreshold = 0.40;

    // screenW/H: 물리 화면 크기 — 역치 판정과 ForceFull의 전체 rect 원료.
    // 기본(0x0)은 "미지정" 상태이고 SetScreenSize로 프레임마다 갱신한다
    // (출력 리사이즈 대응 — T2 배선이 Composite 전에 부른다).
    FrameDirtyAccumulator(int screenW = 0, int screenH = 0)
        : screenW_(screenW), screenH_(screenH) {
    }

    void SetScreenSize(int w, int h) {
        screenW_ = w;
        screenH_ = h;
    }
    int ScreenWidth() const { return screenW_; }
    int ScreenHeight() const { return screenH_; }

    // 표면 좌표 rect → 물리 화면 rect 매핑 적립. (sw, sh) = 표면 크기,
    // (scaleX, scaleY) = 레이어 표면 스케일, (lx, ly) = 레이어 화면 원점 즉
    // dst 산식(JKCompositor.cpp dst = X·ScaleX·outputScale)의 X·outputScale /
    // Y·outputScale 부분 — 호출자가 곱해서 넘긴다(스펙 원문 계약).
    // 매핑 좌표는 반올림(lround)이고 레이어 dst와 화면 경계로 각각 클램프된다
    // (오버플레이·화면 경계 클램프). 무효 사건(면적 0 이하, 스케일 0 이하,
    // 비정상 표면 크기)은 무시한다.
    void AddSurfaceRect(uint32_t layerId, int sw, int sh,
                        float scaleX, float scaleY,
                        int lx, int ly, const ipc::DirtyRect& r);

    // 커밋 rect 부재 레이어 = dst rect 전체(이미 화면 좌표로 들어온다).
    void AddDirtyLayerRect(uint32_t layerId, const SDL_Rect& screenDst);

    // 레이어 이동/리사이즈/표시 변경 사건 = 이전∪새 dst rect(이동 궤적).
    void AddLayerMove(uint32_t layerId, const SDL_Rect& oldDst,
                      const SDL_Rect& newDst);

    // 포커스 재정렬/오버레이 훅/강제 — 다음 TakeDirty가 화면 전체 rect를
    // 내놓는다(사건 유무와 무관 — 포커스 Z 순서 변화는 rect 사건 없이도
    // 제시를 이끌어야 한다).
    void ForceFull() { forceFull_ = true; }

    // 누적 rect 합집합 병합(인접/중첩 정리)을 out에 채우고 계산기를 비운다.
    // 사건 없으면 false와 빈 out(제시 스킵 원료 — 변화 0 프레임의 업로드 0).
    // ForceFull 또는 역치 도달이면 화면 전체 rect 단건을 낸다. 단, 화면 크기
    // 미지정(SetScreenSize 미호출) 상태의 ForceFull은 전체 rect 원료가 없기에
    // 누적 rect 원문으로 강등한다(fail-safe — T2 배선이 프레임마다
    // SetScreenSize하므로 실 배선에는 도달하지 않는다).
    bool TakeDirty(std::vector<SDL_Rect>& out);

    // ForceFull 또는 역치 초과(누적 rect 면적/화면 면적 >= 40%) 판정.
    // 화면 크기 미지정(0x0)에선 역치 판정 불성립 — false로 강등한다
    // (ForceFull이 아닌 한 full 전환하지 않는다).
    bool IsFull() const;

    // 누적을 비운다(TakeDirty와 함께 쓰는 조기 철수 — 화면 크기는 유지).
    void Clear() {
        rects_.clear();
        forceFull_ = false;
    }

private:
    // 인접(1px 접촉 포함)/중첩 rect를 반복 병합해 결과 목록을 만든다. 결과는
    // 서로 무중첩이므로 면적 총합 = 합집합 면적(역치 판정 원료).
    void MergeRects(std::vector<SDL_Rect>& out) const;

    // 하나의 화면 rect를 적립한다(음수/0 면적·화면 경계 외 = 클램프/무시).
    void AddScreenRect(const SDL_Rect& screenRect);

    std::vector<SDL_Rect> rects_;
    bool forceFull_ = false;
    int screenW_ = 0;
    int screenH_ = 0;
};

} // namespace server
} // namespace jk

#endif  // JKFRAMEDIRTY_H