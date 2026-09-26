# Title-strip passthrough probe (docs/67 stage 2 T4): the workshop slot
# combo now lives INSIDE the window title bar (caption embedding). The server
# must NOT eat mouse-downs there (move grab) — the client declares a
# TitlePassthrough rect (MsgType 25) and clicks flow to the app.
#   s1 server up (probe-owned lifecycle)
#   s2 spawn workshop.jkx -> 7 tool rows + Workshop window found
#   s3 set_script {slot:ws2probe} -> probe-owned slot exists + auto-switch
#   c1 surface static (two captures agree) — makes c1b's hash meaningful
#   c1b caption click at surface (132,14) = combo {52,3,160,22} center ->
#      capture hash CHANGES (dropdown popup paints). If the click were still
#      swallowed as a move grab, the surface would not change.
#   c2 click the dropdown's first row -> list_slots "current" switches
#      (row click reached the client: full input passthrough, not just paint)
#   c3 close_window -> catalog rows 7->0 (server close overlay unharmed)
# USER FILE guard (docs/60 s11 lesson): myapp.js / .current_workshop /
# .history are backed up at start and restored in finally; all probe slot
# writes go to ws2probe only.
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe = "$build\jkdesktop.exe"; $agnt = "$build\jkagentd.exe"
$perm = "$build\permissions.json"
$scripts = "$build\state\scripts"
$myapp = "$scripts\myapp.js"
$current = "$scripts\.current_workshop"
$history = "$scripts\.history"
$probeSlot = "ws2probe"
$script:fail = 0
function Check([string]$name, [bool]$cond, [string]$detail) {
    if ($cond) { Write-Output "PASS: $name" }
    else { $script:fail++; Write-Output "FAIL: $name -- $detail" }
}
function Stop-ProbeProcs {
    Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
        Where-Object { $_.CommandLine -match '--client' } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Get-Process jkdesktop, jkwinserver, jkagentd, jkbridge -ErrorAction SilentlyContinue |
        Stop-Process -Force
}
function Invoke-Agentctl([string]$json) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = 'agentctl "' + ($json -replace '"', '\"') + '"'
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.StandardOutputEncoding = [System.Text.Encoding]::UTF8
    $psi.CreateNoWindow = $true
    $p = [System.Diagnostics.Process]::Start($psi)
    $out = $p.StandardOutput.ReadToEnd()
    $p.WaitForExit()
    return ($out -replace '(?m)^\[theme\][^\r\n]*[\r\n]*', '')
}
function Invoke-Mcp([string]$line) {
    $out = ($line | & $agnt)
    return ($out -join "`n")
}
function Catalog {
    return (Invoke-Agentctl '{"tool":"list_app_tools","args":{}}')
}
function Get-WorkshopWindow {
    $r = Invoke-Mcp '{"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"list_windows","arguments":{}}}'
    $script:lastList = $r
    $m = [regex]::Match($r, '\\"id\\":(\d+),\\"title\\":\\"Workshop\\",\\"pid\\":(\d+),' +
                         '\\"x\\":(-?\d+),\\"y\\":(-?\d+),\\"w\\":(\d+),\\"h\\":(\d+)')
    if (-not $m.Success) { return $null }
    return [pscustomobject]@{
        id = [int]$m.Groups[1].Value; pid = [int]$m.Groups[2].Value
        x = [int]$m.Groups[3].Value;  y = [int]$m.Groups[4].Value
        w = [int]$m.Groups[5].Value;  h = [int]$m.Groups[6].Value
    }
}
function Capture-Hash([int]$wid) {
    $r = Invoke-Agentctl ('{"tool":"capture_window","args":{"id":' + $wid + '}}')
    $pm = [regex]::Match($r, '"path\\?":\\?"([^"]*)"')
    if (-not $pm.Success) { return "" }
    $p = $pm.Groups[1].Value -replace '\\\\', '\'
    if (-not (Test-Path $p)) { return "" }
    return (Get-FileHash $p -Algorithm SHA256).Hash
}
function Send-Click([int]$wid, [int]$sx, [int]$sy) {
    return (Invoke-Mcp ('{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $wid + ',"op":"click","x":' + $sx + ',"y":' + $sy + '}}}'))
}

Stop-ProbeProcs
Start-Sleep -Seconds 1
# USER FILE guard
$myappExisted = Test-Path $myapp
if ($myappExisted) { Copy-Item $myapp "$myapp.tstrip_bak" -Force; Write-Output "NOTICE: myapp.js backed up" }
$curExisted = Test-Path $current
if ($curExisted) { Copy-Item $current "$current.tstrip_bak" -Force }
$histExisted = Test-Path $history
$histBak = "$history.tstrip_bak"
if ($histExisted) { Move-Item $history $histBak -Force }
Write-Output "NOTICE: user slot state guarded (.history/.current moved aside)"
# permissions: caption click drive needs send_input ALLOW (default ask)
$permExisted = Test-Path $perm
if ($permExisted) {
    Copy-Item $perm "$perm.tstrip_bak" -Force
    $json = Get-Content $perm -Raw | ConvertFrom-Json
    $json | Add-Member -NotePropertyName send_input -NotePropertyValue "allow" -Force
    $json | ConvertTo-Json -Depth 5 | Set-Content $perm -Encoding ASCII
} else {
    '{"send_input":"allow"}' | Set-Content $perm -Encoding ASCII
}

