# semc1_adhoc.ps1 - semantic cursor Task 1 ad-hoc smoke (spec
# 2026-09-22-semantic-cursor). ASCII-only (PS5.1). Fake app declares a cursor
# grid over a raw pipe; checks declaration parsing, synthesized catalog rows
# (act kinds enum), platform move (absolute/relative/steps/bad_grid/bad_args +
# server-side step magnitude pre-validation, negative deltas legal and
# clamped), read assembly (snapshot compose + snapshot failure + tool_timeout),
# fail-closed registration rejections (cursor_owner_unsupported /
# cursor_name_conflict / bad_cursor / cursor_window_required for control-only
# declares), own-act kind-enum merge (MINOR-1 - STRICT: exactly one "kind" key,
# ConvertFrom-Json parsed, covering replace-in-place / empty-properties /
# description-containing-"properties" schemas), no-snapshot immediate
# unknown_app_tool (NIT-7), empty-snapshot unknown (NIT-5), and explicit
# permissions.json deny on move (MINOR-4) with backup + finally-restore
# (probe_app_tools idiom). Fake apps connect as window clients (Hello +
# CreateSurface) since fix round 1 rejects cursor declarations from
# control-only connections. Helper functions are copied from
# probe_app_tools.ps1 (raw pipe idiom, PS5.1 argv trap workaround).
# Task 2 update: the parked act approval_request name is the banner form
# "<app>.<kind> at (r,c)" (semc2_adhoc.ps1 covers the full act relay set).
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
function New-Pipe([int]$subscriber, [bool]$window) {
    $p = New-Object System.IO.Pipes.NamedPipeClientStream(".", "JKWindowServerPipe",
        [System.IO.Pipes.PipeDirection]::InOut)
    $p.Connect(5000)
    $hello = New-Object byte[] 8
    [BitConverter]::GetBytes([uint32]2).CopyTo($hello, 0)
    [BitConverter]::GetBytes([uint32]$PID).CopyTo($hello, 4)
    SendMsg $p 1 $hello
    if ($window) {
        # Window client (Hello + CreateSurface) - the server requires a surface
        # owner for a cursor declaration (NIT-8 rejects control-only declares).
        # SurfaceCreatePayload: {int32 w, int32 h, char title[128]}.
        $w = 64; $h = 64
        $pl = New-Object byte[] (8 + 128)
        [BitConverter]::GetBytes([int32]$w).CopyTo($pl, 0)
        [BitConverter]::GetBytes([int32]$h).CopyTo($pl, 4)
        $title = [Text.Encoding]::ASCII.GetBytes("fakegrid")
        [Array]::Copy($title, 0, $pl, 8, $title.Length)
        SendMsg $p 3 $pl
        # Drain the SurfaceCreated (type 4) ack - payload is shm info we ignore.
        $created = Read-Frame $p 5000
        if ($created -eq $null -or $created.type -ne 4) {
            Write-Host "FAIL: newpipe-surfacecreated"
            $script:fail++
        }
        if ($subscriber -ne 0) {
            $sub = New-Object byte[] 4
            [BitConverter]::GetBytes([uint32]$subscriber).CopyTo($sub, 0)
            SendMsg $p 19 $sub
        }
        return $p
    }
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

# --- permissions.json (MINOR-4 deny check): backup + finally-restore ---------
# User runtime state (docs/59 §16.1 incident class): back up with a stale-
# residue guard first, apply the deny key for the c-den section, restore in a
# try/finally no matter how the probe ends.
$permFile = Join-Path $root "permissions.json"
$stale = Get-ChildItem (Join-Path $env:TEMP "perm_pre_semc1_*.json") -ErrorAction SilentlyContinue
if ($stale) {
    Write-Host ("FAIL: setup-stale-residue -- " + (($stale | ForEach-Object { $_.Name }) -join ", "))
    Write-Host "      stale residue from a killed run - restore engine/build/permissions.json manually, delete the leftover(s) in TEMP, re-run"
    exit 1
}
$permBakFile = Join-Path $env:TEMP ("perm_pre_semc1_" + $PID + ".json")
$hadPerm = Test-Path $permFile
if ($hadPerm) { Copy-Item $permFile $permBakFile -Force }
function Set-Perms([string]$json) {
    [IO.File]::WriteAllText($permFile, $json, (New-Object System.Text.UTF8Encoding($false)))
}
function Restore-Perms {
    if ($hadPerm) { Copy-Item $permBakFile $permFile -Force }
    else { Remove-Item $permFile -ErrorAction SilentlyContinue }
}

# --- server lifecycle --------------------------------------------------------
try {
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
$up = $false
foreach ($i in 1..30) {
    Start-Sleep -Milliseconds 500
    if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up = $true; break }
}
Check "sm-up" $up "ping"

# --- register the fake cursor app (window client - NIT-8) --------------------
$app = New-Pipe 1 $true
Send-ToolRegister $app $cursorJson
$ack = Read-Frame $app 3000
Check "sm-register-ack" ($ack -ne $null -and $ack.text -match '"ok":true') $ack.text

# --- catalog: synthesized rows + act kinds enum ------------------------------
$agent = New-Pipe 1 $false
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

# --- own-act kind-enum merge (MINOR-1): app registers its own act tool -------
# Strict checks (fix round 2): the round-1 regex was a FALSE PASS - it matched
# the first (shadowed) "kind" occurrence when the merge injected a duplicate
# key. Here: parse the catalog with ConvertFrom-Json, take the app's act row,
# parse its inputSchema with ConvertFrom-Json, and assert exactly ONE "kind"
# key AND that occurrence carries the declared enum.
function Test-MergedAct([string]$app, [string]$kindsCsv, [string]$catalogText) {
    try {
        # Strict parse 1: the whole catalog must be valid JSON (a trailing
        # comma anywhere in the merge throws here).
        $cat = $catalogText | ConvertFrom-Json
        $row = $cat.tools | Where-Object { $_.app -eq $app -and $_.name -eq "act" } | Select-Object -First 1
        if ($row -eq $null) { return @{ ok = $false; why = "no act row"; detail = $catalogText } }
        # Exactly ONE "kind" key - counted on the RAW embedded schema text
        # (ConvertFrom-Json silently last-wins duplicate keys, so the count
        # must happen on the raw bytes; "required":["kind",...] has no colon
        # and is not counted).
        $m = [regex]::Match($catalogText,
            ('"app":"' + [regex]::Escape($app) + '","name":"act".*?"inputSchema":(\{.*?\}),"windowId"'))
        if (-not $m.Success) { return @{ ok = $false; why = "no raw slice"; detail = $catalogText } }
        $kindKeyCount = ([regex]::Matches($m.Groups[1].Value, '"kind"\s*:')).Count
        # Strict parse 2: the parsed schema object must carry the declared
        # enum on its (single) kind key.
        $enumCsv = ($row.inputSchema.properties.kind.enum -join ",")
        return @{ ok = ($kindKeyCount -eq 1 -and $enumCsv -eq $kindsCsv);
                  why = ("kindKeys=" + $kindKeyCount + " enum=" + $enumCsv);
                  detail = $m.Groups[1].Value }
    } catch {
        return @{ ok = $false; why = ("parse: " + $_.Exception.Message); detail = $catalogText }
    }
}

$appOwn = New-Pipe 1 $true
Send-ToolRegister $appOwn ('{"app":"fakegrid2","tools":[{"name":"act","description":"my act","inputSchema":{"type":"object","properties":{"kind":{"type":"string"},"row":{"type":"integer"}},"required":["kind","row"]}},{"name":"snapshot","description":"x","inputSchema":{"type":"object","properties":{}}}],' +
    '"cursor":{"type":"cell-grid","coordSpace":"client","origin":{"x":0,"y":0},"cellW":16,"cellH":16,"rows":4,"cols":4,"cursorOwner":"platform","act":{"kinds":["reveal","flag"],"gate":"ask"}}}')
$ackOwn = Read-Frame $appOwn 3000
Check "sm-ownact-register" ($ackOwn -ne $null -and $ackOwn.text -match '"ok":true') $ackOwn.text
# kind already declared -> replace-in-place path (3a): exactly ONE kind key,
# its object carries the declared enum, other schema members survive.
$script:qid++
SendQuery $agent $script:qid '{"tool":"list_app_tools","args":{}}'
$cat2 = Read-Reply $agent $null $script:qid 5000 ""
$t2 = Test-MergedAct "fakegrid2" "reveal,flag" $cat2
Check "sm-ownact-enum-merged" $t2.ok ($t2.why + " | " + $t2.detail)

# 3b-i: own act schema with EMPTY properties ({} -> no trailing comma) and no
# kind key - the merge must yield strict-parseable JSON (round-1 emitted a
# trailing comma, CLI JSON.parse failure class).
$appOwn3 = New-Pipe 1 $true
Send-ToolRegister $appOwn3 ('{"app":"fakegrid3","tools":[{"name":"act","description":"empty props","inputSchema":{"type":"object","properties":{}}}],' +
    '"cursor":{"type":"cell-grid","coordSpace":"client","origin":{"x":0,"y":0},"cellW":16,"cellH":16,"rows":4,"cols":4,"cursorOwner":"platform","act":{"kinds":["reveal"],"gate":"ask"}}}')
$ackOwn3 = Read-Frame $appOwn3 3000
Check "sm-ownact3-register" ($ackOwn3 -ne $null -and $ackOwn3.text -match '"ok":true') $ackOwn3.text
$script:qid++
SendQuery $agent $script:qid '{"tool":"list_app_tools","args":{}}'
$cat3 = Read-Reply $agent $null $script:qid 5000 ""
$t3 = Test-MergedAct "fakegrid3" "reveal" $cat3
Check "sm-ownact3-emptyprops-strict" $t3.ok ($t3.why + " | " + $t3.detail)

# 3b-ii + NIT (string-aware properties locator): the act schema's description
# text contains the literal "properties" and a { brace - the round-1 find()
# entry point would have matched inside the string literal and injected into a
# wrong object. The locator must skip string literals.
$appOwn4 = New-Pipe 1 $true
Send-ToolRegister $appOwn4 ('{"app":"fakegrid4","tools":[{"name":"act","description":"tricky","inputSchema":{"type":"object","description":"mentions \"properties\" and { here","properties":{"row":{"type":"integer"}}}}],' +
    '"cursor":{"type":"cell-grid","coordSpace":"client","origin":{"x":0,"y":0},"cellW":16,"cellH":16,"rows":4,"cols":4,"cursorOwner":"platform","act":{"kinds":["reveal","question"],"gate":"ask"}}}')
$ackOwn4 = Read-Frame $appOwn4 3000
Check "sm-ownact4-register" ($ackOwn4 -ne $null -and $ackOwn4.text -match '"ok":true') $ackOwn4.text
$script:qid++
SendQuery $agent $script:qid '{"tool":"list_app_tools","args":{}}'
$cat4 = Read-Reply $agent $null $script:qid 5000 ""
$t4 = Test-MergedAct "fakegrid4" "reveal,question" $cat4
Check "sm-ownact4-strlitprops" $t4.ok ($t4.why + " | " + $t4.detail)

# --- move: absolute / relative / steps / bad_grid / bad_args -----------------
function Move-Tool([string]$argsJson) {
    $script:qid++
    SendQuery $agent $script:qid ('{"tool":"app_tool","args":{"app":"fakegrid","tool":"move","args":' + $argsJson + '}}')
    return (Read-Reply $agent $null $script:qid 5000 "")
}
$r = Move-Tool '{"to_row":8,"to_col":8}'
Check "sm-move-abs" ($r -match '"ok":true' -and $r -match '"row":8' -and $r -match '"col":8') $r
$r = Move-Tool '{"to_row":9,"to_col":0}'
# MINOR-2: bad_grid echoes the pre-move position (8,8).
Check "sm-move-badgrid" ($r -match '"ok":false' -and $r -match 'bad_grid' -and $r -match '"row":8' -and $r -match '"col":8') $r
$r = Move-Tool '{"to_row":-1,"to_col":0}'
Check "sm-move-badargs-neg" ($r -match '"ok":false' -and $r -match 'bad_args' -and $r -match '"row":8' -and $r -match '"col":8') $r
$r = Move-Tool '{"dr":0,"dc":-100}'
Check "sm-move-rel-clamp" ($r -match '"ok":true' -and $r -match '"row":8' -and $r -match '"col":0') $r
$r = Move-Tool '{"steps":[{"dr":50},{"dc":3}]}'
# Fix round 2 (task-5 review): the boundary-reaching step must come FIRST or
# break-at-first-boundary and break-at-end are indistinguishable. From (8,0):
# dr 50 would pass the row boundary (8) and the run stops there (first
# boundary) = (8,0); break-at-end would land (8,3). Echo = reached cell (8,0).
Check "sm-move-steps-boundary" ($r -match '"ok":true' -and $r -match '"row":8' -and $r -match '"col":0') $r
# Fix round 2 ruling: negative step deltas are LEGAL movement (dr:-1 = up).
$r = Move-Tool '{"steps":[{"dr":-5}]}'
Check "sm-move-steps-neg-up" ($r -match '"ok":true' -and $r -match '"row":3' -and $r -match '"col":0') $r
# Down/right first so the dc clamp still has room to bite (5,3) -> clamp (5,0).
$r = Move-Tool '{"steps":[{"dr":2},{"dc":3},{"dc":-10}]}'
Check "sm-move-steps-neg-clamp" ($r -match '"ok":true' -and $r -match '"row":5' -and $r -match '"col":0') $r
# Only absurd magnitudes are pre-validated (|delta| > 1<<20) -> bad_args with
# the pre-move echo (5,0), nothing applied (reject-before-apply for the
# pre-validated case - NOT multi-step atomicity, see probe_semantic_cursor).
$r = Move-Tool '{"steps":[{"dr":-1048577}]}'
Check "sm-move-steps-oversize" ($r -match '"ok":false' -and $r -match 'bad_args' -and $r -match '"row":5' -and $r -match '"col":0') $r
$r = Move-Tool '{"steps":[{"dc":1}]}'
Check "sm-move-steps-shortstep" ($r -match '"ok":true' -and $r -match '"row":5' -and $r -match '"col":1') $r
$r = Move-Tool '{"steps":[]}'
Check "sm-move-steps-empty" ($r -match '"ok":false' -and $r -match 'bad_args' -and $r -match '"row":5' -and $r -match '"col":1') $r
$r = Move-Tool '{}'
Check "sm-move-noargs" ($r -match '"ok":false' -and $r -match 'bad_args' -and $r -match '"row":5' -and $r -match '"col":1') $r

# --- read: snapshot compose --------------------------------------------------
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"fakegrid","tool":"read","args":{}}}'
$rd = Read-Reply $agent $app $script:qid 8000 '{"ok":true,"board":["1","*","3"],"status":"playing"}'
Check "sm-read-compose" ($rd -match '"ok":true' -and $rd -match '"cursor":\{"row":5,"col":1\}' -and $rd -match '"rows":9' -and $rd -match '"cols":9' -and $rd -match '"snapshot":\{"ok":true,"board"') $rd

