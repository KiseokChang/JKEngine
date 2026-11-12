// src/json/JKJsonQuick.cpp — 일회용 QuickJS 런타임 파싱 정본(docs/106 §2-5).
#include <json/JKJsonQuick.h>

#include <quickjs.h>

namespace jk {
namespace json {

QuickParseStatus ParseOnce(
    const std::vector<uint8_t>& nulTerminated, const char* label,
    const std::function<void(JSContext*, JSValue)>& extract,
    std::string* error) {
    if (nulTerminated.empty() || nulTerminated.back() != '\0') {
        if (error) *error = "input not NUL-terminated";
        return QuickParseStatus::RuntimeFail;
    }
    JSRuntime* rt = JS_NewRuntime();
    if (!rt) {
        if (error) *error = "JS_NewRuntime failed";
        return QuickParseStatus::RuntimeFail;
    }
    JSContext* ctx = JS_NewContext(rt);
    if (!ctx) {
        JS_FreeRuntime(rt);
        if (error) *error = "JS_NewContext failed";
        return QuickParseStatus::RuntimeFail;
    }
    JSValue root = JS_ParseJSON(
        ctx, reinterpret_cast<const char*>(nulTerminated.data()),
        nulTerminated.size() - 1, label ? label : "json");
    if (JS_IsException(root)) {
        JSValue exc = JS_GetException(ctx);
        const char* msg = JS_ToCString(ctx, exc);
        if (error) *error = msg ? msg : "syntax error";
        if (msg) JS_FreeCString(ctx, msg);
        JS_FreeValue(ctx, exc);
        JS_FreeValue(ctx, root);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        return QuickParseStatus::SyntaxError;
    }
    if (extract) extract(ctx, root);
    JS_FreeValue(ctx, root);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return QuickParseStatus::Ok;
}

}  // namespace json
}  // namespace jk
