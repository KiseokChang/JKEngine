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
// (폴백 원존 — no new server part). T2 (spatial leg line, spec
// 2026-10-10-music-spatial-leg-design) adds the second playback channel: the
// row [spatial] button fires audio_core (StreamPlayer — the brief's
// BufferQueueStreamPlayer, actual class name in stream_player.h — + OpenAL Soft
// — linked only when the SPATIAL_PLAYER_ROOT env var was set at configure
// time, T1 fail-closed wiring) while the double-click delegation stays
// untouched (D3 공존 계약 — two channels coexist).
//
// Idle contract (#89 docs/88) extension, spatial leg: the StreamPlayer pump()
// is polled in OnIdle on every loop iteration (응답 펌프의 렌더 분리 수형 —
// polling renders nothing by itself). The only extra dirty sources are the
// displayed progress integer second crossing (진행 표기의 표기 변화 — the
// displayed clock text advancing) and the terminal transitions (start/stop/
// Eos/device-fail state lines). A stopped/failed leg dirties nothing.
//
// Honest-contract design (정직 계약): the [spatial] button is a T1-pure
// notation-layer part, so it renders in every build; when the leg is absent
// (env-unset or ALC device failure) a click lands on
// music::leg::Apply(DeviceFailed) — the state row then draws
// music::leg::kDelegationHint verbatim ("spatial leg 불가 — vplayer 위임
// 이용"). env-unset and device-fail end at the same UI consequence (D2/D5
// fail-closed — the notation never claims a leg that is not there).
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
#include <apps/MusicSpatialLeg.h>  // leg 순수 부품 소비(LegSupport/Apply/ToolJson
                                   //   — T1, audio_core include 0계약이라 이
                                   //   헤더도 env 미설정 축에서 컴파일된다)

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// audio_core(spatial-player 소비 — T1 CMake env 분기)의 완전형은 T2 cpp의
// JK_MUSIC_SPATIAL_LEG 가드 include가 소유한다. 이 헤더는 불완전 전방선언만
// 가진다(unique_ptr 멤버 — 완전형 가시 필요인 파괴는 원밖 소멸자 소유 수형).
// **spatial-player 헤더 경로는 커밋 어디에도 기입하지 않는다**(env-only 규약).
class SpatialEngine;
class StreamPlayer;

namespace jk {

class ClientMusicApp : public JKClientApplication {
public:
    // Out-of-line on purpose (both directions): the scan worker's std::thread
    // must be joined (quit signal + cv wake) before the members die, and the
    // spatial leg unique_ptr members (T2 — incomplete SpatialEngine/
    // StreamPlayer here) need complete types at the dtor/ctor point — both
    // live in the cpp where the macro-guarded audio_core includes are visible
    // (vplayer PlayerCore out-of-line delete 수형).
    ClientMusicApp();
    ~ClientMusicApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    void OnThemeChanged() override;  // ImGui palette re-apply (docs/52)
    bool PreProcessMessage(const JKEvent& ev) override;
    void OnIdle() override;  // 스캔 결과 수취(도착 더티)+spatial pump 폴링(무더티)
    // 앱 도구 허브 소비 쪽(스펙 2026-09-19-app-tool-hub §8.2 — vplayer
    // OnAgentToolCall 원문 수형): spatial_play{path}/stop/status 3종. 등록은
    // OnInit(SendAgentToolRegister — 연결 후 호출 계약 vplayer 배치 근거 원문).
    bool OnAgentToolCall(const std::string& tool, const std::string& argsJson,
                         std::string& resultJson) override;
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
    // 흡수한다. 소진 크레딧(openRetries_)은 **답신 수취(PollReplies)에서만
    // 감법**하고(발사인 OnIdle pace는 감법을 만들지 않는다), 0 도달 = 수취
    // 시점 즉시 소진 전이(표기 1행+더티 1회 — fix r2 I-1: 감법이 OnIdle에
    // 의존하면 마지막 답신 뒤 영구 실패·표기 dead code가 된다). 답신의 대분법
    // 은 봉투 ok를 무신하는 **본문 재판정**(fix r3 — music::DelegationReplyVerdict
    // 소비): 서버 즉답 경로는 봉투 ok=1 고정(본문 거부와 무관 — T4 실측 결함
    // "즉답 거부를 성공으로 읽어 폴백 크레딧 즉시 소멸"의 진원)이라 본문이
    // 진실원 — 본문 ok=false+error(앱-수준 실패)는 unknown_app_tool만 재청구
    // 대상(fix r1 I-2 계약의 본문계층 승격), 그 밖 즉시 종착+err 부기.
    // 재선택(이미 vplayer 창 존재 → 새 창 없이 open 1콜)은 서버가 launch_app을
    // 무제한 별도 스폰으로 두고(존재 검증 원문 주석 — toggle 계열은 palette
    // 전용 별도 툴) 클라엔 창 목록 수형 채널이 없어 v1 원문 쌍 쿼리 계약
    // 우선(report 부기). 그 경로에서 2 인스턴스 등록이면 릴레이가
    // ambiguous+후보 목록(자기교정)을 준다 — 클라는 추측 없이 status 표기로만
    // 흡수(스펙 §4.2 봉인 승계). launch 미성립 시의 사후 open 답신은 겹침
    // 가드로 무음 회수(fix r1 M-2 — 거부 안내 status 납치 방지).
    void PlaybackDelegate(const music::Track& t);
    // AgentQueryReply 폴백 소비(라이브러리 PollReplies 원문 쌍둥이 — 남의
    // 답신은 흘려보낸다): launch 답신 → 거부 시 폴백 종료, open 답신 → 성공/
    // 재청구(unknown_app_tool)/실패 판정(재청구 자체는 OnIdle pacing이 소유).
    // 표기 변화 수령 프레임에만 더티(#89 — 응답 수령 = 이번 틱 내용 변화).
    void PollReplies();
    uint32_t SendQuery(const char* tool, const std::string& args);

