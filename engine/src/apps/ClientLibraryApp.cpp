// 라이브러리 허브 클라 (스펙 2026-10-06-app-library — settings/notes/files 4호
// 멤버, ClientSettingsApp 패턴: 요청-응답 폴링, 이벤트 구독 없음). 읽기 전용 —
// uninstall 없음(uninstall 스킵 결제 2026-10-07), 스캔은 카탈로그 몫(Task 1).
#include <apps/ClientLibraryApp.h>

#include <apps/ClientScriptApp.h>
#include <fs/JKFs.h>
#include <imgui_impl_jkwindow.h>
#include <JKJkxFile.h>
#include <JKImageLoader.h>
#include <JKTextAtlas.h>
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

// 출처 열 — CLI library-list의 `source=` 표기와 동일 3토큰(같은 진실원 표기 —
// CLI와 클라 화면이 다르게 부르는 것은 결함으로 본다).
const char* SourceLabel(LibrarySource s) {
    switch (s) {
        case LibrarySource::Jkx: return "jkx";
        case LibrarySource::Console: return "console";
        default: return "builtin";
    }
}

// 아이콘 셀 폴백 — 플랫 사각(런처 그리드의 아이콘 없음 폴백 규약 동위).
const float kIconBox = 32.0f;

} // namespace

ClientLibraryApp::~ClientLibraryApp() = default;

void ClientLibraryApp::OnInit() {
    auto main = std::make_unique<SetRoot>("Library");
    main->SetWindowRect(JKRect{ 0, 0, 920, 640 });
    main->SetAttrFlags(WA_CHROMELESS);
    SetMainWindow(std::move(main));

    SetTimerInterval(16);   // 틱 = 배송 채널(#89 T1/T2 — 활동·더티 아님.
                            // 답신 폴링은 OnIdle — 응답 수령 시 더티)

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

    // 카탈로그 스캔 — exe-dir 규약은 launch_app 존재 검증
    // (JKWindowServer.cpp:4420-4426) 본사 복사(뒤 구분자 없음, 추출 실패 빈값).
    const std::string exe = jk::fs::GetExecutablePath();
    std::string basePath;
    if (!exe.empty()) {
        const size_t baseCut = exe.find_last_of("\\/");
        if (baseCut != std::string::npos) basePath = exe.substr(0, baseCut);
    }
    std::vector<jk::LibraryEntry> entries;
    const int n = jk::LibraryScan(basePath, entries);
    rows_.reserve(rows_.size() + entries.size());
    for (jk::LibraryEntry& e : entries) {
        Row row;
        row.cat = std::move(e);
        rows_.push_back(std::move(row));
    }
    // 스캔 개수 stderr 1행 — launch_app 스폰 진단 및 프로브 캡처용.
    std::fprintf(stderr, "[library] apps=%d\n", n);
}

