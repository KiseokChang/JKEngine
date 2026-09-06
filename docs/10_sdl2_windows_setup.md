# Windows 빌드 환경 세팅 (MSYS2 + CMake + Ninja)

> 다른 PC에서 이 저장소를 클론 → 빌드 → 실행하기까지의 전체 가이드.
> 2026-09-07 기준, 실제 빌드 머신에서 검증된 구성입니다.
> 과거 버전(빈 SDL2 창 예제 중심)은 `docs/12` 히스토리로 대체되었습니다.

---

## 1. 요약

| 구성요소 | 동작 확인 버전 | 역할 |
|----------|-----------|------|
| OS | Windows 11 (64bit) | 호스트 OS |
| 셸/패키지 관리자 | MSYS2 UCRT64 | MinGW 툴체인 + pacman |
| 컴파일러 | gcc 16.2.0 (MinGW-w64 UCRT64) | 네이티브 빌드 |
| 빌드 시스템 | cmake 4.4.2 + ninja 1.13.2 | configure + 빌드 |
| 창/입력 | SDL2 2.32.10 | GWES 기반 |
| 미디어 | ffmpeg 9.0.1 | vplayer 디코딩 |

전체 흐름:

```
git clone → MSYS2 설치 → pacman 패키지 설치 → cmake 빌드 → 자가테스트 → --server 실행
```

서드파티(imgui, implot, quickjs-ng, stb, 메모리 에디터)는 **repo에 벤더링**되어
클론만으로 해결됩니다. 유일한 예외는 CEF(브라우저 앱)로, §8의 선택적 조달이
필요합니다 — 없어도 나머지 17개 앱은 정상 빌드됩니다.

---

## 2. 사전 요구

- Windows 10/11 64bit
- git (winget install Git.Git 등)
- 디스크 여유: 소스 + 빌드 트리 + CEF 바이너리까지 합쳐 여유 3GB 이상 권장
  (특히 C: — MinGW가 임시 파일을 C: %TEMP%에 쓰므로 C:가 가득 차면
  빌드가 0xc0000409/짧은 쓰기 오류로 죽습니다. §9 참고)

## 3. MSYS2 설치

### 방법 A: winget (권장)

```powershell
winget install MSYS2.MSYS2 --accept-package-agreements --accept-source-agreements -e
```

설치 완료 후 터미널을 완전히 종료했다가 다시 엽니다.

### 방법 B: 수동

1. https://www.msys2.org/ 에서 설치 프로그램 다운로드
2. 기본 경로 `C:\msys64` 권장

### 패키지 DB 업데이트

"MSYS2 UCRT64" 터미널(시작 메뉴)에서:

```bash
pacman -Syu
```

업데이트 후 창을 닫았다가 다시 엽니다(필수).

## 4. 필수 패키지

UCRT64 터미널에서 한 번에 설치:

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-pkgconf \
  mingw-w64-ucrt-x86_64-SDL2 \
  mingw-w64-ucrt-x86_64-SDL2_image \
  mingw-w64-ucrt-x86_64-SDL2_mixer \
  mingw-w64-ucrt-x86_64-ffmpeg
