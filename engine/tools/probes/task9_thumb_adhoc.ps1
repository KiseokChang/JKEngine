# task9_thumb_adhoc.ps1 - ad-hoc verification for task 9 (spec app-tool-hub SS5 2-3 dan).
# ASCII-only (PS5.1). NOT part of the regression suite - Task 10 owns probe_app_tools.ps1.
# Checks:
#  A event: approval_request (kind=app_tool) carries target{app,tool,windowId,title} + thumb
#  B thumb: PNG exists at the event path, non-trivial, PNG magic
#  C flow:  cross approve -> parked reply ok + approval_resolved + thumb file remains
#  D fail:  control-only manifest (windowId=0) -> no capture -> event without thumb,
#           approval still completes (fake app answers the relay)
# Harness idioms = probe_app_tools.ps1 (raw pipe frame reader, agentctl raw cmdline).
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root = Split-Path $exe
$permFile = Join-Path $root "permissions.json"
$script:fail = 0

function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host ("PASS: " + $name) }
    else { $script:fail++; Write-Host ("FAIL: " + $name + " -- " + $detail) }
    if ($detail) { Write-Host ("      " + $detail) }
}

# --- permissions.json: byte-exact backup/restore (user file, never burn) -----
$stale = Get-ChildItem (Join-Path $env:TEMP "perm_pre_task9_*.json") -ErrorAction SilentlyContinue
if ($stale) {
    Write-Host ("FAIL: setup-stale-residue -- " + (($stale | ForEach-Object { $_.Name }) -join ", "))
    Write-Host "      restore engine/build/permissions.json manually, delete the leftover(s) in TEMP, re-run"
    exit 1
}
$permBakFile = Join-Path $env:TEMP ("perm_pre_task9_" + $PID + ".json")
$hadPerm = Test-Path $permFile
if ($hadPerm) { Copy-Item $permFile $permBakFile -Force }
function Set-Perms([string]$json) {
    [IO.File]::WriteAllText($permFile, $json, (New-Object System.Text.UTF8Encoding($false)))
}
# user's 7-key file (documented state) + the ask keys under test
$baseKeys = '{"close_window":"allow","trust_request":"allow","run_console_app":"allow","trust_revoke":"allow","files_list":"allow","files_read":"allow","permission_set":"allow"'

function Invoke-Agentctl([string]$json) {
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

function SendMsg([System.IO.Pipes.NamedPipeClientStream]$s, [int]$type, [byte[]]$payload) {
    $hdr = New-Object byte[] 12
    [BitConverter]::GetBytes([uint32]0x4A4B0001).CopyTo($hdr, 0)
    [BitConverter]::GetBytes([uint32]$type).CopyTo($hdr, 4)
    [BitConverter]::GetBytes([uint32]$payload.Length).CopyTo($hdr, 8)
    $s.Write($hdr, 0, 12)
    if ($payload.Length -gt 0) { $s.Write($payload, 0, $payload.Length) }
    $s.Flush()
}
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
function SendQuery([System.IO.Pipes.NamedPipeClientStream]$s, [uint32]$qid, [string]$json) {
    $body = [Text.Encoding]::UTF8.GetBytes($json)
    $payload = New-Object byte[] (8 + $body.Length)
    [BitConverter]::GetBytes([uint32]$qid).CopyTo($payload, 0)
    [BitConverter]::GetBytes([uint32]$body.Length).CopyTo($payload, 4)
    [Array]::Copy($body, 0, $payload, 8, $body.Length)
    SendMsg $s 17 $payload
}
function Send-ToolRegister([System.IO.Pipes.NamedPipeClientStream]$s, [string]$json) {
    $body = [Text.Encoding]::UTF8.GetBytes($json)
    $payload = New-Object byte[] (4 + $body.Length)
    [BitConverter]::GetBytes([uint32]$body.Length).CopyTo($payload, 0)
    [Array]::Copy($body, 0, $payload, 4, $body.Length)
    SendMsg $s 22 $payload
}
function Send-ToolResult([System.IO.Pipes.NamedPipeClientStream]$s, [uint32]$reqId, [string]$json) {
    $body = [Text.Encoding]::UTF8.GetBytes($json)
    $payload = New-Object byte[] (12 + $body.Length)
    [BitConverter]::GetBytes([uint32]$reqId).CopyTo($payload, 0)
    [BitConverter]::GetBytes([uint32]1).CopyTo($payload, 4)
    [BitConverter]::GetBytes([uint32]$body.Length).CopyTo($payload, 8)
    [Array]::Copy($body, 0, $payload, 12, $body.Length)
    SendMsg $s 24 $payload
}
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class PipePeek9 {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool PeekNamedPipe(IntPtr hPipe, byte[] buf,
        int bufSize, out int read, out int avail, out int left);
}
"@
function Pipe-Avail([System.IO.Pipes.NamedPipeClientStream]$s) {
    $read = 0; $avail = 0; $left = 0
    try {
        $h = $s.SafePipeHandle.DangerousGetHandle()
        if (-not [PipePeek9]::PeekNamedPipe($h, $null, 0, [ref]$read, [ref]$avail, [ref]$left)) { return -1 }
    } catch { return -1 }
    return $avail
}
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
    return @{ type = $type; len = $len; pl = $pl; text = $text }
}

