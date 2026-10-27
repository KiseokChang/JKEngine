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

// ── 어휘 표 (스펙 §1.3 + #115b 재생·알림 도메인) ─────────────────────
// trigger = 정규화 텍스트의 뒤접미 일치(byte). 배열 순서 = 판정 순서 —
// 앞 어절을 공유하는 트리거는 긴 쪽을 먼저 둔다("앞으로 가져와"가 짧은
// "앞으로"에, "실행해줘"가 "실행"에 잡아먹히지 않게). #115b 신설 exact 행은
// 표 **끝**에 둔다 — 신설 행이 기존 앞 행의 접두 판정을 건드리지 않는
// 순서 계약(리포트 접점 표·2y-⑩ — "창 목록 보여줘"는 여전히 ListWindows
// 먼저).
struct RouteEntry {
    ChatAction::Kind kind;
    const char* trigger;
    // #115b T1 (스펙 D1 무인자 강제): exact=true 행은 정규화 발화 **전체**가
    // 트리거와 등호일 때만 성립한다(뒤접미 일치만으론 부르지 않는다). 무인자
    // 조작("멈춰줘")에 접사·인자가 붙은 발화("볼륨 50으로 멈춰줘")는 토글
    // 지시로 오해하지 않고 Info(stub 안내)로 떨어뜨린다 — 인자 필요 조작은
    // LLM 턴 소관(D1 — 신규 파서 금지 계약).
    bool exact = false;
    // #115b T1 (스펙 D3 접점 실측): AppTool 행의 app_tool 릴레이 대상 — 앱
    // 네임스페이스+도구 이름. 행은 **도구 접점이 실측으로 존재하는 조작만**
    // 세운다: 재생 토글 = vplayer play_pause(ClientVPlayerApp.cpp 등록 원문
    // — OnInit SendAgentToolRegister 6종). 접점 부재 어휘("다음 곡"·vplayer
    // 정지·"알림 뭐 왔어"류 읽기)는 행 자체를 두지 않고 stub로 배제한다
    // (D3 — 신설 도구 금지 — v2 후보·리포트 원장행).
    const char* toolApp = "";
    const char* toolName = "";
};

