# Verify: server survives 3 simultaneous client kills, repeated.
$ErrorActionPreference = 'Continue'
$exe = "I:\progwork\JKENGINE\prototype\sdl2_jkwindow\build\jkdesktop.exe"

$jobs = @()
foreach ($i in 1..3) {
    $jobs += Start-Process -FilePath $exe -ArgumentList "--client","tetris" `
        -WorkingDirectory "I:\progwork\JKENGINE" -PassThru -WindowStyle Hidden
    Start-Sleep -Milliseconds 500
}
Start-Sleep -Seconds 2
$pids = ($jobs | Where-Object { -not $_.HasExited } | Select-Object -ExpandProperty Id)
Write-Host "killing clients: $($pids -join ',')"
$pids | ForEach-Object { taskkill /F /PID $_ 2>&1 | Out-Null }
Start-Sleep -Seconds 3
$server = Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
    Where-Object { $_.CommandLine -match '--server' }
if ($server) { Write-Host "PASS: SERVER ALIVE pid=$($server.ProcessId)" }
else { Write-Host "FAIL: SERVER DEAD" }