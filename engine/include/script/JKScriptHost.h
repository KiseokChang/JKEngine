#ifndef JKSCRIPTHOST_H
#define JKSCRIPTHOST_H

// JavaScript (QuickJS-ng) application host — docs/27.
//
// Embeds the quickjs-ng interpreter and exposes host API v1 to a script:
//   log, messageBox, createButton/createLabel/createEdit, setText/getText,
//   setInterval/clearInterval.
// The script defines global callbacks the host invokes:
//   onCreate() — after evaluation; onClick(controlId) — button clicks;
//   onExit()   — before the context is torn down.
//
// Threading: main/UI thread only (docs/27 §3.2). A QuickJS runtime is never
// shared across threads. Background sources deliver their results through the
// owning application's event loop, which calls back into DispatchTimer().
//
// Sandboxing: scripts see ONLY what this class binds (plus the QuickJS core
// builtins, which have no file/network access — docs/27 §3.2).
//
// quickjs.h stays inside the .cpp; client TUs never need it. The JSValue RAII
// holder is an implementation detail (a value-type wrapper — JSValue is a
// 16-byte POD, so no heap unique_ptr wrapping).

#include <JKTypes.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace jk {

class JKWindow;
class JKControl;
class JKMessageBox;
namespace script_detail { struct Bindings; }  // quickjs thunks (the .cpp)

// Script timer plumbing. The owning application wires these to its timer
// thread (AddTimer/RemoveTimer) and routes JKEventType::Timer events whose
// winId falls in the script range back to DispatchTimerAt().
struct JKScriptTimerServices {
    // Registers a repeating timer for winId; returns the app timer handle.
    std::function<uint64_t(uint32_t winId, uint32_t intervalMs)> start;
    // Cancels a previously started handle.
    std::function<void(uint64_t handle)> stop;
};

class JKScriptHost {
public:
    JKScriptHost();
    ~JKScriptHost();
    friend struct script_detail::Bindings;

    JKScriptHost(const JKScriptHost&) = delete;
    JKScriptHost& operator=(const JKScriptHost&) = delete;

    // Binds the window the script builds controls into. Must be called
    // before Start(). The window must outlive the host (the app owns it).
    void Attach(JKWindow* window);

    // Wires the timer services (both callbacks required for setInterval).
    void SetTimerServices(JKScriptTimerServices services);

    // Loads and evaluates `entryPath` (UTF-8 app.js), then calls the script's
    // onCreate() when defined. Returns false on file or script errors — the
    // error (message + JS stack trace) is logged and kept in LastError().
    bool Start(const std::string& entryPath);

    // Calls the script's onExit() (when defined) and tears the context down.
    // Safe to call twice; the destructor calls it.
    void Stop();

    // Reload support (hot reload, docs/27 단계 1): Stop + Start of the last
    // entry path. Returns false when Start was never successful.
    bool Reload();

    // Dispatchers (main thread). DispatchClick is invoked by buttons the
    // script created; DispatchTimerAt by the app for a Timer event whose
    // winId is a script timer id (see ScriptTimerWinIdBase);
    // DispatchDialogClose by the script's modal dialogs when they close
    // (docs/27 단계 3 — routes into the dialog's JS onClose(result)).
    void DispatchClick(uint16_t controlId);
    void DispatchTimerAt(uint32_t scriptTimerId);
    void DispatchDialogClose(uint32_t dialogId, int result);
    // Canvas input dispatchers (docs/60 §10): the JKScriptCanvas input sink
    // funnels events here; the script's global onMouse(type,x,y,canvasId,button) /
    // onWheel(dy,x,y) / onKey(key,down) run when defined (absent = ignored).
    // kind: 0=down, 1=up, 2=move; button: SDL 1=left 2=middle 3=right (0 on
    // move); key=SDL keycode, down=1/0.
    void DispatchCanvasMouse(uint16_t canvasId, int kind, int32_t x, int32_t y,
                             int32_t button);
    void DispatchCanvasWheel(uint16_t canvasId, int32_t dy, int32_t x,
                             int32_t y);
    void DispatchCanvasKey(uint32_t key, bool down);

    // 의미 커서 (스펙 2026-09-22-semantic-cursor, 워크숍 확장 docs/60 §5):
    // declareCursor 바인딩이 검증해 봉합한 선언 원문(서버 cursor 블록 JSON).
    // 빈 문자열 = 미선언 — Start/Stop마다 리셋(새 스크립트가 선언하지 않으면
    // 이전 스크립트의 커서가 잔존하지 않는다).
    const std::string& DeclaredCursorJson() const { return cursorDeclJson_; }
    // 선언 봉합 시(재선언 포함) 알림 — 앱이 AgentToolRegister를 재송신한다.
    // 스크립트 평가 중(메인 스레드)에만 호출된다.
    void SetCursorDeclChanged(std::function<void(const std::string& json)> cb) {
        cursorDeclChanged_ = std::move(cb);
    }
    // 전역 함수 정의 여부 (선언 시점 계약 판정 — act 도구 등록 조건 등).
    bool HasGlobalFn(const char* name) const;
    // act 중계 (서버 앱 도구 릴레이가 도착한 kind/row/col을 스크립트의 전역
    // onAgentAct로 전달). onAgentAct 부재/예외도 도구 응답으로 표면화한다
    // (talk-to-fix 폐곡선 — 조용한 눌먹기 금지). ok=false = 도구 실패.
    // 반환값: false = 컨텍스트 소멸(호출 불가), true = 결과 봉합 완료.
    bool DispatchAgentAct(const std::string& kind, int row, int col,
                          bool& ok, std::string& resultJson);
    // snapshot 중계 (docs/60 §13 후속): 커서 read의 앱 snapshot 직렬화를
    // 스크립트의 전역 onSnapshot()으로 제공. 반환 계약은 DispatchAgentAct와
    // 동일(문자열=원문, 객체=JSON.stringify). 빈 결과/부재/예외는 ok=false.
    bool DispatchAgentSnapshot(bool& ok, std::string& resultJson);

