#ifndef CLIENTFILESAPP_H
#define CLIENTFILESAPP_H

// 파일 허브 클라 (specs/2026-09-18-file-hub): 3패널 ImGui 앱 — 좌측 브라우저
// (경로 + 목록), 우측 미리보기, 하단 에이전트 접근 로그. ClientNotesApp 패턴:
// 요청-응답 폴링, 이벤트 구독 없음(files_audit 수동 새로고침). 도구 호출은
// 자기 window 연결 — 서버 게이트의 사용자 소스(무승인).

#include <client/JKClientApplication.h>
#include <cstdint>
#include <string>
#include <vector>

namespace jk {

struct FileEntryUi { std::string name; bool dir = false;
                     long long size = 0; long long mtime = 0; };
struct FileAuditRowUi { long long ts = 0; std::string tool; bool ok = false;
                        std::string path; };

class ClientFilesApp : public JKClientApplication {
public:
    ClientFilesApp() = default;
    ~ClientFilesApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    void OnThemeChanged() override;   // ImGui 팔레트 재적용 (docs/52)
    bool PreProcessMessage(const JKEvent& ev) override;
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;

private:
    enum class Query { List, Read, Audit };
    struct PendingQuery { Query kind; };

    void BuildUi(int w, int h);
    void BuildBrowser(int leftW);
    void BuildPreview();
    void BuildAuditPanel(int w, int h);
    void PollReplies();
    void SendQuery(const char* tool, const std::string& args, Query kind);
    void ApplyReply(Query kind, const std::string& json);
    // 현재 경로 + 자식 이름 → 자식 절대 경로 (구분자 정규화).
    std::string JoinPath(const std::string& name) const;

    bool frameDirty_ = true;
    bool imguiReady_ = false;
    bool koreanFont_ = false;   // Malgun Gothic (한글 폰트, notify 선례)
    uint32_t nextQueryId_ = 1;
    std::string status_;        // 하단 상태줄 (마지막 reply/에러)

    char pathBuf_[260] = {};    // 현재 경로 (편집 가능 — 새로고침으로 이동)
    std::vector<FileEntryUi> entries_;
    bool listCapped_ = false;   // 512행 캡 도달

    // 미리보기 (파일 선택 시 files_read).
    std::string prevName_;
    std::string prevText_;
    bool prevBinary_ = false;
    bool prevTrunc_ = false;
    long long prevSize_ = 0;

    // 에이전트 접근 로그 (files_audit — 수동 새로고침).
    std::vector<FileAuditRowUi> audit_;

    std::vector<std::pair<uint32_t, PendingQuery>> pending_;
};

} // namespace jk
#endif // CLIENTFILESAPP_H