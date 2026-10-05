#!/usr/bin/env bash
# detach-kill workaround test — setsid new-session boot, then the spawning
# session exits immediately; a FOLLOWING wsl.exe call checks survival.
set -u
B=/mnt/i/progwork/JKENGINE/engine/buildwsl
pkill -9 -x jkdesktop 2>/dev/null
rm -f /tmp/JKWindowServerPipe.sock
sleep 1
(env DISPLAY=:0 setsid $B/jkdesktop --server >/tmp/vp_srv.log 2>&1 &)
sleep 6
pgrep -ax jkdesktop
echo "=== boot done $(date +%T) — this session now exits"