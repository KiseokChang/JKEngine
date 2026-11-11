// src/apps/DaySummaryPrompt.cpp — 하루 정리 친구 v1 프롬프트 조립(순수 TU).
// 원문 이동: tools/jkweb/main.cpp DaySummaryPrompt()(2026-11-07 착지) —
// 셀프테스트 무접촉이던 부품을 계약 1벌로 승격(어휘/필터 2본 금지).
#include <apps/DaySummaryPrompt.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <system_error>
#include <vector>

namespace jk {
namespace daysummary {
namespace {

// 꼬리 읽기 — 파일 전체를 읽고 마지막 maxRows행만 취한다(행당 400바이트
// 절단). 반환 false = 파일 부재/읽기 실패.
bool GrabTail(const std::string& path, std::size_t maxRows,
              std::string& tail) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::string all;
    if (sz > 0) {
        all.resize(static_cast<std::size_t>(sz));
        const std::size_t n = std::fread(&all[0], 1, all.size(), f);
        all.resize(n);
    }
    std::fclose(f);
    // 행 분해 — 마지막 개행 뒤의 꼬리도 1행으로 센다(부분 행도 원문 증거).
    std::vector<std::string> rows;
    std::size_t at = 0;
    while (at < all.size()) {
        const std::size_t nl = all.find('\n', at);
        rows.push_back(all.substr(
            at, (nl == std::string::npos ? all.size() : nl) - at));
        if (nl == std::string::npos) break;
        at = nl + 1;
    }
    if (rows.empty()) return true;   // 빈 파일 = 성공(기여 0)
    const std::size_t take = std::min(rows.size(), maxRows);
    for (std::size_t i = rows.size() - take; i < rows.size(); ++i) {
        std::string r = rows[i];
        if (r.size() > 400) r.resize(400);
        tail += r;
        tail += '\n';
    }
    return true;
}

}  // namespace

std::string TodayStamp(long long nowSec) {
    std::time_t now = static_cast<std::time_t>(nowSec);
    std::tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &now);
#else
    localtime_r(&now, &lt);
#endif
    char stamp[16];
    std::snprintf(stamp, sizeof(stamp), "%04d%02d%02d", lt.tm_year + 1900,
                  lt.tm_mon + 1, lt.tm_mday);
    return std::string(stamp);
}

std::string BuildPrompt(const std::string& stateDir, long long nowSec,
                        std::size_t maxRows) {
    const long long now = nowSec > 0 ? nowSec
                                     : static_cast<long long>(std::time(nullptr));
    const std::string today = TodayStamp(now);
    const std::string dlg = stateDir + "/dialogue";
    const std::string journal = stateDir + "/use_journal.jsonl";

    std::string tail;
    int files = 0;
    std::error_code fsErr;
    for (const auto& f :
         std::filesystem::directory_iterator(std::filesystem::path(dlg),
                                             fsErr)) {
        const std::string name = f.path().filename().string();
        // today(8자)·구분자·".jsonl" 최소 길이 — 스탬프 접두 일치만.
        if (name.rfind(today, 0) == 0 && name.size() > 14 &&
            name.compare(name.size() - 6, 6, ".jsonl") == 0) {
            if (GrabTail(f.path().string(), maxRows, tail)) ++files;
        }
    }
    if (GrabTail(journal, maxRows, tail)) ++files;
    if (files == 0 || tail.empty()) return "";

    return std::string("[하루 정리 요청] 아래는 오늘(") + today +
           ")의 대화 기록과 도구 사용 저널이다. 한국어 3부 구성 요약: "
           "1)한 일 2)아직 남은 것 3)내일 추천. 마크다운 금지(코드펜스·헤딩·"
           "굵게·백틱), 구두로 읽는다는 전제로 간결하게.\n[기록]\n" + tail;
}

}  // namespace daysummary
}  // namespace jk
