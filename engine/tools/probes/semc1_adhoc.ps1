# semc1_adhoc.ps1 - semantic cursor Task 1 ad-hoc smoke (spec
# 2026-09-22-semantic-cursor). ASCII-only (PS5.1). Fake app declares a cursor
# grid over a raw pipe; checks declaration parsing, synthesized catalog rows
# (act kinds enum), platform move (absolute/relative/steps/bad_grid/bad_args),
# read assembly (snapshot compose + snapshot failure + tool_timeout), fail-
# closed registration rejections (cursor_owner_unsupported / cursor_name_conflict
# / bad_cursor), and act ask-parking -> cross approve -> relay to the app's own
# act tool. Helper functions are copied from probe_app_tools.ps1 (raw pipe
# idiom, PS5.1 argv trap workaround).
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root = Split-Path $exe
$script:fail = 0
function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host ("PASS: " + $name) }
    else { $script:fail++; Write-Host ("FAIL: " + $name + " -- " + $detail) }
    if ($detail) { Write-Host ("      " + $detail) }
}
function Invoke-Agentctl([string]$json) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = 'agentctl "' + ($json -replace '"', '\"') + '"'
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.CreateNoWindow = $true
    $p = [System.Diagnostics.Process]::Start($psi)
    $out = $p.StandardOutput.ReadToEnd()
    $p.WaitForExit()
    return ($out -replace '(?m)^\[theme\][^\r\n]*[\r\n]*', '')
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
function Send-ToolRegister([System.IO.Pipes.NamedPipeClientStream]$s, [string]$json) {
    $body = [Text.Encoding]::UTF8.GetBytes($json)
    $payload = New-Object byte[] (4 + $body.Length)
    [BitConverter]::GetBytes([uint32]$body.Length).CopyTo($payload, 0)
    [Array]::Copy($body, 0, $payload, 4, $body.Length)
    SendMsg $s 22 $payload
}
function Send-ToolResult([System.IO.Pipes.NamedPipeClientStream]$s, [uint32]$reqId, [int]$ok, [string]$json) {
    $body = [Text.Encoding]::UTF8.GetBytes($json)
    $payload = New-Object byte[] (12 + $body.Length)
    [BitConverter]::GetBytes([uint32]$reqId).CopyTo($payload, 0)
    [BitConverter]::GetBytes([uint32]$ok).CopyTo($payload, 4)
    [BitConverter]::GetBytes([uint32]$body.Length).CopyTo($payload, 8)
    [Array]::Copy($body, 0, $payload, 12, $body.Length)
    SendMsg $s 24 $payload
}
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class PipePeekS {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool PeekNamedPipe(IntPtr hPipe, byte[] buf,
        int bufSize, out int read, out int avail, out int left);
}
"@
function Pipe-Avail([System.IO.Pipes.NamedPipeClientStream]$s) {
    $read = 0; $avail = 0; $left = 0
    try {
        $h = $s.SafePipeHandle.DangerousGetHandle()
        if (-not [PipePeekS]::PeekNamedPipe($h, $null, 0, [ref]$read, [ref]$avail, [ref]$left)) { return -1 }
    } catch { return -1 }
    return $avail
}
# One function consumes the entire frame (12-byte header + payload) and slices
# the type-specific header off: 22/20 -> 4, 17/23 -> 8, others -> 12.
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
    $hs = 12
    $head0 = 0
    if ($type -eq 20 -or $type -eq 22) { $hs = 4 }
    elseif ($type -eq 17 -or $type -eq 23) { $hs = 8; $head0 = [BitConverter]::ToUInt32($pl, 0) }
    $text = ""
    if ($len -gt $hs) { $text = [Text.Encoding]::UTF8.GetString($pl, $hs, $len - $hs) }
    return @{ type = $type; len = $len; text = $text; head0 = [uint32]$head0 }
}
# Read an AgentReply, answering app tool calls on the fake app pipe on the way
# (single-threaded pump - the server blocks on the relay while the requester
# waits). The fake app answers every tool call with snapshotJson, except "act"
# which answers with the app-side no_act_impl error. $script:appFailOnce makes
# the NEXT tool call answer ok=0 (snapshot failure compose check).
$script:appFailOnce = $false
function Read-Reply([System.IO.Pipes.NamedPipeClientStream]$agent,
                    [System.IO.Pipes.NamedPipeClientStream]$app,
                    [uint32]$qid, [int]$timeoutMs, [string]$snapshotJson) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $timeoutMs) {
        $f = Read-Frame $agent 200
        if ($f -ne $null) {
            if ($f.type -eq 18) { return $f.text }   # AgentReply
            if ($f.type -eq 20) { continue }         # events - skip
        }
        if ($app -ne $null) {
            $g = Read-Frame $app 0
            if ($g -ne $null -and $g.type -eq 23) {
                # AgentToolCall: {reqId, jsonLen} - reqId is frame.head0.
                $tool = ""
                if ($g.text -match '"tool":"([^"]+)"') { $tool = $Matches[1] }
                $body = $snapshotJson
                $okFlag = 1
                if ($script:appFailOnce) {
                    $script:appFailOnce = $false
                    $okFlag = 0
                    $body = '{"error":"board_gone"}'
                }
                if ($tool -eq "act") { $body = '{"ok":false,"error":"no_act_impl"}' }
                Send-ToolResult $app $g.head0 $okFlag $body
            }
        }
    }
    return $null
}

