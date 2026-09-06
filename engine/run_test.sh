#!/usr/bin/env bash
cd /i/progwork/JKENGINE/prototype/sdl2_jkwindow/build
./jkproto_sdl2_jkwindow.exe > run.log 2>&1 &
PID=$!
sleep 3
kill $PID 2>/dev/null
cat run.log
