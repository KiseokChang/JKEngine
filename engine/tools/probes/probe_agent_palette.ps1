# M2a probe: the command palette is a window client on the agent platform.
# 1. list_windows through the agent API shows the palette.
# 2. close_window is denied by the M2a SERVER-side gate (agentctl bypasses
#    the broker entirely, so this exercises JKWindowServer::AgentToolAllowed).
# 3. With permissions.json granting close, the palette itself closes and
#    leaves the list (the same cycle the Alt+Space palette serves a human).
# PASS/FAIL via exit code.
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 4

# Alt+Space spawns exactly this: --client palette.
Start-Process -FilePath $exe -ArgumentList "--client","palette" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 3

function Invoke-Agentctl([string]$json) {
    # PowerShell 5.1 mangles embedded quotes in native-exe args — escape them
    # as \" so the argv parser rebuilds the JSON intact.
    $escaped = $json -replace '"', '\"'
    $out = & $exe agentctl $escaped
    return ($out -join "`n")
}

$list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
$id = $null
if ($list -match '"id\\?":(\d+),"title":"Command Palette"') { $id = $Matches[1] }

# Default deny (no permissions.json): the SERVER gate answers.
# NOTE: build concatenated JSON in a variable first — inside a command's
# argument list PowerShell treats 'a' + $b + 'c' as three separate tokens.
$denyLine = '{"tool":"close_window","args":{"id":' + $id + '}}'
$deny = Invoke-Agentctl $denyLine

# Grant close, then close the palette itself and verify it is gone.
$permFile = Join-Path (Split-Path $exe) "permissions.json"
'{"close_window":"allow"}' | Set-Content -Path $permFile -Encoding ASCII
$close = Invoke-Agentctl $denyLine
Start-Sleep -Seconds 2
$list2 = Invoke-Agentctl '{"tool":"list_windows","args":{}}'

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item $permFile -ErrorAction SilentlyContinue   # back to default deny

$ok = $true
if ($id -and $list -match 'Command Palette') { Write-Host "palette-in-list: PASS" }
else { $ok = $false; Write-Host "palette-in-list: FAIL $list" }
if ($deny -match 'permission_denied') { Write-Host "server-gate-deny: PASS" }
else { $ok = $false; Write-Host "server-gate-deny: FAIL $deny" }
if ($close -match 'ok\\?":true')      { Write-Host "close-allowed: PASS" }
else { $ok = $false; Write-Host "close-allowed: FAIL $close" }
if ($id -and $list2 -notmatch ('"id\\?":' + $id + '\b')) { Write-Host "gone: PASS" }
else { $ok = $false; Write-Host "gone: FAIL (palette still listed)" }

if ($ok) { Write-Host "PASS: agent palette"; exit 0 }
else { Write-Host "FAIL: agent palette"; exit 1 }