# semc4_adhoc.ps1 - semantic cursor Task 4 smoke (spec
# 2026-09-22-semantic-cursor): the REAL minesweeper client end to end.
# launch_app minesweeper -> the client registers its own act/snapshot tools
# plus a measured cursor declaration -> the platform synthesizes
# minesweeper.move/read, the act relay parks with the banner name
# "<app>.<kind> at (r,c)", approval re-executes into the app, and the app
# echoes kind/row/col/opened/status. Read assembles the cursor header around
# the app snapshot (9 lines x 9 chars). ASCII-only (PS5.1). Helper functions
# are copied from semc2_adhoc.ps1 (raw pipe idiom, PS5.1 argv trap workaround);
# the launch/poll flow follows probe_conquest_minesweeper.ps1.
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root = Split-Path $exe
$script:fail = 0
function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host ("PASS: " + $name) }
    else { $script:fail++; Write-Host ("FAIL: " + $name + " -- " + $detail) }
    if ($detail) { Write-Host ("      " + $detail) }
}
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
function New-Pipe([int]$subscriber, [bool]$window) {
    $p = New-Object System.IO.Pipes.NamedPipeClientStream(".", "JKWindowServerPipe",
        [System.IO.Pipes.PipeDirection]::InOut)
    $p.Connect(5000)
    $hello = New-Object byte[] 8
    [BitConverter]::GetBytes([uint32]2).CopyTo($hello, 0)
    [BitConverter]::GetBytes([uint32]$PID).CopyTo($hello, 4)
    SendMsg $p 1 $hello
    if ($window) {
        # Window client (Hello + CreateSurface) - only used by the fake app
        # path; the real minesweeper client registers itself via launch_app.
        $w = 64; $h = 64
        $pl = New-Object byte[] (8 + 128)
        [BitConverter]::GetBytes([int32]$w).CopyTo($pl, 0)
        [BitConverter]::GetBytes([int32]$h).CopyTo($pl, 4)
        $title = [Text.Encoding]::ASCII.GetBytes("fakegrid")
        [Array]::Copy($title, 0, $pl, 8, $title.Length)
        SendMsg $p 3 $pl
        $created = Read-Frame $p 5000
        if ($created -eq $null -or $created.type -ne 4) {
            Write-Host "FAIL: newpipe-surfacecreated"
            $script:fail++
        }
    }
    if ($subscriber -ne 0) {
        $sub = New-Object byte[] 4
        [BitConverter]::GetBytes([uint32]$subscriber).CopyTo($sub, 0)
        SendMsg $p 19 $sub
    }
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
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class PipePeekS4 {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool PeekNamedPipe(IntPtr hPipe, byte[] buf,
        int bufSize, out int read, out int avail, out int left);
}
"@
function Pipe-Avail([System.IO.Pipes.NamedPipeClientStream]$s) {
    $read = 0; $avail = 0; $left = 0
    try {
        $h = $s.SafePipeHandle.DangerousGetHandle()
        if (-not [PipePeekS4]::PeekNamedPipe($h, $null, 0, [ref]$read, [ref]$avail, [ref]$left)) { return -1 }
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
    $head0 = 0
    if ($type -eq 20 -or $type -eq 22) { $hs = 4 }
    elseif ($type -eq 17 -or $type -eq 23) { $hs = 8; $head0 = [BitConverter]::ToUInt32($pl, 0) }
    $text = ""
    if ($len -gt $hs) { $text = [Text.Encoding]::UTF8.GetString($pl, $hs, $len - $hs) }
    return @{ type = $type; len = $len; text = $text; head0 = [uint32]$head0 }
}
# Wait for one agent event frame matching a needle (skips other frames).
function Wait-Event([System.IO.Pipes.NamedPipeClientStream]$agent,
                    [string]$needle, [int]$ms) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $ms) {
        $f = Read-Frame $agent 200
        if ($f -ne $null -and $f.type -eq 20 -and $f.text -match $needle) {
            return $f
        }
    }
    return $null
}
# Send an app_tool query and return the final AgentReply (type 18). For ask
# acts the caller must approve in parallel - this helper keeps draining until
# the reply lands (parked acts resolve after the approve call).
function Read-Reply([System.IO.Pipes.NamedPipeClientStream]$agent, [uint32]$qid,
                    [int]$timeoutMs) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $timeoutMs) {
        $f = Read-Frame $agent 200
        if ($f -ne $null -and $f.type -eq 18) { return $f.text }
    }
    return $null
}
# Park one act and return the approval_request event frame (drains catalog
# noise on the way).
function Park-Act([System.IO.Pipes.NamedPipeClientStream]$agent, [uint32]$qid,
                  [string]$kind, [int]$row, [int]$col) {
    SendQuery $agent $qid ('{"tool":"app_tool","args":{"app":"minesweeper","tool":"act","args":{"kind":"' + $kind + '","row":' + $row + ',"col":' + $col + '}}}')
    return (Wait-Event $agent '"topic":"agent\.approval_request"' 10000)
}
function Get-ReqId([object]$ev) {
    if ($ev -ne $null -and $ev.text -match '"request":(\d+)') { return [int]$Matches[1] }
    return 0
}

