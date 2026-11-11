// include/apps/DaySummaryPrompt.h — 하루 정리 친구 v1 회고 프롬프트 조립
// (스펙 docs/superpowers/specs/2026-11-07-day-summary-friend-design.md §2·§5).
//
// 왜 순수 TU로 분리하나: 종전엔 jkweb/main.cpp 안에 있었고 셀프테스트가
// 닿지 않았다(오늘 날짜 필터·꼬리 상한·멀티 표면 병합 = 무검증). 여기로 옮겨
// 읽기 전용 부품 계약(쓰기 0)을 고정하고 케이스로 단정한다. jkweb·셀프테스트가
// 같은 진실원을 먹는다.
//
// 계약:
//  - 읽기 전용. 파일 부재/디렉터리 부재 = 빈 문자열(호출부가 정직 안내).
//  - 오늘(로컬 날짜) 파일만: `state/dialogue/<YYYYMMDD>-*.jsonl`.
//  - 꼬리 상한 = maxRows(기본 200 — 오래된 행부터 제거, 행당 400바이트 절단).
//  - 쓰기·삭제 없음. 예외 없음(에러코드 오버로드만).
#ifndef JK_DAY_SUMMARY_PROMPT_H
#define JK_DAY_SUMMARY_PROMPT_H

#include <cstddef>
#include <string>

namespace jk {
namespace daysummary {

// stateDir 아래 dialogue/·use_journal.jsonl을 읽어 회고 프롬프트를 만든다.
// nowSec = 로컬 시각 기준 초(테스트 결정론을 위해 주입 — 0이면 현재 시각).
std::string BuildPrompt(const std::string& stateDir, long long nowSec,
                        std::size_t maxRows = 200);

// 오늘 날짜 문자열(YYYYMMDD) — 파일 필터·프롬프트 표기 공용.
std::string TodayStamp(long long nowSec);

}  // namespace daysummary
}  // namespace jk

#endif  // JK_DAY_SUMMARY_PROMPT_H
