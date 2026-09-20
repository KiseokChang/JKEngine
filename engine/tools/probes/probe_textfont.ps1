# Desktop vector font settings surface probe (docs/63 Task 4, docs/54 hub):
# settings_set text_font_path KV roundtrip + settings_read surface + bad_value
# rejections. No atlas application is asserted here -- the key applies on
# restart (reply note "applies_on_restart"); the atlas side is probe_jktext.
#
# Runtime-file safety (docs/59 16.1 lesson): this probe owns a temporary copy
# of state\settings.json -- backup to settings.json.bak_probe BEFORE the run
# (console notice), restore the original afterwards, and the final set writes
# the original text.font_path value back when one existed.
#
# Harness: raw NamedPipeClientStream frame functions copied verbatim from
# probe_approval_overflow.ps1 (probe_app_tools lineage). Wire constants:
# Hello 1 {version=2,pid}, AgentEventSubscribe 19 {flag}, AgentQuery 17
# {queryId,jsonLen}, AgentReply 18 {queryId,ok,jsonLen}, pipe
# JKWindowServerPipe.
$ErrorActionPreference = "Continue"
$exe  = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root = Split-Path $exe
$settingsFile = Join-Path $root "state\settings.json"
$backupFile   = Join-Path $root "state\settings.json.bak_probe"

# --- check counter (probe_approval_overflow tail convention) -----------------
$script:g_pass = 0
$script:g_fail = 0
function CHECK([bool]$cond, [string]$name) {
    if ($cond) { $script:g_pass++; Write-Host ("PASS: " + $name) }
    else { $script:g_fail++; Write-Host ("FAIL: " + $name) }
}

# --- settings.json lifecycle: backup -> (run) -> restore ---------------------
$hadSettings = Test-Path $settingsFile
if ($hadSettings) {
    Copy-Item $settingsFile $backupFile -Force
    Write-Host "PROBE: backing up settings.json -> settings.json.bak_probe"
    $origText = [IO.File]::ReadAllText($settingsFile)
} else {
    Write-Host "PROBE: settings.json absent - backup skipped, restore will remove it"
    $origText = ""
}
# Original text.font_path (raw file text = already JSON-escaped) for the
# restore-set at the end.
$origFont = ""
if ($origText -match '"font_path"\s*:\s*"((?:[^"\\]|\\.)*)"') { $origFont = $Matches[1] }
Write-Host ("PROBE: original text.font_path = " + $(if ($origFont) { $origFont } else { "(none)" }))

# --- raw pipe harness (probe_approval_overflow.ps1 lineage, verbatim) --------
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

# --- server lifecycle (probe_approval_overflow convention) -------------------
# Remember what was running so the post-run restore matches the pre-run state.
$wasWinSrv = $null -ne (Get-Process jkwinserver -ErrorAction SilentlyContinue)
$wasSrv = $false
Get-CimInstance Win32_Process -Filter "name='jkdesktop.exe'" | ForEach-Object {
    if ($_.CommandLine -match '--server') { $wasSrv = $true }
}
Write-Host ("PROBE: server state before - jkwinserver=" + $wasWinSrv + " jkdesktop--server=" + $wasSrv)

function Stop-Desktop {
    Get-Process jkdesktop, jkwinserver -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 1
}
function Start-TestServer {
    Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
}
function Wait-Ready {
    $ctl = Join-Path $root "jkdesktop.exe"
    foreach ($i in 1..30) {
        Start-Sleep -Milliseconds 500
        $esc = '{"tool":"ping","args":{}}' -replace '"', '\"'
        if ((& $ctl agentctl $esc | Out-String) -match '"ok"\s*:\s*true') { return $true }
    }
    return $false
}
function Restore-Server {
    if ($wasWinSrv) {
        Start-Process -FilePath (Join-Path $root "jkwinserver.exe") -WorkingDirectory $root -WindowStyle Hidden
    } elseif ($wasSrv) {
        Start-TestServer
    }
    if ($wasWinSrv -or $wasSrv) {
        if (Wait-Ready) { Write-Host "PROBE: server restored" }
        else { Write-Host "PROBE: server-up after restore: FAIL" }
    } else {
        Write-Host "PROBE: no server was running before - leaving it stopped"
    }
}

Stop-Desktop
Start-TestServer
$up = Wait-Ready
if (-not $up) {
    Write-Host "server-up: FAIL"
    if (Test-Path $backupFile) { Copy-Item $backupFile $settingsFile -Force; Write-Host "PROBE: settings.json restored from backup" }
    Restore-Server
    exit 1
}
Write-Host "server-up: PASS"

# --- probe body: settings_set text_font_path roundtrip -----------------------
function Wait-Reply([System.IO.Pipes.NamedPipeClientStream]$pipe, [int]$qid, [int]$timeoutMs) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $timeoutMs) {
        $f = Read-Frame $pipe 500
        if ($f -and $f.type -eq 18 -and $f.qid -eq $qid) { return $f.text }
    }
    return $null
}

