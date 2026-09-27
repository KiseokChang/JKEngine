# Phone-mirror probe (docs/67 stage-1 refine T3): window_frame (JPEG b64,
# no disk write) + list_windows dw/dh (display size) + tap-math via
# send_input. Drives the SAME relay frame as the phone web UI:
#   {"type":"tool","tool":"window_frame","args":{"id":N,"maxw":960},"label":"f"}
# Skeleton = probe_jkbridge.ps1 (state swap, raw TcpClient WS client, PS5.1).
# Check order is FIXED: rate-limit check is LAST (a 403-grade IP gate
# poisons every later check, probe_jkbridge lesson).
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe = Join-Path $build "jkbridge.exe"
$server = Join-Path $build "jkdesktop.exe"
if (-not (Test-Path $exe)) { Write-Host "jkbridge.exe missing: FAIL"; exit 1 }

# --- probe-owned state swap (restore ONLY in finally) --------------------------
$stateDir = Join-Path $build "state"
$jbFile = Join-Path $stateDir "jkbridge.json"
$chatFile = Join-Path $stateDir "chat.json"
$hadJb = Test-Path $jbFile;  if ($hadJb) { $jbBak = [System.IO.File]::ReadAllBytes($jbFile) } else { $jbBak = $null }
$hadChat = Test-Path $chatFile; if ($hadChat) { $chatBak = [System.IO.File]::ReadAllBytes($chatFile) } else { $chatBak = $null }
'{"token":"probetoken0123456789abcdef","port":8899}' | Set-Content -Path $jbFile -Encoding ASCII
'{"engine":"stub"}' | Set-Content -Path $chatFile -Encoding ASCII
$permFile = Join-Path $build "permissions.json"
$hadPerm = Test-Path $permFile; if ($hadPerm) { $permBak = [System.IO.File]::ReadAllBytes($permFile) }
# taps must land without approval; window_frame default-allow needs no key
'{"send_input":"allow","window_frame":"allow"}' | Set-Content -Path $permFile -Encoding ASCII

$script:fail = 0
function Check([string]$name, [bool]$cond, [string]$detail) {
    if ($cond) { Write-Host "PASS: $name" }
    else { $script:fail++; Write-Host "FAIL: $name -- $detail" }
}
function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $server agentctl $escaped) -join "`n"
}

