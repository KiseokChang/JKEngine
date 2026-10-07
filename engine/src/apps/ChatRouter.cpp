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

#include <agent/JKAgentJson.h>

#include <cctype>
#include <mutex>
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

// ── 자연어 승격 배선 본체 (스펙 2026-10-08-chat-llm-promotion 설계 결정 3·4
// — T4). 라우터와 같은 파일에 두는 계약 사정: kRouteTable/kAliasTable이 이
// 파일의 static 배열 단일 진실원이고, LLM 프롬프트의 트리거 표·앱 키 표는
// 그 표를 **다시 적지 않고** 여기서 직접 조립해야 어휘 확장(표 1행 추가)이
// 승격 프롬프트에도 자동 승계된다(어휘 2본 금지 계약의 LLM 판 확장).
namespace {

std::string TrimWsp(const std::string& raw) {
    static const char* kSpace = " \t\r\n";
    const size_t b = raw.find_first_not_of(kSpace);
    if (b == std::string::npos) return "";
    return raw.substr(b, raw.find_last_not_of(kSpace) - b + 1);
}

// 행동 JSON 스키마 한 줄 — ChatAction 4종(+talk)의 LLM 표면. kind 순서는
// 라우터 열거 순서(Launch/Close/Focus/ListWindows) 승계, talk는 행동 없는
// 정보성 응답(스펙 결정 4 "정보성 응답에는 모델 텍스트를 안내문으로 회신").
//
// **무따옴표·무파이프 계약(실측 원장 — probe_promote_turn ollama leg 영수증)**:
// 본문에 문자 그대로의 " 와 | 를 금한다. ollama leg는 프롬프트가 ollama의
// 재인용 층(JKLmEngine 원장 — BuildEngineCmd "settings는 ollama leg에서
// ollama의 재인용 층을 한 번 더 통과")을 지나 claude.cmd 사슬로 도달하는데,
// 재인용이 컨텐츠 따옴표를 \" 로 다시 세우면 그 아래 cmd가 지역을 일찍 닫아
// JSON 필드값 launch|close|... 의 | 를 살아있는 파이프로 만든다(실측: cmd가
// 'close'은(는) ... 인식할 수 없습니다 — launch| 로 절단된 우측 조각이 별행
// 개통, exit 255 — claude leg 직행은 지역이 유지되어 통과). 본문에 " · | 가
// 없으면 ShellDqEscape가 무동작이고 모든 층이 인용 지역을 유지해 원문 도달.
// 모델의 **출력** JSON은 인용을 갖는 것이 맞다(파서 검증 대상 — 이 계약은
// 프롬프트 **입력** 본문만 규제한다).
std::string ActionSchemaLine() {
    return "[ 행동 JSON 양식(예시 꼴, 표준 JSON으로 필드명과 값을 큰따옴표로 "
           "감싸라) ] { action: launch / close / focus / list / talk,  app: "
           "앱 영문 키,  text: 사용자에게 보여줄 한국어 안내문 }";
}

// 승격 프롬프트 본문 — 프리앰블 불포함(엔진 계약 소관 — 헤더 주석).
//
// **단일 행 계약(실측 원장 — T4 1차 배선 프루프)**: 본문에 실개행("\n" 바이트)
// 을 넣으면 win32 leg(cmd.exe /c 접두 — JKLmEngine.cpp LlmTurnThread)에서
// cmd가 개행을 명령 분리로 먹어 claude가 아무 것도 받지 못하고(실측: stdout
// 0B·deltas 0B·sawResult=0·ok=1의 공회전 왕복 — probe_promote_turn 영수증)
// 파싱이 폴백한다. 엔진 프리앰블(kLlmTurnPreamble)이 개행 없는 단일 행인 것이
// 그 함정의 선례다 — posix(sh 이중 따옴표 안 개행 합법)는 무관하지만 양축이
// 한 조립식을 먹으므로 본문도 개행 없이 세미콜론 구분으로 맞춘다. 캔 모델의
// 프롬프트는 행구분 없이도 읽는다(트리거 표는 '·' 구분 인쇄). " · | 금지
// 계약은 위 ActionSchemaLine 원장(ollama leg 재인용 층 실측) — 본문 전체가
// 그 같은 규제를 먹는다(모든 인용 지역을 유지해 3 파서 원문 도달).
std::string ComposeChatLlmPrompt(const std::string& text) {
    std::string s =
        "[행동 JSON 요구] 아래 발화를 데스크톱 행동으로 해석해 행동 JSON "
        "한 줄만 출력한다. JSON 앞뒤로 어떤 글도 덧붙이지 않는다(마크다운 "
        "코드펜스도 금지). "
        "양식: " + ActionSchemaLine() + ". "
        "행동 뜻: launch=앱을 띄운다, close=창을 닫는다(app을 비우면 포커스 "
        "창), focus=창을 앞으로 가져온다, list=떠 있는 창 목록을 조회한다, "
        "talk=행동이 필요하지 않은 대답(text의 문장만 회신한다). ";
    // 트리거 표 — kRouteTable 순회(InfoGuide의 표 재인쇄 선례: 진실원에서
    // 뽑아 인쇄한다). 긴/짧은 형 중복은 같은 kind가 인접하므로 첫 등장만.
    std::string triggers;
    for (const RouteEntry& e : kRouteTable) {
        const std::string line = std::string(e.trigger) + "·" +
                                 KindLabel(e.kind) + "/";
        if (triggers.find(line) != std::string::npos) continue;
        triggers += line;
    }
    s += "트리거 예시(이 정확한 꼴은 이미 즉발 처리되니 참고만): " + triggers;
    // 앱 별명 표 — 한국어 별명 → 카탈로그 appName 규약의 LLM 판 재인쇄.
    std::string apps;
    for (const AliasEntry& a : kAliasTable) {
        apps += std::string(a.appName) + "(" + a.alias + ") ";
    }
    s += "앱 키(별명): " + apps;
    s += "표 밖의 앱 요청도 app에 영문 이름을 적어 launch로 보낸다 — 창 서버가 "
         "존재를 검증해 정직 회신하므로 없는 앱은 만들어 달라는 안내를 text에 "
         "쓴다. 행동 지시가 아닌 일반 질문·대화는 talk로 답한다. ";
    s += "[발화] " + text;
    return s;
}

// LLM 파싱 실패 시의 안내문 원천 — 승격 폴백 계약이 "기존 라우터 guide 원문
// 그대로"라 ChatRouteTurn이 이 함수를 재부를 일은 없다. 이 헬퍼는 파서
// 성공인데 모델이 text를 비워둔 경우(드묾)에만 지시 종류별 기본 문구를
// 만든다 — ChatRouterRoute의 지시별 문구와 같은 어조(복제 주석: 어휘 표와
// 무관한 지시 응답 문구라 표 확장에 연동되지 않는다).
std::string DefaultActionGuide(const ChatAction& a) {
    switch (a.kind) {
        case ChatAction::Launch:
            return "'" + a.app + "' 앱을 실행합니다.";
        case ChatAction::Close:
            return a.app.empty() ? std::string("포커스 창을 닫습니다.")
                                 : "'" + a.app + "' 창을 닫습니다.";
        case ChatAction::Focus:
            return "포커스 창을 앞으로 가져옵니다.";
        case ChatAction::ListWindows:
            return "떠 있는 창 목록을 불러옵니다.";
        case ChatAction::Info:
            break;
    }
    return "요청을 처리했습니다.";
}

} // namespace

