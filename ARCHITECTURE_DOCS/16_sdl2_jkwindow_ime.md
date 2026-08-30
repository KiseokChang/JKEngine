# SDL2 JKWindow 한글 IME 입력 아키텍처

> `prototype/sdl2_jkwindow`의 `JKEdit` 한글 입력 설계.  
> OS 기본 IME(Windows 한국어 입력기)를 우선 사용하고, 내부 2벌식 한글 오토마타는 `F2` 폴백으로 유지한다.  
> 2026-08-30 `JKEdit` IME 충돌 문제를 해결하며 확정된 개념·구현·고려사항을 반영한다.

## TL;DR

- **증상**: `JKEdit`에서 Windows 한국어 IME가 켜진 상태로 타이핑하면, OS IME와 `JKEdit`의 내장 2벌식 오토마타가 동시에 조합해 글자가 깨지거나 중복 입력된다.
- **원인**: SDL2 `SDL_TEXTINPUT`/`SDL_TEXTEDITING` 이벤트가 전혀 처리되지 않거나(기존에는 `SDL_TEXTEDITING`을 버림), ASCII 글자도 내장 오토마타가 가로채는 등 라우팅이 한 개의 진실 공급원(single source of truth)을 갖지 않는다.
- **해결**:
  1. OS IME가 주는 이벤트(`SDL_TEXTEDITING` 조합, `SDL_TEXTINPUT` 확정)를 받아서 KSSM 2바이트로 변환해 저장.
  2. 입력 모드를 `Ascii` / `InternalHangul` / `ImeHangul` 3가지로 분리하고, 모드별로 이벤트 소비 경계를 명확히 함.
  3. `JKPlatform` PAL로 Win32 IMM32 API를 격리해 `JKEdit.cpp`에서 `<windows.h>` 오염을 제거.
  4. 조합 중에는 백스페이스/딜리트/방향키/엔터를 프레임워크가 처리하지 않고 OS IME에 양보.
- **핵심 공식/원칙**: `조합 중인 문자(compText_)`는 시각 상태만 담당, `확정된 문자(buffer_)`에 삽입은 `SDL_TEXTINPUT`이나 `CompleteComposition`의 TEXTINPUT 유도를 통해서만 발생.

빠른 찾기: 데이터 파이프라인 §3, 입력 모드/라우팅 §4, 시각 표현 §5, Win32 PAL §6, 안정화 대응 §7, 검증 §8.

---

## 1. 문제와 해결 요약

### 1.1 발생한 문제

`JKEdit`는 초기에 다음 두 경로가 동시에 존재했다.

| 경로 | 동작 |
|------|------|
| ASCII 모드 | `SDL_TEXTINPUT`(`JKEventType::Char`) 이벤트를 받아 ASCII 그대로 삽입 |
| 내장 한글 모드 | `F2` 토글 후 `SDLK_a..SDLK_z`를 가로채 2벌식 `HangulAutomata`로 조합 |

Windows에서 한국어 IME를 켜면:

- `SDL_TEXTINPUT`은 IME가 **확정(commit)**한 완성형 문자열을 UTF-8로 준다.
- `SDL_TEXTEDITING`은 IME가 **조합 중(pre-edit)**인 문자열을 준다.
- 하지만 기존 `JKEvent.cpp`는 `SDL_TEXTEDITING`을 버리고, `JKEdit`는 한글 IME가 활성화된 상태에서도 ASCII 모드로 동작했다.
- 결과: OS IME가 "한"을 조합 중일 때 `JKEdit`가 ASCII 모드로 'h', 'a', 'n'을 따로 받아 삽입하거나, F2 내부 오토마타와 OS IME가 동시에 조합해 글자가 깨지거나 두 번 입력됨.

### 1.2 해결 전략

1. **OS IME 이벤트를 최우선으로 받는다.** `SDL_TEXTEDITING`으로 조합 상태만 갱신, `SDL_TEXTINPUT`으로 확정 삽입.
2. **내장 2벌식 오토마타는 폴백(F2)으로 격리.** `InternalHangul` 모드일 때만 `SDLK_a..SDLK_z`를 오토마타로 라우팅.
3. **Win32 IMM32 API는 `JKPlatform` PAL로 격리.** 헤더 오염과 호출 규약 리스크 차단.
4. **조합 중 키는 OS IME에 양보.** 백스페이스/딜리트/방향키/엔터를 `JKEdit`가 처리하지 않음.
5. **모드 충돌 방지.** `F2`로 내부 오토마타를 켜면 OS IME를 ASCII로 강제 전환.

