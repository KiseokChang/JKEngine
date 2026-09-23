# Conquest ladder LLM live run (docs/62 rungs, the "LLM 실전" half). A real
# LLM agent session — the phone chat lineage — is driven over the bridge WS
# with Korean instructions, and the LADDER's own evidence chain verifies what
# the agent did: server-side list_windows truth + events_list fired counters
# + receipts.jsonl tool-call records. This is the end the scripted probes
# were proxies for: an LLM that reads its tool surface and acts.
#
# LIVE harness — deliberately NOT a teardown probe: it attaches to the user's
# live stack (jkwinserver + jkbridge with the REAL token) because a real LLM
# turn is the thing under test. It never touches permissions.json (full-allow
# runtime file, probe-owned since 2026-09-19) and never prints the token.
# Launches are cleaned up at the end (via the LLM itself, agentctl backstop).
#
# Scenario (conquest shape per turn: instruct -> act -> verify):
#   A "테트리스를 켜 줘."            -> Tetris in list_windows + launch receipt
#   B "지금 열린 창을 전부 알려 줘." -> chat_done result names Tetris
#   C "테트리스 창을 닫아 줘."       -> Tetris gone + close receipt + destroyed
#   D multi-step one turn:           -> both windows present, >=2 launches
#     "지뢰찾기를 켜 줘. 켜진 걸
#      확인하고 테트리스도 같이
#      켜 줘."
#   E "지뢰찾기와 테트리스 창을
#      모두 닫아 줘."               -> both gone (agentctl backstop below)
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe = "$build\jkdesktop.exe"
$server = $exe
$state = "$build\state"

# --- read bridge config (NEVER printed) --------------------------------------
$jbFile = Join-Path $state "jkbridge.json"
$jb = (Get-Content $jbFile -Raw | ConvertFrom-Json)
$token = $jb.token
$port = $jb.port

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
# RFC 6455: client-to-server frames MUST be masked - the first harness sent
# bare frames and the bridge dropped the connection as a protocol error
# (probe_jkbridge's masked sender is the proven primitive; copied verbatim).
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
# ReadTimeout blocking read (probe_jkbridge): control frames consumed, pings
# answered with an (unmasked, lenient) pong - the bridge heartbeats ~4.8 s
# and force-closes after 45 s without pong, and LLM turns run minutes.
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
# --- one LLM turn: chat -> wait chat_done (streams recorded) -------------------
function Invoke-LlmTurn([System.Net.Sockets.TcpClient]$c, [string]$text, [string]$tag) {
    $script:turns += "[turn $tag] $text"
    WsSend $c ('{"type":"chat","text":"' + ($text -replace '"', '\"') + '"}')
    $done = $null
    $deadline = [System.Diagnostics.Stopwatch]::StartNew()
    while ($deadline.ElapsedMilliseconds -lt 240000) {
        $f = WsRecv $c 500
        if ($f -match '"type":"chat_done"') { $done = $f; break }
        if ($f -match '"type":"stream"') { $script:turns += "[stream $tag] " + $f.Substring(0, [Math]::Min(120, $f.Length)) }
    }
    $ok = ($done -ne $null -and $done -match '"ok":1')
    $result = ""
    if ($done -match '"result":"((?:[^"\\]|\\.)*)"') { $result = $Matches[1] -replace '\\n', ' ' -replace '\\"', '"' }
    Check "$tag-llm-done" $ok ($result.Substring(0, [Math]::Min(150, $result.Length)))
    $script:turns += "[done $tag] " + $result.Substring(0, [Math]::Min(300, $result.Length))
    return $result
}
# --- evidence ------------------------------------------------------------------
function List-Has([string]$title) {
    $r = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    return ($r -match ('"title":"' + $title + '"'))
}
function Receipts-HasTool([string]$tool, [string]$needle) {
    $lines = Get-Content (Join-Path $state "receipts.jsonl") -ErrorAction SilentlyContinue
    $hit = $lines | Where-Object { $_.Contains('"tool":"' + $tool + '"') } |
        Where-Object { $_.Contains($needle) }
    return ($null -ne $hit)
}
function Event-Fired([string]$topic) {
    $r = Invoke-Agentctl '{"tool":"events_list","args":{}}'
    $m = [regex]::Match($r, '"topic":"' + [regex]::Escape($topic) + '"[^}]*"fired":(\d+)')
    if ($m.Success) { return [int]$m.Groups[1].Value }
    return -1
}