try {
    Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build -WindowStyle Hidden
    $up = $false
    foreach ($i in 1..30) {
        Start-Sleep -Milliseconds 500
        if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up = $true; break }
    }
    Check "s1-server-up" $up ""
    Write-Output "NOTICE: the user's live desktop server was stopped for the probe run"

    Start-Sleep -Seconds 2
    # spawn workshop via launch_app (probe_workshop convention)
    $jkxJson = (($build + "\apps\workshop.jkx") -replace '\\', '\\\\')
    Invoke-Mcp ('{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"launch_app","arguments":{"jkx":"' + $jkxJson + '"}}}') | Out-Null
    $win = $null
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 500
        $win = Get-WorkshopWindow
        if ($null -ne $win) { break }
    }
    Check "s2-launch-window" ($null -ne $win) ""
    $cat = Catalog
    $rows = [regex]::Matches($cat, '"app":"workshop"').Count
    Check "s2b-catalog-7-rows" ($rows -eq 7) ("rows=$rows")
    if ($null -eq $win) { throw "no workshop window" }

    # s3: create the probe-owned slot (auto-switches to it)
    $src = "createLabel({x:20,y:40,w:260,h:26},'TS2PROBE');"
    $r = Invoke-Mcp ('{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"app_tool","arguments":{"app":"workshop","tool":"set_script","args":{"source":"' + ($src -replace '"', '\"') + '","slot":"' + $probeSlot + '"}}}}')
    Check "s3-set-slot" ($r -match 'ok\\":true' -and $r -match 'ws2probe') ($r.Substring(0, [Math]::Min(220, $r.Length)))
    Start-Sleep -Milliseconds 800

    # c1: surface static (clock-free source)
    $h0 = Capture-Hash $win.id
    $h0b = Capture-Hash $win.id
    Check "c1-surface-static" ($h0 -ne "" -and $h0 -eq $h0b) ""

    # c1b: caption click = passthrough. Measured (2026-09-27 live diag): the
    # combo only toggles its dropdown from the RIGHT 18px arrow zone — the
    # combo arrow zone is surface x ~194..212, center (203,14). A body click
    # (132,14) only paints the focus ring (hash changes but no dropdown).
    $r2 = Send-Click $win.id ($win.x + 203) ($win.y + 14)
    Check "c1b-click-sent" ($r2 -match 'sent\\":true') ($r2.Substring(0, [Math]::Min(160, $r2.Length)))
    Start-Sleep -Milliseconds 600
    $h1 = Capture-Hash $win.id
    Check "c1b-caption-click-hash" ($h1 -ne "" -and $h1 -ne $h0) ""
    # c2: the dropdown stays open; popup top = surface y 25 (client y 1),
    # first row (myapp, alphabetical first) center ~= surface y 34. Row click
    # switching the slot is the FULL passthrough proof (not just paint).
    $r3 = Send-Click $win.id ($win.x + 110) ($win.y + 34)
    Check "c2-row-click-sent" ($r3 -match 'sent\\":true') ""
    Start-Sleep -Milliseconds 900
    $r4 = (Invoke-Mcp '{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"app_tool","arguments":{"app":"workshop","tool":"list_slots","args":{}}}}')
    # rows sorted: myapp < ws2probe; current was ws2probe, so row 1 = myapp
    Check "c2-row-switches-slot" ($r4 -match 'current\\":\\"myapp') ($r4.Substring(0, [Math]::Min(220, $r4.Length)))

    # c3: chrome survival — close via the tool (server Close path unharmed)
    $rc = Invoke-Agentctl ('{"tool":"close_window","args":{"id":' + $win.id + '}}')
    Check "c3-close-window" ($rc -match '"ok":true') ($rc.Substring(0, [Math]::Min(160, $rc.Length)))
    $rows2 = 0
    foreach ($i in 1..10) {
        Start-Sleep -Milliseconds 400
        $rows2 = [regex]::Matches((Catalog), '"app":"workshop"').Count
        if ($rows2 -eq 0) { break }
    }
    Check "c3b-catalog-cleared" ($rows2 -eq 0) "rows=$rows2"
} finally {
    Stop-ProbeProcs
    # probe-owned slot artifacts only
    Remove-Item (Join-Path $scripts ($probeSlot + ".js")) -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $history $probeSlot) -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item $current -Force -ErrorAction SilentlyContinue
    if (Test-Path $histBak) {
        if (Test-Path $history) { Remove-Item $history -Recurse -Force -ErrorAction SilentlyContinue }
        Move-Item $histBak $history -Force
        Write-Output "NOTICE: user .history restored"
    }
    if ($curExisted) {
        Copy-Item "$current.tstrip_bak" $current -Force
        Remove-Item "$current.tstrip_bak" -Force
    }
    if ($myappExisted -and (Test-Path "$myapp.tstrip_bak")) {
        Copy-Item "$myapp.tstrip_bak" $myapp -Force
        Remove-Item "$myapp.tstrip_bak" -Force
        Write-Output "NOTICE: myapp.js restored"
    } elseif (-not $myappExisted) {
        Remove-Item $myapp -Force -ErrorAction SilentlyContinue
    }
    if ($permExisted) {
        Copy-Item "$perm.tstrip_bak" $perm -Force
        Remove-Item "$perm.tstrip_bak" -Force
    }
}
if ($script:fail -eq 0) { Write-Output "RESULT: ALL PASS" }
else { Write-Output ("RESULT: " + $script:fail + " FAIL"); exit 1 }