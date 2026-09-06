#!/usr/bin/env bash
# OCC Phase 2 smoke: run occ mode 5 s with sample OCCDATA.DAT/OCCUNIT.DAT.
export PATH=/usr/bin:/c/msys64/ucrt64/bin:$PATH
cd /i/progwork/JKENGINE/prototype/sdl2_jkwindow/build || exit 1

echo "== occ smoke (with data) =="
./jkproto_sdl2_jkwindow.exe occ &
APP=$!
sleep 5
kill $APP 2>/dev/null
wait $APP 2>/dev/null
echo "occ_exit=$?"
echo "OCCDATA.DAT exists: $(test -f OCCDATA.DAT && echo yes || echo no)"
echo "OCCUNIT.DAT exists: $(test -f OCCUNIT.DAT && echo yes || echo no)"
exit 0