bool ChatLlmEngineConfigured(const agent::ChatConfig& cfg) {
    return cfg.engine == "ollama" || cfg.engine == "claude" ||
           cfg.engine == "ollama-direct";
}

std::string ChatLlmTurnPrompt(const std::string& text) {
    return ComposeChatLlmPrompt(text);
}

bool ChatLlmActionParse(const std::string& llmText, ChatAction& out,
                        std::string& note) {
    out.kind = ChatAction::Info;  // fail-closed — 성공 시에만 지시를 채운다
    out.app.clear();
    note.clear();

    // (a) 펜스 제거 — 마크다운 코드펜스 감쌈(T1 R4 3회 실측 재현): ```(json류
    // 태그 포함)로 시작하는 행을 행 단위로 걷어낸다. 펜스 안 JSON 자체는 아래
    // (b) 절단의 수한다.
    std::string stripped;
    {
        size_t i = 0;
        for (;;) {
            const size_t nl = llmText.find('\n', i);
            std::string ln = nl == std::string::npos
                                 ? llmText.substr(i)
                                 : llmText.substr(i, nl - i);
            if (!ln.empty() && ln.back() == '\r') ln.pop_back();
            const size_t fs = ln.find_first_not_of(" \t");
            const bool fence = fs != std::string::npos &&
                               ln.compare(fs, 3, "```") == 0;
            if (!fence) { stripped += ln; stripped += '\n'; }
            if (nl == std::string::npos) break;
            i = nl + 1;
        }
    }

    // (b) 최초 `{` .. 마지막 `}` 절단 — 해설 혼입 헤지(모델이 JSON 앞뒤로
    // 사족을 단다). 괄호 쌍이 성립하지 않으면 파싱 불성립 = 정직 폴백.
    const size_t b = stripped.find('{');
    const size_t e = stripped.rfind('}');
    if (b == std::string::npos || e == std::string::npos || e <= b) {
        return false;
    }
    const std::string pre = TrimWsp(stripped.substr(0, b));
    const std::string post = TrimWsp(stripped.substr(e + 1));

    // (c) 완건 파서 검증 — 절단 스팬을 quickjs JSON 파서(AgentJson — 서버
    //   도구 디스패처와 같은 파서)로 통과시키고 action/app/키를 검증한다.
    //   모양 불량·스키마 밖 값은 false — 호출측의 정직 폴백 계약 대상.
    jk::agent::AgentJson j(stripped.substr(b, e - b + 1));
    if (!j.ok()) return false;
    std::string act;
    if (!j.GetStr("action", act)) return false;
    if (act == "launch") {
        // launch는 app 필수 — 빈 스폰 키는 launch_app 지시로 불성립
        // (라우터 1n-6 "앱어 부재=Info"와 같은 판정 — 여기선 파싱 실패).
        // out은 fail-closed 계약이므로 지시를 채우는 건 검증 통과 이후다.
        std::string app;
        if (!j.GetStr("app", app) || app.empty()) return false;
        out.kind = ChatAction::Launch;
        out.app = app;
    } else if (act == "close") {
        out.kind = ChatAction::Close;
        j.GetStr("app", out.app);   // 무app=포커스 창(라우터 1n-7 폼 그대로)
    } else if (act == "focus") {
        out.kind = ChatAction::Focus;
    } else if (act == "list") {
        out.kind = ChatAction::ListWindows;
    } else if (act == "talk") {
        out.kind = ChatAction::Info;   // 행동 없음 — text만 회신
    } else {
        return false;                  // 스키마 밖 action — 정직 폴백
    }

    // note(안내문 원료) — "text" 필드가 1차, JSON 밖 해설(pre/post)은 다른
    // 내용이면 그 뒤에 병기(스펙 결정 4 — 정보 손실 없음).
    std::string text1;
    j.GetStr("text", text1);
    note = text1;
    const bool hasText = !text1.empty();
    std::string extra;
    if (!pre.empty() && (!hasText || pre != text1)) extra = pre;
    if (!post.empty() && (!hasText || post != text1) && post != pre) {
        if (!extra.empty()) extra += "\n";
        extra += post;
    }
    if (!extra.empty()) {
        if (!note.empty()) note += "\n";
        note += extra;
    }
    return true;
}

