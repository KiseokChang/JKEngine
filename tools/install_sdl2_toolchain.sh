#!/usr/bin/env bash
# MSYS2 UCRT64 환경에서 SDL2 + CMake + MinGW-w64 툴체인을 설치하는 helper 스크립트
# Windows + VS Code + MSYS2 + SDL2 개발 환경 구축용
# 이 환경에서는 bash -lc로 실행하면 stdout이 run_commands에 표시됨.

set -e

echo "[INFO] Updating package database..."
pacman -Sy --noconfirm

echo "[INFO] Checking for broken local pacman database entries..."
BROKEN_HEADERS=0
for broken in /var/lib/pacman/local/mingw-w64-ucrt-x86_64-headers-*; do
    if [ -d "$broken" ] && [ ! -f "$broken/desc" ]; then
        echo "[WARN] Broken local DB entry: $broken (missing desc). Backing up and removing..."
        mv "$broken" "$broken.bak.$(date +%s)" || true
        BROKEN_HEADERS=1
    fi
done

if [ "$BROKEN_HEADERS" -eq 1 ]; then
    echo "[INFO] Repairing/reinstalling base headers package to fix local DB..."
    pacman -S --noconfirm --overwrite '*' mingw-w64-ucrt-x86_64-headers || true
else
    echo "[INFO] Headers local DB looks healthy; skipping repair."
fi

echo "[INFO] Installing MinGW-w64 UCRT64 toolchain..."
pacman -S --noconfirm --needed --overwrite '*' \
    mingw-w64-ucrt-x86_64-toolchain \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-pkgconf \
    mingw-w64-ucrt-x86_64-ninja

echo "[INFO] Installing SDL2..."
pacman -S --noconfirm --needed --overwrite '*' \
    mingw-w64-ucrt-x86_64-SDL2

echo "[INFO] Installing optional SDL2 extension libraries..."
pacman -S --noconfirm --needed --overwrite '*' \
    mingw-w64-ucrt-x86_64-SDL2_ttf \
    mingw-w64-ucrt-x86_64-SDL2_image \
    mingw-w64-ucrt-x86_64-SDL2_mixer \
    mingw-w64-ucrt-x86_64-SDL2_net || true

echo ""
echo "[INFO] Verifying installation..."
command -v gcc >/dev/null 2>&1 || { echo "[ERROR] gcc not found"; exit 1; }
command -v g++ >/dev/null 2>&1 || { echo "[ERROR] g++ not found"; exit 1; }
command -v cmake >/dev/null 2>&1 || { echo "[ERROR] cmake not found"; exit 1; }
pkg-config --modversion sdl2 || { echo "[ERROR] SDL2 pkg-config not found"; exit 1; }

echo ""
echo "[OK] SDL2 development environment is ready."
echo ""
echo "Versions:"
echo "  gcc:  $(gcc --version | head -n1)"
echo "  g++:  $(g++ --version | head -n1)"
echo "  cmake: $(cmake --version | head -n1)"
echo "  sdl2: $(pkg-config --modversion sdl2)"
