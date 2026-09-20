# Parking flood cap probe (docs/56 §2b, docs/59 §16): one control-only
# connection parks trust_request approvals until the per-requester cap
# (8) rejects the 9th with approval_overflow. trust_request parks by
# default, but the user runtime permissions.json is full-allow — the probe
# owns a temporary {"trust_request":"ask"} file and restores the original
# (backup/restore convention, probe_settings lesson 6).
#
# Mechanics: the cap counts PendingApproval entries by requesterId
# (connection id), so all 9 queries MUST come from the SAME connection —
# agentctl one-shots each get a fresh id. Raw NamedPipeClientStream
# client — frame harness copied from probe_app_tools.ps1 (mgr_t8 lineage):
# Hello -> AgentEventSubscribe -> AgentQuery 1..9. Queries 1..8 park
# (replied=false, no reply frame), query 9 gets the approval_overflow
# reply, and the 8 parked approvals broadcast agent.approval_request
# events on the same subscription.
$ErrorActionPreference = "Continue"
$exe   = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root  = Split-Path $exe
$permFile = Join-Path $root "permissions.json"

# --- permissions.json lifecycle: backup -> ask-only -> restore ------------
$hadPerm = Test-Path $permFile
if ($hadPerm) { Copy-Item $permFile (Join-Path $env:TEMP "perm_overflow_backup.json") -Force }
'{"trust_request":"ask"}' | Set-Content -Path $permFile -Encoding ASCII

function Restore-Perm {
    if ($hadPerm) { Copy-Item (Join-Path $env:TEMP "perm_overflow_backup.json") $permFile -Force }
    else { Remove-Item $permFile -ErrorAction SilentlyContinue }
}

# --- raw pipe harness (probe_app_tools.ps1 lineage) ------------------------
# Wire constants verified against include/ipc/JKWireProtocol.h: hdr magic@0
# type@4 len@8; AgentQuery 17 {queryId,jsonLen}; AgentReply 18
# {queryId,ok,jsonLen}; AgentEvent 20 {connId}; AgentEventSubscribe 19
# {flag}; Hello 1 {version=2,pid} must precede the subscribe (acceptor
# decides control-only by the 2nd message).
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
    # PeekNamedPipe P/Invoke (probe_app_tools.ps1) — ReadAsync stalls under
    # PS5.1, availability polling is the proven drain primitive.
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
    $hs = 12   # AgentReply 18: {queryId,ok,jsonLen}
    if ($type -eq 20) { $hs = 4 }          # AgentEvent: {connId}
    elseif ($type -eq 17) { $hs = 8 }      # AgentQuery: {queryId,jsonLen}
    $text = ""
    if ($len -gt $hs) { $text = [Text.Encoding]::UTF8.GetString($pl, $hs, $len - $hs) }
    $qid = [BitConverter]::ToUInt32($pl, 0)
    return @{ type = $type; len = $len; text = $text; qid = [int]$qid }
}

# --- server lifecycle --------------------------------------------------------
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
$up = $false
$ctl = Join-Path $root "jkdesktop.exe"
foreach ($i in 1..30) {
    Start-Sleep -Milliseconds 500
    $esc = '{"tool":"ping","args":{}}' -replace '"', '\"'
    if ((& $ctl agentctl $esc | Out-String) -match '"ok"\s*:\s*true') { $up = $true; break }
}
if (-not $up) { Write-Host "server-up: FAIL"; Restore-Perm; exit 1 }

$s = New-Pipe 1
Start-Sleep -Milliseconds 300

# 9 trust_request queries from THIS connection — valid shape (name/origin/
# fingerprint validated before parking; unique fingerprints per query)
for ($i = 1; $i -le 9; $i++) {
    $fp = "sha256:" + ('{0:x16}{1:x16}{2:x16}{3:x16}' -f $i,$i,$i,$i)  # 정확 64hex
    SendQuery $s ([uint32]$i) ('{"tool":"trust_request","args":{"name":"overflow_probe_' +
        $i + '","origin":"dev","fingerprint":"' + $fp + '"}}')
    Start-Sleep -Milliseconds 100
}

# Drain replies + events. Expected: exactly ONE AgentReply (qid 9,
# approval_overflow), none for qid 1..8, and 8 approval_request events.
$replies = @{}
$approvals = 0
$deadline = [System.Diagnostics.Stopwatch]::StartNew()
while ($deadline.ElapsedMilliseconds -lt 15000) {
    $f = Read-Frame $s 300
    if ($null -eq $f) {
        if ($deadline.ElapsedMilliseconds -gt 6000) { break }
        continue
    }
    if ($f.type -eq 18) { $replies[$f.qid] = $f.text }
    if ($f.type -eq 20 -and $f.text -match '"topic":"agent\.approval_request"') { $approvals++ }
    if ($replies.Count -ge 1 -and $deadline.ElapsedMilliseconds -gt 6000) { break }
}
$s.Close()
$ok9    = ($replies.Count -eq 1 -and $replies.ContainsKey(9) -and
           $replies[9] -match 'approval_overflow')
$okPark = ($replies.Keys -notcontains 1 -and $replies.Keys -notcontains 8)
$okEv   = ($approvals -eq 8)
Write-Host ("overflow-reply: " + $(if ($ok9) { "PASS" } else { "FAIL — replies: " + (($replies.Keys | Sort-Object | ForEach-Object { "$_=$($replies[$_])" } | Out-String)) }))
Write-Host ("parked-1..8:     " + $(if ($okPark) { "PASS" } else { "FAIL" }))
Write-Host ("approval-events: " + $(if ($okEv) { "PASS (8)" } else { "FAIL ($approvals)" }))
Restore-Perm
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
if ($ok9 -and $okPark -and $okEv) { Write-Host "PASS: approval overflow" }
else { exit 1 }