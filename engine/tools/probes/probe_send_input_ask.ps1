# probe_send_input_ask: send_input ask parking + approve-time re-execution
# (spec 2026-09-21-conquest-ladder Task 2). Clone of probe_send_input.ps1
# (server lifecycle + MCP pipe + escaped tool-text regex + capture hash),
# with the approval_request id acquisition from probe_approve_self.ps1
# (7/7 precedent): raw pipe + Hello + AgentEventSubscribe + blocking Read +
# type-20 frame decode. Requester = background-job jkagentd one-shot,
# approver = a fresh agentctl one-shot -> always different connections, so
# the self-approve gate never trips. The raw subscriber connection also
# satisfies the parking approval_unavailable check (JKAgentClient declares
# control-only subscribe=0 - measured in JKAgentClient.cpp:20).
# ASCII-only PS5.1 (lesson 50), "> log 2>&1" redirect, pid-only client kill.
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
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
    # approve ride agentctl like the shot/approve_self probes.
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
function Wait-ApprovalRequest([System.IO.Pipes.NamedPipeClientStream]$s) {
    # probe_approve_self.ps1 convention (lines 98-123): blocking Read frames
    # until the type-20 approval_request push, then take the request id.
    # PS5.1 trap: a second -match overwrites $Matches - capture the request
    # id from the FIRST pattern before testing the tool. The approval_resolved
    # push also carries "request":N (but no "tool") - the tool check skips it.
    $reqId = 0
    foreach ($i in 1..20) {
        $hdr = ReadExact $s 12
        $ftype = [int][BitConverter]::ToUInt32($hdr, 4)
        $flen  = [int][BitConverter]::ToUInt32($hdr, 8)
        if ($ftype -ne 20) {
            if ($flen -gt 0) { ReadExact $s $flen | Out-Null }
            Write-Output "DIAG frame type=$ftype len=$flen"
            continue
        }
        $pl = ReadExact $s $flen
        $ev = [Text.Encoding]::UTF8.GetString($pl, 4, $pl.Length - 4)
        $rid = 0
        if ($ev -match '"request":(\d+)') { $rid = [int]$Matches[1] }
        if ($rid -gt 0 -and $ev -match '"tool":"send_input"') {
            return $rid
        }
        Write-Output "DIAG event: $ev"
    }
    return 0
}

Stop-ProbeProcs
Start-Sleep -Seconds 1
# permissions: send_input must be ASK for Task 2 (parking under test). RMW:
# set the key on the existing file, restore original bytes in finally.
Write-Output "NOTICE: editing $perm (backup+restore)"
Copy-Item $perm "$perm.probe_bak" -Force
try {
    $json = Get-Content $perm -Raw | ConvertFrom-Json
    $json | Add-Member -NotePropertyName send_input -NotePropertyValue "ask" -Force
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

        # --- raw pipe subscriber (probe_approve_self.ps1 lines 37-93) --------
        # Blocking read is the only reliable PS5.1 pipe pattern (Available is
        # null / ReadAsync+Wait stalls on event frames); hang risk is bounded
        # by the server's 60s approval expiry. This connection is the event
        # listener AND the parking approval_unavailable satisfier.
        $pipe = New-Object System.IO.Pipes.NamedPipeClientStream(".", "JKWindowServerPipe",
            [System.IO.Pipes.PipeDirection]::InOut)
        $pipe.Connect(5000)
        function ReadExact([System.IO.Pipes.NamedPipeClientStream]$s, [int]$n) {
            $buf = New-Object byte[] $n; $off = 0
            while ($off -lt $n) {
                $r = $s.Read($buf, $off, $n - $off)
                if ($r -le 0) { throw "pipe closed" }
                $off += $r
            }
            return ,$buf
        }
        function SendMsg([System.IO.Pipes.NamedPipeClientStream]$s, [int]$type, [byte[]]$payload) {
            $hdr = New-Object byte[] 12
            [BitConverter]::GetBytes([uint32]0x4A4B0001).CopyTo($hdr, 0)
            [BitConverter]::GetBytes([uint32]$type).CopyTo($hdr, 4)
            [BitConverter]::GetBytes([uint32]$payload.Length).CopyTo($hdr, 8)
            $s.Write($hdr, 0, 12)
            if ($payload.Length -gt 0) { $s.Write($payload, 0, $payload.Length) }
            $s.Flush()
        }
        # Hello (type 1): HelloPayload = protocolVersion(4)=2 + pid(4).
        $hello = New-Object byte[] 8
        [BitConverter]::GetBytes([uint32]2).CopyTo($hello, 0)
        [BitConverter]::GetBytes([uint32]$PID).CopyTo($hello, 4)
        SendMsg $pipe 1 $hello
        # AgentEventSubscribe (type 19): subscribe=1 - pushes are not replayed,
        # subscribe BEFORE the parked send_input fires.
        $sub = New-Object byte[] 4
        [BitConverter]::GetBytes([uint32]1).CopyTo($sub, 0)
        SendMsg $pipe 19 $sub

        # Requester = background-job jkagentd one-shot (QueryRaw blocks until
        # the parked reply arrives, JKAgentClient.cpp:46-67). Approver = a
        # fresh agentctl one-shot - always a different connection.
        $jobScript = { param($agntPath, $call) ($call | & $agntPath) -join "`n" }

        # --- allow scenario: park -> approve -> re-executed click lands -----
        $call = '{"jsonrpc":"2.0","id":11,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"click","x":' + $cx + ',"y":' + $cy + '}}}'
        $job = Start-Job -ScriptBlock $jobScript -ArgumentList $agnt, $call
        $reqId = Wait-ApprovalRequest $pipe
        Check "1-allow-parked" ($reqId -gt 0)
        if ($reqId -gt 0) {
            $ap = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
            Check "2-allow-approved" ($ap -match 'approved\\?":true')
            $out = (Receive-Job -Wait $job) -join "`n"
            Remove-Job $job -Force
            Check "3-allow-reexec-sent" ($out -match 'sent\\":true')
            Start-Sleep -Milliseconds 700
            $h1 = Capture-Hash $win.id
            Check "4-allow-click-landed" ($h1 -ne "" -and $h1 -ne $h0)
        } else {
            Stop-Job $job -ErrorAction SilentlyContinue
            Remove-Job $job -Force -ErrorAction SilentlyContinue
        }

        # --- deny scenario: park -> deny -> no injection ---------------------
        $call2 = '{"jsonrpc":"2.0","id":12,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"click","x":' + $cx + ',"y":' + $cy + '}}}'
        $job2 = Start-Job -ScriptBlock $jobScript -ArgumentList $agnt, $call2
        $reqId2 = Wait-ApprovalRequest $pipe
        Check "5-deny-parked" ($reqId2 -gt 0)
        if ($reqId2 -gt 0) {
            $ap2 = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId2 + ',"decision":"deny"}}')
            Check "6-deny-resolved" ($ap2 -match 'approved\\?":true')
            $out2 = (Receive-Job -Wait $job2) -join "`n"
            Remove-Job $job2 -Force
            Check "7-deny-denied-by-user" ($out2 -match 'denied_by_user')
            Start-Sleep -Milliseconds 400
            $h2 = Capture-Hash $win.id
            Check "8-deny-no-injection" ($h2 -ne "" -and $h2 -eq $h1)
        } else {
            Stop-Job $job2 -ErrorAction SilentlyContinue
            Remove-Job $job2 -Force -ErrorAction SilentlyContinue
        }

        $pipe.Close()
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