std::string ChatRouteTurn(const std::string& text, ChatAction& out,
                          const agent::ChatConfig& cfg, bool* usedLlm) {
    if (usedLlm) *usedLlm = false;

    // (1) 정확 트리거 매치 — 기존 즉발 경로 유지(LLM 왕복 0, 지연 최소 계약).
    const std::string stubGuide = ChatRouterRoute(text, out);
    if (out.kind != ChatAction::Info || !ChatLlmEngineConfigured(cfg)) {
        return stubGuide;   // 매치 or 미구성("stub"/미지 값) — 즉발·폴백이 같은 길
    }

    // (2) 비매치 + cfg 구성 — cfg가 고른 엔진으로 동기 1턴(TurnSync — T3).
    //   프롬프트 본문만 여기서 조립하고 프리앰블 접두는 엔진 계약이 소관이다
    //   (BuildEngineCmd/BuildOllamaDirectCmd — 복제 금지).
    //   엔진은 프로세스 수명 정적 멤버 + 락: TurnSync 타임아웃 뒤에도 worker
    //   턴이 busy_ 멤버 atomic을 마주보므로 스택 인스턴스는 금물이고, jkweb의
    //   연결당 스레드는 "한 엔진 1턴" 계약을 이 락으로 직렬화한다.
    static agent::JKLlmEngine llm;
    static std::mutex llmMtx;
    agent::LlmTurnResult r;
    {
        const std::lock_guard<std::mutex> lock(llmMtx);
        if (!llm.TurnSync(ChatLlmTurnPrompt(text), r)) {
            return stubGuide;   // (3) 스폰 실패/턴 실패 — stub 안내문 원문 그대로
        }
    }
    ChatAction llmAction;
    std::string note;
    if (!ChatLlmActionParse(r.result, llmAction, note)) {
        return stubGuide;       // (3) 파싱 불가 — 같은 폴백 계약
    }
    if (usedLlm) *usedLlm = true;
    out = llmAction;
    return note.empty() ? DefaultActionGuide(llmAction) : note;
}

} // namespace jk
