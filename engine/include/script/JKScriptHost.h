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
    // winId is a script timer id (see ScriptTimerWinIdBase).
    void DispatchClick(uint16_t controlId);
    void DispatchTimerAt(uint32_t scriptTimerId);

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

    int assertChecks_ = 0;
    int assertFailures_ = 0;
};

} // namespace jk

#endif // JKSCRIPTHOST_H