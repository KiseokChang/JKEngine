#ifndef APPS_CLIENTMINESWEEPERAPP_H
#define APPS_CLIENTMINESWEEPERAPP_H

#include <client/JKClientApplication.h>
#include <JKEvent.h>
#include <memory>

namespace jk {

class MineGameWindow;

// Separate-process Minesweeper client. Renders into a server-managed surface via
// JKClientApplication instead of owning a visible SDL window.
class ClientMineSweeperApp : public JKClientApplication {
public:
    ClientMineSweeperApp();
    ~ClientMineSweeperApp() override;

protected:
    void OnInit() override;
    bool PreProcessMessage(const JKEvent& ev) override;
    // 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §8.2): 코어 펌프가 이 훅으로
    // 중계한다. 의미 커서 act/snapshot (스펙 2026-09-22-semantic-cursor §3)은
    // MineGameWindow의 게임 전이+뷰 배선으로 포워딩한다.
    bool OnAgentToolCall(const std::string& tool, const std::string& argsJson,
                         std::string& resultJson) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace jk

#endif // APPS_CLIENTMINESWEEPERAPP_H
