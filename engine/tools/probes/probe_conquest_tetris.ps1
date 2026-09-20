# Conquest contract probe: tetris (spec 2026-09-21-conquest-ladder s2/s5).
# Ladder rung 2 - a CLONE of probe_conquest_minesweeper.ps1 (the conquest
# template, docs/62 s3) with ONLY the scenario swapped: launch_app "tetris",
# window regex 'Tetris', and the drive stage is send_input KEY (not click -
# tetris has no clickable board action; keys are the only progression lever).
# Key arg format mirrors probe_send_input.ps1's key-ok check (1073741883 =
# SDLK_F2 smoke); the driven key is SDLK_LEFT = 1073741904 (SDLK_SCANCODE_MASK
# 1<<30 | scancode 80), handled by TetrisApp.cpp TetrisGrid::RespondMessage
# (LEFT/RIGHT/UP/DOWN/SPACE all -> changed -> Invalidate, i.e. a redraw).
# launch -> observe -> drive -> verify -> recover, all via the agent tool
# surface ONLY (send_input/app_tool, never OS SendInput - spec s3.2).
# Conventions: probe_send_input.ps1 (lifecycle/MCP/capture hash), lesson 28
# (subscribe-first event jobs), lesson 42 (pid-only kill).
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\.claude\worktrees\conquest-ladder\engine\build"
$exe = "$build\jkdesktop.exe"; $agnt = "$build\jkagentd.exe"
$ctl = $exe   # probe_agent_events.ps1 names the exe $ctl - the verbatim
              # Invoke-Ctl helper below references that spelling.
