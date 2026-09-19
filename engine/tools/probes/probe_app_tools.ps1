# probe_app_tools.ps1 - app tool hub stage 1 regression (spec 2026-09-19-app-tool-hub).
# ASCII-only (PS5.1). Checks (9 + setup):
#   1  catalog: vplayer spawn -> list_app_tools 6 rows (name/description/windowId>0)
#   2  seek e2e: open clip -> poll get_status opened -> seek 1 -> pos < 2.0
#   3  error surface: unknown app -> unknown_app_tool; seek w/o seconds -> app bad_args
#   4  multi-instance: 2nd vplayer -> 12 rows; no windowId -> ambiguous+candidates(2);
#      windowId of instance 1 -> success; close 2nd
#   5  auto-cleanup: subscriber first, close_window -> catalog cleared +
#      agent.app_tools_changed event
#   6  registration validation: bad_name / namespace_conflict / too_many_tools
#   7  gate deny: permissions.json {"app_tool.vplayer.seek":"deny"} -> denied
#   8  gate ask + parking: approval_request event (kind=app_tool) -> cross approve
#      (agentctl, different conn) -> parked result + paused actually toggled
#   9  tool_timeout: fake app never answers AgentToolCall -> tool_timeout <= 15s,
#      then close -> manifest cleanup
#   10 bridge regression: probe_jkbridge.ps1 re-run (generic relay unchanged)
# Raw pipe helpers = mgr_t1_read/mgr_t8_cross_approve frame reader (one function
# consumes header+payload whole). agentctl args = probe_notes ProcessStartInfo
# raw command-line idiom (PS5.1 argv re-parsing trap). Tool args use INTEGERs
# (AgentJson GetInt coercion contract, task-5 note).
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root = Split-Path $exe
$clip = "I:/progwork/JKENGINE/tmp/vpt2_test.mp4"
$permFile = Join-Path $root "permissions.json"
$script:fail = 0

function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host ("PASS: " + $name) }
    else { $script:fail++; Write-Host ("FAIL: " + $name + " -- " + $detail) }
    if ($detail) { Write-Host ("      " + $detail) }
}

function Invoke-Agentctl([string]$json) {
    # PS5.1 native quoting trap (docs/55 lesson 3): values with quotes/spaces
    # get mangled when PS builds the child command line. Write the raw command
    # line ourselves: agentctl "{escaped-json}" - CRT turns \" into a literal ".
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = 'agentctl "' + ($json -replace '"', '\"') + '"'
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.CreateNoWindow = $true
    $p = [System.Diagnostics.Process]::Start($psi)
    $out = $p.StandardOutput.ReadToEnd()
    $p.WaitForExit()
    return ($out -replace '(?m)^\[theme\][^\r\n]*[\r\n]*', '')
}
# windowId sits at the app_tool args level (alongside app/tool) - the app's
# own payload is the nested "args" passthrough (server reads windowId via
# GetObjInt("args","windowId"); nested passthrough args are opaque).
function AppTool([string]$app, [string]$tool, [string]$argsJson, [int]$windowId = 0) {
    $a = '{"app":"' + $app + '","tool":"' + $tool + '"'
    if ($windowId -gt 0) { $a += ',"windowId":' + $windowId }
    if ($argsJson) { $a += ',"args":' + $argsJson }
    $a += '}'
    return (Invoke-Agentctl ('{"tool":"app_tool","args":' + $a + '}'))
}

# --- permissions.json is probe-owned state: back up, remove for a clean
# baseline (absent file = allow for app_tool, deny for close_window), restore
# in finally (probe_files/probe_notes pattern - never burn user state).
# Backup name is per-process: a KILLED earlier run (timeout) must not have its
# own leftover adopted as "user state" by a later run's backup overwrite.
$permBakFile = Join-Path $env:TEMP ("perm_pre_apptools_" + $PID + ".json")
$hadPerm = Test-Path $permFile
if ($hadPerm) { Copy-Item $permFile $permBakFile -Force }
function Set-Perms([string]$json) {
    [IO.File]::WriteAllText($permFile, $json, (New-Object System.Text.UTF8Encoding($false)))
}
function Clear-Perms { Remove-Item $permFile -ErrorAction SilentlyContinue }

