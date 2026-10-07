// 채팅 앱 클라 (스펙 2026-10-07-desktop-chat-app — library 허브 원준:
// ClientLibraryApp 패턴의 요청-응답 폴링, 이벤트 구독 없음).
//
// 백엔드 슬롯(스펙 §4 — cfg 선택형 JKLlmEngine; 승격 판정=docs/80 §3, stub
// 유지): 지금은 T1 stub 라우터(jk::ChatRouterRoute)만 이 자리에 꽂는다(룰링 —
// posix엔 claude/ollama CLI가 없어 지금 실장하면 폰에서 dead code). cfg가
// 백엔드를 고르는 형태로 이 호출 자리를 갈아끼우면 된다 — 앱 나머지(기록·
// 입력·송신)는 백엔드 무관. 배선 실장=백로그(docs/80 §3 명단).
#include <apps/ClientChatApp.h>

#include <apps/ChatRouter.h>
#include <JKTextAtlas.h>   // ResolveDesktopFontPath (docs/63 폰트 계약)
#include <imgui_impl_jkwindow.h>
#include "theme/JKThemeImGui.h"
#include <SDL.h>

#include <cstdio>

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

} // namespace

ClientChatApp::~ClientChatApp() = default;

void ClientChatApp::OnInit() {
    auto main = std::make_unique<SetRoot>("Chat");
    main->SetWindowRect(JKRect{ 0, 0, 720, 540 });
    main->SetAttrFlags(WA_CHROMELESS);
    SetMainWindow(std::move(main));

    SetTimerInterval(16);   // ~60 Hz frame cadence (docs/78 backlog — 답신
                            // 폴링이 RenderOverlay에서만 일어나므로 프레임을
                            // 계속 밀어야 한다. library 선례와 동일 계약)

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
    if (ev.type == JKEventType::Timer) frameDirty_ = true;
    return true;
}

void ClientChatApp::OnFrameCommitted() { frameDirty_ = false; }

void ClientChatApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
    if (!imguiReady_) {
        if (!ImGui_ImplJKWindow_Init(renderer)) return;
        imguiReady_ = true;
    }
    PollReplies();

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
    const std::string resp = jk::ChatRouterRoute(text, action);
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
            // 서버 계약 실측(JKWindowServer.cpp:4074): close_window는
            // args.id(창 id) 한정 — 앱 지명 미지원, 무지정 id도 그냥
            // window_not_found. 라우터의 app 키는 보낼 곳이 없으므로 argless
            // 폼을 보낸다(앱→창 id 상관은 list_windows 합성 필요 — T6 과제).
            SendQuery("close_window", "{}");
            break;
        case ChatAction::Focus:
            // 동일 계약(JKWindowServer.cpp:3505): focus_window도 args.id 한정
            // — argless 폼(window_not_found 정직 회신).
            SendQuery("focus_window", "{}");
            break;
        case ChatAction::ListWindows:
            // 답신 JSON을 기록에 그대로 인쇄가 계약(스펙 §1.1) — PollReplies.
            SendQuery("list_windows", "{}");
            break;
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

void ClientChatApp::BuildUi(int w, int h) {
    // 서버 크롬이 상단 24pt를 먹는다(settings 레슨 8 — library 본문 동일).
    const float topY = 30.0f;
    const float inputH = 50.0f;   // 입력 1행 + 전송 버튼 + 상태 행
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

    // ---- 하: 입력 + 전송 + 상태 ----
    ImGui::SetNextWindowPos(ImVec2(0, topY + transH));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(w), inputH));
    if (ImGui::Begin(koreanFont_ ? "입력" : "Input", nullptr,
                     ImGuiWindowFlags_NoDecoration |
                         ImGuiWindowFlags_NoMove)) {
        const char* sendLabel = koreanFont_ ? "전송" : "Send";
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x -
                                ImGui::CalcTextSize(sendLabel).x -
                                ImGui::GetStyle().ItemSpacing.x - 24.0f);
        const bool enter = ImGui::InputText(
            "##chatinput", input_, sizeof(input_),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        const bool button = ImGui::Button(sendLabel);
        if (enter || button) {
            const std::string text(input_);
            input_[0] = '\0';
            Submit(text);
        }
        if (!status_.empty()) ImGui::TextWrapped("%s", status_.c_str());
    }
    ImGui::End();
}
} // namespace jk
