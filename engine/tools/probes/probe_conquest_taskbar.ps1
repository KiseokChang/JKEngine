# Conquest contract probe: taskbar / desktop shell (spec 2026-09-21-conquest-
# ladder, rung 8 - terminal gate). RULING (docs/62 s4): the shell cannot take
# the standard track-B drive - send_input excludes it by design (IsShell() ->
# bad_target, user-approved 2026-09-21) and list_windows excludes it too. Its
# legitimate drive channel is the SHELL PROTOCOL itself: the taskbar renders
# one button per window from WindowList snapshots (docs/28), so the agent
# tool surface drives it through the window lifecycle tools (launch_app /
# kill) and the taskbar's own pixels verify. No clock, no self-redraw
# (ClientTaskbarApp.cpp paints only on WindowListChanged/SizeChanged) - the
# capture hash is a VALID verify signal here (unlike taskmgr, rung 5).
# The taskbar layer is not in list_windows, but capture_window looks the
# layer up directly (no shell exclusion) - the probe finds it by scanning
# ids 1..16 for the short-wide layer (1280x40).
# Scenario per cycle:
#   observe: find taskbar layer id -> capture h0 (static x2) + hard gate:
#            list_windows must NOT contain "Taskbar" (shell exclusion)
#   drive 1: launch_app tetris -> taskbar gains a button -> h1 != h0
#   drive 2: send_input on the shell -> bad_target (design exclusion verified)
#   drive 3: kill tetris -> button gone -> h2 == h0 EXACT (deterministic
#            reflow: 1 or 2 windows both cap at kButtonMaxWidth=180)
#   recover: kill the taskbar pid -> capture -> window_not_found (hard gate;
#            list_windows never shows the shell) -> app.crashed fired
#   cycle 2: launch_app taskbar re-registers the shell role (docs/28:
#            first-wins) -> fresh layer -> h0(c2) == h0(c1) and the re-driven
#            h1(c2) == h1(c1) - cross-cycle determinism.
# Measured (diag_tb8/tb8c): killing the shell leaves the server alive and
# shell-less; the relaunch can race the disconnect sweep (ShellRegister
# denied -> the new client lives shell-less), so the relaunch polls and
# retries the launch_app call until a fresh short-wide layer appears.
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe = "$build\jkdesktop.exe"; $agnt = "$build\jkagentd.exe"
$ctl = $exe
$perm = "$build\permissions.json"
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
Add-Type -AssemblyName System.Drawing
# Layer dimensions by capture - the probe's only way to see the shell, which
# list_windows deliberately excludes.
function Cap-Dims([int]$id) {
    $r = Invoke-Agentctl ('{"tool":"capture_window","args":{"id":' + $id + '}}')
    $pm = [regex]::Match($r, '"path\\?":\\?"([^"]*)"')
    if (-not $pm.Success) { return $null }
    $p = $pm.Groups[1].Value -replace '\\\\', '\'
    if (-not (Test-Path $p)) { return $null }
    $img = [System.Drawing.Image]::FromFile($p)
    $d = [pscustomobject]@{ w = $img.Width; h = $img.Height; path = $p }
    $img.Dispose()
    return $d
}
function Capture-Hash([int]$id) {
    $r = Invoke-Agentctl ('{"tool":"capture_window","args":{"id":' + $id + '}}')
    $pm = [regex]::Match($r, '"path\\?":\\?"([^"]*)"')
    if (-not $pm.Success) { return "" }
    $p = $pm.Groups[1].Value -replace '\\\\', '\'
    if (-not (Test-Path $p)) { return "" }
    return (Get-FileHash $p -Algorithm SHA256).Hash
}
# The taskbar is the short-wide layer (1280x40 requested; the server reshapes
# the width to the desktop). A denied-shell-register client would also be
# 1280x40 - the list_windows guard below rules it out: the real shell never
# appears there. The scan range must grow over the probe run: every
# agentctl/MCP call consumes a control-only client id from the same counter
# (measured diag_tb8e: ~35 calls -> the relaunched taskbar landed at id 25),
# so by cycle2 the shell id sits far above the fresh-server id 1.
function Find-TaskbarId([int]$maxId) {
    for ($i = 1; $i -le $maxId; $i++) {
        $d = Cap-Dims $i
        if ($null -ne $d -and $d.h -le 120 -and $d.w -ge 800) {
            $r = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
            if (-not ($r -match '"title\\?":\s*\\"?Taskbar\\"?')) { return $i }
            Write-Output "DIAG id=$i is a non-shell Taskbar client (register denied)"
        }
    }
    return 0
}
function Get-WindowPid([string]$title) {
    $r = Invoke-Mcp '{"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"list_windows","arguments":{}}}'
    $script:lastList = $r
    $m = [regex]::Match($r, '\\"id\\":(\d+),\\"title\\":\\"' + $title + '\\",\\"pid\\":(\d+)')
    if (-not $m.Success) { return 0 }
    return [int]$m.Groups[2].Value
}
$script:cycleOk = $false
function Invoke-ConquestCycle([string]$tag) {
    $script:cycleOk = $true
    # (1)+(2) observe: find the shell layer, capture a stable h0, verify the
    #     list_windows exclusion.
    $tbId = 0
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 500
        $tbId = Find-TaskbarId 40
        if ($tbId -gt 0) { break }
    }
    Check "$tag-find-taskbar" ($tbId -gt 0)
    if ($tbId -eq 0) { $script:cycleOk = $false; return }
    $script:tbId = $tbId
    $r = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    Check "$tag-observe-shell-excluded-from-list" (-not ($r -match 'Taskbar'))
    $h0 = Capture-Hash $tbId
    Check "$tag-observe-capture" ($h0 -ne "")
    $h0b = Capture-Hash $tbId
    Check "$tag-observe-static" ($h0b -ne "" -and $h0b -eq $h0)
    if (-not ($h0 -ne "" -and $h0b -eq $h0)) { $script:cycleOk = $false; return }
    # (3) drive 1: launch tetris -> the shell protocol pushes the window list
    #     -> the taskbar gains a button.
    Invoke-Mcp '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"launch_app","arguments":{"app":"tetris"}}}' | Out-Null
    $tetPid = 0
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 500
        $tetPid = Get-WindowPid "Tetris"
        if ($tetPid -gt 0) { break }
    }
    Check "$tag-drive-launch-tetris" ($tetPid -gt 0)
    if ($tetPid -eq 0) { $script:cycleOk = $false; return }
    $h1 = ""
    for ($i = 0; $i -lt 12; $i++) {
        Start-Sleep -Milliseconds 500
        $h1 = Capture-Hash $tbId
        if ($h1 -ne "" -and $h1 -ne $h0) { break }
    }
    Check "$tag-verify-button-hash" ($h1 -ne "" -and $h1 -ne $h0)
    # (4) drive 2: the design exclusion - synthetic input on the shell is
    #     bad_target (docs/62 s2). Reaching this check needs send_input allow;
    #     the ask path would return an approval error instead.
    $si = Invoke-Mcp ('{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $tbId + ',"op":"click","x":10,"y":10}}}')
    Check "$tag-drive-send-input-bad-target" ($si -match 'bad_target')
    # (5) drive 3: kill tetris -> the dead window disappears from the taskbar
    #     (shell protocol) -> exact hash restore (static render, no clock).
    Stop-Process -Id $tetPid -Force -ErrorAction SilentlyContinue
    $h2 = ""
    for ($i = 0; $i -lt 12; $i++) {
        Start-Sleep -Milliseconds 500
        $h2 = Capture-Hash $tbId
        if ($h2 -ne "" -and $h2 -eq $h0) { break }
    }
    Check "$tag-verify-restore-hash" ($h2 -ne "" -and $h2 -eq $h0)
    $still = Capture-Hash $tbId
    Check "$tag-verify-alive" ($still -ne "")
    if (-not (($tetPid -gt 0) -and ($h1 -ne "" -and $h1 -ne $h0) -and
        ($si -match 'bad_target') -and ($h2 -eq $h0) -and ($still -ne ""))) {
        $script:cycleOk = $false
    }
    if ($tag -eq "cycle1") { $script:h0c1 = $h0; $script:h1c1 = $h1 }
    if ($tag -eq "cycle2") {
        Check "$tag-verify-home-deterministic" ($h0 -eq $script:h0c1)
        Check "$tag-verify-button-deterministic" ($h1 -eq $script:h1c1)
        if (-not (($h0 -eq $script:h0c1) -and ($h1 -eq $script:h1c1))) { $script:cycleOk = $false }
    }
}