# --- raw named pipe client (mgr_t8_cross_approve idiom + deadline reads) -----
function SendMsg([System.IO.Pipes.NamedPipeClientStream]$s, [int]$type, [byte[]]$payload) {
    $hdr = New-Object byte[] 12
    [BitConverter]::GetBytes([uint32]0x4A4B0001).CopyTo($hdr, 0)
    [BitConverter]::GetBytes([uint32]$type).CopyTo($hdr, 4)
    [BitConverter]::GetBytes([uint32]$payload.Length).CopyTo($hdr, 8)
    $s.Write($hdr, 0, 12)
    if ($payload.Length -gt 0) { $s.Write($payload, 0, $payload.Length) }
    $s.Flush()
}
# The acceptor reads Hello + a SECOND message: CreateSurface (window client)
# or AgentEventSubscribe (control-only; 1 = subscriber, 0 = plain agent decl,
# jkagentd/agentctl convention). A raw client whose second message is anything
# else is discarded server-side ("expected CreateSurface", pipe closed) - so
# fake-app connections must declare BEFORE registering.
function New-Pipe([int]$subscriber) {
    $p = New-Object System.IO.Pipes.NamedPipeClientStream(".", "JKWindowServerPipe",
        [System.IO.Pipes.PipeDirection]::InOut)
    $p.Connect(5000)
    $hello = New-Object byte[] 8
    [BitConverter]::GetBytes([uint32]2).CopyTo($hello, 0)
    [BitConverter]::GetBytes([uint32]$PID).CopyTo($hello, 4)
    SendMsg $p 1 $hello
    $sub = New-Object byte[] 4
    [BitConverter]::GetBytes([uint32]$subscriber).CopyTo($sub, 0)
    SendMsg $p 19 $sub
    return $p
}
function Subscribe-Pipe([System.IO.Pipes.NamedPipeClientStream]$s, [uint32]$flag) {
    $sub = New-Object byte[] 4
    [BitConverter]::GetBytes($flag).CopyTo($sub, 0)
    SendMsg $s 19 $sub
}
function SendQuery([System.IO.Pipes.NamedPipeClientStream]$s, [uint32]$qid, [string]$json) {
    $body = [Text.Encoding]::UTF8.GetBytes($json)
    $payload = New-Object byte[] (8 + $body.Length)
    [BitConverter]::GetBytes([uint32]$qid).CopyTo($payload, 0)
    [BitConverter]::GetBytes([uint32]$body.Length).CopyTo($payload, 4)
    [Array]::Copy($body, 0, $payload, 8, $body.Length)
    SendMsg $s 17 $payload
}
# AgentToolRegister (type 22): header {jsonLen} + JSON.
function Send-ToolRegister([System.IO.Pipes.NamedPipeClientStream]$s, [string]$json) {
    $body = [Text.Encoding]::UTF8.GetBytes($json)
    $payload = New-Object byte[] (4 + $body.Length)
    [BitConverter]::GetBytes([uint32]$body.Length).CopyTo($payload, 0)
    [Array]::Copy($body, 0, $payload, 4, $body.Length)
    SendMsg $s 22 $payload
}
# Bytes buffered in the pipe (PeekNamedPipe - PS5.1 NamedPipeClientStream has
# no IsDataAvailable). -1 = pipe closed/broken.
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class PipePeek {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool PeekNamedPipe(IntPtr hPipe, byte[] buf,
        int bufSize, out int read, out int avail, out int left);
}
"@
function Pipe-Avail([System.IO.Pipes.NamedPipeClientStream]$s) {
    $read = 0; $avail = 0; $left = 0
    try {
        $h = $s.SafePipeHandle.DangerousGetHandle()
        if (-not [PipePeek]::PeekNamedPipe($h, $null, 0, [ref]$read, [ref]$avail, [ref]$left)) { return -1 }
    } catch { return -1 }
    return $avail
}
# Deadline-bounded frame read: poll availability, then a SYNCHRONOUS Read only
# when the needed bytes are already buffered (Read returns without blocking).
# ReadAsync+Wait is unusable here - an abandoned pending read makes the NEXT
# ReadAsync block indefinitely (measured: 2nd frame read hung past its budget).
# One function consumes the entire frame (12-byte wire header + payload) and
# slices the type-specific message header off the payload: AgentQuery 8
# {queryId,jsonLen}, AgentReply/AgentToolResult 12 {queryId,ok,jsonLen},
# AgentEvent/AgentToolRegister 4 {jsonLen}, AgentToolCall 8 {reqId,jsonLen}.
# (mgr_t8's flat 12-byte skip happened to work for its event regexes because
# they matched mid-JSON - events actually carry a 4-byte header.)
function Read-Frame([System.IO.Pipes.NamedPipeClientStream]$s, [int]$timeoutMs) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ((Pipe-Avail $s) -lt 12) {
        if ($sw.ElapsedMilliseconds -gt $timeoutMs) { return $null }
        Start-Sleep -Milliseconds 10
    }
    $hdr = New-Object byte[] 12
    if ($s.Read($hdr, 0, 12) -ne 12) { return $null }
    $type = [int][BitConverter]::ToUInt32($hdr, 4)
    $len = [int][BitConverter]::ToUInt32($hdr, 8)
    if ($len -lt 0 -or $len -gt (4 * 1024 * 1024)) { return $null }
    while ((Pipe-Avail $s) -lt $len) {
        if ($sw.ElapsedMilliseconds -gt $timeoutMs) { return $null }
        Start-Sleep -Milliseconds 10
    }
    $pl = New-Object byte[] $len
    $off = 0
    while ($off -lt $len) {
        $a = Pipe-Avail $s
        if ($a -le 0) { return $null }
        $n = [Math]::Min($a, $len - $off)
        $r = $s.Read($pl, $off, $n)
        if ($r -le 0) { return $null }
        $off += $r
    }
    $hs = 12
    if ($type -eq 20 -or $type -eq 22) { $hs = 4 }
    elseif ($type -eq 17 -or $type -eq 23) { $hs = 8 }
    $text = ""
    if ($len -gt $hs) { $text = [Text.Encoding]::UTF8.GetString($pl, $hs, $len - $hs) }
    return @{ type = $type; len = $len; text = $text }
}