void ClientLibraryApp::OnClose() {
    // 행 텍스처 소각 — OnClose는 JKClientApplication.cpp:260 기준
    // DestroyHiddenRenderer(:270)보다 먼저 불린다(ClientShotApp.cpp:74 선례) →
    // hidden renderer가 아직 살아 있어 SDL_DestroyTexture가 안전하다.
    for (Row& row : rows_) {
        if (row.tex) {
            SDL_DestroyTexture(row.tex);
            row.tex = nullptr;
        }
    }
    if (imguiReady_) {
        ImGui_ImplJKWindow_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
}

void ClientLibraryApp::OnThemeChanged() {
    if (imguiReady_) jk::theme::ApplyImGuiTheme();   // docs/52 hot-swap
}

bool ClientLibraryApp::PreProcessMessage(const JKEvent& ev) {
    ImGui_ImplJKWindow_ProcessJKEvent(ev);
    // #89 T2 — Timer 무조건 더티 관용구 삭제: 타이머 틱은 활동이 아니다
    // (T1 게이트 재계약)이고 라이브러리는 정적 UI다. 남는 렌더 원 = 입력·
    // 테마(게이트 활동)·응답 수령(OnIdle 폴백 — 수령 틱만 더티).
    return true;
}

void ClientLibraryApp::OnIdle() {
    // #89 T2 — 응답 폴백의 렌더 분리(응답 도착 시 더티 — 수령 즉시 렌더).
    PollReplies();
}

void ClientLibraryApp::OnFrameCommitted() { frameDirty_ = false; }

void ClientLibraryApp::RenderOverlay(SDL_Renderer* renderer, int w, int h) {
    if (!imguiReady_) {
        if (!ImGui_ImplJKWindow_Init(renderer)) return;
        imguiReady_ = true;
    }
    // 아이콘 디코드 1회 — renderer는 RenderOverlay에만 존재(ClientShotApp
    // :196-205 선례), 첫 프레임에 래치한다.
    if (!iconsLoaded_) {
        iconsLoaded_ = true;
        LoadIcons(renderer);
    }

    ImGui_ImplJKWindow_NewFrame(1.0f / 60.0f, w, h);
    ImGui::NewFrame();
    BuildUi(w, h);
    ImGui::Render();
    ImGui_ImplJKWindow_RenderDrawData(ImGui::GetDrawData(), renderer);
}

void ClientLibraryApp::LoadIcons(SDL_Renderer* renderer) {
    // 카탈로그 hasIcon엔트리만 JKJkxFile 재열어 디코드. 폰 SW 렌더러 함정
    // (docs/78 §5.7): STREAMING 텍스처 + 선형 스케일 필터는 SW 백엔드에서 값이
    // 싸지만 1행/앱이라 수용 — 런처 JKCompositor.cpp:48 선형 계약 승계(아이콘은
    // 32x32 축소라 nearest 픽셀 깨짐 방지가 더 크다).
    for (Row& row : rows_) {
        if (row.cat.source != LibrarySource::Jkx || !row.cat.hasIcon) continue;
        jk::JKJkxFile jkx;
        if (!jkx.Open(row.cat.path)) continue;
        const JkxManifest& mani = jkx.Manifest();
        // 아이콘 wanted 산식 — 카탈로그 hasIcon 판정(JKLibraryCatalog.cpp:101)과
        // 동일한 **무조건 2x 우선**. 런처 스케일 분기 산식(JKDesktopShell.cpp:400)
        // 을 쓰지 않는다: hasIcon==true가 이 FindEntry의 성공을 보장해야 한다
        // (일치 계약 — Task 1 review deferred minor ③ — 스케일 분기로 1x를
        // 고르는 순간 hasIcon=true에서 텍스처 누락이 생긴다).
        const std::string wanted = !mani.icon2x.empty() ? mani.icon2x : mani.icon;
        const int entry = wanted.empty() ? -1 : jkx.FindEntry("ICON", wanted);
        std::vector<uint8_t> png;
        jk::LoadedImage img;
        if (entry < 0 || !jkx.ReadEntry(entry, png) ||
            !jk::LoadImageMemory(png.data(), png.size(), img) ||
            img.rgba.empty() || img.w <= 0 || img.h <= 0) {
            continue;   // 실패는 tex=nullptr로 남긴다 — 플레이스홀더 폴백(런처
                        // 그리드 규약: 아이콘 미스가 목록을 죽이지 않는다)
        }
        SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             img.w, img.h);
        if (!tex) continue;
        SDL_UpdateTexture(tex, nullptr, img.rgba.data(), img.w * 4);
        SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
        row.tex = tex;
        row.texW = img.w;
        row.texH = img.h;
    }
}

// ClientSettingsApp::SendQuery :96-107 본사 복사 — 래퍼·연결 폴백 유지, 다만
// 이 앱의 쿼리는 launch_app 1종이라 kind/arg 메타 대신 id만 적립한다.
uint32_t ClientLibraryApp::SendQuery(const char* tool, const std::string& args) {
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

void ClientLibraryApp::PollReplies() {
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
        if (reply.queryId == launchId_) {
            launchId_ = 0;
            if (reply.ok) {
                status_ = koreanFont_ ? "실행 요청됨" : "launched";
            } else {
                status_ = "[!] " + reply.json;
            }
            frameDirty_ = true;   // #89 T2 — 수령한 응답 = 이번 틱 내용 변화
        }
    }
}

