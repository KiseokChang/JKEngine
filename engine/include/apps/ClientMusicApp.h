#ifndef CLIENTMUSICAPP_H
#define CLIENTMUSICAPP_H

// Music hub client (spec 2026-10-09-music-library-design, T2) — the gallery
// twin (ClientGalleryApp): dark root window, 16 ms timer as a delivery
// channel only (#89 — a tick is not activity and never dirties), ImGui over
// the surface, Korean font via the desktop resolver (docs/63 §4.1),
// AppContentTopOffset band math, theme hot-swap re-apply. T2 scope (this
// line): dir strip over the resolved source dirs (state/music default +
// settings.json music.dirs) + an async dir-scan worker + a track table
// (rel / size / mtime) + the name filter. Playback delegation to vplayer is
// T3 — rows are display-only here and no row reacts to clicks yet.
//
// Async scan contract (brief: 갤러리 썸네일 워커 원문 계약 — the
// vplayer/list arrival-dirty idiom is the actual template): a single parked
// worker thread (std::thread) is created in OnInit and joined in OnClose
// (파괴 join); requests cross a one-slot latest-wins mailbox under a mutex
// (재스캔이 빠르게 겹쳐도 항상 최신 세대만 살아난다 — stale results are
// dropped by generation match at adoption). The worker publishes the result
// + raises std::atomic<bool> scanDone_; the UI thread adopts it in OnIdle
// (library 답신 수령 + 도착 더티 수형) — **도착 = 유일 더티**(#89 idle
// 계약, docs/88): timer ticks and unchanged frames must not dirty, and the
// filter typing dirties for free (it is an input event — the gate renders).
//
// The displayed table data is the ListAudioFiles vector verbatim (재정렬·
// 파생 없음 — 정렬은 mtime desc/rel asc tie, rel 열은 그대로) — selftest
// 2m-f asserts this display isomorphism against the same header functions.
#include <client/JKClientApplication.h>
#include <apps/MusicModel.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace jk {

class ClientMusicApp : public JKClientApplication {
public:
    ClientMusicApp() = default;
    // Out-of-line on purpose: the scan worker's std::thread must be joined
    // (quit signal + cv wake) before the members die — the destructor in the
    // cpp owns that handover (vplayer PlayerCore out-of-line delete 수형).
    ~ClientMusicApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    void OnThemeChanged() override;  // ImGui palette re-apply (docs/52)
    bool PreProcessMessage(const JKEvent& ev) override;
    void OnIdle() override;  // 스캔 결과 수취(도착 더티 — 유일 더티 원 #89)
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;

private:
    void BuildUi(int w, int h);
    // 필터 적용 후 표시될 행 수(상단 카운터 — tracks_를 열외 순회만).
    int VisibleTrackCount() const;
    // 트랙 표(경로/크기/수정 3열) — 표기 데이터는 tracks_ 원문(재정렬 없음),
    // 필터는 순서 보존 열외(2m-f 동형 계약의 UI측). 행 선택은 T3.
    void BuildTrackTable();
    // exe-dir state/settings.json 직독 → MusicDirList(기본 폴더+유저 dirs).
    // settings 부재/파손 = 기본 1건(fail-safe) — 앱이 죽지 않는다. 갤러리
    // ResolveDirs 수형(원문 텍스트를 순수 리졸버에 건넨다 — 셀프테스트 쌍둥이
    // 같은 경로 단정).
    void ResolveDirs();
    // 현재 탭의 루트로 비동기 스캔 재청구(새로고침·탭 전환·부팅 공용). 우편함
    // 1발+세대 증감+cv 기상 — 이전 스캔의 결과가 아직 도착 중이면 그 결과는
    // 도착 시 세대 불일치로 폐기된다(최신 요청만 살아난다).
    void RequestScan();
    // 워커 본문 — 요청 대기(cv)→ListAudioFiles→결과 항적+scanDone_ 1올림.
    // 절대 UI/imgui/SDL에 접촉하지 않는다(순수 파트 소비 계약).
    void ScanWorker();
    static std::string ExeDir();

    bool frameDirty_ = true;   // 부팅 첫 렌더(게이트의 renderedOnce 경로) 보증
    bool imguiReady_ = false;

    std::vector<std::string> dirs_;    // resolved source dirs (default first)
    int dirIndex_ = 0;                 // active tab
    std::vector<music::Track> tracks_; // 표기 데이터 = ListAudioFiles 결과 벡터
                                       //   동형(재정렬·파생 없음 — 2m-f 계약)
    char filterBuf_[128] = {0};        // 이름 필터(MatchFilter — 타이핑 더티는
                                       //   입력 이벤트 자연 귀결, 조작 없음)

    // ---- 스캔 워커 상태 ----
    // 단일 상시 스레드(갤러리 썸네일 원문 계약 승계: 생성 = OnInit 1회,
    // 파괴 join = OnClose). 우편함은 1슬롯 최신식 — 스캔 중 재청구는 세대로
    // 흡수하고 오래된 결과는 수취 시 폐기한다.
    std::thread scan_;
    std::mutex scanM_;                 // 우편함+결과 박스 공유 가드
    std::condition_variable scanCv_;   // 요청 기상+quit 폐기
    bool scanRequestOpen_ = false;     // 미수취 요청 존재(scanM_ — UI·워커 양측)
    unsigned long long scanSeq_ = 0;   // 요청 세대(우편함 유일 진실원)
    std::string scanRoot_;             // 미수취 요청 루트
    std::vector<music::Track> scanOut_;          // 결과 박스(scanM_)
    unsigned long long scanOutGen_ = 0;          // 결과 세대
    std::atomic<bool> scanDone_{false};          // 도착 표식(OnIdle 수취 트리거)
    std::atomic<bool> scanQuit_{false};          // 파괴 경련 — wait 폐기+루프 탈출
    bool scanBusy_ = false;            // 요청이 워커에 있고 결과 미도착(표시 몫)
};

} // namespace jk

#endif // CLIENTMUSICAPP_H