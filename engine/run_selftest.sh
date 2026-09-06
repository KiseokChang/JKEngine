#!/usr/bin/env bash
# 헤드리스 자기 테스트 실행: "test" 모드.
export PATH=/usr/bin:/c/msys64/ucrt64/bin:$PATH
cd /i/progwork/JKENGINE/engine/build || exit 1
./jkdesktop.exe test
echo "selftest_exit=$?"
exit 0