# server must be up (live stack restored before this run)
$ping = Invoke-Agentctl '{"tool":"ping","args":{}}'
Check "setup-server-up" ($ping -match '"ok":true') $ping

$script:turns = @()
$c = WsConnect $token $port
WsSend $c '{"type":"hello","resume_session":""}'
$hello = WsRecv $c 8000
Check "setup-hello" ($hello -ne $null -and $hello -match '"type":"hello"') "$hello"

# receipts baseline (count grows = LLM activity record)
$recBefore = (Get-Content (Join-Path $state "receipts.jsonl") -ErrorAction SilentlyContinue).Count

# ---- A: launch via LLM ---------------------------------------------------------
[void](Invoke-LlmTurn $c "테트리스를 켜 줘." "a-launch")
Start-Sleep -Seconds 2
Check "a-tetris-window" (List-Has "Tetris") (Invoke-Agentctl '{"tool":"list_windows","args":{}}')
Check "a-launch-receipt" (Receipts-HasTool "launch_app" "tetris") "receipts"
Check "a-window-created-event" ((Event-Fired "window.created") -ge 1) "events_list"

# ---- B: observation relay --------------------------------------------------------
$doneB = Invoke-LlmTurn $c "지금 열린 창을 전부 알려 줘." "b-observe"
Start-Sleep -Seconds 1
Check "b-result-names-tetris" ($script:turns[-1] -match "테트리스|Tetris") ($script:turns[-1])
Check "b-list-receipt" (Receipts-HasTool "list_windows" "Tetris") "receipts"

# ---- C: close via LLM ------------------------------------------------------------
[void](Invoke-LlmTurn $c "테트리스 창을 닫아 줘." "c-close")
Start-Sleep -Seconds 2
Check "c-tetris-gone" (-not (List-Has "Tetris")) (Invoke-Agentctl '{"tool":"list_windows","args":{}}')
Check "c-close-receipt" (Receipts-HasTool "close_window" "") "receipts"
Check "c-window-destroyed-event" ((Event-Fired "window.destroyed") -ge 1) "events_list"

# ---- D: multi-step single turn (launch + verify + launch) ------------------------
[void](Invoke-LlmTurn $c "지뢰찾기를 켜 줘. 켜진 걸 확인하고 테트리스도 같이 켜 줘." "d-multistep")
Start-Sleep -Seconds 2
$bothOk = (List-Has "Minesweeper") -and (List-Has "Tetris")
Check "d-both-windows" $bothOk (Invoke-Agentctl '{"tool":"list_windows","args":{}}')
Check "d-two-launches-receipt" ((Receipts-HasTool "launch_app" "minesweeper") -and (Receipts-HasTool "launch_app" "tetris")) "receipts"

# ---- E: cleanup via LLM, agentctl backstop ---------------------------------------
[void](Invoke-LlmTurn $c "지뢰찾기와 테트리스 창을 모두 닫아 줘." "e-cleanup")
Start-Sleep -Seconds 2
$clean = (-not (List-Has "Tetris")) -and (-not (List-Has "Minesweeper"))
Check "e-clean-by-llm" $clean (Invoke-Agentctl '{"tool":"list_windows","args":{}}')
if (-not $clean) {
    # backstop: never leave probe-launched windows on the user's desktop
    $list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    foreach ($m in [regex]::Matches($list, '"id\\?":(\d+),"title\\?":"(?:Tetris|Minesweeper)"')) {
        [void](Invoke-Agentctl ('{"tool":"close_window","args":{"id":' + $m.Groups[1].Value + '}}'))
        Start-Sleep -Milliseconds 500
    }
    Write-Output "NOTICE: leftover windows closed via agentctl backstop"
}

$recAfter = (Get-Content (Join-Path $state "receipts.jsonl") -ErrorAction SilentlyContinue).Count
Check "receipts-grew-by-llm-turns" ($recAfter -gt $recBefore) "before=$recBefore after=$recAfter"
$script:turns | Out-File (Join-Path $env:TEMP "llm_conquest_transcript.txt") -Encoding UTF8
Write-Output "NOTICE: transcript saved to %TEMP%\llm_conquest_transcript.txt (not committed)"

if ($script:fail -gt 0) { Write-Output "FAILED: $($script:fail)"; exit 1 }
Write-Output "LLM CONQUEST PASS"
exit 0