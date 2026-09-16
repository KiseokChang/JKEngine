@echo off
REM go-todo - P4 SDK sample console app (Go).
REM NOTE: keep this file ASCII-only - cmd reads .bat in the OEM codepage
REM (docs/48 CP949 lesson); Korean text here would garble into broken lines.
set HERE=%~dp0
if exist "%HERE%go-todo.exe" goto :run
where go >nul 2>nul
if errorlevel 1 (
    echo Go toolchain not found - install go or edit main.cmd.
    exit /b 1
)
echo building go-todo ^(first run, one-time^)...
pushd "%HERE%"
go build -o go-todo.exe todo.go
set RC=%ERRORLEVEL%
popd
if not %RC%==0 exit /b %RC%
:run
"%HERE%go-todo.exe" %*