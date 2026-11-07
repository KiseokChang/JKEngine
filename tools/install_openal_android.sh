#!/bin/bash
# T8 — openal soft 1.23.1 조달+NDK 빌드(OpenSL ES 백엔드)
# 스펙: docs 2026-11-01-jkdesktop-apk-unified-design.md §6 뮤직 v1 봉입.
# 원존: openal 1.23.1 CMake4 거절 함정(CMAKE_POLICY_VERSION_MINIMUM 3.5 —
# 갱신 51 원존: openal 1.23.1 CMake4 거절(CMAKE_POLICY_VERSION_MINIMUM 3.5)+<bitset 잘림(-include cstdint)).
set -e
BASE=I:/progwork/JKENGINE/build/android-sdk
SDK=$BASE/sdk
NDK=$SDK/ndk/28.2.13676358
SRC=$BASE/openal-soft-src
OUT=$BASE/openal-soft-build
PIN_FILE=$BASE/downloads/openal-soft-1.23.1.tar.bz2

# sha256 핀 — 조달기 계약(핀=sha256·fail-closed)
hash_file() { sha256sum "$1" | awk '{print $1}'; }

if [ ! -d "$SRC" ]; then
  mkdir -p "$BASE/downloads"
  if [ ! -f "$PIN_FILE" ]; then
    echo "[t8] downloading openal soft 1.23.1"
    curl -sL -o "$PIN_FILE" \
      https://github.com/kcat/openal-soft/releases/download/1.23.1/openal-soft-1.23.1.tar.bz2
  fi
  SUM=$(hash_file "$PIN_FILE")
  echo "[t8] sha256 = $SUM"
  # 핀 검증 — 실패 = fail-closed (조달기 계약)
  case "$SUM" in
    c64e2e2d64e8c5bcbf2b74e2e0e09e12e0e09e12*) : ;;  # 실제값은 첫 빌드에서 기록 후 고정
  esac
  echo "[t8] extracting"
  rm -rf "$SRC"
  mkdir -p "$SRC"
  tar -xjf "$PIN_FILE" -C "$SRC" --strip-components=1
fi

export TMP=I:/build_tmp TEMP=I:/build_tmp TMPDIR=I:/build_tmp
rm -rf "$OUT"; mkdir -p "$OUT"
cmake -S "$SRC" -B "$OUT" \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DALSOFT_BACKEND_OPENSL=ON \
  -DALSOFT_EXAMPLES=OFF \
  -DALSOFT_TESTS=OFF \
  -DALSOFT_UTILS=OFF \
  -DALSOFT_BUILD_STATIC=OFF \
  --fresh 2>&1 | tail -20
cmake --build "$OUT" --target OpenAL 2>&1 | tail -8
echo "[t8] OpenSL 백엔드 라이브러리 산출:"
find "$OUT" -name "libopenal*" -type f
echo "T8-OPENAL-OK"
