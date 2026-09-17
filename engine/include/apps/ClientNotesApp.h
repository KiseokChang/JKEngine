#ifndef CLIENTNOTESAPP_H
#define CLIENTNOTESAPP_H

// 노트 허브 클라 (specs/2026-09-18-notes-hub): 2탭 ImGui 앱 — 코멘트(마진
// 코멘트, 창 링크) + 백로그(대기/진행/완료 3열 보드). ClientAgentMgrApp
// 패턴: 요청-응답 폴링, 이벤트 구독 없음(에이전트 노트 유입은 알림 센터가
// 담당 — agent.notify 방송은 서버 몫).

#include <client/JKClientApplication.h>
#include <cstdint>
#include <string>
#include <vector>

namespace jk {

struct NoteRowUi { long long id = 0; std::string text; int win = 0;
                   long long ts = 0; std::string src; };
struct NoteItemUi { long long id = 0; std::string title; int state = 0;
                    long long ts = 0; std::string src; };

class ClientNotesApp : public JKClientApplication {
public:
    ClientNotesApp() = default;
    ~ClientNotesApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    void OnThemeChanged() override;   // ImGui 팔레트 재적용 (docs/52)
    bool PreProcessMessage(const JKEvent& ev) override;
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;

private:
    enum class Query { Read, Windows, Write };
    struct PendingQuery { Query kind; };

    void BuildUi(int w, int h);
    void BuildCommentsTab();
    void BuildBacklogTab();
    void PollReplies();
    void SendQuery(const char* tool, const std::string& args, Query kind);
    void ApplyReply(Query kind, const std::string& json);

    bool frameDirty_ = true;
    bool imguiReady_ = false;
    bool koreanFont_ = false;   // Malgun Gothic (한글 폰트, notify 선례)
    int tab_ = 0;               // 활성 탭 — BuildUi의 BeginTabItem이 기록
    uint32_t nextQueryId_ = 1;
    std::string status_;        // 하단 상태줄 (마지막 reply/에러)

    std::vector<NoteRowUi> notes_;      // notes_read 파일 순서(구→신)
    std::vector<NoteItemUi> items_;
    std::vector<std::pair<uint32_t, std::string>> windows_;  // id, title

    // 입력 버퍼 — 코멘트(+창 피커) / 백로그 각각.
    char commentBuf_[1024] = {};
    char backlogBuf_[256] = {};
    int winSel_ = 0;            // 코멘트 창 피커 (0=범용, 1..=windows_)
    // Write 성공 후 즉시 재조회(수동 새로고침 + 자기 쓰기 반영).
    std::vector<std::pair<uint32_t, PendingQuery>> pending_;
};

} // namespace jk
#endif // CLIENTNOTESAPP_H