try {

Get-Process jkdesktop, jkwinserver, jkbridge, jkagentd -ErrorAction SilentlyContinue |
    Stop-Process -Force
Start-Sleep -Seconds 1
Write-Output "NOTICE: live desktop/bridge processes stopped for the probe run"

# --- start the window server + bridge ----------------------------------------
Start-Process -FilePath $server -ArgumentList "--server" `
    -WorkingDirectory $build -WindowStyle Hidden
Start-Sleep -Seconds 4
$run = Join-Path ([System.IO.Path]::GetTempPath()) "jkmirror_probe"
if (Test-Path $run) { Remove-Item -Recurse -Force $run }
New-Item -ItemType Directory -Path $run -Force | Out-Null
Start-Process -FilePath $exe `
    -WorkingDirectory $build -RedirectStandardOutput (Join-Path $run "bridge.log") `
    -RedirectStandardError (Join-Path $run "bridge_err.log") `
    -WindowStyle Hidden
Start-Sleep -Seconds 2

# --- raw WS client helpers (probe_jkbridge idiom) ----------------------------
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
function Send-Tool([System.Net.Sockets.TcpClient]$c, [string]$tool, [string]$argsJson, [string]$label) {
    WsSend $c ('{"type":"tool","tool":"' + $tool + '","args":' + $argsJson + ',"label":"' + $label + '"}')
}

# --- s1: minesweeper window (target of every frame check) --------------------
[void](Invoke-Agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}')
Start-Sleep -Seconds 2
$listCtl = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
$mineId = $null
if ($listCtl -match '"id\\?":(\d+),"title":"Minesweeper"') { $mineId = $Matches[1] }
Check "s1-minesweeper-launched" ($mineId -ne $null) ($listCtl.Substring(0, [Math]::Min(200, $listCtl.Length)))
if ($mineId -eq $null) { throw "no minesweeper window" }

# --- s2: bridge WS session ----------------------------------------------------
$c1 = New-Sock "127.0.0.1" 8899
$hs = WsHandshake $c1 "probetoken0123456789abcdef"
Check "s2-ws-handshake" ($hs -match "101")
WsSend $c1 '{"type":"hello"}'
$hello = $null
foreach ($i in 1..10) { $r = WsRecv $c1 3000; if ($r -match '"type":"hello"') { $hello = $r; break } }
Check "s2-hello-ok" ($hello -match '"ok"\s*:\s*(1|true)') "$hello"

# --- c3: list_windows dw/dh — STRICT JSON parse -------------------------------
Send-Tool $c1 "list_windows" "{}" "list"
$lr = $null
foreach ($i in 1..10) { $r = WsRecv $c1 3000; if ($r -match '"label":"list"') { $lr = $r; break } }
$lo = $null
try { $lo = ($lr | ConvertFrom-Json).json } catch { $lo = $null }
$mine = $null
if ($lo -ne $null) { foreach ($w in $lo.windows) { if ($w.id -eq [int]$mineId) { $mine = $w; break } } }
Check "c3-list-strict-json" ($lo -ne $null -and $mine -ne $null) "parse failed"
Check "c3-dw-dh-present" ($mine -ne $null -and ($null -ne ($mine | Get-Member -Name dw)) -and ($null -ne ($mine | Get-Member -Name dh))) "dw/dh missing"
Check "c3-dw-eq-w-1to1" ($mine -ne $null -and $mine.dw -eq $mine.w -and $mine.dh -eq $mine.h) ("dw={0} w={1}" -f $mine.dw, $mine.w)

# --- c4: window_frame maxw:960 — JPEG SOI + b64 charset -----------------------
Send-Tool $c1 "window_frame" ('{"id":' + $mineId + ',"maxw":960}') "f960"
$fr = $null
foreach ($i in 1..20) { $r = WsRecv $c1 6000; if ($r -match '"label":"f960"') { $fr = $r; break } }
$fo = $null
try { $fo = ($fr | ConvertFrom-Json).json } catch { $fo = $null }
Check "c4-frame-ok" ($fo -ne $null -and $fo.ok) ($fr.Substring(0, [Math]::Min(160, $fr.Length)))
Check "c4-w-le-960" ($fo -ne $null -and $fo.w -le 960) "w=$($fo.w)"
$b64 = ""
if ($fo -ne $null -and $fo.data) { $b64 = $fo.data }
Check "c4-b64-charset" ($b64 -match '^[A-Za-z0-9+/=]+$') "bad charset"
$soi = $false
if ($b64 -ne "") {
    try {
        $bytes = [Convert]::FromBase64String($b64)
        $soi = ($bytes[0] -eq 0xFF -and $bytes[1] -eq 0xD8 -and $bytes[2] -eq 0xFF)
    } catch { $soi = $false }
}
Check "c4-jpeg-soi" $soi "not a JPEG"

# --- c5: frame size budget -----------------------------------------------------
Check "c5-frame-under-950k" ($fr -ne $null -and $fr.Length -lt 950000) "len=$($fr.Length)"

# --- c6: no maxw -> original surface size --------------------------------------
Send-Tool $c1 "window_frame" ('{"id":' + $mineId + '}') "fraw"
$fr2 = $null
foreach ($i in 1..20) { $r = WsRecv $c1 6000; if ($r -match '"label":"fraw"') { $fr2 = $r; break } }
$fo2 = $null
try { $fo2 = ($fr2 | ConvertFrom-Json).json } catch { $fo2 = $null }
Check "c6-raw-w-eq-sw" ($fo2 -ne $null -and $fo2.ok -and $fo2.w -eq $fo2.sw) "w=$($fo2.w) sw=$($fo2.sw)"

# --- c7: maxw:320 -> downscaled -------------------------------------------------
Send-Tool $c1 "window_frame" ('{"id":' + $mineId + ',"maxw":320}') "f320"
$fr3 = $null
foreach ($i in 1..20) { $r = WsRecv $c1 6000; if ($r -match '"label":"f320"') { $fr3 = $r; break } }
$fo3 = $null
try { $fo3 = ($fr3 | ConvertFrom-Json).json } catch { $fo3 = $null }
$downscaled = ($fo3 -ne $null -and $fo3.ok -and ($fo3.w -le 320) -and ($fo3.w -eq 320 -or $fo3.sw -le 320))
Check "c7-maxw-320" $downscaled "w=$($fo3.w) sw=$($fo3.sw)"

# --- c8: bad ids ------------------------------------------------------------------
Send-Tool $c1 "window_frame" '{"id":99999}' "fbad"
$rbad = $null
foreach ($i in 1..10) { $r = WsRecv $c1 6000; if ($r -match '"label":"fbad"') { $rbad = $r; break } }
Check "c8-window-not-found" ($rbad -match 'window_not_found') ($rbad.Substring(0, [Math]::Min(160, $rbad.Length)))
Send-Tool $c1 "window_frame" '{"id":-1}' "fneg"
$rneg = $null
foreach ($i in 1..10) { $r = WsRecv $c1 6000; if ($r -match '"label":"fneg"') { $rneg = $r; break } }
Check "c8-id-neg-bad-request" ($rneg -match 'bad_request') ($rneg.Substring(0, [Math]::Min(160, $rneg.Length)))
# id=0 = desktop-wide view (docs/57 §14.15): composited full framebuffer as
# JPEG b64 + logical dw/dh. The probe taps it via send_input id=0 (c9b).
Send-Tool $c1 "window_frame" '{"id":0,"maxw":960}' "fdesk"
$rdesk = $null
foreach ($i in 1..15) { $r = WsRecv $c1 6000; if ($r -match '"label":"fdesk"') { $rdesk = $r; break } }
$fd = $null
try { $fd = ($rdesk | ConvertFrom-Json).json } catch { $fd = $null }
$deskOk = ($fd -ne $null -and $fd.ok -and $fd.desktop -eq 1 -and $fd.dw -gt 0 -and $fd.dh -gt 0 -and $fd.data.Length -gt 100)
Check "c8-desktop-frame" $deskOk "w=$($fd.w) dw=$($fd.dw) dh=$($fd.dh) len=$($fd.data.Length)"
$deskB64ok = ($fd -ne $null -and $fd.data -match '^[A-Za-z0-9+/=]+$' -and [Convert]::FromBase64String($fd.data.Substring(0,4))[0] -eq 0xFF)
Check "c8-desktop-jpeg-soi" $deskB64ok "head=$($fd.data.Substring(0,4))"

# --- c9: TAP MATH — server-verified landing -------------------------------------
# desktop point = list.x + fx*dw ; fx=0.5 -> x + dw/2. send_input must accept
# the click (ok:true). The conversion ((x - X())/ScaleX) lands on surface
# px fx*w — the load-bearing coordinate identity of the mirror.
$tapX = $mine.x + [int][Math]::Floor($mine.dw * 0.5)
$tapY = $mine.y + [int][Math]::Floor($mine.dh * 0.5)
Send-Tool $c1 "send_input" ('{"id":' + $mineId + ',"op":"click","x":' + $tapX + ',"y":' + $tapY + '}') "tap"
$tr = $null
foreach ($i in 1..10) { $r = WsRecv $c1 6000; if ($r -match '"label":"tap"') { $tr = $r; break } }
Check "c9-tap-click-ok" ($tr -match '"label":"tap"' -and ($tr -match '"ok\\?"\s*:\s*(true|1)' -or $tr -match '"sent\\?"\s*:\s*(true|1)')) ($tr.Substring(0, [Math]::Min(160, $tr.Length)))

# --- c9b: RIGHT CLICK — the flag button of minesweeper-class apps -------------
# button:3 (SDL: 1=left/2=middle/3=right) rides the click op as the MouseDown/
# MouseUp keyCode. The phone UI sends this from the long-press gesture.
Send-Tool $c1 "send_input" ('{"id":' + $mineId + ',"op":"click","button":3,"x":' + $tapX + ',"y":' + $tapY + '}') "rtap"
$rtr = $null
foreach ($i in 1..10) { $r = WsRecv $c1 6000; if ($r -match '"label":"rtap"') { $rtr = $r; break } }
Check "c9b-right-click-ok" ($rtr -match '"label":"rtap"' -and ($rtr -match '"ok\\?"\s*:\s*(true|1)' -or $rtr -match '"sent\\?"\s*:\s*(true|1)')) ($rtr.Substring(0, [Math]::Min(160, $rtr.Length)))

# --- c9c: DESKTOP-WIDE TAP — id=0 hit-test dispatch (docs/57 §14.15) -----------
# The phone's desktop view taps the LOGICAL DESKTOP point of a window center
# with id=0; the server hit-tests and forwards to the topmost client. The
# minesweeper window center is occupied by its own layer -> must be accepted.
Send-Tool $c1 "send_input" ('{"id":0,"op":"click","x":' + $tapX + ',"y":' + $tapY + '}') "dtap"
$dtr = $null
foreach ($i in 1..10) { $r = WsRecv $c1 6000; if ($r -match '"label":"dtap"') { $dtr = $r; break } }
Check "c9c-desktop-tap-ok" ($dtr -match '"label":"dtap"' -and ($dtr -match '"ok\\?"\s*:\s*(true|1)' -or $dtr -match '"sent\\?"\s*:\s*(true|1)')) ($dtr.Substring(0, [Math]::Min(160, $dtr.Length)))
# desktop tap on empty space -> window_not_found (no layer under the point)
Send-Tool $c1 "send_input" '{"id":0,"op":"click","x":2,"y":2}' "dmiss"
$dmr = $null
foreach ($i in 1..10) { $r = WsRecv $c1 6000; if ($r -match '"label":"dmiss"') { $dmr = $r; break } }
$dmOk = $false
try { $dmOk = (($dmr | ConvertFrom-Json).json.error -eq 'window_not_found') } catch { $dmOk = $false }
Check "c9c-desktop-tap-miss" $dmOk ($dmr.Substring(0, [Math]::Min(160, $dmr.Length)))

# --- c10: capture_window untouched guard (WS relay frame — agentctl is
# unreachable once the bridge WS session holds the agent slot) ---------------------
Send-Tool $c1 "capture_window" ('{"id":' + $mineId + '}') "capw"
$capr = $null
foreach ($i in 1..10) { $r = WsRecv $c1 6000; if ($r -match '"label":"capw"') { $capr = $r; break } }
$capo = $null
try { $capo = ($capr | ConvertFrom-Json).json } catch { $capo = $null }
$capPath = if ($capo -ne $null -and $capo.path) { $capo.path } else { $null }
Check "c10-capture-ok" ($capo -ne $null -and $capo.ok -and $capPath -ne $null -and (Test-Path $capPath)) "path=$capPath"

# --- c11: permissions flip window_frame->ask = hard capture_ask -------------------
'{"send_input":"allow","window_frame":"ask"}' | Set-Content -Path $permFile -Encoding ASCII
Start-Sleep -Milliseconds 300
Send-Tool $c1 "window_frame" ('{"id":' + $mineId + ',"maxw":960}') "fask"
$fr3b = $null
foreach ($i in 1..20) { $r = WsRecv $c1 6000; if ($r -match '"label":"fask"') { $fr3b = $r; break } }
Check "c11-ask-capture_ask" ($fr3b -match 'capture_ask') ($fr3b.Substring(0, [Math]::Min(160, $fr3b.Length)))

# --- c12: rate limit LAST (403-grade gate poisons later checks) -------------------
$rateHit = $false
for ($i = 0; $i -lt 11; $i++) {
    try {
        $bad = New-Sock "127.0.0.1" 8899
        $hsb = WsHandshake $bad "wrongtoken0000000000000000000000"
        if ($hsb -match "403") { $rateHit = $true }
        $bad.Close()
    } catch { }
    if ($rateHit) { break }
}
Check "c12-rate-limit-403" $rateHit "no 403 in 11 bad tokens"

} finally {
    Get-Process jkdesktop, jkwinserver, jkbridge, jkagentd -ErrorAction SilentlyContinue |
        Stop-Process -Force -ErrorAction SilentlyContinue
    if ($hadPerm) { [System.IO.File]::WriteAllBytes($permFile, $permBak) }
    elseif (Test-Path $permFile) { Remove-Item $permFile -Force -ErrorAction SilentlyContinue }
    if ($hadChat) { [System.IO.File]::WriteAllBytes($chatFile, $chatBak) }
    if ($hadJb) { [System.IO.File]::WriteAllBytes($jbFile, $jbBak) }
    Write-Output "NOTICE: probe state restored"
}
if ($script:fail -eq 0) { Write-Output "RESULT: ALL PASS" }
else { Write-Output ("RESULT: " + $script:fail + " FAIL"); exit 1 }