$cursorJson = '{"app":"fakegrid","tools":[{"name":"snapshot","description":"board snapshot","inputSchema":{"type":"object","properties":{}}}],' +
    '"cursor":{"type":"cell-grid","coordSpace":"client","origin":{"x":12,"y":44},"cellW":16,"cellH":16,' +
    '"rows":9,"cols":9,"cursorOwner":"platform","act":{"kinds":["reveal","flag","question"],"gate":"ask"}}}'

# --- server lifecycle --------------------------------------------------------
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
$up = $false
foreach ($i in 1..30) {
    Start-Sleep -Milliseconds 500
    if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up = $true; break }
}
Check "sm-up" $up "ping"

# --- register the fake cursor app -------------------------------------------
$app = New-Pipe 1
Send-ToolRegister $app $cursorJson
$ack = Read-Frame $app 3000
Check "sm-register-ack" ($ack -ne $null -and $ack.text -match '"ok":true') $ack.text

# --- catalog: synthesized rows + act kinds enum ------------------------------
$agent = New-Pipe 1
$script:qid = 100
$script:qid++
SendQuery $agent $script:qid '{"tool":"list_app_tools","args":{}}'
$cat = Read-Reply $agent $null $script:qid 5000 ""
$hasMove = $cat -match '"app":"fakegrid","name":"move"'
$hasRead = $cat -match '"app":"fakegrid","name":"read"'
$hasAct = $cat -match '"app":"fakegrid","name":"act"'
$hasEnum = $cat -match '"kind":\{"type":"string","enum":\["reveal","flag","question"\]\}'
Check "sm-catalog-move" $hasMove $cat
Check "sm-catalog-read" $hasRead ""
Check "sm-catalog-act" $hasAct ""
Check "sm-catalog-enum" $hasEnum ""

