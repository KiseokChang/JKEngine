# jkbridge probe: phone web gateway end-to-end (spec §7).
# Runs jkbridge from the BUILD dir (it links jkcore → SDL2.dll etc. live
# there) with probe-owned state swap: state\jkbridge.json (fixed token/port)
# and state\chat.json (stub engine — pollution guard, docs/54 lesson 6) are
# backed up and restored. Drives the wire with a raw PS TcpClient WS client.
# Checks: HTTP UI/health/404, app_tool approval-strip wording (served JS,
# task-9 review M-4), token gate, rate limit (LAST — a 403-grade IP
# relay, ask-gate close approval roundtrip, session cap, 1MiB frame cap,
# resume memo. jkwinserver (jkdesktop --server) lifecycle probe-managed.
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe = Join-Path $build "jkbridge.exe"
$server = Join-Path $build "jkdesktop.exe"
if (-not (Test-Path $exe)) { Write-Host "jkbridge.exe missing: FAIL"; exit 1 }

# --- probe-owned state swap ---------------------------------------------------
$stateDir = Join-Path $build "state"
$jbFile = Join-Path $stateDir "jkbridge.json"
$chatFile = Join-Path $stateDir "chat.json"
$hadJb = Test-Path $jbFile;  if ($hadJb) { $jbBak = [System.IO.File]::ReadAllBytes($jbFile) } else { $jbBak = $null }
$hadChat = Test-Path $chatFile; if ($hadChat) { $chatBak = [System.IO.File]::ReadAllBytes($chatFile) } else { $chatBak = $null }
'{"token":"probetoken0123456789abcdef","port":8899}' | Set-Content -Path $jbFile -Encoding ASCII
'{"engine":"stub"}' | Set-Content -Path $chatFile -Encoding ASCII
# State restore MUST survive every exit path: an end-of-file restore ran only
# on clean completion, so a mid-body crash leaked the probe token into the
# LIVE bridge state (2026-09-20: user's real jkbridge.json token replaced by
# probetoken*, phone URL dead). finally below is the only guarantee.
try {

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process jkbridge -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

$ok = $true
function Check($name, $cond) {
    if ($cond) { Write-Host "$name`: PASS" }
    else { $script:ok = $false; Write-Host "$name`: FAIL" }
}

# --- start the window server (tool relay checks need it) --------------------
Start-Process -FilePath $server -ArgumentList "--server" `
    -WorkingDirectory $build -WindowStyle Hidden
Start-Sleep -Seconds 4

# --- start jkbridge (redirected stdout: URL + self-test go to a file) -------
$run = Join-Path ([System.IO.Path]::GetTempPath()) "jkbridge_probe"
if (Test-Path $run) { Remove-Item -Recurse -Force $run }
New-Item -ItemType Directory -Path $run -Force | Out-Null
$bridgeProc = Start-Process -FilePath $exe `
    -WorkingDirectory $build -RedirectStandardOutput (Join-Path $run "bridge.log") `
    -RedirectStandardError (Join-Path $run "bridge_err.log") `
    -WindowStyle Hidden -PassThru
Start-Sleep -Seconds 2

# --- raw WS client helpers (PS 5.1, TcpClient) ------------------------------
function New-Sock([string]$h, [int]$p) {
    $c = New-Object System.Net.Sockets.TcpClient
    $c.Connect($h, $p)
    return $c
}
function Read-Until-Limit([System.Net.Sockets.NetworkStream]$s) {
    $buf = New-Object byte[] 4096
    $text = ""
    while ($text.Length -lt 8192 -and -not $text.Contains("`r`n`r`n")) {
        $n = $s.Read($buf, 0, $buf.Length)
        if ($n -le 0) { break }
        $text += [System.Text.Encoding]::ASCII.GetString($buf, 0, $n)
    }
    return $text
}
function WsHandshake([System.Net.Sockets.TcpClient]$c, [string]$token) {
    $s = $c.GetStream()
    $key = [Convert]::ToBase64String((1..16 | ForEach-Object { Get-Random -Maximum 256 }) -as [byte[]])
    $req = "GET /ws?token=$token HTTP/1.1`r`nHost: localhost`r`nUpgrade: websocket`r`n" +
           "Connection: Upgrade`r`nSec-WebSocket-Key: $key`r`nSec-WebSocket-Version: 13`r`n`r`n"
    $b = [System.Text.Encoding]::ASCII.GetBytes($req)
    $s.Write($b, 0, $b.Length)
    return (Read-Until-Limit $s)
}
function WsSend([System.Net.Sockets.TcpClient]$c, [string]$text) {
    $s = $c.GetStream()
    $payload = [System.Text.Encoding]::UTF8.GetBytes($text)
    $frame = New-Object System.Collections.Generic.List[byte]
    $frame.Add(0x81)
    $maskKey = Get-Random -Minimum 0 -Maximum 2147483647
    $m0 = ($maskKey -shr 24) -band 0xFF; $m1 = ($maskKey -shr 16) -band 0xFF
    $m2 = ($maskKey -shr 8) -band 0xFF;  $m3 = $maskKey -band 0xFF
    $mask = [byte[]]@( (($m0 -band 0x7F) -bor 0x80), $m1, $m2, $m3 )
    $n = $payload.Length
    if ($n -lt 126) { $frame.Add($n -bor 0x80) }
    elseif ($n -le 0xFFFF) { $frame.Add(126 -bor 0x80); $frame.Add(($n -shr 8) -band 0xFF); $frame.Add($n -band 0xFF) }
    else { $frame.Add(127 -bor 0x80); for ($i = 3; $i -ge 0; $i--) { $frame.Add(($n -shr ($i*8)) -band 0xFF) } }
    $frame.AddRange([byte[]]$mask)
    for ($i = 0; $i -lt $n; $i++) { $frame.Add($payload[$i] -bxor $mask[$i % 4]) }
    $b = $frame.ToArray()
    $s.Write($b, 0, $b.Length)
    $s.Flush()
}
function WsRecv([System.Net.Sockets.TcpClient]$c, [int]$waitMs) {
    $s = $c.GetStream()
    $s.ReadTimeout = $waitMs
    try {
        for (;;) {
            $b0 = $s.ReadByte(); if ($b0 -lt 0) { return $null }
            $b1 = $s.ReadByte()
            $op = $b0 -band 0x0F
            $len = $b1 -band 0x7F
            if ($len -eq 126) { $e0 = $s.ReadByte(); $e1 = $s.ReadByte(); $len = ($e0 -shl 8) -bor $e1 }
            elseif ($len -eq 127) { $len = 0; for ($i = 0; $i -lt 8; $i++) { $len = ($len -shl 8) -bor $s.ReadByte() } }
            # control frames: consume (the bridge heartbeats every ~4.8s),
            # answer pings with an (unmasked, lenient) pong, and keep reading
            if ($op -eq 0x9 -or $op -eq 0xA) {
                if ($len -gt 0) { $ctrl = New-Object byte[] $len; [void]($s.Read($ctrl, 0, $len)) }
                if ($op -eq 0x9) {
                    $pong = [byte[]]@(0x8A, 0x00)
                    $s.Write($pong, 0, 2); $s.Flush()
                }
                continue
            }
            $buf = New-Object byte[] $len
            $off = 0
            while ($off -lt $len) { $n = $s.Read($buf, $off, $len - $off); if ($n -le 0) { break }; $off += $n }
            return [System.Text.Encoding]::UTF8.GetString($buf, 0, $off)
        }
    } catch { return $null }
}

# --- 1. HTTP: health + UI + 404 ----------------------------------------------
try { $health = (Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:8899/health" -TimeoutSec 5).Content } catch { $health = "" }
Check "http-health" ($health -eq "ok")
try { $ui = (Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:8899/" -TimeoutSec 5).Content } catch { $ui = "" }
Check "http-ui" ($ui -match "doctype html" -and $ui -match "jkbridge")

# --- 1b. approval strip wording: app_tool branch (task-9 review M-4 fix) ------
# The strip text is rendered client-side, so the probe asserts the SERVED JS:
# the app_tool branch must render the target window honestly ("[<app> 창 #id]
# <tool> 실행할까요?" - jkchat wording) and the close_window fallback wording
# ("창을 닫을까요?") must stay untouched. Korean needles are built from
# codepoints - this file stays ASCII-only (PS5.1 encoding trap). $ui is
# decoded per the served "charset=utf-8" header, so string matching is exact.
function K([int[]]$c) { return (-join ($c | ForEach-Object { [char]$_ })) }
$krChang = K @(0xCC3D)                                  # chang (window)
$krShil  = K @(0xC2E4,0xD589,0xD560,0xAE4C,0xC694)      # silhaeng (exec)
$krChaRe = (K @(0xCC3D,0xC744)) + " " + (K @(0xB2EB,0xC744,0xAE4C,0xC694))  # chang-eul kka-yo (close re-ask)
Check "bridge-app-tool-strip" ($ui -match "k === 'app_tool'" -and
                               $ui -match ("' " + $krChang + " #'") -and
                               $ui -match ($krShil + "\?") -and
                               $ui -match $krChaRe)
try {
    Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:8899/bogus" -TimeoutSec 5 | Out-Null
    $code = 200
} catch { $code = [int]$_.Exception.Response.StatusCode }
Check "http-404" ($code -eq 404)

# --- 2. token gate: no token → 401 -------------------------------------------
$c0 = New-Sock "127.0.0.1" 8899
$noTok = "GET /ws HTTP/1.1`r`nHost: x`r`nUpgrade: websocket`r`nConnection: Upgrade`r`n" +
         "Sec-WebSocket-Key: AAAAAAAAAAAAAAAAAAAAAA==`r`nSec-WebSocket-Version: 13`r`n`r`n"
$b = [System.Text.Encoding]::ASCII.GetBytes($noTok)
$c0.GetStream().Write($b, 0, $b.Length)
$h0 = Read-Until-Limit $c0.GetStream()
Check "ws-no-token-401" ($h0 -match "401")
$c0.Close()

# --- 3. WS + hello ------------------------------------------------------------
$c1 = New-Sock "127.0.0.1" 8899
$h1 = WsHandshake $c1 "probetoken0123456789abcdef"
Check "ws-handshake-101" ($h1 -match "101 Switching Protocols")
WsSend $c1 '{"type":"hello","resume_session":""}'
$hello = WsRecv $c1 5000
Check "ws-hello-ok" ($hello -ne $null -and $hello -match '"type":"hello"' -and $hello -match '"ok":1')

# --- 4. stub chat: done arrives with the stub result -------------------------
WsSend $c1 '{"type":"chat","text":"stub hello"}'
$chatDone = $null
foreach ($i in 1..10) { $r = WsRecv $c1 3000; if ($r -match "chat_done") { $chatDone = $r; break } }
Check "stub-chat-done" ($chatDone -match '"ok":1' -and $chatDone -match "stub ok" -and $chatDone -match "stub-1")

# --- 5. tool relay: list_windows (window server up) --------------------------
WsSend $c1 '{"type":"tool","tool":"list_windows","args":{},"label":"list"}'
$reply = $null
foreach ($i in 1..10) { $r = WsRecv $c1 3000; if ($r -match '"type":"reply"') { $reply = $r; break } }
Check "tool-reply" ($reply -match '"label":"list"' -and $reply -match '"ok"\s*:\s*(true|1)')

# --- 5b. report: transcript dump -> state\bridge_report_*.txt (docs/57 §12) ---
# The phone posts its transcript; the bridge writes the file, replies with the
# path, and fires agent.notify. A second report inside 10 s hits the cooldown.
WsSend $c1 '{"type":"report","text":"probe report body 12345"}'
$repReply = $null
foreach ($i in 1..10) { $r = WsRecv $c1 3000; if ($r -match '"label":"report"') { $repReply = $r; break } }
$repPath = $null
if ($repReply -match '"path\\?":"([^"]+)"') { $repPath = $Matches[1] -replace '\\\\', '\' }
$repOk = ($repPath -ne $null -and (Test-Path $repPath) -and
          ((Get-Content $repPath -Raw -ErrorAction SilentlyContinue) -match "probe report body 12345"))
Check "report-file-written" $repOk ("path=$repPath")
if ($repPath -ne $null) { Remove-Item $repPath -Force -ErrorAction SilentlyContinue }
WsSend $c1 '{"type":"report","text":"probe cooldown probe"}'
$repCool = $null
foreach ($i in 1..6) { $r = WsRecv $c1 2000; if ($r -match "report_cooldown") { $repCool = $r; break } }
Check "report-cooldown" ($repCool -ne $null) "$repCool"

# --- 6. approve roundtrip: ask-gated close via the bridge --------------------
# agentctl argument passing: the probe_agent_chat convention (PS 5.1 strips
# embedded quotes on reparse — escape them the way the green probe does).
function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $server agentctl $escaped) -join "`n"
}
$permFile = Join-Path $build "permissions.json"
$hadPerm = Test-Path $permFile; if ($hadPerm) { $permBak = [System.IO.File]::ReadAllBytes($permFile) }
'{"close_window":"ask"}' | Set-Content -Path $permFile -Encoding ASCII
[void](Invoke-Agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}')
Start-Sleep -Seconds 2
$list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
$mineId = $null
if ($list -match '"id\\?":(\d+),"title":"Minesweeper"') { $mineId = $Matches[1] }
Check "minesweeper-launched" ($mineId -ne $null)

WsSend $c1 ('{"type":"tool","tool":"close_window","args":{"id":' + $mineId + '},"label":"close"}')
$apprReq = $null
foreach ($i in 1..12) { $r = WsRecv $c1 3000; if ($r -match '"request\\?":(\d+)') { $apprReq = $Matches[1]; break } }
Check "approval-request-event" ($apprReq -ne $null)

WsSend $c1 ('{"type":"approve","request":' + $apprReq + ',"decision":"allow"}')
$apprReply = $null
foreach ($i in 1..12) { $r = WsRecv $c1 3000; if ($r -match '"type":"reply"' -and $r -match "approved") { $apprReply = $r; break } }
Start-Sleep -Seconds 2
$list2 = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
Check "approve-roundtrip" ($apprReply -ne $null -and $list2 -notmatch ('"id\\?":' + $mineId + '\b'))

# --- 6b. app_tool ask THROUGH the bridge: the phone approves its own park ----
# Final-review Important 2. A fake app registers one tool, permissions gate it
# to ask; the phone relays the tool call - the bridge SESSION connection is
# the requester, so the ask parks on it. Pre-fix the phone's Allow died with
# self_approve (approve rode the same connection); it now rides a second
# approval-only control connection (jkchat cross-approve precedent) and the
# parked app result flows back to the phone as a labelled reply.
# Raw pipe helpers = task9_thumb_adhoc/probe_app_tools idiom.
function SendMsgB([System.IO.Pipes.NamedPipeClientStream]$s, [int]$type, [byte[]]$payload) {
    $hdr = New-Object byte[] 12
    [BitConverter]::GetBytes([uint32]0x4A4B0001).CopyTo($hdr, 0)
    [BitConverter]::GetBytes([uint32]$type).CopyTo($hdr, 4)
    [BitConverter]::GetBytes([uint32]$payload.Length).CopyTo($hdr, 8)
    $s.Write($hdr, 0, 12)
    if ($payload.Length -gt 0) { $s.Write($payload, 0, $payload.Length) }
    $s.Flush()
}
function New-PipeB([int]$subscriber) {
    $p = New-Object System.IO.Pipes.NamedPipeClientStream(".", "JKWindowServerPipe",
        [System.IO.Pipes.PipeDirection]::InOut)
    $p.Connect(5000)
    $hello = New-Object byte[] 8
    [BitConverter]::GetBytes([uint32]2).CopyTo($hello, 0)
    [BitConverter]::GetBytes([uint32]$PID).CopyTo($hello, 4)
    SendMsgB $p 1 $hello
    $sub = New-Object byte[] 4
    [BitConverter]::GetBytes([uint32]$subscriber).CopyTo($sub, 0)
    SendMsgB $p 19 $sub
    return $p
}
function Send-ToolRegisterB([System.IO.Pipes.NamedPipeClientStream]$s, [string]$json) {
    $body = [Text.Encoding]::UTF8.GetBytes($json)
    $payload = New-Object byte[] (4 + $body.Length)
    [BitConverter]::GetBytes([uint32]$body.Length).CopyTo($payload, 0)
    [Array]::Copy($body, 0, $payload, 4, $body.Length)
    SendMsgB $s 22 $payload
}
function Send-ToolResultB([System.IO.Pipes.NamedPipeClientStream]$s, [uint32]$reqId, [string]$json) {
    $body = [Text.Encoding]::UTF8.GetBytes($json)
    $payload = New-Object byte[] (12 + $body.Length)
    [BitConverter]::GetBytes([uint32]$reqId).CopyTo($payload, 0)
    [BitConverter]::GetBytes([uint32]1).CopyTo($payload, 4)
    [BitConverter]::GetBytes([uint32]$body.Length).CopyTo($payload, 8)
    [Array]::Copy($body, 0, $payload, 12, $body.Length)
    SendMsgB $s 24 $payload
}
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class PipePeekB {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool PeekNamedPipe(IntPtr hPipe, byte[] buf,
        int bufSize, out int read, out int avail, out int left);
}
"@
function Pipe-AvailB([System.IO.Pipes.NamedPipeClientStream]$s) {
    $read = 0; $avail = 0; $left = 0
    try {
        $h = $s.SafePipeHandle.DangerousGetHandle()
        if (-not [PipePeekB]::PeekNamedPipe($h, $null, 0, [ref]$read, [ref]$avail, [ref]$left)) { return -1 }
    } catch { return -1 }
    return $avail
}
# Same frame reader as probe_app_tools (deadline-bounded, whole-frame), plus
# the reqId the type-23 tool-call header carries (the fake app must echo it).
function Read-FrameB([System.IO.Pipes.NamedPipeClientStream]$s, [int]$timeoutMs) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ((Pipe-AvailB $s) -lt 12) {
        if ($sw.ElapsedMilliseconds -gt $timeoutMs) { return $null }
        Start-Sleep -Milliseconds 10
    }
    $hdr = New-Object byte[] 12
    if ($s.Read($hdr, 0, 12) -ne 12) { return $null }
    $type = [int][BitConverter]::ToUInt32($hdr, 4)
    $len = [int][BitConverter]::ToUInt32($hdr, 8)
    if ($len -lt 0 -or $len -gt (4 * 1024 * 1024)) { return $null }
    while ((Pipe-AvailB $s) -lt $len) {
        if ($sw.ElapsedMilliseconds -gt $timeoutMs) { return $null }
        Start-Sleep -Milliseconds 10
    }
    $pl = New-Object byte[] $len
    $off = 0
    while ($off -lt $len) {
        $a = Pipe-AvailB $s
        if ($a -le 0) { return $null }
        $n = [Math]::Min($a, $len - $off)
        $r = $s.Read($pl, $off, $n)
        if ($r -le 0) { return $null }
        $off += $r
    }
    $hs = 12; $reqId = [uint32]0
    if ($type -eq 20 -or $type -eq 22) { $hs = 4 }
    elseif ($type -eq 17) { $hs = 8 }
    elseif ($type -eq 23) { $hs = 8; $reqId = [BitConverter]::ToUInt32($pl, 0) }
    $text = ""
    if ($len -gt $hs) { $text = [Text.Encoding]::UTF8.GetString($pl, $hs, $len - $hs) }
    return @{ type = $type; len = $len; text = $text; reqId = $reqId }
}
$permAskAll = '{"close_window":"ask","app_tool.probeapp.echo":"ask"}'
[IO.File]::WriteAllText($permFile, $permAskAll, (New-Object System.Text.UTF8Encoding($false)))
$appPipe = New-PipeB 0   # control-only declarer - fake app, no window
Send-ToolRegisterB $appPipe '{"app":"probeapp","tools":[{"name":"echo","description":"probe echo tool"}]}'
$ackA = ""
foreach ($i in 1..20) {
    $f = Read-FrameB $appPipe 400
    if ($f -and $f.type -eq 18 -and $f.text -match '"ok"') { $ackA = $f.text; break }
}
Check "bridge-app-registered" ($ackA -match '"ok":true') $ackA

WsSend $c1 '{"type":"tool","tool":"app_tool","args":{"app":"probeapp","tool":"echo","args":{}},"label":"at"}'
$atReq = $null
foreach ($i in 1..12) {
    $r = WsRecv $c1 3000
    # 2nd -match order: the capturing match must run LAST or $Matches gets
    # clobbered by the group-less one (probe_notes lesson 5).
    if ($r -match 'app_tool' -and $r -match '"request\\?":(\d+)') { $atReq = $Matches[1]; break }
}
Check "bridge-ask-parks" ($atReq -ne $null)

# The approve the phone sends must NOT die with self_approve - it rides the
# bridge's second control connection (this exact flow failed pre-fix).
WsSend $c1 ('{"type":"approve","request":' + $atReq + ',"decision":"allow"}')
$apprAckB = $null
foreach ($i in 1..12) {
    $r = WsRecv $c1 3000
    if ($r -match '"label":"approve"' -and $r -match '"approved":true') { $apprAckB = $r; break }
}
Check "bridge-phone-approve" ($apprAckB -ne $null)

# The parked relay: the fake app got the AgentToolCall and its result must
# reach the phone as the labelled reply of the original tool query.
$callB = $null
foreach ($i in 1..25) {
    $f = Read-FrameB $appPipe 400
    if ($f -and $f.type -eq 23) { $callB = $f; break }
}
Check "bridge-tool-relayed" ($callB -ne $null -and $callB.text -match '"app":"probeapp"' -and $callB.text -match '"tool":"echo"')
$atReply = $null
if ($callB) {
    Send-ToolResultB $appPipe $callB.reqId '{"echo":"pong"}'
    foreach ($i in 1..12) {
        $r = WsRecv $c1 3000
        if ($r -match '"label":"at"' -and $r -match '"echo\\?":\\?"pong') { $atReply = $r; break }
    }
}
Check "bridge-parked-result" ($atReply -ne $null)

$appPipe.Dispose()
'{"close_window":"ask"}' | Set-Content -Path $permFile -Encoding ASCII

# --- 7. session cap: with c1 live, 4 more connects — growth past 4 refused ---
$socks = @()
foreach ($i in 1..4) {
    $c = New-Sock "127.0.0.1" 8899
    [void](WsHandshake $c "probetoken0123456789abcdef")
    WsSend $c '{"type":"hello","resume_session":""}'
    [void](WsRecv $c 3000)
    $socks += $c
    Start-Sleep -Milliseconds 300
}
$c5 = New-Sock "127.0.0.1" 8899
$h5 = WsHandshake $c5 "probetoken0123456789abcdef"
# the cap error frame is sent immediately after the 101 — Read-Until-Limit
# may have swallowed it. Anything beyond the head IS the frame.
$idx = $h5.IndexOf("`r`n`r`n")
$capped = if ($idx -ge 0 -and $idx + 4 -lt $h5.Length) { $h5.Substring($idx + 4) } else { $null }
if (-not $capped) { $capped = WsRecv $c5 3000 }
Check "session-cap" ($capped -ne $null -and $capped -match "too many sessions")
$c5.Close()
$socks | ForEach-Object { $_.Close() }
Start-Sleep -Seconds 2  # sessions drain, cap frees

# --- 8. frame cap: oversized declared length → connection dies --------------
$c9 = New-Sock "127.0.0.1" 8899
[void](WsHandshake $c9 "probetoken0123456789abcdef")
WsSend $c9 '{"type":"hello","resume_session":""}'
[void](WsRecv $c9 3000)
# header only, len=0xFFFFFFFFFFFFFFFF — refused before the mask read
$hdr = New-Object System.Collections.Generic.List[byte]
$hdr.Add(0x81); $hdr.Add(127)
foreach ($i in 1..8) { $hdr.Add(0xFF) }
$c9.GetStream().Write($hdr.ToArray(), 0, $hdr.ToArray().Length)
$c9.GetStream().ReadTimeout = 3000
$buf2 = New-Object byte[] 64
try {
    $t = $c9.GetStream().Read($buf2, 0, $buf2.Length)
} catch {
    # a bare timeout would be a false PASS — the server must actually close
    $sc = $_.Exception.InnerException.SocketErrorCode
    if ($sc -eq [System.Net.Sockets.SocketError]::TimedOut) { $t = -2 } else { $t = -1 }
}
Check "frame-cap" ($t -ne -2 -and ($t -le 0))
$c9.Close()

# --- 8b. slot reclaim: closed sessions free their slots (FIN-less leak guard)
Start-Sleep -Seconds 2
$c8 = New-Sock "127.0.0.1" 8899
[void](WsHandshake $c8 "probetoken0123456789abcdef")
WsSend $c8 '{"type":"hello","resume_session":""}'
$hello8 = WsRecv $c8 5000
Check "slot-reclaim" ($hello8 -ne $null -and $hello8 -match '"type":"hello"' -and $hello8 -match '"ok":1')
$c8.Close()

# --- 9. resume memo: hello with the stub session id → pending_result -------
# Retry-tolerant connect: stage 7's five sockets may still be draining when
# we get here (2s sleep is not a guarantee) — a refused c10 gets the cap
# error frame, not hello. That is a harness race, not a product defect, so
# reconnect until hello actually arrives (2026-09-20 flake: resume-memo
# FAIL ×3 → DBG instrumentation PASS → cap-frame cause).
$hello10 = $null
foreach ($try in 1..5) {
    $c10 = New-Sock "127.0.0.1" 8899
    [void](WsHandshake $c10 "probetoken0123456789abcdef")
    WsSend $c10 '{"type":"hello","resume_session":"stub-1"}'
    $hello10 = WsRecv $c10 5000
    $c10.Close()
    if ($hello10 -ne $null -and $hello10 -match '"type":"hello"') { break }
    Start-Sleep -Seconds 2
}
Check "resume-memo" ($hello10 -ne $null -and $hello10 -match '"pending_result":"stub ok"')

# --- 9b. QR startup output (console QR for phone camera scan) ----------------
# --qr-debug: 0/1 matrix dump (verifier interface); --qr-print: half-block
# glyphs. Structural checks here; full decode verified via cv2/pyzbar (docs/57).
$qrUrl = "http://192.168.0.34:8790/?token=a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6"
$qrDebug1 = & $exe --qr-debug $qrUrl 2>&1
$qrDebug2 = & $exe --qr-debug $qrUrl 2>&1
$lines = @($qrDebug1 | ForEach-Object { "$_" })
$hdr = ""
if ($lines.Count -ge 1) { $hdr = $lines[0] }
$mtx = @()
foreach ($l in $lines) { if ($l -match "^[01]{21,}$") { $mtx += $l } }
Check "qr-debug-parse" ($hdr -match "^version=\d+ mask=[0-7] size=\d+$" -and $mtx.Count -ge 21 -and $mtx.Count -eq 17 + 4 * [int]($hdr -replace "^version=(\d+).*", '$1'))
if ($mtx.Count -gt 0) {
    $n = $mtx[0].Length
    $finderTop = $mtx[0].Substring(0, 7)
    Check "qr-finder" ($finderTop -eq "1111111" -and $mtx[6].Substring(0, 7) -eq "1111111" -and $mtx[3].Substring(0, 7) -eq "1011101")
    $timing = ""
    for ($i = 8; $i -lt $n - 8; $i++) { $timing += $mtx[6][$i] }
    Check "qr-timing" ($timing -match "^1(01)*0?$")
    $v = [int]($hdr -replace "^version=(\d+).*", '$1')
    $darkOk = $mtx[$n - 8][8] -eq "1"
    Check "qr-dark-module" $darkOk
}
Check "qr-determinism" (("$qrDebug1" -replace "`r`n", "`n") -eq (("$qrDebug2" -replace "`r`n", "`n")))

$glyphs = $null
$savedEnc = [Console]::OutputEncoding
try {
    [Console]::OutputEncoding = [System.Text.Encoding]::UTF8  # glyphs are UTF-8
    $glyphs = & $exe --qr-print $qrUrl 2>&1
} finally {
    [Console]::OutputEncoding = $savedEnc
}
$gl = @($glyphs | ForEach-Object { "$_" }) | Where-Object { $_ -and $_.Trim().Length -gt 0 }
$onlyGlyphs = $true
foreach ($l in $gl) {
    if ("$l".Trim() -match "[^█▀▄ ]") { $onlyGlyphs = $false }
}
Check "qr-print-glyphs" ($gl.Count -ge 15 -and $onlyGlyphs -and ($gl[0].Length -ge 2 * 21))

# --- 10. rate limit: 10 bad tokens → the next one is refused (LAST — the IP
# gate would poison every good-token check after it) --------------------------
$limited = $false
foreach ($i in 1..11) {
    $c = New-Sock "127.0.0.1" 8899
    $h = WsHandshake $c ("bad" + $i)
    if ($h -match "403") { $limited = $true }
    $c.Close()
}
Check "rate-limit" $limited

# --- cleanup ------------------------------------------------------------------
} finally {
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
    if ($bridgeProc -and -not $bridgeProc.HasExited) { $bridgeProc.Kill() }
    if ($hadPerm) { [System.IO.File]::WriteAllBytes($permFile, $permBak) } else { Remove-Item $permFile -ErrorAction SilentlyContinue }
    if ($hadJb) { [System.IO.File]::WriteAllBytes($jbFile, $jbBak) } else { Remove-Item $jbFile -ErrorAction SilentlyContinue }
    if ($hadChat) { [System.IO.File]::WriteAllBytes($chatFile, $chatBak) } else { Remove-Item $chatFile -ErrorAction SilentlyContinue }
    Remove-Item -Recurse -Force $run -ErrorAction SilentlyContinue
}

if ($ok) { Write-Host "PASS: jkbridge"; exit 0 } else { Write-Host "FAIL: jkbridge"; exit 1 }