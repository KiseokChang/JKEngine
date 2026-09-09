# Chat MVP probe: ask-gated close -> approval via the approve tool -> resolved.
# The chat window is UI (eyeball item); this probe drives the wire:
#   launch_chat (SpawnProcess) -> close under ask (blocks) -> approval_request
#   event -> approve allow -> close resolves; then deny path -> window survives.
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process jkchat -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

# ask mode IS the approval act for this probe
$permFile = Join-Path (Split-Path $exe) "permissions.json"
'{"close_window":"ask"}' | Set-Content -Path $permFile -Encoding ASCII

Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 4

function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}

# launch_chat: the server spawns jkchat.exe (SpawnProcess path)
$chat = Invoke-Agentctl '{"tool":"launch_chat","args":{}}'
Start-Sleep -Seconds 2
$chatProc = Get-Process jkchat -ErrorAction SilentlyContinue
if ($chat -match '"ok\\?":true' -and $chatProc) { Write-Host "launch_chat: PASS" }
else { Write-Host "launch_chat: FAIL ($chat / proc=$($chatProc -ne $null))"; exit 1 }

$launch = Invoke-Agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}'
Start-Sleep -Seconds 2
$list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
$id = $null
if ($list -match '"id\\?":(\d+),"title":"Minesweeper"') { $id = $Matches[1] }
if (-not $id) { Write-Host "minesweeper: FAIL ($list)"; exit 1 }

# close under ask mode BLOCKS until approved — run as a job. The events
# subscription must exist BEFORE the close fires (pushes are not replayed):
# start the events job first and give it time to connect.
$evJob = Start-Job -ScriptBlock {
    param($e)
    (& $e agent-events 10) -join "`n"
} -ArgumentList $exe
Start-Sleep -Seconds 2
$closeLine = '{"tool":"close_window","args":{"id":' + $id + '}}'
$closeJob = Start-Job -ScriptBlock {
    param($e, $line)
    $esc = $line -replace '"', '\"'
    (& $e agentctl $esc) -join "`n"
} -ArgumentList $exe, $closeLine
Start-Sleep -Seconds 2

$approveLine = ""
$closeOut = ""
# poll for the approval request id in the event stream
$reqId = $null
foreach ($i in 1..10) {
    $events = Receive-Job -Job $evJob -ErrorAction SilentlyContinue | Out-String
    if ($events -match '"request\\?":(\d+)') { $reqId = $Matches[1]; break }
    Start-Sleep -Seconds 1
}
if ($reqId) { Write-Host "approval-request: PASS (request $reqId)" }
else { Write-Host "approval-request: FAIL"; Get-Process jkdesktop,jkchat -ErrorAction SilentlyContinue | Stop-Process -Force; exit 1 }

$approveLine = '{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}'
$approve = Invoke-Agentctl $approveLine
$closeOut = Receive-Job -Job $closeJob -Wait | Out-String
Start-Sleep -Seconds 2
$list2 = Invoke-Agentctl '{"tool":"list_windows","args":{}}'

$ok = $true
if ($approve -match 'approved\\?":true') { Write-Host "approve: PASS" }
else { $ok = $false; Write-Host "approve: FAIL $approve" }
if ($closeOut -match 'ok\\?":true') { Write-Host "close-resolved: PASS" }
else { $ok = $false; Write-Host "close-resolved: FAIL $closeOut" }
if ($list2 -notmatch ('"id\\?":' + $id + '\b')) { Write-Host "gone: PASS" }
else { $ok = $false; Write-Host "gone: FAIL" }

# deny path: tetris survives a denied close
$launch2 = Invoke-Agentctl '{"tool":"launch_app","args":{"app":"tetris"}}'
Start-Sleep -Seconds 2
$list3 = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
$tetId = $null
if ($list3 -match '"id\\?":(\d+),"title":"Tetris"') { $tetId = $Matches[1] }
$denyClose = '{"tool":"close_window","args":{"id":' + $tetId + '}}'
$evJob2 = Start-Job -ScriptBlock {
    param($e)
    (& $e agent-events 10) -join "`n"
} -ArgumentList $exe
Start-Sleep -Seconds 2
$denyJob = Start-Job -ScriptBlock {
    param($e, $line)
    $esc = $line -replace '"', '\"'
    (& $e agentctl $esc) -join "`n"
} -ArgumentList $exe, $denyClose
Start-Sleep -Seconds 2
$req2 = $null
foreach ($i in 1..10) {
    $events2 = Receive-Job -Job $evJob2 -ErrorAction SilentlyContinue | Out-String
    $ids = [regex]::Matches($events2, '"request\\?":(\d+)')
    if ($ids.Count -gt 0) { $req2 = $ids[$ids.Count - 1].Groups[1].Value; break }
    Start-Sleep -Seconds 1
}
$denyLine = '{"tool":"approve","args":{"request":' + $req2 + ',"decision":"deny"}}'
$deny = Invoke-Agentctl $denyLine
$denyOut = Receive-Job -Job $denyJob -Wait | Out-String
Start-Sleep -Seconds 1
$list4 = Invoke-Agentctl '{"tool":"list_windows","args":{}}'

if ($denyOut -match 'denied_by_user') { Write-Host "deny-result: PASS" }
else { $ok = $false; Write-Host "deny-result: FAIL $denyOut" }
if ($list4 -match ('"id\\?":' + $tetId + '\b')) { Write-Host "deny-survives: PASS" }
else { $ok = $false; Write-Host "deny-survives: FAIL" }

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process jkchat -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item $permFile -ErrorAction SilentlyContinue

if ($ok) { Write-Host "PASS: agent chat"; exit 0 } else { Write-Host "FAIL: agent chat"; exit 1 }