# --- move: absolute / relative / steps / bad_grid / bad_args -----------------
function Move-Tool([string]$argsJson) {
    $script:qid++
    SendQuery $agent $script:qid ('{"tool":"app_tool","args":{"app":"fakegrid","tool":"move","args":' + $argsJson + '}}')
    return (Read-Reply $agent $null $script:qid 5000 "")
}
$r = Move-Tool '{"to_row":8,"to_col":8}'
Check "sm-move-abs" ($r -match '"ok":true' -and $r -match '"row":8' -and $r -match '"col":8') $r
$r = Move-Tool '{"to_row":9,"to_col":0}'
Check "sm-move-badgrid" ($r -match '"ok":false' -and $r -match 'bad_grid') $r
$r = Move-Tool '{"to_row":-1,"to_col":0}'
Check "sm-move-badargs-neg" ($r -match '"ok":false' -and $r -match 'bad_args') $r
$r = Move-Tool '{"dr":0,"dc":-100}'
Check "sm-move-rel-clamp" ($r -match '"ok":true' -and $r -match '"row":8' -and $r -match '"col":0') $r
$r = Move-Tool '{"steps":[{"dc":3},{"dr":5},{"dc":-50}]}'
# from (8,0): dc 3 -> (8,3); dr 5 would reach 13 - clamped to the boundary row
# 8 and the run stops there (first boundary). Echo = reached cell (8,3).
Check "sm-move-steps-boundary" ($r -match '"ok":true' -and $r -match '"row":8' -and $r -match '"col":3') $r
$r = Move-Tool '{"steps":[{"dc":1}]}'
Check "sm-move-steps-shortstep" ($r -match '"ok":true' -and $r -match '"row":8' -and $r -match '"col":4') $r
$r = Move-Tool '{"steps":[]}'
Check "sm-move-steps-empty" ($r -match '"ok":false' -and $r -match 'bad_args') $r
$r = Move-Tool '{}'
Check "sm-move-noargs" ($r -match '"ok":false' -and $r -match 'bad_args') $r

# --- read: snapshot compose --------------------------------------------------
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"fakegrid","tool":"read","args":{}}}'
$rd = Read-Reply $agent $app $script:qid 8000 '{"ok":true,"board":["1","*","3"],"status":"playing"}'
Check "sm-read-compose" ($rd -match '"ok":true' -and $rd -match '"cursor":\{"row":8,"col":4\}' -and $rd -match '"rows":9' -and $rd -match '"cols":9' -and $rd -match '"snapshot":\{"ok":true,"board"') $rd

# --- read failure: app answers ok=0 -> cursor + explicit error ---------------
$script:appFailOnce = $true
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"fakegrid","tool":"read","args":{}}}'
$rdFail = Read-Reply $agent $app $script:qid 8000 '{"ok":true,"board":["1"]}'
Check "sm-read-fail-compose" ($rdFail -match '"ok":false' -and $rdFail -match '"error":"snapshot_failed"' -and $rdFail -match '"detail":\{"error":"board_gone"\}' -and $rdFail -match '"cursor":\{"row":8,"col":4\}') $rdFail

# --- read timeout: silent cursor app -> composed tool_timeout ----------------
$appSilent = New-Pipe 1
Send-ToolRegister $appSilent '{"app":"fakegridt","tools":[{"name":"snapshot","description":"x","inputSchema":{"type":"object","properties":{}}}],"cursor":{"type":"cell-grid","coordSpace":"client","origin":{"x":0,"y":0},"cellW":16,"cellH":16,"rows":4,"cols":4,"cursorOwner":"platform","act":{"kinds":["reveal"],"gate":"ask"}}}'
$ackSilent = Read-Frame $appSilent 3000
Check "sm-silent-register" ($ackSilent -ne $null -and $ackSilent.text -match '"ok":true') $ackSilent.text
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"fakegridt","tool":"read","args":{}}}'
# silent app: $null pump - nothing answers the snapshot relay -> 10s expiry
# scan -> ReplyAppToolError composed with the cursor header.
$rdTo = Read-Reply $agent $null $script:qid 15000 '{"ok":true}'
Check "sm-read-timeout-compose" ($rdTo -match '"ok":false' -and $rdTo -match '"error":"tool_timeout"' -and $rdTo -match '"rows":4' -and $rdTo -match '"cursor"') $rdTo
$appSilent.Dispose()

