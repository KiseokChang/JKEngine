#ifndef JKSERVERSERVICES_H
#define JKSERVERSERVICES_H

// [P1 — docs/108 2단계] 에이전트 허브가 서버에 요구하는 최소 창 표면.
// 허브(HandleAgentQuery 44도구 — 3단계 추출 대상)가 컴포지터 내부(clients_·
// focusedClientId_)에 직접 결합되지 않도록 하는 경계. 구현 = JKWindowServer
// (동작 무변). Focus/Close 승격은 3단계(핸들러 몸통 이동)에서 — 그 전까지
// 허브는 기존 코드 경로 그대로.
#include <cstdint>
#include <string>
#include <vector>

namespace jk {
namespace server {

// 에이전트 창 참조(1n-14.. 계약 승계 — 원문 주석은 JKWindowServer.h 참조).
struct AgentWindowRef {
    uint32_t id = 0;
    std::string title;
    bool focused = false;
};
// 매칭 0 = window_not_found(정직 실패 계약). 순수 로직.
uint32_t ResolveAgentWindowTarget(const std::vector<AgentWindowRef>& windows,
                                  bool hasId, uint32_t id,
                                  const std::string& app);

struct IWindowServerServices {
    virtual ~IWindowServerServices() = default;

    // 서버가 추적하는 창 스냅샷(컨트롤 온리·셸·해제 창 제외) — 허브의
    // list/close/focus 대상 해소 근거. clientsMutex_ 자체 보호(호출 안전).
    virtual std::vector<AgentWindowRef> AgentWindowSnapshot() = 0;

    // state 파일 루트(docs/108 1단계 JKServerState 승계).
    virtual std::string StateRoot() const = 0;

    // 앱 스폰(기존 SpawnClient 계약 — fromJkx = .jkx 컨테이너 경로).
    virtual bool SpawnAgentClient(const char* appName, bool fromJkx) = 0;
};

}  // namespace server
}  // namespace jk

#endif  // JKSERVERSERVICES_H
