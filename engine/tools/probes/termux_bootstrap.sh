#!/usr/bin/env bash
# Termux first-contact bootstrap (docs/78 TX1) — run INSIDE Termux on the phone:
#   bash <path>/termux_bootstrap.sh > tx1.log 2>&1
# Toolchain + libraries + versions probe. No repo source needed; rsync from
# the PC happens separately (PC side, LAN only).
set -u
LOG=$HOME/tx1.log
: > "$LOG"
exec > "$LOG" 2>&1

echo "=== Android/Arch:"
uname -m
getprop ro.build.version.release 2>/dev/null || true
echo "=== storage dir:"; ls "$HOME" | head

echo "=== pkg update:"
pkg update -y
echo "=== toolchain install:"
pkg install -y clang cmake ninja pkg-config git openssh rsync
echo "=== library install (SDL2/SDL2_mixer/ffmpeg — engine system deps):"
pkg install -y sdl2 sdl2-mixer ffmpeg
echo "=== font install (Korean UI — candidates probed in order):"
pkg install -y font-noto-cjk 2>/dev/null || \
pkg install -y noto-fonts-cjk 2>/dev/null || \
pkg search noto || true

echo "=== versions:"
clang --version | head -1
cmake --version | head -1
ninja --version
pkg-config --modversion sdl2
pkg-config --modversion SDL2_mixer
pkg-config --modversion libavcodec
echo "=== font files found:"
find "$PREFIX/share/fonts" -iname '*cjk*' -o -iname '*Noto*ot*' 2>/dev/null | grep -i noto | head -5
echo "=== cores:"; nproc
echo "=== TX1 probe end"