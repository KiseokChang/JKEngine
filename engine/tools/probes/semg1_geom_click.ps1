# semg1: click minesweeper "B" button via send_input (parks -> approve) to
# trigger NewGame full repaint + MINOR-4 redeclaration. Then capture.
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
function SendMsg([System.IO.Pipes.NamedPipeClientStream]$s, [int]$type, [byte[]]$payload) {
    $hdr = New-Object byte[] 12
    [BitConverter]::GetBytes([uint32]0x4A4B0001).CopyTo($hdr, 0)
    [BitConverter]::GetBytes([uint32]$type).CopyTo($hdr, 4)
    [BitConverter]::GetBytes([uint32]$payload.Length).CopyTo($hdr, 8)
    $s.Write($hdr, 0, 12)
    if ($payload.Length -gt 0) { $s.Write($payload, 0, $payload.Length) }
    $s.Flush()
}
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class PP1 {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool PeekNamedPipe(IntPtr hPipe, byte[] buf,
        int bufSize, out int read, out int avail, out int left);
}
"@
function Pipe-Avail([System.IO.Pipes.NamedPipeClientStream]$s) {
    $read = 0; $avail = 0; $left = 0
    try {
        $h = $s.SafePipeHandle.DangerousGetHandle()
        if (-not [PP1]::PeekNamedPipe($h, $null, 0, [ref]$read, [ref]$avail, [ref]$left)) { return -1 }
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
    $hs = 12; $head0 = 0
    if ($type -eq 20 -or $type -eq 22) { $hs = 4 }
    elseif ($type -eq 17 -or $type -eq 23) { $hs = 8; $head0 = [BitConverter]::ToUInt32($pl, 0) }
    $text = ""
    if ($len -gt $hs) { $text = [Text.Encoding]::UTF8.GetString($pl, $hs, $len - $hs) }
    return @{ type = $type; len = $len; text = $text; head0 = [uint32]$head0 }
}
function SendQuery([System.IO.Pipes.NamedPipeClientStream]$s, [uint32]$qid, [string]$json) {
    $body = [Text.Encoding]::UTF8.GetBytes($json)
    $payload = New-Object byte[] (8 + $body.Length)
    [BitConverter]::GetBytes([uint32]$qid).CopyTo($payload, 0)
    [BitConverter]::GetBytes([uint32]$body.Length).CopyTo($payload, 4)
    [Array]::Copy($body, 0, $payload, 8, $body.Length)
    SendMsg $s 17 $payload
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

$qid = [uint32]900
$qid++
SendQuery $p $qid '{"tool":"send_input","args":{"op":"click","id":11,"x":626,"y":251}}'
$ev = $null
$sw = [System.Diagnostics.Stopwatch]::StartNew()
while ($sw.ElapsedMilliseconds -lt 10000) {
    $f = Read-Frame $p 200
    if ($f -ne $null -and $f.type -eq 20 -and $f.text -match '"topic":"agent\.approval_request"') { $ev = $f; break }
}
if ($ev -eq $null) { Write-Output "NO-EVENT"; exit 1 }
Write-Output ("EVENT: " + $ev.text)
$reqId = 0
if ($ev.text -match '"request":(\d+)') { $reqId = [int]$Matches[1] }
$ap = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
Write-Output ("APPROVE: " + $ap)
$rep = $null
$sw.Restart()
while ($sw.ElapsedMilliseconds -lt 10000) {
    $f = Read-Frame $p 200
    if ($f -ne $null -and $f.type -eq 18) { $rep = $f.text; break }
}
Write-Output ("REPLY: " + $rep)
Start-Sleep -Milliseconds 1200
