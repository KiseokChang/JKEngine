#!/bin/sh
# engine/tools/posix_selftest/build.sh — builds the WSL posix selftest harness
# (stage-2 plan D, task 1). Compiles the posix adapter TUs directly with g++ —
# no CMakeLists changes (CMake stays Windows-only).
#
# Flags per controller ruling: -std=c++17 -pthread -lutil unconditionally
# (-lutil's real consumer lands in later stage-2 tasks; harmless here).
# T3 승격: -std=c++20 — JKFs_posix의 FileTimeToSys가 clock_cast(docs/78 TX2,
# C++20 전용)를 태우므로 c++17 하네스는 더 이상 컴파일 되지 않는다(하네스 낙후
# 실측 — agent TU 링크 전 재발견). c++20으로 올려 케이스 15를 개통한다.
#
# Out-dir: engine/build/posix_selftest — a scratch exe inside the existing
# gitignored engine/build directory (nothing else in engine/build is touched,
# no new scratch dir outside the tree).
#
# Usage (repo root /mnt/i/progwork/JKENGINE under WSL Ubuntu-24.04):
#   sh engine/tools/posix_selftest/build.sh && ./engine/build/posix_selftest
#
# T3 (2026-10-08): agent TU 2건 추가(JKLmEngine/JKAgentJson) — case 15가
# 직 링크로 TurnSync 동기 브리지+ollama-direct leg를 어댑터 축에서 잠근다.
# quickjs-ng 인클루드 경로 — JKAgentJson.h가 <quickjs.h>를 들고 온다(링크 의존
# 아님: quickjs 기호 참조 없이 통과한다).
set -e

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SCRIPT_DIR"/../../.. && pwd)
OUT="$ROOT/engine/build/posix_selftest"

mkdir -p "$ROOT/engine/build"

QJS="$ROOT/engine/third_party/quickjs-ng"

# quickjs는 C 원문 소스 — C 컴파일러(cc)로 오브젝트를 뜯어 g++ 링크에 얹는다
# (엔진 CMake 쪽과 같은 C 라이브러리 대상 계약). 이 트리의 quickjs-ng에는
# cutils/xsum의 .c가 없다(헤더 쪽 소화) — 존재하는 .c만 링크 변수에 얹는다.
QJS_OBJS=""
cc -c -O1 -I"$QJS" -o "$ROOT/engine/build/posix_selftest_qjs.o" "$QJS/quickjs.c"
QJS_OBJS="$QJS_OBJS $ROOT/engine/build/posix_selftest_qjs.o"
for c in xsum dtoa libregexp libunicode cutils; do
    if [ -f "$QJS/$c.c" ]; then
        cc -c -O1 -I"$QJS" -o "$ROOT/engine/build/posix_selftest_$c.o" "$QJS/$c.c"
        QJS_OBJS="$QJS_OBJS $ROOT/engine/build/posix_selftest_$c.o"
    fi
done

g++ -std=c++20 -pthread -lutil -O1 -Wall -Wextra \
    -I"$ROOT/engine/include" \
    -I"$QJS" \
    -o "$OUT" \
    "$SCRIPT_DIR/main.cpp" \
    "$ROOT/engine/src/agent/JKLlmEngine.cpp" \
    "$ROOT/engine/src/agent/JKAgentJson.cpp" \
    "$ROOT/engine/src/fs/JKFs_posix.cpp" \
    "$ROOT/engine/src/process/JKProcess_posix.cpp" \
    "$ROOT/engine/src/net/JKNet_posix.cpp" \
    "$ROOT/engine/src/terminal/JKConPtyBridge_posix.cpp" \
    "$ROOT/engine/src/ipc/JKPipeTransport_posix.cpp" \
    "$ROOT/engine/src/fs/JKInstanceLock_posix.cpp" \
    "$ROOT/engine/src/text/JKTextConv_posix.cpp" \
    $QJS_OBJS

echo "posix_selftest build ok -> $OUT"
