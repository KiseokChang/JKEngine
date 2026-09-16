#ifndef CLIENTAGENTMGRAPP_H
#define CLIENTAGENTMGRAPP_H

// 에이전트 관리자 클라 (specs/2026-09-16-agent-manager): 4탭 ImGui 앱 —
// 권한 매트릭스/트리거/신뢰/설치+receipts. ClientNotifyApp 템플릿(docs/33),
// 이벤트 구독 없음 — 요청-응답 폴링(팔레트 선례: 클라 read 루프는 프레임 루프
// 결합이라 PollAgentReply로 큐를 드레인한다).

#include <client/JKClientApplication.h>
#include <cstdint>
#include <string>
#include <vector>

namespace jk {

struct MgrPermRow { std::string tool, gate, file, effective, deflt; };
// topics는 2레벨 리더가 못 읽어(파서 계약, 구현 주석 참고) 열에서 뺐다 —
// 트리거 탭은 name/enabled만.
struct MgrTriggerRow { std::string name; int enabled = 1; };
struct MgrTrustRow { std::string name, source, fp, shortFp; };
struct MgrInstalledRow { std::string name, kind; bool running = false; };
struct MgrReceiptRow { long long ts = 0; std::string tool; bool ok = false; };

class ClientAgentMgrApp : public JKClientApplication {
public:
    ClientAgentMgrApp() = default;
    ~ClientAgentMgrApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    void OnThemeChanged() override;   // ImGui 팔레트 재적용 (docs/52)
    bool PreProcessMessage(const JKEvent& ev) override;
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;

private:
    enum class Query { Perms, Triggers, Trust, InstalledApps, RunningWindows,
                       Receipts, Toggle, PermissionSet, TrustRevoke };
    struct PendingQuery { Query kind; std::string arg; };

    void BuildUi(int w, int h);
    void BuildPermissionsTab();
    void BuildTriggersTab();
    void BuildTrustTab();
    void BuildInstalledTab();
    void PollReplies();
    void SendQuery(const char* tool, const std::string& args, Query kind,
                   const std::string& arg = {});
    void Refresh(int tab);              // 0=권한 1=트리거 2=신뢰 3=설치
    void ApplyReply(Query kind, const std::string& json);
    void MergeRunning();

    bool frameDirty_ = true;
    bool imguiReady_ = false;
    bool koreanFont_ = false;   // Malgun Gothic (한글 폰트, notify 선례)
    int tab_ = 0;               // 활성 탭 — BuildUi의 BeginTabItem이 기록
    uint32_t nextQueryId_ = 1;
    std::string status_;        // 하단 상태줄 (마지막 reply/에러)
    std::string pendingPerm_;   // 승인 대기 표시 "tool → decision"
    std::string trustStoreError_;  // 신뢰 탭 에러 박스 (스펙 §5)

    std::vector<MgrPermRow> perms_;
    std::vector<MgrTriggerRow> triggers_;
    std::vector<MgrTrustRow> trust_;
    std::vector<MgrInstalledRow> installed_;
    std::vector<MgrReceiptRow> receipts_;
    std::vector<std::pair<uint32_t, std::string>> windows_;  // id, title
    std::vector<std::pair<uint32_t, PendingQuery>> pending_;
};

} // namespace jk
#endif // CLIENTAGENTMGRAPP_H