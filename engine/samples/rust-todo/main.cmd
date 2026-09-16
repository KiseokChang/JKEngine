@echo off
REM rust-todo - P4 SDK sample console app (Rust).
REM NOTE: keep this file ASCII-only - cmd reads .bat in the OEM codepage
REM (docs/48 CP949 lesson); Korean text here would garble into broken lines.
set HERE=%~dp0
if exist "%HERE%target\release\rust-todo.exe" goto :run
where cargo >nul 2>nul
if errorlevel 1 (
    echo Rust toolchain not found - install cargo or edit main.cmd.
    exit /b 1
)
echo building rust-todo ^(first run, one-time^)...
pushd "%HERE%"
cargo build --release
set RC=%ERRORLEVEL%
popd
if not %RC%==0 exit /b %RC%
:run
"%HERE%target\release\rust-todo.exe" %*