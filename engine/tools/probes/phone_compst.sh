#!/bin/bash
# 폰 합성 분해 계측 ([compst]) — docs/78 TX7 잔여 성질 규명
cd ~/JKENGINE/engine
ninja -C buildterm -j4 >~/tmp/rb3.log 2>&1
echo "build rc=$?"; tail -1 ~/tmp/rb3.log
pkill -f buildterm/jkdesktop 2>/dev/null; sleep 1
env DISPLAY=:1 JK_CPU_TRACE=1 setsid nohup ./buildterm/jkdesktop --server >~/tmp/srvz.log 2>&1 &
sleep 6
timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"terminal"}}' >~/tmp/lt.log 2>&1
echo "terminal rc=$?"
timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"jkx":"phoneprobe"}}' >~/tmp/lp.log 2>&1
echo "probe rc=$?"
sleep 12
echo "--- compst (phone idle) ---"
grep -a compst ~/tmp/srvz.log | tail -6
echo "--- cpustat (phone idle) ---"
grep -a 'cpustat] sdl' ~/tmp/srvz.log | tail -4
echo "=== PHTX7C-END ==="