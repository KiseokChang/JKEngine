# jkbridge probe: phone web gateway end-to-end (spec §7).
# Runs jkbridge from the BUILD dir (it links jkcore → SDL2.dll etc. live
# there) with probe-owned state swap: state\jkbridge.json (fixed token/port)
# and state\chat.json (stub engine — pollution guard, docs/54 lesson 6) are
# backed up and restored. Drives the wire with a raw PS TcpClient WS client.
# Checks: HTTP UI/health/404, token gate, rate limit (LAST — a 403-grade IP
# gate would poison later good-token checks), WS hello, stub chat done, tool
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
        $b0 = $s.ReadByte(); if ($b0 -lt 0) { return $null }
        $b1 = $s.ReadByte()
        $len = $b1 -band 0x7F
        if ($len -eq 126) { $e0 = $s.ReadByte(); $e1 = $s.ReadByte(); $len = ($e0 -shl 8) -bor $e1 }
        elseif ($len -eq 127) { $len = 0; for ($i = 0; $i -lt 8; $i++) { $len = ($len -shl 8) -bor $s.ReadByte() } }
        $buf = New-Object byte[] $len
        $off = 0
        while ($off -lt $len) { $n = $s.Read($buf, $off, $len - $off); if ($n -le 0) { break }; $off += $n }
        return [System.Text.Encoding]::UTF8.GetString($buf, 0, $off)
    } catch { return $null }
}

# --- 1. HTTP: health + UI + 404 ----------------------------------------------
try { $health = (Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:8899/health" -TimeoutSec 5).Content } catch { $health = "" }
Check "http-health" ($health -eq "ok")
try { $ui = (Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:8899/" -TimeoutSec 5).Content } catch { $ui = "" }
Check "http-ui" ($ui -match "doctype html" -and $ui -match "jkbridge")
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
Check "tool-reply" ($reply -match '"label":"list"' -and $reply -match '"ok"')

# --- 6. approve roundtrip: ask-gated close via the bridge --------------------
# agentctl argument passing: the probe_agent_chat convention (PS 5.1 strips
# embedded quotes on reparse — escape them the way the green probe does).
function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $server agentctl $escaped) -join "`n"
}
'{"close_window":"ask"}' | Set-Content -Path (Join-Path $build "permissions.json") -Encoding ASCII
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

# --- 7. session cap: 4 live sessions, the 5th refused ------------------------
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
try { $t = $c9.GetStream().Read($buf2, 0, $buf2.Length) } catch { $t = -1 }
# read either 0/-1 or throws — both mean "connection closed"
Check "frame-cap" ($t -le 0)
$c9.Close()

# --- 9. resume memo: hello with the stub session id → pending_result -------
$c10 = New-Sock "127.0.0.1" 8899
[void](WsHandshake $c10 "probetoken0123456789abcdef")
WsSend $c10 '{"type":"hello","resume_session":"stub-1"}'
$hello10 = WsRecv $c10 5000
Check "resume-memo" ($hello10 -ne $null -and $hello10 -match '"pending_result":"stub ok"')
$c10.Close()

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
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
if ($bridgeProc -and -not $bridgeProc.HasExited) { $bridgeProc.Kill() }
Remove-Item (Join-Path $build "permissions.json") -ErrorAction SilentlyContinue
if ($hadJb) { [System.IO.File]::WriteAllBytes($jbFile, $jbBak) } else { Remove-Item $jbFile -ErrorAction SilentlyContinue }
if ($hadChat) { [System.IO.File]::WriteAllBytes($chatFile, $chatBak) } else { Remove-Item $chatFile -ErrorAction SilentlyContinue }
Remove-Item -Recurse -Force $run -ErrorAction SilentlyContinue

if ($ok) { Write-Host "PASS: jkbridge"; exit 0 } else { Write-Host "FAIL: jkbridge"; exit 1 }