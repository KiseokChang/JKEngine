#!/usr/bin/env bash
set -e

export PATH=/c/msys64/ucrt64/bin:$PATH

SRC=/i/progwork/JKENGINE/engine
TEMP=/c/temp_jkdesktop

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
cp -f "$TEMP/build/jkdesktop.exe" "$SRC/build/"
# jkwinserver.exe도 복사 (docs/44: 빌드 bat이 desktop만 복사해 서버 exe가
# temp에만 남는 갭 — docs/43 P1 split으로 서버가 필수 실행 파일이 됨).
cp -f "$TEMP/build/jkwinserver.exe" "$SRC/build/"
# 클라이언트 모듈 DLL도 복사 — 새 모듈(jkapp_taskbar 등)이 temp에만 남아
# "no jkapp_taskbar.dll — desktop runs without a shell"로 뜨는 갭 (docs/44).
for dll in "$TEMP"/build/jkapp_*.dll; do
    [ -e "$dll" ] && cp -f "$dll" "$SRC/build/"
done
# 도구 exe도 복사 — jkagentd/jkchat/jktriggers/jkctl(P4 SDK, docs/51)이
# temp에만 남으면 실행 환경에서 agent 채널/jkctl이 사라진다.
for exe in jkagentd jkchat jktriggers jkctl; do
    [ -e "$TEMP/build/$exe.exe" ] && cp -f "$TEMP/build/$exe.exe" "$SRC/build/"
done
# assets 폴더도 build 디렉터리에 동기화하여 실행 파일이 단독으로 리소스를 찾을 수 있게 한다.
if [ -d "$SRC/assets" ]; then
    cp -R "$SRC/assets" "$SRC/build/"
fi
# 콘솔 앱 폴더(kind) 동기화 (P4 SDK): apps/<dir>/manifest.json 앱을 실행
# 폴더로 복사 — .jkx 패커 산출물과 같은 자리. 빈 폴더/더미는 스킵.
if [ -d "$SRC/apps" ]; then
    for d in "$SRC"/apps/*/; do
        [ -d "$d" ] && cp -R "$d" "$SRC/build/apps/"
    done
fi
# jkctl init 템플릿 동기화 (docs/51 C 후보): jkctl은 exe 옆 templates\
# console-app에서 읽는다. `/.` 복사(중첩 방지 — docs/51 부록 레슨).
if [ -d "$SRC/templates/console-app" ]; then
    mkdir -p "$SRC/build/templates"
    cp -R "$SRC/templates/console-app/." "$SRC/build/templates/console-app/"
fi
echo "Build succeeded. Output copied to $SRC/build/jkdesktop.exe"
