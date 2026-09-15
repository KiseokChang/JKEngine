@echo off
REM sampletodo - P4 SDK sample console app (specs/2026-09-16-p4-sdk-contract SS6).
REM NOTE: keep this file ASCII-only - cmd reads .bat in the OEM codepage
REM (docs/48 CP949 lesson); Korean text here would garble into broken lines.
set HERE=%~dp0
if "%~1"=="" goto :list
"%HERE%..\..\jkctl.exe" ask "Review this todo list and set priorities: %~1"
goto :eof
:list
type "%~dp0todo.txt" 2>nul
if errorlevel 1 echo (todo.txt not found - write one item per line)