@echo off
REM agent-notify - P4 SDK sample console app (jkctl agent-channel demo).
REM NOTE: keep this file ASCII-only - cmd reads .bat in the OEM codepage
REM (docs/48 CP949 lesson); Korean text here would garble into broken lines.
set HERE=%~dp0
set JKCTL=%HERE%..\..\jkctl.exe

echo === agent-notify: jkctl notify demo ===
"%JKCTL%" notify "agent-notify sample ran at %DATE% %TIME% - the chat window should surface this"
echo.
echo === agent-notify: jkctl ask demo ^(streams an LLM reply^) ===
"%JKCTL%" ask "In one sentence: what is the JKENGINE P4 SDK console app contract for?"
echo.
echo === agent-notify: jkctl agent raw tool-call demo ===
"%JKCTL%" agent "{\"tool\":\"list_windows\",\"args\":{}}"
echo.
echo done - all three agent-channel paths exercised.