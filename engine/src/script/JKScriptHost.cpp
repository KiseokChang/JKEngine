#include <script/JKScriptHost.h>

#include <JKApplicationHost.h>
#include <JKButton.h>
#include <JKControl.h>
#include <JKDialog.h>
#include <JKEdit.h>
#include <JKEvent.h>
#include <JKHangulUtil.h>
#include <JKMessageBox.h>
#include <JKScriptCanvas.h>
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

std::string ToUtf8(JSContext* ctx, JSValueConst v);

// 위젯 텍스트는 KSSM 조합형(JKDC 비트맵 폰트 경로)을 기대한다 — JS 문자열은
// UTF-8이므로 위젯 경계에서 변환(docs/60: 워크숍 한글 깨짐 실측). 콘솔 log는
// UTF-8 그대로.
static std::string ToWidgetText(JSContext* ctx, JSValueConst v) {
    return jk::Utf8ToKssm(ToUtf8(ctx, v).c_str());
}

std::string ToUtf8(JSContext* ctx, JSValueConst v) {
    size_t len = 0;
    const char* s = JS_ToCStringLen(ctx, &len, v);
    if (!s) return std::string();
    std::string out(s, len);
    JS_FreeCString(ctx, s);
    return out;
}

// Binding guard failures must THROW a real exception, not bare-return
// JS_EXCEPTION: with no pending exception QuickJS surfaces garbage
// ("thrown value: [uninitialized]", 2026-09-24 receipts) and the set_script
// talk-to-fix loop feeds the agent an error it cannot act on. JS_ThrowTypeError
// sets the pending exception AND returns JS_EXCEPTION, so each site stays a
// one-line return.
JSValue ThrowTypeError(JSContext* ctx, const char* what, const char* why) {
    return JS_ThrowTypeError(ctx, "%s: %s", what, why);
}

