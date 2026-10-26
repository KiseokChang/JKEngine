// 채팅 앱 클라 (스펙 2026-10-07-desktop-chat-app — library 허브 원준:
// ClientLibraryApp 패턴의 요청-응답 폴링, 이벤트 구독 없음).
//
// 백엔드 슬롯(스펙 §4 — cfg 선택형 JKLlmEngine; 승격 판정=docs/80 §3, stub
// 유지): 지금은 T1 stub 라우터(jk::ChatRouterRoute)만 이 자리에 꽂는다(룰링 —
// posix엔 claude/ollama CLI가 없어 지금 실장하면 폰에서 dead code). cfg가
// 백엔드를 고르는 형태로 이 호출 자리를 갈아끼우면 된다 — 앱 나머지(기록·
// 입력·송신)는 백엔드 무관. 배선 실장=백로그(docs/80 §3 명단).
#include <apps/ClientChatApp.h>

#include <agent/JKLlmEngine.h>  // LoadChatConfig — 구두 턴 접점 인자(#112 T4)
#include <apps/ChatRouter.h>
#include <fs/JKFs.h>        // TempDir — 캡처 임시 WAV 경로 진실원(표면 소유)
#include <JKTextAtlas.h>   // ResolveDesktopFontPath (docs/63 폰트 계약)
#include <voice/SttVoice.h> // 캡처+턴 계약(#112 T3 소비 — 스펙 §3 ④·R2·R5)
#include <imgui_impl_jkwindow.h>
#include "theme/JKThemeImGui.h"
#include <SDL.h>

#include <cstdio>
#include <filesystem>
#include <system_error>
#if !defined(_WIN32)
#include <unistd.h>  // ::getpid — ThisProcessId 쌍둥이(MusicDirStore.h:52 계약)
#endif
#if defined(__ANDROID__)  // #117 T1 — D1 폰 축 TTS 스폰 leg 전용(include 포함
                          // 가드 — Win/WSL 컴파일 무영향 원장행)
#include <process/JKProcess.h>  // Spawn 어댑터(VoiceCaptureTermux 원존 수형)

#include <sys/stat.h>  // ::stat — ResolveTermuxBin 수형(Termux prefix 조립)

#include <thread>
#endif

