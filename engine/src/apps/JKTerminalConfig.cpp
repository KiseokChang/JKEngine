// terminal.json reader — docs/26 단계 5 / docs/27 단계 4. JSON parsing rides
// the vendored quickjs (JS_ParseJSON): the bridge already ships the runtime,
// so the config layer adds no dependency of its own. Every key falls back
// independently — the doc's verification line is "잘못된 키는 기본값 폴백".
#include <apps/JKTerminalConfig.h>

#include <quickjs.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <cstdio>
#include <vector>

namespace jk {

namespace {

constexpr size_t kMaxConfigBytes = 1u << 20;   // 1 MiB is far beyond a config

bool ReadFileBytes(const std::string& path, std::vector<uint8_t>& out) {
#ifdef _WIN32
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0) return false;
#else
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
#endif
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0 || static_cast<size_t>(size) > kMaxConfigBytes) {
        std::fclose(f);
        return false;
    }
    out.resize(static_cast<size_t>(size) + 1);  // + NUL: JS_ParseJSON requires
    const size_t read = std::fread(out.data(), 1, static_cast<size_t>(size), f);
    std::fclose(f);
    if (read != static_cast<size_t>(size)) return false;
    out[static_cast<size_t>(size)] = '\0';
    return true;
}

// "#RRGGBB" or a plain number. Returns false when neither form matches.
bool ParseColor(JSContext* ctx, JSValueConst v, uint32_t* out) {
    if (JS_IsNumber(v)) {
        int32_t rgb = 0;
        if (JS_ToInt32(ctx, &rgb, v)) return false;
        *out = static_cast<uint32_t>(rgb) & 0xFFFFFFu;
        return true;
    }
    if (JS_IsString(v)) {
        size_t len = 0;
        const char* s = JS_ToCStringLen(ctx, &len, v);
        if (!s) return false;
        std::string hex(s, len);
        JS_FreeCString(ctx, s);
        if (!hex.empty() && hex[0] == '#') hex = hex.substr(1);
        if (hex.size() != 6) return false;
        uint32_t rgb = 0;
        for (char c : hex) {
            int d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else return false;
            rgb = (rgb << 4) | static_cast<uint32_t>(d);
        }
        *out = rgb;
        return true;
    }
    return false;
}

} // namespace

bool JKTerminalConfig::Load(const std::string& path) {
    std::vector<uint8_t> bytes;
    if (!ReadFileBytes(path, bytes)) return false;

    // Throwaway runtime — parse + field extraction, then it's gone.
    JSRuntime* rt = JS_NewRuntime();
    if (!rt) return false;
    JSContext* ctx = JS_NewContext(rt);
    if (!ctx) {
        JS_FreeRuntime(rt);
        return false;
    }

    JSValue root = JS_ParseJSON(ctx, reinterpret_cast<const char*>(bytes.data()),
                                bytes.size() - 1,  // exclude the NUL terminator
                                path.c_str());
    if (JS_IsException(root)) {
        JSValue exc = JS_GetException(ctx);
        const char* msg = JS_ToCString(ctx, exc);
        std::printf("[terminal] config: %s is not valid JSON (%s) — defaults\n",
                    path.c_str(), msg ? msg : "?");
        if (msg) JS_FreeCString(ctx, msg);
        JS_FreeValue(ctx, exc);
        JS_FreeValue(ctx, root);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        return true;
    }

    if (JS_IsObject(root)) {
        auto getString = [&](const char* key, std::string* field) {
            JSValue v = JS_GetPropertyStr(ctx, root, key);
            if (JS_IsString(v)) {
                size_t len = 0;
                const char* s = JS_ToCStringLen(ctx, &len, v);
                if (s) {
                    *field = std::string(s, len);
                    JS_FreeCString(ctx, s);
                }
            } else if (!JS_IsUndefined(v)) {
                std::printf("[terminal] config: '%s' ignored (not a string)\n", key);
            }
            JS_FreeValue(ctx, v);
        };
        auto getInt = [&](const char* key, int* field, int lo, int hi, int def) {
            JSValue v = JS_GetPropertyStr(ctx, root, key);
            if (JS_IsNumber(v)) {
                int32_t n = 0;
                if (!JS_ToInt32(ctx, &n, v) && n >= lo && n <= hi) {
                    *field = n;
                } else {
                    std::printf("[terminal] config: '%s'=%d out of range [%d..%d] — default %d\n",
                                key, n, lo, hi, def);
                }
            } else if (!JS_IsUndefined(v)) {
                std::printf("[terminal] config: '%s' ignored (not a number)\n", key);
            }
            JS_FreeValue(ctx, v);
        };
        auto getColor = [&](const char* key, uint32_t* field) {
            JSValue v = JS_GetPropertyStr(ctx, root, key);
            uint32_t rgb = 0;
            if (ParseColor(ctx, v, &rgb)) {
                *field = rgb;
            } else if (!JS_IsUndefined(v)) {
                std::printf("[terminal] config: '%s' ignored (want #RRGGBB or a number)\n", key);
            }
            JS_FreeValue(ctx, v);
        };

        getString("shell", &shell);
        getString("font", &font);
        getString("fontFallback", &fontFallback);
        getInt("scrollback", &scrollback, 0, 100000, 1000);
        getColor("themeBg", &themeBg);
        getColor("themeFg", &themeFg);
    } else {
        std::printf("[terminal] config: root is not an object — defaults\n");
    }

    // Applied-values line: the manual verification (docs/27 단계 4 검증) reads
    // this to confirm which values the terminal actually runs with.
    std::printf("[terminal] config: shell='%s' scrollback=%d\n",
                shell.c_str(), scrollback);
    std::fflush(stdout);

    JS_FreeValue(ctx, root);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return true;
}

std::string JKTerminalConfig::DefaultPath() {
    std::string dir;
#ifdef _WIN32
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
        dir = exePath;
        const size_t slash = dir.find_last_of("\\/");
        if (slash != std::string::npos) dir.resize(slash + 1);
    }
#endif
    return dir + "terminal.json";
}

} // namespace jk