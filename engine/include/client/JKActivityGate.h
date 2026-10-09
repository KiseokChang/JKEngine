#ifndef JKACTIVITYGATE_H
#define JKACTIVITYGATE_H

// 클라 렌더 활동 게이트 — 순수 판정 부품 (#89 T1, 스펙
// docs/superpowers/specs/2026-10-09-client-idle-design.md).
//
// 근거: 스파이크 원장(.superpowers/sdd/2026-10-09-clt-spin/spike-report.md
// §1a) — Run() 루프(engine/src/client/JKClientApplication.cpp)가 타이머 채널
// 소비 수>0을 활동으로 계수해 docs/78 활동 게이트를 매 16ms 틱마다 무력화,
// 폰 갤러리 클라가 무변화 19fps 풀코어(100%) 스핀이 됐다. 이 헤더는 그 판정
// 식을 SDL·서버·렌더러 접촉 없이 단정 가능한 수형으로 뽑아 Run() 루프와
// selftest 쌍둥이가 하나로 소비하게 한다(단일 진실원 — selftest 2i 계열,
// 캐논 계보 기존 Win 569/WSL 546/posix 277 다음 신설 +10 케이스).
//
// 계약(#89 T1 핵심): **타이머 틱은 배송일 뿐 활동이 아니다.** 타이머 채널은
// 이 판정에 참여하지 않는다. 활동 = 입력·에이전트 이벤트·에이전트 툴콜·테마
// 변경. docs/78의 게이트 원 의도(이벤트 부재 시 렌더 스킵)의 이행이다.
//
// 폴백 1s 안전망은 원문 그대로 유지: 판정이 false여도 마지막 렌더에서 1s가
// 지났고 장면에 더티가 남아 있으면 렌더한다(이벤트로 승계되지 않는 느린
// 변화 — 비동기 Invalidate 등 — 의 최악 지연 상한). 폴백 도달시에만 더티
// 조회가 일어난다. 마지막 렌더 이후 첫 이터레이션은 이벤트 없이도 즉시
// 1프레임(renderedOnce = false — 부팅 즉시 1프레임 원문).
namespace jk {
namespace client {

// wantRender 판정 (Run() 루프 원문 구조 보존):
//   wantRender = frameDirty || activity(타이머 제외 합산) || 첫 렌더 전 ||
//                (폴백 1s 도달 && 장면 더티 잔존)
// dirtyWindow는 폴백 도달시에만 조회(HasDirtyWindows는 그룹 탐색이라 매
// 이터레이션 열람을 회피 — Run() 원문 구조). 게이트가 false로 결정하는 판정
// 에서 프레디케이트를 부르지 않는다(2i-a probeCount 단정 대상).
template <typename DirtyProbe>
inline bool GateWantRender(bool timerDelivered, bool inputDrained,
                           bool agentEvent, bool toolCall, bool themeChanged,
                           bool frameDirty, bool renderedOnce, bool fallback,
                           DirtyProbe&& dirtyWindow) {
    (void)timerDelivered;  // #89 T1 — 타이머 틱은 배송 기록일 뿐, 활동이 아니다
    // #89 T1 fix r2 — 인자 연결 계약(호출부 결함 원장 — JKClientApplication
    // Run()): 7번째 매개변수는 "이미 한 프레임 그렸나"의 **원본 bool**이다.
    // 본문에서 `!renderedOnce`(첫 번째 렌더 대기)로 단수 부정하므로, 호출부가
    // `!renderedOnce`를 넘기면 이중 부정이 되어 first-render 항이 뒤집힌다 —
    // 첫 렌더 1프레임은 안 나고(항목 반전) 프레임 뒤에는 wantRender 항시 참
    // (idle 풀코어 스핀). 호출부 계약: 원본 bool만 넘긴다.
    const bool activity =
        inputDrained || agentEvent || toolCall || themeChanged;
    if (frameDirty || activity || !renderedOnce) {
        return true;
    }
    return fallback && dirtyWindow();
}

}  // namespace client
}  // namespace jk

#endif  // JKACTIVITYGATE_H