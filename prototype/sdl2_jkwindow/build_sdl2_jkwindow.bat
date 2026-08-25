@echo off
setlocal
REM Work around MinGW being unable to write object files directly to I: drive.
REM Sync sources to C:, build there, then copy the resulting exe back.
C:\msys64\usr\bin\bash.exe -l "%~dp0build_with_temp.sh"
if errorlevel 1 exit /b 1
echo Build succeeded.
endlocal
