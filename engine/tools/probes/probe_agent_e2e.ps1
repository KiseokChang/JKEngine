# Desktop Agent M1 E2E probe (spec 6.1 완료 조건): the full conquest cycle —
# launch → observe → snapshot → close → verify gone → restore → receipts.
# PASS/FAIL via exit code.
$ErrorActionPreference = "Continue"
$exe  = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$agnt = "I:\progwork\JKENGINE\engine\build\jkagentd.exe"

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

# M1 approval model: there is no approval UI, so permissions.json next to the
# broker IS the approval act. Grant close_window for the close step, restore
# the default (deny) at the end.
$permFile = Join-Path (Split-Path $exe) "permissions.json"
'{"close_window":"allow"}' | Set-Content -Path $permFile -Encoding ASCII

Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 4

function Invoke-Mcp([string]$line) {
    $out = $line | & $agnt
    return ($out -join "`n")
}

# 1. launch (MCP, not --client — the agent path IS the tested path)
$r1 = Invoke-Mcp '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"launch_app","arguments":{"app":"minesweeper"}}}'
Start-Sleep -Seconds 2

# 2. observe
$r2 = Invoke-Mcp '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"list_windows","arguments":{}}}'
$id = $null
if ($r2 -match '"id\\?":(\d+)') { $id = $Matches[1] }

# 3. snapshot
$r3 = Invoke-Mcp '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"save_layout","arguments":{"name":"e2e_final"}}}'

# 4. close + verify gone
$closeLine = '{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"close_window","arguments":{"id":' + $id + '}}}'
$r4 = Invoke-Mcp $closeLine
Start-Sleep -Seconds 2
$r5 = Invoke-Mcp '{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"list_windows","arguments":{}}}'

# 5. restore (window is gone — expect ok with the title unmatched)
$r6 = Invoke-Mcp '{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"restore_layout","arguments":{"name":"e2e_final"}}}'

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item $permFile -ErrorAction SilentlyContinue   # back to default deny

$ok = $true
# Tool results ride inside the envelope, quotes escaped: \"ok\":true.
if ($r1 -match 'ok\\":true')            { Write-Host "launch: PASS" }   else { $ok = $false; Write-Host "launch: FAIL $r1" }
if ($r2 -match 'Minesweeper')           { Write-Host "list: PASS" }     else { $ok = $false; Write-Host "list: FAIL $r2" }
if ($r3 -match 'ok\\":true')            { Write-Host "save: PASS" }     else { $ok = $false; Write-Host "save: FAIL $r3" }
if ($r4 -match 'ok\\":true')            { Write-Host "close: PASS" }    else { $ok = $false; Write-Host "close: FAIL $r4" }
if ($id -and $r5 -notmatch ('\"id\\?":' + $id + '\b')) {
    Write-Host "gone: PASS"
} else { $ok = $false; Write-Host "gone: FAIL (id $id still listed)" }
if ($r6 -match 'ok\\":true')            { Write-Host "restore: PASS" }  else { $ok = $false; Write-Host "restore: FAIL $r6" }

$receipts = Join-Path (Split-Path $exe) "state\receipts.jsonl"
if ((Test-Path $receipts) -and ((Get-Content $receipts).Count -ge 5)) {
    Write-Host "receipts: PASS"
} else {
    $ok = $false; Write-Host "receipts: FAIL (missing or <5 records)"
}

if ($ok) { Write-Host "PASS: agent e2e final"; exit 0 } else { Write-Host "FAIL: agent e2e final"; exit 1 }