# --- server lifecycle ---------------------------------------------------------
Get-Process jkdesktop, jkwinserver, jkbridge, jkagentd -ErrorAction SilentlyContinue |
    Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
$up = $false
foreach ($i in 1..30) {
    Start-Sleep -Milliseconds 500
    if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up = $true; break }
}
Check "t4-up" $up "ping"
Write-Output "NOTICE: the user's live desktop server was stopped for the probe run - restart jkwinserver.exe (and jkbridge.exe) afterwards"

# --- launch the real minesweeper client ---------------------------------------
$agent = New-Pipe 1 $false
Invoke-Agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}' | Out-Null
$win = $null
$winJson = ""
for ($i = 0; $i -lt 20; $i++) {
    Start-Sleep -Milliseconds 500
    $lw = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    $winJson = $lw
    if ($lw -match '"title":"Minesweeper"') { $win = $true; break }
}
Check "t4-launch-window" ($win -ne $null) $winJson

# --- tools/list catalog: synthesized move/read + the app's own act/snapshot ---
$cat = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
Check "t4-catalog-move"   ($cat -match '"app":"minesweeper","name":"move"') $cat
Check "t4-catalog-read"   ($cat -match '"app":"minesweeper","name":"read"') $cat
Check "t4-catalog-act"    ($cat -match '"app":"minesweeper","name":"act"') $cat
Check "t4-catalog-snapshot" ($cat -match '"app":"minesweeper","name":"snapshot"') $cat
# The declared kinds drive the enum the LLM sees (server merge).
$enumOk = ($cat -match '"kind":\{"type":"string","enum":\["reveal","flag","question","clear","reset"\]\}')
Check "t4-catalog-act-enum" $enumOk $cat
$script:qid = [uint32]300

# --- platform move: absolute, echo convention ---------------------------------
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"minesweeper","tool":"move","args":{"to_row":4,"to_col":4}}}'
$mv = Read-Reply $agent $script:qid 8000
Check "t4-move-echo" ($mv -ne $null -and $mv -match '"ok":true' -and $mv -match '"row":4' -and $mv -match '"col":4') $mv

# --- read: cursor header + 9x9 board serialization -----------------------------
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"minesweeper","tool":"read","args":{}}}'
$rd = Read-Reply $agent $script:qid 12000
Check "t4-read-ok" ($rd -ne $null -and $rd -match '"ok":true') $rd
Check "t4-read-cursor-header" ($rd -ne $null -and $rd -match '"cursor":\{"row":4,"col":4\}' -and $rd -match '"rows":9,"cols":9') $rd
$linesOk = $false
$snapText = ""
if ($rd -ne $null -and $rd -match '"snapshot":\{(.*)\}') {
    $snapText = $Matches[1]
    # Parse the snapshot properly (PS5.1: ConvertFrom-Json on the full reply's
    # snapshot object) and verify 9 lines x 9 chars of legal glyphs.
    $parsed = $null
    try {
        $replyObj = $rd | ConvertFrom-Json
        $snap = $replyObj.snapshot
        if ($snap.status -eq "playing" -and $snap.lines.Count -eq 9) {
            $linesOk = $true
            foreach ($l in $snap.lines) {
                if ($l.Length -ne 9 -or -not ($l -match '^[#F?0-9*]{9}$')) { $linesOk = $false }
            }
            $script:snap0 = $snap
        }
    } catch { $linesOk = $false; $snapText = "parse-error: " + $_.Exception.Message }
}
Check "t4-read-board-9x9" $linesOk ("reply=" + $rd)
$closed = 0
if ($linesOk) { $closed = ($script:snap0.board.ToCharArray() | Where-Object { $_ -eq '#' }).Count }
Check "t4-read-fresh-board" ($closed -eq 81) ("closed=" + $closed)

