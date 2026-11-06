# mgr_t8_cross_approve: cross-connection approve frame delivery test.
# Conn A parks close_window + subscribes; agentctl (different conn) approves.
# Prints every frame A receives. Same-connection approve anomaly discriminator.
$ErrorActionPreference = "Stop"
$build = "I:\progwork\JKENGINE\engine\build"

$pipeA = New-Object System.IO.Pipes.NamedPipeClientStream(".", "JKWindowServerPipe",
    [System.IO.Pipes.PipeDirection]::InOut)
$pipeA.Connect(5000)
function ReadExact([System.IO.Pipes.NamedPipeClientStream]$s, [int]$n) {
    $buf = New-Object byte[] $n; $off = 0
    while ($off -lt $n) {
        $r = $s.Read($buf, $off, $n - $off)
        if ($r -le 0) { throw "pipe closed" }
        $off += $r
    }
    return ,$buf
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
function SendQuery([System.IO.Pipes.NamedPipeClientStream]$s, [uint32]$qid, [string]$json) {
    $body = [Text.Encoding]::UTF8.GetBytes($json)
    $payload = New-Object byte[] (8 + $body.Length)
    [BitConverter]::GetBytes([uint32]$qid).CopyTo($payload, 0)
    [BitConverter]::GetBytes([uint32]$body.Length).CopyTo($payload, 4)
    [Array]::Copy($body, 0, $payload, 8, $body.Length)
    SendMsg $s 17 $payload
}
function ReadFrame2([System.IO.Pipes.NamedPipeClientStream]$s) {
    $hdr = ReadExact $s 12
    return @{
        type = [int][BitConverter]::ToUInt32($hdr, 4)
        len  = [int][BitConverter]::ToUInt32($hdr, 8)
    }
}
function ReadPayloadBytes2([System.IO.Pipes.NamedPipeClientStream]$s, [int]$len) {
    if ($len -le 0) { return (New-Object byte[] 0) }
    return (ReadExact $s $len)
}
# 주의: payload를 DumpFrame이 통째로 소비하므로 반환 text를 재활용한다 —
# DumpFrame 후 ReadPayloadBytes 재호출은 다음 프레임 헤더를 삼켜 misalign.
function DumpFrame2([System.IO.Pipes.NamedPipeClientStream]$s) {
    $f = ReadFrame2 $s
    $pl = ReadPayloadBytes2 $s $f.len
    $hex = ""
    foreach ($b in ($pl | Select-Object -First 12)) { $hex += ('{0:x2}' -f $b) + " " }
    $txt = ""
    if ($pl.Length -gt 12) { $txt = [Text.Encoding]::UTF8.GetString($pl, 12, $pl.Length - 12) }
    Write-Host ("frame type=" + $f.type + " len=" + $f.len + " prefix=[" + $hex + "] body=" + $txt)
    return @{ type = $f.type; len = $f.len; text = $txt }
}

$hello = New-Object byte[] 8
[BitConverter]::GetBytes([uint32]2).CopyTo($hello, 0)
[BitConverter]::GetBytes([uint32]$PID).CopyTo($hello, 4)
SendMsg $pipeA 1 $hello
$sub = New-Object byte[] 4
[BitConverter]::GetBytes([uint32]1).CopyTo($sub, 0)
SendMsg $pipeA 19 $sub

# 1. find minesweeper
SendQuery $pipeA 1 '{"tool":"list_windows","args":{}}'
$winId = 0
foreach ($i in 1..30) {
    $f = DumpFrame2 $pipeA
    if ($f.type -eq 18) {
        if ($f.text -match '"id":(\d+),"title":"Minesweeper"') { $winId = [int]$Matches[1]; break }
    }
}
Write-Host "mine-id=$winId"
if ($winId -eq 0) { Write-Host "RESULT: FAIL (no minesweeper)"; exit 1 }

# 2. park close_window on conn A
SendQuery $pipeA 2 ('{"tool":"close_window","args":{"id":' + $winId + '}}')
$reqId = 0
foreach ($i in 1..30) {
    $f = DumpFrame2 $pipeA
    if ($f.type -eq 20) {
        $rid = 0
        if ($f.text -match '"request":(\d+)') { $rid = [int]$Matches[1] }
        if ($rid -gt 0 -and $f.text -match '"tool":"close_window"') { $reqId = $rid; break }
    }
}
Write-Host "parked-request=$reqId"
if ($reqId -eq 0) { Write-Host "RESULT: FAIL (no approval push)"; exit 1 }

# 3. CROSS approve via agentctl (different connection) — proven ack path.
# PS5.1 함정(docs/55 레슨 ③): 임베디드 quote는 \" escape로 넘겨야 argv
# 재파싱이 JSON을 자르지 않는다(probe_approve_self의 Invoke-Agentctl 규약).
$ajson = '{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}'
$escaped = $ajson -replace '"', '\"'
$ap = (& "$build\jkdesktop.exe" agentctl $escaped | Select-Object -Last 1)
Write-Host "agentctl-approve=$ap"

# 4. conn A: everything after resolution — parked reply, resolved push, etc.
foreach ($i in 1..6) {
    $f = DumpFrame2 $pipeA
    if ($f.type -eq 18 -and $f.text -match '"windows"') {
        Write-Host "RESULT: PASS (full frame flow)"; exit 0
    }
}
Write-Host "RESULT: DONE (frames above show what A received)"
exit 0