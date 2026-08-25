@echo off
setlocal
set MSYSTEM=UCRT64
C:\msys64\usr\bin\bash.exe -lc "cd /i/progwork/JKENGINE/prototype/sdl2_jkwindow && rm -rf build && cmake -B build -G Ninja && cmake --build build && cp /ucrt64/bin/SDL2.dll /ucrt64/bin/libgcc_s_seh-1.dll /ucrt64/bin/libstdc++-6.dll /ucrt64/bin/libwinpthread-1.dll build/"
endlocal