# --- NIT-5: app answers ok=1 with an EMPTY result -> snapshot "unknown" -------
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"fakegrid","tool":"read","args":{}}}'
$rdEmpty = Read-Reply $agent $app $script:qid 8000 ''
Check "sm-read-empty-unknown" ($rdEmpty -match '"ok":true' -and $rdEmpty -match '"snapshot":"unknown"' -and $rdEmpty -match '"cursor":\{"row":5,"col":1\}') $rdEmpty

# --- read failure: app answers ok=0 -> cursor + explicit error ---------------
$script:appFailOnce = $true
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"fakegrid","tool":"read","args":{}}}'
$rdFail = Read-Reply $agent $app $script:qid 8000 '{"ok":true,"board":["1"]}'
Check "sm-read-fail-compose" ($rdFail -match '"ok":false' -and $rdFail -match '"error":"snapshot_failed"' -and $rdFail -match '"detail":\{"error":"board_gone"\}' -and $rdFail -match '"cursor":\{"row":5,"col":1\}') $rdFail

# --- read timeout: silent cursor app -> composed tool_timeout ----------------
$appSilent = New-Pipe 1 $true
Send-ToolRegister $appSilent '{"app":"fakegridt","tools":[{"name":"snapshot","description":"x","inputSchema":{"type":"object","properties":{}}}],"cursor":{"type":"cell-grid","coordSpace":"client","origin":{"x":0,"y":0},"cellW":16,"cellH":16,"rows":4,"cols":4,"cursorOwner":"platform","act":{"kinds":["reveal"],"gate":"ask"}}}'
$ackSilent = Read-Frame $appSilent 3000
Check "sm-silent-register" ($ackSilent -ne $null -and $ackSilent.text -match '"ok":true') $ackSilent.text
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"fakegridt","tool":"read","args":{}}}'
# silent app: $null pump - nothing answers the snapshot relay -> 10s expiry
# scan -> ReplyAppToolError composed with the cursor header.
$rdTo = Read-Reply $agent $null $script:qid 15000 '{"ok":true}'
Check "sm-read-timeout-compose" ($rdTo -match '"ok":false' -and $rdTo -match '"error":"tool_timeout"' -and $rdTo -match '"rows":4' -and $rdTo -match '"cursor"') $rdTo

