#!/bin/sh
# sampletodo - P4 SDK sample console app (specs/2026-09-16-p4-sdk-contract SS6).
# Posix twin of sampletodo.cmd (app-coverage Task 3) - same body, sh syntax.
# Spawn contract: the catalog console entry resolves this file as
# "terminal:apps/sampletodo/sampletodo.sh" (manifest "cmd_posix", posix only).
# posix spawns it through /bin/sh -c with cwd = exe dir (JKWindowServer
# SpawnProcess posix leg), so the path and the exec bit are load-bearing
# (drvfs/buildwsl trees keep the bit set).
# NOTE: keep this file ASCII-only - the console writes in the terminal
# codepage (docs/48 CP949 lesson); Korean text here would garble into broken
# lines.
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ $# -gt 0 ]; then
    "$here/../../jkctl" ask "Review this todo list and set priorities: $1"
    exit $?
fi
if ! cat "$here/todo.txt" 2>/dev/null; then
    echo "(todo.txt not found - write one item per line)"
fi
