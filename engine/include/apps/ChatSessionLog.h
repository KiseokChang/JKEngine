#ifndef JK_CHATSESSIONLOG_H
#define JK_CHATSESSIONLOG_H

// 대화 세션 기록 (스펙 2026-11-01-chat-session-log-design D1-D4 — 구현 단위 1).
// 말로 하는 대화의 한 턴 전체(user → route → action → reply)를 세션 단위
// JSONL로 남긴다. 창·imgui 무접촉 pure TU(jkcore) — jkweb·jkdesktop 공용.
//
// 파일 (D1): <stateDir>/sessions/<YYYYMMDD-HHMMSS>_<source>_<id4>.jsonl
//   - YYYYMMDD-HHMMSS = 세션 열린 시각(로컬 시간), id4 = 소문자 hex 4자.
//   - stateDir = 서버 use_journal과 같은 <exeDir>/state 규약. 서버의
//     JKWindowServer::StateDir()는 멤버라 공용 함수가 없다 → **호출자가
//     넘긴다**(jkweb/jkdesktop 모두 exe 디렉터리 + "/state"를 만들면 된다).
//   - sessions/ 디렉터리는 첫 쓰기 때 만든다(create_directories, 무예외).
//
// 줄 형식 (D3 — 한 줄 한 사건, turn으로 묶음, 필드 순서 고정):
//   {"type":"user","ts":MS,"turn":N,"source":"jkapk","stt":"system","text":"…","stt_ms":MS}
//   {"type":"route","ts":MS,"turn":N,"path":"exact|fuzzy|ai|none","matched":"…","kind":"Launch","app":"…","ms":MS}
//   {"type":"action","ts":MS,"turn":N,"tool":"launch_app","args":"…","ok":true,"err":"","ms":MS}
//   {"type":"reply","ts":MS,"turn":N,"text":"…","total_ms":MS}
//   - user 줄의 stt는 빈 문자열이면 생략, stt_ms는 음수면 생략(D3 "없으면 생략").
//   - 문자열 필드는 UTF-8 안전 절단 512바이트 상한(소스 시점 절단 → 이스케이프).
//   - 이스케이프 = " \ 와 제어문자(\n \r \t 는 짧은 형, 나머지 \u00XX).
//     UTF-8(한글)은 그대로 통과.
//
// 쓰기 실패 = 조용한 스킵 (D4): 모든 기록 함수는 void이고 예외를 던지지
// 않는다 — 대화 처리에 영향 0.
//
// 스레드 안전: ChatSessionLog와 ChatSessionRegistry 모두 내부 뮤텍스
// (jkweb 연결당 스레드가 같은 세션을 공유할 수 있다).
//
// 시간 주입: nowMs 인자가 음수(기본 -1)면 실제 시계(epoch ms). selftest는
// 고정값을 넣어 결정론으로 단정한다.

#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace jk {

namespace chatlog {

constexpr size_t kFieldMaxBytes = 512;
constexpr long long kIdleNewSessionMs = 30LL * 60 * 1000;  // D2: 30분

// 현재 시각 epoch ms.
long long NowMs();

// JSON 문자열 이스케이프(따옴표 없이 내용만). UTF-8 바이트 통과.
std::string JsonEsc(const std::string& s);

// UTF-8 안전 절단: 결과 ≤ maxBytes, 다중 바이트 시퀀스를 중간에서 자르지
// 않는다(잘리는 글리프는 통째로 버림). maxBytes 이하 입력은 그대로.
std::string Utf8Clip(const std::string& s, size_t maxBytes = kFieldMaxBytes);

// "<YYYYMMDD-HHMMSS>_<source>_<id4>.jsonl" (로컬 시간).
std::string SessionFileName(long long openMs, const std::string& source,
                            const std::string& id4);

// 줄 조립(개행 포함). 위 D3 형식 그대로 — 순수 함수(selftest 단정 대상).
std::string UserLine(long long ts, int turn, const std::string& source,
                     const std::string& text, const std::string& stt,
                     long long sttMs);
std::string RouteLine(long long ts, int turn, const std::string& path,
                      const std::string& matched, const std::string& kind,
                      const std::string& app, long long ms);
std::string ActionLine(long long ts, int turn, const std::string& tool,
                       const std::string& args, bool ok,
                       const std::string& err, long long ms);
std::string ReplyLine(long long ts, int turn, const std::string& text,
                      long long totalMs);

}  // namespace chatlog

