#ifndef CLIENTLIBRARYAPP_H
#define CLIENTLIBRARYAPP_H

// 라이브러리 허브 클라 (스펙 2026-10-06-app-library — settings/notes/files 4호
// 멤버, ClientSettingsApp 패턴: 요청-응답 폴링, 이벤트 구독 없음).
// 읽기 전용 설비 — uninstall 없음(Q2 스킵 결제), 자기 기기 apps/를 직접
// 읽어 본다(수신 기기에서는 수신 기기 apps/가 진실원 — 파일 시스템 기반
// 자동 성립).

#include <client/JKClientApplication.h>
#include <JKLibraryCatalog.h>

#include <cstdint>
#include <string>
#include <vector>

namespace jk {

// 목록(좌) + 상세(우) 2창 ImGui 클라. 카탈로그(jk::LibraryScan — Task 1 pure
// 스캔)를 OnInit에서 1회 먹고, 아이콘 텍스처는 첫 RenderOverlay에서 1회 디코드
// 한다(hidden renderer는 RenderOverlay에만 존재 — ClientShotApp 선례).
class ClientLibraryApp : public JKClientApplication {
public:
    ClientLibraryApp() = default;
    ~ClientLibraryApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    void OnThemeChanged() override;   // ImGui 팔레트 재적용 (docs/52)
    bool PreProcessMessage(const JKEvent& ev) override;
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;

private:
    struct Row {                      // 목록 행 — 카탈로그 + 런타임 텍스처
        jk::LibraryEntry cat;
        SDL_Texture* tex = nullptr;   // 아이콘(있으면) — 클라 hidden renderer
        int texW = 0, texH = 0;
    };

    void BuildUi(int w, int h);
    void LoadIcons(SDL_Renderer* renderer);   // 카탈로그 ICON → 텍스처 1회
    void PollReplies();                       // AgentQueryReply 소비 — 런치 답신
    uint32_t SendQuery(const char* tool, const std::string& args);
    void LaunchSelected();            // launch_app 재용 (폰 TX6 실측 경로)

    std::vector<Row> rows_;
    int selected_ = -1;
    std::string status_;
    bool frameDirty_ = true;
    bool imguiReady_ = false;
    bool koreanFont_ = false;
    uint32_t nextQueryId_ = 1;
    uint32_t launchId_ = 0;           // 응답 식별 — SendQuery 폴링 1종
    bool iconsLoaded_ = false;        // 첫 RenderOverlay에서 디코드 1회
    std::vector<uint32_t> pending_;   // AgentQueryReply 라운트트립 체
};

} // namespace jk
#endif // CLIENTLIBRARYAPP_H