$perm = "$build\permissions.json"
$script:fail = 0
function Check([string]$name, [bool]$cond) {
    if ($cond) { Write-Output "PASS $name" } else { Write-Output "FAIL $name"; $script:fail++ }
}
function Stop-ProbeProcs {
    # Spawned client apps by PID only (lesson 42: never by image name - the
    # server shares the jkdesktop.exe image name); the server itself the
    # probes stop by name (shot/trust convention).
    Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
        Where-Object { $_.CommandLine -match '--client' } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    # Full teardown (probe_workshop lines 66-67 precedent): the LIVE desktop
    # server is jkwinserver.exe - a jkdesktop-only stop leaves it owning the
    # default pipe and every MCP call lands on the OLD server (measured:
    # probe run 1 - all send_input checks FAILed against the stale server).
    Get-Process jkdesktop, jkwinserver, jkagentd, jkbridge -ErrorAction SilentlyContinue |
        Stop-Process -Force
}
function Invoke-Mcp([string]$line) {
    # One-shot jsonrpc call piped to jkagentd (probe_agent_e2e convention:
    # no spaces in the JSON; tool results arrive with quotes escaped).
    $out = ($line | & $agnt)
    return ($out -join "`n")
}
function Invoke-Agentctl([string]$json) {
    # Direct server channel (probe_agent_shot precedent): capture_window and
    # ping are NOT in the jkagentd MCP catalog (measured: unknown_tool) - the
    # pixel-evidence + readiness paths ride agentctl like the shot probe.
    $escaped = $json -replace '"', '\"'
    # docs/52 lesson: the server prints "[theme] preset ..." loader lines to
    # stdout - pass only JSON rows (first '{' onward) to keep parsing clean.
    # PS5.1 trap: a one-line native-command output arrives as a SCALAR string,
    # so line-array indexing ($raw[0]) yields the first CHARACTER, not the
    # line. Join first, then slice from the first '{'.
    $out = (& $exe agentctl $escaped) -join "`n"
    $idx = $out.IndexOf('{')
    if ($idx -lt 0) {
        Write-Output "DIAG agentctl-no-json: $out"
        return ""
    }
    return $out.Substring($idx)
}
function Invoke-Ctl([string]$json) {
    # Event-reading helper (probe_agent_events.ps1, copied verbatim):
    # PS 5.1 native-arg quoting: bare quotes get stripped (probe lesson) -
    # escape them the way the other agentctl probes do.
    $escaped = $json -replace '"', '\"'
    return (& $ctl agentctl $escaped 2>$null) -join "`n"
}
function Get-TetrisWindow {
    # Window fields live ESCAPED inside the tool text (probe_agent_maximize.ps1
    # regex, copied verbatim). The server is fresh, so the only listed window
    # is the tetris client.
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
    # Direct server channel (probe_agent_shot precedent) - capture_window
    # is not in the jkagentd MCP catalog (measured: unknown_tool).
    $r = Invoke-Agentctl ('{"tool":"capture_window","args":{"id":' + $wid + '}}')
    # path extraction: probe_agent_shot.ps1's escaped-path regex
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
# One short-lived events job per phase (trust/maximize probe convention):
# job stdout is fully buffered, so the subscriber must already be up BEFORE
# the triggering call (lesson 28) and is drained with Receive-Job -Wait
# afterwards.
function Watch-Events([int]$sec) {
    return (Start-Job -ScriptBlock { param($e, $s) (& $e agent-events $s) -join "`n" } `
        -ArgumentList $exe, $sec)
}

# Stage helpers - the reusable conquest skeleton. The cycle result rides
# $script:cycleOk, NOT the function's return value: the Check calls inside
# emit PASS/FAIL lines, and in PowerShell a return value would arrive glued
# to that output as a collection (array -> [bool] cast is always true).
$script:cycleOk = $false
function Invoke-ConquestCycle([string]$tag) {
    $script:cycleOk = $true
    # (1) launch: launch_app + window appearance poll (hard gate).
    Invoke-Mcp '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"launch_app","arguments":{"app":"tetris"}}}' | Out-Null
    $win = $null
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 500
        $win = Get-TetrisWindow
        if ($null -ne $win) { break }
    }
    Check "$tag-launch-window" ($null -ne $win)
    if ($null -eq $win) { $script:win = $null; $script:cycleOk = $false; return }
    $script:win = $win
    # (2) observe: list_windows fields (id/title/pid/geometry) non-degenerate
    #     + title is the tetris window (-match 'Tetris')
    #     + capture_window hash non-empty. The aggregate $script:cycleOk
    #     mirrors the stage Check exactly (review round 1 NIT-1/2).
    Check "$tag-observe-fields" ($win.id -gt 0 -and $win.pid -gt 0 -and $win.w -gt 0 -and $win.h -gt 0 -and $win.title -match 'Tetris')
    if (-not ($win.id -gt 0 -and $win.pid -gt 0 -and $win.w -gt 0 -and $win.h -gt 0 -and $win.title -match 'Tetris')) { $script:cycleOk = $false }
    $h0 = Capture-Hash $win.id
    Check "$tag-observe-capture" ($h0 -ne "")
    # (3) drive: send_input KEY - tetris progresses by keyboard only, so the
    #     click stage is gone (a key op takes no coordinates). Arg shape is
    #     probe_send_input.ps1's key-ok check verbatim: {"id":N,"op":"key",
    #     "key":1073741904} (SDLK_LEFT). The tetris client focuses the game
    #     window in OnInit (ClientTetrisApp.cpp:27-34), so the routed KeyDown
    #     reaches the grid with no prior click.
    $r = Invoke-Mcp ('{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"key","key":1073741904}}}')
    Check "$tag-drive-key" ($r -match 'sent\\":true')
    Start-Sleep -Milliseconds 700
    # (4) verify: capture hash changed (the key moved pixels - note the game's
    #     gravity timer also repaints, so this check proves drive+render, not
    #     the LEFT key alone; the drive-key tool ack is the key-specific gate)
    #     + the window is still alive in list_windows under the same id AND
    #     still titled Tetris (title reconfirm per the rung-2 brief).
    $h1 = Capture-Hash $win.id
    Check "$tag-verify-hash" ($h1 -ne "" -and $h1 -ne $h0)
    $again = Get-TetrisWindow
    Check "$tag-verify-alive" ($null -ne $again -and $again.id -eq $win.id -and $again.title -match 'Tetris')
    if (-not (($h0 -ne "") -and ($r -match 'sent\\":true') -and
        ($h1 -ne "" -and $h1 -ne $h0) -and
        ($null -ne $again -and $again.id -eq $win.id -and $again.title -match 'Tetris'))) {
        $script:cycleOk = $false
    }
}

Stop-ProbeProcs
Start-Sleep -Seconds 1
# permissions: send_input must be ALLOW for the drive stage (default is ask =
# parked). RMW: set the key on the existing file, restore original bytes in
# finally + console notice (docs/59 s16.1 lesson).
Write-Output "NOTICE: editing $perm (backup+restore)"
Copy-Item $perm "$perm.probe_bak" -Force
try {
    $json = Get-Content $perm -Raw | ConvertFrom-Json
    $json | Add-Member -NotePropertyName send_input -NotePropertyValue "allow" -Force
    $json | ConvertTo-Json -Depth 5 | Set-Content $perm -Encoding ASCII
    Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build -WindowStyle Hidden
    # Server-up ping (probe_workshop lines 70-75 precedent): never launch
    # against a still-starting server.
    $up = $false
    foreach ($i in 1..30) {
        Start-Sleep -Milliseconds 500
        if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up = $true; break }
    }
    Check "setup-server-up" $up
    Write-Output "NOTICE: the user's live desktop server was stopped for the probe run"

    # event job: window.created (launch) - subscribe BEFORE the triggering
    # launch (lesson 28: pushes are not replayed). Soft check only: the hard
    # gate is the launch-window poll inside the cycle.
    $job = Watch-Events 25
    Start-Sleep -Seconds 2
    Invoke-ConquestCycle "cycle1"         # 1st cycle
    Check "cycle1" $script:cycleOk
    $createdEvent = (Receive-Job -Job $job -Wait 2>&1) | Out-String
    # soft check: window.created event observed after launch
    Check "launch-window-created-event" ($createdEvent -match 'window\.created')

    # (5) recover: kill the client by PID -> app.crashed -> relaunch ->
    #     full cycle again. Subscribe BEFORE the kill (lesson 28). Hard
    #     gate: recover-gone (pid leaves list_windows) + cycle2-after-recover.
    $job2 = Watch-Events 15
    Start-Sleep -Seconds 2
    $gone = $false
    if ($null -ne $script:win) {
        Stop-Process -Id $script:win.pid -Force -ErrorAction SilentlyContinue
        # recover hard gate (review round 1 FIX): the killed pid must leave
        # list_windows before the relaunch - otherwise cycle2's Get-TetrisWindow
        # (first window in the list) can silently match the stale window. Same
        # polling pattern as the launch stage; if the pid never disappears,
        # the Check stays FAILed and the doomed relaunch is skipped.
        for ($i = 0; $i -lt 20; $i++) {
            Start-Sleep -Milliseconds 500
            [void](Get-TetrisWindow)   # refreshes $script:lastList
            if (-not ($script:lastList -match ('\\"pid\\":' + $script:win.pid + '(?![0-9])'))) {
                $gone = $true
                break
            }
        }
    }
    Check "recover-gone" $gone
    Start-Sleep -Seconds 2
    $crashedEvent = (Receive-Job -Job $job2 -Wait 2>&1) | Out-String
    # soft check: app.crashed event observed after the forced kill
    Check "recover-app-crashed-event" ($crashedEvent -match 'app\.crashed')
    # events_list corroboration (probe_agent_events.ps1 reading convention:
    # the per-topic fired counters live server-side, so they need no
    # subscription and cannot race the drain). Soft check only.
    $stats = Invoke-Ctl '{"tool":"events_list","args":{}}'
    $sm = [regex]::Match($stats, '"topic":"app\.crashed"[^}]*"fired":(\d+)')
    Check "recover-app-crashed-stat" ($sm.Success -and ([int]$sm.Groups[1].Value) -ge 1)
    Start-Sleep -Seconds 1   # spawn throttle is 500ms (docs/28) - 1s headroom
    if ($gone) {
        Invoke-ConquestCycle "cycle2"     # 2nd cycle after recovery
        Check "cycle2-after-recover" $script:cycleOk
    } else {
        Write-Output "DIAG cycle2-skipped: stale pid $($script:win.pid) still in list_windows"
    }
} finally {
    Stop-ProbeProcs
    Copy-Item "$perm.probe_bak" $perm -Force
    Remove-Item "$perm.probe_bak" -Force
    Write-Output "NOTICE: permissions.json restored"
    Write-Output "NOTICE: the desktop server is left stopped (probe_workshop convention) - restart jkwinserver.exe to resume the live desktop"
}
if ($script:fail -gt 0) { Write-Output "FAILED: $($script:fail)"; exit 1 }
Write-Output "CONQUEST PASS"
exit 0