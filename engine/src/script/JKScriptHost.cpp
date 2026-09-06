#include <script/JKScriptHost.h>

#include <JKButton.h>
#include <JKControl.h>
#include <JKEdit.h>
#include <JKEvent.h>
#include <JKMessageBox.h>
#include <JKStatic.h>
#include <JKWindow.h>
#include <apps/AppUtil.h>
#include <quickjs.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace jk {

namespace {

// ---------------------------------------------------------------------------
// JSValue RAII holder — value-type wrapper (docs/27 §3.2/§5 단계 1). JSValue is
// a 16-byte POD carrying a refcounted pointer; the holder frees it in the
// destructor. No heap unique_ptr<JSValue> wrapping: every JSValue would pay an
// allocation, and moves only swap ownership of the POD.
// ---------------------------------------------------------------------------
struct JsValue {
    JSContext* ctx = nullptr;
    JSValue v{};

    JsValue() = default;
    JsValue(JSContext* c, JSValue val) : ctx(c), v(val) {}
    JsValue(const JsValue&) = delete;
    JsValue& operator=(const JsValue&) = delete;
    JsValue(JsValue&& o) noexcept : ctx(o.ctx), v(o.v) {
        o.ctx = nullptr;
        o.v = JS_UNDEFINED;
    }
    JsValue& operator=(JsValue&& o) noexcept {
        if (ctx) JS_FreeValue(ctx, v);
        ctx = o.ctx;
        v = o.v;
        o.ctx = nullptr;
        o.v = JS_UNDEFINED;
        return *this;
    }
    ~JsValue() {
        if (ctx) JS_FreeValue(ctx, v);
    }