constexpr RouteEntry kRouteTable[] = {
    {ChatAction::Close,       "닫아줘"},
    {ChatAction::Close,       "꺼줘"},
    {ChatAction::Focus,       "앞으로 가져와"},
    {ChatAction::Focus,       "앞으로"},
    {ChatAction::Focus,       "포커스"},
    {ChatAction::ListWindows, "창 목록"},
    // #115b T1 — 신설 Launch 트리거 "보여줘"(알림 보여줘 접점 — 알림 별명
    // 1행)의 복합 발화 방어 행: 뒤접미 판정은 중간 어절을 보지 못하므로
    // "창 목록 보여줘"는 그대로 두면 기존 "창 목록" ListWindows의 뜻으로
    // 안 갈리고 Launch(앱어 "목록"→unknown_app)로 새는 원장행(2y-⑩ — 본
    // 행 2개로 봉합 — 1표 1의미 유지 — 같은 ListWindows 지시).
    {ChatAction::ListWindows, "창 목록 보여줘"},
    {ChatAction::ListWindows, "창목록 보여줘"},
    {ChatAction::ListWindows, "창목록"},
    {ChatAction::ListWindows, "뭐 떠 있어"},
    {ChatAction::ListWindows, "뭐 떠"},
    {ChatAction::ListWindows, "뭐떠"},
    {ChatAction::Launch,      "열어줘"},
    {ChatAction::Launch,      "켜줘"},
    {ChatAction::Launch,      "실행해줘"},
    {ChatAction::Launch,      "실행"},
    // #115b T1 신설 어휘 v1.5 (스펙 2026-10-27-voice-vocab-design §1·D1-D3).
    // 재생계 — 도구 접점 = vplayer play_pause(무인자 "{}")라 7행 전부
    // 같은 릴레이 대상이다. **토글 수용 원장행**: "정지(seek stop)" 류
    // 방향 고정 도구는 당초 부재(ClientVPlayerApp OnAgentToolCall —
    // open/play_pause/seek/set_volume/set_av_delay/get_status 6종)하고
    // music의 spatial_stop은 다른 앱 도메인(음역 오유 가능)이라 접점의
    // 정직 채용은 토글 한 형태 — "멈춰줘"가 멈춰 있으면 이어서 재생하고,
    // "재생해줘"가 재생 중이면 멈춘다. 안내문(AppToolGuide)에도 양방향으로
    // 인쇄한다. (v1.6 갱신 — play_stop 도구가 7종째로 신설되어 #115b
    // 원장행 D3 "vplayer 정지 접점 부재"는 해소 — 아래 신설 행.)
    {ChatAction::AppTool,     "멈춰줘",   true, "vplayer", "play_pause"},
    {ChatAction::AppTool,     "멈춰",     true, "vplayer", "play_pause"},
    {ChatAction::AppTool,     "정지해줘", true, "vplayer", "play_pause"},
    {ChatAction::AppTool,     "정지",     true, "vplayer", "play_pause"},
    {ChatAction::AppTool,     "일시정지", true, "vplayer", "play_pause"},
    {ChatAction::AppTool,     "재생해줘", true, "vplayer", "play_pause"},
    {ChatAction::AppTool,     "재생",     true, "vplayer", "play_pause"},
    // 알림 — 도구 접점 = notify **창 open**(launch_app — 기존 Launch 경로
    // 재용·앱 키 = kAliasTable 별명 1행 "알림"→notify). notify의 읽기 도구는
    // 부재(ClientNotifyApp 등록 0 — Subscribe만; publish_event는 서버 쓰기
    // 전용)라 "알림 뭐 왔어"류 읽기 발화 = D3 배제(stub — 2y-⑮ 어설션).
    // #118 T2 신설 어휘(스펙 2026-10-27-window-overlay-design D3) — 창 번호
    // 배지 켬 2행(overlay_numbers 서버 도구 — T1 원존 재용 — 새 도구 0).
    // exact 행 — "번호 보여줘" 류 접사("번호 자주 보여줘")는 Info(D1 무인자
    // 강제 정합). **Launch "보여줘" 행보다 앞 배치** — 뒤접미 판정은 배열
    // 순서로 갈리므로 "번호 보여줘"가 "보여줘"(Launch·비exact)에 눌려 앱어
    // "번호"의 launch로 새는 것을 막는다("창 목록 보여줘" 방어 행 2y-⑩ 동형
    // 순서 계약 — 2y-⑲).
    {ChatAction::OverlayNumbers, "창 번호 보여줘", true},
    {ChatAction::OverlayNumbers, "번호 보여줘",   true},
    {ChatAction::Launch,      "보여줘"},
    // v1.6 신설(vplayer play_stop 7종째 — #115b 원장행 승계): "완전히 정지"
    // = 방향 고정 멈춤(토글 아님). 표 **끝** 배치 — 신설 exact 행이 기존
    // 앞 행("정지해줘"/"정지" 등 접두 공유)의 접미 판정을 건드리지 않는 순서
    // 계약(2y-⑩ 원존). exact 게이트라 "완전히 정지해줘"는 앞 exact 행과
    // 등호가 아니라 여기 도달한다.
    {ChatAction::AppTool,     "완전히 정지해줘", true, "vplayer", "play_stop"},
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
    {"notify",      "notify"},      // #115b T1 — 알림 창 open(launch_app
                                    // 존재 검증은 서버 몫 — fail-open 원존)
    {"알림",        "notify"},
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

// ── "창 3"/"3번 창" 화법의 숫자 판정 부품 (#118 T2 — 스펙
//    2026-10-27-window-overlay-design D3) ────────────────────────────────
// 정확매치 어휘 표의 한계(brief 원존): 배지 숫자는 가변이라 표 행이 못 된다
// — 숫자 포함 발화의 즉발은 정규식성. 본 부품 = **순수·결정**(selftest 2y-⑲
// — 스폰 어셜션 0 원존): 정규화 문자열(NormalizeText 통과본 — 무손·소문화)
// 의 숫자 접미/접두 추출만 하고, 다른 판정(사이닝·상한·stub)은 하지 않는다.
// 성립 형 4종(접어체 무공백 붙음을 흡수하기 위한 2종 추가 — ExtractAppWord
// 의 "접어체는 무공백으로 붙어 도는 일이 많다" 원존):
//   "3번 창"·"3번창" (숫자 어절+「번 창」접미)  "창 3"·"창3" (「창」접두+숫자
//   접미). 반환 0 = 화법 아님·"창 0" 포함(배지는 1..N — 0은 지시 불성립 —
//   호출측이 Info stub로 떨어뜨린다 — D1 정합). 상한 99999 — 배지 스냅샷
//   범위 밖 과대 수도의 int 넘침 방지(서버 num 판정이 window_not_found로
//   정직 회신하는 축과 무관한 파서 자기 보호).
int ParseWindowNumberSpeech(const std::string& norm) {
    auto DigitsValue = [](const std::string& t) -> int {
        if (t.empty()) return 0;
        int v = 0;
        for (char c : t) {
            if (c < '0' || c > '9') return 0;
            v = v * 10 + (c - '0');
            if (v > 99999) return 0;
        }
        return v == 0 ? 0 : v;
    };
    const std::string byun_chang = "번 창";
    const std::string byunchang = "번창";
    // 형 A — 「번 창」접미: 접미 앞부분을 떼어 완전숫자 어절만 판정(조사·한글
    // 수사("창 삼") 등은 v1 미구어 — 원장행: 전사 계보의 실관측 EYES 몫).
    for (const std::string& suf : {byun_chang, byunchang}) {
        if (norm.size() > suf.size() &&
            norm.compare(norm.size() - suf.size(), suf.size(), suf) == 0) {
            std::string head = norm.substr(0, norm.size() - suf.size());
            while (!head.empty() &&
                   std::isspace(static_cast<unsigned char>(head.back()))) {
                head.pop_back();
            }
            const int v = DigitsValue(head);
            if (v > 0) return v;
        }
    }
    // 형 B — 「창」접두: 접두 뒤가 (공백 생략 가능) 전부 숫자면 성립.
    const std::string chang = "창";
    if (norm.compare(0, chang.size(), chang) == 0 &&
        norm.size() > chang.size()) {
        size_t b = chang.size();
        while (b < norm.size() &&
               std::isspace(static_cast<unsigned char>(norm[b]))) {
            ++b;
        }
        const int v = DigitsValue(norm.substr(b));
        if (v > 0) return v;
    }
    return 0;
}

// Info 안내문용 트리거 라벨 — 표와 같은 진실원에서 뽑아 인쇄한다.
const char* KindLabel(ChatAction::Kind kind) {
    switch (kind) {
        case ChatAction::Launch:      return "앱 실행";
        case ChatAction::Close:       return "창 닫기";
        case ChatAction::Focus:       return "창 앞으로";
        case ChatAction::ListWindows: return "창 목록";
        case ChatAction::AppTool:     return "재생 조작";
        case ChatAction::OverlayNumbers: return "창 번호 표지";
        case ChatAction::Info:        break;
    }
    return "안내";
}

// AppTool 즉발 안내문 — play_pause는 토글이라 방향을 고정 문장으로 못 쓴다:
// 양방향 표기(재생 중=멈춤·멈춰 있음=이어서 재생)가 정직(표 토글 원장행).
// 그 밖의 도구는 릴레이 대상을 그대로 알린다(차기 도메인 행 안내 계열).
std::string AppToolGuide(const std::string& app, const std::string& toolName) {
    if (toolName == "play_pause") {
        return "'" + app + "' 재생 토글을 보냅니다 — 재생 중이면 멈추고, "
                          "멈춰 있으면 이어서 재생합니다.";
    }
    if (toolName == "play_stop") {
        // 방향 고정 문어(토글 아님 — play_stop 계약): 항상 멈춤 방향.
        return "'" + app + "' 재생을 정지합니다(현재 위치 유지 — "
                          "재개는 '재생해줘').";
    }
    return "'" + app + "' 도구 '" + toolName + "'을(를) 보냅니다.";
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
    s += "예: '지뢰찾기 켜줘', '멈춰줘', '알림 보여줘', '창 목록', "
         "'창 번호 보여줘', '창 3'";
    return s;
}

} // namespace

