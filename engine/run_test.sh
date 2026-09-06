#!/usr/bin/env bash
cd /i/progwork/JKENGINE/engine/build
./jkdesktop.exe > run.log 2>&1 &
PID=$!
sleep 3
kill $PID 2>/dev/null
cat run.log
