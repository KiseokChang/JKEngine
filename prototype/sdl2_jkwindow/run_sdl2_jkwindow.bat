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

"%EXE%"
endlocal
