# probe_window_geom.ps1 - window_move / window_resize server tools
# (spec 2026-09-21-phone-practical-improvements task 2).
# ASCII-only (PS5.1). The phone chat LLM rearranges windows: this probe pins
# the geometry contract on the agent tool surface ONLY (agentctl + jkagentd
# MCP, no OS input).
#
# Checks:
#   c1  spawn minesweeper -> list_windows fields (id/title/pid/x/y/w/h)
#   c2  window_move (200,150) -> ok + list_windows x==200 y==150
#   c3  window_resize 500x400 -> ok + list w==500 h==400, position preserved
#   c4  no-op resize: same (w,h) again -> ok (current-pixel-size early out)
#   c5  bad_args: move x=40000 / move missing y / resize w=79 / w=8193 /
#       resize missing h
#   c6  omitted-id from a CONTROL-ONLY caller (agentctl) -> no_window
#       (both tools)
#   c7  explicit unknown id 65000 -> no_window (window_fullscreen parity)
#   c8  fullscreen-state rejection: window_fullscreen on=1 -> move/resize
#       both window_fullscreen_state; restore on=0 -> pre-fs rect back
#       (200,150,500,400)
#   c9  broker end-to-end: jkagentd tools/list exposes both + tools/call
#       window_move/window_resize relay ok (raw args passthrough branch)
#   SKIPPED (harness limit, noted in the report): window_maximized rejection.
#       Maximizing needs a chrome title-bar interaction, and send_input
#       synthetic clicks bypass TryChromeGrab (server chrome) - they are
#       delivered to the client surface (ExecuteSendInputOp). No tool can
#       maximize a window today, so the preMaxRects_ branch is untestable
#       from the outside. Same for window_not_found (only shell/overlay/
#       control-only targets hit it, none enumerable via list_windows).
#
# Conventions: probe_conquest_minesweeper (lifecycle/Invoke-Agentctl theme
# filter), lesson 42 (kill spawned clients by PID - close_window defaults
# deny and this probe never touches permissions.json; both new tools are
# default allow with no file at all).
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe = "$build\jkdesktop.exe"
$agnt = "$build\jkagentd.exe"
$script:fail = 0
function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Output "PASS: $name" }
    else { $script:fail++; Write-Output "FAIL: $name -- $detail" }
    if ($detail) { Write-Output "      $detail" }
}
function Invoke-Agentctl([string]$json) {
    # PS5.1 native-arg quoting (probe_conquest idiom): escape the quotes,
    # then slice the first JSON row - the server prints "[theme] ..." loader
    # lines to stdout (docs/52 lesson).
    $escaped = $json -replace '"', '\"'
    $out = (& $exe agentctl $escaped 2>$null) -join "`n"
    $idx = $out.IndexOf('{')
    if ($idx -lt 0) { return "" }
    return $out.Substring($idx)
}
function Invoke-Mcp([string]$line) {
    # One-shot jsonrpc call piped to jkagentd (probe_agent_e2e convention).
    $out = ($line | & $agnt 2>$null)
    return ($out -join "`n")
}
# list_windows parser: first window row (the fresh server lists only the
# spawned client - taskbar is IsShell-excluded). Unescaped server-channel
# JSON, so the field regex is plain.
function Get-TargetWindow {
    $r = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    $script:lastList = $r
    $m = [regex]::Match($r, '"id":(\d+),"title":"([^"]*)","pid":(\d+),' +
                         '"x":(-?\d+),"y":(-?\d+),"w":(\d+),"h":(\d+)')
    if (-not $m.Success) { return $null }
    return [pscustomobject]@{
        id = [int]$m.Groups[1].Value; title = $m.Groups[2].Value
        pid = [int]$m.Groups[3].Value; x = [int]$m.Groups[4].Value
        y = [int]$m.Groups[5].Value;   w = [int]$m.Groups[6].Value
        h = [int]$m.Groups[7].Value
    }
}
function Wait-TargetWindow {
    foreach ($i in 1..20) {
        Start-Sleep -Milliseconds 500
        $w = Get-TargetWindow
        if ($null -ne $w) { return $w }
    }
    return $null
}
function Stop-ProbeProcs {
    # Spawned client apps by PID only (lesson 42 - the server shares the
    # jkdesktop.exe image name); the server itself by name.
    Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
        Where-Object { $_.CommandLine -match '--client' } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Get-Process jkdesktop, jkwinserver, jkagentd, jkbridge -ErrorAction SilentlyContinue |
        Stop-Process -Force
}