    // ---- spatial 재생 leg (T2 — 스펙 2026-10-10 §2, T1 MusicSpatialLeg.h 소비) ----
    // 행 [spatial] 버튼과 spatial_play 도구의 단일 발사 경로. M-3 가드(경로
    // 공문자 → 발사하지 않는다)가 선행하고, 아래로: leg 부재(env 미설정)는
    // DeviceFailed 전이+표기(honest contract — D2/D5 파), leg 있으면 lazy
    // SpatialEngine init(ALC 실패 → DeviceFailed)→Decoder 열기(4행 판별
    // 판별식 — I-1 원장: .vorbis 리터럴은 표는 Supported지만 decoder는 거부;
    // 열기 실패는 StopRequested 전이+원문 부기)→Start 전이+위치 반영+play.
    // 반환 = 발사 성립(Start 전이까지 갔다) — 도구 경계(spatial_play)의 ok
    // 판정 원문. 실패 시 status_에 고장 원문이 남는다.
    bool SpatialStart(const music::Track& t);
    // leg 정지(성공 종착 — player stop+StopRequested 전이). 재생 중에만 의미
    // (idle leg의 정지는 무동작 — T1 StopRequested의 도구측 idempotent 수형).
    void SpatialStop();
    // StreamPlayer pump() 폴링(OnIdle 매 이터레이션 — vplayer 응답 펌프의 렌더
    // 분리 수형: 폴링 자체는 무더티). 재생 중(active — 유일 근거)일 때만
    // 접촉하며, 진행 표기의 정수초가 넘어갈 때만 더티(#89: 진행 표기 갱신만
    // 더티 — 1초당 1프레임, vplayer의 프레임마다 더티[영상 예외 조항]와의
    // 관용구 차이는 오디오 leg의 표기 분해가 정수초이기 때문). Eos(자연 종료)
    // 판정도 여기가 진실원(is_playing false — pause는 이 leg가 부여하지 않는다).
    void SpatialPump();
    // 위치 슬라이더 3조의 변경 1발사(legAzimuth_/Distance_/Elevation_ →
    // set_position 1회). 드래그=입력 이벤트라 더티는 게이트 원문(조작 없음).
    void SpatialPositionChanged();
    // 하단 spatial leg 패널(상태 행+진행+정지·모드+슬라이더 3조+D4 표기 1행)
    // — 표기 데이터는 T1 LegState 원문+UI 상태뿐, leg 부재 빌드도 같은 패널이
    // 안내로 응답한다(정직 계약 — 위 상단 원문).
    void BuildSpatialPanel();

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

    // ---- spatial 재생 leg 상태 (T2) ----
    // leg의 유일 진실원 = T1 LegState(전이는 Apply 원문 — 이 앱은 사건의
    // 사실원만 관측해 LegEvent로 번역한다). 슬라이더 3조는 UI 상태(수형 float
    // — set_position 인자 원문), legState_에 대응 멤버 없음(전이 계약 외
    // 필드 — T1 헤더가 소유하지 않는다). 재생 중 활성(리 슬라이더 게이트).
    music::leg::LegState legState_;
    float legAzimuth_ = 0.0f;      // 방위각 -180..180(양=오른쪽 — spatial.h 좌표계)
    float legDistance_ = 1.0f;     // 거리 0.5..20 m
    float legElevation_ = 0.0f;    // 고도 -45..45
    long long legShownSec_ = -1;   // 마지막 표기한 진행 정수초(표기 변화만 더티
                                   //   — SpatialPump의 유일 더티 근거)
#ifdef JK_MUSIC_SPATIAL_LEG
    // 완전형은 cpp의 매크로 가드 include — unique_ptr 소멸은 원밖 소멸자
    // (cpp, 완전형 가시점)가 처리한다. 선언 순서 파괴 계약: 엔진이 플레이어를
    // 선행 생존(StreamPlayer가 SpatialEngine&를 유지 — stream_player.h 원문).
    std::unique_ptr<SpatialEngine> legEngine_;
    std::unique_ptr<StreamPlayer> legPlayer_;
#endif
};

} // namespace jk

#endif // CLIENTMUSICAPP_H