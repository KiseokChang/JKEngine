#include <agent/JKAgentJson.h>

namespace jk {
namespace agent {

AgentJson::AgentJson(const std::string& text) {
    rt_ = JS_NewRuntime();
    if (!rt_) return;
    ctx_ = JS_NewContext(rt_);
    if (!ctx_) {
        JS_FreeRuntime(rt_);
        rt_ = nullptr;
        return;
    }
    root_ = JS_ParseJSON(ctx_, text.c_str(), text.size(), "args");
    ok_ = !JS_IsException(root_);
    if (!ok_) {
        JS_FreeValue(ctx_, root_);  // the exception value
        JS_FreeValue(ctx_, JS_GetException(ctx_));
        root_ = JS_UNDEFINED;
    }
}

AgentJson::~AgentJson() {
    if (!ctx_) {
        if (rt_) JS_FreeRuntime(rt_);
        return;
    }
    JS_FreeValue(ctx_, root_);
    JS_FreeContext(ctx_);
    JS_FreeRuntime(rt_);
}

bool AgentJson::GetStr(const char* key, std::string& out) const {
    if (!ok_) return false;
    JSValue v = JS_GetPropertyStr(ctx_, root_, key);
    bool got = false;
    if (JS_IsString(v)) {
        const char* s = JS_ToCString(ctx_, v);
        if (s) {
            out = s;
            got = true;
        }
        JS_FreeCString(ctx_, s);
    }
    JS_FreeValue(ctx_, v);
    return got;
}

bool AgentJson::GetInt(const char* key, int& out) const {
    if (!ok_) return false;
    JSValue v = JS_GetPropertyStr(ctx_, root_, key);
    int64_t i = 0;
    bool got = !JS_IsException(v) && !JS_IsUndefined(v) &&
               JS_ToInt64(ctx_, &i, v) == 0;
    JS_FreeValue(ctx_, v);
    if (got) out = static_cast<int>(i);
    return got;
}

bool AgentJson::GetObjStr(const char* obj, const char* key, std::string& out) const {
    if (!ok_) return false;
    JSValue o = JS_GetPropertyStr(ctx_, root_, obj);
    bool got = false;
    if (JS_IsObject(o)) {
        JSValue v = JS_GetPropertyStr(ctx_, o, key);
        if (JS_IsString(v)) {
            const char* s = JS_ToCString(ctx_, v);
            if (s) {
                out = s;
                got = true;
            }
            JS_FreeCString(ctx_, s);
        }
        JS_FreeValue(ctx_, v);
    }
    JS_FreeValue(ctx_, o);
    return got;
}

bool AgentJson::GetObjInt(const char* obj, const char* key, int& out) const {
    if (!ok_) return false;
    JSValue o = JS_GetPropertyStr(ctx_, root_, obj);
    bool got = false;
    if (JS_IsObject(o)) {
        JSValue v = JS_GetPropertyStr(ctx_, o, key);
        int64_t i = 0;
        got = !JS_IsException(v) && !JS_IsUndefined(v) &&
              JS_ToInt64(ctx_, &i, v) == 0;
        JS_FreeValue(ctx_, v);
        if (got) out = static_cast<int>(i);
    }
    JS_FreeValue(ctx_, o);
    return got;
}

bool AgentJson::GetDeepStr(const char* a, const char* b, const char* key,
                           std::string& out) const {
    if (!ok_) return false;
    JSValue pa = JS_GetPropertyStr(ctx_, root_, a);
    bool got = false;
    if (JS_IsObject(pa)) {
        JSValue pb = JS_GetPropertyStr(ctx_, pa, b);
        if (JS_IsObject(pb)) {
            JSValue v = JS_GetPropertyStr(ctx_, pb, key);
            if (JS_IsString(v)) {
                const char* s = JS_ToCString(ctx_, v);
                if (s) {
                    out = s;
                    got = true;
                }
                JS_FreeCString(ctx_, s);
            }
            JS_FreeValue(ctx_, v);
        }
        JS_FreeValue(ctx_, pb);
    }
    JS_FreeValue(ctx_, pa);
    return got;
}

bool AgentJson::GetDeepInt(const char* a, const char* b, const char* key,
                           int& out) const {
    if (!ok_) return false;
    JSValue pa = JS_GetPropertyStr(ctx_, root_, a);
    bool got = false;
    if (JS_IsObject(pa)) {
        JSValue pb = JS_GetPropertyStr(ctx_, pa, b);
        if (JS_IsObject(pb)) {
            JSValue v = JS_GetPropertyStr(ctx_, pb, key);
            int64_t i = 0;
            got = !JS_IsException(v) && !JS_IsUndefined(v) &&
                  JS_ToInt64(ctx_, &i, v) == 0;
            JS_FreeValue(ctx_, v);
            if (got) out = static_cast<int>(i);
        }
        JS_FreeValue(ctx_, pb);
    }
    JS_FreeValue(ctx_, pa);
    return got;
}

bool AgentJson::GetArraySize(const char* key, int& out) const {
    if (!ok_) return false;
    JSValue v = JS_GetPropertyStr(ctx_, root_, key);
    bool got = false;
    if (JS_IsArray(v)) {
        int64_t len = 0;
        if (JS_GetLength(ctx_, v, &len) == 0) {
            out = static_cast<int>(len);
            got = true;
        }
    }
    JS_FreeValue(ctx_, v);
    return got;
}

bool AgentJson::GetArrStr(const char* key, int idx, const char* field,
                          std::string& out) const {
    if (!ok_) return false;
    JSValue arr = JS_GetPropertyStr(ctx_, root_, key);
    bool got = false;
    if (JS_IsArray(arr)) {
        JSValue item = JS_GetPropertyUint32(ctx_, arr, static_cast<uint32_t>(idx));
        if (JS_IsObject(item)) {
            JSValue v = JS_GetPropertyStr(ctx_, item, field);
            if (JS_IsString(v)) {
                const char* s = JS_ToCString(ctx_, v);
                if (s) {
                    out = s;
                    got = true;
                }
                JS_FreeCString(ctx_, s);
            }
            JS_FreeValue(ctx_, v);
        }
        JS_FreeValue(ctx_, item);
    }
    JS_FreeValue(ctx_, arr);
    return got;
}

bool AgentJson::GetArrInt(const char* key, int idx, const char* field, int& out) const {
    if (!ok_) return false;
    JSValue arr = JS_GetPropertyStr(ctx_, root_, key);
    bool got = false;
    if (JS_IsArray(arr)) {
        JSValue item = JS_GetPropertyUint32(ctx_, arr, static_cast<uint32_t>(idx));
        if (JS_IsObject(item)) {
            JSValue v = JS_GetPropertyStr(ctx_, item, field);
            int64_t i = 0;
            got = !JS_IsException(v) && !JS_IsUndefined(v) &&
                  JS_ToInt64(ctx_, &i, v) == 0;
            JS_FreeValue(ctx_, v);
            if (got) out = static_cast<int>(i);
        }
        JS_FreeValue(ctx_, item);
    }
    JS_FreeValue(ctx_, arr);
    return got;
}

} // namespace agent
} // namespace jk