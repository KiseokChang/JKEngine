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
# T3 (2026-10-08): agent TU 2건 추가(JKLlmEngine/JKAgentJson) — case 15가
# 직 링크로 TurnSync 동기 브리지+ollama-direct leg를 어댑터 축에서 잠근다.
# quickjs-ng 인클루드 경로 — JKAgentJson.h가 <quickjs.h>를 들고 온다(링크 의존
# 아님: quickjs 기호 참조 없이 통과한다).
# T4 (2026-10-08): ChatRouter.cpp 추가 — case 16(자연어 승격 배선)이
# kRouteTable 단일 진실원에서 승격 프롬프트를 조립하고 배선 순서(트리거
# 즉발→TurnSync→stub 폴백)를 sh 스폰 스터브로 잠근다.
# T1 (2026-10-08): JKFrameDirty.cpp 추가 — case 17(프레임 더티 계산기)가
# 렌더러 없이 매핑·합집합·역치를 단정한다. SDL 접촉은 SDL_Rect 타입뿐(링크
# 심볼 없음)이라 잉크는 cflags(헤더 경로)만이고, pkg-config가 있으면 쓰고
# 없으면 데비안 기본 경로로 떨어진다.
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

SDL_CFLAGS=""
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2; then
    SDL_CFLAGS=$(pkg-config --cflags sdl2)
else
    SDL_CFLAGS="-I/usr/include/SDL2"
fi

g++ -std=c++20 -pthread -lutil -O1 -Wall -Wextra \
    -I"$ROOT/engine/include" \
    -I"$QJS" \
    $SDL_CFLAGS \
    -o "$OUT" \
    "$SCRIPT_DIR/main.cpp" \
    "$ROOT/engine/src/apps/ChatRouter.cpp" \
    "$ROOT/engine/src/server/JKFrameDirty.cpp" \
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
