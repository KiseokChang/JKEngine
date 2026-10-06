// 채팅 명령 라우터 (스펙 2026-10-07-desktop-chat-app §1.3). 어휘 표·앱 별명
// 표 둘 다 이 파일 상단 정적 배열 단일 진실원 — 채팅 앱(jkapp_chat stub
// 백엔드)·셀프테스트(케이스 1n)가 같은 표를 먹는다. 어휘 확장은 표 1행
// 추가+해당 축 셀프테스트 갱신뿐.
//
// 절단 규약(스펙 §1.3 "app=접어체 앞 어절"): 뒤접미 동사형 트리거("켜줘"
// 등)를 걷어내고 남은 앞부분에서 마지막 어절을 앱어로 삼은 뒤, 한국어 조사
// (을/를/은/는/이/가)를 떼어낸다. 어절 1개만 보는 것이 MVP — "테트리스 다시
// 켜줘"의 앱어는 "다시"(launch_app unknown_app로 서버판정 이탈 — 스펙 밖
// 확장은 룰링 몫).
//
// 정규화는 무손·순수 std: 앞뒤 ASCII 공백 제거 + ASCII 소문화만. UTF-8
// 판정은 byte 접미 비교로 안전(UTF-8은 접두·접미가 byte 정렬을 보존한다).
// ec-safe — 실패 경로가 없는 룩업이므로 error_code 대상도 없다.

#include <apps/ChatRouter.h>

#include <cctype>
#include <string>

