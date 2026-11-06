$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$trig = "I:\progwork\JKENGINE\engine\build\jktriggers.exe"
Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
$root = Split-Path $exe
$permFile = Join-Path $root "permissions.json"
if (Test-Path $permFile) { Write-Host "t2: ABORT - permissions.json exists"; exit 1 }
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
Start-Sleep -Seconds 3
Start-Process -FilePath $trig -WorkingDirectory (Split-Path $trig) -WindowStyle Hidden
Start-Sleep -Seconds 3
function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}
# parked query blocks - separate job
$job = Start-Job -ScriptBlock {
    param($exe)
    $escaped = '{"tool":"permission_set","args":{"tool":"close_window","decision":"allow"}}' -replace '"', '\"'
    & $exe agentctl $escaped
} -ArgumentList $exe
Start-Sleep -Seconds 3
# approval (request 1 = first park after server start; nextApprovalId_=1)
$ap = Invoke-Agentctl '{"tool":"approve","args":{"request":1,"decision":"allow"}}'
Start-Sleep -Seconds 2
$reply = Receive-Job $job -Wait
if ($reply -match '"written":true') { Write-Host "t2-reply: PASS" }
else { Write-Host "t2-reply: FAIL $reply" }
$file = Get-Content $permFile -Raw -ErrorAction SilentlyContinue
if ($file -match '"close_window":"allow"' -and $file -match '"trust_request"') {
    Write-Host "t2-file: PASS"
} else { Write-Host "t2-file: FAIL $file" }
# validation without parking: unknown_tool / bad_decision
$bad = Invoke-Agentctl '{"tool":"permission_set","args":{"tool":"nope","decision":"allow"}}'
if ($bad -match 'unknown_tool') { Write-Host "t2-unknown: PASS" } else { Write-Host "t2-unknown: FAIL $bad" }
$bad2 = Invoke-Agentctl '{"tool":"permission_set","args":{"tool":"close_window","decision":"maybe"}}'
if ($bad2 -match 'bad_decision') { Write-Host "t2-baddec: PASS" } else { Write-Host "t2-baddec: FAIL $bad2" }
# fixed-ask regression: file says allow, gate must still park (not allow)
$job2 = Start-Job -ScriptBlock {
    param($exe)
    $escaped = '{"tool":"permission_set","args":{"tool":"read_log","decision":"deny"}}' -replace '"', '\"'
    & $exe agentctl $escaped
} -ArgumentList $exe
Start-Sleep -Seconds 3
$ap2 = Invoke-Agentctl '{"tool":"approve","args":{"request":2,"decision":"allow"}}'
Start-Sleep -Seconds 2
$reply2 = Receive-Job $job2 -Wait
if ($reply2 -match '"written":true') { Write-Host "t2-fixedask: PASS" }
else { Write-Host "t2-fixedask: FAIL $reply2" }
Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item $permFile -ErrorAction SilentlyContinue