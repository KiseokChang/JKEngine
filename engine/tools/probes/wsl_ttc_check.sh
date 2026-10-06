#!/bin/bash
# docs/70 §8.4 판정 2 봉합 실측 — .ttc face-0 support (JKTextAtlas/JKGlyphAtlas)
# WSL 서버 부팅 → 클라 터미널 init → "vector font init failed" 소멸 확인.
# 표준: 셀프테스트 RC + 리졸버 폴백 대상(.ttc) 실존 확인.
set -u
cd /mnt/i/progwork/JKENGINE/engine

echo "=== resolver chain target exists ==="
ls -la /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc 2>&1 | head -1

echo "=== boot headless server ==="
pkill -f 'buildwsl/jkdesktop' 2>/dev/null
sleep 1
./buildwsl/jkdesktop --server >/tmp/ttc_srv.log 2>&1 &
SRV=$!
sleep 4
echo "server pid=$SRV"
head -3 /tmp/ttc_srv.log

echo "=== run client (captures stderr warning) ==="
timeout 8 ./buildwsl/jkdesktop --client terminal >/tmp/ttc_cli.out 2>/tmp/ttc_cli.err
echo "client rc=$?"
grep -aE "vector font init failed|no vector font|fallback font init failed" /tmp/ttc_cli.err && echo "WARN-STILL-PRESENT" || echo "WARN-GONE"
echo "--- client stderr head ---"
head -6 /tmp/ttc_cli.err

echo "=== selftest (font-atlas cases included) ==="
./buildwsl/jkdesktop test >/tmp/ttc_test.log 2>&1
grep -aE "failure|FAIL" /tmp/ttc_test.log | head -3

pkill -f 'buildwsl/jkdesktop' 2>/dev/null
echo "=== TTC-CHECK-END ==="