namespace jk {

namespace {

// ── 어휘 표 (스펙 §1.3) ──────────────────────────────────────────────
// trigger = 정규화 텍스트의 뒤접미 일치(byte). 배열 순서 = 판정 순서 —
// 앞 어절을 공유하는 트리거는 긴 쪽을 먼저 둔다("앞으로 가져와"가 짧은
// "앞으로"에, "실행해줘"가 "실행"에 잡아먹히지 않게).
struct RouteEntry {
    ChatAction::Kind kind;
    const char* trigger;
};

constexpr RouteEntry kRouteTable[] = {
    {ChatAction::Close,       "닫아줘"},
    {ChatAction::Close,       "꺼줘"},
    {ChatAction::Focus,       "앞으로 가져와"},
    {ChatAction::Focus,       "앞으로"},
    {ChatAction::Focus,       "포커스"},
    {ChatAction::ListWindows, "창 목록"},
    {ChatAction::ListWindows, "창목록"},
    {ChatAction::ListWindows, "뭐 떠 있어"},
    {ChatAction::ListWindows, "뭐 떠"},
    {ChatAction::ListWindows, "뭐떠"},
    {ChatAction::Launch,      "열어줘"},
    {ChatAction::Launch,      "켜줘"},
    {ChatAction::Launch,      "실행해줘"},
    {ChatAction::Launch,      "실행"},
};

// ── 앱 별명 표 (라이브러리 appName 규약 인지) ────────────────────────
// 내장 둘의 영문 스폰 키(카탈로그 appName)가 1순위, 한국어 별명이 그 뒤.
// 표 밖 어절은 정리 후 그대로 스폰 키로 통과 — launch_app 존재 검증은
// 서버 몫(JKWindowServer jkapp_<app> 모듈 파일 판정)이므로 라우터는 임의로
// 거르지 않는다(fail-open — 실수 어절도 서버가 unknown_app로 정직 회신).
struct AliasEntry {
    const char* alias;
    const char* appName;
};

constexpr AliasEntry kAliasTable[] = {
    {"minesweeper", "minesweeper"},
    {"지뢰찾기",     "minesweeper"},
    {"tetris",      "tetris"},
    {"테트리스",     "tetris"},
};

// 앞뒤 공백(공백·탭·개행) 제거 + ASCII 소문화. 한국어는 소문화 무영향.
std::string NormalizeText(const std::string& raw) {
    std::string s = raw;
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    size_t b = 0;
    size_t e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

bool EndsWith(const std::string& s, const char* suffix) {
    const std::string suf(suffix);
    return s.size() >= suf.size() &&
           s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

// Info 안내문용 트리거 라벨 — 표와 같은 진실원에서 뽑아 인쇄한다.
const char* KindLabel(ChatAction::Kind kind) {
    switch (kind) {
        case ChatAction::Launch:      return "앱 실행";
        case ChatAction::Close:       return "창 닫기";
        case ChatAction::Focus:       return "창 앞으로";
        case ChatAction::ListWindows: return "창 목록";
        case ChatAction::Info:        break;
    }
    return "안내";
}

// 뒤접미 트리거를 걷어낸 앞부분에서 마지막 어절을 추출 → 조사 절단.
// 공백만 남으면 ""(호출측이 Info 분기).
std::string ExtractAppWord(const std::string& text, const char* trigger) {
    std::string s = text.substr(0, text.size() - std::string(trigger).size());
    size_t b = 0;
    size_t e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    if (b >= e) return "";
    // 마지막 어절 — 접어체는 무공백으로 붙어 도는 일이 많지만 어절이 갈리면
    // 가장 뒤 어절 하나가 앱어(스펙 "접어체 앞 어절").
    size_t wordB = e;
    while (wordB > b && !std::isspace(static_cast<unsigned char>(s[wordB - 1]))) --wordB;
    std::string word = s.substr(wordB, e - wordB);
    // 조사 절단 — 발화 "테트리스를 켜줘"의 「를」 같은 것. 남는 어근이 빠지면
    // 조사로 보고 뗀다(어근 통째가 조사인 극단은 앱어 보존 — 절단 금지).
    static constexpr const char* kParticles[] = {
        "을", "를", "은", "는", "이", "가",
    };
    for (const char* p : kParticles) {
        if (EndsWith(word, p)) {
            std::string stem = word.substr(0, word.size() - std::string(p).size());
            if (!stem.empty()) return stem;
        }
    }
    return word;
}

// 별명 해소 — 표 밖은 통과(fail-open, 위 표 주석). 정규화가 이미 적용된
// 어절이므로 영문 키는 대소문자 무시로 맞는다.
std::string ResolveAppName(const std::string& word) {
    for (const AliasEntry& a : kAliasTable) {
        if (word == a.alias) return a.appName;
    }
    return word;
}

std::string InfoGuide() {
    std::string s = "인식하지 못했습니다. 채팅에서 쓸 수 있는 명령:\n";
    // 표 순회 — 접두 공유로 인한 중복 트리거(긴/짧은 형)은 첫 등장만 인쇄.
    for (const RouteEntry& e : kRouteTable) {
        const std::string line = std::string("  '") + e.trigger + "' — " +
                                 KindLabel(e.kind) + "\n";
        if (s.find(line) != std::string::npos) continue;
        s += line;
    }
    s += "예: '지뢰찾기 켜줘', '창 목록'";
    return s;
}

} // namespace

std::string ChatRouterRoute(const std::string& text, ChatAction& out) {
    out.kind = ChatAction::Info;  // fail-closed — 지시는 명중 시에만 채운다
    out.app.clear();

    const std::string norm = NormalizeText(text);

    const RouteEntry* hit = nullptr;
    for (const RouteEntry& e : kRouteTable) {
        if (EndsWith(norm, e.trigger)) {
            hit = &e;
            break;  // 배열 순서 = 판정 순서(긴 트리거 우선 — 표 주석)
        }
    }
    if (!hit) {
        return InfoGuide();
    }

    switch (hit->kind) {
        case ChatAction::Launch: {
            const std::string word = ExtractAppWord(norm, hit->trigger);
            if (word.empty()) return InfoGuide();  // 앱어 부재 — 지시 불성립
            out.kind = ChatAction::Launch;
            out.app = ResolveAppName(word);
            return "'" + word + "' 앱을 실행합니다.";
        }
        case ChatAction::Close: {
            const std::string word = ExtractAppWord(norm, hit->trigger);
            out.kind = ChatAction::Close;
            if (!word.empty()) {
                out.app = ResolveAppName(word);
                return "'" + word + "' 창을 닫습니다.";
            }
            return "포커스 창을 닫습니다.";  // app="" — 서버 포커스 창 판정 위임
        }
        case ChatAction::Focus:
            out.kind = ChatAction::Focus;
            return "포커스 창을 앞으로 가져옵니다.";
        case ChatAction::ListWindows:
            out.kind = ChatAction::ListWindows;
            return "떠 있는 창 목록을 불러옵니다.";
        case ChatAction::Info:
            break;
    }
    return InfoGuide();  // 도달 없음 — Info도 지시 없음 정합
}

} // namespace jk