---

## 2. 데이터 모델: 완성형 ↔ KSSM 조합형 변환

`JKDC` 비트맵 폰트 렌더러는 **2바이트 KSSM(조합형 한글)** 을 기대한다. 따라서 OS IME가 주는 UTF-8 완성형을 KSSM으로 변환해야 한다.

```
SDL_TEXTINPUT / SDL_TEXTEDITING text (UTF-8 완성형)
        │
        ▼
   Utf8ToKssm()
        │
        ├──► MultiByteToWideChar(CP_UTF8) → WCHAR
        ├──► WideCharToMultiByte(949)    → CP949 (EUC-KR)
        └──► wancode.h의 wCodeTable[] / SingleHan[] / 역한자 매핑 → KSSM 2바이트
        │
        ▼
   JKEdit buffer_ (KSSM 2바이트 저장)
        │
        ▼
   JKDC::TextOut() → HANGUL.FNT / HANJA.FNT / SPECIAL.FNT 비트맵 폰트 렌더링
```

### 2.1 변환 불가 문자에 대한 방어

- CP949에 없는 문자(일부 이모지, 확장 완성형): `'?'`로 폴백.
- KSSM 매핑 테이블에 없는 CP949 영역: `'?'`로 폴백.
- ASCII 영역(0x00..0x7F): 1바이트 그대로 통과.

이 방어 덕분에 IME로 샾(`#`), 특수기호, 한자 등이 들어와도 버퍼가 깨지지 않는다. 다만 매핑되지 않는 문자는 `'?'`로 표시되므로, 필요 시 추가 매핑을 `JKHangulUtil.cpp`에 확장하면 된다.

### 2.2 파일 위치

| 파일 | 역할 |
|------|------|
| `include/JKHangulUtil.h` | `Utf8ToKssm()` 선언 |
| `src/JKHangulUtil.cpp` | UTF-8 → CP949 → KSSM 변환 구현. `MultiByteToWideChar`/`WideCharToMultiByte`를 직접 선언해 `<windows.h>` 의존 회피 |
| `JKWINDOW/WANCODE.CPP` / `wancode.h` | 레거시 KSSM 변환표 (`wCodeTable[]`, `SingleHan[]`) |

---

## 3. 입력 모드와 이벤트 라우팅

`JKEdit`는 3가지 입력 모드를 가진다.

```cpp
enum class InputMode { Ascii, InternalHangul, ImeHangul };
```

| 모드 | 의미 | 진입 방법 |
|------|------|-----------|
| `Ascii` | ASCII/숫자/특수 문자만 직접 입력 | 기본. OS IME가 영문 상태이면 자동 설정 |
| `ImeHangul` | OS IME가 한글 조합 중 | Windows에서 IME가 켜지고 native 비트가 설정되면 자동 설정 |
| `InternalHangul` | 내장 2벌식 오토마타 사용 | `F2` 키로 토글 |

### 3.1 이벤트별 책임 분리

| 이벤트 | 책임 | JKEdit 동작 |
|--------|------|-------------|
| `SDL_TEXTEDITING` (`JKEventType::TextEditing`) | 조합 중 시각 상태 갱신 ONLY | `compText_ = Utf8ToKssm(ev.text)`, `compCursor_ = ev.editStart`, `imeComposing_ = !compText_.empty()` |
| `SDL_TEXTINPUT` (`JKEventType::Char`) | 확정된 문자 삽입 ONLY | `compText_.clear()`, `imeComposing_ = false`, `InsertKssmText(Utf8ToKssm(ev.text))` |
| `SDL_KEYDOWN` (`JKEventType::KeyDown`) | 모드 토글, 커서 이동, 편집 | 모드/조합 상태에 따라 라우팅. 조합 중일 때는 제어키를 OS에 양보 |

### 3.2 라우팅 상세