Stop-ProbeProcs
Start-Sleep -Seconds 1
try {
    Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build -WindowStyle Hidden
    $up = $false
    foreach ($i in 1..30) {
        Start-Sleep -Milliseconds 500
        if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up = $true; break }
    }
    Check "setup-server-up" $up ""
    if (-not $up) { throw "server never came up" }
    Write-Output "NOTICE: the live desktop server was stopped for the probe run (jkbridge also stopped - phone web link down)"

    # ---- setup: spawn minesweeper (320x380) ---------------------------------
    Invoke-Agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}' | Out-Null
    $w = Wait-TargetWindow
    Check "setup-mine-window" ($null -ne $w) ($script:lastList)
    if ($null -eq $w) { throw "minesweeper window never appeared" }

    # ---- c1: list_windows fields non-degenerate ------------------------------
    Check "c1-list-fields" ($w.id -gt 0 -and $w.pid -gt 0 -and $w.w -gt 0 -and
                            $w.h -gt 0 -and $w.title -match 'Mine') ("$($w.id)/$($w.pid)/$($w.w)x$($w.h) $($w.title)")

    # ---- c2: window_move ------------------------------------------------------
    $mv = Invoke-Agentctl ('{"tool":"window_move","args":{"id":' + $w.id + ',"x":200,"y":150}}')
    Check "c2-move-ack" ($mv -match '"ok"\s*:\s*true') $mv
    Start-Sleep -Milliseconds 300
    $w2 = Get-TargetWindow
    Check "c2-move-rect" ($null -ne $w2 -and $w2.x -eq 200 -and $w2.y -eq 150) ("x=$($w2.x) y=$($w2.y)")

    # ---- c3: window_resize ----------------------------------------------------
    $rs = Invoke-Agentctl ('{"tool":"window_resize","args":{"id":' + $w.id + ',"w":500,"h":400}}')
    Check "c3-resize-ack" ($rs -match '"ok"\s*:\s*true') $rs
    Start-Sleep -Milliseconds 300
    $w3 = Get-TargetWindow
    Check "c3-resize-rect" ($null -ne $w3 -and $w3.w -eq 500 -and $w3.h -eq 400) ("w=$($w3.w) h=$($w3.h)")
    Check "c3-pos-preserved" ($null -ne $w3 -and $w3.x -eq 200 -and $w3.y -eq 150) ("x=$($w3.x) y=$($w3.y)")

    # ---- c4: no-op resize (same pixel size) -----------------------------------
    $noop = Invoke-Agentctl ('{"tool":"window_resize","args":{"id":' + $w.id + ',"w":500,"h":400}}')
    Check "c4-noop-resize-ok" ($noop -match '"ok"\s*:\s*true') $noop

    # ---- c5: bad_args surface ---------------------------------------------------
    $b1 = Invoke-Agentctl ('{"tool":"window_move","args":{"id":' + $w.id + ',"x":40000,"y":150}}')
    Check "c5-move-range" ($b1 -match '"error":"bad_args"') $b1
    $b2 = Invoke-Agentctl ('{"tool":"window_move","args":{"id":' + $w.id + ',"x":10}}')
    Check "c5-move-missing-y" ($b2 -match '"error":"bad_args"') $b2
    $b3 = Invoke-Agentctl ('{"tool":"window_resize","args":{"id":' + $w.id + ',"w":79,"h":400}}')
    Check "c5-resize-low" ($b3 -match '"error":"bad_args"') $b3
    $b4 = Invoke-Agentctl ('{"tool":"window_resize","args":{"id":' + $w.id + ',"w":8193,"h":400}}')
    Check "c5-resize-high" ($b4 -match '"error":"bad_args"') $b4
    $b5 = Invoke-Agentctl ('{"tool":"window_resize","args":{"id":' + $w.id + ',"w":500}}')
    Check "c5-resize-missing-h" ($b5 -match '"error":"bad_args"') $b5

    # ---- c6: omitted id from a control-only caller -> no_window --------------
    $o1 = Invoke-Agentctl '{"tool":"window_move","args":{"x":1,"y":1}}'
    Check "c6-move-omitted-id" ($o1 -match '"error":"no_window"') $o1
    $o2 = Invoke-Agentctl '{"tool":"window_resize","args":{"w":500,"h":400}}'
    Check "c6-resize-omitted-id" ($o2 -match '"error":"no_window"') $o2

    # ---- c7: explicit unknown id (window_fullscreen parity) -------------------
    $u1 = Invoke-Agentctl '{"tool":"window_move","args":{"id":65000,"x":1,"y":1}}'
    Check "c7-move-unknown-id" ($u1 -match '"error":"no_window"') $u1
    $u2 = Invoke-Agentctl '{"tool":"window_resize","args":{"id":65000,"w":500,"h":400}}'
    Check "c7-resize-unknown-id" ($u2 -match '"error":"no_window"') $u2

    # ---- c8: fullscreen-state rejection + restore rect ------------------------
    $fs = Invoke-Agentctl ('{"tool":"window_fullscreen","args":{"id":' + $w.id + ',"on":1}}')
    Check "c8-fs-on" ($fs -match '"ok"\s*:\s*true' -and $fs -match '"fullscreen"\s*:\s*true') $fs
    $f1 = Invoke-Agentctl ('{"tool":"window_move","args":{"id":' + $w.id + ',"x":10,"y":10}}')
    Check "c8-move-fs-state" ($f1 -match '"error":"window_fullscreen_state"') $f1
    $f2 = Invoke-Agentctl ('{"tool":"window_resize","args":{"id":' + $w.id + ',"w":300,"h":300}}')
    Check "c8-resize-fs-state" ($f2 -match '"error":"window_fullscreen_state"') $f2
    $fsOff = Invoke-Agentctl ('{"tool":"window_fullscreen","args":{"id":' + $w.id + ',"on":0}}')
    Check "c8-fs-off" ($fsOff -match '"ok"\s*:\s*true' -and $fsOff -match '"fullscreen"\s*:\s*false') $fsOff
    Start-Sleep -Milliseconds 300
    $w4 = Get-TargetWindow
    Check "c8-restore-rect" ($null -ne $w4 -and $w4.x -eq 200 -and $w4.y -eq 150 -and
                             $w4.w -eq 500 -and $w4.h -eq 400) ("$($w4.x),$($w4.y) $($w4.w)x$($w4.h)")
    # post-restore the geometry tools work again (rejection was state-only)
    $f3 = Invoke-Agentctl ('{"tool":"window_move","args":{"id":' + $w.id + ',"x":200,"y":150}}')
    Check "c8-move-after-restore" ($f3 -match '"ok"\s*:\s*true') $f3

    # ---- c9: broker end-to-end (jkagentd MCP relay) ---------------------------
    # tools/list must expose both (registration) and tools/call must relay
    # (raw args passthrough branch). Result text rides content[0].text with
    # escaped quotes (probe_agent_e2e convention).
    $lst = Invoke-Mcp '{"jsonrpc":"2.0","id":1,"method":"tools/list"}'
    Check "c9-list-exposed" ($lst -match '"name":"window_move"' -and
                             $lst -match '"name":"window_resize"') ""
    # PS5.1 argument-mode trap: the concatenation must be parenthesized -
    # bare 'a' + $x + 'b' in a command argument binds only the first string
    # fragment (probe_conquest idiom), which arrives as unterminated JSON.
    $m1 = Invoke-Mcp ('{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"window_move","arguments":{"id":' + $w.id + ',"x":260,"y":180}}}')
    Check "c9-mcp-move" ($m1 -match 'ok\\":true') $m1
    $m2 = Invoke-Mcp ('{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"window_resize","arguments":{"id":' + $w.id + ',"w":480,"h":380}}}')
    Check "c9-mcp-resize" ($m2 -match 'ok\\":true') $m2
    Start-Sleep -Milliseconds 300
    $w5 = Get-TargetWindow
    Check "c9-mcp-rect-reflects" ($null -ne $w5 -and $w5.x -eq 260 -and $w5.y -eq 180 -and
                                  $w5.w -eq 480 -and $w5.h -eq 380) ("$($w5.x),$($w5.y) $($w5.w)x$($w5.h)")

    # ---- cleanup: kill the client by PID (close_window defaults deny) ---------
    if ($null -ne $w5 -and $w5.pid -gt 0) {
        Stop-Process -Id $w5.pid -Force -ErrorAction SilentlyContinue
        $gone = $false
        foreach ($i in 1..20) {
            Start-Sleep -Milliseconds 400
            [void](Get-TargetWindow)
            if ($script:lastList -notmatch ('"pid":' + $w5.pid + '(?![0-9])')) { $gone = $true; break }
        }
        Check "cleanup-client-gone" $gone ("pid=$($w5.pid)")
    }
} finally {
    Stop-ProbeProcs
    Write-Output "NOTICE: the live desktop server AND jkbridge were stopped for the probe run - restart jkdesktop.exe --server AND jkbridge.exe to resume (phone web link dies silently without jkbridge)"
}
Write-Output ("RESULT: " + $(if ($script:fail -eq 0) { "ALL PASS" } else { "$($script:fail) FAILURE(S)" }))
exit $(if ($script:fail -eq 0) { 0 } else { 1 })