Stop-ProbeProcs
Start-Sleep -Seconds 1
# permissions: send_input must be ALLOW to reach the bad_target check (the
# ask gate would answer with an approval error before the shell exclusion).
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
    Invoke-ConquestCycle "cycle1"
    Check "cycle1" $script:cycleOk
    # window.created via events_list fired counter - delivery-independent.
    $stats = Invoke-Ctl '{"tool":"events_list","args":{}}'
    $wm = [regex]::Match($stats, '"topic":"window\.created"[^}]*"fired":(\d+)')
    Check "launch-window-created-event" ($wm.Success -and ([int]$wm.Groups[1].Value) -ge 1)

    # (6) recover: kill the shell itself. The server survives shell-less
    #     (measured diag_tb8). list_windows never shows the shell, so the
    #     recover-gone hard gate is capture_window -> window_not_found.
    $gone = $false
    $tbProc = Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
        Where-Object { $_.CommandLine -match '--client taskbar' } |
        Select-Object -First 1
    if ($null -ne $tbProc -and $script:tbId -gt 0) {
        Stop-Process -Id $tbProc.ProcessId -Force -ErrorAction SilentlyContinue
        for ($i = 0; $i -lt 20; $i++) {
            Start-Sleep -Milliseconds 500
            $r2 = Invoke-Agentctl ('{"tool":"capture_window","args":{"id":' + $script:tbId + '}}')
            if ($r2 -match 'window_not_found') { $gone = $true; break }
        }
    }
    Check "recover-gone" $gone
    $stats = Invoke-Ctl '{"tool":"events_list","args":{}}'
    $sm = [regex]::Match($stats, '"topic":"app\.crashed"[^}]*"fired":(\d+)')
    Check "recover-app-crashed-stat" ($sm.Success -and ([int]$sm.Groups[1].Value) -ge 1)
    Start-Sleep -Seconds 1
    if ($gone) {
        # cycle 2: the shell is a ROLE (first ShellRegister wins) - relaunch
        # via the tool surface. The register can race the disconnect sweep
        # (denied = the new client lives shell-less), so poll + retry; the
        # scan ceiling grows per attempt because the probe's own tool calls
        # keep consuming client ids.
        $tbId2 = 0
        for ($i = 0; $i -lt 4 -and $tbId2 -eq 0; $i++) {
            Invoke-Mcp '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"launch_app","arguments":{"app":"taskbar"}}}' | Out-Null
            for ($j = 0; $j -lt 3 -and $tbId2 -eq 0; $j++) {
                Start-Sleep -Seconds 2
                $tbId2 = Find-TaskbarId (60 + 40 * $i)
            }
        }
        Check "cycle2-shell-reregistered" ($tbId2 -gt 0)
        if ($tbId2 -gt 0) {
            Invoke-ConquestCycle "cycle2"
            Check "cycle2-after-recover" $script:cycleOk
        } else {
            Write-Output "DIAG cycle2-skipped: relaunched taskbar never registered as shell"
        }
    } else {
        Write-Output "DIAG cycle2-skipped: stale taskbar layer still capturable"
    }
} finally {
    Stop-ProbeProcs
    if ($permExisted -and (Test-Path "$perm.probe_bak")) {
        Copy-Item "$perm.probe_bak" $perm -Force
        Remove-Item "$perm.probe_bak" -Force
        Write-Output "NOTICE: permissions.json restored"
    } elseif (-not $permExisted) {
        Remove-Item $perm -ErrorAction SilentlyContinue
        Write-Output "NOTICE: probe-local permissions.json removed"
    }
    Write-Output "NOTICE: the desktop server is left stopped - restart jkwinserver.exe to resume the live desktop"
}
if ($script:fail -gt 0) { Write-Output "FAILED: $($script:fail)"; exit 1 }
Write-Output "CONQUEST PASS"
exit 0