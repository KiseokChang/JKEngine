// 대화 세션 기록 — 계약 전문은 include/apps/ChatSessionLog.h 주석
// (스펙 2026-11-01-chat-session-log-design D1-D4).

#include <apps/ChatSessionLog.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <random>
#include <system_error>

namespace jk {

namespace chatlog {

long long NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
        .count();
}

// 서버 JsonEsc(JKWindowServer.cpp)와 같은 원칙 — 따옴표·백슬래시·제어문자만
// 이스케이프, UTF-8 통과. 중괄호 이스케이프는 하지 않는다(행 수술 용도 없음
// — 이 파일은 append 전용이고 리더는 jksessions.sh/표준 JSON 파서).
std::string JsonEsc(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    char num[8];
    for (char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20)  { std::snprintf(num, sizeof(num), "\\u%04x", c); out += num; }
        else                out += ch;
    }
    return out;
}

// UseJournalAppend 절단 규약 승계: continuation 역행 후 남은 마지막 바이트가
// lead면(그 시퀀스의 꼬리가 잘림) 1바이트 더 물러난다.
std::string Utf8Clip(const std::string& s, size_t maxBytes) {
    if (s.size() <= maxBytes) return s;
    size_t cut = maxBytes;
    while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80)
        --cut;  // s[cut]이 continuation이면 그 글리프의 lead까지 물러남
    return s.substr(0, cut);
}

std::string SessionFileName(long long openMs, const std::string& source,
                            const std::string& id4) {
    const std::time_t t = static_cast<std::time_t>(openMs / 1000);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d%02d%02d",
                  tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour,
                  tmv.tm_min, tmv.tm_sec);
    return std::string(buf) + "_" + source + "_" + id4 + ".jsonl";
}

namespace {

std::string Str(const std::string& s) {
    return "\"" + JsonEsc(Utf8Clip(s)) + "\"";
}

std::string Head(const char* type, long long ts, int turn) {
    return std::string("{\"type\":\"") + type + "\",\"ts\":" +
           std::to_string(ts) + ",\"turn\":" + std::to_string(turn);
}

}  // namespace

std::string UserLine(long long ts, int turn, const std::string& source,
                     const std::string& text, const std::string& stt,
                     long long sttMs) {
    std::string l = Head("user", ts, turn) + ",\"source\":" + Str(source);
    if (!stt.empty()) l += ",\"stt\":" + Str(stt);
    l += ",\"text\":" + Str(text);
    if (sttMs >= 0) l += ",\"stt_ms\":" + std::to_string(sttMs);
    return l + "}\n";
}

std::string RouteLine(long long ts, int turn, const std::string& path,
                      const std::string& matched, const std::string& kind,
                      const std::string& app, long long ms) {
    return Head("route", ts, turn) + ",\"path\":" + Str(path) +
           ",\"matched\":" + Str(matched) + ",\"kind\":" + Str(kind) +
           ",\"app\":" + Str(app) + ",\"ms\":" + std::to_string(ms) + "}\n";
}

std::string ActionLine(long long ts, int turn, const std::string& tool,
                       const std::string& args, bool ok,
                       const std::string& err, long long ms) {
    return Head("action", ts, turn) + ",\"tool\":" + Str(tool) +
           ",\"args\":" + Str(args) + ",\"ok\":" + (ok ? "true" : "false") +
           ",\"err\":" + Str(err) + ",\"ms\":" + std::to_string(ms) + "}\n";
}

std::string ReplyLine(long long ts, int turn, const std::string& text,
                      long long totalMs) {
    return Head("reply", ts, turn) + ",\"text\":" + Str(text) +
           ",\"total_ms\":" + std::to_string(totalMs) + "}\n";
}

}  // namespace chatlog

namespace {

long long Now(long long nowMs) { return nowMs < 0 ? chatlog::NowMs() : nowMs; }

std::string MakeId4() {
    static std::atomic<unsigned> counter{0};
    static const unsigned seed = [] {
        std::random_device rd;
        return static_cast<unsigned>(rd()) ^
               static_cast<unsigned>(chatlog::NowMs());
    }();
    unsigned v = seed + 0x9E3779B9u * (counter.fetch_add(1) + 1);
    v ^= v >> 16;
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%04x", v & 0xFFFFu);
    return buf;
}

}  // namespace

