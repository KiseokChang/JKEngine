// include/json/JKJson.h — JSON 문자열 이스케이프 단일 진실원 (docs/106 §2-5)
//
// 왜: 같은 일을 하는 복사본이 21벌 있었고(앱·서버·에이전트·스크립트), 출력이
// 서로 달랐다(`\n` 단축형 vs `\u000a`). 둘 다 JSON으로 유효하지만, 같은
// 문자열이 표면마다 다른 바이트로 기록되면 로그 비교·해시·회귀 판정이 어긋난다.
//
// 이 헤더는 **기존 네 가지 바이트 계약을 그대로 보존**하면서 구현만 하나로
// 모은다(통합이 동작 변경이 되지 않게 하는 조건 — 각 이름이 곧 계약 이름).
//
//   Escape()            새 코드 정본: " \ + \b \f \n \r \t 단축, 그 밖 제어 \u00XX
//   EscapeLegacyLog()   대화 로그 leg: \n \r \t 만 단축, \b \f 는 \uXXXX
//   EscapeStrict()      모든 제어문자를 \uXXXX (표 단축 없음)
//   EscapeRowSafe()     EscapeStrict + `{` `}` 이스케이프 (서버 행 수술 스캔)
//
// 공통: UTF-8 바이트(0x80 이상)는 그대로 통과. `'` `/` 는 건드리지 않는다.
#ifndef JK_JSON_H
#define JK_JSON_H

#include <string>

namespace jk {
namespace json {

std::string Escape(const std::string& in);
std::string EscapeLegacyLog(const std::string& in);
std::string EscapeStrict(const std::string& in);
std::string EscapeRowSafe(const std::string& in);

// 인용부호 없는 원문을 "…" 로 감싼 JSON 문자열 리터럴(Escape 정본 사용).
std::string Quote(const std::string& in);

}  // namespace json
}  // namespace jk

#endif  // JK_JSON_H
