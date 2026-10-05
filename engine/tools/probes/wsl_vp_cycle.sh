#!/usr/bin/env bash
# vplayer WSL end-of-clip crash reproduction — controlled cycle (docs/70 §6 #6).
# Boot server+taskbar -> launch vplayer -> open 30s engine-owned clip ->
# poll status every 3s; on death or ended:true, dump dmesg + server log.
set -u
B=/mnt/i/progwork/JKENGINE/engine/buildwsl
CLI=$B/jkctl
LOG=/tmp/vp_cycle.log
CLIP=/mnt/i/progwork/JKENGINE/tmp/vpt2_test.mp4

echo "=== cycle start $(date +%T)"
ls -la "$CLIP" || { echo "NO CLIP"; exit 1; }

# 1. clean boot (docs/70 §5 standard)
pkill -9 -x jkdesktop 2>/dev/null
rm -f /tmp/JKWindowServerPipe.sock
sleep 1
(env DISPLAY=:0 $B/jkdesktop --server >/tmp/vp_srv.log 2>&1 &)
sleep 6
echo "--- boot:"
pgrep -ax jkdesktop > /tmp/vp_pids_boot.txt
cat /tmp/vp_pids_boot.txt

# 2. launch vplayer
echo "--- launch_app:"
$CLI agent '{"tool":"launch_app","args":{"app":"vplayer"}}' | head -c 300
echo
sleep 3
pgrep -ax jkdesktop

# 3. open clip
echo "--- open:"
$CLI agent '{"tool":"app_tool","args":{"app":"vplayer","tool":"open","args":{"path":"/mnt/i/progwork/JKENGINE/tmp/vpt2_test.mp4"}}}' | head -c 300
echo

# 4. poll every 3s up to 60s
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
  sleep 3
  S=$($CLI agent '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}' 2>&1 | head -c 220)
  N=$(pgrep -x jkdesktop | wc -l)
  echo "t=$((i*3)) procs=$N status=$S"
  case "$S" in
    *'"ended":1'*|*'"ended": 1'*)
      echo "=== ENDED RECEIPT at t=$((i*3))"
      break
      ;;
  esac
  if [ "$N" -eq 0 ]; then
    echo "!!! ALL DEAD at t=$((i*3))"
    break
  fi
done

echo "--- final procs:"
pgrep -ax jkdesktop
echo "--- dmesg crash records:"
dmesg 2>/dev/null | grep -E "fatal signal|CaptureCrash" | tail -8
echo "--- server log tail:"
tail -25 /tmp/vp_srv.log
echo "=== cycle end $(date +%T)"