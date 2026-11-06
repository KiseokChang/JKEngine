$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Remove-Item (Join-Path (Split-Path $exe) "permissions.json") -ErrorAction SilentlyContinue
Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 3
function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}
$p = Invoke-Agentctl '{"tool":"agent_permissions","args":{}}'
if ($p -match '"close_window","gate":"server","file":"","effective":"deny"' -and `
    $p -match '"permission_set","gate":"server\(fixed\)","file":"","effective":"ask"') {
    Write-Host "t1-perms: PASS"
} else { Write-Host "t1-perms: FAIL $p" }
$i = Invoke-Agentctl '{"tool":"installed_list","args":{}}'
if ($i -match 'sampletodo' -and $i -match '"kind":"console"' -and `
    $i -match '"minesweeper"' -and $i -match '"kind":"jkx"') {
    Write-Host "t1-installed: PASS"
} else { Write-Host "t1-installed: FAIL $i" }
$r = Invoke-Agentctl '{"tool":"read_receipts","args":{}}'
if ($r -match '"rows":\[') { Write-Host "t1-receipts: PASS" } else { Write-Host "t1-receipts: FAIL $r" }
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force