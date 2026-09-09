# Desktop Agent MCP E2E (spec 6.1) — server up, MCP tool calls through
# jkagentd, permission gate + receipts verified. PASS/FAIL via exit code.
# Judged on JSON responses (no pixel diffs — control-plane only).
$ErrorActionPreference = "Continue"
$exe  = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$agnt = "I:\progwork\JKENGINE\engine\build\jkagentd.exe"

# Fresh server (kill leftovers first).
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 4

function Invoke-Mcp([string]$line) {
    # One jkagentd process per call — the broker is stateless between calls
    # except the event queue (not exercised here; probe_agent_e2e covers it).
    $out = $line | & $agnt
    return ($out -join "`n")
}

$r1 = Invoke-Mcp '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"launch_app","arguments":{"app":"minesweeper"}}}'
Start-Sleep -Seconds 2
$r2 = Invoke-Mcp '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"list_windows","arguments":{}}}'
$r3 = Invoke-Mcp '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"save_layout","arguments":{"name":"probe_e2e"}}}'
$r4 = Invoke-Mcp '{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"close_window","arguments":{"id":999}}}'

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force

$ok = $true
# Tool results ride INSIDE the MCP envelope as content[0].text, so quotes are
# backslash-escaped in the wire text: \"ok\":true — match that form.
if ($r1 -match 'ok\\":true')         { Write-Host "launch: PASS" }  else { $ok = $false; Write-Host "launch: FAIL $r1" }
if ($r2 -match 'Minesweeper')        { Write-Host "list: PASS" }    else { $ok = $false; Write-Host "list: FAIL $r2" }
if ($r3 -match 'ok\\":true')         { Write-Host "save: PASS" }    else { $ok = $false; Write-Host "save: FAIL $r3" }
if ($r4 -match 'permission_denied')  { Write-Host "deny: PASS" }    else { $ok = $false; Write-Host "deny: FAIL $r4" }

$receipts = Join-Path (Split-Path $exe) "state\receipts.jsonl"
if ((Test-Path $receipts) -and ((Get-Content $receipts).Count -ge 4)) {
    Write-Host "receipts: PASS"
} else {
    $ok = $false; Write-Host "receipts: FAIL (missing or <4 records)"
}

if ($ok) { Write-Host "PASS: agent mcp e2e"; exit 0 } else { Write-Host "FAIL: agent mcp e2e"; exit 1 }