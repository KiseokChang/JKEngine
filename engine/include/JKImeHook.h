#ifndef JKIMEHOOK_H
#define JKIMEHOOK_H

#include <cstdint>

struct SDL_Window;

// 한/영 전환키 관측(docs/61 §16): OS IME가 VK_HANGUL을 삼켜 앱에 어떤 키
// 이벤트도 도달하지 않고(실측 — KEYDOWN/KEYUP 0건), 신식 MS 한글 IME는
// 구형 IMM 변환 플래그도 갱신하지 않는다(실측 — 포커스 확실한 창에서
// open=0 conv=0 고정). 남은 관측점은 저수준 키보드 훅(WH_KEYBOARD_LL)뿐 —
// IME 가로채기 전의 원시 VK를 본다.
//
// 훅은 설치 스레드의 메시지 펌프 중에 발화한다. 서버 Run 루프(SDL_PollEvent)
// 가 Windows 메시지를 계속 펌프하므로 거기서 설치한다. 토글이 관측되면 SDL
// 사용자 이벤트(JkImeToggleEventType)를 밀어 넣는다 — 메인 루프가 배출한다.
//
// watchWindow가 전경일 때만 발화한다 — 유저가 다른 앱에서 한/영을 눌러도
// JK 데스크탑 편집창이 따라 뒤집히는 오탐을 막는다.

// 설치. 이미 설치돼 있으면 no-op true. watchWindow가 nullptr면 전경 검사 없이
// 항상 보고한다.
bool JkInstallImeKeyHook(SDL_Window* watchWindow);
void JkUninstallImeKeyHook();

// 토글 관측 시 SDL_PushEvent로 밀어 넣는 사용자 이벤트 타입(미설치면 0).
uint32_t JkImeToggleEventType();

#endif // JKIMEHOOK_H