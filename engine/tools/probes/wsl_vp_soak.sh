#!/usr/bin/env bash
# vplayer WSL death-timing discriminator — keep ONE session attached for 240s
# and poll liveness every 10s. Death while attached = real crash/exit (catch
# the moment + final log). Alive whole time = death correlates with the
# spawner session exiting (detach cleanup).
set -u
B=/mnt/i/progwork/JKENGINE/engine/buildwsl
CLI=$B/jkctl
LOG=/tmp/vp_soak.log

pkill -9 -x jkdesktop 2>/dev/null
rm -f /tmp/JKWindowServerPipe.sock
sleep 1
(env DISPLAY=:0 $B/jkdesktop --server >/tmp/vp_srv.log 2>&1 &)
sleep 6
pgrep -ax jkdesktop > /tmp/vp_pids_boot.txt
echo "=== boot: $(date +%T)"; cat /tmp/vp_pids_boot.txt

$CLI agent '{"tool":"launch_app","args":{"app":"vplayer"}}' >/dev/null 2>&1
sleep 3
$CLI agent '{"tool":"app_tool","args":{"app":"vplayer","tool":"open","args":{"path":"/mnt/i/progwork/JKENGINE/tmp/vpt2_test.mp4"}}}' >/dev/null 2>&1

DEAD=0
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24; do
  sleep 10
  N=$(pgrep -x jkdesktop | wc -l)
  LAST=$(tail -1 /tmp/vp_srv.log)
  echo "t=$((i*10)) procs=$N last_log=$LAST"
  if [ "$N" -eq 0 ]; then
    DEAD=1
    echo "!!! ALL DEAD while attached at t=$((i*10)) $(date +%T)"
    echo "--- dmesg tail:"; dmesg 2>/dev/null | grep -E "fatal signal|CaptureCrash" | tail -6
    echo "--- srv log tail:"; tail -8 /tmp/vp_srv.log
    break
  fi
done
if [ "$DEAD" -eq 0 ]; then
  echo "=== SURVIVED 240s attached: $(date +%T)"
  pgrep -ax jkdesktop
fi