# --- server lifecycle --------------------------------------------------------
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process jkapp_vplayer -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
$up = $false
foreach ($i in 1..30) {
    Start-Sleep -Milliseconds 500
    if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up = $true; break }
}
Check "setup-server-up" $up ""

# Launch a vplayer and poll until its 6 tools are registered (registration
# happens in OnInit, post-connect). Returns the list_app_tools reply.
function Spawn-VPlayer {
    Invoke-Agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
    foreach ($i in 1..30) {
        Start-Sleep -Milliseconds 500
        $l = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
        if ($l -match '"app":"vplayer"') { return $l }
    }
    return $l
}
# Poll get_status until opened (<=10s). Returns the get_status reply.
function Wait-Opened([string]$app) {
    foreach ($i in 1..25) {
        $st = (AppTool $app "get_status" "")
        if ($st -match '"opened":true') { return $st }
        Start-Sleep -Milliseconds 400
    }
    return $st
}

try {
    # ---- check 1: catalog ----------------------------------------------------
    $cat = Spawn-VPlayer
    $rows = [regex]::Matches($cat, '"app":"vplayer"').Count
    Check "c1-catalog-6-rows" ($cat -match '"ok":true' -and $rows -eq 6) ("rows=$rows")
    $namesOk = $true
    foreach ($n in @("open", "play_pause", "seek", "set_volume", "set_av_delay", "get_status")) {
        if ($cat -notmatch ('"app":"vplayer","name":"' + $n + '"')) { $namesOk = $false }
    }
    Check "c1-catalog-names" $namesOk "6 expected tool names present"
    $descOk = ($cat -match '"name":"seek","description":"[^"]+"') -and
              ($cat -match '"name":"get_status","description":"[^"]+"')
    Check "c1-catalog-descriptions" $descOk ""
    $wid = 0
    $m = [regex]::Match($cat, '"name":"seek","description":"[^"]*","inputSchema":[\s\S]*?"windowId":(\d+)')
    if ($m.Success) { $wid = [int]$m.Groups[1].Value }
    Check "c1-catalog-windowId>0" ($wid -gt 0) ("seek windowId=$wid")

    # ---- check 2: seek e2e ----------------------------------------------------
    $st = (AppTool "vplayer" "get_status" "")
    if ($st -notmatch '"opened":true') {
        $op = (AppTool "vplayer" "open" ('{"path":"' + $clip + '"}'))
        Check "c2-open-accepted" ($op -match '"accepted":true') $op
        $st = Wait-Opened "vplayer"
    }
    Check "c2-opened" ($st -match '"opened":true') $st
    $sk = (AppTool "vplayer" "seek" '{"seconds":1}')
    Check "c2-seek-ack" ($sk -match '"ok":true') $sk
    Start-Sleep -Milliseconds 300
    $st2 = (AppTool "vplayer" "get_status" "")
    $pos = -1.0
    $m = [regex]::Match($st2, '"pos":([0-9.]+)')
    if ($m.Success) { $pos = [double]$m.Groups[1].Value }
    Check "c2-pos<2.0" ($pos -ge 0 -and $pos -lt 2.0) ("pos=$pos")

    # ---- check 3: error surface ------------------------------------------------
    $e1 = (AppTool "nope" "x" "")
    Check "c3-unknown-app-tool" ($e1 -match '"error":"unknown_app_tool"') $e1
    $e2 = (AppTool "vplayer" "seek" "")
    Check "c3-bad-args-passthrough" ($e2 -match '"error":\{"error":"bad_args"') $e2

    # ---- check 4: multi-instance + ambiguity -----------------------------------
    Set-Perms '{"close_window":"allow"}'   # close_window defaults deny w/o file
    Invoke-Agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
    $cat2 = ""
    foreach ($i in 1..30) {
        Start-Sleep -Milliseconds 500
        $cat2 = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
        if (([regex]::Matches($cat2, '"app":"vplayer"').Count) -eq 12) { break }
    }
    $rows2 = [regex]::Matches($cat2, '"app":"vplayer"').Count
    Check "c4-12-rows" ($rows2 -eq 12) ("rows=$rows2")
    $ids = @()
    foreach ($mm in [regex]::Matches($cat2, '"name":"get_status","description":"[^"]*","inputSchema":[\s\S]*?"windowId":(\d+)')) {
        $ids += [int]$mm.Groups[1].Value
    }
    $amb = (AppTool "vplayer" "get_status" "")
    $cn = 0
    $m = [regex]::Match($amb, '"error":"ambiguous","candidates":\[([^\]]*)\]')
    if ($m.Success) { $cn = [regex]::Matches($m.Groups[1].Value, '"windowId":').Count }
    Check "c4-ambiguous-candidates" ($cn -eq 2 -and $ids.Count -eq 2) ("candidates=$cn ids=$($ids -join ',')")
    # windowId of instance 1 (the manifest registered first) -> direct hit.
    $w1 = $wid
    $d1 = (AppTool "vplayer" "get_status" "" $w1)
    Check "c4-windowId-direct" ($d1 -match '"ok":true' -and $d1 -match '"result":\{') $d1
    # close the second instance -> back to 6 rows
    $w2 = ($ids | Where-Object { $_ -ne $w1 } | Select-Object -First 1)
    $cl2 = (Invoke-Agentctl ('{"tool":"close_window","args":{"id":' + $w2 + '}}'))
    $cleared = $false
    foreach ($i in 1..20) {
        Start-Sleep -Milliseconds 500
        $cat3 = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
        if (([regex]::Matches($cat3, '"app":"vplayer"').Count) -eq 6) { $cleared = $true; break }
    }
    Check "c4-second-closed-back-to-6" ($cleared -and $cl2 -match '"ok":true') ("closed2=$cl2 rows=6:$cleared")

    # ---- check 5: auto-cleanup (subscriber FIRST - lesson 28) ------------------
    $connE = New-Pipe 1   # subscriber
    $cl1 = (Invoke-Agentctl ('{"tool":"close_window","args":{"id":' + $w1 + '}}'))
    $gone = $false
    foreach ($i in 1..20) {
        Start-Sleep -Milliseconds 500
        $cat4 = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
        if (([regex]::Matches($cat4, '"app":"vplayer"').Count) -eq 0) { $gone = $true; break }
    }
    Check "c5-catalog-cleared" ($gone -and $cl1 -match '"ok":true') "close=$cl1 rows0=$gone"
    $evt = ""
    foreach ($i in 1..50) {
        $f = Read-Frame $connE 400
        if ($f -and $f.type -eq 20 -and $f.text -match '"topic":"agent\.app_tools_changed"') { $evt = $f.text; break }
    }
    Check "c5-app-tools-changed-event" ($evt -ne "") $evt
    $connE.Dispose()

    # ---- check 6: registration validation (fake app over raw pipe) -------------
    $connR = New-Pipe 0   # control-only declaration, then register
    Send-ToolRegister $connR '{"app":"fakereg","tools":[{"name":"Bad-Name","description":"x"}]}'
    $ack1 = ""
    foreach ($i in 1..20) {
        $f = Read-Frame $connR 400
        if ($f -and $f.type -eq 18 -and $f.text -match '"error"') { $ack1 = $f.text; break }
    }
    Check "c6-bad-name" ($ack1 -match '"error":"bad_name"') $ack1
    Send-ToolRegister $connR '{"app":"list_windows","tools":[{"name":"ok_tool","description":"x"}]}'
    $ack2 = ""
    foreach ($i in 1..20) {
        $f = Read-Frame $connR 400
        if ($f -and $f.type -eq 18 -and $f.text -match '"error"') { $ack2 = $f.text; break }
    }
    Check "c6-namespace-conflict" ($ack2 -match '"error":"namespace_conflict"') $ack2
    $many = '{"app":"fakereg2","tools":['
    for ($i = 0; $i -lt 33; $i++) {
        if ($i) { $many += "," }
        $many += '{"name":"tool_' + ("{0:d2}" -f $i) + '","description":"x"}'
    }
    $many += "]}";
    Send-ToolRegister $connR $many
    $ack3 = ""
    foreach ($i in 1..20) {
        $f = Read-Frame $connR 400
        if ($f -and $f.type -eq 18 -and $f.text -match '"error"') { $ack3 = $f.text; break }
    }
    Check "c6-too-many-tools" ($ack3 -match '"error":"too_many_tools"') $ack3
    $connR.Dispose()

    # ---- respawn vplayer for the gate checks (7/8) + open the clip -------------
    Clear-Perms
    $cat5 = Spawn-VPlayer
    Check "setup-vplayer-reregistered" (([regex]::Matches($cat5, '"app":"vplayer"').Count) -eq 6) $cat5
    $op2 = (AppTool "vplayer" "open" ('{"path":"' + $clip + '"}'))
    $st3 = Wait-Opened "vplayer"
    Check "setup-clip-opened" ($st3 -match '"opened":true') "$op2 / $st3"

    # ---- check 7: gate deny -----------------------------------------------------
    Set-Perms '{"app_tool.vplayer.seek":"deny"}'
    $d = (AppTool "vplayer" "seek" '{"seconds":1}')
    Check "c7-deny" ($d -match '"error":"denied"') $d
    Clear-Perms

    # ---- check 8: gate ask + parking (cross approve) -----------------------------
    $pre = (AppTool "vplayer" "get_status" "")
    $pausedPre = $null
    $m = [regex]::Match($pre, '"paused":(true|false)')
    if ($m.Success) { $pausedPre = ($m.Groups[1].Value -eq "true") }
    Check "c8-pre-status" ($pausedPre -ne $null) $pre
    Set-Perms '{"app_tool.vplayer.play_pause":"ask"}'
    $connA = New-Pipe 1   # subscriber (parks the ask query)
    SendQuery $connA 901 '{"tool":"app_tool","args":{"app":"vplayer","tool":"play_pause","args":{}}}'
    $reqId = 0
    foreach ($i in 1..50) {
        $f = Read-Frame $connA 400
        if ($f -and $f.type -eq 20 -and $f.text -match '"kind":"app_tool"' -and
            $f.text -match '"name":"vplayer\.play_pause"') {
            $mm = [regex]::Match($f.text, '"request":(\d+)')
            if ($mm.Success) { $reqId = [int]$mm.Groups[1].Value; $evt8 = $f.text }
            break
        }
    }
    Check "c8-approval-request" ($reqId -gt 0) ($evt8 + " req=$reqId")
    $ap = (Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}'))
    Check "c8-approve-ack" ($ap -match '"approved":true') $ap
    $parked = ""
    foreach ($i in 1..50) {
        $f = Read-Frame $connA 400
        if ($f -and $f.type -eq 18 -and $f.text -match '"result"') { $parked = $f.text; break }
    }
    $pausedNew = $null
    $m = [regex]::Match($parked, '"paused":(true|false)')
    if ($m.Success) { $pausedNew = ($m.Groups[1].Value -eq "true") }
    Check "c8-parked-result" ($parked -match '"ok":true' -and $pausedNew -ne $pausedPre) ($parked + " pre=$pausedPre new=$pausedNew")
    $post = (AppTool "vplayer" "get_status" "")
    $pausedPost = $null
    $m = [regex]::Match($post, '"paused":(true|false)')
    if ($m.Success) { $pausedPost = ($m.Groups[1].Value -eq "true") }
    Check "c8-paused-toggled" ($pausedNew -ne $null -and $pausedPost -eq $pausedNew -and $pausedPost -ne $pausedPre) ("post=$post")
    $connA.Dispose()
    Clear-Perms

    # ---- check 9: tool_timeout (fake app registers, never answers) ---------------
    $connB = New-Pipe 0   # control-only declaration, then register
    Send-ToolRegister $connB '{"app":"slowpoke","tools":[{"name":"ping1","description":"never answers"}]}'
    $ackB = ""
    foreach ($i in 1..20) {
        $f = Read-Frame $connB 400
        if ($f -and $f.type -eq 18 -and $f.text -match '"ok"') { $ackB = $f.text; break }
    }
    Check "c9-register-ack" ($ackB -match '"ok":true') $ackB
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $to = (AppTool "slowpoke" "ping1" "")
    $ms = [int]$sw.ElapsedMilliseconds
    Check "c9-tool-timeout" ($to -match '"error":"tool_timeout"' -and $ms -le 15000) ("$to elapsed=${ms}ms")
    $connB.Dispose()   # close the client: manifest cleanup (inflight already consumed)
    $goneB = $false
    foreach ($i in 1..20) {
        Start-Sleep -Milliseconds 400
        $cat6 = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
        if (([regex]::Matches($cat6, '"app":"slowpoke"').Count) -eq 0) { $goneB = $true; break }
    }
    Check "c9-manifest-cleanup" $goneB ""

    # ---- check 10: bridge regression (probe-managed server, restores perms) ------
    # Clean slate first (dev-run lesson: the nested probe's fixed 2s jkbridge
    # startup window is tight while this probe's server + vplayer decode are
    # still live; a standalone-run-shaped slate keeps it deterministic).
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-Process jkapp_vplayer -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-Process jkbridge -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 2
    $bridgeLog = Join-Path $env:TEMP "probe_app_tools_bridge.log"
    $bo = (& powershell -NoProfile -ExecutionPolicy Bypass `
        -File "I:\progwork\JKENGINE\engine\tools\probes\probe_jkbridge.ps1") 2>&1
    $bo | Out-File -FilePath $bridgeLog -Encoding ASCII
    $brOk = (($bo | ForEach-Object { "$_" }) -join "`n") -match "PASS: jkbridge"
    Check "c10-jkbridge-regression" $brOk ("log=$bridgeLog")

} finally {
    Clear-Perms
    if ($hadPerm) { Copy-Item $permBakFile $permFile -Force; Remove-Item $permBakFile -ErrorAction SilentlyContinue }
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-Process jkapp_vplayer -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-Process jkbridge -ErrorAction SilentlyContinue | Stop-Process -Force
}

Write-Host ("RESULT: " + $(if ($script:fail -eq 0) { "ALL PASS" } else { "$($script:fail) FAILURE(S)" }))
exit $(if ($script:fail -eq 0) { 0 } else { 1 })