```
KeyDown 도착
    │
    ├── Ctrl+C/V/X/A → 클립보드/전체선택 (항상 우선)
    │
    ├── F2
    │       ├── 조합 중이면 무시
    │       └── 조합 중 아니면 InternalHangul ↔ Ascii 토글
    │           └── InternalHangul로 진입 시 OS IME ASCII 모드로 강제 전환
    │
    ├── InternalHangul 모드 AND 조합 중 아니고 AND SDLK_a..SDLK_z
    │       └── ProcessHangulKey() → 내장 오토마타
    │
    ├── imeComposing_ == true
    │       └── Backspace/Delete/←→↑↓/Home/End/PgUp/PgDn/Enter 는 OS IME에 양보 (return)
    │
    └── 나머지 → 기존 커서 이동/삭제/Return 처리
```

### 3.3 `TEXTEDITING`과 `TEXTINPUT` 순서 꼬임 방지

OS마다, 심지어 동일 OS 내에서도 두 이벤트의 순서가 뒤바뀔 수 있다. 예를 들어:

- `TEXTEDITING("")` → `TEXTINPUT("한글")`
- `TEXTINPUT("한글")` → `TEXTEDITING("")`

`JKEdit`는 `TEXTINPUT`이 도착하면 기존 `compText_`를 **로컬 커밋하지 않고 그냥 클리어**한다. 왜냐하면:

- `TEXTINPUT` 자체가 IME가 이미 확정한 문자열이다.
- `TEXTEDITING("")`은 단순히 조합 UI를 닫는 이벤트일 뿐 확정을 의미하지 않는다.
- 로컬 커밋을 섞으면 "이미 확정된 조합"을 두 번 버퍼에 넣는 race condition이 발생한다.

따라서 **확정은 오직 `TEXTINPUT`이 책임**하고, `TEXTEDITING`은 시각 상태만 동기화한다.

---

## 4. 조합 상태의 시각 표현

`JKEdit::OnPaintClient()`에서 `buffer_`를 그린 뒤, `compText_`를 커서 위치에 추가로 그린다.

```
[buffer_ "abc"] [caret] [compText_ "한글" highlighted]
```

- **하이라이트**: 파란색 반투명(alpha 64) 배경 박스.
- **보조 캐럿**: 빨간색 세로선, `compCursor_` 위치에 표시.
- **단일/멀티라인 모두 동일한 원칙** 적용. 멀티라인은 현재 커서가 있는 행/열에 compText_를 붙인다.

### 4.1 IME 후보창 위치 동기화

`SDL_SetTextInputRect()`를 호출해 OS IME의 후보/자동완성 창이 에디트 컨트롤 아래쪽에 나타나도록 한다.

호출 시점:

- `OnSetFocus()`
- `TEXTEDITING`/`TEXTINPUT` 처리 후 (`UpdateTextInputRect()`)
- 커서 이동/삽입 후

---

## 5. 플랫폼 추상화: `JKPlatform`

`JKEdit.cpp`에서 `<windows.h>`를 직접 포함하면 `TextOut`, `SendMessage`, `min`, `max` 등의 매크로가 전역 네임스페이스를 오염시켜 `JKDC::TextOut` 같은 메서드가 충돌한다. 또 IMM32 함수의 호출 규약(`__stdcall`)과 타입을 직접 선언하면 32/64비트 안정성이 떨어진다. 따라서 Win32 관련 로직을 단일 PAL인 `JKPlatform`으로 모았다.

### 5.1 PAL 구조

| 파일 | 역할 |
|------|------|
| `include/JKPlatform.h` | SDL_Window 포인터만 사용하는 순수 C++ 인터페이스. Win32 타입 없음 |
| `src/JKPlatform_win32.cpp` | Windows 빌드에서만 컴파일. 여기서만 `<windows.h>`, `<imm.h>` 포함 |

