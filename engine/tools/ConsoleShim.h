#ifndef JK_CONSOLE_SHIM_H
#define JK_CONSOLE_SHIM_H
// tools 공용 플랫폼 심(shim) — linux stage-3 task 8 (tools 3종, docs/69). 콘솔
// 툴 트리오(jkbridge/jktriggers/jkctl)가 windows.h에서 필요로 하던 부름이
// 겹친 것(Sleep 3 TU 전부, jkbridge의 콘솔/리다이렉트 분기 — 후자는 jkbridge
// 전용이라 main.cpp 로컬에 남는다)만 여기로 모았다: windows.h는 툴 TU에서
// 소멸하고 각 플랫폼 leg는 계약을 그대로 지킨다.
//
//  - Windows leg: 원문 API 그대로(Sleep) — 관측 동일.
//  - Posix leg: nanosleep. posix는 신규 leg라 관측 계약이 아니라 실패 없는
//    최소 대응(수면 ms 동일)만 맞춘다.
#ifdef _WIN32
#include <windows.h>
#else
#include <ctime>
#include <unistd.h>
#endif

namespace jkconsole {

// Sleep(dwMilliseconds) 그대로 — ms 수면.
inline void SleepMs(unsigned ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    const timespec ts{static_cast<time_t>(ms / 1000u),
                      static_cast<long>(ms % 1000u) * 1000000L};
    ::nanosleep(&ts, nullptr);
#endif
}

}  // namespace jkconsole

#endif  // JK_CONSOLE_SHIM_H
