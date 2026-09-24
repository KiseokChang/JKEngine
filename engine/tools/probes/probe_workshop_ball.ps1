# probe_workshop_ball.ps1 - the phone eye-check replay (2026-09-24 09:23
# defect): the user asked the phone LLM "공튀기는 앱 만들어줘" and the LLM
# could not launch the workshop. Root causes fixed: (1) the launch_app schema
# listed workshop among built-in app names (it is a .jkx package) so
# {"app":"workshop"} died on the new pre-spawn verification, and (2) the LLM
# had no guidance that "make me an app" = the workshop. This probe drives the
# REAL LLM over the bridge WS (the phone path, same engine) and verifies the
# agent itself picks the workshop, installs an animated canvas script, and
# the window animates.
#
# LIVE harness (probe_llm_conquest pattern): attaches to the user's live
# stack (jkwinserver + jkbridge, REAL token - never printed), never touches
# permissions.json (full-allow runtime file, probe-owned since 2026-09-19).
# myapp.js IS touched by set_script (user runtime file): backup at start,
# restore in finally, NOTICE both ways.
#   s1 bridge WS connected (token handshake 101)
#   t1 LLM turn "공 튀기는 앱을 만들어 줘." completes (chat_done ok:1)
#   v1 a Workshop window exists afterwards (the LLM chose the vehicle)
#   v2 receipts.jsonl gained a launch_app record naming workshop
#   v3 receipts.jsonl gained an app_tool set_script record
#   v4 the workshop window animates (capture hash differs 1.5s apart)
#   c1 cleanup: workshop window closed; myapp.js truth source restored
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe = "$build\jkdesktop.exe"
$state = "$build\state"
$myapp = "$state\scripts\myapp.js"