```cpp
namespace jk {

struct DisplayInfo { /* ... */ };
struct FrameMetrics { /* ... */ };
struct Placement { /* ... */ };

class JKPlatform {
public:
    // Process / display / window placement
    static void InitializeProcessDpiAwareness();
    static int  GetDisplayCount();
    static bool GetDisplayInfo(int displayIndex, DisplayInfo& out);
    static bool GetWindowFrameMetrics(SDL_Window* window, FrameMetrics& out);
    static bool ComputeCenteredPlacement(SDL_Window* window,
                                        int clientPtW, int clientPtH,
                                        Placement& out);

    // Mouse and synthetic input
    static bool GetPhysicalMousePos(SDL_Window* window, int& outX, int& outY);
    static bool SendSyntheticKey(uint32_t vkOrScancode, bool extended, bool keyUp);
    static bool SendSyntheticChar(wchar_t ch);

    // IME (moved from the former JKPlatformIme PAL)
    enum class ImeMode { Ascii, Hangul, Unknown };
    static ImeMode GetCurrentConversionMode(SDL_Window* window);
    static void    SetConversionMode(SDL_Window* window, ImeMode mode);
    static void    CompleteComposition(SDL_Window* window);
};

} // namespace jk
```

`JKPlatform`은 IME뿐 아니라 DPI 인식, 디스플레이 열거, 창 프레임 메트릭/배치, 물리 픽셀 마우스 좌표, 합성 키 입력 등 Windows 전용 로직도 한 곳에서 캡슐화한다. `JKApplication`과 `JKEdit`는 모두 `JKPlatform.h`만 포함한다.

### 5.2 구현 세부

`GetCurrentConversionMode()`:

1. `SDL_GetWindowWMInfo()`로 `HWND` 획득.
2. `ImmGetContext()` → `ImmGetOpenStatus()` + `ImmGetConversionStatus()`.
3. `conversion & IME_CMODE_NATIVE` 이면 `Hangul`, 아니면 `Ascii`.
4. 반드시 `ImmReleaseContext()`로 핸들 반환.

`SetConversionMode()`:

- `Hangul` 요청 시: `conversion |= IME_CMODE_NATIVE`, `conversion &= ~IME_CMODE_ALPHANUMERIC`.
- `Ascii` 요청 시: `conversion &= ~IME_CMODE_NATIVE`, `conversion |= IME_CMODE_ALPHANUMERIC`.
- `ImmSetConversionStatus()`로 적용.

`CompleteComposition()`:

- `ImmNotifyIME(himc, NI_COMPOSITIONSTR, CPS_COMPLETE, 0)` 호출.
- 이 호출은 OS IME에게 "지금 조합 중인 문자를 TEXTINPUT으로 확정하라"고 요청한다.
- 대부분의 IME는 즉시 `SDL_TEXTINPUT` 이벤트를 SDL 큐에 넣는다.

### 5.3 크로스 플랫폼 확장

현재는 Windows 구현만 있지만, Linux(IBus/Fcitx)나 macOS 확장 시:

- `src/JKPlatform_linux.cpp`, `src/JKPlatform_mac.cpp`를 추가.
- `CMakeLists.txt`에서 OS별로 소스 선택.
- `JKPlatform.h` 인터페이스는 그대로 유지되므로 `JKApplication.cpp`와 `JKEdit.cpp`를 수정할 필요가 없다.

---

## 6. 안정화를 위한 추가 고려사항

### 6.1 조합 중 포커스 아웃

사용자가 "한그"까지 조합한 뒤 다른 컨트롤을 클릭하면:

1. `JKEdit::OnKillFocus()`에서 `JKPlatform::CompleteComposition()` 호출.
2. OS IME가 `TEXTINPUT("한글")`을내면 `buffer_`에 안전히 삽입.
3. 만약 OS가 이벤트를 보내지 않으면(일부 IME/상황), 로컬 `CommitComposition()` 폴백이 남아 있는 `compText_`를 버퍼에 넣는다.

### 6.2 조합 중 같은 에디트 내 마우스 클릭

`MouseDown` 핸들러의 첫 부분에서 `CompleteComposition()`을 호출한 뒤에 커서/선택을 이동한다. 그렇지 않으면 조합 중인 글자가 증발하거나 커서 위치가 꼬인다.

### 6.3 F2로 인한 이중 조합 방지

OS IME가 한글 모드인 상태에서 사용자가 `F2`를 누르면:

