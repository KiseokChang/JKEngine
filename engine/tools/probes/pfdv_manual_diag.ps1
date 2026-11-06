# Manual diagnostic: file_open -> dialog -> choose immediately and over time.
# ASCII only. Own server spawn + teardown. Not part of the probe suite.
$ErrorActionPreference = 'Continue'
$root = "I:\progwork\JKENGINE\engine\build"
$exe  = Join-Path $root "jkdesktop.exe"
$fix  = "I:\progwork\JKENGINE\tmp\pfdv_diag"
New-Item -ItemType Directory -Force -Path (Join-Path $fix "start") | Out-Null
Set-Content -Path (Join-Path $fix "start\a.txt") -Value "x"
Set-Content -Path (Join-Path $fix "start\b.txt") -Value "y"

function Invoke-Agentctl([string]$json) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = 'agentctl "' + ($json -replace '"', '\"') + '"'
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $p = [System.Diagnostics.Process]::Start($psi)
    $out = $p.StandardOutput.ReadToEnd()
    $p.WaitForExit()
    return $out.Trim()
}

# spawn own server
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden | Out-Null
Start-Sleep -Milliseconds 1200
$serverPid = 0
foreach ($c in (Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'")) {
    if ($c.CommandLine -and $c.CommandLine -match '--server') { $serverPid = $c.ProcessId }
}
"SERVER pid=$serverPid"

$sw = [System.Diagnostics.Stopwatch]::StartNew()
function Stamp { return ("[{0,4}s]" -f [int]$script:sw.Elapsed.TotalSeconds) }

# file_open reply mode (default)
$r = Invoke-Agentctl ('{"tool":"file_open","args":{"start":"' + ($fix -replace '\\','/') + '/start"}}')
"$(Stamp) file_open -> $r"

# wait for dialog child
$dlgPid = 0
foreach ($i in 1..30) {
    Start-Sleep -Milliseconds 400
    foreach ($c in (Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'")) {
        if ($c.ParentProcessId -eq $serverPid -and $c.CommandLine -and $c.CommandLine -match '--filedlg') { $dlgPid = $c.ProcessId; break }
    }
    if ($dlgPid -gt 0) { break }
}
"$(Stamp) dialog pid=$dlgPid"
Start-Sleep -Seconds 2

# registration poll: list_app_tools until filedlg rows appear
$rows = 0
foreach ($i in 1..20) {
    $cat = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
    $rows = ([regex]::Matches($cat, '"app":"filedlg"')).Count
    if ($rows -ge 3) { break }
    Start-Sleep -Milliseconds 500
}
"$(Stamp) rows=$rows"
$m1 = [regex]::Match($cat, '"app":"filedlg","name":"choose".*?"connId":(\d+)')
"$(Stamp) choose-manifest connId=$($m1.Groups[1].Value)"

# immediate choose
$r1 = Invoke-Agentctl '{"tool":"app_tool","args":{"app":"filedlg","tool":"choose","args":{"name":"a.txt"}}}'
"$(Stamp) choose@+0s -> $r1"

# if the dialog survived (choose failed), probe again over time
foreach ($waitS in 15, 30, 45, 60) {
    Start-Sleep -Seconds $waitS
    $alive = [bool](Get-Process -Id $dlgPid -ErrorAction SilentlyContinue)
    if (-not $alive) { "$(Stamp) dialog gone - stop"; break }
    $r2 = Invoke-Agentctl '{"tool":"app_tool","args":{"app":"filedlg","tool":"list","args":{"offset":0}}}'
    $m2 = [regex]::Match($r2, '"connId":(\d+)')
    "$(Stamp) +${waitS}s alive=$alive -> $($r2.Substring(0, [Math]::Min(160, $r2.Length)))"
}

# teardown
Get-Process -Id $dlgPid -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Stop-Process -Id $serverPid -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 800
Remove-Item -Recurse -Force $fix -ErrorAction SilentlyContinue
"TEARDOWN done"