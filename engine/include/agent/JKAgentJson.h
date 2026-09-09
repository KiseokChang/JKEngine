#ifndef JKAGENTJSON_H
#define JKAGENTJSON_H

#include <quickjs.h>

#include <string>

namespace jk {
namespace agent {

// Throwaway-runtime JSON field reader (the JKTerminalConfig pattern, docs/27):
// parse once, extract fields, everything frees when the object dies. Used by
// the server's agent tool dispatcher and by jkagentd for MCP request lines.
//
// Note (docs/27 lesson 3): JS_ParseJSON requires buf[buf_len] == '\0' —
// std::string::c_str() guarantees that, so pass size() as the length.
class AgentJson {
public:
    explicit AgentJson(const std::string& text);
    ~AgentJson();

    AgentJson(const AgentJson&) = delete;
    AgentJson& operator=(const AgentJson&) = delete;

    bool ok() const { return ok_; }

    // Top-level field access.
    bool GetStr(const char* key, std::string& out) const;
    bool GetInt(const char* key, int& out) const;

    // Two-level access: <obj>.<key> (e.g. "params"."name" in an MCP request).
    bool GetObjStr(const char* obj, const char* key, std::string& out) const;
    bool GetObjInt(const char* obj, const char* key, int& out) const;

private:
    JSRuntime* rt_ = nullptr;
    JSContext* ctx_ = nullptr;
    JSValue root_;
    bool ok_ = false;
};

} // namespace agent
} // namespace jk

#endif // JKAGENTJSON_H