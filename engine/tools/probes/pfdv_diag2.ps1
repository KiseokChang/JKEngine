# Diag 2: does the dialog CRASH (app.crashed) or exit gracefully (window.destroyed)
# on choose-of-a-file? Subscriber pipe + non-blocking file_open + choose.
# ASCII only. Own server spawn + teardown.
$ErrorActionPreference = 'Continue'
$root = "I:\progwork\JKENGINE\engine\build"
$exe  = Join-Path $root "jkdesktop.exe"
$permFile = "I:\progwork\JKENGINE\engine\build\permissions.json"
$permBackup = $null
if (Test-Path $permFile) { $permBackup = [IO.File]::ReadAllText($permFile) }
# file_open + app_tool need allow; filedlg flow uses no permissions beyond tool gates
Set-Perms '{"close_window":"allow","run_console_app":"allow","file_open":"allow"}'

function Invoke-Agentctl([string]$json) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = 'agentctl "' + ($json -replace '"', '\"') + '"'
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $p = [System.Diagnostics.Process]::Start($psi)
    $out = $p.StandardOutput.ReadToEnd()
    $p.WaitForExit()
    return ($out -split "`n" | Where-Object { $_ -match '^\{' }) -join "`n"
}

# --- pipe helpers (probe_filedlg_voice idiom) ---
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
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class PipePeekD2 {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool PeekNamedPipe(IntPtr hPipe, byte[] buf,
        int bufSize, out int read, out int avail, out int left);
}
"@
function Pipe-Avail([System.IO.Pipes.NamedPipeClientStream]$s) {
    $read = 0; $avail = 0; $left = 0
    try {
        $h = $s.SafePipeHandle.DangerousGetHandle()
        if (-not [PipePeekD2]::PeekNamedPipe($h, $null, 0, [ref]$read, [ref]$avail, [ref]$left)) { return -1 }
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
    $qid = 0; $okf = 0
    if ($type -eq 18 -and $len -ge 12) {
        $qid = [int][BitConverter]::ToUInt32($pl, 0)
        $okf = [int][BitConverter]::ToUInt32($pl, 4)
    }
    $text = ""
    if ($len -gt $hs) { $text = [Text.Encoding]::UTF8.GetString($pl, $hs, $len - $hs) }
    return @{ type = $type; len = $len; qid = $qid; ok = $okf; text = $text }
}

# --- main ---
$fix = "I:\progwork\JKENGINE\tmp\pfdv_diag2"
New-Item -ItemType Directory -Force -Path (Join-Path $fix "start") | Out-Null
Set-Content -Path (Join-Path $fix "start\a.txt") -Value "x"
Set-Content -Path (Join-Path $fix "start\b.txt") -Value "y"

Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden | Out-Null
Start-Sleep -Milliseconds 1500
$serverPid = 0
foreach ($c in (Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'")) {
    if ($c.CommandLine -and $c.CommandLine -match '--server') { $serverPid = $c.ProcessId }
}
"SERVER pid=$serverPid"

$connS = New-Pipe 1
SendQuery $connS 401 ('{"tool":"file_open","args":{"start":"' + ($fix -replace '\\','/') + '/start"}}')

$dlgPid = 0
foreach ($i in 1..30) {
    Start-Sleep -Milliseconds 400
    foreach ($c in (Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'")) {
        if ($c.ParentProcessId -eq $serverPid -and $c.CommandLine -and $c.CommandLine -match '--filedlg') { $dlgPid = $c.ProcessId; break }
    }
    if ($dlgPid -gt 0) { break }
}
"DIALOG pid=$dlgPid"
Start-Sleep -Seconds 2

# drain events seen so far
foreach ($i in 1..10) { $f = Read-Frame $connS 100; if ($f) { "PRE-EVENT t=$($f.type) q=$($f.qid) $($f.text)" } }

# choose a file via agentctl (separate connection, like the voice agent)
$ch = Invoke-Agentctl '{"tool":"app_tool","args":{"app":"filedlg","tool":"choose","args":{"name":"a.txt"}}}'
"CHOOSE -> $ch"

# dialog state right after
Start-Sleep -Milliseconds 500
$alive = [bool](Get-Process -Id $dlgPid -ErrorAction SilentlyContinue)
"DIALOG alive=$alive"

# watch events + parked reply for up to 6s
foreach ($i in 1..60) {
    $f = Read-Frame $connS 100
    if ($f) { "EVENT t=$($f.type) q=$($f.qid) ok=$($f.ok) $($f.text)" }
}

# teardown
if ($alive) { Stop-Process -Id $dlgPid -Force -ErrorAction SilentlyContinue }
Stop-Process -Id $serverPid -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 800
Remove-Item -Recurse -Force $fix -ErrorAction SilentlyContinue
if ($null -ne $permBackup) { [IO.File]::WriteAllText($permFile, $permBackup, (New-Object System.Text.UTF8Encoding($false))) } else { Remove-Item $permFile -ErrorAction SilentlyContinue }
"TEARDOWN done perms-raw=[$permBackup]"