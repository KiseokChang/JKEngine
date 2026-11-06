$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 3
function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}
$l = Invoke-Agentctl '{"tool":"launch_app","args":{"app":"agentmgr"}}'
if ($l -match '"ok"') { Write-Host "t6-launch: PASS" } else { Write-Host "t6-launch: FAIL $l" }
Start-Sleep -Seconds 4
$w = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
if ($w -match 'Agent Manager') { Write-Host "t6-window: PASS" } else { Write-Host "t6-window: FAIL $w" }
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
# spawned client host process dies with server kill; sweep any strays
Get-Process jkapp_agentmgr -ErrorAction SilentlyContinue | Stop-Process -Force