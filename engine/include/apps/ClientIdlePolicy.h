#ifndef APPS_CLIENTIDLEPOLICY_H
#define APPS_CLIENTIDLEPOLICY_H

// 클라 idle 정책 산치(#89 T2 — 스펙 docs/superpowers/specs/
// 2026-10-09-client-idle-design.md 설계 2-3, 플랜 Task 2). 근거: 스윕 스파이크
// 원장(.superpowers/sdd/2026-10-09-clt-spin/spike-report.md §1a) — 16 ImGui
// 앱의 "Timer 이벤트마다 frameDirty_=true" 무조건 관용구가 docs/78 활동
// 게이트를 매 16ms 틱마다 통과시켜 폰 클라가 무변화 19fps 풀코어 스핀이
// 됐다. T1(GateWantRender — client/JKActivityGate.h)이 타이머 틱을 배송으로
// 재계약했으므로, 이후 앱의 타이머 콜백은 **이번 틱에 실제로 바뀌는 내용이
// 있을 때만** 더티를 낸다.
//
// 이 헤더는 그 조건 판정 중 산치가 있는 것을 순수 부품(jk::idle)으로 뽑아
// 앱과 selftest 쌍둥이(engine/src/main.cpp RunAppSelfTest의 2i-c +
// tools/posix_selftest/main.cpp case 2i)가 하나의 진실원을 소비하게 한다
// (gallery GalleryModel.h · T1 JKActivityGate.h와 같은 수형). 16앱별 처치
// 표는 태스크 리포트(.superpowers/sdd/2026-10-09-client-idle/
// task-2-report.md)의 16앱 표가 원문이다:
//
//   gallery   썸네일 요청 실패(풀 상한+동프레임 유보) 남은 프레임만 재요청 —
//             도착은 BuildUi 동기라 도착 완료 프레임이 곧 마지막 프레임
//   vplayer   재생·비동기 열기·역재생·조그/휠 스크럽·시크 UI·OSD 애니메이션
//             (극장 모드)만 프레임 클록(Progressing)
//   browser   CEF 펌프는 OnIdle로 프레임에서 분리 — on_paint(페이지 픽셀
//             도착)가 있은 펌프만 프레임
//   taskmgr   500ms 샘플 경계만(SampleDue — CPU%·플롯 스크롤)
//   notify    토스트 페이드 2s(ToastFading)+소멸 경계 1프레임 —
//             풀알파 3s 구간은 정적
//   imguidemo 패널 롤링 웨이브 — 200ms 샘플 경계(5fps — 진행 중 산치)
//   files·notes·settings·chat·library·agentmgr·filedialog·palette
//             타이머 더티 분기 삭제 — 남는 더티 원 = 에이전트 응답 수령
//             (OnIdle 폴백 + 수령 시 더티)·입력·테마(게이트 활동)
//   shot·snap·imguidemo  snap은 응답 대기 중(캡처 결과/3s 사망선)만
//             자기유지 더티 — shot은 자기유지 더티 분기 삭제(완전 정적)
//
// 공통 계약: 타이머 틱만으로는 렌더가 일어나지 않는다(2i-a 원문 — 타이머
// 채널은 배송일 뿐). 무입력 idle 앱 = 무렌더(정적)가 의도다.
namespace jk {
namespace idle {

// notify — 토스트 도착 표시는 도착 프레임(풀알파), 말미 페이드 2s만
// 진행 중 애니메이션이다. remainMs는 토스트 만료(5000ms)까지의 잔여.
// 풀알파 구간(>= 2000ms)은 정적 — 무더티. remainMs == 0 은 이미 만료
// (경계 프레임은 앱이 래치로 소각 — 헤더 밖 앱 몫).
inline bool ToastFading(unsigned long long remainMs) {
    return remainMs > 0 && remainMs < 2000;
}

// taskmgr — 통계 샘플 경계(500ms)만 내용이 바뀐다(CPU%·메모리·플롯 스크롤).
// 16ms 무변화 틱은 더티가 아니며, 내용 변화는 최저 2fps 저빈도(초시계 규약 —
// 스펙 설계 2 "초시계 1Hz"의 500ms 대응).
inline bool SampleDue(unsigned long long elapsedMs) {
    return elapsedMs >= 500;
}

// vplayer — 프레임 클록이 필요한 "진행 중" 상태 합산(스펙 설계 3 — 다음
// 프레임을 계속 필요로 하는 상태만 자기유지/틱 더티). 정지(일시정지·미개
// ·끝까지 재생 완료)는 정적 화면 — 무렌더가 의도다(재개는 입력/응답
// 활동이 렌더를 회복한다: 스페이스 토글·Replay·get_status 도구 등).
//   opening    비동기 열기 진행 중(스피너+완료 전환 표시)
//   playing    재생 중(동영상 — 프레임마다)
//   scrubbing  조그 드래그·휠 스크럽·시크 슬라이더·역재생 펌프
//   osdAnimating  극장 모드 OSD 페이드/카운트다운 진행 중
struct VplayerProgress {
    bool opening = false;
    bool playing = false;
    bool scrubbing = false;
    bool osdAnimating = false;
};

inline bool Progressing(const VplayerProgress& p) {
    return p.opening || p.playing || p.scrubbing || p.osdAnimating;
}

}  // namespace idle
}  // namespace jk

#endif  // APPS_CLIENTIDLEPOLICY_H