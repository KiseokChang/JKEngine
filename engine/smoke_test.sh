#!/usr/bin/env bash
# 스모크 테스트: test 모드 + jango/occ 모드 5초 실행.
# [P0 수형 — docs/106 §1-7] rc 캡처·전파(1판 exit 0 소각).
export PATH=/usr/bin:/c/msys64/ucrt64/bin:$PATH
cd /i/progwork/JKENGINE/engine/build || exit 1

fail=0
echo "== self test =="
./jkdesktop.exe test
rc=$?
echo "selftest_exit=$rc"
[ $rc -ne 0 ] && fail=1

echo "== jango smoke =="
./jkdesktop.exe jango &
APP=$!
sleep 5
kill $APP 2>/dev/null
wait $APP 2>/dev/null
echo "jango_exit=$?"

echo "== occ smoke =="
./jkdesktop.exe occ &
APP=$!
sleep 5
kill $APP 2>/dev/null
wait $APP 2>/dev/null
echo "occ_exit=$?"

exit $fail
