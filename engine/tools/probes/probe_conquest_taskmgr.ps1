# Conquest contract probe: taskmgr (spec 2026-09-21-conquest-ladder, rung 5).
# Track B drive via send_input on an ImGui app: select the Minesweeper row in
# the Task Manager table, then click the Activate button - the window focus
# flips on the tool surface. Verify = list_windows focused flags (state, the
# strong gate) + window.focused fired counter. NOTE: the capture hash is NOT
# a verify signal here - taskmgr self-redraws (live CPU% + ImPlot history),
# so its hash changes with no drive at all (measured). Row mapping rule: the
# table lists windows in SPAWN order (not pid value - measured), so the probe
# launches minesweeper FIRST and it is always row 1 at surface y~57.
# launch -> observe -> drive -> verify -> recover, agent tool surface ONLY
# (never OS SendInput - spec s3.2). Template: probe_conquest_terminal.ps1
# (skeleton + permissions RMW verbatim - send_input default gate is ask).
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe = "$build\jkdesktop.exe"; $agnt = "$build\jkagentd.exe"
$ctl = $exe   # probe_agent_events.ps1 names the exe $ctl - the verbatim
              # Invoke-Ctl helper below references that spelling.
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
function Get-WinByTitle([string]$t) {
    # Window fields live ESCAPED inside the tool text (template regex
    # verbatim, title spliced in).
    $r = Invoke-Mcp '{"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"list_windows","arguments":{}}}'
    $script:lastList = $r
    $m = [regex]::Match($r, '\\"id\\":(\d+),\\"title\\":\\"' + $t +
        '\\",\\"pid\\":(\d+),\\"x\\":(-?\d+),\\"y\\":(-?\d+),\\"w\\":(\d+),' +
        '\\"h\\":(\d+),\\"focused\\":(true|false)')
    if (-not $m.Success) { return $null }
    return [pscustomobject]@{
        id = [int]$m.Groups[1].Value;   title = $t
        pid = [int]$m.Groups[2].Value;  x = [int]$m.Groups[3].Value
        y = [int]$m.Groups[4].Value;    w = [int]$m.Groups[5].Value
        h = [int]$m.Groups[6].Value;    focused = $m.Groups[7].Value
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
# List-window presence helpers (no focused group needed for the gone gate).
function Pid-Gone([int]$pid2) {
    return -not ($script:lastList -match ('\\"pid\\":' + $pid2 + '(?![0-9])'))
}
$script:cycleOk = $false
function Invoke-ConquestCycle([string]$tag) {
    $script:cycleOk = $true
    # (1) launch: minesweeper FIRST (spawn order = table row order, so the
    #     minesweeper row is row 1), then taskmgr. Hard gates on appearance.
    Invoke-Mcp '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"launch_app","arguments":{"app":"minesweeper"}}}' | Out-Null
    $ms = $null
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 500
        $ms = Get-WinByTitle 'Minesweeper'
        if ($null -ne $ms) { break }
    }
    Check "$tag-launch-minesweeper" ($null -ne $ms)
    if ($null -eq $ms) { $script:cycleOk = $false; return }
    Invoke-Mcp '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"launch_app","arguments":{"app":"taskmgr"}}}' | Out-Null
    $tm = $null
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 500
        $tm = Get-WinByTitle 'Task Manager'
        if ($null -ne $tm) { break }
    }
    Check "$tag-launch-taskmgr" ($null -ne $tm)
    if ($null -eq $tm) { $script:cycleOk = $false; return }
    $script:ms = $ms; $script:tm = $tm
    # Settle: the launch poll breaks on first appearance - the focus handover
    # (newest window owns focus) lands a beat later. Re-poll the baseline.
    $base = $false
    for ($i = 0; $i -lt 8; $i++) {
        Start-Sleep -Milliseconds 500
        $ms = Get-WinByTitle 'Minesweeper'
        $tm = Get-WinByTitle 'Task Manager'
        if ($null -ne $ms -and $null -ne $tm -and
            $tm.focused -eq 'true' -and $ms.focused -eq 'false') {
            $base = $true; break
        }
    }
    # (2) observe: fields non-degenerate + focus baseline (taskmgr launched
    #     last owns focus) + capture path works. The hash VALUE is unused as
    #     a verify signal (self-redraw, see header note).
    $obs = ($ms.id -gt 0 -and $ms.pid -gt 0 -and $tm.id -gt 0 -and
            $tm.pid -gt 0 -and $tm.w -eq 900 -and $tm.h -eq 620)
    Check "$tag-observe-fields" $obs
    if (-not $obs) { $script:cycleOk = $false }
    Check "$tag-observe-focus-baseline" $base
    $h0 = Capture-Hash $tm.id
    Check "$tag-observe-capture" ($h0 -ne "")
    # (3) drive: click the minesweeper row (row 1, surface y~57) to select,
    #     then the Activate button (surface ~38,509). Two send_input calls -
    #     selection first (the button is BeginDisabled(!sel), ClientTaskmgrApp
    #     Activate needs a selected row).
    $r1 = Invoke-Mcp ('{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $tm.id + ',"op":"click","x":' + ($tm.x + 120) + ',"y":' + ($tm.y + 57) + '}}}')
    Check "$tag-drive-select" ($r1 -match 'sent\\":true')
    Start-Sleep -Milliseconds 600
    $r2 = Invoke-Mcp ('{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $tm.id + ',"op":"click","x":' + ($tm.x + 38) + ',"y":' + ($tm.y + 509) + '}}}')
    Check "$tag-drive-activate" ($r2 -match 'sent\\":true')
    Start-Sleep -Seconds 1
    # (4) verify: the focus FLIPPED on the tool surface - minesweeper owns
    #     focus now, taskmgr lost it (SendWindowActivate -> FocusClient).
    $ms2 = Get-WinByTitle 'Minesweeper'
    $tm2 = Get-WinByTitle 'Task Manager'
    $flip = ($null -ne $ms2 -and $null -ne $tm2 -and
             $ms2.focused -eq 'true' -and $tm2.focused -eq 'false')
    Check "$tag-verify-focused-flip" $flip
    # Corroboration: the publisher's own fired counter (delivery-independent).
    $ts = Invoke-Ctl '{"tool":"events_list","args":{}}'
    $fm = [regex]::Match($ts, '"topic":"window\.focused"[^}]*"fired":(\d+)')
    Check "$tag-verify-focused-fired" ($fm.Success -and ([int]$fm.Groups[1].Value) -ge 1)
    Check "$tag-verify-alive" ($null -ne $tm2 -and $tm2.id -eq $tm.id)
    if (-not (($r1 -match 'sent\\":true') -and ($r2 -match 'sent\\":true') -and
        $base -and $flip -and ($fm.Success) -and
        ($null -ne $tm2 -and $tm2.id -eq $tm.id))) {
        $script:cycleOk = $false
    }
}

