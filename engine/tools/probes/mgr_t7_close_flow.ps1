# mgr_t7_close_flow: chat-equivalent close_window flow verification.
# Same-connection park + approve (close_window self-approve exception,
# docs/31 §3) — mirrors the jkchat approval strip. Server must be running.
$ErrorActionPreference = "Stop"
$root = "I:\progwork\JKENGINE\engine\build"

$pipe = New-Object System.IO.Pipes.NamedPipeClientStream(".", "JKWindowServerPipe",
    [System.IO.Pipes.PipeDirection]::InOut)
$pipe.Connect(5000)
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
$hello = New-Object byte[] 8
[BitConverter]::GetBytes([uint32]2).CopyTo($hello, 0)
[BitConverter]::GetBytes([uint32]$PID).CopyTo($hello, 4)
SendMsg $pipe 1 $hello
$sub = New-Object byte[] 4
[BitConverter]::GetBytes([uint32]1).CopyTo($sub, 0)
SendMsg $pipe 19 $sub
function SendQuery([System.IO.Pipes.NamedPipeClientStream]$s, [uint32]$qid, [string]$json) {
    $body = [Text.Encoding]::UTF8.GetBytes($json)
    $payload = New-Object byte[] (8 + $body.Length)
    [BitConverter]::GetBytes([uint32]$qid).CopyTo($payload, 0)
    [BitConverter]::GetBytes([uint32]$body.Length).CopyTo($payload, 4)
    [Array]::Copy($body, 0, $payload, 8, $body.Length)
    SendMsg $s 17 $payload
}
function ReadFrame([System.IO.Pipes.NamedPipeClientStream]$s) {
    $hdr = ReadExact $s 12
    return @{
        type = [int][BitConverter]::ToUInt32($hdr, 4)
        len  = [int][BitConverter]::ToUInt32($hdr, 8)
    }
}
function ReadPayloadBytes([System.IO.Pipes.NamedPipeClientStream]$s, [int]$len) {
    if ($len -le 0) { return (New-Object byte[] 0) }
    return (ReadExact $s $len)
}
# 진단용: 모든 프레임의 type/len/prefix hex/payload 텍스트를 덤프한다
function DumpFrame([System.IO.Pipes.NamedPipeClientStream]$s) {
    $f = ReadFrame $s
    $pl = ReadPayloadBytes $s $f.len
    $hex = ""
    foreach ($b in ($pl | Select-Object -First 12)) { $hex += ('{0:x2}' -f $b) + " " }
    $txt = ""
    if ($pl.Length -gt 12) { $txt = [Text.Encoding]::UTF8.GetString($pl, 12, $pl.Length - 12) }
    Write-Host ("frame type=" + $f.type + " len=" + $f.len + " prefix=[" + $hex + "] body=" + $txt)
    return $f
}

# 1. list_windows -> find Minesweeper
SendQuery $pipe 1 '{"tool":"list_windows","args":{}}'
$winId = 0
foreach ($i in 1..30) {
    $f = ReadFrame $pipe
    if ($f.type -eq 18) {
        $pl = ReadPayloadBytes $pipe $f.len
        $txt = [Text.Encoding]::UTF8.GetString($pl, 4, $pl.Length - 4)
        if ($txt -match '"id":(\d+),"title":"Minesweeper"') { $winId = [int]$Matches[1]; break }
    }
}
Write-Host "mine-id=$winId"
if ($winId -eq 0) { Write-Host "RESULT: FAIL (no minesweeper)"; exit 1 }

# 2. close_window -> parked; capture request id from the approval push
SendQuery $pipe 2 ('{"tool":"close_window","args":{"id":' + $winId + '}}')
$reqId = 0
foreach ($i in 1..30) {
    $f = ReadFrame $pipe
    if ($f.type -eq 20) {
        $pl = ReadPayloadBytes $pipe $f.len
        $ev = [Text.Encoding]::UTF8.GetString($pl, 4, $pl.Length - 4)
        $rid = 0
        if ($ev -match '"request":(\d+)') { $rid = [int]$Matches[1] }
        if ($rid -gt 0 -and $ev -match '"tool":"close_window"') { $reqId = $rid; break }
        # 주의: close_window 승인 이벤트 페이로드에는 kind 필드가 없다
        # (JKWindowServer.cpp:2604 — tool 필드로 식별). kind 매칭은 영원히
        # 블록한다.
    }
}
Write-Host "parked-request=$reqId"
if ($reqId -eq 0) { Write-Host "RESULT: FAIL (no approval push)"; exit 1 }

# 3. approve from the SAME connection (chat strip equivalent)
SendQuery $pipe 3 ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
$got = ""
foreach ($i in 1..30) {
    $f = DumpFrame $pipe
    if ($f.type -eq 18) {
        $pl = ReadPayloadBytes $pipe $f.len
        $txt = [Text.Encoding]::UTF8.GetString($pl)
        if ($txt -match '"approved":true') { $got = $txt; break }
    }
}
Write-Host "approve-reply=$got"
Start-Sleep -Seconds 2

# 4. window must be gone
SendQuery $pipe 4 '{"tool":"list_windows","args":{}}'
$gone = $false
foreach ($i in 1..30) {
    $f = ReadFrame $pipe
    if ($f.type -eq 18) {
        $pl = ReadPayloadBytes $pipe $f.len
        $txt = [Text.Encoding]::UTF8.GetString($pl, 4, $pl.Length - 4)
        if ($txt -match '"windows"') {
            $gone = ($txt -notmatch ('"id":' + $winId + '\b')); break
        }
    }
}
$pipe.Dispose()
if ($got -ne "" -and $gone) { Write-Host "RESULT: PASS"; exit 0 }
Write-Host "RESULT: FAIL (approved=$($got -ne ''), window-gone=$gone)"
exit 1