#!/bin/bash
# 폰 SW 렌더러 강제 후 합성/CPU 재측정 — docs/78 TX7 잔여
cd ~/JKENGINE/engine
ninja -C buildterm -j4 >~/tmp/rb4.log 2>&1
echo "build rc=$?"; tail -1 ~/tmp/rb4.log
./buildterm/jkdesktop test >~/tmp/ph4_test.log 2>&1
echo "selftest rc=$?"; grep -aE 'failure' ~/tmp/ph4_test.log | head -1
pkill -f buildterm/jkdesktop 2>/dev/null; sleep 1
env DISPLAY=:1 JK_CPU_TRACE=1 setsid nohup ./buildterm/jkdesktop --server >~/tmp/srvy.log 2>&1 &
sleep 6
renderer=$(grep -a 'renderer' ~/tmp/srvy.log | head -2)
echo "renderer log: [$renderer]"
timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"terminal"}}' >~/tmp/lt.log 2>&1
echo "terminal rc=$?"
timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"jkx":"phoneprobe"}}' >~/tmp/lp.log 2>&1
echo "probe rc=$?"
sleep 12
echo "--- compst (phone SW) ---"
grep -a compst ~/tmp/srvy.log | tail -6
echo "--- cpustat (phone SW) ---"
grep -a 'cpustat] sdl' ~/tmp/srvy.log | tail -3
echo "--- cpu delta 5s each ---"
for pid in $(pgrep -f 'buildterm/jkdesktop'); do
  cmd=$(tr '\0' ' ' < /proc/$pid/cmdline 2>/dev/null)
  case "$cmd" in *"sh -c"*) continue;; esac
  [ -z "$cmd" ] && continue
  t1=$(awk '{print $14+$15}' /proc/$pid/stat 2>/dev/null); [ -z "$t1" ] && continue
  sleep 5
  t2=$(awk '{print $14+$15}' /proc/$pid/stat 2>/dev/null); [ -z "$t2" ] && continue
  awk -v d=$((t2-t1)) -v c="$cmd" 'BEGIN{printf "%5.1f%% %s\n", d/500*100, c}'
done
pk=$(pgrep -f 'buildterm/jkdesktop --server' | head -1)
kill -0 $pk 2>/dev/null && echo "server alive pid=$pk"
echo "=== PHTX7D-END ==="