- `inputMode_`를 `InternalHangul`로 설정.
- 동시에 `JKPlatform::SetConversionMode(window, JKPlatform::ImeMode::Ascii)`로 OS IME를 영문으로 전환.
- 결과: OS IME는 더 이상 한글을 조합하지 않고, 내부 오토마타만 동작.

### 6.4 조합 중 커서/삭제키 차단

`imeComposing_ == true`일 때 `KeyDown`에서 다음 키를 무시(return)한다:

- `SDLK_BACKSPACE`, `SDLK_DELETE`
- `SDLK_LEFT`, `SDLK_RIGHT`, `SDLK_UP`, `SDLK_DOWN`
- `SDLK_HOME`, `SDLK_END`, `SDLK_PAGEUP`, `SDLK_PAGEDOWN`
- `SDLK_RETURN`, `SDLK_KP_ENTER`

이렇게 하지 않으면 백스페이스 한 번에 "조합 중인 자모"와 "이미 확정된 앞 글자"가 동시에 삭제되는 "대참사"가 발생한다.

---

## 7. 파일 변경 및 의존성

### 7.1 추가된 파일

| 파일 | 역할 |
|------|------|
| `include/JKHangulUtil.h` | UTF-8 → KSSM 변환 헤더 |
| `src/JKHangulUtil.cpp` | 변환 구현 |
| `include/JKPlatform.h` | 통합 Win32 PAL 인터페이스(DPI, 디스플레이, 창 배치, 마우스, 합성 입력, IME) |
| `src/JKPlatform_win32.cpp` | Windows PAL 구현. 여기서만 `<windows.h>`, `<imm.h>` 사용 |

### 7.2 수정된 파일

| 파일 | 변경 내용 |
|------|-----------|
| `include/JKEvent.h` | `JKEventType::TextEditing` 추가, `editStart`/`editLength`/`text[64]` 확장 |
| `src/JKEvent.cpp` | `SDL_TEXTEDITING` → `JKEventType::TextEditing` 변환 |
| `src/JKApplication.cpp` | `TextEditing` 이벤트 라우팅 |
| `include/JKEdit.h` | `InputMode` enum, `compText_`/`compCursor_`/`imeComposing_` 멤버, `UpdateTextInputRect()`, `CommitComposition()` 추가 |
| `src/JKEdit.cpp` | IME 이벤트 처리, 조합 상태 그리기, `JKPlatform` 사용 |
| `src/JKApplication.cpp` | `JKPlatform::ComputeCenteredPlacement()`, `JKPlatform::GetPhysicalMousePos()` 사용 |
| `src/main.cpp` | `Utf8ToKssm` 제거 → `JKHangulUtil.h` 사용, `AppSelfTest`에 JKEdit IME 단위 테스트 추가 |
| `CMakeLists.txt` | `JKHangulUtil.cpp`, `JKPlatform_win32.cpp`, `imm32` 링크 추가 |

### 7.3 의존성

- `JKEdit.cpp`는 이제 `JKHangulUtil.h`와 `JKPlatform.h`만 포함. `<windows.h>`/`imm.h` 의존 제거.
- `JKPlatform_win32.cpp`만 SDL + Win32 + IMM32에 의존.
- Linux/macOS 빌드 시 `JKPlatform_win32.cpp`를 제외하면 `JKPlatform::GetCurrentConversionMode()`는 `JKPlatform::ImeMode::Unknown`을 반환하고 `JKEdit`는 `F2` 내부 오토마타를 기본 폴백으로 동작.

---

## 8. 검증

### 8.1 자동 단위 테스트

`AppSelfTest`에 추가된 5개 합성 이벤트 테스트:

| # | 체크 | 검증 내용 |
|---|------|-----------|
| 1 | `ime pre-edit does not commit to buffer` | `TEXTEDITING`만으로는 `buffer_`가 변하지 않음 |
| 2 | `ime pre-edit update still not committed` | 조합 중간 갱신도 버퍼에 반영되지 않음 |
| 3 | `ime committed text stored as KSSM` | `TEXTINPUT`이 들어오면 `Utf8ToKssm` 후 2바이트 삽입 |
| 4 | `f2 toggles internal hangul automata` | `F2`로 `InternalHangul` 모드 진입 |
| 5 | `internal automata produces multi-byte KSSM` | 내부 오토마타 결과가 멀티바이트 KSSM임 |