# --- NIT-7: cursor app with NO snapshot tool -> immediate unknown_app_tool ---
$appNs = New-Pipe 1 $true
Send-ToolRegister $appNs '{"app":"fakegridns","tools":[{"name":"ping","description":"x","inputSchema":{"type":"object","properties":{}}}],"cursor":{"type":"cell-grid","coordSpace":"client","origin":{"x":0,"y":0},"cellW":16,"cellH":16,"rows":4,"cols":4,"cursorOwner":"platform","act":{"kinds":["reveal"],"gate":"ask"}}}'
$ackNs = Read-Frame $appNs 3000
Check "sm-ns-register" ($ackNs -ne $null -and $ackNs.text -match '"ok":true') $ackNs.text
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"fakegridns","tool":"read","args":{}}}'
# No pump: a 10s relay would time out long after this short read - only an
# immediate unknown_app_tool reply can satisfy this check.
$rdNs = Read-Reply $agent $null $script:qid 4000 '{"ok":true}'
Check "sm-ns-read-unknown" ($rdNs -match '"ok":false' -and $rdNs -match 'unknown_app_tool') $rdNs
$appNs.Dispose()
$appSilent.Dispose()

# --- registration rejections (fail-closed) -----------------------------------
# Window clients - a cursor declaration from a control-only connection is
# rejected outright (NIT-8, checked separately below), so the parse-error
# rejections must come from a surface owner to reach the cursor parser.
function Try-Register([string]$json) {
    $p = New-Pipe 1 $true
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
# NIT-8: control-only connection declaring a cursor -> rejected at declaration
# time (fail-closed, ack(false) style).
$pCtl = New-Pipe 1 $false
Send-ToolRegister $pCtl '{"app":"fakegridctl","tools":[{"name":"t","description":"x"}],"cursor":{"type":"cell-grid","coordSpace":"client","origin":{"x":0,"y":0},"cellW":16,"cellH":16,"rows":4,"cols":4,"cursorOwner":"platform","act":{"kinds":["reveal"],"gate":"ask"}}}'
$rejCtl = Read-Frame $pCtl 3000
$pCtl.Dispose()
Check "sm-reject-controlonly" ($rejCtl -ne $null -and $rejCtl.text -match '"ok":false' -and $rejCtl.text -match 'cursor_window_required') $rejCtl.text

# --- act: ask parking (no permissions.json key = ask default) -----------------
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"fakegrid","tool":"act","args":{"kind":"flag","row":3,"col":5}}}'
# drain events until the approval_request lands (app_tools_changed from the
# silent-app registration can arrive first). Task 2: the parked act carries
# the banner name "<app>.<kind> at (r,c)" (spec 2026-09-22-semantic-cursor
# §3/§8) - the pre-Task-2 name was "fakegrid.act".
$ev = $null
$evSw = [System.Diagnostics.Stopwatch]::StartNew()
while ($evSw.ElapsedMilliseconds -lt 10000) {
    $f = Read-Frame $agent 300
    if ($f -ne $null -and $f.type -eq 20 -and
        $f.text -match '"topic":"agent.approval_request"' -and
        $f.text -match '"name":"fakegrid.flag at \(3,5\)"') { $ev = $f; break }
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

# --- MINOR-4: explicit permissions.json deny on move -> denied ----------------
# The gate file is read per call, so swapping it while the server is live is
# enough. Deny key at tier 1 (app_tool.<app>.<tool>).
Set-Perms '{"app_tool.fakegrid.move":"deny"}'
$r = Move-Tool '{"to_row":2,"to_col":2}'
Check "sm-deny-move" ($r -match '"ok":false' -and $r -match '"error":"denied"') $r
# read is also gated - deny both with the tier-3 key for fakegrid.
Set-Perms '{"app_tool.fakegrid":"deny"}'
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"fakegrid","tool":"read","args":{}}}'
$rdDeny = Read-Reply $agent $null $script:qid 4000 '{"ok":true}'
Check "sm-deny-read" ($rdDeny -match '"ok":false' -and $rdDeny -match '"error":"denied"') $rdDeny
# Restore the pre-probe state: without a key the move is allowed again (no key
# = allow). Note: if the live user file had full-allow keys they come back via
# Restore-Perms in the finally block - this intermediate state only proves the
# gate reads the file, so write the minimal allow-absent baseline.
if ($hadPerm) { Copy-Item $permBakFile $permFile -Force }
else { Remove-Item $permFile -ErrorAction SilentlyContinue }
$r = Move-Tool '{"to_row":2,"to_col":2}'
Check "sm-deny-move-after-restore" ($r -match '"ok":true' -and $r -match '"row":2' -and $r -match '"col":2') $r

# --- cleanup ------------------------------------------------------------------
$app.Dispose()
$appOwn.Dispose()
$appOwn3.Dispose()
$appOwn4.Dispose()
$agent.Dispose()
Write-Host ("RESULT: " + ($(if ($script:fail -eq 0) { "ALL PASS" } else { "FAIL " + $script:fail })))
exit ($script:fail)
}
finally {
    Restore-Perms
    Remove-Item $permBakFile -ErrorAction SilentlyContinue
}