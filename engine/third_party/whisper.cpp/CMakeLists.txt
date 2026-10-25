# ---------------------------------------------------------------------------
# ENGINE-OWNED build surface for the vendored whisper.cpp subset — this file is
# NOT upstream whisper.cpp's CMakeLists.txt (engine/third_party 선례 — quickjs:
# "trimmed to the four library sources ... Mirrors the upstream CMake's compile
# definitions"). Upstream's root CMakeLists (v1.9.5) also drives WASM/JS
# bindings, install/export, tests and ~20 example binaries — none of that is
# vendored here (선택 근거: third_party 선형 = MB급 소스·빌드 타깃 최소화).
#
# Contract (spec 2026-10-25-stt-voice-design §3 ②, R1 — T1):
#   * exactly ONE executable target = whisper-cli (별도 프로세스 스폰 — the
#     engine never links the whisper/ggml static libs; 폰 #3117 segfault
#     격리 원존 — "라이브러리 링크 금지").
#   * the ggml subtree keeps upstream's own CMakeLists (add_subdirectory(ggml))
#     so the CPU backend selection (x86/aarch64 arch sources, OpenMP, -march)
#     stays upstream's — Windows/WSL x86_64 build here, and the SAME tree is
#     what the phone (Termux aarch64, Task 5) rebuild picks up.
#   * upstream src/CMakeLists.txt is reduced to the CPU-only `whisper` target
#     (coreml/openvino/vitisai/parakeet dirs are pruned from the vendored
#     subset; their upstream file lists sit behind WHISPER_USE_* options that
#     are never defined here).
#   * examples/common* + grammar-parser are the `common` static lib upstream
#     examples/CMakeLists.txt declares (FFMPEG/SDL2 variants pruned —
#     WHISPER_COMMON_FFMPEG/WHISPER_SDL2 are off; ffmpeg-transcode.cpp and
#     common-sdl.cpp are not in the subset).
# Source of truth for the vendored files = upstream tag v1.9.5 —
# https://github.com/ggml-org/whisper.cpp (LICENSE + AUTHORS vendored alongside).
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)
project(whisper_cpp_vendor LANGUAGES C CXX)

# src/whisper.cpp:9336 prints this via the WHISPER_VERSION macro (upstream
# v1.9.5 — pin it instead of running upstream's version plumbing, which
# depends on the pruned bindings/git state).
set(WHISPER_VERSION "1.9.5")

# Static everything: ggml defaults to SHARED libs on non-MINGW hosts, and the
# WSL/phone build dirs live on DrvFs mounts where cmake_symlink_library
# fails ("failed to create symbolic link ... Operation not permitted" — 2026-10-25
# first WSL build 원장). The single-executable contract wants static libs anyway —
# one whisper-cli binary, no runtime .so search (폰 #3117 backend-공유-lib
# 탐색 segfault와 같은 계통의 fail 면를 소각).
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(ggml)

# whisper — CPU-only subset of upstream src/CMakeLists.txt
add_library(whisper STATIC
    src/whisper.cpp
    src/aneforge/whisper-aneforge.cpp
)
target_include_directories(whisper PUBLIC src include)
target_compile_features(whisper PUBLIC cxx_std_11)  # upstream — "don't bump"
target_compile_definitions(whisper PRIVATE
    WHISPER_VERSION="${WHISPER_VERSION}"
)
find_package(Threads REQUIRED)
target_link_libraries(whisper PUBLIC ggml Threads::Threads)

# common — upstream examples/CMakeLists.txt "common" lib, trimmed to the files
# the cli example needs (renamed whisper-common so the generic upstream name
# cannot collide with engine targets).
add_library(whisper-common STATIC
    examples/common.cpp
    examples/common-ggml.cpp
    examples/common-whisper.cpp
    examples/grammar-parser.cpp
)
target_include_directories(whisper-common PUBLIC examples)
target_compile_features(whisper-common PUBLIC cxx_std_11)
target_link_libraries(whisper-common PRIVATE whisper ${CMAKE_DL_LIBS})

# cli — the single executable target (whisper-cli)
add_executable(whisper-cli examples/cli/cli.cpp)
target_link_libraries(whisper-cli PRIVATE whisper-common whisper Threads::Threads)