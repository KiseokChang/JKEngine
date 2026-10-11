#!/usr/bin/env bash
# detach-kill workaround test — setsid new-session boot, then the spawning
# session exits immediately; a FOLLOWING wsl.exe call checks survival.
set -u
B=/mnt/i/progwork/JKENGINE/engine/buildwsl
pkill -9 -x jkdesktop 2>/dev/null
# #98 pre-clean guard: rm on a LIVE server's socket entombs its parked
# listener (client connects land ENOENT) — skip and note.
if pgrep -f '[j]kdesktop --server' >/dev/null 2>&1; then
    echo "NOTE #98: live server holds /tmp/JKWindowServerPipe.sock — pre-clean rm skipped"
else
    rm -f /tmp/JKWindowServerPipe.sock
fi
sleep 1
(env DISPLAY=:0 setsid $B/jkdesktop --server >/tmp/vp_srv.log 2>&1 &)
sleep 6
pgrep -ax jkdesktop
echo "=== boot done $(date +%T) — this session now exits"