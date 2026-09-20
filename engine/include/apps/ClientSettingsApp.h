#ifndef CLIENTSETTINGSAPP_H
#define CLIENTSETTINGSAPP_H

// 설정 허브 클라 (specs/2026-09-18-settings-hub): 단일 스크롤 5섹션 ImGui 앱 —
// 테마/트리거/데이터/권한/장치. ClientAgentMgrApp 패턴(요청-응답 폴링,
// 이벤트 구독 없음 — 설정 화면은 정적 데이터). 권한 본체는 agentmgr이 소유 —
// 이 앱은 요약+라우팅(launch_app agentmgr)만.

#include <client/JKClientApplication.h>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace jk {

struct SetTriggerRow { std::string name; int enabled = 1; };
struct SetPermRow { std::string tool, gate, file, effective, deflt; };

class ClientSettingsApp : public JKClientApplication {
public:
    ClientSettingsApp() = default;
    ~ClientSettingsApp() override;

protected:
    void OnInit() override;
    void OnClose() override;
    void OnThemeChanged() override;   // ImGui 팔레트 재적용 (docs/52)
    bool PreProcessMessage(const JKEvent& ev) override;
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;

private:
    enum class Query { Read, Perms, ThemeSet, TriggerToggle, Set, LayoutSave,
                       LayoutRestore, Launch };
    struct PendingQuery { Query kind; std::string arg; };

    void BuildUi(int w, int h);
    void PollReplies();
    void SendQuery(const char* tool, const std::string& args, Query kind,
                   const std::string& arg = {});
    void ApplyReply(Query kind, const std::string& json,
                    const std::string& arg);
    void ApplyRead(const std::string& json);   // settings_read 봉투 흡수

    // kv 원천(settings_read) + 섹션이 쓰는 파싱된 상태.
    std::map<std::string, std::string> kv_;      // key → value(문자열)
    std::vector<SetTriggerRow> triggers_;
    std::vector<std::string> layouts_;
    std::vector<SetPermRow> perms_;
    long long receiptRows_ = 0;
    long long receiptLastTs_ = 0;

    bool frameDirty_ = true;
    bool imguiReady_ = false;
    bool koreanFont_ = false;
    uint32_t nextQueryId_ = 1;
    std::string status_;
    std::string pendingCapture_;   // 승인 대기 표시
    // 볼륨 슬라이더 커밋-on-release(vplayer seekingUi_ 선례): 드래그 중에는
    // 임시값, 해제 시점에 settings_set 1회.
    int volumeTemp_ = 80;
    bool volumeDragging_ = false;
    // 보존기간 콤보 — settings_read value와 동기(사용자 조작 중엔 임시 유지).
    int retentionSel_ = -1;
    // 텍스트 폰트 경로 입력 (docs/63 §4) — Enter 시 settings_set. 300은 서버
    // 캡과 동일(초과치는 bad_value).
    char textFontBuf_[300] = {};
    // 보조 폰트 체인 경로 입력 (docs/63 §6 2단계) — 빈 값 = 해제(서버가 허용).
    // 1회 시드 플래그 — **첫 settings_read 도착 시**(ApplyRead)에 래치한다.
    // 첫 UI 프레임 래치는 kv_ 공백(비동기 답신 도착 전)을 근거로 삼아
    // 설정된 경로의 영구 미표시=사용자 Enter로 해제 전송 사각이 됐다
    // (리뷰 fix-2). 래치 후에는 재시드 없음 — 삭제 원본 유지.
    char fallbackFontBuf_[300] = {};
    bool fallbackSeeded_ = false;
    // 셀 확대 배율 입력 (docs/63 §6 Task 3) — 숫자 문자열(1.0–3.0), Enter 시
    // settings_set. 시드는 fallbackFontBuf_와 동일 **첫 settings_read 도착
    // 시**(ApplyRead) 1회 래치 — 키 부재 시 기본 "1.0" 표시.
    char scaleBuf_[16] = {};
    bool scaleSeeded_ = false;
    std::vector<std::pair<uint32_t, PendingQuery>> pending_;
};

} // namespace jk
#endif // CLIENTSETTINGSAPP_H