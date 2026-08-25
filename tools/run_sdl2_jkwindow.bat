@echo off
setlocal
set MSYSTEM=UCRT64
set EXE=I:\progwork\JKENGINE\prototype\sdl2_jkwindow\build\jkproto_sdl2_jkwindow.exe

if not exist "%EXE%" (
    echo [ERROR] Executable not found: %EXE%
    echo [HINT] Run build_sdl2_jkwindow.bat first.
    exit /b 1
)

echo [INFO] Launching SDL2 JKWINDOW prototype...
echo [INFO] Executable: %EXE%

start "JKENGINE SDL2 Prototype" /D "I:\progwork\JKENGINE\prototype\sdl2_jkwindow" "%EXE%"
set PID=
for /f "tokens=2 delims=," %%a in ('tasklist /fi "imagename eq jkproto_sdl2_jkwindow.exe" /fo csv /nh') do set PID=%%a

if defined PID (
    echo [INFO] Process started with PID %PID%.
    echo [INFO] Waiting 5 seconds to verify the window/event loop stays alive...
    timeout /t 5 /nobreak >nul
    tasklist /fi "imagename eq jkproto_sdl2_jkwindow.exe" /fo csv /nh >nul 2>&1
    if not errorlevel 1 (
        echo [OK] Prototype is running successfully. Closing test instance...
        taskkill /im jkproto_sdl2_jkwindow.exe /f >nul 2>&1
    ) else (
        echo [WARN] Prototype process ended unexpectedly.
    )
) else (
    echo [WARN] Could not determine process PID, but the executable was launched.
)

endlocal