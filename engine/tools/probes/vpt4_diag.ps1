$build = "I:\progwork\JKENGINE\engine\build"
$exe = "$build\jkdesktop.exe"
function Invoke-Agentctl([string]$json) {
    $escaped = $json.Replace('"', [string][char]92 + '"')
    return (& $exe agentctl $escaped 2>&1) -join "`n"
}
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build -RedirectStandardError "I:\progwork\JKENGINE\tmp\vpt4_client_stderr.log"
Start-Sleep -Seconds 4
Invoke-Agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
Start-Sleep -Seconds 8
Write-Host "=== list_windows ==="
Invoke-Agentctl '{"tool":"list_windows","args":{}}'