$script:fail = 0
function Check([string]$name, [bool]$cond, [string]$detail) {
    if ($cond) { Write-Output "PASS $name" }
    else { Write-Output "FAIL $name  ($detail)"; $script:fail++ }
}
function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    $out = (& $exe agentctl $escaped 2>$null) -join "`n"
    $idx = $out.IndexOf('{')
    if ($idx -lt 0) { return "" }
    return $out.Substring($idx)
}
# --- receipts delta reader ----------------------------------------------------
$receipts = "$state\receipts.jsonl"
function Receipts-Size { if (Test-Path $receipts) { return (Get-Item $receipts).Length } return 0 }
function Receipts-Delta([long]$from) {
    if (-not (Test-Path $receipts)) { return "" }
    $fs = [System.IO.File]::Open($receipts, 'Open', 'Read', 'ReadWrite')
    try {
        if ($fs.Length -le $from) { return "" }
        [void]$fs.Seek($from, 'Begin')
        $r = New-Object System.IO.StreamReader($fs)
        return $r.ReadToEnd()
    } finally { $fs.Close() }
}
function Capture-Hash([int]$wid) {
    $r = Invoke-Agentctl ('{"tool":"capture_window","args":{"id":' + $wid + '}}')
    $pm = [regex]::Match($r, '"path\\?":\\?"([^"]*)"')
    if (-not $pm.Success) { return "" }
    $p = $pm.Groups[1].Value -replace '\\\\', '\'
    if (-not (Test-Path $p)) { return "" }
    return (Get-FileHash $p -Algorithm SHA256).Hash
}
# --- bridge WS (probe_llm_conquest primitives, copied verbatim) ---------------
$jb = (Get-Content (Join-Path $state "jkbridge.json") -Raw | ConvertFrom-Json)
$token = $jb.token
$port = $jb.port
function WsConnect([string]$tok, [int]$portNo) {
    # A refused connection must surface as a NULL, not a thrown exception —
    # an exception would skip the s1 check entirely and print a spurious
    # ALL PASS (2026-09-24 실측: dead bridge → connect throw → ALL PASS).
    try { $c = New-Object System.Net.Sockets.TcpClient("127.0.0.1", $portNo) }
    catch { Write-Output "DIAG connect failed: $_"; return $null }
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

# --- truth source backup (user runtime file rule) ------------------------------
$permBak = "$myapp.probe_ball_bak"
$hadMyapp = (Test-Path $myapp)
if ($hadMyapp) {
    Copy-Item $myapp $permBak -Force
    Write-Output "NOTICE: backed up myapp.js -> probe_ball_bak"
}
$receiptMark = Receipts-Size
$c = $null
try {
    # ---- s1: connect ------------------------------------------------------------
    $c = WsConnect $token $port
    Check "s1-bridge-ws" ($c -ne $null) "token handshake"

    # ---- t1: the user's exact ask ------------------------------------------------
    WsSend $c '{"type":"chat","text":"공 튀기는 앱을 만들어 줘."}'
    $done = $null
    $deadline = [System.Diagnostics.Stopwatch]::StartNew()
    $result = ""
    while ($deadline.ElapsedMilliseconds -lt 300000) {
        $f = WsRecv $c 500
        if ($f -match '"type":"chat_done"') { $done = $f; break }
    }
    $ok = ($done -ne $null -and $done -match '"ok":1')
    if ($done -match '"result":"((?:[^"\\]|\\.)*)"') { $result = $Matches[1] -replace '\\n', ' ' -replace '\\"', '"' }
    Check "t1-llm-done" $ok ($result.Substring(0, [Math]::Min(150, $result.Length)))

    # ---- v1-v4: verify what the agent did ----------------------------------------
    Start-Sleep -Seconds 2
    $lw = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    $wid = 0
    $m = [regex]::Match($lw, '\{"id":(\d+)[^}]*"title":"Workshop"')
    if ($m.Success) { $wid = [int]$m.Groups[1].Value }
    Check "v1-workshop-window" ($wid -gt 0) ("list_windows titles: " +
        (([regex]::Matches($lw, '"title":"[^"]*"') | ForEach-Object { $_.Value }) -join ','))

    $delta = Receipts-Delta $receiptMark
    Check "v2-launch-receipt-workshop" ($delta -match 'launch_app' -and $delta -match 'workshop') "receipts delta"
    Check "v3-setscript-receipt" ($delta -match '"tool":"set_script"' -or $delta -match 'set_script') "receipts delta"

    if ($wid -gt 0) {
        # Script variants differ per run (LLM nondeterminism), and most ball
        # scripts SETTLE — damping/friction make the canvas static seconds
        # after boot (runs 4/9), while click-to-spawn variants stay empty
        # until a click (run 1). v4 is therefore variant-agnostic:
        #   1. round-trip the script verbatim via get_script/set_script
        #      (SyncReload restarts the animation clock; same source, so
        #      myapp.js content is unchanged);
        #   2. sample 5 hashes 1s apart — ANY change = animates;
        #   3. if all static, click the canvas (window rect + canvas offset,
        #      send_input = allow-gated tool) and sample 3 more.
        $gs = Invoke-Agentctl '{"tool":"app_tool","args":{"app":"workshop","tool":"get_script","args":{}}}'
        $srcEsc = [regex]::Match($gs, '"source":"((?:[^"\\]|\\.)*)"').Groups[1].Value
        $hs = @()
        if ($srcEsc -ne "") {
            # Restart = rewrite the SAME source to myapp.js (the truth source,
            # watch=1 hot-reloads it). This avoids the agentctl argv escaping
            # layer that mangled a nested set_script round-trip into
            # bad_request (run 10 DIAG). Identical bytes — content unchanged;
            # the fresh mtime restarts the animation clock.
            $sMark = [string][char]1 + "SL" + [char]1
            $srcRaw = $srcEsc -replace '\\\\', $sMark -replace '\\n', "`n" -replace '\\"', '"' -replace $sMark, '\'
            [IO.File]::WriteAllText($myapp, $srcRaw, (New-Object System.Text.UTF8Encoding($false)))
            Start-Sleep -Milliseconds 2500  # watch poll + two-phase reload
            for ($i = 0; $i -lt 5; $i++) {
                $hs += (Capture-Hash $wid)
                Start-Sleep -Milliseconds 1000
            }
            if (@($hs | Select-Object -Unique).Count -lt 2) {
                $wm = [regex]::Match($lw, '\{"id":(\d+),"title":"Workshop","pid":\d+,"x":(-?\d+),"y":(-?\d+)')
                if ($wm.Success) {
                    $cx = [int]$wm.Groups[2].Value + 210
                    $cy = [int]$wm.Groups[3].Value + 110
                    [void](Invoke-Agentctl ('{"tool":"send_input","args":{"id":' + $wid +
                        ',"op":"click","x":' + $cx + ',"y":' + $cy + '}}'))
                    for ($i = 0; $i -lt 3; $i++) {
                        $hs += (Capture-Hash $wid)
                        Start-Sleep -Milliseconds 1000
                    }
                }
            }
        }
        $uniq = @($hs | Where-Object { $_ -ne "" } | Select-Object -Unique)
        Check "v4-ball-animates" ($uniq.Count -ge 2) ("distinct hashes=$($uniq.Count) of $($hs.Count) script=" +
            ($srcEsc -replace '\\n', ' '))
    } else {
        Check "v4-ball-animates" $false "no workshop window to capture"
    }
} finally {
    # ---- c1: cleanup ----------------------------------------------------------------
    $lw = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    $m = [regex]::Match($lw, '\{"id":(\d+)[^}]*"title":"Workshop"')
    if ($m.Success) {
        [void](Invoke-Agentctl ('{"tool":"close_window","args":{"id":' + $m.Groups[1].Value + '}}'))
        Write-Output "NOTICE: workshop window closed"
    }
    if ($c) { $c.Close() }
    if (Test-Path $permBak) {
        Copy-Item $permBak $myapp -Force
        Remove-Item $permBak -Force
        Write-Output "NOTICE: myapp.js restored from backup"
    } else {
        Write-Output "NOTICE: no myapp.js backup existed at start"
    }
}

if ($script:fail -eq 0) { Write-Output "RESULT: ALL PASS" }
else { Write-Output ("RESULT: " + $script:fail + " FAIL"); exit 1 }