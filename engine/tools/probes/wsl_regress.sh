#!/usr/bin/env bash
# docs/78 TX2 regression (WSL/glibc): rebuild buildwsl and run selftest.
# Run from wsl.exe: wsl.exe -- bash /mnt/i/progwork/JKENGINE/engine/tools/probes/wsl_regress.sh
set -u
LOG=/tmp/reg.log
: > "$LOG"
exec > "$LOG" 2>&1
cd /mnt/i/progwork/JKENGINE/engine
# [P0 수형 — docs/106 §1-7] 1판: $? 가 echo의 rc(항상 0)를 검사 — rc 캐처 후 전파.
ninja -C buildwsl -j3
NRC=$?
echo "WSL-BUILD-RC=$NRC"
fail=0
[ $NRC -ne 0 ] && fail=1
if [ $NRC -eq 0 ]; then
    set -o pipefail
    timeout 300 ./buildwsl/jkdesktop test 2>&1 | grep -E "AppSelfTest|^\[FAIL\]|^\[PASS\]" | grep -vE "^\[PASS\]" | tail -8
    TRC=${PIPESTATUS[0]}
    echo "WSL-TEST-RC=$TRC"
    [ $TRC -ne 0 ] && fail=1
fi
echo "=== wsl_regress end fail=$fail"
exit $fail