ChatSessionLog::ChatSessionLog(std::string stateDir, std::string source,
                               std::string sessionKey, long long openMs)
    : source_(std::move(source)), key_(std::move(sessionKey)) {
    while (!stateDir.empty() &&
           (stateDir.back() == '/' || stateDir.back() == '\\'))
        stateDir.pop_back();
    dir_ = (stateDir.empty() ? std::string(".") : stateDir) + "/sessions";
    lastMs_ = Now(openMs);
    // 같은 초·같은 source의 기존 파일과 겹치면 id4 재추첨(최대 8회 — 그래도
    // 겹치면 이어쓰기: 기록은 부가 기능, D4).
    std::error_code ec;
    for (int i = 0; i < 8; ++i) {
        path_ = dir_ + "/" +
                chatlog::SessionFileName(lastMs_, source_, MakeId4());
        if (!std::filesystem::exists(std::filesystem::path(path_), ec)) break;
    }
}

int ChatSessionLog::TurnCount() const {
    std::lock_guard<std::mutex> lock(mu_);
    return turn_;
}

long long ChatSessionLog::LastActivityMs() const {
    std::lock_guard<std::mutex> lock(mu_);
    return lastMs_;
}

void ChatSessionLog::Append(const std::string& line, long long ts) {
    lastMs_ = ts;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(dir_), ec);
    std::FILE* f = std::fopen(path_.c_str(), "ab");
    if (!f) return;  // D4 — 조용한 스킵
    const size_t wrote = std::fwrite(line.data(), 1, line.size(), f);
    (void)wrote;     // 부분 쓰기도 D4
    std::fflush(f);
    std::fclose(f);
}

int ChatSessionLog::BeginTurn(const std::string& text, const std::string& stt,
                              long long sttMs, long long nowMs) {
    const long long ts = Now(nowMs);
    std::lock_guard<std::mutex> lock(mu_);
    const int turn = ++turn_;
    Append(chatlog::UserLine(ts, turn, source_, text, stt, sttMs), ts);
    return turn;
}

void ChatSessionLog::Route(int turn, const std::string& path,
                           const std::string& matched, const std::string& kind,
                           const std::string& app, long long ms,
                           long long nowMs) {
    const long long ts = Now(nowMs);
    std::lock_guard<std::mutex> lock(mu_);
    Append(chatlog::RouteLine(ts, turn, path, matched, kind, app, ms), ts);
}

void ChatSessionLog::Action(int turn, const std::string& tool,
                            const std::string& args, bool ok,
                            const std::string& err, long long ms,
                            long long nowMs) {
    const long long ts = Now(nowMs);
    std::lock_guard<std::mutex> lock(mu_);
    Append(chatlog::ActionLine(ts, turn, tool, args, ok, err, ms), ts);
}

void ChatSessionLog::Reply(int turn, const std::string& text, long long totalMs,
                           long long nowMs) {
    const long long ts = Now(nowMs);
    std::lock_guard<std::mutex> lock(mu_);
    Append(chatlog::ReplyLine(ts, turn, text, totalMs), ts);
}

ChatSessionRegistry::ChatSessionRegistry(std::string stateDir, long long idleMs)
    : stateDir_(std::move(stateDir)), idleMs_(idleMs) {}

std::shared_ptr<ChatSessionLog> ChatSessionRegistry::Get(
    const std::string& key, const std::string& keyedSource, long long nowMs) {
    const long long now = Now(nowMs);
    std::lock_guard<std::mutex> lock(mu_);
    if (key.empty()) {
        if (!web_ || now - web_->LastActivityMs() >= idleMs_)
            web_ = std::make_shared<ChatSessionLog>(stateDir_, "web", "", now);
        return web_;
    }
    auto it = keyed_.find(key);
    if (it != keyed_.end()) return it->second;
    if (keyed_.size() >= kMaxKeyed) {
        auto oldest = keyed_.begin();
        for (auto j = keyed_.begin(); j != keyed_.end(); ++j)
            if (j->second->LastActivityMs() < oldest->second->LastActivityMs())
                oldest = j;
        keyed_.erase(oldest);
    }
    auto log = std::make_shared<ChatSessionLog>(stateDir_, keyedSource, key, now);
    keyed_.emplace(key, log);
    return log;
}

}  // namespace jk
