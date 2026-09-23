# Conquest contract probe: workshop script app (spec 2026-09-21-conquest-ladder,
# rung 6). RULING recorded in docs/62 s4: scriptdemo itself (SCRI-embedded,
# clock script) has NO tool/event surface and self-redraws at 1Hz, so the
# verify stage of the conquest contract cannot stand on the tool surface - the
# conquerable form of the same jkapp_script module is the workshop app
# (get_script/set_script registered). Drive is DUAL-TRACK, the ladder's first:
#   A) app_tool set_script -> SyncReload (in-place, same connection id) ->
#      verified by get_script round-trip AND the capture hash of the reloaded
#      UI (seed label changes).
#   B) send_input click on the rewritten script's counter button -> verified
#      by the capture hash (the rewritten script is clock-free, so the surface
#      is static and the hash is a VALID verify signal here - measured rule:
#      scriptdemo's clock is what invalidated it).
# Recover: pid kill -> recover-gone hard gate -> app.crashed -> cycle2.
# USER FILE: state/scripts/myapp.js is the workshop's live truth source -
# backed up at start, restored in finally, console-noticed (docs/59 s16.1
# discipline, extended to script files).
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe = "$build\jkdesktop.exe"; $agnt = "$build\jkagentd.exe"
$ctl = $exe
$perm = "$build\permissions.json"
$myapp = "$build\state\scripts\myapp.js"
$marker1 = "CONQSEED2623"
$marker2 = "CONQSEED2624"
$script:fail = 0
function Check([string]$name, [bool]$cond) {
    if ($cond) { Write-Output "PASS $name" } else { Write-Output "FAIL $name"; $script:fail++ }
}
function Stop-ProbeProcs {
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
# Workshop app tool via the generic app_tool relay - MCP channel. Measured
# (rung 6 diag): agentctl is native argv and SPACES in the payload split the
# argument (lesson 26 extended to app_tool set_script payloads - even
# "var x = 1;" dies with bad_request), while the MCP channel takes the JSON
# over stdin and survives. probe_workshop's tiny/no-space payloads hid this.
function Invoke-AppTool([string]$tool, [string]$toolArgs) {
    return (Invoke-Mcp ('{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"app_tool","arguments":{"app":"workshop","tool":"' + $tool + '","args":' + $toolArgs + '}}}' ))
}
function Get-WorkshopWindow {
    $r = Invoke-Mcp '{"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"list_windows","arguments":{}}}'
    $script:lastList = $r
    $m = [regex]::Match($r, '\\"id\\":(\d+),\\"title\\":\\"Workshop\\",\\"pid\\":(\d+),' +
                         '\\"x\\":(-?\d+),\\"y\\":(-?\d+),\\"w\\":(\d+),\\"h\\":(\d+)')
    if (-not $m.Success) { return $null }
    return [pscustomobject]@{
        id = [int]$m.Groups[1].Value; title = "Workshop"
        pid = [int]$m.Groups[2].Value; x = [int]$m.Groups[3].Value
        y = [int]$m.Groups[4].Value;   w = [int]$m.Groups[5].Value
        h = [int]$m.Groups[6].Value
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
# The rewritten script: single line, SINGLE-QUOTED JS strings (docs/60 lesson:
# double quotes die in the CRT re-escape), clock-free so the surface is static.
# seed label differs per cycle so the reload-hash check has something to move.
function New-ScriptSource([string]$seed) {
    return ("var lbl = createLabel({ x: 20, y: 20, w: 260, h: 26 }, '" + $seed +
            "'); var btn = createButton({ x: 20, y: 60, w: 150, h: 38 }, 'Click: 0');" +
            " var n = 0; function onClick(id) { if (id === btn) { n++;" +
            " setText(btn, 'Click: ' + n); setText(lbl, 'MARK' + n); } }")
}
function Set-Script([string]$source) {
    $esc = $source -replace '"', '\"'
    return (Invoke-AppTool "set_script" ('{"source":"' + $esc + '"}'))
}
$script:cycleOk = $false
function Invoke-ConquestCycle([string]$tag, [string]$seed) {
    $script:cycleOk = $true
    # (1) launch: launch_app jkx form (tool surface, not a manual --jkx spawn).
    # Backslashes doubled for the JSON string literal.
    $jkxJson = (($build + "\apps\workshop.jkx") -replace '\\', '\\\\')
    Invoke-Mcp ('{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"launch_app","arguments":{"jkx":"' + $jkxJson + '"}}}') | Out-Null
    $win = $null
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 500
        $win = Get-WorkshopWindow
        if ($null -ne $win) { break }
    }
    Check "$tag-launch-window" ($null -ne $win)
    if ($null -eq $win) { $script:cycleOk = $false; return }
    $script:win = $win
    # (2) observe: fields non-degenerate + capture works.
    $obs = ($win.id -gt 0 -and $win.pid -gt 0 -and $win.w -gt 0 -and $win.h -gt 0)
    Check "$tag-observe-fields" $obs
    if (-not $obs) { $script:cycleOk = $false }
    $h0 = Capture-Hash $win.id
    Check "$tag-observe-capture" ($h0 -ne "")
    # (3) drive A (track A): set_script -> synchronous reload (same id).
    $r1 = Set-Script (New-ScriptSource $seed)
    Check "$tag-drive-set-script" ($r1 -match 'ok\\":true')
    # (4) verify A1: get_script round-trip carries the new source (tool truth).
    #     (args must be a literal "{}" - an empty string dies as bad JSON.)
    $g1 = Invoke-AppTool "get_script" "{}"
    Check "$tag-verify-get-script" ($g1 -match $seed)
    # verify A2: the reloaded UI changed pixels (template label -> seed label).
    Start-Sleep -Milliseconds 800
    $h1 = Capture-Hash $win.id
    Check "$tag-verify-reload-hash" ($h1 -ne "" -and $h1 -ne $h0)
    # verify A3: the rewritten surface is STATIC (two captures agree) - this
    # is what makes the drive-B hash check meaningful.
    $h1b = Capture-Hash $win.id
    Check "$tag-verify-static-hash" ($h1b -ne "" -and $h1b -eq $h1)
    # (5) drive B (track B): click the counter button. Measured (rung 6 diag
    #     capture): script widgets render offset inside the panel - the button
    #     declared at x20 y60 lands at surface ~ (22..172, 108..148), center
    #     (97,128); a click at the declared coords (95,79) hits dead space.
    $r2 = Invoke-Mcp ('{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"click","x":' + ($win.x + 97) + ',"y":' + ($win.y + 128) + '}}}')
    Check "$tag-drive-click" ($r2 -match 'sent\\":true')
    Start-Sleep -Milliseconds 800
    $h2 = Capture-Hash $win.id
    Check "$tag-verify-click-hash" ($h2 -ne "" -and $h2 -ne $h1)
    $again = Get-WorkshopWindow
    Check "$tag-verify-alive" ($null -ne $again -and $again.id -eq $win.id)
    if (-not (($r1 -match 'ok\\":true') -and ($g1 -match $seed) -and
        ($h1 -ne "" -and $h1 -ne $h0) -and ($h1b -eq $h1) -and
        ($r2 -match 'sent\\":true') -and ($h2 -ne "" -and $h2 -ne $h1) -and
        ($null -ne $again -and $again.id -eq $win.id))) {
        $script:cycleOk = $false
    }
}

Stop-ProbeProcs
Start-Sleep -Seconds 1
# USER FILE guard: myapp.js is the workshop's live truth source (the user's
# phone-session content). Backup + console notice + restore in finally.
$myappExisted = Test-Path $myapp
if ($myappExisted) {
    Write-Output "NOTICE: editing $myapp (backup+restore)"
    Copy-Item $myapp "$myapp.probe_bak" -Force
} else {
    Write-Output "DIAG myapp.js missing at probe start - the template seeds it on first spawn; a probe-created file is removed in finally"
}
# permissions: send_input must be ALLOW for drive B (default is ask). app_tool
# defaults to allow (kPermMatrix) - no edit needed for drive A.
$permExisted = Test-Path $perm
if ($permExisted) {
    Write-Output "NOTICE: editing $perm (backup+restore)"
    Copy-Item $perm "$perm.probe_bak" -Force
} else {
    Write-Output "DIAG permissions.json missing at probe start - a prior probe deleted it (docs/59 s16.1 class); writing a probe-local file"
    '{"send_input":"allow"}' | Set-Content $perm -Encoding ASCII
}
try {
    if ($permExisted) {
        $json = Get-Content $perm -Raw | ConvertFrom-Json
        $json | Add-Member -NotePropertyName send_input -NotePropertyValue "allow" -Force
        $json | ConvertTo-Json -Depth 5 | Set-Content $perm -Encoding ASCII
    }
    Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build -WindowStyle Hidden
    $up = $false
    foreach ($i in 1..30) {
        Start-Sleep -Milliseconds 500
        if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up = $true; break }
    }
    Check "setup-server-up" $up
    Write-Output "NOTICE: the user's live desktop server was stopped for the probe run"

    Start-Sleep -Seconds 2
    Invoke-ConquestCycle "cycle1" $marker1
    Check "cycle1" $script:cycleOk
    # window.created via events_list fired counter - delivery-independent.
    $stats = Invoke-Ctl '{"tool":"events_list","args":{}}'
    $wm = [regex]::Match($stats, '"topic":"window\.created"[^}]*"fired":(\d+)')
    Check "launch-window-created-event" ($wm.Success -and ([int]$wm.Groups[1].Value) -ge 1)

    # (6) recover: kill the client by PID -> recover-gone hard gate ->
    #     app.crashed -> cycle2 (relaunch reads the marker script from disk,
    #     then rewrites it to marker2 - the reload-hash check has a fresh pair).
    $gone = $false
    if ($null -ne $script:win) {
        Stop-Process -Id $script:win.pid -Force -ErrorAction SilentlyContinue
        for ($i = 0; $i -lt 20; $i++) {
            Start-Sleep -Milliseconds 500
            [void](Get-WorkshopWindow)
            if (-not ($script:lastList -match ('\\"pid\\":' + $script:win.pid + '(?![0-9])'))) {
                $gone = $true
                break
            }
        }
    }
    Check "recover-gone" $gone
    $stats = Invoke-Ctl '{"tool":"events_list","args":{}}'
    $sm = [regex]::Match($stats, '"topic":"app\.crashed"[^}]*"fired":(\d+)')
    Check "recover-app-crashed-stat" ($sm.Success -and ([int]$sm.Groups[1].Value) -ge 1)
    Start-Sleep -Seconds 1
    if ($gone) {
        Invoke-ConquestCycle "cycle2" $marker2
        Check "cycle2-after-recover" $script:cycleOk
    } else {
        Write-Output "DIAG cycle2-skipped: stale pid $($script:win.pid) still in list_windows"
    }
} finally {
    Stop-ProbeProcs
    if ($myappExisted -and (Test-Path "$myapp.probe_bak")) {
        Copy-Item "$myapp.probe_bak" $myapp -Force
        Remove-Item "$myapp.probe_bak" -Force
        Write-Output "NOTICE: myapp.js restored"
    } elseif (-not $myappExisted) {
        Remove-Item $myapp -ErrorAction SilentlyContinue
        Write-Output "NOTICE: probe-created myapp.js removed"
    }
    if ($permExisted -and (Test-Path "$perm.probe_bak")) {
        Copy-Item "$perm.probe_bak" $perm -Force
        Remove-Item "$perm.probe_bak" -Force
        Write-Output "NOTICE: permissions.json restored"
    } elseif (-not $permExisted) {
        Remove-Item $perm -ErrorAction SilentlyContinue
        Write-Output "NOTICE: probe-local permissions.json removed"
    }
    Write-Output "NOTICE: the desktop server is left stopped (probe_workshop convention) - restart jkwinserver.exe to resume the live desktop"
}
if ($script:fail -gt 0) { Write-Output "FAILED: $($script:fail)"; exit 1 }
Write-Output "CONQUEST PASS"
exit 0