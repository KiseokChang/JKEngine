# Windows + VS Code + MSYS2 + SDL2 개발 환경 세팅

> JKENGINE 재현 프로젝트의 기본 SDL2 개발 환경 설정 가이드.
> 이 문서를 따르면 Windows에서도 Linux/Tizen과 거의 동일한 C++ 빌드 환경을 구축할 수 있습니다.

---

## 1. 요약 (추천 스택)

| 구성요소 | 버전/종류 | 역할 |
|----------|-----------|------|
| OS | Windows 10/11 | 호스트 OS |
| 에디터 | VS Code | 코드 편집 + Cline 플러그인 |
| 셸/패키지 관리자 | MSYS2 UCRT64 | Linux 스타일 개발 환경 |
| 컴파일러 | MinGW-w64 UCRT64 | Windows 네이티브 빌드 |
| 빌드 시스템 | CMake | 크로스 플랫폼 빌드 |
| 그래픽 라이브러리 | SDL2 | JKWINDOW 대체 핵심 API |

---

## 2. MSYS2 설치

### 방법 A: winget으로 자동 설치 (권장)

PowerShell 또는 CMD에서:

```powershell
winget install MSYS2.MSYS2 --accept-package-agreements --accept-source-agreements -e
```

설치가 완료되면 터미널을 완전히 종료하고 다시 열어야 합니다.

### 방법 B: 수동 설치

1. https://www.msys2.org/ 에서 설치 프로그램 다운로드
2. 기본 경로 `C:\msys64` 권장
3. 설치 후 MSYS2 UCRT64 터미널 실행 (시작 메뉴에서 "MSYS2 UCRT64" 검색)

### 설치 확인

다음 경로가 존재해야 합니다.

```
C:\msys64\usr\bin\bash.exe
C:\msys64\ucrt64\bin\gcc.exe
```

첫 실행 시 패키지 데이터베이스 업데이트:

```bash
pacman -Syu
```

업데이트 후 터미널을 완전히 종료했다가 다시 실행 (창 닫기 필수).

### SDL2 툴체인 자동 설치

MSYS2 UCRT64 터미널에서 프로젝트의 helper 스크립트를 실행합니다.

```bash
cd /i/progwork/JKENGINE/tools
# Windows에서 파일이 CRLF로 저장되었을 경우를 대비해 LF로 변환
if command -v dos2unix > /dev/null 2>&1; then
    dos2unix ./install_sdl2_toolchain.sh
fi
bash ./install_sdl2_toolchain.sh
```

이 스크립트는 다음을 한 번에 설치합니다.

- MinGW-w64 UCRT64 툴체인 (`gcc`, `g++`, `gdb`, `make`)
- CMake
- SDL2 본체
- (향후 확장 시) SDL2_ttf, SDL2_image

---

## 3. SDL2 + CMake + MinGW 툴체인 설치

UCRT64 터미널에서 다음 명령 실행:

```bash
# 기본 툴체인 (gcc, g++, make, gdb 등)
pacman -S mingw-w64-ucrt-x86_64-toolchain

# CMake
pacman -S mingw-w64-ucrt-x86_64-cmake

# SDL2 본체
pacman -S mingw-w64-ucrt-x86_64-SDL2

# (향후 필요 시) SDL2 확장 라이브러리
# pacman -S mingw-w64-ucrt-x86_64-SDL2_ttf
# pacman -S mingw-w64-ucrt-x86_64-SDL2_image
```

설치 확인:

```bash
gcc --version
g++ --version
cmake --version
pkg-config --modversion sdl2
```

---

## 4. VS Code 터미널을 MSYS2 UCRT64로 설정

`.vscode/settings.json`에 추가:

```json
{
  "terminal.integrated.profiles.windows": {
    "MSYS2 UCRT64": {
      "path": "C:\\msys64\\usr\\bin\\bash.exe",
      "args": [
        "--login",
        "-i"
      ],
      "env": {
        "MSYSTEM": "UCRT64",
        "CHERE_INVOKING": "1"
      }
    }
  },
  "terminal.integrated.defaultProfile.windows": "MSYS2 UCRT64"
}
```

> 터미널을 새로 열면 자동으로 `/ucrt64/bin`, `/usr/bin` 등이 PATH에 포함됩니다.

---

## 5. 최소 예제 프로젝트

### 디렉토리 구조

```
prototype/
└── sdl2_empty_window/
    ├── CMakeLists.txt
    └── main.cpp
```

### `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.16)
project(jkproto_empty_window LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

add_executable(jkproto_empty_window main.cpp)

target_include_directories(jkproto_empty_window PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(jkproto_empty_window PRIVATE ${SDL2_LIBRARIES})
```

### `main.cpp`

```cpp
#include <SDL.h>
#include <cstdio>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "JKENGINE SDL2 Empty Window",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        640,
        480,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        SDL_Delay(16); // ~60 FPS
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
```

---

## 6. 빌드 및 실행

VS Code 터미널(MSYS2 UCRT64)에서:

```bash
cd prototype/sdl2_empty_window
cmake -B build -G "MinGW Makefiles"
cmake --build build
./build/jkproto_empty_window.exe
```

정상 실행 시 "JKENGINE SDL2 Empty Window"라는 제목의 640x480 창이 뜹니다.

---

## 7. PATH에 MSYS2/UCRT64 추가 (VS Code 외부에서도 실행하려면)

VS Code 외부나 PowerShell에서도 `cmake`, `g++`, SDL2를 찾을 수 있도록 시스템 환경 변수 `Path`에 추가:

```
C:\msys64\ucrt64\bin
C:\msys64\usr\bin
```

> VS Code 내 터미널에서는 `.vscode/settings.json` 설정만으로 충분합니다.

---

## 8. 주의사항

| 항목 | 설명 |
|------|------|
| Tizen과의 차이 | MSYS2는 빌드 환경을 맞춰주는 역할이며, Tizen API 그대로 실행하는 것은 아님 |
| 런타임 의존성 | 실행 파일을 배포하려면 `SDL2.dll`을 같은 폴더에 포함해야 함 (`/ucrt64/bin/SDL2.dll`) |
| SDL3 마이그레이션 | 이 예제는 SDL2 기준. SDL3로 옮길 때는 `bool` 타입 등 소규모 수정 필요 |

---

## 9. 다음 단계

이 환경이 구축되고 빈 창이 뜨는 것을 확인한 후 다음 문서로 이동:

- `ARCHITECTURE_DOCS/11_jkwindow_sdl_mapping.md` — JKWINDOW → SDL2 클래스 설계
- `prototype/sdl2_jkwindow/` — JKDC/JKControl 기반 프로토타입 구현

---

## 10. 확인 체크리스트

- [ ] MSYS2 설치 완료
- [ ] `pacman -Syu` 업데이트 완료
- [ ] MinGW-w64, CMake, SDL2 설치 완료
- [ ] VS Code 터미널이 MSYS2 UCRT64로 열림
- [ ] `cmake -B build -G "MinGW Makefiles"` 성공
- [ ] `cmake --build build` 성공
- [ ] `./build/jkproto_empty_window.exe` 실행 시 창이 정상 표시됨
