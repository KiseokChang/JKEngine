# WS ping tool call against the bridge — used to verify auto-reconnect.
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$jb = (Get-Content (Join-Path $build "state\jkbridge.json") -Raw | ConvertFrom-Json)
$token = $jb.token; $port = $jb.port
try { $c = New-Object System.Net.Sockets.TcpClient("127.0.0.1", $port) }
catch { Write-Output "CONNECT-FAIL"; exit 1 }
$req = "GET /ws?token=$token HTTP/1.1`r`nHost: localhost`r`nUpgrade: websocket`r`n" +
       "Connection: Upgrade`r`nSec-WebSocket-Key: c3Byb3plcHJvYmUxMjM0NQ==`r`n" +
       "Sec-WebSocket-Version: 13`r`n`r`n"
$b = [Text.Encoding]::ASCII.GetBytes($req)
$c.GetStream().Write($b, 0, $b.Length)
Start-Sleep -Milliseconds 800
$buf = New-Object byte[] 2048
$n = $c.GetStream().Read($buf, 0, $buf.Length)
$resp = [Text.Encoding]::ASCII.GetString($buf, 0, [Math]::Max(0, $n))
if ($resp -notmatch "101") { Write-Output "HANDSHAKE-FAIL"; exit 1 }

function WsSend([System.Net.Sockets.TcpClient]$c, [string]$text) {
    $s = $c.GetStream()
    $payload = [Text.Encoding]::UTF8.GetBytes($text)
    $frame = New-Object System.Collections.Generic.List[byte]
    $frame.Add(0x81)
    $maskKey = Get-Random -Minimum 0 -Maximum 2147483647
    $m0 = ($maskKey -shr 24) -band 0xFF; $m1 = ($maskKey -shr 16) -band 0xFF
    $m2 = ($maskKey -shr 8) -band 0xFF;  $m3 = $maskKey -band 0xFF
    $mask = [byte[]]@( (($m0 -band 0x7F) -bor 0x80), $m1, $m2, $m3 )
    $len = $payload.Length
    if ($len -lt 126) { $frame.Add($len -bor 0x80) }
    else { $frame.Add(126 -bor 0x80); $frame.Add(($len -shr 8) -band 0xFF); $frame.Add($len -band 0xFF) }
    $frame.AddRange($mask)
    for ($i = 0; $i -lt $len; $i++) { $frame.Add($payload[$i] -bxor $mask[$i % 4]) }
    $b = $frame.ToArray()
    $s.Write($b, 0, $b.Length); $s.Flush()
}
function WsRecv([System.Net.Sockets.TcpClient]$c, [int]$timeoutMs) {
    $s = $c.GetStream()
    $s.ReadTimeout = $timeoutMs
    try {
        for (;;) {
            $b0 = $s.ReadByte(); if ($b0 -lt 0) { return $null }
            $b1 = $s.ReadByte()
            $op = $b0 -band 0x0F
            $len = $b1 -band 0x7F
            if ($len -eq 126) { $e0 = $s.ReadByte(); $e1 = $s.ReadByte(); $len = ($e0 -shl 8) -bor $e1 }
            elseif ($len -eq 127) { $len = 0; for ($i = 0; $i -lt 8; $i++) { $len = ($len -shl 8) -bor $s.ReadByte() } }
            if ($op -eq 0x9) { $s.Write([byte[]]@(0x8A, 0x00), 0, 2); continue }
            if ($op -eq 0x8) { return $null }
            $buf = New-Object byte[] $len
            $off = 0
            while ($off -lt $len) { $r = $s.Read($buf, $off, $len - $off); if ($r -le 0) { break }; $off += $r }
            return [Text.Encoding]::UTF8.GetString($buf, 0, $off)
        }
    } catch { return $null }
}
WsSend $c '{"type":"tool","tool":"ping","args":{},"label":"reconnect-test"}'
$deadline = [System.Diagnostics.Stopwatch]::StartNew()
while ($deadline.ElapsedMilliseconds -lt 20000) {
    $f = WsRecv $c 3000
    if ($f -match '"label":"reconnect-test"') {
        Write-Output ("REPLY: " + $f)
        $c.Close()
        exit 0
    }
    if ($f -match 'server connection lost') {
        Write-Output "SAW-LOST-MSG"
    }
}
Write-Output "TIMEOUT-no-reply"
$c.Close()
exit 1