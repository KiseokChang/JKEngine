#ifndef APPS_APPUTIL_H
#define APPS_APPUTIL_H

// 원본 WINDBASE 앱 포팅(protobuf: JangoApp/Equip24/Equip/Insa)에서 공유하는
// 작은 유틸리티 모음. 헤더 온리로 제공한다.

#include <JKApplication.h>
#include <JKMessageBox.h>
#include <JKWindow.h>

#include <cstdio>
#include <ctime>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace jk {
namespace apputil {

// '|' 구분자 문자열 분할 (텍스트 데이터 파일용)
inline std::vector<std::string> Split(const std::string& s, char sep = '|') {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

// 오늘 날짜를 "YYYY-MM-DD"로 반환
inline std::string TodayString() {
    time_t t = time(nullptr);
    struct tm lt;
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                  lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
    return buf;
}

// 문자열 -> 정수 (실패 시 def)
inline int ParseInt(const std::string& s, int def = 0) {
    try {
        size_t pos = 0;
        int v = std::stoi(s, &pos);
        if (pos == 0) return def;
        return v;
    } catch (...) {
        return def;
    }
}

// 리스트박스 열 정렬용 고정 폭 패딩
inline std::string PadRight(const std::string& s, size_t width) {
    std::string r = s;
    if (r.size() < width) r.append(width - r.size(), ' ');
    return r;
}

// 모달 윈도우 위에서 띄우는 메시지 박스. 닫히면 소유 다이얼로그(owner)를
// 다시 모달로 복원해 입력이 아웃터 윈도우로 돌아가게 한다.
inline void ShowModalMessage(JKWindow* owner,
                             std::unique_ptr<JKMessageBox>& slot,
                             const std::string& title,
                             const std::string& message,
                             JKMessageBox::Buttons buttons,
                             const std::function<void(int)>& onResult) {
    slot = std::make_unique<JKMessageBox>(
        title, message, buttons,
        [owner, &slot, onResult](int result) {
            if (g_currentJKApp && g_currentJKApp->GetModalWindow() == slot.get()) {
                g_currentJKApp->SetModalWindow(owner);
            }
            if (onResult) {
                onResult(result);
            }
        });
    slot->Show();
}

} // namespace apputil
} // namespace jk

#endif // APPS_APPUTIL_H