# --- act reveal parks with the cell banner, approval re-executes ---------------
function Approve-Act([string]$kind, [int]$row, [int]$col) {
    $script:qid++
    $ev = Park-Act $agent $script:qid $kind $row $col
    $okPark = ($ev -ne $null -and $ev.text -match ('"name":"minesweeper\.' + $kind + ' at \(' + $row + ',' + $col + '\)"'))
    Check ("t4-park-" + $kind + "-" + $row + "-" + $col) $okPark ($(if ($ev) { $ev.text } else { "no approval_request event" }))
    $reqId = Get-ReqId $ev
    if ($reqId -le 0) { return $null }
    $ap = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
    Check ("t4-approve-" + $kind + "-" + $row + "-" + $col) ($ap -match '"approved":true') $ap
    return (Read-Reply $agent $script:qid 12000)
}
$act = Approve-Act "reveal" 4 4
Check "t4-reveal-opened-echo" ($act -ne $null -and $act -match '"opened":([1-9][0-9]*)' -and $act -match '"kind":"reveal"' -and $act -match '"status":"playing"') $act

# board changed: fresh '#' count must have dropped
$rd2 = $null
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"minesweeper","tool":"read","args":{}}}'
$rd2 = Read-Reply $agent $script:qid 12000
$closed2 = -1
if ($rd2 -ne $null) {
    try {
        $snap2 = ($rd2 | ConvertFrom-Json).snapshot
        $closed2 = ($snap2.board.ToCharArray() | Where-Object { $_ -eq '#' }).Count
    } catch { $closed2 = -2 }
}
Check "t4-read-after-reveal" ($closed2 -ge 0 -and $closed2 -lt $closed) ("closed2=" + $closed2)

# --- invalid transitions: bad_state echoes -------------------------------------
# Flagging an already-opened cell = bad_state (the reveal opened (4,4)).
$act2 = Approve-Act "flag" 4 4
Check "t4-bad-state-echo" ($act2 -ne $null -and $act2 -match '"error":"bad_state"' -and
                           $act2 -match '"kind":"flag"' -and $act2 -match '"row":4' -and
                           $act2 -match '"status":"playing"') $act2

# --- flag marks a still-closed cell (read shows F) ------------------------------
# Pick the first closed cell from the post-reveal board - the flood fill may
# have opened most of the grid, so the target must be read out, not assumed.
$fr = -1; $fc = -1
if ($rd2 -ne $null) {
    try {
        $snap2b = ($rd2 | ConvertFrom-Json).snapshot
        for ($r = 0; $r -lt 9; $r++) {
            $idx = $snap2b.lines[$r].IndexOf('#')
            if ($idx -ge 0) { $fr = $r; $fc = $idx; break }
        }
    } catch {}
}
Check "t4-flag-target-found" ($fr -ge 0) ("rd2=" + $rd2)
$act3 = Approve-Act "flag" $fr $fc
Check "t4-flag-echo" ($act3 -ne $null -and $act3 -match '"opened":1' -and $act3 -match '"status":"playing"') $act3
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"minesweeper","tool":"read","args":{}}}'
$rd3 = Read-Reply $agent $script:qid 12000
$flagOk = $false
if ($rd3 -ne $null) {
    try {
        $snap3 = ($rd3 | ConvertFrom-Json).snapshot
        $flagOk = ($snap3.status -eq "playing" -and $snap3.flags -eq 1 -and
                   $fr -ge 0 -and $snap3.lines[$fr][$fc] -eq 'F')
    } catch { $flagOk = $false }
}
Check "t4-read-flag-cell" $flagOk $rd3

# --- reset transition: fresh board, status playing ------------------------------
$act4 = Approve-Act "reset" 0 0
Check "t4-reset-echo" ($act4 -ne $null -and $act4 -match '"kind":"reset"' -and $act4 -match '"status":"playing"') $act4
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"minesweeper","tool":"read","args":{}}}'
$rd4 = Read-Reply $agent $script:qid 12000
$resetOk = $false
if ($rd4 -ne $null) {
    try {
        $snap4 = ($rd4 | ConvertFrom-Json).snapshot
        $allClosed = ($snap4.lines.Count -eq 9)
        foreach ($l in $snap4.lines) { if (-not ($l -match '^[#]{9}$')) { $allClosed = $false } }
        $resetOk = ($snap4.status -eq "playing" -and $allClosed)
    } catch { $resetOk = $false }
}
Check "t4-read-after-reset" $resetOk $rd4

# --- teardown -------------------------------------------------------------------
Get-Process jkdesktop, jkwinserver, jkbridge -ErrorAction SilentlyContinue |
    Stop-Process -Force
Write-Output "NOTICE: the desktop server is left stopped - restart jkwinserver.exe (and jkbridge.exe) to resume the live desktop"
if ($script:fail -gt 0) { Write-Output ("FAILED: " + $script:fail); exit 1 }
Write-Output "SEMC4 PASS"
exit 0