$p = New-Pipe 0
Start-Sleep -Milliseconds 300

# T1: valid set -> ok + applies_on_restart note (KV write, no live apply)
$qid = 1
SendQuery $p ([uint32]$qid) '{"tool":"settings_set","args":{"key":"text_font_path","value":"C:\\Windows\\Fonts\\malgun.ttf"}}'
$r = Wait-Reply $p $qid 5000
CHECK ($null -ne $r -and $r -match '"ok":true') "T1 set ok"
CHECK ($null -ne $r -and $r -match 'applies_on_restart') "T1 restart note"

# T2: settings_read surfaces text.font_path with the set value
$qid = 2
SendQuery $p ([uint32]$qid) '{"tool":"settings_read","args":{}}'
$r = Wait-Reply $p $qid 5000
CHECK ($null -ne $r -and $r -match '"key":"text\.font_path"') "T2 read shows text.font_path"
CHECK ($null -ne $r -and $r -match 'malgun\.ttf') "T2 value echoed"

# T3: oversized (400 chars > 300 cap) rejected with bad_value
$long = 'C:\' + ('a' * 400) + '.ttf'
$qid = 3
SendQuery $p ([uint32]$qid) ('{"tool":"settings_set","args":{"key":"text_font_path","value":"' + $long.Replace('\', '\\') + '"}}')
$r = Wait-Reply $p $qid 5000
CHECK ($null -ne $r -and $r -match '"error":"bad_value"') "T3 oversized rejected"

# T4: empty value rejected with bad_value (not a silent fallback)
$qid = 4
SendQuery $p ([uint32]$qid) '{"tool":"settings_set","args":{"key":"text_font_path","value":""}}'
$r = Wait-Reply $p $qid 5000
CHECK ($null -ne $r -and $r -match '"error":"bad_value"') "T4 empty rejected"

# T6/T7/T8 (docs/63 §6 2단계): text_font_fallback roundtrip. Unlike
# text_font_path the empty value is ACCEPTED (empty = chain disabled).
$qid = 6
SendQuery $p ([uint32]$qid) '{"tool":"settings_set","args":{"key":"text_font_fallback","value":"C:\\Windows\\Fonts\\consola.ttf"}}'
$r = Wait-Reply $p $qid 5000
CHECK ($null -ne $r -and $r -match '"ok":true' -and $r -match 'applies_on_restart') "T6 fallback set ok + restart note"

$qid = 7
SendQuery $p ([uint32]$qid) '{"tool":"settings_read","args":{}}'
$r = Wait-Reply $p $qid 5000
CHECK ($null -ne $r -and $r -match '"key":"text\.font_fallback"' -and $r -match 'consola\.ttf') "T7 read echoes text.font_fallback"

$qid = 8
SendQuery $p ([uint32]$qid) '{"tool":"settings_set","args":{"key":"text_font_fallback","value":""}}'
$r = Wait-Reply $p $qid 5000
CHECK ($null -ne $r -and $r -match '"ok":true' -and $r -match 'applies_on_restart') "T8 empty fallback = clear (allowed)"

# T5 (restore): write the original text.font_path back when one existed
if ($origFont -ne "") {
    $qid = 5
    SendQuery $p ([uint32]$qid) ('{"tool":"settings_set","args":{"key":"text_font_path","value":"' + $origFont + '"}}')
    $r = Wait-Reply $p $qid 5000
    CHECK ($null -ne $r -and $r -match '"ok":true') "T5 restore-set ok"
} else {
    Write-Host "SKIP: T5 restore-set (no original text.font_path - file restore covers it)"
}
$p.Dispose()

# --- teardown: stop test server, restore settings.json, restore server -------
Stop-Desktop
if ($hadSettings) { Copy-Item $backupFile $settingsFile -Force }
else { Remove-Item $settingsFile -ErrorAction SilentlyContinue }
Write-Host "PROBE: settings.json restored"
$restoredOk = $true
if ($hadSettings) {
    $nowText = ""
    if (Test-Path $settingsFile) { $nowText = [IO.File]::ReadAllText($settingsFile) }
    if ($nowText -ne $origText) {
        Write-Host "PROBE: settings.json restore MISMATCH - keeping settings.json.bak_probe for inspection"
        CHECK $false "settings.json byte-identical restore"
    } else {
        Remove-Item $backupFile -Force
        Write-Host "PROBE: restore verified byte-identical - backup removed"
    }
}
Restore-Server

Write-Host ("checks: " + $g_pass + " pass, " + $g_fail + " fail")
if ($g_fail -eq 0) { Write-Host "PASS: text font settings" }
else { exit 1 }