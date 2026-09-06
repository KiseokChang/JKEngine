# Repro: spawn 3 clients, kill all 3 simultaneously, check server survival.
$ErrorActionPreference = 'Continue'
$exe = "I:\progwork\JKENGINE\prototype\sdl2_jkwindow\build\jkdesktop.exe"

# Server should already be running. Spawn 3 clients.
$jobs = @()
foreach ($i in 1..3) {
    $jobs += Start-Process -FilePath $exe -ArgumentList "--client","tetris" `
        -WorkingDirectory "I:\progwork\JKENGINE" -PassThru -WindowStyle Hidden
    Start-Sleep -Milliseconds 600
}
Start-Sleep -Seconds 2

# Kill all 3 clients in one shot (simultaneous TerminateProcess)
$pids = ($jobs | Where-Object { -not $_.HasExited } | Select-Object -ExpandProperty Id)
Write-Host "killing clients: $($pids -join ',')"
$pids | ForEach-Object { taskkill /F /PID $_ 2>&1 | Out-Null }
Start-Sleep -Seconds 3

# Is the server still alive?
$server = Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
    Where-Object { $_.CommandLine -match '--server' }
if ($server) {
    Write-Host "SERVER ALIVE pid=$($server.ProcessId)"
} else {
    Write-Host "SERVER DEAD"
}