# --- registration rejections (fail-closed) -----------------------------------
function Try-Register([string]$json) {
    $p = New-Pipe 1
    Send-ToolRegister $p $json
    $a = Read-Frame $p 3000
    $p.Dispose()
    return $a.text
}
$rej = Try-Register ('{"app":"fakegrid2","tools":[{"name":"t","description":"x"}],"cursor":{"type":"cell-grid","coordSpace":"client","origin":{"x":0,"y":0},"cellW":16,"cellH":16,"rows":4,"cols":4,"cursorOwner":"app","act":{"kinds":["reveal"],"gate":"ask"}}}')
Check "sm-reject-owner-app" ($rej -match '"ok":false' -and $rej -match 'cursor_owner_unsupported') $rej
$rej = Try-Register ('{"app":"fakegrid2","tools":[{"name":"move","description":"x"}],"cursor":{"type":"cell-grid","coordSpace":"client","origin":{"x":0,"y":0},"cellW":16,"cellH":16,"rows":4,"cols":4,"cursorOwner":"platform","act":{"kinds":["reveal"],"gate":"ask"}}}')
Check "sm-reject-name-clash" ($rej -match '"ok":false' -and $rej -match 'cursor_name_conflict') $rej
$rej = Try-Register ('{"app":"fakegrid2","tools":[{"name":"t","description":"x"}],"cursor":{"type":"hex-grid","coordSpace":"client","origin":{"x":0,"y":0},"cellW":16,"cellH":16,"rows":4,"cols":4,"cursorOwner":"platform","act":{"kinds":["reveal"],"gate":"ask"}}}')
Check "sm-reject-badtype" ($rej -match '"ok":false' -and $rej -match 'bad_cursor') $rej
$rej = Try-Register ('{"app":"fakegrid2","tools":[{"name":"t","description":"x"}],"cursor":{"type":"cell-grid","coordSpace":"screen","origin":{"x":0,"y":0},"cellW":16,"cellH":16,"rows":4,"cols":4,"cursorOwner":"platform","act":{"kinds":["reveal"],"gate":"ask"}}}')
Check "sm-reject-badspace" ($rej -match '"ok":false' -and $rej -match 'bad_cursor') $rej
$rej = Try-Register ('{"app":"fakegrid2","tools":[{"name":"t","description":"x"}],"cursor":{"type":"cell-grid","coordSpace":"client","origin":{"x":0,"y":0},"cellW":16,"cellH":16,"rows":4,"cols":4,"cursorOwner":"platform","act":{"kinds":["reveal"],"gate":"allow"}}}')
Check "sm-reject-badgate" ($rej -match '"ok":false' -and $rej -match 'bad_cursor') $rej

# --- act: ask parking (no permissions.json key = ask default) -----------------
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"fakegrid","tool":"act","args":{"kind":"flag","row":3,"col":5}}}'
# drain events until the approval_request lands (app_tools_changed from the
# silent-app registration can arrive first).
$ev = $null
$evSw = [System.Diagnostics.Stopwatch]::StartNew()
while ($evSw.ElapsedMilliseconds -lt 10000) {
    $f = Read-Frame $agent 300
    if ($f -ne $null -and $f.type -eq 20 -and
        $f.text -match '"topic":"agent.approval_request"' -and
        $f.text -match '"name":"fakegrid.act"') { $ev = $f; break }
}
$okPark = ($ev -ne $null -and $ev.text -match '"kind":"app_tool"')
$reqId = 0
if ($okPark -and $ev.text -match '"request":(\d+)') { $reqId = [int]$Matches[1] }
Check "sm-act-parking" ($okPark -and $reqId -gt 0) ($(if ($ev) { $ev.text } else { "no event" }))
$ap = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
Check "sm-act-approve-ack" ($ap -match '"approved":true') $ap
# approve relays act to the app's tool "act" - the fake app answers no_act_impl
# (no act tool registered) - the relay error surfaces verbatim.
$actReply = Read-Reply $agent $app $script:qid 8000 '{"ok":false,"error":"no_act_impl"}'
Check "sm-act-relayed" ($actReply -match 'no_act_impl') $actReply

# --- undeclared app unchanged: move/read on a non-cursor app ------------------
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"vplayer","tool":"move","args":{}}}'
$und = Read-Reply $agent $null $script:qid 5000 ""
Check "sm-undeclared-unknown" ($und -match 'unknown_app_tool') $und

# --- cleanup ------------------------------------------------------------------
$app.Dispose()
$agent.Dispose()
Write-Host ("RESULT: " + ($(if ($script:fail -eq 0) { "ALL PASS" } else { "FAIL " + $script:fail })))