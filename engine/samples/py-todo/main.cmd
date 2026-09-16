@echo off
REM py-todo - P4 SDK sample console app (Python).
REM NOTE: keep this file ASCII-only - cmd reads .bat in the OEM codepage
REM (docs/48 CP949 lesson); Korean text here would garble into broken lines.
set HERE=%~dp0
where py >nul 2>nul
if not errorlevel 1 (
    py -3 "%HERE%todo.py" %*
    goto :eof
)
where python >nul 2>nul
if not errorlevel 1 (
    python "%HERE%todo.py" %*
    goto :eof
)
echo Python not found - install Python 3 or edit main.cmd to point at it.