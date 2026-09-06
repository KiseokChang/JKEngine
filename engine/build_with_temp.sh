#!/usr/bin/env bash
set -e

export PATH=/c/msys64/ucrt64/bin:$PATH

SRC=/i/progwork/JKENGINE/prototype/sdl2_jkwindow
TEMP=/c/temp_jkproto_sdl2_jkwindow

# 1. 소스를 C: 드라이브로 동기화 (MinGW가 I: 드라이브에 쓰지 못하는 문제 회피)
rm -rf "$TEMP"
mkdir -p "$TEMP"
cp -R "$SRC"/. "$TEMP"/
# 원래 build 디렉터리는 임시 빌드를 위해 제거
rm -rf "$TEMP/build"

# 2. configure (point back at the real JKENGINE root for shared legacy sources)
mkdir -p "$TEMP/build"
cd "$TEMP/build"
cmake .. -G Ninja -DJKENGINE_ROOT=/i/progwork/JKENGINE

# 3. build
ninja

# 4. 결과물을 원래 build 디렉터리로 복사
cp -f "$TEMP/build/jkproto_sdl2_jkwindow.exe" "$SRC/build/"
# assets 폴더도 build 디렉터리에 동기화하여 실행 파일이 단독으로 리소스를 찾을 수 있게 한다.
if [ -d "$SRC/assets" ]; then
    cp -R "$SRC/assets" "$SRC/build/"
fi
echo "Build succeeded. Output copied to $SRC/build/jkproto_sdl2_jkwindow.exe"
