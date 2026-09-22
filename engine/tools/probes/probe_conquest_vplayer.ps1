# Conquest contract probe: vplayer (spec 2026-09-21-conquest-ladder s2, rung 3).
# TRACK A - tool relay: the drive stage rides the app's OWN registered agent
# tools (app_tool open/play_pause/seek/get_status) instead of send_input.
# launch -> observe -> drive -> verify -> recover, all via the agent tool
# surface ONLY (never OS SendInput - spec s3.2). Template: probe_conquest_
# minesweeper.ps1 (skeleton verbatim); app_tool/clip/get_status conventions:
# probe_app_tools.ps1.
# NOTE: no permissions.json edit here - app_tool default gate is allow
# (kPermMatrix {"app_tool","server","allow"}); the user runtime file passes.
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe = "$build\jkdesktop.exe"; $agnt = "$build\jkagentd.exe"
$ctl = $exe   # probe_agent_events.ps1 names the exe $ctl - the verbatim
              # Invoke-Ctl helper below references that spelling.
$clip = "I:/progwork/JKENGINE/tmp/vpt2_test.mp4"   # forward slashes: fopen OK
$script:fail = 0
function Check([string]$name, [bool]$cond) {
    if ($cond) { Write-Output "PASS $name" } else { Write-Output "FAIL $name"; $script:fail++ }
}
function Stop-ProbeProcs {
    # jkapp_vplayer is a dedicated image name (probe_app_tools line 246).
    Get-Process jkapp_vplayer -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
        Where-Object { $_.CommandLine -match '--client' } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    # Full teardown (conquest template): jkwinserver owns the default pipe -
    # leaving it up lands every MCP call on the stale server (measured).
    Get-Process jkdesktop, jkwinserver, jkagentd, jkbridge -ErrorAction SilentlyContinue |
        Stop-Process -Force
}
function Invoke-Mcp([string]$line) {
    $out = ($line | & $agnt)
    return ($out -join "`n")
}
function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    $out = (& $exe agentctl $escaped) -join "`n"
    $idx = $out.IndexOf('{')
    if ($idx -lt 0) {
        Write-Output "DIAG agentctl-no-json: $out"
        return ""
    }
    return $out.Substring($idx)
}
function Invoke-Ctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $ctl agentctl $escaped 2>$null) -join "`n"
}
function AppTool([string]$tool, [string]$argsJson, [int]$windowId = 0) {
    # probe_app_tools.ps1 shape: windowId at the app_tool args level, the
    # app's own payload is the nested "args" passthrough.
    $a = '{"app":"vplayer","tool":"' + $tool + '"'
    if ($windowId -gt 0) { $a += ',"windowId":' + $windowId }
    if ($argsJson) { $a += ',"args":' + $argsJson }
    $a += '}'
    return (Invoke-Agentctl ('{"tool":"app_tool","args":' + $a + '}'))
}
function Get-VpWindow {
    # Window fields live ESCAPED inside the tool text (template regex
    # verbatim). Fresh server: the only listed window is the vplayer client.
    $r = Invoke-Mcp '{"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"list_windows","arguments":{}}}'
    $script:lastList = $r
    $m = [regex]::Match($r, '\\"id\\":(\d+),\\"title\\":\\"([^"\\]*)\\",\\"pid\\":(\d+),' +
                         '\\"x\\":(-?\d+),\\"y\\":(-?\d+),\\"w\\":(\d+),\\"h\\":(\d+)')
    if (-not $m.Success) { return $null }
    return [pscustomobject]@{
        id = [int]$m.Groups[1].Value; title = $m.Groups[2].Value
        pid = [int]$m.Groups[3].Value; x = [int]$m.Groups[4].Value
        y = [int]$m.Groups[5].Value;   w = [int]$m.Groups[6].Value
        h = [int]$m.Groups[7].Value
    }
}
function Capture-Hash([int]$wid) {
    $r = Invoke-Agentctl ('{"tool":"capture_window","args":{"id":' + $wid + '}}')
    $pm = [regex]::Match($r, '"path\\?":\\?"([^"]*)"')
    if (-not $pm.Success) {
        Write-Output "DIAG capture-no-path: $r"
        return ""
    }
    $p = $pm.Groups[1].Value -replace '\\\\', '\'
    if (-not (Test-Path $p)) {
        Write-Output "DIAG capture-path-missing: $p"
        return ""
    }
    return (Get-FileHash $p -Algorithm SHA256).Hash
}
function Watch-Events([int]$sec) {
    return (Start-Job -ScriptBlock { param($e, $s) (& $e agent-events $s) -join "`n" } `
        -ArgumentList $exe, $sec)
}
# Poll get_status until a regex matches (<=10s). $rx matches against the
# whole app_tool relay reply ({"ok":true,"windowId":N,"result":{...}}).
function Poll-Status([string]$pattern) {
    foreach ($i in 1..20) {
        Start-Sleep -Milliseconds 500
        $st = AppTool "get_status" ""
        if ($st -match $pattern) { return $st }
    }
    return $st
}
$script:cycleOk = $false
function Invoke-ConquestCycle([string]$tag) {
    $script:cycleOk = $true
    # (1) launch: launch_app + window appearance poll (hard gate).
    Invoke-Mcp '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"launch_app","arguments":{"app":"vplayer"}}}' | Out-Null
    $win = $null
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 500
        $win = Get-VpWindow
        if ($null -ne $win) { break }
    }
    Check "$tag-launch-window" ($null -ne $win)
    if ($null -eq $win) { $script:cycleOk = $false; return }
    # (2) observe: fields non-degenerate + title + idle-window capture hash.
    $obs = ($win.id -gt 0 -and $win.pid -gt 0 -and $win.w -gt 0 -and $win.h -gt 0 -and $win.title -match 'Video')
    Check "$tag-observe-fields" $obs
    if (-not $obs) { $script:cycleOk = $false }
    $h0 = Capture-Hash $win.id
    Check "$tag-observe-capture" ($h0 -ne "")
    # (3) drive: open clip via the app's OWN tool (track A - no send_input).
    #     open is async: the truth is get_status.opened (probe_app_tools c2).
    $op = AppTool "open" ('{"path":"' + $clip + '"}') $win.id
    $st = Poll-Status '"opened\\?":true'
    Check "$tag-drive-open-opened" ($st -match '"opened\\?":true')
    # play_pause -> paused:true
    [void](AppTool "play_pause" "" $win.id)
    Start-Sleep -Milliseconds 400
    $st2 = AppTool "get_status" "" $win.id
    Check "$tag-drive-pause" ($st2 -match '"paused\\?":true')
    # (4) verify: the playback window painted pixels (hash moved from the
    #     idle window) + seek echoes a position + same id still alive.
    $h1 = Capture-Hash $win.id
    Check "$tag-verify-hash" ($h1 -ne "" -and $h1 -ne $h0)
    [void](AppTool "seek" '{"seconds":1}' $win.id)
    Start-Sleep -Milliseconds 400
    $st3 = AppTool "get_status" "" $win.id
    $posm = [regex]::Match($st3, '"pos":([0-9.]+)')
    $posOk = $posm.Success -and ([double]$posm.Groups[1].Value) -lt 2.0
    Check "$tag-verify-seek" $posOk
    $again = Get-VpWindow
    Check "$tag-verify-alive" ($null -ne $again -and $again.id -eq $win.id)
    if (-not (($st -match '"opened\\?":true') -and ($st2 -match '"paused\\?":true') -and
        ($h1 -ne "" -and $h1 -ne $h0) -and $posOk -and
        ($null -ne $again -and $again.id -eq $win.id))) {
        $script:cycleOk = $false
    }
}

Stop-ProbeProcs
Start-Sleep -Seconds 1
Write-Output "NOTICE: permissions.json is NOT edited (app_tool default gate allow)"
try {
    Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build -WindowStyle Hidden
    $up = $false
    foreach ($i in 1..30) {
        Start-Sleep -Milliseconds 500
        if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up = $true; break }
    }
    Check "setup-server-up" $up
    Write-Output "NOTICE: the user's live desktop server was stopped for the probe run"

    $job = Watch-Events 25
    Start-Sleep -Seconds 2
    Invoke-ConquestCycle "cycle1"
    Check "cycle1" $script:cycleOk
    $createdEvent = (Receive-Job -Job $job -Wait 2>&1) | Out-String
    Check "launch-window-created-event" ($createdEvent -match 'window\.created')

    # (5) recover: kill the client by PID -> app.crashed -> relaunch -> full
    #     cycle again. Hard gate: recover-gone (pid leaves list_windows).
    $job2 = Watch-Events 15
    Start-Sleep -Seconds 2
    $gone = $false
    if ($null -ne $script:lastList -and $script:lastList -match '\\"id\\":(\d+)') {
        $pm = [regex]::Match($script:lastList, '\\"pid\\":(\d+)')
        if ($pm.Success) {
            $script:vpPid = [int]$pm.Groups[1].Value
            Stop-Process -Id $script:vpPid -Force -ErrorAction SilentlyContinue
            for ($i = 0; $i -lt 20; $i++) {
                Start-Sleep -Milliseconds 500
                [void](Get-VpWindow)
                if (-not ($script:lastList -match ('\\"pid\\":' + $script:vpPid + '(?![0-9])'))) {
                    $gone = $true
                    break
                }
            }
        }
    }
    Check "recover-gone" $gone
    Start-Sleep -Seconds 2
    $crashedEvent = (Receive-Job -Job $job2 -Wait 2>&1) | Out-String
    Check "recover-app-crashed-event" ($crashedEvent -match 'app\.crashed')
    $stats = Invoke-Ctl '{"tool":"events_list","args":{}}'
    $sm = [regex]::Match($stats, '"topic":"app\.crashed"[^}]*"fired":(\d+)')
    Check "recover-app-crashed-stat" ($sm.Success -and ([int]$sm.Groups[1].Value) -ge 1)
    Start-Sleep -Seconds 1
    if ($gone) {
        Invoke-ConquestCycle "cycle2"
        Check "cycle2-after-recover" $script:cycleOk
    } else {
        Write-Output "DIAG cycle2-skipped: stale pid still in list_windows"
    }
} finally {
    Stop-ProbeProcs
    Write-Output "NOTICE: the desktop server is left stopped (probe_workshop convention) - restart jkwinserver.exe to resume the live desktop"
}
if ($script:fail -gt 0) { Write-Output "FAILED: $($script:fail)"; exit 1 }
Write-Output "CONQUEST PASS"
exit 0