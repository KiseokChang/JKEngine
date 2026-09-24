# Conquest contract probe: browser app (spec 2026-09-21-conquest-ladder, rung 7).
# Track B drive: send_input click on the URL bar -> type a local file:// URL ->
# key RETURN(13) commits (InputText Enter commit, ClientBrowserApp.cpp BuildUi).
#   - about:blank is NOT usable: Navigate() prefixes bare hosts with https://
#     (no "://" in "about:blank" -> "https://about:blank" -> network error
#     page, LAN-dependent). The probe navigates to a probe-owned local html
#     file instead (state/conq7_page.html, light background vs the dark home
#     page -> the capture hash is a strong verify signal).
#   - Timing rule measured in calibration (diag_b7): the click needs ~800ms
#     to settle before typing - 400ms raced and the text never landed.
# Verify is 2-stage:
#   1) h0 (home) -> h1 (probe page) hash change; the probe page is static so
#      the hash is stable (measured: h0 = CC8070C9... and h1 = AA103F7A...
#      reproduced identically across 3 calibration runs).
#   2) Home button click round-trip -> h2 != h1; exact hash equality with h0
#      is NOT expected (the URL bar buffer keeps the home URL after Home
#      sets it, and caret/focus state differs from the fresh h0) - the
#      round-trip is instead proven in cycle2 by re-driving the SAME url and
#      requiring h1(cycle2) == h1(cycle1) (deterministic render + repeatable
#      drive across a full recover cycle).
# Recover: pid kill -> recover-gone hard gate -> app.crashed -> cycle2.
# CEF readiness: the page area shows "starting Chromium..." until the first
# texture lands - h0 is taken only after the capture hash stabilizes.
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe = "$build\jkdesktop.exe"; $agnt = "$build\jkagentd.exe"
$ctl = $exe
$perm = "$build\permissions.json"
$page = "$build\state\conq7_page.html"
$url = "file:///I:/progwork/JKENGINE/engine/build/state/conq7_page.html"
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
function Get-BrowserWindow {
    $r = Invoke-Mcp '{"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"list_windows","arguments":{}}}'
    $script:lastList = $r
    $m = [regex]::Match($r, '\\"id\\":(\d+),\\"title\\":\\"Browser\\",\\"pid\\":(\d+),' +
                         '\\"x\\":(-?\d+),\\"y\\":(-?\d+),\\"w\\":(\d+),\\"h\\":(\d+)')
    if (-not $m.Success) { return $null }
    return [pscustomobject]@{
        id = [int]$m.Groups[1].Value; title = "Browser"
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
# Wait for the page to CHANGE away from $baseline and then SETTLE: the
# changed hash is only returned once two consecutive captures agree. The
# change-poll alone is not enough - the first differing capture can be a
# mid-render transition state (measured batch_browser_fix1: home-static
# FAILed because the settle happened between the two captures).
function Wait-ChangedStable([int]$wid, [string]$baseline) {
    $c = ""
    for ($i = 0; $i -lt 30; $i++) {
        Start-Sleep -Milliseconds 500
        $c = Capture-Hash $wid
        if ($c -ne "" -and $c -ne $baseline) { break }
    }
    if ($c -eq "" -or $c -eq $baseline) { return "" }
    for ($i = 0; $i -lt 20; $i++) {
        $c2 = Capture-Hash $wid
        if ($c2 -ne "" -and $c2 -eq $c) { return $c }
        if ($c2 -eq "") { return $c }
        $c = $c2
    }
    return $c
}
# CEF readiness: poll until two consecutive captures agree (the page area
# flips from "starting Chromium..." text to the loaded page texture).
function Wait-Ready([int]$wid) {
    $prev = ""
    for ($i = 0; $i -lt 40; $i++) {
        Start-Sleep -Milliseconds 500
        $h = Capture-Hash $wid
        if ($h -ne "" -and $h -eq $prev) { return $h }
        $prev = $h
    }
    return $prev
}
# The drive, measured in calibration (diag_b7/b7d/b7e): click the URL bar
# (surface ~(300,54) inside the bar row y=30..110), wait 800ms for the
# InputText to settle, type the URL, Enter. Enter works because the app
# feeds every event to the ImGui backend first (PreProcessMessage) - the
# WantTextInput/WantCaptureKeyboard gates only decide CEF forwarding.
function Drive-Navigate([object]$win) {
    $r1 = Invoke-Mcp ('{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"click","x":' + ($win.x + 300) + ',"y":' + ($win.y + 54) + '}}}')
    Check "drive-urlbar-click" ($r1 -match 'sent\\":true')
    Start-Sleep -Milliseconds 800
    $r2 = Invoke-Mcp ('{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"type","text":"' + $url + '"}}}')
    Check "drive-type-url" ($r2 -match 'sent\\":true')
    Start-Sleep -Milliseconds 500
    $r3 = Invoke-Mcp ('{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"key","key":13}}}')
    Check "drive-key-return" ($r3 -match 'sent\\":true')
    Start-Sleep -Seconds 3
}
$script:cycleOk = $false
function Invoke-ConquestCycle([string]$tag) {
    $script:cycleOk = $true
    # (1) launch via the tool surface.
    Invoke-Mcp '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"launch_app","arguments":{"app":"browser"}}}' | Out-Null
    $win = $null
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 500
        $win = Get-BrowserWindow
        if ($null -ne $win) { break }
    }
    Check "$tag-launch-window" ($null -ne $win)
    if ($null -eq $win) { $script:cycleOk = $false; return }
    $script:win = $win
    # (2) observe: fields non-degenerate + CEF readiness (stable h0).
    $obs = ($win.id -gt 0 -and $win.pid -gt 0 -and $win.w -eq 960 -and $win.h -eq 640)
    Check "$tag-observe-fields" $obs
    if (-not $obs) { $script:cycleOk = $false }
    $h0 = Wait-Ready $win.id
    Check "$tag-observe-capture" ($h0 -ne "")
    if ($h0 -eq "") { $script:cycleOk = $false; return }
    # (3)+(4) drive + verify: navigate to the probe page.
    Drive-Navigate $win
    # Poll for the hash CHANGE and settle (Wait-ChangedStable) instead of one
    # capture after a fixed wait: the batch official run (2026-09-24)
    # measured CEF nav+render exceeding the old 3 s under back-to-back
    # server restarts, and even the bare change-poll grabbed a mid-render
    # transition as the home target. The assertions are unchanged - a
    # different, stable hash - only the arrival wait is.
    $h1 = Wait-ChangedStable $win.id $h0
    Check "$tag-verify-nav-hash" ($h1 -ne "" -and $h1 -ne $h0)
    if ($h1 -eq "") { $script:cycleOk = $false; return }
    # the probe page is static - the hash must hold (validates the verify).
    $h1b = Capture-Hash $win.id
    Check "$tag-verify-nav-static" ($h1b -ne "" -and $h1b -eq $h1)
    # Home round-trip: click Home (surface ~(849,58)), page returns home.
    $r4 = Invoke-Mcp ('{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"click","x":' + ($win.x + 849) + ',"y":' + ($win.y + 58) + '}}}')
    Check "$tag-drive-home-click" ($r4 -match 'sent\\":true')
    $h2 = Wait-ChangedStable $win.id $h1
    Check "$tag-verify-home-hash" ($h2 -ne "" -and $h2 -ne $h1)
    $h2b = Capture-Hash $win.id
    Check "$tag-verify-home-static" ($h2b -ne "" -and $h2b -eq $h2)
    $again = Get-BrowserWindow
    Check "$tag-verify-alive" ($null -ne $again -and $again.id -eq $win.id)
    if (-not (($h1 -ne "" -and $h1 -ne $h0) -and ($h1b -eq $h1) -and
        ($r4 -match 'sent\\":true') -and ($h2 -ne "" -and $h2 -ne $h1) -and
        ($h2b -eq $h2) -and ($null -ne $again -and $again.id -eq $win.id))) {
        $script:cycleOk = $false
    }
    # cross-cycle witnesses carried on the script scope
    if ($tag -eq "cycle1") { $script:h0c1 = $h0; $script:h1c1 = $h1 }
    if ($tag -eq "cycle2") {
        Check "$tag-verify-home-deterministic" ($h0 -eq $script:h0c1)
        Check "$tag-verify-nav-deterministic" ($h1 -eq $script:h1c1)
        if (-not (($h0 -eq $script:h0c1) -and ($h1 -eq $script:h1c1))) { $script:cycleOk = $false }
    }
}

Stop-ProbeProcs
Start-Sleep -Seconds 1
# PROBE FILE: state/conq7_page.html is probe-owned (light page vs the dark
# home page) - written at start, removed in finally.
'<!DOCTYPE html><html><head><meta charset="utf-8"><style>body{background:#cfe8ff;color:#102030;font-family:sans-serif;}h1{color:#102030;}</style></head><body><h1>CONQRUN7</h1><p>probe conquest page</p></body></html>' |
    Set-Content $page -Encoding ASCII
# permissions: send_input must be ALLOW (default is ask).
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

    # (6) recover: kill the client by PID -> recover-gone hard gate ->
    #     app.crashed -> cycle2 (fresh browser re-driven to the same URL).
    $gone = $false
    if ($null -ne $script:win) {
        Stop-Process -Id $script:win.pid -Force -ErrorAction SilentlyContinue
        for ($i = 0; $i -lt 20; $i++) {
            Start-Sleep -Milliseconds 500
            [void](Get-BrowserWindow)
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
        Invoke-ConquestCycle "cycle2"
        Check "cycle2-after-recover" $script:cycleOk
    } else {
        Write-Output "DIAG cycle2-skipped: stale pid $($script:win.pid) still in list_windows"
    }
} finally {
    Stop-ProbeProcs
    Remove-Item $page -Force -ErrorAction SilentlyContinue
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