// 세션 하나 = 파일 하나. 생성자는 파일명만 정하고(디스크 무접촉), 첫 기록
// 때 디렉터리·파일을 만든다(빈 세션은 파일을 남기지 않는다).
class ChatSessionLog {
public:
    // source: "jkapk" | "chat" | "web" | "talk" (D1).
    // sessionKey: 호출자 식별용(JK 앱 세션 id 등) — 파일명에는 안 들어간다.
    // openMs: 세션 열린 시각(파일명) — 음수면 지금.
    ChatSessionLog(std::string stateDir, std::string source,
                   std::string sessionKey = "", long long openMs = -1);

    // user 줄을 쓰고 새 턴 번호(1부터 증가)를 돌려준다. stt = "system" |
    // "whisper" | "typed" | ""(생략), sttMs < 0 = 생략.
    int BeginTurn(const std::string& text, const std::string& stt = "",
                  long long sttMs = -1, long long nowMs = -1);
    // path = "exact" | "fuzzy" | "ai" | "none". kind = ChatAction::Kind 이름.
    void Route(int turn, const std::string& path, const std::string& matched,
               const std::string& kind, const std::string& app, long long ms,
               long long nowMs = -1);
    // args = 도구 인자 원문(보통 JSON 문자열 — 문자열 필드로 이스케이프).
    void Action(int turn, const std::string& tool, const std::string& args,
                bool ok, const std::string& err, long long ms,
                long long nowMs = -1);
    void Reply(int turn, const std::string& text, long long totalMs,
               long long nowMs = -1);

    const std::string& Path() const { return path_; }
    const std::string& Source() const { return source_; }
    const std::string& Key() const { return key_; }
    int TurnCount() const;
    long long LastActivityMs() const;  // 마지막 기록(또는 열린) 시각

private:
    void Append(const std::string& line, long long ts);  // 락 보유 상태에서 호출

    mutable std::mutex mu_;
    std::string dir_;     // <stateDir>/sessions
    std::string path_;    // dir_/<파일명>
    std::string source_;
    std::string key_;
    int turn_ = 0;
    long long lastMs_ = 0;
};

// 키별 열린 세션 관리(jkweb용 — D2).
//   - key 비어 있음 = source "web" 단일 세션. 마지막 기록 뒤 idleMs(기본
//     30분) 이상 무활동이면 다음 Get에서 새 세션(새 파일).
//   - key 있음 = 그 key(JK 앱 프로세스 세션 id)의 세션, source는
//     keyedSource(기본 "jkapk"). 앱 재시작 = 새 key = 새 세션이므로 무활동
//     경계를 적용하지 않는다. 맵은 kMaxKeyed 초과 시 가장 오래 쉰 key부터
//     축출(메모리 상한 — 축출된 key가 다시 오면 새 파일).
class ChatSessionRegistry {
public:
    static constexpr size_t kMaxKeyed = 64;

    explicit ChatSessionRegistry(std::string stateDir,
                                 long long idleMs = chatlog::kIdleNewSessionMs);

    // 돌려준 포인터는 계속 유효(shared_ptr) — 턴 하나를 쓰는 동안 들고 있으면
    // 된다. nowMs < 0 = 지금.
    std::shared_ptr<ChatSessionLog> Get(const std::string& key,
                                        const std::string& keyedSource = "jkapk",
                                        long long nowMs = -1);

private:
    std::mutex mu_;
    std::string stateDir_;
    long long idleMs_;
    std::shared_ptr<ChatSessionLog> web_;
    std::map<std::string, std::shared_ptr<ChatSessionLog>> keyed_;
};

}  // namespace jk

#endif  // JK_CHATSESSIONLOG_H
