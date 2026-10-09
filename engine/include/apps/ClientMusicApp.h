#ifndef CLIENTMUSICAPP_H
#define CLIENTMUSICAPP_H

// Music hub client (spec 2026-10-09-music-library-design, T2) — the gallery
// twin (ClientGalleryApp): dark root window, 16 ms timer as a delivery
// channel only (#89 — a tick is not activity and never dirties), ImGui over
// the surface, Korean font via the desktop resolver (docs/63 §4.1),
// AppContentTopOffset band math, theme hot-swap re-apply. T2 scope (this
// line): dir strip over the resolved source dirs (state/music default +
// settings.json music.dirs) + an async dir-scan worker + a track table
// (rel / size / mtime) + the name filter. T3 adds the playback delegation —
// double-click on a track row sends the spec §4 query pair (launch_app vplayer
// → app_tool open) via the library app's SendQuery/PollReplies idiom, with a
// client-side bounded retry absorbing the cold-boot tool-registration race
// (폴백 원존 — no new server part).
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
#include <chrono>
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

    // ---- 재생 위임 (T3 — 스펙 §4 D1, 라이브러리 ClientLibraryApp 원문 쌍둥이) ----
    // 더블클릭 핸들: 쿼리 쌍 ①launch_app({"app":"vplayer"} — 리터럴, 라이브러리
    // LaunchSelected 원문 재용) → ②이어서 같은 핸들 틱에 app_tool open(
    // jk::music::OpenRequestJson 원문 꾸러미 — windowId 미기술; 같은 파이프
    // 작성 순서 = 서버 처리 순서 ①→②). 서버의 app_tool 릴레이는 등록된
    // (app,tool) 역매칭만 지원(vplayer는 OnInit에서 SendAgentToolRegister —
    // 자기 연결 수명 동안)이라 **콜드 부팅 경기**가 실측 성립한다: launch_app의
    // SpawnClient는 비동기라 프로세스 기동+연결+도구 등록이 open 릴레이보다
    // 뒤져 unknown_app_tool로 떨어진다. 폴백 원존 원칙(스펙 §4 — 신규 서버
    // 부품 금지·기존 채널만): open 답신의 unknown_app_tool을 **클라 재청구**
    // (kOpenRetryMax × kOpenRetryMs 시간 pacing — OnIdle, 타이머 더티 아님)로
    // 흡수한다 — 20회 소진 시 소진 표기 1행+더티 1회(fix r1 I-1 — 무음 침묵
    // 방지). ok=false의 error 객체(앱-수준 도구 실패)는 재청구 대상 아님 —
    // 즉시 종착(서버 HandleToolResult 조립 정합 — fix r1 I-2). 재선택(이미
    // vplayer 창 존재 → 새 창 없이 open 1콜)은 서버가 launch_app을 무제한
    // 별도 스폰으로 두고(존재 검증 원문 주석 — toggle 계열은 palette 전용
    // 별도 툴) 클라엔 창 목록 수형 채널이 없어 v1 원문 쌍 쿼리 계약 우선
    // (report 부기). 그 경로에서 2 인스턴스 등록이면 릴레이가 ambiguous+후보
    // 목록(자기교정)을 준다 — 클라는 추측 없이 status 표기로만 흡수(스펙
    // §4.2 봉인 승계). launch 미성립 시의 사후 open 답신은 겹침 가드로 무음
    // 회수(fix r1 M-2 — 거부 안내 status 납치 방지).
    void PlaybackDelegate(const music::Track& t);
    // AgentQueryReply 폴백 소비(라이브러리 PollReplies 원문 쌍둥이 — 남의
    // 답신은 흘려보낸다): launch 답신 → 거부 시 폴백 종료, open 답신 → 성공/
    // 재청구(unknown_app_tool)/실패 판정(재청구 자체는 OnIdle pacing이 소유).
    // 표기 변화 수령 프레임에만 더티(#89 — 응답 수령 = 이번 틱 내용 변화).
    void PollReplies();
    uint32_t SendQuery(const char* tool, const std::string& args);

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

    // ---- 재생 위임 상태 (T3 — 라이브러리 답신 폴링 원문 쌍둥이) ----
    std::string status_;               // 위임 결과 표기(빈값 = 표기 없음)
    uint32_t nextQueryId_ = 1;
    std::vector<uint32_t> pending_;    // AgentQueryReply 라운트트립 체(원문)
    uint32_t launchId_ = 0;            // ① launch_app 답신 식별 → open 1발 트리거
    uint32_t openId_ = 0;              // ② app_tool open 답신 식별
    std::string openPath_;             // 재청구에 다시 전달할 경로(Track.full 사본
                                       //   — 핸들 시점의 행 참조 죽음 무관)
    int openRetries_ = 0;              // open 재청구 잔여(콜드 부팅 경기 폴백)
    bool launchAborted_ = false;       // launch 미성립 표식 — 사후 open 답신 겹침
                                       //   가드(M-2): 무음 회수, status 납치 금지
    std::chrono::steady_clock::time_point openRetryAt_{};  // 재청구 가능 시각
};

} // namespace jk

#endif // CLIENTMUSICAPP_H