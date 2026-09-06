#!/usr/bin/env bash
# 스모크 테스트: test 모드 + jango/occ 모드 5초 실행.
export PATH=/usr/bin:/c/msys64/ucrt64/bin:$PATH
cd /i/progwork/JKENGINE/prototype/sdl2_jkwindow/build || exit 1

echo "== self test =="
./jkproto_sdl2_jkwindow.exe test
echo "selftest_exit=$?"

echo "== jango smoke =="
./jkproto_sdl2_jkwindow.exe jango &
APP=$!
sleep 5
kill $APP 2>/dev/null
wait $APP 2>/dev/null
echo "jango_exit=$?"

echo "== occ smoke =="
./jkproto_sdl2_jkwindow.exe occ &
APP=$!
sleep 5
kill $APP 2>/dev/null
wait $APP 2>/dev/null
echo "occ_exit=$?"
exit 0