```

| 패키지 | 필요 시점 | 비고 |
|--------|-----------|------|
| toolchain | 빌드 | gcc/g++/gdb 등 |
| cmake, ninja, pkgconf | 빌드 | 제너레이터는 Ninja 고정 |
| SDL2 | configure (REQUIRED) | 창/입력/렌더 |
| SDL2_image | **런타임** | PNG 로딩 — dlopen이라 configure엔 안 보이지만 반드시 필요 |
| SDL2_mixer | configure (REQUIRED) | 사운드 |
| ffmpeg | configure (**REQUIRED**) | libavformat/avcodec/swscale/swresample/avutil — 빠지면 cmake configure 자체가 실패 |

SDL2_ttf / SDL2_net은 사용하지 않습니다(설치되어 있어도 무해).

설치 확인:

```bash
gcc --version && cmake --version && ninja --version
pkg-config --modversion sdl2 libavformat
```

## 5. 클론

```bash
cd /드라이브/작업위치        # 어느 드라이브든 가능 (예: /d/work)
git clone https://github.com/KiseokChang/JKEngine.git
cd JKEngine
```

디렉터리 구성:

```
engine/   — 엔진 전부 (src/include/assets/third_party/tools)
legacy/   — 원본 JKWINDOW/WINDBASE 등 읽기 전용 참조
docs/     — 아키텍처 문서
```

## 6. 빌드

**반드시 `engine/` 디렉터리에서 실행**합니다(다른 cwd에서는 CMake 캐시를
못 찾아 실패). 매 세션 PATH 세팅이 필요합니다:

```bash
cd engine
export PATH="/c/msys64/ucrt64/bin:$PATH"
cmake -B build -G Ninja
cmake --build build          # 또는: cmake --build build --target jkx_packages
```

빌드가 자동으로 해 주는 것:

- 런타임 DLL 클로저 수집 — exe/모듈 import를 objdump로 훑어 SDL2/FFmpeg/
  libcef 등을 `build/`로 복사 → **PATH 없이 바로 실행 가능한 self-contained
  빌드 트리**
- assets 트리 복사 → `build/assets/`
- `apps/*.jkx` 18개 패키지 자동 재팩 (DLL/스크립트/아이콘 변경 감지)

빌드 검증:

```bash
./build/jkdesktop.exe test
# 마지막 줄: AppSelfTest: 0 failure(s)
```

### 레거시 빌드 스크립트에 대해

`engine/build_sdl2_jkwindow.bat`는 예전 I: 드라이브에 obj 파일을 직접 쓰지
못하던 시절의 우회 스크립트(소스 전체를 C:로 복사해 빌드)로, 드라이브 경로가
하드코딩되어 있습니다. 다른 PC에서는 위 표준 경로를 사용하세요. I: 드라이브
쓰기 문제가 재현되면 빌드 트리만 C:로 분리합니다:
`cmake -B /c/jk_build -S . -G Ninja` 이후 `cmake --build /c/jk_build`.

## 7. 실행

| 명령 | 동작 |
|------|------|
| `./build/jkdesktop.exe --server` | **윈도우 서버(데스크탑 셸)** — 배경화면 + 런처 아이콘 18개 + 태스크바. 아이콘 클릭 시 `apps/*.jkx`에서 클라이언트 프로세스 스폰 |
| `./build/jkdesktop.exe` | 레거시 단일 프로세스 프로토타입 데모 |
| `./build/jkdesktop.exe terminal` | 단일 프로세스 ConPTY 터미널 |
| `./build/jkdesktop.exe pcx 파일경로` | 이미지 뷰어 (PCX/PNG/JPG/BMP) |
| `./build/jkdesktop.exe tetris` / `minesweeper` / `vector` / `iconedit` / `recog` / `vfont` / `vpres` / `jango` / `occ` | 단일 모드 앱들 |
| `./build/jkdesktop.exe test` | 자가테스트 |
| `./build/jkdesktop.exe test-script 파일.js` | UI 자동화 시나리오 (exit code = 실패 수) |
| `./build/jkdesktop.exe jkx-list apps/xxx.jkx` | .jkx 컨테이너 TOC 조회 |
| `./build/jkdesktop.exe jkx-extract apps/xxx.jkx` | .jkx 엔트리 추출 |

서버 모드 팁:

- 서버를 죽이면 살아 있는 클라이언트(태스크바 포함)는 3초 안에 자가 종료됩니다.
- 클론 직후 첫 실행 체크: `--server` → 배경화면에 아이콘 그리드 18개 + 하단
  태스크바 → 아이콘 클릭 → 앱 창 스폰 → 태스크바 버튼으로 포커스 전환.

## 8. (선택) CEF 조달 — 브라우저 앱

`jkapp_browser`만 CEF가 필요합니다. `engine/third_party/cef/`는 용량 문제로
git에서 제외되어 있으므로 수동 조달합니다:

1. https://cef-builds.spotifycdn.com/index.html 에서
   `cef_binary_144.0.6+...chromium-144.0.7559.59_windows64_minimal.tar.bz2`
   (Spotify 빌드, windows64 minimal) 다운로드
2. `engine/third_party/cef/` 아래에 압축 해제 (`include/`, `Release/`,
   `Resources/`가 바로 보이게)
3. 다시 `cmake -B build -G Ninja`부터 재configure → 빌드에 browser가 포함되고
   `cef_deploy` 단계가 런타임 파일을 `build/`로 복사

third_party/cef가 없으면 CMake가 브라우저만 자동 제외합니다. 자세한 내용은
`engine/third_party/cef/README.md` 참조.

## 9. 트러블슈팅

| 증상 | 원인/해결 |
|------|-----------|
| `cmake: command not found`, SDL2를 못 찾음 | `export PATH="/c/msys64/ucrt64/bin:$PATH"` 누락. UCRT64 터미널인지 확인 |
| configure 실패: `libavformat ... REQUIRED` | ffmpeg 패키지 미설치 (§4 표) |
| 빌드 중 0xc0000409, "Short write extracting", 이상한 fwrite 실패 | **C: 디스크 부족**. MinGW 임시파일/%TEMP%가 C:에 쓰임. 공간 확보하거나 `TMP`/`TEMP` 환경변수를 다른 드라이브 경로로 지정 |
| 코드를 고쳤는데 앱이 안 바뀜 | 실행 중인 클라이언트가 .jkx를 잠금 — 해당 앱 창을 닫고 재빌드. 자동 재팩은 빌드 시점에만 동작 |
| `build is not a directory`류 빌드 에러 | cwd가 `engine/`이 아님 (§6) |
| 화면 캡처/검증이 이상함 | PrintWindow는 가상화 좌표+스테일 프레임 반환 — CopyFromScreen 기반 캡처만 신뢰 (docs/15 참조) |

## 10. 확인 체크리스트

- [ ] `pacman -Syu` + §4 패키지 설치 완료
- [ ] `pkg-config --modversion sdl2 libavformat` 출력됨
- [ ] `cmake -B build -G Ninja` 성공 (engine/ 안에서)
- [ ] `cmake --build build` 성공 — `build/jkdesktop.exe` 생성
- [ ] `./build/jkdesktop.exe test` → `AppSelfTest: 0 failure(s)`
- [ ] `./build/jkdesktop.exe --server` → 런처 18개 고유 아이콘 + 태스크바
- [ ] 런처에서 minesweeper/terminal 등 클릭 → 앱 창 스폰 확인

## 11. 다음 단계

- `docs/19_sdl2_window_server.md` — 윈도우 서버 아키텍처
- `docs/21_jkx_container.md` — .jkx 패키지 포맷
- `docs/23_sdl2_dear_imgui.md` — ImGui 앱 작성 규약
- `docs/README.md` — 전체 문서 인덱스