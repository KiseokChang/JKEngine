#!/bin/sh
# engine/tools/posix_selftest/build.sh — builds the WSL posix selftest harness
# (stage-2 plan D, task 1). Compiles the posix adapter TUs directly with g++ —
# no CMakeLists changes (CMake stays Windows-only).
#
# Flags per controller ruling: -std=c++17 -pthread -lutil unconditionally
# (-lutil's real consumer lands in later stage-2 tasks; harmless here).
#
# Out-dir: engine/build/posix_selftest — a scratch exe inside the existing
# gitignored engine/build directory (nothing else in engine/build is touched,
# no new scratch dir outside the tree).
#
# Usage (repo root /mnt/i/progwork/JKENGINE under WSL Ubuntu-24.04):
#   sh engine/tools/posix_selftest/build.sh && ./engine/build/posix_selftest
set -e

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SCRIPT_DIR"/../../.. && pwd)
OUT="$ROOT/engine/build/posix_selftest"

mkdir -p "$ROOT/engine/build"

g++ -std=c++17 -pthread -lutil -O1 -Wall -Wextra \
    -I"$ROOT/engine/include" \
    -o "$OUT" \
    "$SCRIPT_DIR/main.cpp" \
    "$ROOT/engine/src/fs/JKFs_posix.cpp"

echo "posix_selftest build ok -> $OUT"