// {x, y, w, h} object or [x, y, w, h] array -> JKRect. The array form is
// accepted because agent-written scripts naturally guess it (2026-09-24
// receipts: the model twice wrote createCanvas([10,10,W,H]) / createLabel
// ([10,10,200,20]) while the docs only show the object form). A missing
// component must FAIL, not fall back to 0: quickjs converts undefined to
// int 0 without error (JS_ToIntegerFree's JS_TAG_UNDEFINED case), so a
// malformed rect silently became a 0x0 invisible control and set_script
// reported ok:true — the talk-to-fix closed loop never fired. Returns false
// when the argument is neither shape or any component is missing / fails
// to convert.
JKRect RectFromArg(JSContext* ctx, JSValueConst v, bool* ok) {
    *ok = false;
    static const char* kKeys[4] = { "x", "y", "w", "h" };
    static const char* kIdx[4] = { "0", "1", "2", "3" };
    const bool isArr = JS_IsArray(v);
    if (!isArr && !JS_IsObject(v)) return JKRect{};
    JsValue comp[4];
    for (int i = 0; i < 4; i++) {
        comp[i] = JsValue(ctx, JS_GetPropertyStr(ctx, v, isArr ? kIdx[i]
                                                               : kKeys[i]));
        // Absent key/element (undefined) rejects — see comment above.
        if (JS_IsUndefined(comp[i].value())) return JKRect{};
    }
    int32_t n[4];
    for (int i = 0; i < 4; i++) {
        if (JS_ToInt32(ctx, &n[i], comp[i].value())) return JKRect{};
    }
    *ok = true;
    return JKRect{ n[0], n[1], n[2], n[3] };
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

    // Modal dialogs (docs/27 단계 3). The windows live here (JangoUI's proven
    // reuse model: Close() hides a dialog, Show()/Open() revives it) so a
    // script can reopen one without recreating; the JS onClose refs must be
    // released in Stop() BEFORE JS_FreeRuntime (the 단계 1 lesson).
    struct DialogEntry {
        uint32_t id = 0;
        std::unique_ptr<JKDialog> window;
        JSValue onClose{};  // owned ref
    };
    std::vector<DialogEntry> dialogs;
    uint32_t nextDialogId = 1;

    DialogEntry* FindDialog(uint32_t id) {
        for (auto& d : dialogs) {
            if (d.id == id) return &d;
        }
        return nullptr;
    }
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
        // Tree fallback (docs/27 단계 2): scenarios also target controls that
        // belong to the attached window's own subtree (a real app's UI).
        return host->window_ ? host->window_->FindControlByControlId(id) : nullptr;
    }

    // Depth-first text match. Visible labels are how scenario scripts locate
    // real-app controls — the same cells the coordinate probes used to click.
    static JKControl* FindControlByText(JKControl* node, const std::string& text) {
        if (!node) return nullptr;
        if (node->GetText() == text) return node;
        for (const auto& child : node->GetChildren()) {
            if (JKControl* hit = FindControlByText(child.get(), text)) return hit;
        }
        return nullptr;
    }

    static JKControl* FindInPanelTree(JKScriptHost* host, const std::string& text) {
        if (!host->window_) return nullptr;
        for (const auto& child : host->window_->GetChildren()) {
            if (JKControl* hit = FindControlByText(child.get(), text)) return hit;
        }
        // v3: dialog contents live in separate windows — search those too
        // (children only, so a matching dialog TITLE never resolves to the
        // window itself).
        for (const auto& d : host->impl_->dialogs) {
            if (!d.window) continue;
            for (const auto& child : d.window->GetChildren()) {
                if (JKControl* hit = FindControlByText(child.get(), text)) {
                    return hit;
                }
            }
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
                                  ToWidgetText(ctx, argv[0]),
                                  ToWidgetText(ctx, argv[1]),
                                  JKMessageBox::Buttons::Ok, nullptr);
        return JS_UNDEFINED;
    }

    static JSValue CreateButton(JSContext* ctx, JSValueConst, int argc,
                                JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        if (!host || !host->window_ || argc < 2)
            return ThrowTypeError(ctx, "createButton", "needs (rect, text)");
        bool ok = false;
        const JKRect rect = RectFromArg(ctx, argv[0], &ok);
        if (!ok) return ThrowTypeError(ctx, "createButton",
            "rect must be {x,y,w,h} or [x,y,w,h] with numbers");
        auto* btn = new JKButton(rect, 0);
        const uint16_t id =
            ResolveControlId(host, ctx, argc >= 3 ? argv[2] : JS_UNDEFINED);
        btn->SetText(ToWidgetText(ctx, argv[1]));
        btn->SetControlId(id);
        btn->SetOnClick([host, id]() { host->DispatchClick(id); });
        host->controls_.emplace_back(id, btn);
        host->window_->AddControl(std::unique_ptr<JKButton>(btn));
        return JS_NewInt32(ctx, static_cast<int32_t>(id));
    }

    static JSValue CreateLabel(JSContext* ctx, JSValueConst, int argc,
                               JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        if (!host || !host->window_ || argc < 2)
            return ThrowTypeError(ctx, "createLabel", "needs (rect, text)");
        bool ok = false;
        const JKRect rect = RectFromArg(ctx, argv[0], &ok);
        if (!ok) return ThrowTypeError(ctx, "createLabel",
            "rect must be {x,y,w,h} or [x,y,w,h] with numbers");
        auto* label = new JKStatic(rect, 0);
        const uint16_t id =
            ResolveControlId(host, ctx, argc >= 3 ? argv[2] : JS_UNDEFINED);
        label->SetText(ToWidgetText(ctx, argv[1]));
        label->SetControlId(id);
        host->controls_.emplace_back(id, label);
        host->window_->AddControl(std::unique_ptr<JKStatic>(label));
        return JS_NewInt32(ctx, static_cast<int32_t>(id));
    }

    static JSValue CreateEdit(JSContext* ctx, JSValueConst, int argc,
                              JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        if (!host || !host->window_ || argc < 2)
            return ThrowTypeError(ctx, "createEdit", "needs (rect, text)");
        bool ok = false;
        const JKRect rect = RectFromArg(ctx, argv[0], &ok);
        if (!ok) return ThrowTypeError(ctx, "createEdit",
            "rect must be {x,y,w,h} or [x,y,w,h] with numbers");
        auto* edit = new JKEdit(rect, 0, 256, false);
        const uint16_t id =
            ResolveControlId(host, ctx, argc >= 3 ? argv[2] : JS_UNDEFINED);
        edit->SetText(ToWidgetText(ctx, argv[1]));
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
            c->SetText(ToWidgetText(ctx, argv[1]));
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
            // 위젯 저장 텍스트는 KSSM(JKEdit 입력 포함) — JS에는 UTF-8로.
            const std::string text =
                jk::KssmToUtf8(c->GetText().c_str());
            return JS_NewStringLen(ctx, text.data(), text.size());
        }
        return JS_NewString(ctx, "");
    }

    static JSValue SetInterval(JSContext* ctx, JSValueConst, int argc,
                               JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        int32_t ms = 0;
        // JS contract: setInterval(fn, ms) — argv[0] is the callback, argv[1] ms.
        // Contract is setInterval(fn, ms) — the browser API order. Agent
        // scripts guess setInterval(ms, fn) often enough that the refusal
        // must name the order (2026-09-24: setInterval(16, fn) reached the
        // agent as "thrown value: [uninitialized]" and it could not recover).
        if (!host || argc < 2 || !JS_IsFunction(ctx, argv[0]) ||
            JS_ToInt32(ctx, &ms, argv[1]) || ms <= 0 || !host->timers_.start) {
            return JS_ThrowTypeError(ctx,
                "setInterval: needs (fn, ms) — callback first, delay second");
        }
        if (host->impl_->nextTimerId >= JKScriptHost::Impl::kScriptTimerLimit) {
            return JS_ThrowRangeError(ctx, "setInterval: timer registry exhausted");
        }
        const uint32_t id = host->impl_->nextTimerId++;
        // winId range claimed for script timers — the app routes Timer events
        // whose winId falls here back to DispatchTimerAt.
        const uint32_t winId = JKScriptHost::ScriptTimerWinIdBase + id;
        const uint64_t handle =
            host->timers_.start(winId, static_cast<uint32_t>(ms));
        JKScriptHost::Impl::TimerEntry entry;
        entry.appHandle = handle;
        entry.fn = JS_DupValue(ctx, argv[0]);  // borrowed argv -> own a ref
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

    // --- host API v2 — UI automation (docs/27 단계 2) ---------------------

    // findControl(idOrText): a number resolves through the tree by controlId;
    // a string resolves depth-first by visible text. Returns the controlId
    // (number) or null — the handle every other v2 binding accepts.
    static JSValue FindControlBinding(JSContext* ctx, JSValueConst, int argc,
                                      JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        if (!host || !host->window_ || argc < 1) return JS_NULL;
        if (JS_IsNumber(argv[0])) {
            int32_t id = 0;
            if (JS_ToInt32(ctx, &id, argv[0]) || id < 0 || id > 0xFFFF) {
                return JS_NULL;
            }
            return FindControl(host, static_cast<uint16_t>(id))
                       ? JS_NewInt32(ctx, id)
                       : JS_NULL;
        }
        if (JKControl* c = FindInPanelTree(host, ToUtf8(ctx, argv[0]))) {
            return JS_NewInt32(ctx, static_cast<int32_t>(c->GetControlId()));
        }
        return JS_NULL;
    }

    // Structural click: invoke the control's own OnClick — the same entry a
    // real mouse-up reaches through RespondMessage.
    static JSValue Click(JSContext* ctx, JSValueConst, int argc,
                         JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        int32_t id = 0;
        if (!host || argc < 1 || JS_ToInt32(ctx, &id, argv[0]) || id < 0 ||
            id > 0xFFFF) {
            return JS_UNDEFINED;
        }
        JKControl* c = FindControl(host, static_cast<uint16_t>(id));
        if (!c) {
            std::printf("[script] click: no control %d\n", id);
            std::fflush(stdout);
            return JS_UNDEFINED;
        }
        if (JKButton* btn = dynamic_cast<JKButton*>(c)) {
            btn->OnClick();
        } else {
            std::printf("[script] click: control %d is not a button\n", id);
            std::fflush(stdout);
        }
        return JS_UNDEFINED;
    }

    // Behavioral injection (aux): deliver real mouse/key events through the
    // window's RespondMessage routing, so hit-testing, focus, and control
    // handlers run exactly as they would for OS input. Coordinates are panel
    // client pixels — the same space createButton rects use.
    static JSValue InjectMouse(JSContext* ctx, JSValueConst, int argc,
                               JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        int32_t x = 0, y = 0;
        if (!host || !host->window_ || argc < 2 ||
            JS_ToInt32(ctx, &x, argv[0]) || JS_ToInt32(ctx, &y, argv[1])) {
            return JS_UNDEFINED;
        }
        JKEvent ev;
        ev.type = JKEventType::MouseDown;
        // HitTest works in screen space; the binding's (x, y) are panel client
        // pixels — translate through the window's screen client rect. (The
        // call goes through the public JKControl declaration; JKWindow's
        // override is protected.)
        const JKRect client =
            static_cast<JKControl*>(host->window_)->GetScreenClientRect();
        ev.x = client.x + x;
        ev.y = client.y + y;
        host->window_->RespondMessage(ev);
        ev.type = JKEventType::MouseUp;
        host->window_->RespondMessage(ev);
        return JS_UNDEFINED;
    }

    static JSValue InjectKey(JSContext* ctx, JSValueConst, int argc,
                             JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        int32_t key = 0;
        if (!host || !host->window_ || argc < 1 ||
            JS_ToInt32(ctx, &key, argv[0])) {
            return JS_UNDEFINED;
        }
        JKEvent ev;
        ev.type = JKEventType::KeyDown;
        ev.keyCode = static_cast<uint32_t>(key);
        host->window_->RespondMessage(ev);
        ev.type = JKEventType::KeyUp;
        host->window_->RespondMessage(ev);
        return JS_UNDEFINED;
    }

    // Assertion helpers — log + failure count; the `test-script` runner turns
    // the counters into the process exit code.
    static JSValue Assert(JSContext* ctx, JSValueConst, int argc,
                          JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        if (!host) return JS_UNDEFINED;
        ++host->assertChecks_;
        if (argc >= 1 && JS_ToBool(ctx, argv[0]) == 1) return JS_UNDEFINED;
        ++host->assertFailures_;
        std::printf("[script] ASSERT FAIL: %s\n",
                    (argc >= 2 ? ToUtf8(ctx, argv[1]) : std::string()).c_str());
        std::fflush(stdout);
        return JS_UNDEFINED;
    }

    // JSON.stringify when possible (distinguishes number 1 from string "1"),
    // raw ToUtf8 fallback for undefined / non-stringifiable values.
    static std::string StringifyForAssert(JSContext* ctx, JSValueConst v) {
        JsValue s(ctx, JS_JSONStringify(ctx, v, JS_UNDEFINED, JS_UNDEFINED));
        if (!JS_IsException(s.value()) && !JS_IsUndefined(s.value())) {
            return ToUtf8(ctx, s.value());
        }
        return ToUtf8(ctx, v);
    }

    static JSValue AssertEq(JSContext* ctx, JSValueConst, int argc,
                            JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        if (!host) return JS_UNDEFINED;
        ++host->assertChecks_;
        if (argc >= 2) {
            const std::string a = StringifyForAssert(ctx, argv[0]);
            const std::string b = StringifyForAssert(ctx, argv[1]);
            if (a == b) return JS_UNDEFINED;
            ++host->assertFailures_;
            std::printf("[script] ASSERT FAIL: %s (actual=%s, expected=%s)\n",
                        (argc >= 3 ? ToUtf8(ctx, argv[2]) : std::string()).c_str(),
                        a.c_str(), b.c_str());
        } else {
            ++host->assertFailures_;
            std::printf("[script] ASSERT FAIL: assertEq needs 2 arguments\n");
        }
        std::fflush(stdout);
        return JS_UNDEFINED;
    }

    // --- host API v3 — modal dialogs (docs/27 단계 3) ---------------------
    // Legacy screens are dialog-centric (JangoUI/OccUI builders): a modal
    // JKDialog window + controls + a result callback. The dialog-scoped
    // dialogAdd* functions mirror the C++ construction pattern; their controls
    // join the shared registry so findControl/click/setText/getText work on
    // them unchanged.

    static JSValue CreateDialog(JSContext* ctx, JSValueConst, int argc,
                                JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        if (!host || argc < 3 || !JS_IsFunction(ctx, argv[2])) {
            return JS_ThrowTypeError(ctx,
                "dialogCreate: needs (title, rect, onClose)");
        }
        bool ok = false;
        const JKRect rect = RectFromArg(ctx, argv[1], &ok);
        if (!ok) return JS_ThrowTypeError(ctx, "dialogCreate",
            "rect must be {x,y,w,h} or [x,y,w,h] with numbers");
        auto window = std::make_unique<JKDialog>(ToUtf8(ctx, argv[0]));
        window->SetWindowRect(rect);
        // Legacy dialogs are draggable by their title bar (JangoUI:
        // SetAttrFlags(WA_TITLEMOVEABLE) on every dialog).
        window->SetAttrFlags(WA_TITLEMOVEABLE);

        JKScriptHost::Impl::DialogEntry entry;
        entry.id = host->impl_->nextDialogId++;
        entry.window = std::move(window);
        entry.onClose = JS_DupValue(ctx, argv[2]);
        const uint32_t id = entry.id;
        entry.window->SetOnClose([host, id](int result) {
            host->DispatchDialogClose(id, result);
        });
        host->impl_->dialogs.push_back(std::move(entry));
        return JS_NewInt32(ctx, static_cast<int32_t>(id));
    }

    // Shared prologue for the dialogAdd* thunks. argv: dialogId, rect, text,
    // [id?]. Returns the new control or nullptr (bad arguments / no dialog).
    static JKControl* DialogAddControl(JKScriptHost* host, JSContext* ctx,
                                       int argc, JSValueConst* argv, int kind) {
        int32_t dialogId = 0;
        if (!host || argc < 3 || JS_ToInt32(ctx, &dialogId, argv[0]) ||
            dialogId <= 0) {
            return nullptr;
        }
        auto* entry = host->impl_->FindDialog(static_cast<uint32_t>(dialogId));
        if (!entry) return nullptr;
        bool ok = false;
        const JKRect rect = RectFromArg(ctx, argv[1], &ok);
        if (!ok) return nullptr;
        const std::string text = ToUtf8(ctx, argv[2]);
        const uint16_t id =
            ResolveControlId(host, ctx, argc >= 4 ? argv[3] : JS_UNDEFINED);
        JKControl* added = nullptr;
        switch (kind) {
            case 0: {  // label
                auto* label = new JKStatic(rect, 0);
                label->SetText(text);
                label->SetControlId(id);
                host->controls_.emplace_back(id, label);
                entry->window->AddControl(std::unique_ptr<JKStatic>(label));
                added = label;
                break;
            }
            case 1: {  // edit (same shape as createEdit: 256 chars, 1 line)
                auto* edit = new JKEdit(rect, 0, 256, false);
                edit->SetText(text);
                edit->SetControlId(id);
                host->controls_.emplace_back(id, edit);
                entry->window->AddControl(std::unique_ptr<JKEdit>(edit));
                added = edit;
                break;
            }
            default: {  // button
                auto* btn = new JKButton(rect, 0);
                btn->SetText(text);
                btn->SetControlId(id);
                btn->SetOnClick([host, id]() { host->DispatchClick(id); });
                host->controls_.emplace_back(id, btn);
                entry->window->AddControl(std::unique_ptr<JKButton>(btn));
                added = btn;
                break;
            }
        }
        return added;
    }

    static JSValue DialogAddLabel(JSContext* ctx, JSValueConst, int argc,
                                  JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        const JKControl* c = DialogAddControl(host, ctx, argc, argv, 0);
        return c ? JS_NewInt32(ctx, c->GetControlId()) : JS_EXCEPTION;
    }

    static JSValue DialogAddEdit(JSContext* ctx, JSValueConst, int argc,
                                 JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        const JKControl* c = DialogAddControl(host, ctx, argc, argv, 1);
        return c ? JS_NewInt32(ctx, c->GetControlId()) : JS_EXCEPTION;
    }

    static JSValue DialogAddButton(JSContext* ctx, JSValueConst, int argc,
                                   JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        const JKControl* c = DialogAddControl(host, ctx, argc, argv, 2);
        return c ? JS_NewInt32(ctx, c->GetControlId()) : JS_EXCEPTION;
    }

    static JSValue DialogShow(JSContext* ctx, JSValueConst, int argc,
                              JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        int32_t id = 0;
        if (!host || argc < 1 || JS_ToInt32(ctx, &id, argv[0]) || id <= 0) {
            return JS_UNDEFINED;
        }
        // JKDialog::Show saves the previously focused control and focuses the
        // dialog's first child — the modal focus restore contract.
        if (auto* entry = host->impl_->FindDialog(static_cast<uint32_t>(id))) {
            entry->window->Show();
        }
        return JS_UNDEFINED;
    }

    static JSValue DialogClose(JSContext* ctx, JSValueConst, int argc,
                               JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        int32_t id = 0, result = JKDialog::ResultCancel;
        if (!host || argc < 1 || JS_ToInt32(ctx, &id, argv[0]) || id <= 0) {
            return JS_UNDEFINED;
        }
        if (argc >= 2) JS_ToInt32(ctx, &result, argv[1]);
        if (auto* entry = host->impl_->FindDialog(static_cast<uint32_t>(id))) {
            entry->window->Close(result);  // fires onClose -> DispatchDialogClose
        }
        return JS_UNDEFINED;
    }

    // --- host API v4 — config injection (docs/27 단계 4) ------------------
    // readConfig(fileName): the script sandbox has no file access, so the
    // host reads one JSON file that sits next to the entry script and hands
    // the parsed object over (JS_ParseJSON — the runtime's own parser).
    // Absolute paths and traversal are rejected; per-key defaulting is the
    // script's job.

    static JSValue ReadConfig(JSContext* ctx, JSValueConst, int argc,
                              JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        if (!host || argc < 1 || host->entryPath_.empty()) return JS_NULL;
        const std::string name = ToUtf8(ctx, argv[0]);
        const bool rejected =
            name.empty() || name.find("..") != std::string::npos ||
            name[0] == '/' || name[0] == '\\' || name.find(':') != std::string::npos;
        if (rejected) {
            std::printf("[script] readConfig: '%s' rejected (a file name next "
                        "to app.js only)\n", name.c_str());
            std::fflush(stdout);
            return JS_NULL;
        }
        std::string dir = host->entryPath_;
        const size_t slash = dir.find_last_of("/\\");
        dir = (slash == std::string::npos) ? std::string() : dir.substr(0, slash + 1);
        const std::string path = dir + name;

        FILE* f = nullptr;
#ifdef _WIN32
        if (fopen_s(&f, path.c_str(), "rb") != 0) f = nullptr;
#else
        f = std::fopen(path.c_str(), "rb");
#endif
        if (!f) {
            std::printf("[script] readConfig: cannot open '%s'\n", path.c_str());
            std::fflush(stdout);
            return JS_NULL;
        }
        std::vector<char> buf;
        char chunk[8192];
        size_t n = 0;
        while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0) {
            buf.insert(buf.end(), chunk, chunk + n);
            if (buf.size() > (1u << 20)) break;  // 1 MiB cap — a config, not data
        }
        std::fclose(f);
        buf.push_back('\0');  // JS_ParseJSON requires buf[buf_len] == '\0'

        JsValue val(ctx, JS_ParseJSON(ctx, buf.data(), buf.size() - 1,
                                      path.c_str()));
        if (JS_IsException(val.value())) {
            std::printf("[script] readConfig: %s is not valid JSON\n", path.c_str());
            std::printf("%s\n", DumpPendingException(ctx).c_str());
            std::fflush(stdout);
            return JS_NULL;
        }
        return JS_DupValue(ctx, val.value());
    }

    // --- host API v5 — canvas + input events (docs/60 §10) ----------------
    // Retained-mode canvas control (JKScriptCanvas): draw bindings append ops,
    // OnPaintClient replays them. Input: the canvas's sink funnels events to
    // the script's onMouse/onWheel/onKey globals via the DispatchCanvas*
    // methods. Colors are 0xRRGGBB numbers or "#rrggbb"/"#rgb" strings.

    // Color arg -> (r,g,b). Accepts a number (0xRRGGBB) or a hex string.
    static bool ColorFromArg(JSContext* ctx, JSValueConst v, uint8_t out[3]) {
        if (JS_IsNumber(v)) {
            int32_t c = 0;
            if (JS_ToInt32(ctx, &c, v)) return false;
            out[0] = static_cast<uint8_t>((c >> 16) & 0xFF);
            out[1] = static_cast<uint8_t>((c >> 8) & 0xFF);
            out[2] = static_cast<uint8_t>(c & 0xFF);
            return true;
        }
        if (JS_IsString(v)) {
            std::string s = ToUtf8(ctx, v);
            if (!s.empty() && s[0] == '#') s.erase(0, 1);
            const bool short3 = (s.size() == 3);
            if (s.size() != 6 && !short3) return false;
            unsigned val = 0;
            for (char c : s) {
                int d = (c >= '0' && c <= '9') ? c - '0'
                      : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                      : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
                if (d < 0) return false;
                val = val * 16 + static_cast<unsigned>(d);
            }
            if (short3) {
                out[0] = static_cast<uint8_t>((val >> 8) & 0xF) * 17;
                out[1] = static_cast<uint8_t>((val >> 4) & 0xF) * 17;
                out[2] = static_cast<uint8_t>(val & 0xF) * 17;
            } else {
                out[0] = static_cast<uint8_t>((val >> 16) & 0xFF);
                out[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
                out[2] = static_cast<uint8_t>(val & 0xFF);
            }
            return true;
        }
        return false;
    }

    static JKScriptCanvas* CanvasOf(JKScriptHost* host, JSContext* ctx,
                                    JSValueConst idArg) {
        int32_t id = 0;
        if (!host || JS_ToInt32(ctx, &id, idArg) || id < 0 || id > 0xFFFF) {
            return nullptr;
        }
        JKControl* c = FindControl(host, static_cast<uint16_t>(id));
        return c ? dynamic_cast<JKScriptCanvas*>(c) : nullptr;
    }

    static JSValue CreateCanvas(JSContext* ctx, JSValueConst, int argc,
                                JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        if (!host || !host->window_ || argc < 1)
            return ThrowTypeError(ctx, "createCanvas", "needs (rect[, id])");
        bool ok = false;
        const JKRect rect = RectFromArg(ctx, argv[0], &ok);
        if (!ok) return ThrowTypeError(ctx, "createCanvas",
            "rect must be {x,y,w,h} or [x,y,w,h] with numbers");
        auto* canvas = new JKScriptCanvas(rect, 0);
        const uint16_t id =
            ResolveControlId(host, ctx, argc >= 2 ? argv[1] : JS_UNDEFINED);
        canvas->SetControlId(id);
        canvas->SetInputSink([host, id](const JKScriptCanvas::InputEvent& in) {
            switch (in.kind) {
                case 3:
                    host->DispatchCanvasWheel(id, in.detail, in.x, in.y);
                    break;
                case 4:
                    host->DispatchCanvasKey(static_cast<uint32_t>(in.detail),
                                            true);
                    break;
                case 5:
                    host->DispatchCanvasKey(static_cast<uint32_t>(in.detail),
                                            false);
                    break;
                default:  // 0 down, 1 up, 2 move
                    host->DispatchCanvasMouse(id, in.kind, in.x, in.y,
                                              in.button);
                    break;
            }
        });
        host->controls_.emplace_back(id, canvas);
        host->window_->AddControl(std::unique_ptr<JKScriptCanvas>(canvas));
        return JS_NewInt32(ctx, static_cast<int32_t>(id));
    }

    static JSValue CanvasClear(JSContext* ctx, JSValueConst, int argc,
                               JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        if (!host || argc < 1) return JS_UNDEFINED;
        uint8_t col[3] = { 32, 32, 32 };
        if (argc >= 2) ColorFromArg(ctx, argv[1], col);
        if (JKScriptCanvas* c = CanvasOf(host, ctx, argv[0])) {
            c->Clear(col[0], col[1], col[2]);
        }
        return JS_UNDEFINED;
    }

    static JSValue CanvasRect(JSContext* ctx, JSValueConst, int argc,
                              JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        int32_t x = 0, y = 0, w = 0, h = 0;
        if (!host || argc < 6 || JS_ToInt32(ctx, &x, argv[1]) ||
            JS_ToInt32(ctx, &y, argv[2]) || JS_ToInt32(ctx, &w, argv[3]) ||
            JS_ToInt32(ctx, &h, argv[4])) {
            return JS_UNDEFINED;
        }
        uint8_t col[3] = { 255, 255, 255 };
        if (!ColorFromArg(ctx, argv[5], col)) return JS_UNDEFINED;
        bool filled = (argc >= 7 && JS_ToBool(ctx, argv[6]) == 1);
        if (JKScriptCanvas* c = CanvasOf(host, ctx, argv[0])) {
            c->DrawRectOp(x, y, w, h, col[0], col[1], col[2], filled);
        }
        return JS_UNDEFINED;
    }

    static JSValue CanvasPixel(JSContext* ctx, JSValueConst, int argc,
                               JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        int32_t x = 0, y = 0;
        if (!host || argc < 4 || JS_ToInt32(ctx, &x, argv[1]) ||
            JS_ToInt32(ctx, &y, argv[2])) {
            return JS_UNDEFINED;
        }
        uint8_t col[3] = { 255, 255, 255 };
        if (!ColorFromArg(ctx, argv[3], col)) return JS_UNDEFINED;
        if (JKScriptCanvas* c = CanvasOf(host, ctx, argv[0])) {
            c->DrawPixel(x, y, col[0], col[1], col[2]);
        }
        return JS_UNDEFINED;
    }

    static JSValue CanvasLine(JSContext* ctx, JSValueConst, int argc,
                              JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        int32_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        if (!host || argc < 6 || JS_ToInt32(ctx, &x1, argv[1]) ||
            JS_ToInt32(ctx, &y1, argv[2]) || JS_ToInt32(ctx, &x2, argv[3]) ||
            JS_ToInt32(ctx, &y2, argv[4])) {
            return JS_UNDEFINED;
        }
        uint8_t col[3] = { 255, 255, 255 };
        if (!ColorFromArg(ctx, argv[5], col)) return JS_UNDEFINED;
        if (JKScriptCanvas* c = CanvasOf(host, ctx, argv[0])) {
            c->DrawLineOp(x1, y1, x2, y2, col[0], col[1], col[2]);
        }
        return JS_UNDEFINED;
    }

    static JSValue CanvasCircle(JSContext* ctx, JSValueConst, int argc,
                                JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        int32_t cx = 0, cy = 0, r = 0;
        if (!host || argc < 5 || JS_ToInt32(ctx, &cx, argv[1]) ||
            JS_ToInt32(ctx, &cy, argv[2]) || JS_ToInt32(ctx, &r, argv[3])) {
            return JS_UNDEFINED;
        }
        uint8_t col[3] = { 255, 255, 255 };
        if (!ColorFromArg(ctx, argv[4], col)) return JS_UNDEFINED;
        bool filled = (argc >= 6 && JS_ToBool(ctx, argv[5]) == 1);
        if (JKScriptCanvas* c = CanvasOf(host, ctx, argv[0])) {
            c->DrawCircle(cx, cy, r, col[0], col[1], col[2], filled);
        }
        return JS_UNDEFINED;
    }

    static JSValue CanvasText(JSContext* ctx, JSValueConst, int argc,
                              JSValueConst* argv) {
        JKScriptHost* host = HostOf(ctx);
        int32_t x = 0, y = 0;
        if (!host || argc < 5 || JS_ToInt32(ctx, &x, argv[1]) ||
            JS_ToInt32(ctx, &y, argv[2])) {
            return JS_UNDEFINED;
        }
        uint8_t col[3] = { 255, 255, 255 };
        if (!ColorFromArg(ctx, argv[4], col)) return JS_UNDEFINED;
        if (JKScriptCanvas* c = CanvasOf(host, ctx, argv[0])) {
            c->DrawText(x, y, ToWidgetText(ctx, argv[3]),
                        col[0], col[1], col[2]);
        }
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
    assertChecks_ = 0;
    assertFailures_ = 0;

    // Host API v1 + v2 (docs/27 §4). Global functions — the .d.ts contract
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
    bind("findControl", Bindings::FindControlBinding, 1);
    bind("click", Bindings::Click, 1);
    bind("injectMouse", Bindings::InjectMouse, 2);
    bind("injectKey", Bindings::InjectKey, 1);
    bind("assert", Bindings::Assert, 2);
    bind("assertEq", Bindings::AssertEq, 3);
    bind("createDialog", Bindings::CreateDialog, 3);
    bind("dialogAddLabel", Bindings::DialogAddLabel, 4);
    bind("dialogAddEdit", Bindings::DialogAddEdit, 4);
    bind("dialogAddButton", Bindings::DialogAddButton, 4);
    bind("dialogShow", Bindings::DialogShow, 1);
    bind("dialogClose", Bindings::DialogClose, 2);
    bind("readConfig", Bindings::ReadConfig, 1);
    bind("createCanvas", Bindings::CreateCanvas, 2);
    bind("canvasClear", Bindings::CanvasClear, 2);
    bind("canvasRect", Bindings::CanvasRect, 7);
    bind("canvasPixel", Bindings::CanvasPixel, 4);
    bind("canvasLine", Bindings::CanvasLine, 6);
    bind("canvasCircle", Bindings::CanvasCircle, 6);
    bind("canvasText", Bindings::CanvasText, 5);

    // readConfig resolves files next to the entry script — the path must be
    // known BEFORE evaluation and onCreate() run (the script may call it in
    // either). A failed Start leaves it set; Reload() retries the same path.
    entryPath_ = entryPath;

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

    // Dialogs: free the JS onClose refs (before JS_FreeRuntime) and destroy
    // the windows. The app's modal slot must not outlive a dialog window it
    // points at (hot reload / teardown with a dialog open).
    for (auto& d : impl_->dialogs) {
        if (g_jkAppHost &&
            g_jkAppHost->GetModalWindow() == d.window.get()) {
            g_jkAppHost->SetModalWindow(nullptr);
        }
        JS_FreeValue(ctx, d.onClose);
    }
    impl_->dialogs.clear();
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

void JKScriptHost::DispatchDialogClose(uint32_t dialogId, int result) {
    if (!ctx_) return;
    JSContext* ctx = static_cast<JSContext*>(ctx_);
    Impl::DialogEntry* entry = impl_->FindDialog(dialogId);
    if (!entry || !JS_IsFunction(ctx, entry->onClose)) return;
    // Hold a ref across the call: the callback may call dialogClose/dialogShow
    // (or Stop() from the app side) while we are inside JKDialog::Close.
    JsValue fn(ctx, JS_DupValue(ctx, entry->onClose));
    JsValue arg(ctx, JS_NewInt32(ctx, result));
    JSValueConst argv[1] = { arg.value() };
    JsValue call(ctx, JS_Call(ctx, fn.value(), JS_UNDEFINED, 1, argv));
    if (JS_IsException(call.value())) {
        std::printf("[script] dialog %u onClose error: %s\n", dialogId,
                    DumpPendingException(ctx).c_str());
        std::fflush(stdout);
    }
}

// --- canvas input dispatchers (docs/60 §10) ---------------------------------
// The JKScriptCanvas sink funnels events here. The script's global callbacks
// run when defined; exceptions log and the script continues (DispatchClick
// precedent). The kind string mirrors the .d.ts contract.

void JKScriptHost::DispatchCanvasMouse(uint16_t canvasId, int kind,
                                       int32_t x, int32_t y, int32_t button) {
    if (!ctx_) return;
    static const char* kKinds[] = { "down", "up", "move" };
    if (kind < 0 || kind > 2) return;
    JSContext* ctx = static_cast<JSContext*>(ctx_);
    JsValue global(ctx, JS_GetGlobalObject(ctx));
    JsValue onMouse(ctx, JS_GetPropertyStr(ctx, global.value(), "onMouse"));
    if (!JS_IsFunction(ctx, onMouse.value())) return;
    // 5th arg = SDL button (2026-09-24 폰 실전: 좌/우 구분 부재로 6턴 소모).
    // Additive — 4-arg callbacks keep working (extra args are ignored).
    JsValue argvs[5] = {
        JsValue(ctx, JS_NewString(ctx, kKinds[kind])),
        JsValue(ctx, JS_NewInt32(ctx, x)),
        JsValue(ctx, JS_NewInt32(ctx, y)),
        JsValue(ctx, JS_NewInt32(ctx, static_cast<int32_t>(canvasId))),
        JsValue(ctx, JS_NewInt32(ctx, button)),
    };
    JSValueConst argv[5] = { argvs[0].value(), argvs[1].value(),
                             argvs[2].value(), argvs[3].value(),
                             argvs[4].value() };
    JsValue call(ctx, JS_Call(ctx, onMouse.value(), JS_UNDEFINED, 5, argv));
    if (JS_IsException(call.value())) {
        std::printf("[script] onMouse error: %s\n",
                    DumpPendingException(ctx).c_str());
        std::fflush(stdout);
    }
}

void JKScriptHost::DispatchCanvasWheel(uint16_t canvasId, int32_t dy,
                                       int32_t x, int32_t y) {
    if (!ctx_) return;
    JSContext* ctx = static_cast<JSContext*>(ctx_);
    JsValue global(ctx, JS_GetGlobalObject(ctx));
    JsValue onWheel(ctx, JS_GetPropertyStr(ctx, global.value(), "onWheel"));
    if (!JS_IsFunction(ctx, onWheel.value())) return;
    JsValue argvs[3] = {
        JsValue(ctx, JS_NewInt32(ctx, dy)),
        JsValue(ctx, JS_NewInt32(ctx, x)),
        JsValue(ctx, JS_NewInt32(ctx, y)),
    };
    JSValueConst argv[3] = { argvs[0].value(), argvs[1].value(),
                             argvs[2].value() };
    JsValue call(ctx, JS_Call(ctx, onWheel.value(), JS_UNDEFINED, 3, argv));
    if (JS_IsException(call.value())) {
        std::printf("[script] onWheel error: %s\n",
                    DumpPendingException(ctx).c_str());
        std::fflush(stdout);
    }
}

void JKScriptHost::DispatchCanvasKey(uint32_t key, bool down) {
    if (!ctx_) return;
    JSContext* ctx = static_cast<JSContext*>(ctx_);
    JsValue global(ctx, JS_GetGlobalObject(ctx));
    JsValue onKey(ctx, JS_GetPropertyStr(ctx, global.value(), "onKey"));
    if (!JS_IsFunction(ctx, onKey.value())) return;
    JsValue argvs[2] = {
        JsValue(ctx, JS_NewInt32(ctx, static_cast<int32_t>(key))),
        JsValue(ctx, down ? JS_TRUE : JS_FALSE),
    };
    JSValueConst argv[2] = { argvs[0].value(), argvs[1].value() };
    JsValue call(ctx, JS_Call(ctx, onKey.value(), JS_UNDEFINED, 2, argv));
    if (JS_IsException(call.value())) {
        std::printf("[script] onKey error: %s\n",
                    DumpPendingException(ctx).c_str());
        std::fflush(stdout);
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