void ClientLibraryApp::LaunchSelected() {
    if (selected_ < 0 || static_cast<size_t>(selected_) >= rows_.size()) return;
    const std::string& app = rows_[selected_].cat.appName;
    // launch_app 존재 검증(JKWindowServer.cpp:4401-4448): app은
    // jkapp_<app><접미> 모듈 파일이 exe 옆에 있어야 한다 — 단 "terminal:"/
    // "filedlg:" 접두는 면제(prefixed :4437). 콘솔·내장 lf/hx 엔트리의 appName은
    // "terminal:<cmdline>" 전체(prefixed)라 면제 경로가 자동 성립된다.
    const std::string args = "{\"app\":\"" + EscapeJson(app) + "\"}";
    const uint32_t id = SendQuery("launch_app", args);
    if (id) launchId_ = id;
}

void ClientLibraryApp::BuildUi(int w, int h) {
    // 서버 크롬이 상단 밴드를 먹는다(settings 레슨 8) — 본문은 밴드+여백 6부터
    // (T3 fix r1: 고정 30 → 밴드 산식 진실원; s=1.0 등호 30).
    const float topY = static_cast<float>(jk::text::AppContentTopOffset());
    const float bodyH = static_cast<float>(h) - topY;
    const float listW = 600.0f;   // 920 서피스 기준 좌 600 / 우 320

    // ---- 좌: 라이브러리 목록 ----
    ImGui::SetNextWindowPos(ImVec2(0, topY));
    ImGui::SetNextWindowSize(ImVec2(listW, bodyH));
    // settings ##settingsbody 형태: Begin-false면 End()만 하고 나간다, 본문은
    // Begin/End **안**에서 그린다 — End 뒤에 그리면 implicit "Debug" 폴백 창으로
    // 나간다(imgui.cpp:7521 — fix r1 CRITICAL). NoDecoration은 NoScrollbar를
    // 포함한다 — 스크롤바는 명시 AlwaysVerticalScrollbar로(settings 우 창 선례
    // — fix r1 2번).
    if (!ImGui::Begin(koreanFont_ ? "라이브러리" : "Library",
                      nullptr,
                      ImGuiWindowFlags_NoDecoration |
                          ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        ImGui::End();
        return;
    }

    if (rows_.empty()) {
        ImGui::TextDisabled(koreanFont_ ? "(설치된 앱이 없습니다)"
                                        : "(no installed apps)");
    } else if (ImGui::BeginTable("libtable", 5,
                                 ImGuiTableFlags_Borders |
                                     ImGuiTableFlags_RowBg |
                                     ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn(koreanFont_ ? "아이콘" : "icon",
                                ImGuiTableColumnFlags_WidthFixed, 42.0f);
        ImGui::TableSetupColumn(koreanFont_ ? "이름" : "name");
        ImGui::TableSetupColumn("title");
        ImGui::TableSetupColumn(koreanFont_ ? "출처" : "source");
        ImGui::TableSetupColumn(koreanFont_ ? "능력" : "caps");
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(rows_.size()); ++i) {
            const Row& row = rows_[i];
            ImGui::PushID(i);
            ImGui::TableNextRow();

            // 행 선택은 첫 셀의 Selectable이 담당 — SpanAllColumns로 행 전체가
            // 히트 영역(imgui 테이블 데모의 혼합 패턴), 이후 셀 내용은 그 위에
            // 다시 커서를 세워 겹쳐 그린다.
            ImGui::TableSetColumnIndex(0);
            const float rowH = kIconBox + 4.0f;
            const bool clicked =
                ImGui::Selectable("##row", selected_ == i,
                                  ImGuiSelectableFlags_SpanAllColumns,
                                  ImVec2(0.0f, rowH));
            if (clicked) {
                selected_ = i;
                frameDirty_ = true;
            }

            ImGui::TableSetColumnIndex(0);
            if (row.tex) {
                // 32x32 박스 안에서 비율 유지 축소.
                const float sx = kIconBox / static_cast<float>(row.texW);
                const float sy = kIconBox / static_cast<float>(row.texH);
                const float s = sx < sy ? sx : sy;
                ImGui::Image((ImTextureID)row.tex,
                             ImVec2(static_cast<float>(row.texW) * s,
                                    static_cast<float>(row.texH) * s));
            } else {
                const ImVec2 p = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(
                    p, ImVec2(p.x + kIconBox, p.y + kIconBox),
                    ImGui::GetColorU32(ImGuiCol_FrameBg));
                ImGui::GetWindowDrawList()->AddRect(
                    p, ImVec2(p.x + kIconBox, p.y + kIconBox),
                    ImGui::GetColorU32(ImGuiCol_TableBorderStrong));
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(row.cat.appName.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(row.cat.title.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(SourceLabel(row.cat.source));
            ImGui::TableSetColumnIndex(4);
            // 능력 배지 — ClientScriptApp.h CapabilityBadgeText(문구 고정 계약
            // docs/76 §9: "능력: <원문>" / 빈 선언 "능력 없음"). 고침 없음.
            ImGui::TextUnformatted(
                CapabilityBadgeText(row.cat.capabilities).c_str());
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::End();

    // ---- 우: 상세 ----
    ImGui::SetNextWindowPos(ImVec2(listW, topY));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(w) - listW, bodyH));
    if (ImGui::Begin(koreanFont_ ? "상세" : "Details", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        if (selected_ >= 0 && static_cast<size_t>(selected_) < rows_.size()) {
            const Row& row = rows_[selected_];
            ImGui::Text("%s: %s", koreanFont_ ? "스폰 키" : "spawn key",
                        row.cat.appName.c_str());
            ImGui::Text("%s: %s", "title", row.cat.title.c_str());
            ImGui::Text("%s: %s", koreanFont_ ? "출처" : "source",
                        SourceLabel(row.cat.source));
            // 능력 배지 — 목록과 동일 진실원 ClientScriptApp.h:277-283.
            ImGui::Text("%s", CapabilityBadgeText(row.cat.capabilities).c_str());
            ImGui::Text("%s: %s", "path",
                        row.cat.path.empty() ? "-" : row.cat.path.c_str());
            if (row.cat.sizeBytes > 0) {
                ImGui::Text("%s: %lld %s",
                            koreanFont_ ? "크기" : "size", row.cat.sizeBytes,
                            koreanFont_ ? "바이트" : "bytes");
            }
            ImGui::Separator();
            if (ImGui::Button(koreanFont_ ? "실행" : "Launch")) {
                LaunchSelected();
            }
            if (!status_.empty()) ImGui::TextWrapped("%s", status_.c_str());
            // MANI 원문(스펙 §3 "상세: 선택 항목의 MANI 원문·크기·경로") —
            // 카탈로그에서 전승한 원문 그대로(.jkx=패키지 내 MANI 엔트리 bytes,
            // 콘솔=manifest.json, 내장=공란 → 블록 숨김). 창은 스크롤 가능이라
            // 여러 줄 무해 — TextUnformatted 1회(파싱·재조립 없음).
            if (!row.cat.manifestRaw.empty()) {
                ImGui::Separator();
                ImGui::TextUnformatted(koreanFont_ ? "MANI 원문" : "MANI raw");
                ImGui::TextUnformatted(row.cat.manifestRaw.c_str());
            }
        } else {
            ImGui::TextDisabled(koreanFont_ ? "선택된 앱이 없습니다"
                                            : "no selection");
        }
    }
    ImGui::End();
}
} // namespace jk
