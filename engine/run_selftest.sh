#!/usr/bin/env bash
# 헤드리스 자기 테스트 실행: "test" 모드.
# [P0 수형 — docs/106 §1-7] 1판 = echo 후 exit 0(실패가 파이프라인 밖으로 삼킴)
# → rc 캡처 후 그대로 전파(정직 종료코드 계약).
export PATH=/usr/bin:/c/msys64/ucrt64/bin:$PATH
cd /i/progwork/JKENGINE/engine/build || exit 1
./jkdesktop.exe test
rc=$?
echo "selftest_exit=$rc"
exit $rc
