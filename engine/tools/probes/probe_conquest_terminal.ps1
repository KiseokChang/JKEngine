# Conquest contract probe: terminal (spec 2026-09-21-conquest-ladder s2, rung 4).
# Track B drive via send_input: type a marker echo + Enter into the pty, then
# verify BOTH on the tool surface - the terminal.output event (the app's own
# output topic) AND the capture hash change. launch -> observe -> drive ->
# verify -> recover, agent tool surface ONLY (never OS SendInput - spec s3.2).
# Template: probe_conquest_minesweeper.ps1 (skeleton + permissions RMW
# verbatim - send_input default gate is ask).
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe = "$build\jkdesktop.exe"; $agnt = "$build\jkagentd.exe"
$ctl = $exe   # probe_agent_events.ps1 names the exe $ctl - the verbatim
              # Invoke-Ctl helper below references that spelling.
$perm = "$build\permissions.json"
$marker = "JKCONQOK2623"
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
function Get-TermWindow {
    # Window fields live ESCAPED inside the tool text (template regex
    # verbatim). Fresh server: the only listed window is the terminal client.
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
$script:cycleOk = $false
function Invoke-ConquestCycle([string]$tag) {
    $script:cycleOk = $true
    # (1) launch: launch_app + window appearance poll (hard gate). The pty
    #     shell boot can take a moment - the poll IS the readiness gate.
    Invoke-Mcp '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"launch_app","arguments":{"app":"terminal"}}}' | Out-Null
    $win = $null
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 500
        $win = Get-TermWindow
        if ($null -ne $win) { break }
    }
    Check "$tag-launch-window" ($null -ne $win)
    if ($null -eq $win) { $script:cycleOk = $false; return }
    $script:win = $win
    # (2) observe: fields non-degenerate + title + pre-drive capture hash.
    $obs = ($win.id -gt 0 -and $win.pid -gt 0 -and $win.w -gt 0 -and $win.h -gt 0 -and $win.title -match 'Terminal')
    Check "$tag-observe-fields" $obs
    if (-not $obs) { $script:cycleOk = $false }
    $h0 = Capture-Hash $win.id
    Check "$tag-observe-capture" ($h0 -ne "")
    # (3) drive: type the marker echo + Enter - send_input type + key(SDLK_
    #     RETURN=13). Subscribe BEFORE the typing (lesson 28: pushes are not
    #     replayed) and poll the drain for the app's own terminal.output.
    $evJob = Start-Job -ScriptBlock { param($e) (& $e agent-events 20) -join "`n" } `
        -ArgumentList $exe
    Start-Sleep -Seconds 2
    $r1 = Invoke-Mcp ('{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"type","text":"echo ' + $marker + '"}}}')
    Check "$tag-drive-type" ($r1 -match 'sent\\":true')
    $r2 = Invoke-Mcp ('{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"key","key":13}}}')
    Check "$tag-drive-enter" ($r2 -match 'sent\\":true')
    # (4) verify: the output event carries the marker (the app's own echo of
    #     the executed command) + pixels moved + same id still alive.
    #     agent-events CLI output is FULLY buffered (GUI-subsystem exe,
    #     probe_agent_trust lesson 1) - Receive-Job returns NOTHING until the
    #     CLI exits, so polling mid-run is structurally blind. Bounded sleep
    #     for the drive window, then one blocking drain (duration convention).
    Start-Sleep -Seconds 6
    $outEv = (Receive-Job -Job $evJob -Wait 2>&1) | Out-String
    Check "$tag-verify-output-event" ($outEv -match 'terminal\.output' -and $outEv -match $marker)
    try { Stop-Job $evJob -ErrorAction SilentlyContinue; Remove-Job $evJob -Force -ErrorAction SilentlyContinue } catch {}
    # Corroboration: the publisher's own fired counter (delivery-independent).
    $ts = Invoke-Ctl '{"tool":"events_list","args":{}}'
    $tm = [regex]::Match($ts, '"topic":"terminal\.output"[^}]*"fired":(\d+)')
    Check "$tag-verify-output-fired" ($tm.Success -and ([int]$tm.Groups[1].Value) -ge 1)
    $h1 = Capture-Hash $win.id
    Check "$tag-verify-hash" ($h1 -ne "" -and $h1 -ne $h0)
    $again = Get-TermWindow
    Check "$tag-verify-alive" ($null -ne $again -and $again.id -eq $win.id)
    if (-not (($r1 -match 'sent\\":true') -and ($r2 -match 'sent\\":true') -and
        ($outEv -match 'terminal\.output' -and $outEv -match $marker) -and
        ($h1 -ne "" -and $h1 -ne $h0) -and
        ($null -ne $again -and $again.id -eq $win.id))) {
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
    # is singular (deviation finding: two concurrent agent-events clients, the
    # outer Watch-Events job here + the cycle-internal drain job, starved the
    # cycle's terminal.output delivery), so the cycle owns the only subscription.
    $stats = Invoke-Ctl '{"tool":"events_list","args":{}}'
    $wm = [regex]::Match($stats, '"topic":"window\.created"[^}]*"fired":(\d+)')
    Check "launch-window-created-event" ($wm.Success -and ([int]$wm.Groups[1].Value) -ge 1)

    # (5) recover: kill the client by PID -> app.crashed -> relaunch -> full
    #     cycle again. Hard gate: recover-gone (pid leaves list_windows).
    $job2 = Watch-Events 15
    Start-Sleep -Seconds 2
    $gone = $false
    if ($null -ne $script:win) {
        Stop-Process -Id $script:win.pid -Force -ErrorAction SilentlyContinue
        for ($i = 0; $i -lt 20; $i++) {
            Start-Sleep -Milliseconds 500
            [void](Get-TermWindow)
            if (-not ($script:lastList -match ('\\"pid\\":' + $script:win.pid + '(?![0-9])'))) {
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
        Write-Output "DIAG cycle2-skipped: stale pid $($script:win.pid) still in list_windows"
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