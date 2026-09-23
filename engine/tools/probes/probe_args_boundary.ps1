# Args cap 256KiB boundary probe (docs/57 s13.3 item 6): directly proves the
# app_tool relay front cap (JKWindowServer.cpp, argsRaw.size() > 256*1024)
# at its exact edge. probe_app_tools c3c only proved indirect passage at
# ~10KiB; the docs/57 backlog asked for a direct over/under test - and the
# command line cannot carry 300KiB (CRT 32KiB argv limit), so this needs the
# raw pipe harness (probe_approval_overflow lineage).
#
# Error ordering is the witness: the cap check runs BEFORE candidate
# matching, so a nonexistent app ("nope") answers
#   under boundary  -> "unknown_app_tool"  (cap passed, lookup ran)
#   over boundary   -> "args_too_large"    (cap rejected before lookup)
# Exact edge: the cap measures the RAW inner args object text. Compact JSON
# {"pad":"aaa..a"} = 10 + n bytes, so n=262134 -> 262144 bytes passes
# (<=, not >) and n=262135 -> 262145 fails. One byte either side.
$ErrorActionPreference = "Continue"
$exe   = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root  = Split-Path $exe

# --- raw pipe harness (probe_approval_overflow.ps1 lineage) -----------------
function SendMsg([System.IO.Pipes.NamedPipeClientStream]$s, [int]$type, [byte[]]$payload) {
    $hdr = New-Object byte[] 12
    [BitConverter]::GetBytes([uint32]0x4A4B0001).CopyTo($hdr, 0)
    [BitConverter]::GetBytes([uint32]$type).CopyTo($hdr, 4)
    [BitConverter]::GetBytes([uint32]$payload.Length).CopyTo($hdr, 8)
    $s.Write($hdr, 0, 12)
    if ($payload.Length -gt 0) { $s.Write($payload, 0, $payload.Length) }
    $s.Flush()
}
function Pipe-Avail([System.IO.Pipes.NamedPipeClientStream]$s) {
    $sig = 'using System;using System.Runtime.InteropServices;public static class PipePeek {' +
           '[DllImport("kernel32.dll", SetLastError=true)]' +
           'public static extern bool PeekNamedPipe(IntPtr h, IntPtr buf, uint sz, IntPtr rd, out uint avail, IntPtr left);}'
    if (-not ("PipePeek" -as [type])) { Add-Type -TypeDefinition $sig }
    $mh = $s.SafePipeHandle
    $avail = [uint32]0
    $ok = [PipePeek]::PeekNamedPipe($mh.DangerousGetHandle(), [IntPtr]::Zero, 0,
                                   [IntPtr]::Zero, [ref]$avail, [IntPtr]::Zero)
    if (-not $ok) { return 0 }
    return [int]$avail
}
function New-Pipe {
    $p = New-Object System.IO.Pipes.NamedPipeClientStream(".", "JKWindowServerPipe",
        [System.IO.Pipes.PipeDirection]::InOut)
    $p.Connect(5000)
    $hello = New-Object byte[] 8
    [BitConverter]::GetBytes([uint32]2).CopyTo($hello, 0)
    [BitConverter]::GetBytes([uint32]$PID).CopyTo($hello, 4)
    SendMsg $p 1 $hello
    $sub = New-Object byte[] 4
    [BitConverter]::GetBytes([uint32]1).CopyTo($sub, 0)
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
    if ($type -eq 20) { $hs = 4 }
    elseif ($type -eq 17) { $hs = 8 }
    $text = ""
    if ($len -gt $hs) { $text = [Text.Encoding]::UTF8.GetString($pl, $hs, $len - $hs) }
    $qid = [BitConverter]::ToUInt32($pl, 0)
    return @{ type = $type; len = $len; text = $text; qid = [int]$qid }
}

# --- check machinery ----------------------------------------------------------
$script:fail = 0
function Check([string]$name, [bool]$cond, [string]$detail) {
    if ($cond) { Write-Output "PASS $name" }
    else { Write-Output "FAIL $name  ($detail)"; $script:fail++ }
}

# Send one app_tool query with an inner args pad of $n chars; return the
# reply text for that qid (draining any interleaved event frames).
function Query-Pad([System.IO.Pipes.NamedPipeClientStream]$s, [uint32]$qid, [int]$n) {
    $inner = '{"pad":"' + ('a' * $n) + '"}'
    $json = '{"tool":"app_tool","args":{"app":"nope","tool":"x","args":' + $inner + '}}'
    SendQuery $s $qid $json
    $deadline = [System.Diagnostics.Stopwatch]::StartNew()
    while ($deadline.ElapsedMilliseconds -lt 10000) {
        $f = Read-Frame $s 500
        if ($null -eq $f) { continue }
        if ($f.type -eq 18 -and $f.qid -eq [int]$qid) { return $f.text }
    }
    return ""
}

# --- run one full round (2-consecutive rule) -----------------------------------
function Invoke-Round([string]$tag) {
    Get-Process jkdesktop, jkwinserver, jkagentd, jkbridge -ErrorAction SilentlyContinue |
        Stop-Process -Force
    Start-Sleep -Seconds 1
    Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
    $up = $false
    $ctl = Join-Path $root "jkdesktop.exe"
    foreach ($i in 1..30) {
        Start-Sleep -Milliseconds 500
        $esc = '{"tool":"ping","args":{}}' -replace '"', '\"'
        if ((& $ctl agentctl $esc | Out-String) -match '"ok"\s*:\s*true') { $up = $true; break }
    }
    Check "$tag-setup-server-up" $up "ping"
    if (-not $up) { return }
    Start-Sleep -Seconds 1

    $s = New-Pipe
    Start-Sleep -Milliseconds 300

    # under: ~200KiB raw args -> cap passes -> unknown_app_tool
    $r1 = Query-Pad $s 1 199990
    Check "$tag-under-200k-passes" ($r1 -match '"error":"unknown_app_tool"' -and
                                    $r1 -notmatch 'args_too_large') ("len=" + $r1.Length + " head=" + $r1.Substring(0, [Math]::Min(80, $r1.Length)))

    # over: ~300KiB raw args -> args_too_large before the lookup
    $r2 = Query-Pad $s 2 299990
    Check "$tag-over-300k-rejected" ($r2 -match '"error":"args_too_large"') ("head=" + $r2.Substring(0, [Math]::Min(80, $r2.Length)))

    # exact edge: {"pad":"a"*n} = 10+n bytes raw. 262144 passes (cap is >),
    # 262145 fails - one byte either side of 256KiB.
    $r3 = Query-Pad $s 3 262134
    Check "$tag-exact-262144-passes" ($r3 -match '"error":"unknown_app_tool"' -and
                                      $r3 -notmatch 'args_too_large') ("head=" + $r3.Substring(0, [Math]::Min(80, $r3.Length)))
    $r4 = Query-Pad $s 4 262135
    Check "$tag-exact-262145-rejected" ($r4 -match '"error":"args_too_large"') ("head=" + $r4.Substring(0, [Math]::Min(80, $r4.Length)))

    $s.Dispose()
}

Invoke-Round "r1"
Start-Sleep -Seconds 2
Invoke-Round "r2"

Get-Process jkdesktop, jkwinserver -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Output "NOTICE: probe-owned server stopped - live stack restored separately"

if ($script:fail -gt 0) { Write-Output "FAILED: $($script:fail)"; exit 1 }
Write-Output "ALL PASS"
exit 0