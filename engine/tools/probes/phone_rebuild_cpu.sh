#!/bin/bash
# 폰 재빌드+셀프테스트+부팅 (docs/78 TX7 CPU 소등 재배포)
# 표준: exe=~/JKENGINE/engine/buildterm/jkdesktop, 로그 리다이렉트는 ~/tmp/
cd ~/JKENGINE/engine
ninja -C buildterm -j4 >/tmp/rb.log 2>&1 || ninja -C buildterm -j4 >>/tmp/rb.log 2>&1
echo "build rc=$?"
tail -3 /tmp/rb.log
./buildterm/jkdesktop test >~/tmp/phone_test.log 2>&1
echo "selftest rc=$?"
grep -aE 'failure' ~/tmp/phone_test.log | head -2
pkill -f buildterm/jkdesktop 2>/dev/null; sleep 1
echo "=== RB-END ==="