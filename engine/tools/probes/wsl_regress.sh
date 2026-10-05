#!/usr/bin/env bash
# docs/78 TX2 regression (WSL/glibc): rebuild buildwsl and run selftest.
# Run from wsl.exe: wsl.exe -- bash /mnt/i/progwork/JKENGINE/engine/tools/probes/wsl_regress.sh
set -u
LOG=/tmp/reg.log
: > "$LOG"
exec > "$LOG" 2>&1
cd /mnt/i/progwork/JKENGINE/engine
ninja -C buildwsl -j3
echo "WSL-BUILD-RC=$?"
if [ $? -eq 0 ]; then
    timeout 300 ./buildwsl/jkdesktop test 2>&1 | grep -E "AppSelfTest|^\[FAIL\]|^\[PASS\]" | grep -vE "^\[PASS\]" | tail -8
fi
echo "=== wsl_regress end"
exit 0