namespace jk {
namespace {
// Root window paints the dark clear color (palette/notify/agentmgr idiom).
class SetRoot : public JKWindow {
public:
    explicit SetRoot(const std::string& title) : JKWindow(title) {}
    void OnPaintClient(JKDC& dc) override {
        const JKRect client = GetClientRect();
        const auto& t = jk::theme::current();
        dc.SetColor(t.appClearBg.r, t.appClearBg.g, t.appClearBg.b, 255);
        dc.FillRect(client);
    }
};

// 임시 wav 경로의 pid 접미 — MusicDirStore.h:64 ThisProcessId 쌍둥이 원문
// 수형(쌍둥이 반복 선례 — kernel32 단일 선언·windows.h 전개 금지 계약).
#if defined(_WIN32)
extern "C" __declspec(dllimport) unsigned long __stdcall
    GetCurrentProcessId(void);
unsigned long ThisProcessId() { return GetCurrentProcessId(); }
#else
unsigned long ThisProcessId() { return static_cast<unsigned long>(::getpid()); }
#endif

std::string EscapeJson(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (const char ch : in) {
        if (ch == '"' || ch == '\\') { out += '\\'; out += ch; }
        else if (static_cast<unsigned char>(ch) < 0x20) {
            char num[8];
            std::snprintf(num, sizeof(num), "\\u%04x", ch);
            out += num;
        } else {
            out += ch;
        }
    }
    return out;
}

// 발화 앞뒤 공백 절단 — 기록에는 정돈된 원문을 남긴다(라우터가 먹는 것은
// ChatRouterRoute 내부 정규화 결과라 이 절단은 표시 몫).
std::string TrimText(const std::string& raw) {
    const size_t b = raw.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    const size_t e = raw.find_last_not_of(" \t\r\n");
    return raw.substr(b, e - b + 1);
}

#if defined(__ANDROID__)
// #117 T1 — 구두 응답 TTS 스폰 leg (D1 폰 축 + D5 실패 무시). 수형 원존 =
// VoiceCaptureTermux.cpp의 스폰 계열($PREFIX/bin resolve·지역 인용·마감+
// kill-on-close job) — 그 파일의 계열 상수/도우미는 파일-local라 이 TU에
// 축소 수형으로 독립한다(쌍둥이 반복 선례 — MusicDirStore ThisProcessId와
// 같은 관행). **이 경로는 Win/WSL 캐논 바이너리에 미포함**(컴파일 무영향)
// — 폰 빌드(T2 배포)만 링크된다 — 원장행.

// 도달 불가 컴파일 경로 원장행: 아래 4종 전부 __ANDROID__ 게이트 안 — Win/
// WSL 빌드(jkapp_chat)에서는 전처리기가 본체를 폐기해 심볼·코드 0. jkdesktop
// selftest(2z)가 단정하는 것은 헤더 인라인 결정 부품 뿐(D6 — 스폰 어셜션
// 금지). 스폰 leg의 실측 = 폰 E2E probe(T2 — "[TTS] speak" 행 원문).

// Termux prefix 폴백 2종($PREFIX 실측 통용·TERMUX_PREFIX 명시 스펠링) —
// VoiceCaptureTermux 수형의 문자 그대로. 미설정 = 빈 — resolve 실패로 귀결.
std::string TtsTermuxPrefix() {
    const char* p = std::getenv("TERMUX_PREFIX");
    if (!p || !*p) p = std::getenv("PREFIX");
    return (p && *p) ? std::string(p) : std::string();
}

// $PREFIX/bin/<name> resolve 수형 — 절대 경로 선호(spawn은 /bin/sh -c PATH
// 조회 — PATH 상실 서버 계통에서도 성립하는 정직 resolve).
bool TtsResolveBin(const char* name, std::string& abs) {
    const std::string prefix = TtsTermuxPrefix();
    if (!prefix.empty()) {
        const std::string cand = prefix + "/bin/" + name;
        struct stat st;
        if (::stat(cand.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            abs = cand;
            return true;
        }
    }
    return false;
}

// posix sh 이중 따옴표 지역 이스케이프 — VoiceCaptureTermux Quoted 수형의
// 문자 그대로(speak 문언에 따옴표·달러·백틱이 실려도 단일 인자로 간다).
std::string TtsQuoted(const std::string& s) {
    std::string out = "\"";
    for (const char ch : s) {
        if (ch == '"' || ch == '\\' || ch == '$' || ch == '`')
            out += '\\';
        out += ch;
    }
    out += '"';
    return out;
}

// speak 백그라운드 상주 — 수형 원장행(#117 T2 리뷰 1순위 — 2026-10-27
// batch2 실환 소각): 구 마감 15s 대기는 hang 스폰 1건 = detached 스레드
// 15s 점유+`[TTS] fail (rc=137)`(마감 kill — 128+9) 증상행 반복 소모 —
// 폰 웨지 계통의 실측 원진. 수형: 스폰 성립 직후 즉시 반환 — speak 진행은
// 프로세스가 백그라운드로 스스로 완주한다(D5 재시도 없음·콜백 없음 승계,
// 조달/스폰 실패 = stamp fail — 같은 표).
//   - 서브셸 백그라운드 래퍼 `( … ) >/dev/null 2>&1 &` — speak는 어댑터
//     직계 자식(sh -c) 내부의 서브셸로 상주하고 sh 본체는 즉시 종료라
//     부모 쪽 좀비·좀비 수거 부재·rc 0 = 고아 서브셸은 init 수거(리소스
//     누수 없음의 진실원 — 직계 파산이 아니라 고아 전이가 좀비를 지운다).
//   - 출력 폐기 = 셸 리디렉션 /dev/null — 부모 파이프 0(파이프 생성 자체
//     불요 — 구 inheritedStdioPipes 소각·SIGPIPE 창 0·drain 스레드 0).
//   - kill-on-close job 폐기: posix close IS kill(jk::process 계약 b)라
//     상주 중 close = 즉사 — 백그라운드 상주 계약과 양립 불가. 마감 kill
//     제거 = hang 스폰의 영구 상주(완주/정산) = 사용자 몫 원장행(부작용
//     정직 표기 — rc 관측 자체가 소멸한다).
// 1회 스폰 본체 — DrainVoice(UI 축)가 detached 단명 스레드로 불러 쏜다.
// speak 문언은 이미 tts::TtsReplyClip(D4)을 통과한 절단판.
void TtsSpawnSpeak(const std::string& speakText) {
    // D5 — 실패 무시 계통: 조달 실패(termux-tts-speak 부재) = 스킵. 이하
    // 같은 표 — UI·기록 무영향·재시도 없음. trace stamp = stderr 1행(폰 E2E
    // probe의 "[TTS] speak/실패 원장행" 소관 — 스펙 §2).
    std::string bin;
    if (!TtsResolveBin("termux-tts-speak", bin)) {
        std::fprintf(stderr, "[TTS] fail (termux-tts-speak 부재)\n");
        return;
    }
    jk::process::SpawnOptions opt;
    opt.commandLineUtf8 =
        "( " + TtsQuoted(bin) + " " + TtsQuoted(speakText) +
        " ) >/dev/null 2>&1 &";
    opt.hideWindow = true;
    // 파이프 자원 0(폐기 = 리디렉션 흡수) — 생성/포기 전양 대신 미조달.
    const jk::process::SpawnResult spawned = jk::process::Spawn(opt);
    if (!spawned.ok) {
        std::fprintf(stderr, "[TTS] fail (스폰 실패 err=%u)\n",
                     spawned.errorCode);
        return;
    }
    std::fprintf(stderr, "[TTS] speak: %s\n", speakText.c_str());
    // 직계 자식 = 단명 sh 래퍼 — 핸들 즉시 폐쇄(posix CloseHandleLike =
    // WNOHANG 수거 1회 — 이미 종료한 sh 좀비 소각·close ≠ kill 계약이라
    // 상주 subshell 무영향). sh 폐쇄 실패(스폰 경합 극소 창) = 좀비 1행 —
    // 직계 자식의 수거만 부모 몫으로 남는다(상주의 수거 = init).
    jk::process::CloseHandleLike(spawned.process);
    // rc 0 = speak 시작 수락 원장행 — 응답 지연(~1s)과 실제 합성 종료는
    // Termux TTS 엔징 몫(EYES — 폰에서 답을 듣는 체감 — 스펙 §2).
}
#endif  // __ANDROID__ (#117 T1 — 폰 축 스폰 leg; 폰 축 외 = 빈 영역)

} // namespace

ClientChatApp::~ClientChatApp() {
    // 구두 워커 join 수형(ClientMusicApp 스캔 원문 계약 — 파괴 join): quit
    // 경련+cv 기상 뒤 join. 진행 중 whisper 전사는 절단 계약 없음(T2 단일
    // 마감 600s — 1발화 1-15s 관측)이라 창 닫힌 뒤 남은 전사만큼 기다린다
    // — 원장행(리포트 §함정).
    StopVoiceWorker();
}

void ClientChatApp::OnInit() {
    auto main = std::make_unique<SetRoot>("Chat");
    main->SetWindowRect(JKRect{ 0, 0, 720, 540 });
    main->SetAttrFlags(WA_CHROMELESS);
    SetMainWindow(std::move(main));

    SetTimerInterval(16);   // 틱 = 배송 채널(#89 T1/T2 — 활동·더티 아님.
                            // 답신 폴링은 OnIdle — 응답 수령 시 더티)

    // 구두 워커 — 단일 상시 스레드(생성 주기 원문 계약 — ClientMusicApp
    // OnInit 수형). 이후의 구두 턴은 FinishVoice의 우편함 1발만 만든다
    // (스레드 재창설 없음).
    voice_ = std::thread([this] { VoiceWorker(); });

    ImGui::CreateContext();
    jk::theme::ApplyImGuiTheme();
    ImGui::GetIO().IniFilename = nullptr;
    ImGuiIO& io = ImGui::GetIO();
    // 한국어 폰트 리졸러 계약(docs/63 §4.1) 승계 — 빈 해석은 커스텀 폰트
    // 스킵(내장 기본 글리프; 하드코딩 malgun은 리눅스 AddFont assert 사망).
    const std::string fontPath = jk::text::ResolveDesktopFontPath();
    if (!fontPath.empty() &&
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f,
                                     nullptr,
                                     io.Fonts->GetGlyphRangesKorean())) {
        koreanFont_ = true;
    }
}

void ClientChatApp::OnClose() {
    // 구두 마이크 즉시 절단(legPlayer reset 수형 — ClientMusicApp OnClose의
    // "창이 닫히는 프레임에 끊는다" 원문): 닫힘 시점에 캡처 중이면 정지 —
    // 착지한 WAV는 이제 소비자가 없다(턴 큐에 실리기 전) — 잔산 소각.
    if (voice::capture::IsCapturing()) {
        const voice::capture::CaptureState st = voice::capture::GetCaptureState();
        std::string capErr;
        if (voice::capture::StopCapture(capErr) && !st.wavPath.empty()) {
            std::error_code rmEc;
            std::filesystem::remove(std::filesystem::path(st.wavPath), rmEc);
        }
        voiceStatus_.clear();
        voiceOriginPending_ = false;   // 턴 0건 — 표지 소각(placeholder 없음)
    }
    // 텍스처 소유 없음 — ImGui만 봉합. 순서 계약: OnClose는 JKClientApplication
    // .cpp:260 기준 DestroyHiddenRenderer(:270)보다 먼저 불린다(ClientShotApp
    // .cpp:74 선례) → hidden renderer가 아직 살아 있어 Shutdown이 안전하다.
    if (imguiReady_) {
        ImGui_ImplJKWindow_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
}

void ClientChatApp::OnThemeChanged() {
    if (imguiReady_) jk::theme::ApplyImGuiTheme();   // docs/52 hot-swap
}

bool ClientChatApp::PreProcessMessage(const JKEvent& ev) {
    ImGui_ImplJKWindow_ProcessJKEvent(ev);
    // #89 T2 — Timer 무조건 더티 관용구 삭제: 타이머 틱은 활동이 아니다
    // (T1 게이트 재계약)이고 채팅은 정적 UI다. 남는 렌더 원 = 입력·테마
    // (게이트 활동)·응답 수령(OnIdle 폴백 — 응답 수령 틱만 더티, PollReplies).
    // 입력 중 커서 블링크 유지·무입력 정지는 수용(스펙 결정 ③).
    //
    // #112 T4 — 구두 핫키 F6 = hold-to-talk(R2+스펙 §3 ①): 누름=StartCapture
    // ·해제=StopCapture→WAV→비동기 전사. 핫키를 **토글**(재누름=마침)로 하지
    // 않은 근거(원장): TranslateSDLEvent(JKEvent.cpp:59)가 SDL repeat 플래그를
    // 승계하지 않아 지속 누름의 자동재키 repeat KeyDown이 토글을 진동시킬 수
    // 있다 — hold/down+up 모양은 repeat를 무해화 한다(캡처 중 repeat KeyDown
    // = no-op). 버튼 쪽은 토글(ImGui::Button — 자동재키 없음).
    if (ev.type == JKEventType::KeyDown && ev.keyCode == SDLK_F6) {
        if (!voice::capture::IsCapturing()) StartVoice();
    } else if (ev.type == JKEventType::KeyUp && ev.keyCode == SDLK_F6) {
        if (voice::capture::IsCapturing()) FinishVoice();
    }
    return true;
}

void ClientChatApp::OnIdle() {
    // #89 T2 — 응답 폴백의 렌더 분리: 수령이 렌더 프레임에만 일어나면
    // 정적 화면(무렌더)에서 답신을 영원히 못 받는다. 수령한 응답이 기록을
    // 바꿀 때 더티(PollReplies의 frameDirty_ — 원문 유지).
    PollReplies();
    // 구두 턴 드레인(응답 펌프의 렌더 분리 수형 — music SpatialPump와 같은
    // 자리): 폴링 자체는 무더티고 결과 도착(DrainVoice → 턴 치환·기록) 틱만
    // 더티 — #89 조건화 계약. 대기 중 상태행 "전사 중…"은 정적 표기다.
    DrainVoice();
}

void ClientChatApp::OnFrameCommitted() { frameDirty_ = false; }

void ClientChatApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
    if (!imguiReady_) {
        if (!ImGui_ImplJKWindow_Init(renderer)) return;
        imguiReady_ = true;
    }

    ImGui_ImplJKWindow_NewFrame(1.0f / 60.0f, w, h);
    ImGui::NewFrame();
    BuildUi(w, h);
    ImGui::Render();
    ImGui_ImplJKWindow_RenderDrawData(ImGui::GetDrawData(), renderer);
}

void ClientChatApp::Submit(const std::string& raw) {
    const std::string text = TrimText(raw);
    if (text.empty()) return;   // 공백만 — 무발화로 기록을 부풀리지 않는다

    turns_.push_back(Turn{ true, text });

    // 뇌호출 — 백엔드 슬롯(파일 상단 주석). 응답문은 도구 지시가 아니라
    // **사용자에게 보여질** 안내/확인문(ChatRouter.h 계약)이라 기록에 그대로.
    jk::ChatAction action;
    std::string resp = jk::ChatRouterRoute(text, action);
    // fix r1 (Minor 2) — 타이핑 표면은 app_tool 질의를 미송신(발송 표면 = 구두
    // leg만 — ChatRouter.h 소비 계약). 라우터의 "보냅니다" 확인문이 미송신
    // 지시를 쓰는 부정직을 공유 정직 문언으로 교체(2y-⑰). 전송 배선은 후속
    // 태스크 부재 — 원장행(아래 스위치의 AppTool 전송 없음 사례와 동형).
    if (action.kind == jk::ChatAction::AppTool) {
        resp = jk::ChatAppToolTypingGuide(action);
    }
    turns_.push_back(Turn{ false, resp });
    frameDirty_ = true;
    scrollBottom_ = true;

    switch (action.kind) {
        case ChatAction::Launch: {
            // launch_app 존재 검증(JKWindowServer.cpp:4420 부근): app은
            // jkapp_<app><접미> 모듈 파일이 exe 옆에 있어야 한다 — 표 밖 앱어는
            // 서버가 unknown_app으로 정직 회신(fail-open 계약).
            const std::string args =
                "{\"app\":\"" + EscapeJson(action.app) + "\"}";
            SendQuery("launch_app", args);
            break;
        }
        case ChatAction::Close:
            // 서버 계약 확장(채팅 F1 — 2026-10-08 착지, JKWindowServer.cpp
            // close_window): 라우터가 해소한 app 키가 이제 보낼 곳이 있다 —
            // 서버가 제목 매칭(대소문자 무시)으로 target을 고른다. 무앱(
            // 1n-7 app="")은 argless 폼 → 서버 포커스 창(폰 "닫아줘" 계약).
            // 부재·무매칭은 서버가 window_not_found로 정직 회신한다.
            SendQuery("close_window", action.app.empty()
                ? "{}"
                : "{\"app\":\"" + EscapeJson(action.app) + "\"}");
            break;
        case ChatAction::Focus:
            // 동형 해소(JKWindowServer.cpp focus_window): app 지명 폴백 +
            // argless = 포커스 창 — close와 같은 계약.
            SendQuery("focus_window", action.app.empty()
                ? "{}"
                : "{\"app\":\"" + EscapeJson(action.app) + "\"}");
            break;
        case ChatAction::ListWindows:
            // 답신 JSON을 기록에 그대로 인쇄가 계약(스펙 §1.1) — PollReplies.
            SendQuery("list_windows", "{}");
            break;
        case ChatAction::AppTool:
            break;   // fix r1 — 전송 없음(타이핑 표면 안내만 — 위 resp 교체 소관)
        case ChatAction::Info:
            break;   // 도구 지시 없음 — 안내문만 이미 기록에 들어갔다
    }
}

// ClientLibraryApp::SendQuery :195 본사 복사 — 래퍼·연결 폴백 유지, 이 앱의
// 답신은 전부 기록 원문 인쇄라 kind/arg 메타 대신 id만 적립한다.
uint32_t ClientChatApp::SendQuery(const char* tool, const std::string& args) {
    jk::client::JKClientSurface* surface = Surface();
    if (!surface || !surface->IsConnected()) {
        status_ = koreanFont_ ? "[!] 서버에 연결되어 있지 않습니다"
                              : "[!] not connected";
        return 0;
    }
    const std::string json =
        "{\"tool\":\"" + std::string(tool) + "\",\"args\":" + args + "}";
    const uint32_t id = nextQueryId_++;
    if (!surface->SendAgentQuery(id, json)) {
        status_ = "[!] send failed";
        return 0;
    }
    pending_.push_back(id);
    return id;
}

void ClientChatApp::PollReplies() {
    jk::client::JKClientSurface* surface = Surface();
    if (!surface) return;
    jk::client::AgentReply reply;
    while (surface->PollAgentReply(reply)) {
        bool mine = false;
        for (auto it = pending_.begin(); it != pending_.end(); ++it) {
            if (*it == reply.queryId) {
                pending_.erase(it);
                mine = true;
                break;
            }
        }
        if (!mine) continue;   // 우리 쿼리가 아니다 — 무시(settings PollReplies
                               // 같은 드레인 계약: 남의 답신을 흘려보낸다)
        // 응답 원문 그대로 기록 — list_windows JSON 인쇄 계약. 실패는 [!] 접두
        // (library 상태 행 규약 동일 — 창 닫기의 window_not_found 등도 정직히
        // 보인다).
        turns_.push_back(Turn{ false,
                               reply.ok ? reply.json : "[!] " + reply.json });
        frameDirty_ = true;
        scrollBottom_ = true;
    }
}

// ---- 구두 표면 (#112 T4 — 스펙 §3 ④·R2·R5) --------------------------------
void ClientChatApp::StartVoice() {
    if (voice::capture::IsCapturing()) return;
    // fail-closed 사전 판정(정직 거부 전 마이크 개방 낭비 소각) — T1 판정부
    // verdict 원문 그대로(2v-a 4행 계약을 표면이 쓰는 얼굴).
    bool binFound = false;
    voice::run::ResolveCliPath(&binFound);
    const jk::stt::VoiceVerdict v = jk::stt::VoiceLegVerdict(
        jk::stt::VoiceWhisperRootSet(), binFound,
        !voice::run::ResolveModelPath().empty());
    if (!v.enabled) {
        status_ = std::string("[!] ") + v.reason;
        PublishNotify(v.reason);   // 인식 실패 단문(토표면 — ClientNotifyApp)
        frameDirty_ = true;
        return;
    }
    // 임시 WAV 경로 = 표면 소유 진실원(T3 계약 — "캡처 leg는 경로를 강요하지
    // 않는다"). TempDir()+pid 접미(MusicDirStore ThisProcessId 쌍둥이 —
    // 프로세스별 접촉 소관)+발화 순서 접미(결정론 — F6 재발화가 덮지 않게).
    std::string capErr;
    const std::string wav = jk::fs::TempDir() + "jkchat_voice_" +
                            std::to_string(ThisProcessId()) + "_" +
                            std::to_string(voiceWavSeq_++) + ".wav";
    if (!voice::capture::StartCapture(wav, capErr)) {
        status_ = "[!] " + capErr;   // SDL 개방 실패·진행 중 등 정직 거부 원문
        frameDirty_ = true;
        return;
    }
    status_.clear();
    // #117 T1 — D3: 이 세션 = 구두 출발 발화(StartVoice가 유일 개입 통로 —
    // F6 hold·[구두] 버튼 2계통 전부 여기서). 표지는 placeholder 턴에 수납(
    // FinishVoice) — 답신 도착(DrainVoice)의 TTS 관문 진실원.
    voiceOriginPending_ = true;
    voiceStatus_ = koreanFont_ ? "녹음 중… (F6 해제 또는 [구두] 버튼으로 마침)"
                               : "recording... (release F6 or press [Voice])";
    frameDirty_ = true;   // 상태 전이 1틱만 더티 — 대기 무더티(#89 계약)
}

void ClientChatApp::FinishVoice() {
    if (!voice::capture::IsCapturing()) return;
    const voice::capture::CaptureState st = voice::capture::GetCaptureState();
    std::string err;
    if (!voice::capture::StopCapture(err)) {
        // 빈 결실(PCM 0)·쓰기 실패 등 — 정직 거부 원문 그대로(턴 0건 —
        // 기록을 부풀리지 않는다).
        status_ = "[!] " + err;
        voiceStatus_.clear();
        frameDirty_ = true;
        return;
    }
    // placeholder 턴(전사 비결정 — 1-15s) → 전사 도착 시 이 슬롯을 치환(R5 —
    // "턴 기록에 전사 원문 도착 시 치환"). turns_는 append-only라 슬롯 안정.
    // D3 — origin 표지 수납 접점(1곳): StartVoice 세션 표지를 placeholder에
    // 새기고 소모(1회). 답신 도착(DrainVoice)이 이 표지를 본다.
    turns_.push_back(Turn{ true, koreanFont_ ? "(전사 중…)" : "(transcribing...)",
                           voiceOriginPending_ });
    voiceOriginPending_ = false;   // 세션 표지 소각 — 다음 발화는 새 표지 몫
    const size_t slot = turns_.size() - 1;
    {
        const std::lock_guard<std::mutex> lock(voiceMtx_);
        voiceJobs_.push_back(VoiceJob{ st.wavPath, slot });
    }
    voiceCv_.notify_one();
    voiceStatus_ = koreanFont_ ? "전사 중…" : "transcribing...";
    frameDirty_ = true;
    scrollBottom_ = true;
}

void ClientChatApp::DrainVoice() {
    std::deque<VoiceDone> done;
    std::deque<std::pair<std::string, std::string>> outbox;
    {
        const std::lock_guard<std::mutex> lock(voiceMtx_);
        if (voiceDone_.empty() && voiceOutbox_.empty()) return;  // 무더티
        done.swap(voiceDone_);
        outbox.swap(voiceOutbox_);
    }
    // 기록 반영(§3 ④ — 전사 원문=정직 기록) → 도구 질의 송신 순서 계약:
    // 답신([시스템] 항)이 기록에 도착한 뒤 창 서버 지시가 나간다.
    for (const VoiceDone& d : done) {
        if (d.slot >= turns_.size()) continue;   // 도달 없음(append-only 슬롯)
        // #117 T1 — D3 표지 원독(치환에 앞서): 실패 치환(Slot 재배정은 표지를
        // 지운다 — Turn{false, ...} 대입)로 지워지기 전에 1뒤.
        const bool voiceOrigin = turns_[d.slot].originVoice;
        if (!d.ok && !d.err.empty()) {
            turns_[d.slot] = Turn{ false, "[!] " + d.err };   // 정직 단문 치환
            PublishNotify(d.err);   // 인식 실패 토스트(알림 표면 — §3 ④②)
        } else {
            turns_[d.slot].text = d.sttText;   // 치환 — 전사 원문 그대로
        }
        if (!d.guide.empty()) {
            turns_.push_back(Turn{ false, d.guide });   // 접점 안내문
        }
        // #117 T1 — D2·D3·D4·D5: 구두 출발 발화 턴의 [시스템] 답신 도착 =
        // 1회 TTS. 판정은 tts::TtsSpeakWanted(3진실원 — 표지·답신 재원·
        // opt-out), 문언은 TtsReplyClip 절단판(D4), 스폰은 detached 단명
        // 스레드(워커 재용 금지 — RunVoiceTurn 마감 뒤 스폰 대기가 전사
        // FIFO를 밀지 않게; UI 축 완료 대기도 봉쇄). 타이핑 발화의 답신은
        // voiceOrigin 0이라 구조적으로 도달하지 않는다(D3 무음 존중).
        // 폰 축(#ifdef) 밖 = 스폰 leg 부재 — Win/WSL은 판정만하고 침묵(컴파일
        // 무영향 원장행).
        if (!d.guide.empty() && voiceOrigin) {
            const bool optOut = jk::tts::VoiceTtsOptOutRuntime();
            if (jk::tts::TtsSpeakWanted(true, d.guide.empty(), optOut)) {
#if defined(__ANDROID__)
                std::thread([speak = jk::tts::TtsReplyClip(d.guide)] {
                    TtsSpawnSpeak(speak);
                }).detach();
#endif
            }
        }
        if (d.ok && d.launched) {
            PublishNotify(!d.guide.empty() ? d.guide : d.sttText);
        }
        frameDirty_ = true;
        scrollBottom_ = true;
    }
    // 도구 지시 전달 — 질의는 UI 축 전용통로(Surface를 워커가 만지지 않는다).
    for (const auto& q : outbox) {
        SendQuery(q.first.c_str(), q.second);
        frameDirty_ = true;
    }
    if (!done.empty() && voiceStatus_.empty() == false) {
        // 전사 수령 = 상태행 치환(대기 표기 종료 — R5). 연속 발화 대기 중이면
        // 다음 FinishVoice가 다시 "전사 중…"을 얹는다.
        voiceStatus_.clear();
    }
}

void ClientChatApp::PublishNotify(const std::string& body) {
    jk::client::JKClientSurface* surface = Surface();
    if (!surface || !surface->IsConnected()) return;   // 토스트 2차 표면일 뿐
    // ClientNotifyApp 토스트 선존 재용(agent.notify — 서버 publish_event가
    // 스탬프를 찍고 jkapp_notify가 띄운다 — jkctl notify 원문 경로). 질의 id는
    // 토스트 대역(0x40000000+) — pending_에 넣지 않아 PollReplies의 mine 체크
    // 밖으로 흘려보낸다(기록에 publish_event 답신 JSON을 부풀리지 않는다).
    const std::string args =
        "{\"topic\":\"agent.notify\",\"data\":{\"title\":\"구두\",\"body\":\"" +
        voice::turn::JsonEscapeStr(body) + "\"}}";
    const uint32_t id = voiceToastId_++;
    (void)surface->SendAgentQuery(
        id, "{\"tool\":\"publish_event\",\"args\":" + args + "}");
}

void ClientChatApp::StopVoiceWorker() {
    voiceQuit_.store(true, std::memory_order_release);
    voiceCv_.notify_all();
    if (voice_.joinable()) voice_.join();   // 파괴 join 수형(스캔 원문 계약)
}

void ClientChatApp::VoiceWorker() {
    for (;;) {
        std::string wav;
        size_t slot = 0;
        {
            std::unique_lock<std::mutex> lock(voiceMtx_);
            voiceCv_.wait(lock, [this] {
                return voiceQuit_.load(std::memory_order_acquire) ||
                       !voiceJobs_.empty();
            });
            if (voiceQuit_.load(std::memory_order_acquire)) return;
            wav = std::move(voiceJobs_.front().wav);
            slot = voiceJobs_.front().slot;
            voiceJobs_.pop_front();
        }
        // chat.json은 워커 축에서 조달(파일 접촉 UI 축 밖 — 1-15s 전사 대기
        // 동안 UI는 무더티). 미구성 cfg = 라우터 즉발+stub 폴백 원존 —
        // "LLM이 아니어도 구두는 조작된다"(스펙 §3 ③).
        const agent::ChatConfig cfg = agent::LoadChatConfig();
        jk::voice::turn::VoiceTurnResult r;
        // dispatch = 표면 소유 통로의 워커 측 우편함 — SendQuery/Surface는 UI
        // 스레드 소유(계약)라 질의를 수발 큐에 적립하고 전송은 DrainVoice 몫.
        // launch_app 지시는 앱 실행 토스트 단문 몫(§3 ④ — 알림 표면).
        bool launched = false;
        const jk::voice::turn::ToolDispatcher disp =
            [this, &launched](const std::string& tool,
                              const std::string& args) {
                {
                    const std::lock_guard<std::mutex> lock(voiceMtx_);
                    if (voiceQuit_.load(std::memory_order_acquire)) return false;
                    voiceOutbox_.emplace_back(tool, args);
                }
                if (tool == "launch_app") launched = true;
                return true;
            };
        (void)jk::voice::turn::RunVoiceTurn(wav, cfg, disp, r);
        // 임시 wav 소각(표면 소유 임시 경로의 정산 — 캡처 leg의 절반 WAV
        // remove 수형 동일 원칙).
        std::error_code rmEc;
        std::filesystem::remove(std::filesystem::path(wav), rmEc);
        VoiceDone done;
        done.ok = r.ok;
        done.sttText = r.sttText;
        done.guide = r.guide;
        done.err = r.err;
        done.launched = launched;
        done.slot = slot;
        const std::lock_guard<std::mutex> lock(voiceMtx_);
        voiceDone_.push_back(std::move(done));
        // 기상 불요 — OnIdle의 드레인(응답 펌프 수형)이 매 이터레이션 본다.
    }
}

void ClientChatApp::BuildUi(int w, int h) {
    // 서버 크롬이 상단 밴드를 먹는다(settings 레슨 8 — library 본문 동일).
    // T3 fix r1: 고정 30 → 밴드 산식 진실원(밴드+여백 6; s=1.0 등호 30).
    const float topY = static_cast<float>(jk::text::AppContentTopOffset());
    // 입력 1행 + 전송/구두 버튼 + 상태 행 + 구두 상태 행(R5 — 정직 표기 2행).
    const float inputH = 70.0f;
    const float transH = static_cast<float>(h) - topY - inputH;

    // ---- 상: 대화 기록 ----
    ImGui::SetNextWindowPos(ImVec2(0, topY));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(w), transH));
    // settings ##settingsbody 형태: Begin-false면 End()만 하고 나간다, 본문은
    // Begin/End **안**에서 그린다 — End 뒤에 그리면 implicit "Debug" 폴백 창으로
    // 나간다(imgui.cpp:7521 — library :263 fix r1 CRITICAL 선례). NoDecoration은
    // NoScrollbar를 포함한다 — 스크롤바는 명시 AlwaysVerticalScrollbar로.
    if (!ImGui::Begin(koreanFont_ ? "기록" : "Transcript",
                      nullptr,
                      ImGuiWindowFlags_NoDecoration |
                          ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        ImGui::End();
        return;
    }
    if (turns_.empty()) {
        ImGui::TextDisabled(koreanFont_ ? "(기록이 없습니다 — 아래 입력창에서 시작)"
                                        : "(no transcript yet)");
    }
    for (const Turn& t : turns_) {
        // 발화는 본문색, 시스템(응답·답신)은 흐린 색 — 팔레트 파생색만 쓴다
        // (하드코딩 금지 docs/52 — 테마 핫스왑에 따라온다).
        if (!t.mine) {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        }
        ImGui::TextWrapped("%s%s", t.mine ? "[나] " : "[시스템] ",
                           t.text.c_str());
        if (!t.mine) ImGui::PopStyleColor();
    }
    // 새 기록 반영 프레임만 맨 아래 앵커 — 읽다 말았던 스크롤을 안 뺏는다.
    if (scrollBottom_) {
        ImGui::SetScrollHereY(1.0f);
        scrollBottom_ = false;
    }
    ImGui::End();

    // ---- 하: 입력 + 전송 + 구두 + 상태 ----
    ImGui::SetNextWindowPos(ImVec2(0, topY + transH));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(w), inputH));
    if (ImGui::Begin(koreanFont_ ? "입력" : "Input", nullptr,
                     ImGuiWindowFlags_NoDecoration |
                         ImGuiWindowFlags_NoMove)) {
        const bool capturing = voice::capture::IsCapturing();
        const char* voiceLabel =
            capturing ? (koreanFont_ ? "마침" : "Done")
                      : (koreanFont_ ? "구두" : "Voice");
        const char* sendLabel = koreanFont_ ? "전송" : "Send";
        const float rightW = ImGui::CalcTextSize(sendLabel).x +
                             ImGui::CalcTextSize(voiceLabel).x +
                             2.0f * 24.0f +
                             2.0f * ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - rightW);
        const bool enter = ImGui::InputText(
            "##chatinput", input_, sizeof(input_),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        const bool button = ImGui::Button(sendLabel);
        ImGui::SameLine();
        const bool voiceBtn = ImGui::Button(voiceLabel);
        if (enter || button) {
            const std::string text(input_);
            input_[0] = '\0';
            Submit(text);
        }
        // [구두] 토글(스펙 R2) — 버튼은 자동재키가 없어 repeat 무해(F6 핫키
        // hold 수명주기는 PreProcessMessage 원장행). 캡처 중 눌림 = 마침.
        if (voiceBtn) {
            if (capturing) FinishVoice();
            else StartVoice();
        }
        if (!status_.empty()) ImGui::TextWrapped("%s", status_.c_str());
        // 구두 상태행(R5 — "녹음 중…/전사 중…"). 표기는 상태 전이 틱에만
        // 더티를 낸다(StartVoice/FinishVoice/DrainVoice 소관 — 여기 무덴더).
        if (!voiceStatus_.empty())
            ImGui::TextWrapped("%s", voiceStatus_.c_str());
    }
    ImGui::End();
}
} // namespace jk