    JSValue value() const { return v; }
};

std::string ToUtf8(JSContext* ctx, JSValueConst v) {
    size_t len = 0;
    const char* s = JS_ToCStringLen(ctx, &len, v);
    if (!s) return std::string();
    std::string out(s, len);
    JS_FreeCString(ctx, s);
    return out;
}

// {x, y, w, h} object -> JKRect. Returns false when the argument is not an
// object or any property fails to convert.
JKRect RectFromArg(JSContext* ctx, JSValueConst v, bool* ok) {
    *ok = false;
    if (!JS_IsObject(v)) return JKRect{};
    JsValue px(ctx, JS_GetPropertyStr(ctx, v, "x"));
    JsValue py(ctx, JS_GetPropertyStr(ctx, v, "y"));
    JsValue pw(ctx, JS_GetPropertyStr(ctx, v, "w"));
    JsValue ph(ctx, JS_GetPropertyStr(ctx, v, "h"));
    int32_t x = 0, y = 0, w = 0, h = 0;
    if (JS_ToInt32(ctx, &x, px.value()) || JS_ToInt32(ctx, &y, py.value()) ||
        JS_ToInt32(ctx, &w, pw.value()) || JS_ToInt32(ctx, &h, ph.value())) {
        return JKRect{};
    }
    *ok = true;
    return JKRect{ x, y, w, h };
}

// Formats the pending exception (message + JS stack trace, docs/27 §3.2 — the
// agent self-heals from the log text) and consumes it.
std::string DumpPendingException(JSContext* ctx) {
    JsValue exc(ctx, JS_GetException(ctx));
    std::string out;
    if (JS_IsNull(exc.value())) return out;
    if (JS_IsError(exc.value())) {
        JsValue msg(ctx, JS_GetPropertyStr(ctx, exc.value(), "message"));
        JsValue stack(ctx, JS_GetPropertyStr(ctx, exc.value(), "stack"));
        out += ToUtf8(ctx, msg.value());
        const std::string trace = ToUtf8(ctx, stack.value());
        if (!trace.empty()) {
            out += "\n";
            out += trace;
        }
    } else {
        out += "thrown value: " + ToUtf8(ctx, exc.value());
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// Impl — state that must not leak into the header (owns JSValue refs).
// ---------------------------------------------------------------------------

struct JKScriptHost::Impl {
    // winId = ScriptTimerWinIdBase + scriptTimerId must stay well below the
    // ranges real windows use; 0x1000 timers per context is far beyond what a
    // UI script schedules.
    static constexpr uint32_t kScriptTimerLimit = 0x1000;

    struct TimerEntry {
        uint64_t appHandle = 0;  // owner application's timer (JKTimerThread)
        JSValue fn{};            // owned ref to the interval callback
    };

    // scriptTimerId -> entry. Vector (not map): scripts hold a handful of
    // timers and dispatch order is irrelevant.
    std::vector<std::pair<uint32_t, TimerEntry>> timers;
    uint32_t nextTimerId = 0;
};

// ---------------------------------------------------------------------------
// Host API v1 bindings (docs/27 §4). static thunks in script_detail::Bindings
// (a friend of JKScriptHost) so quickjs.h stays out of the header while the
// thunks reach window_/controls_/impl_ directly.
// ---------------------------------------------------------------------------

namespace script_detail {

struct Bindings {
    static JKScriptHost* HostOf(JSContext* ctx) {
        return static_cast<JKScriptHost*>(JS_GetContextOpaque(ctx));
    }

    static JKControl* FindControl(JKScriptHost* host, uint16_t id) {
        for (const auto& c : host->controls_) {
            if (c.first == id) return c.second;
        }
        return nullptr;
    }

    // Explicit free id (>0) when given; otherwise the next auto id (>= 1000,
    // skipping collisions so explicit ids are never handed out twice).
    static uint16_t ResolveControlId(JKScriptHost* host, JSContext* ctx,
                                     JSValueConst idArg) {
        int32_t requested = -1;
        if (JS_IsNumber(idArg) && !JS_ToInt32(ctx, &requested, idArg) &&
            requested >= 1 && requested <= 0xFFFF &&
            FindControl(host, static_cast<uint16_t>(requested)) == nullptr) {
            return static_cast<uint16_t>(requested);
        }
        uint16_t id = host->nextControlId_;
        while (FindControl(host, id) != nullptr) ++id;
        host->nextControlId_ = static_cast<uint16_t>(id + 1);
        return id;
    }

    static void EraseTimer(JKScriptHost* host, JSContext* ctx, uint32_t id) {
        auto& timers = host->impl_->timers;
        for (auto it = timers.begin(); it != timers.end(); ++it) {
            if (it->first != id) continue;
            if (host->timers_.stop) host->timers_.stop(it->second.appHandle);
            JS_FreeValue(ctx, it->second.fn);
            timers.erase(it);
            return;
        }
    }

    // --- thunks ----------------------------------------------------------

    static JSValue Log(JSContext* ctx, JSValueConst, int argc,
                       JSValueConst* argv) {
        if (argc >= 1) {
            std::printf("[script] %s\n", ToUtf8(ctx, argv[0]).c_str());
            std::fflush(stdout);
        }
        return JS_UNDEFINED;
    }

    static JSValue MessageBox(JSContext* ctx, JSValueConst, int argc,
                              JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        if (!host || !host->window_ || argc < 2) return JS_UNDEFINED;
        apputil::ShowModalMessage(host->window_, host->msgboxSlot_,
                                  ToUtf8(ctx, argv[0]), ToUtf8(ctx, argv[1]),
                                  JKMessageBox::Buttons::Ok, nullptr);
        return JS_UNDEFINED;
    }

    static JSValue CreateButton(JSContext* ctx, JSValueConst, int argc,
                                JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        if (!host || !host->window_ || argc < 2) return JS_EXCEPTION;
        bool ok = false;
        const JKRect rect = RectFromArg(ctx, argv[0], &ok);
        if (!ok) return JS_EXCEPTION;
        auto* btn = new JKButton(rect, 0);
        const uint16_t id =
            ResolveControlId(host, ctx, argc >= 3 ? argv[2] : JS_UNDEFINED);
        btn->SetText(ToUtf8(ctx, argv[1]));
        btn->SetControlId(id);
        btn->SetOnClick([host, id]() { host->DispatchClick(id); });
        host->controls_.emplace_back(id, btn);
        host->window_->AddControl(std::unique_ptr<JKButton>(btn));
        return JS_NewInt32(ctx, static_cast<int32_t>(id));
    }

    static JSValue CreateLabel(JSContext* ctx, JSValueConst, int argc,
                               JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        if (!host || !host->window_ || argc < 2) return JS_EXCEPTION;
        bool ok = false;
        const JKRect rect = RectFromArg(ctx, argv[0], &ok);
        if (!ok) return JS_EXCEPTION;
        auto* label = new JKStatic(rect, 0);
        const uint16_t id =
            ResolveControlId(host, ctx, argc >= 3 ? argv[2] : JS_UNDEFINED);
        label->SetText(ToUtf8(ctx, argv[1]));
        label->SetControlId(id);
        host->controls_.emplace_back(id, label);
        host->window_->AddControl(std::unique_ptr<JKStatic>(label));
        return JS_NewInt32(ctx, static_cast<int32_t>(id));
    }

    static JSValue CreateEdit(JSContext* ctx, JSValueConst, int argc,
                              JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        if (!host || !host->window_ || argc < 2) return JS_EXCEPTION;
        bool ok = false;
        const JKRect rect = RectFromArg(ctx, argv[0], &ok);
        if (!ok) return JS_EXCEPTION;
        auto* edit = new JKEdit(rect, 0, 256, false);
        const uint16_t id =
            ResolveControlId(host, ctx, argc >= 3 ? argv[2] : JS_UNDEFINED);
        edit->SetText(ToUtf8(ctx, argv[1]));
        edit->SetControlId(id);
        host->controls_.emplace_back(id, edit);
        host->window_->AddControl(std::unique_ptr<JKEdit>(edit));
        return JS_NewInt32(ctx, static_cast<int32_t>(id));
    }

    static JSValue SetText(JSContext* ctx, JSValueConst, int argc,
                           JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        int32_t id = 0;
        if (!host || argc < 2 || JS_ToInt32(ctx, &id, argv[0]) || id < 0) {
            return JS_UNDEFINED;
        }
        if (JKControl* c = FindControl(host, static_cast<uint16_t>(id))) {
            c->SetText(ToUtf8(ctx, argv[1]));
            c->Invalidate();
        }
        return JS_UNDEFINED;
    }

    static JSValue GetText(JSContext* ctx, JSValueConst, int argc,
                           JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        int32_t id = 0;
        if (!host || argc < 1 || JS_ToInt32(ctx, &id, argv[0]) || id < 0) {
            return JS_NewString(ctx, "");
        }
        if (const JKControl* c =
                FindControl(host, static_cast<uint16_t>(id))) {
            const std::string& text = c->GetText();
            return JS_NewStringLen(ctx, text.data(), text.size());
        }
        return JS_NewString(ctx, "");
    }

    static JSValue SetInterval(JSContext* ctx, JSValueConst, int argc,
                               JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        int32_t ms = 0;
        if (!host || argc < 2 || JS_ToInt32(ctx, &ms, argv[0]) || ms <= 0 ||
            !JS_IsFunction(ctx, argv[1]) || !host->timers_.start) {
            return JS_EXCEPTION;
        }
        if (host->impl_->nextTimerId >= JKScriptHost::Impl::kScriptTimerLimit) {
            return JS_EXCEPTION;  // registry exhausted
        }
        const uint32_t id = host->impl_->nextTimerId++;
        // winId range claimed for script timers — the app routes Timer events
        // whose winId falls here back to DispatchTimerAt.
        const uint32_t winId = JKScriptHost::ScriptTimerWinIdBase + id;
        const uint64_t handle =
            host->timers_.start(winId, static_cast<uint32_t>(ms));
        JKScriptHost::Impl::TimerEntry entry;
        entry.appHandle = handle;
        entry.fn = JS_DupValue(ctx, argv[1]);  // borrowed argv -> own a ref
        host->impl_->timers.emplace_back(id, std::move(entry));
        return JS_NewInt32(ctx, static_cast<int32_t>(id));
    }

    static JSValue ClearInterval(JSContext* ctx, JSValueConst, int argc,
                                 JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        int32_t id = 0;
        if (!host || argc < 1 || JS_ToInt32(ctx, &id, argv[0]) || id < 0) {
            return JS_UNDEFINED;
        }
        EraseTimer(host, ctx, static_cast<uint32_t>(id));
        return JS_UNDEFINED;
    }
};

} // namespace script_detail

// ---------------------------------------------------------------------------
// Host lifecycle
// ---------------------------------------------------------------------------

JKScriptHost::JKScriptHost() {
    impl_ = new Impl();
}

JKScriptHost::~JKScriptHost() {
    Stop();
    delete impl_;
    impl_ = nullptr;
}

void JKScriptHost::Attach(JKWindow* window) {
    window_ = window;
}

void JKScriptHost::SetTimerServices(JKScriptTimerServices services) {
    timers_ = std::move(services);
}

bool JKScriptHost::Start(const std::string& entryPath) {
    lastError_.clear();

    if (!window_) {
        lastError_ = "JKScriptHost::Start: no window attached";
        return false;
    }
    Stop();

    rt_ = JS_NewRuntime();
    if (!rt_) {
        lastError_ = "JS_NewRuntime failed";
        return false;
    }
    ctx_ = JS_NewContext(static_cast<JSRuntime*>(rt_));
    if (!ctx_) {
        JS_FreeRuntime(static_cast<JSRuntime*>(rt_));
        rt_ = nullptr;
        lastError_ = "JS_NewContext failed";
        return false;
    }

    bool ok = false;
    std::string source;
    {
        FILE* f = nullptr;
#ifdef _WIN32
        fopen_s(&f, entryPath.c_str(), "rb");
#else
        f = std::fopen(entryPath.c_str(), "rb");
#endif
        if (!f) {
            lastError_ = "cannot open script '" + entryPath + "'";
        } else {
            std::fseek(f, 0, SEEK_END);
            const long size = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);
            if (size > 0) {
                source.resize(static_cast<size_t>(size));
                const size_t read = std::fread(source.data(), 1, source.size(), f);
                ok = (read == source.size());
                if (!ok) lastError_ = "short read on '" + entryPath + "'";
            } else {
                ok = true;  // empty script is a valid (no-op) script
            }
            std::fclose(f);
        }
    }
    if (!ok) {
        Stop();
        return false;
    }

    JSContext* ctx = static_cast<JSContext*>(ctx_);
    JS_SetContextOpaque(ctx, this);
    controls_.clear();
    nextControlId_ = 1000;

    // Host API v1 (docs/27 §4). Global functions — the .d.ts contract
    // (engine/scripts/jk.d.ts) is generated from exactly this set; the
    // self-test diffs the two (§2.4).
    using Bindings = script_detail::Bindings;
    auto bind = [ctx](const char* name, JSCFunction fn, int nargs) {
        JSValue global = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, global, name, JS_NewCFunction(ctx, fn, name, nargs));
        JS_FreeValue(ctx, global);
    };
    bind("log", Bindings::Log, 1);
    bind("messageBox", Bindings::MessageBox, 2);
    bind("createButton", Bindings::CreateButton, 3);
    bind("createLabel", Bindings::CreateLabel, 3);
    bind("createEdit", Bindings::CreateEdit, 3);
    bind("setText", Bindings::SetText, 2);
    bind("getText", Bindings::GetText, 1);
    bind("setInterval", Bindings::SetInterval, 2);
    bind("clearInterval", Bindings::ClearInterval, 1);

    // Evaluate the script (global code — the completion value is unused).
    // Failure paths set a flag and Stop() AFTER the scope: the JsValue holders
    // here own refs into the dying runtime, so they must be released before
    // JS_FreeRuntime (a live external ref trips the gc_obj_list assert).
    bool evalFailed = false;
    {
        JsValue result(ctx, JS_Eval(ctx, source.data(), source.size(),
                                    entryPath.c_str(), JS_EVAL_TYPE_GLOBAL));
        if (JS_IsException(result.value())) {
            lastError_ = DumpPendingException(ctx);
            evalFailed = true;
        }
    }
    if (evalFailed) {
        Stop();
        return false;
    }

    // onCreate() callback — optional.
    bool createFailed = false;
    {
        JsValue global(ctx, JS_GetGlobalObject(ctx));
        JsValue onCreate(ctx, JS_GetPropertyStr(ctx, global.value(), "onCreate"));
        if (JS_IsFunction(ctx, onCreate.value())) {
            JsValue call(ctx, JS_Call(ctx, onCreate.value(), JS_UNDEFINED, 0, nullptr));
            if (JS_IsException(call.value())) {
                lastError_ = DumpPendingException(ctx);
                createFailed = true;
            }
        }
    }
    if (createFailed) {
        Stop();
        return false;
    }

    entryPath_ = entryPath;
    return true;
}

void JKScriptHost::Stop() {
    if (!ctx_) return;
    JSContext* ctx = static_cast<JSContext*>(ctx_);

    // onExit() — optional.
    {
        JsValue global(ctx, JS_GetGlobalObject(ctx));
        JsValue onExit(ctx, JS_GetPropertyStr(ctx, global.value(), "onExit"));
        if (JS_IsFunction(ctx, onExit.value())) {
            JsValue call(ctx, JS_Call(ctx, onExit.value(), JS_UNDEFINED, 0, nullptr));
            if (JS_IsException(call.value())) {
                std::printf("[script] onExit error: %s\n",
                            DumpPendingException(ctx).c_str());
                std::fflush(stdout);
            }
        }
    }

    // Script timers first (they reference JS functions in the dying context).
    if (timers_.stop) {
        for (const auto& t : impl_->timers) timers_.stop(t.second.appHandle);
    }
    for (const auto& t : impl_->timers) JS_FreeValue(ctx, t.second.fn);
    impl_->timers.clear();
    controls_.clear();
    msgboxSlot_.reset();

    JS_SetContextOpaque(ctx, nullptr);
    JS_FreeContext(ctx);
    ctx_ = nullptr;
    if (rt_) {
        JS_FreeRuntime(static_cast<JSRuntime*>(rt_));
        rt_ = nullptr;
    }
    entryPath_.clear();
}

bool JKScriptHost::Reload() {
    if (entryPath_.empty()) return false;
    const std::string path = entryPath_;
    Stop();
    return Start(path);
}

void JKScriptHost::DispatchClick(uint16_t controlId) {
    if (!ctx_) return;
    JSContext* ctx = static_cast<JSContext*>(ctx_);
    JsValue global(ctx, JS_GetGlobalObject(ctx));
    JsValue onClick(ctx, JS_GetPropertyStr(ctx, global.value(), "onClick"));
    if (!JS_IsFunction(ctx, onClick.value())) return;
    JsValue arg(ctx, JS_NewInt32(ctx, static_cast<int32_t>(controlId)));
    JSValueConst argv[1] = { arg.value() };
    JsValue call(ctx, JS_Call(ctx, onClick.value(), JS_UNDEFINED, 1, argv));
    if (JS_IsException(call.value())) {
        std::printf("[script] onClick error: %s\n",
                    DumpPendingException(ctx).c_str());
        std::fflush(stdout);
    }
}

void JKScriptHost::DispatchTimerAt(uint32_t scriptTimerId) {
    if (!ctx_) return;
    JSContext* ctx = static_cast<JSContext*>(ctx_);
    for (const auto& t : impl_->timers) {
        if (t.first != scriptTimerId) continue;
        // Hold a ref across the call: the callback may clearInterval() itself,
        // which would erase the stored ref mid-call.
        JsValue fn(ctx, JS_DupValue(ctx, t.second.fn));
        JsValue call(ctx, JS_Call(ctx, fn.value(), JS_UNDEFINED, 0, nullptr));
        if (JS_IsException(call.value())) {
            std::printf("[script] timer %u error: %s\n", scriptTimerId,
                        DumpPendingException(ctx).c_str());
            std::fflush(stdout);
        }
        return;
    }
}

std::vector<std::string> JKScriptHost::BoundNames() const {
    std::vector<std::string> names;
    if (!ctx_) return names;
    JSContext* ctx = static_cast<JSContext*>(ctx_);
    // Global eval returns the completion value — a newline-joined name list.
    const char* code = "Object.getOwnPropertyNames(globalThis).join('\\n')";
    JsValue result(ctx, JS_Eval(ctx, code, std::strlen(code),
                                "<introspect>", JS_EVAL_TYPE_GLOBAL));
    if (JS_IsException(result.value())) {
        DumpPendingException(ctx);
        return names;
    }
    const std::string joined = ToUtf8(ctx, result.value());
    size_t begin = 0;
    while (begin <= joined.size()) {
        size_t end = joined.find('\n', begin);
        if (end == std::string::npos) end = joined.size();
        if (end > begin) names.push_back(joined.substr(begin, end - begin));
        begin = end + 1;
    }
    return names;
}

} // namespace jk