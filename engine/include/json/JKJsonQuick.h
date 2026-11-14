// include/json/JKJsonQuick.h — "일회용 QuickJS 런타임으로 JSON 1회 파싱" 정본
// (docs/106 §2-5 잔여: QuickJS 리더 5벌).
//
// 왜: NewRuntime→NewContext→JS_ParseJSON→예외 문자열→정리 6단계가 TU마다
// 복사돼 있었고, 한 곳이라도 JS_Free*/예외 처리를 빠뜨리면 누수·조용한 실패가
// 된다. 파싱 절차만 공유하고 필드 추출은 호출자 콜백에 둔다(스키마는 TU 소관).
//
// 계약:
//  - 입력 bytes는 NUL 종료여야 한다(fs/JKFileRead.h 산출물 — len = size()-1).
//  - 반환 Ok일 때만 fn가 불린다. SyntaxError는 진단 문자열을 담는다(호출자가
//    "깨짐 = 기본값" 정책을 고를 수 있게 실패를 구분한다).
#ifndef JK_JSON_QUICK_H
#define JK_JSON_QUICK_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <quickjs.h>  // JSContext/JSValue — 전방선언 대신 실제 정의(ABI 단일)

namespace jk {
namespace json {

enum class QuickParseStatus { Ok, SyntaxError, RuntimeFail };

// 앱 매니페스트(manifest.json) 평면 문자열 4필드 — 셸·라이브러리 카탈로그가
// 같은 스키마를 각자 파싱하던 복제(3곳)의 정본. ok = 파싱 성공(문법/형 오류
// 포함 아님: 필드 부재는 빈 문자열 — 호출자가 name/cmd 필수 게이트를 건다).
struct ManifestFields {
    std::string name;
    std::string cmd;
    std::string desc;
    std::string cmdPosix;
    bool ok = false;
};

ManifestFields ParseManifest(const std::vector<uint8_t>& nulTerminated,
                             const char* label);

QuickParseStatus ParseOnce(
    const std::vector<uint8_t>& nulTerminated, const char* label,
    const std::function<void(JSContext*, JSValue)>& extract, std::string* error);

}  // namespace json
}  // namespace jk

#endif  // JK_JSON_QUICK_H