std::string ChatRouterRoute(const std::string& text, ChatAction& out) {
    out.kind = ChatAction::Info;  // fail-closed — 지시는 명중 시에만 채운다
    out.app.clear();
    out.winNum = 0;   // #118 T2 — 번호 화법도 명중 시에만(fail-closed 동형)

    const std::string norm = NormalizeText(text);

    const RouteEntry* hit = nullptr;
    for (const RouteEntry& e : kRouteTable) {
        if (!EndsWith(norm, e.trigger)) continue;
        // D1 무인자 강제 — exact 행은 발화 전체 등호만 성립. 불성립은 이 행을
        // 건너뛴다(continue) — 정지 접두 공유 행("일시정지"↔"정지")도 표
        // 순서로 갈림이 유지된다.
        if (e.exact && norm != std::string(e.trigger)) continue;
        hit = &e;
        break;  // 배열 순서 = 판정 순서(긴 트리거 우선 — 표 주석)
    }
    if (!hit) {
        // #118 T2 (스펙 D3) — 숫자 화법("창 3"·"3번 창")의 즉발: 표 행이
        // 될 수 없는 가변 숫자 발화의 결정 부품(위 ParseWindowNumberSpeech
        // — 순수). 0 = 화법 아님·"창 0" — Info stub 정직 유지(D1 정합).
        const int num = ParseWindowNumberSpeech(norm);
        if (num > 0) {
            out.kind = ChatAction::Focus;
            out.winNum = num;
            return std::to_string(num) + "번 창을 앞으로 가져옵니다." +
                   " (배지 숫자 — '창 번호 보여줘'로 번호를 확인할 수 있습니다)";
        }
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
        case ChatAction::AppTool:
            out.kind = ChatAction::AppTool;
            out.app = hit->toolApp;
            out.tool = hit->toolName;
            return AppToolGuide(hit->toolApp, hit->toolName);
        case ChatAction::OverlayNumbers:
            // #118 T2 (스펙 D3) — 창 번호 배지 켬. off 어휘는 v1 생략(D3
            // 원존 — 10분 자동 소멸이 끔을 대신한다) — 문어에 그 사실을
            // 정직 표기(가짜 확인문·소멸 시점 미표기 방지).
            out.kind = ChatAction::OverlayNumbers;
            return "떠 있는 창의 제목줄에 번호를 표시합니다(10분 뒤 자동으로 "
                   "사라집니다 — '창 3'처럼 번호로 창을 부를 수 있습니다).";
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
// 재인용 층(JKLlmEngine 원장 — BuildEngineCmd "settings는 ollama leg에서
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
// 을 넣으면 win32 leg(cmd.exe /c 접두 — JKLlmEngine.cpp LlmTurnThread)에서
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
            // #118 T2 — 번호 화법의 LLM 판 폴백 문어(파서 성공+빈 text의
            // 방어 사례 — winNum은 즉발 경로만 채운다 — 스키마 4종+talk 원존).
            return a.winNum > 0
                       ? std::to_string(a.winNum) +
                             "번 창을 앞으로 가져옵니다."
                       : std::string("포커스 창을 앞으로 가져옵니다.");
        case ChatAction::ListWindows:
            return "떠 있는 창 목록을 불러옵니다.";
        case ChatAction::OverlayNumbers:
            return "떠 있는 창에 번호를 표시합니다.";
        case ChatAction::AppTool:
            return AppToolGuide(a.app, a.tool);  // LLM은 app_tool을 못 낸다(스키마
                                                 // 4종+talk) — 방어 사례만
        case ChatAction::Info:
            break;
    }
    return "요청을 처리했습니다.";
}

} // namespace

