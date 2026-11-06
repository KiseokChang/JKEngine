# ad-hoc diag: rerun the ball turn, keep the LLM's script, sample hashes
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe = "$build\jkdesktop.exe"
$state = "$build\state"
$myapp = "$state\scripts\myapp.js"
function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    $out = (& $exe agentctl $escaped 2>$null) -join "`n"
    $idx = $out.IndexOf('{')
    if ($idx -lt 0) { return "" }
    return $out.Substring($idx)
}
$jb = (Get-Content (Join-Path $state "jkbridge.json") -Raw | ConvertFrom-Json)
$token = $jb.token; $port = $jb.port
function WsConnect([string]$tok, [int]$portNo) {
    $c = New-Object System.Net.Sockets.TcpClient("127.0.0.1", $portNo)
    $req = "GET /ws?token=$tok HTTP/1.1`r`nHost: localhost`r`nUpgrade: websocket`r`n" +
           "Connection: Upgrade`r`nSec-WebSocket-Key: c3Byb3plcHJvYmUxMjM0NQ==`r`n" +
           "Sec-WebSocket-Version: 13`r`n`r`n"
    $b = [Text.Encoding]::ASCII.GetBytes($req)
    $c.GetStream().Write($b, 0, $b.Length)
    Start-Sleep -Milliseconds 800
    $buf = New-Object byte[] 2048
    $n = $c.GetStream().Read($buf, 0, $buf.Length)
    $resp = [Text.Encoding]::ASCII.GetString($buf, 0, [Math]::Max(0, $n))
    if ($resp -notmatch "101") { Write-Output "DIAG handshake: $resp"; return $null }
    return $c
}
function WsSend([System.Net.Sockets.TcpClient]$c, [string]$text) {
    $s = $c.GetStream()
    $payload = [Text.Encoding]::UTF8.GetBytes($text)
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
            return [Text.Encoding]::UTF8.GetString($buf, 0, $off)
        }
    } catch { return $null }
}
$permBak = "$myapp.diag_bak"
if (Test-Path $myapp) { Copy-Item $myapp $permBak -Force; Write-Output "NOTICE: myapp.js backed up" }
try {
    $c = WsConnect $token $port
    if (-not $c) { Write-Output "NO-WS"; return }
    WsSend $c '{"type":"chat","text":"공 튀기는 앱을 만들어 줘."}'
    $done = $null
    $deadline = [System.Diagnostics.Stopwatch]::StartNew()
    while ($deadline.ElapsedMilliseconds -lt 300000) {
        $f = WsRecv $c 500
        if ($f -match '"type":"chat_done"') { $done = $f; break }
    }
    Write-Output ("done: " + ($(if ($done) { $done.Substring(0, [Math]::Min(220, $done.Length)) } else { "TIMEOUT" })))
    Start-Sleep -Seconds 2
    $lw = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    $m = [regex]::Match($lw, '\{"id":(\d+)[^}]*"title":"Workshop"')
    if (-not $m.Success) { Write-Output "NO-WORKSHOP-WINDOW"; return }
    $wid = $m.Groups[1].Value
    Write-Output "wid=$wid"
    # the LLM's script, verbatim
    $gs = Invoke-Agentctl '{"tool":"app_tool","args":{"app":"workshop","tool":"get_script","args":{}}}'
    $js = [regex]::Match($gs, '"source":"((?:[^"\\]|\\.)*)"').Groups[1].Value
    $js = $js -replace '\\n', "`n" -replace '\\"', '"' -replace '\\\\', '\'
    Write-Output "--- LLM SCRIPT BEGIN ---"
    Write-Output $js
    Write-Output "--- LLM SCRIPT END ---"
    # hash samples over 4s
    for ($i = 0; $i -lt 4; $i++) {
        $r = Invoke-Agentctl ('{"tool":"capture_window","args":{"id":' + $wid + '}}')
        $pm = [regex]::Match($r, '"path\\?":\\?"([^"]*)"')
        if ($pm.Success) {
            $p = $pm.Groups[1].Value -replace '\\\\', '\'
            $h = (Get-FileHash $p -Algorithm SHA256).Hash
            Write-Output ("t+" + ($i * 1200) + "ms hash=" + $h.Substring(0, 12) + " path=" + $p)
            if ($i -eq 0) { Copy-Item $p ("I:\progwork\JKENGINE\engine\tools\probes\diag_ball_frame0.png") -Force }
        }
        Start-Sleep -Milliseconds 1200
    }
    [void](Invoke-Agentctl ('{"tool":"close_window","args":{"id":' + $wid + '}}'))
} finally {
    $c.Close()
    if (Test-Path $permBak) { Copy-Item $permBak $myapp -Force; Remove-Item $permBak -Force; Write-Output "NOTICE: myapp.js restored" }
}