Stop-ProbeProcs
Start-Sleep -Seconds 1
# permissions: send_input must be ALLOW for the drive stage (default is ask =
# parked). RMW: set the key on the existing file, restore original bytes in
# finally + console notice (docs/59 s16.1 lesson). A silent RMW failure on a
# missing file parks every drive on ask - the guard makes it loud.
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

    $job = Watch-Events 25
    Start-Sleep -Seconds 2
    Invoke-ConquestCycle "cycle1"
    Check "cycle1" $script:cycleOk
    try { Stop-Job $job -ErrorAction SilentlyContinue; Remove-Job $job -Force -ErrorAction SilentlyContinue } catch {}
    # window.created via events_list fired counter - the stream subscriber slot
    # is singular (two concurrent agent-events clients starve each other), so
    # the fired counter is the delivery-independent witness.
    $stats = Invoke-Ctl '{"tool":"events_list","args":{}}'
    $wm = [regex]::Match($stats, '"topic":"window\.created"[^}]*"fired":(\d+)')
    Check "launch-window-created-event" ($wm.Success -and ([int]$wm.Groups[1].Value) -ge 2)

    # (5) recover: kill BOTH clients (taskmgr is the conquered app; killing
    #     minesweeper too keeps cycle2's spawn order - and therefore the row
    #     mapping - identical to cycle1). Hard gate: recover-gone (both pids
    #     leave list_windows).
    $job2 = Watch-Events 15
    Start-Sleep -Seconds 2
    $gone = $false
    if ($null -ne $script:tm -and $null -ne $script:ms) {
        Stop-Process -Id $script:tm.pid -Force -ErrorAction SilentlyContinue
        Stop-Process -Id $script:ms.pid -Force -ErrorAction SilentlyContinue
        for ($i = 0; $i -lt 20; $i++) {
            Start-Sleep -Milliseconds 500
            [void](Get-WinByTitle 'Task Manager')
            if ((Pid-Gone $script:tm.pid) -and (Pid-Gone $script:ms.pid)) {
                $gone = $true
                break
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
        Write-Output "DIAG cycle2-skipped: stale pids still in list_windows"
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
    Write-Output "NOTICE: the desktop server is left stopped (probe_workshop convention) - restart jkwinserver.exe to resume the live desktop"
}
if ($script:fail -gt 0) { Write-Output "FAILED: $($script:fail)"; exit 1 }
Write-Output "CONQUEST PASS"
exit 0