// NR4-1 (T4 fix r1) — 파일 부재=미구성: engine 값만 보면 구조 기본값
// ("ollama")의 우연 일치를 실제 구성으로 쳐주는 오류가 됐다(컨트롤러 룰링 —
// 승격은 opt-in 가법이고 chat.json이 없는 기기는 stub 폴백이 기본). fileKnown
// 표지(JKLlmEngine.h — LoadChatConfig가 engine 키 명시 시만 세운다)가
// 참여해야 문을 연다. 기존 엔진 소비자(jkbridge·jkchat·LlmTurnThread)는 이
// 게이트를 읽지 않아 무영향.
bool ChatLlmEngineConfigured(const agent::ChatConfig& cfg) {
    return cfg.fileKnown &&
           (cfg.engine == "ollama" || cfg.engine == "claude" ||
            cfg.engine == "ollama-direct");
}

std::string ChatLlmTurnPrompt(const std::string& text) {
    return ComposeChatLlmPrompt(text);
}

bool ChatLlmActionParse(const std::string& llmText, ChatAction& out,
                        std::string& note) {
    out.kind = ChatAction::Info;  // fail-closed — 성공 시에만 지시를 채운다
    out.app.clear();
    out.winNum = 0;   // #118 T2 — LLM 스키마는 4종+talk(번호 발화 = 즉발 몫
                      // — 스키마 불변 — 스펙 D4) — fail-closed 동형
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

// AppTool의 타이핑·웹 경로 정직 문언 — 헤더 계약(3 소비자 공유 1본). 전송
// 없는 표면임을 문언 자체가 말한다(죽은 "보냅니다" 확인문 봉쇄 — fix r1
// Minor 2). AppTool은 현재 재생 토글만이지만 app/tool 원문을 함께 알린다.
std::string ChatAppToolTypingGuide(const ChatAction& a) {
    return "재생 조작(" + a.app + "·" + a.tool +
           ")은 구두(음성) 표면에서만 보낼 수 있습니다 — 타이핑 표면은 재생 "
           "안내만 제공합니다";
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
