# send_input tool probe (spec 2026-09-21-conquest-ladder Task 1).
# Conventions: probe_agent_maximize.ps1 (server lifecycle + MCP pipe +
# escaped tool-text regex), lesson 42 (pid-only client kill), ASCII-only
# PS5.1, "> log 2>&1" redirect. The probe edits permissions.json -> backup,
# restore in finally, console notice (docs/59 s16.1 lesson).
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\.claude\worktrees\conquest-ladder\engine\build"
$exe   = "$build\jkdesktop.exe"
$agnt  = "$build\jkagentd.exe"
$perm  = "$build\permissions.json"
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

Stop-ProbeProcs
Start-Sleep -Seconds 1
# permissions: send_input must be ALLOW for Task 1 (default is ask = parked).
# RMW: set the key on the existing file, restore original bytes in finally.
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

    Invoke-Mcp '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"launch_app","arguments":{"app":"minesweeper"}}}' | Out-Null
    $win = $null
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 500
        $r = Invoke-Mcp '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"list_windows","arguments":{}}}'
        $m = [regex]::Match($r, '\\"id\\":(\d+),\\"title\\":\\"([^"\\]*)\\",\\"pid\\":(\d+),' +
                             '\\"x\\":(-?\d+),\\"y\\":(-?\d+),\\"w\\":(\d+),\\"h\\":(\d+)')
        if ($m.Success) {
            $win = [pscustomobject]@{ id=[int]$m.Groups[1].Value; pid=[int]$m.Groups[3].Value
                x=[int]$m.Groups[4].Value; y=[int]$m.Groups[5].Value
                w=[int]$m.Groups[6].Value; h=[int]$m.Groups[7].Value }
            break
        }
    }
    Check "launch-window" ($null -ne $win)
    if ($null -ne $win) {
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
        $h0 = Capture-Hash $win.id
        Check "capture-before" ($h0 -ne "")

        $cx = $win.x + [int]($win.w / 2)
        $cy = $win.y + [int]($win.h / 2)
        $r = Invoke-Mcp ('{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"click","x":' + $cx + ',"y":' + $cy + '}}}')
        Check "click-ok" ($r -match 'sent\\":true')
        Start-Sleep -Milliseconds 700
        $h1 = Capture-Hash $win.id
        Check "click-changes-pixels" ($h1 -ne "" -and $h1 -ne $h0)

        $r = Invoke-Mcp '{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"send_input","arguments":{"id":9999,"op":"click","x":10,"y":10}}}'
        Check "bad-id" ($r -match 'window_not_found')
        $r = Invoke-Mcp ('{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"nosuch"}}}')
        Check "bad-op" ($r -match 'bad_op')
        $r = Invoke-Mcp ('{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"key","key":1073741883}}}')
        Check "key-ok" ($r -match 'sent\\":true')   # SDLK_F2 smoke: delivery only
        $r = Invoke-Mcp ('{"jsonrpc":"2.0","id":8,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"key","key":0}}}')
        Check "bad-key" ($r -match 'bad_key')
        $r = Invoke-Mcp '{"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"send_input","arguments":{"id":9999,"op":"wheel","dy":1}}}'
        Check "wheel-nosuch" ($r -match 'window_not_found')
        $r = Invoke-Mcp ('{"jsonrpc":"2.0","id":10,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"wheel","dy":1}}}')
        Check "wheel-ok" ($r -match 'sent\\":true')
    }
} finally {
    Stop-ProbeProcs
    Copy-Item "$perm.probe_bak" $perm -Force
    Remove-Item "$perm.probe_bak" -Force
    Write-Output "NOTICE: permissions.json restored"
    Write-Output "NOTICE: the desktop server is left stopped (probe_workshop convention) - restart jkwinserver.exe to resume the live desktop"
}
if ($script:fail -gt 0) { Write-Output "FAILED: $($script:fail)"; exit 1 }
Write-Output "ALL PASS"
exit 0