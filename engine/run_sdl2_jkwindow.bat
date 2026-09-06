@echo off
setlocal
REM Remove the trailing backslash from %~dp0 so the quoted cd path is valid.
set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "EXE=%SCRIPT_DIR%\build\jkproto_sdl2_jkwindow.exe"

cd /d "%SCRIPT_DIR%\build"
if not exist "%EXE%" (
    echo ERROR: %EXE% not found. Run build_sdl2_jkwindow.bat first.
    exit /b 1
)

REM Usage: run_sdl2_jkwindow.bat [mode]
REM   (no args) : JKENGINE SDL2 prototype main demo
REM   jango     : WINDBASE/JANGO launcher port
REM   occ       : WINDBASE/2CAOCC C2 port (Phase 2: units + fire orders + timer)
REM   test      : data manager self test (includes OccUnit/OccFire managers)
if "%~1"=="" (
    "%EXE%"
) else (
    "%EXE%" %*
)
endlocal