    // --- 상태 보존 리로드 (docs/67 단 1 — 사훈 1의 폴백 경로) ---------------
    // 캡처: 편집창(JKEdit)만 — 텍스트(KSSM→UTF-8 경계 변환)+입력모드(한/영).
    // 라벨은 스크립트 소유 파생 출력이라 복원하지 않는다(한계 문서화).
    // 형식 {"v":1,"edits":[{"i":N,"t":"...","m":M},...]} — i=편집창 생성순번.
    // 편집창이 하나도 없으면 false(스냅샷 대상 없음 — 호출자 생략).
    bool CaptureWidgetState(std::string& outJson) const;
    // 복원: 새 controls_의 j번째 JKEdit ↔ edits[j](편집창 생성순서 접두 매칭 —
    // 컨트롤 id는 Start마다 1000 리셋이므로 순번이 유일한 안정 키). 초과분
    // 무시·부족분 신규 기본값 — 양쪽 모두 "[script] state restore: n/m edits"
    // 로그로 표면화(조용한 눌먹기 금지 — 상태 동일성 게이트가 이 로그 단정).
    // Start 후(ctx 생존)에만 호출 가능(QuickJS JSON.parse 사용).
    bool RestoreWidgetState(const std::string& json);
    // 스크립트의 onSaveState() 호출. 반환 계약은 onSnapshot과 동일(문자열=원문,
    // 객체=JSON.stringify). 훅 부재·예외·빈 결과 = jsonOut ""(false).
    bool DispatchSaveState(std::string& jsonOut);
    // 스크립트의 onRestoreState(saved) 호출 — additive 훅(부재=무사). 예외는
    // 로그 덤프하고 계속한다(브릭 금지 — 복원 실패가 리로드를 죽이지 않는다).
    // 반환: false = 컨텍스트 소멸, true = 봉합 완료(예외 포함).
    bool DispatchRestoreState(const std::string& json);
    // 리로드 직전 캡처한 JS 상태를 Start 후반(onCreate 직후)에 소비하도록
    // 적재. 불변 = pending 1건 ↔ Start 1회 — 적재는 항상 Start 직전에
    // (Start 서두에서 클리어하지 않는다: 적재가 흡수된다 — 2026-09-26 유닛
    // 프로브 c4 실측). 빈 원문 적재 = 훅 무해 통과.
    void SetPendingRestoreState(const std::string& json) {
        pendingRestoreJson_ = json;
    }

    // Introspection (self-test, docs/27 §2.4): the property names actually
    // visible to the script via Object.getOwnPropertyNames(globalThis).
    std::vector<std::string> BoundNames() const;

    // winId range the host claims for script timers. Applications route
    // Timer events with winId >= ScriptTimerWinIdBase to DispatchTimerAt.
    static constexpr uint32_t ScriptTimerWinIdBase = 0x7F00;

    bool IsRunning() const { return ctx_ != nullptr; }
    const std::string& LastError() const { return lastError_; }

    // Assertion counters (docs/27 단계 2): assert/assertEq record here and the
    // `test-script` runner turns the failure count into the exit code.
    int AssertChecks() const { return assertChecks_; }
    int AssertFailures() const { return assertFailures_; }

    // Entry path of the last successful Start ("" before that).
    const std::string& EntryPath() const { return entryPath_; }

private:
    struct Impl;
    Impl* impl_ = nullptr;          // JS timer registry (owns JSValue fns)

    // Opaque QuickJS handles (full types live in the .cpp).
    void* rt_ = nullptr;
    void* ctx_ = nullptr;

    JKWindow* window_ = nullptr;
    JKScriptTimerServices timers_;

    // Control registry: controlId -> control added to window_.
    std::vector<std::pair<uint16_t, JKControl*>> controls_;
    uint16_t nextControlId_ = 1000;

    // Modal slot for jk.messageBox (reused per host).
    std::unique_ptr<JKMessageBox> msgboxSlot_;

    std::string entryPath_;
    std::string lastError_;

    // 의미 커서 (declareCursor 봉합 원문; 빈 문자열 = 미선언).
    std::string cursorDeclJson_;
    std::function<void(const std::string& json)> cursorDeclChanged_;

    // 상태 보존 리로드 (docs/67 단 1): 리로드 직전 캡처한 JS 상태 원문.
    // 소비 후 클리어(Start 후반 블록) — 불변 = pending 1건 ↔ Start 1회.
    std::string pendingRestoreJson_;

    int assertChecks_ = 0;
    int assertFailures_ = 0;
};

} // namespace jk

#endif // JKSCRIPTHOST_H