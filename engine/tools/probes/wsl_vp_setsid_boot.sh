#!/usr/bin/env bash
# detach-kill workaround test — setsid new-session boot, then the spawning
# session exits immediately; a FOLLOWING wsl.exe call checks survival.
set -u
B=/mnt/i/progwork/JKENGINE/engine/buildwsl

# #98 runner guard (park 4 — ENTRY 확장, taskmgr 수형): this runner hard-pkills
# every jkdesktop (-9 by name) and rm-unlinks the desktop socket path before
# anything asserts — against a LIVE server that is probe-foreign (the user's
# real session), that kills an unrelated desktop. Abort the whole runner
# honestly instead; the pkill below only runs when no live server was up.
if pgrep -f '[j]kdesktop --server' >/dev/null 2>&1; then
    echo "[SKIP] vp-setsid-boot: a live jkdesktop server is running — #98 runner guard (nothing pkilled, nothing rm'd)"
    exit 0
fi

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