# --- lifecycle ---------------------------------------------------------------
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process jkapp_vplayer -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Set-Perms ($baseKeys + ',"app_tool.vplayer.get_status":"ask"}')
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
$up = $false
foreach ($i in 1..30) {
    Start-Sleep -Milliseconds 500
    if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up = $true; break }
}
Check "setup-server-up" $up ""

try {
    # ---- vplayer spawn + registration ----------------------------------------
    Invoke-Agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
    $cat = ""
    foreach ($i in 1..30) {
        Start-Sleep -Milliseconds 500
        $cat = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
        if (([regex]::Matches($cat, '"app":"vplayer"').Count) -eq 6) { break }
    }
    $wid = 0
    $m = [regex]::Match($cat, '"name":"get_status","description":"[^"]*","inputSchema":[\s\S]*?"windowId":(\d+)')
    if ($m.Success) { $wid = [int]$m.Groups[1].Value }
    Check "setup-vplayer-6-rows" (([regex]::Matches($cat, '"app":"vplayer"').Count) -eq 6 -and $wid -gt 0) ("wid=$wid")

    # ---- A/B: ask get_status -> approval_request with target + thumb ----------
    $connA = New-Pipe 1   # subscriber (parks the ask query)
    SendQuery $connA 901 '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}'
    $evt = ""
    $reqId = 0
    foreach ($i in 1..50) {
        $f = Read-Frame $connA 400
        if ($f -and $f.type -eq 20 -and $f.text -match '"kind":"app_tool"' -and
            $f.text -match '"name":"vplayer\.get_status"') {
            $evt = $f.text
            $mm = [regex]::Match($f.text, '"request":(\d+)')
            if ($mm.Success) { $reqId = [int]$mm.Groups[1].Value }
            break
        }
    }
    Check "A-approval-request-arrived" ($reqId -gt 0) $evt
    $tApp = ""; $tTool = ""; $tWid = 0; $tTitle = ""
    $m = [regex]::Match($evt, '"target":\{[^}]*\}')
    if ($m.Success) {
        $tObj = $m.Groups[0].Value
        $m2 = [regex]::Match($tObj, '"app":"([^"]*)"'); if ($m2.Success) { $tApp = $m2.Groups[1].Value }
        $m2 = [regex]::Match($tObj, '"tool":"([^"]*)"'); if ($m2.Success) { $tTool = $m2.Groups[1].Value }
        $m2 = [regex]::Match($tObj, '"windowId":(\d+)'); if ($m2.Success) { $tWid = [int]$m2.Groups[1].Value }
        $m2 = [regex]::Match($tObj, '"title":"([^"]*)"'); if ($m2.Success) { $tTitle = $m2.Groups[1].Value }
    }
    Check "A-target-fields" ($tApp -eq "vplayer" -and $tTool -eq "get_status" -and $tWid -eq $wid -and $tTitle -ne "") `
        ("target=$($m.Groups[0].Value) wantWid=$wid")
    Check "A-target-id-kept" ($evt -match '"target_id":' + $wid) "target_id retained for Task 8 consumers"
    $thumb = ""
    $m = [regex]::Match($evt, '"thumb":"([^"]*)"')
    if ($m.Success) { $thumb = $m.Groups[1].Value -replace '\\\\', '\' }
    Check "A-thumb-present" ($thumb -ne "" -and $thumb -match 'approval_\d+_' + $reqId + '\.png$') "thumb=$thumb"
    # ---- B: PNG exists, non-trivial, real PNG magic ---------------------------
    $pngOk = $false; $pngSize = 0; $magic = ""
    if ($thumb -ne "" -and (Test-Path $thumb)) {
        $fi = Get-Item $thumb
        $pngSize = $fi.Length
        $b = [IO.File]::ReadAllBytes($thumb)[0..7]
        $magic = (($b | ForEach-Object { $_.ToString("x2") }) -join "")
    }
    $pngOk = ($pngSize -gt 512 -and $magic -eq "89504e470d0a1a0a")
    Check "B-png-nontrivial" $pngOk ("size=$pngSize magic=$magic")

    # ---- C: cross approve -> flow completes, thumb file remains ---------------
    $ap = (Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}'))
    Check "C-approve-ack" ($ap -match '"approved":true') $ap
    $parked = ""
    $resolved = ""
    foreach ($i in 1..50) {
        $f = Read-Frame $connA 400
        if (-not $f) { continue }
        if ($f.type -eq 18 -and $f.text -match '"result"') { $parked = $f.text }
        if ($f.type -eq 20 -and $f.text -match '"topic":"agent\.approval_resolved"' -and
            $f.text -match '"request":' + $reqId) { $resolved = $f.text }
        if ($parked -and $resolved) { break }
    }
    Check "C-parked-result" ($parked -match '"ok":true' -and $parked -match '"result":\{') $parked
    Check "C-approval-resolved-event" ($resolved -match '"decision":"allow"') $resolved
    Check "C-thumb-remains" ($thumb -ne "" -and (Test-Path $thumb)) "thumb=$thumb"
    $connA.Dispose()

    # ---- D: capture-failure path (control-only manifest, windowId=0) ----------
    Set-Perms ($baseKeys + ',"app_tool.thumbfail.notool":"ask"}')
    $connB = New-Pipe 0   # control-only, registers fake app (windowId=0)
    Send-ToolRegister $connB '{"app":"thumbfail","tools":[{"name":"notool","description":"no layer behind me"}]}'
    $ack = ""
    foreach ($i in 1..20) {
        $f = Read-Frame $connB 400
        if ($f -and $f.type -eq 18 -and $f.text -match '"ok"') { $ack = $f.text; break }
    }
    Check "D-register-ack" ($ack -match '"ok":true') $ack
    $connC = New-Pipe 1
    SendQuery $connC 902 '{"tool":"app_tool","args":{"app":"thumbfail","tool":"notool","args":{}}}'
    $evt2 = ""
    $reqId2 = 0
    foreach ($i in 1..50) {
        $f = Read-Frame $connC 400
        if ($f -and $f.type -eq 20 -and $f.text -match '"kind":"app_tool"' -and
            $f.text -match '"name":"thumbfail\.notool"') {
            $evt2 = $f.text
            $mm = [regex]::Match($f.text, '"request":(\d+)')
            if ($mm.Success) { $reqId2 = [int]$mm.Groups[1].Value }
            break
        }
    }
    Check "D-ask-parked-no-thumb" ($reqId2 -gt 0 -and $evt2 -notmatch '"thumb"') $evt2
    Check "D-target-still-present" ($evt2 -match '"target":\{') "target object present even on capture failure"
    $ap2 = (Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId2 + ',"decision":"allow"}}'))
    Check "D-approve-ack" ($ap2 -match '"approved":true') $ap2
    # fake app answers the relay -> parked reply ok (flow completes without thumb)
    $call = $null
    foreach ($i in 1..50) {
        $f = Read-Frame $connB 400
        if ($f -and $f.type -eq 23) { $call = $f; break }
    }
    Check "D-relay-reached-fake-app" ($call -ne $null -and $call.text -match '"tool":"notool"') $call.text
    if ($call) {
        $rid = [BitConverter]::ToUInt32($call.pl, 0)
        Send-ToolResult $connB $rid '{"state":"answered"}'
    }
    $parked2 = ""
    foreach ($i in 1..50) {
        $f = Read-Frame $connC 400
        if ($f -and $f.type -eq 18 -and $f.text -match '"result"') { $parked2 = $f.text; break }
    }
    Check "D-parked-result-ok" ($parked2 -match '"ok":true' -and $parked2 -match '"answered"') $parked2
    $connC.Dispose()
    $connB.Dispose()
}
finally {
    # restore the user file BYTE-EXACT
    Get-Process jkapp_vplayer -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
    if ($hadPerm) {
        Copy-Item $permBakFile $permFile -Force
        Remove-Item $permBakFile -Force
    } else {
        Remove-Item $permFile -ErrorAction SilentlyContinue
    }
    Write-Host ("perms-restored-bytes: " +
        ((Get-Item $permFile -ErrorAction SilentlyContinue).Length) + " bytes")
}
Write-Host ("RESULT: " + $(if ($script:fail -eq 0) { "ALL PASS" } else { "$($script:fail) FAIL" }))
exit $(if ($script:fail -eq 0) { 0 } else { 1 })