### 8.2 수동 통합 테스트

1. `jkproto_sdl2_jkwindow.exe` 실행 → 메인 데모.
2. 에디트 컨트롤 클릭으로 포커스.
3. Windows 한국어 IME 켜고 "한글" 입력.
4. 기대:
   - 조합 중 글자가 파란 하이라이트 박스 안에 표시.
   - 빨간 보조 캐럿이 조합 문자열 안에서 이동.
   - 스페이스/엔터 확정 시 하이라이트 사라지고 2바이트 KSSM 저장.
   - 동일 문자 중복 삽입 없음.
5. `F2` 누르고 `gksrmf` 입력 → 내부 오토마타가 "한글" 생성.

### 8.3 회귀 프로브

변경 후 반드시 수행:

- `jkproto_sdl2_jkwindow.exe test` → 0 failure(s)
- `tools\verify_tab_navigation.ps1` → PASS
- `tools\verify_dialog_keyboard.ps1` → PASS
- `tools\click_jango_probe.ps1` → PASS
- `tools\verify_fixwin3.ps1` → 14 pass, 0 fail

---

## 9. 결정사항/가정

1. **OS IME 우선, 내부 오토마타 폴백**: Windows에서는 MS 한국어 IME가 가장 자연스러운 UX를 제공(한/영 전환, 한자, 특수기호 후보창). 내장 2벌식 오토마타는 IME가 없는 환경이나 특수한 폴백용.
2. **KSSM 2바이트가 내부 저장의 단일 진실**: 레거시 `JKDC` 폰트 렌더러가 KSSM을 기대하므로, 모든 텍스트는 저장 시점에 KSSM으로 정규화한다. 변환은 `JKHangulUtil`에서만 수행.
3. **Win32 API는 PAL 뒤로 숨김**: `JKEdit` 같은 프레임워크 코어 코드에서 `<windows.h>`를 직접 포함하지 않는다. 매크로 오염과 호출 규약 문제를 원천 차단.
4. **TEXTEDITING은 시각 상태만**: 버퍼 수정 권한은 오직 `TEXTINPUT`과 `CompleteComposition`의 TEXTINPUT 유도에만 있음. 이벤트 순서 꼬임에 강하다.
5. **조합 중 키는 OS IME가 소유**: 백스페이스/삭제/방향키/엔터를 프레임워크가 처리하면 IME와 충돌. 포커스 아웃/클릭 시 강제 확정을 통해 안전하게 회복.

---

## 10. 향후 개선 포인트

- **Linux/macOS PAL**: `JKPlatform_linux.cpp`, `JKPlatform_mac.cpp` 추가.
- **IME 모드 동기화 양방향**: 현재는 `JKEdit`가 OS IME 상태를 읽어오고 `F2` 시 OS IME를 ASCII로 내리는 단방향. OS IME가 외부에서 다시 한글로 전환되면 `DetectWindowsImeState()`가 이를 감지해 `ImeHangul`로 복귀.
- **조합 문자 저장/복원**: 에디트가 포커스를 잃었다가 다시 얻을 때 OS IME가 이전 조합을 복원하지 않는 경우가 많음. 현재는 포커스 아웃 시 강제 확정으로 일관성을 유지.
- **매핑 확장**: 현재 `'?'`로 폴백되는 문자들(확장 한글, 일부 특수기호)에 대한 KSSM 매핑 추가 가능.

---

## 11. 관련 문서

- `ARCHITECTURE_DOCS/15_verification_playbook.md` §11 — IME 입력 검증 절차 및 안정화 포인트 요약
- `ARCHITECTURE_DOCS/12_sdl2_prototype_roadmap.md` Phase 1 §1.6 — IME 항목
- `ARCHITECTURE_DOCS/11_jkwindow_sdl_mapping.md` — JKWINDOW → SDL2 이벤트/컨트롤 매핑
- `ARCHITECTURE_DOCS/14_sdl2_window_dpi.md` — 좌표계/스케일링 (마우스/입력 좌표와 연결)
- `.claude/PROJECT.md` — 변경 후 필수 검증 절차
