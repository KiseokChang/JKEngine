# probe_semantic_cursor.ps1 - semantic cursor OFFICIAL probe (spec
# 2026-09-22-semantic-cursor §9). ASCII-only (PS5.1). Self-contained: absorbs
# the semc1-semc4 ad-hoc coverage the brief pins (the ad-hoc files stay for
# their extra negative/fail-closed rounds). Harness = semc4_adhoc.ps1 (raw
# pipe idiom, PS5.1 argv trap workaround, launch/poll flow of
# probe_conquest_minesweeper).
#
# Scenarios (task-5 brief):
#   1  declaration: launch_app minesweeper -> tools/list exposes
#      minesweeper.move/read/act with the kinds enum the LLM sees
#   2  move: absolute echo / relative clamp / steps (first-boundary stop +
#      reached echo) / negative deltas legal / oversize delta = bad_args
#      (pre-move echo, nothing applied - reject-before-apply, NOT multi-step
#      atomicity: partial application within a multi-step run is intentional,
#      JKSemanticCursor.cpp:65) / missing args = bad_args / out-of-grid
#      absolute = bad_grid (requested-cell echo)
#   3  read serialization: cursor header + rows/cols + 9x9 board (9 lines of
#      [#F?0-9*]) + status field, fresh board all '#'
#   4  act park -> banner name "<app>.<kind> at (r,c)" -> approve -> opened
#      echo + board change; parked event carries the bridge surface fields
#      (kind=app_tool + name + request id + target identity); cursor stays
#      after act (spec §4 - act does not move the cursor)
#   5  bad_state: reveal an opened cell / flag an opened cell
#   5b flag serialization 'F' in the snapshot + flags count
#   5c question-mark cycle: question -> read '?' -> clear -> read '#'
#   6  boom: reveal loop until a mine -> echo status lost + snapshot exposes
#      the mines ('*') + board still 9x9 legal glyphs
#   7  reset -> approve -> read -> cursor (0,0) + status playing + all '#'
#      (reset = definition transition, platform cursor reset)
#   8  bad_args: act with missing row / kind outside the declared enum
#      (server pre-app rejection - the app pipe must see no tool call)
#
# Run: powershell -File probe_semantic_cursor.ps1 "> log 2>&1" (file redirect
# - lesson 42). Kills the live desktop stack and starts a probe-owned server;
# leaves it STOPPED at teardown (NOTICE) - restart jkwinserver.exe +
# jkbridge.exe to resume the live desktop. permissions.json is probe-owned
# state: stale-residue guard + backup + NOTICE + byte-identical finally
# restore (docs/59 §16.1 incident class).
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root = Split-Path $exe
$script:fail = 0
function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host ("PASS: " + $name) }
    else { $script:fail++; Write-Host ("FAIL: " + $name + " -- " + $detail) }
    if ($detail) { Write-Host ("      " + $detail) }
}

# --- agentctl (raw command line - PS5.1 argv re-parsing trap) -----------------
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

# --- raw named pipe client (semc idiom) ----------------------------------------
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
    # Control-only agent pipe (the minesweeper client registers itself via
    # launch_app - the probe only queries and reads events).
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
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class PipePeekSC {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool PeekNamedPipe(IntPtr hPipe, byte[] buf,
        int bufSize, out int read, out int avail, out int left);
}
"@
function Pipe-Avail([System.IO.Pipes.NamedPipeClientStream]$s) {
    $read = 0; $avail = 0; $left = 0
    try {
        $h = $s.SafePipeHandle.DangerousGetHandle()
        if (-not [PipePeekSC]::PeekNamedPipe($h, $null, 0, [ref]$read, [ref]$avail, [ref]$left)) { return -1 }
    } catch { return -1 }
    return $avail
}
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
    elseif ($type -eq 18) { $head0 = [BitConverter]::ToUInt32($pl, 0) }   # AgentReplyHeader.queryId
    $text = ""
    if ($len -gt $hs) { $text = [Text.Encoding]::UTF8.GetString($pl, $hs, $len - $hs) }
    return @{ type = $type; len = $len; text = $text; head0 = [uint32]$head0 }
}
# Wait for one agent event frame matching a needle (skips other frames).
function Wait-Event([System.IO.Pipes.NamedPipeClientStream]$agent,
                    [string]$needle, [int]$ms) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $ms) {
        $f = Read-Frame $agent 200
        if ($f -ne $null -and $f.type -eq 20 -and $f.text -match $needle) {
            return $f
        }
    }
    return $null
}
# Send an app_tool query and return the final AgentReply (type 18 ONLY, and
# ONLY the frame whose AgentReplyHeader.queryId matches the sent qid - a
# delayed reply from a previous query must not be consumed by the next reader
# (false-PASS direction flake). The agent pipe subscribes to events, a
# single-frame read is a flake not a product fault; lesson 30). Parked acts
# resolve after the approve call, so the caller keeps draining until the
# matching reply lands.
function Read-Reply([System.IO.Pipes.NamedPipeClientStream]$agent, [uint32]$qid,
                    [int]$timeoutMs) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $timeoutMs) {
        $f = Read-Frame $agent 200
        if ($f -ne $null -and $f.type -eq 18 -and $f.head0 -eq $qid) { return $f.text }
    }
    return $null
}
function Send-AppTool([System.IO.Pipes.NamedPipeClientStream]$agent, [uint32]$qid,
                      [string]$app, [string]$tool, [string]$argsJson) {
    SendQuery $agent $qid ('{"tool":"app_tool","args":{"app":"' + $app +
        '","tool":"' + $tool + '","args":' + $argsJson + '}}')
}
function Get-ReqId([object]$ev) {
    if ($ev -ne $null -and $ev.text -match '"request":(\d+)') { return [int]$Matches[1] }
    return 0
}
# Read the minesweeper board: returns the parsed snapshot hashtable or $null.
function Read-Board([System.IO.Pipes.NamedPipeClientStream]$agent, [uint32]$qid) {
    Send-AppTool $agent $qid "minesweeper" "read" "{}"
    $rd = Read-Reply $agent $qid 12000
    if ($rd -eq $null) { return $null }
    try {
        $obj = $rd | ConvertFrom-Json
        if ($obj.ok -ne $true -or $obj.snapshot -eq $null) { return $null }
        return @{ raw = $rd; obj = $obj; snap = $obj.snapshot }
    } catch { return $null }
}
function Count-Chars([string]$s, [char]$c) {
    return ($s.ToCharArray() | Where-Object { $_ -eq $c }).Count
}
# Park one act (kind/row/col) and return the approval_request event frame.
function Park-Act([System.IO.Pipes.NamedPipeClientStream]$agent, [uint32]$qid,
                  [string]$kind, [int]$row, [int]$col) {
    Send-AppTool $agent $qid "minesweeper" "act" (
        '{"kind":"' + $kind + '","row":' + $row + ',"col":' + $col + '}')
    return (Wait-Event $agent '"topic":"agent\.approval_request"' 10000)
}

# --- permissions.json: probe-owned state (stale guard + backup + restore) ------
$permFile = Join-Path $root "permissions.json"
$stale = Get-ChildItem (Join-Path $env:TEMP "perm_pre_semcursor_*.json") -ErrorAction SilentlyContinue
if ($stale) {
    Write-Host ("FAIL: setup-stale-residue -- " + (($stale | ForEach-Object { $_.Name }) -join ", "))
    Write-Host "      stale residue from a killed run - restore engine/build/permissions.json manually, delete the leftover(s) in TEMP, re-run"
    exit 1
}
$permBakFile = Join-Path $env:TEMP ("perm_pre_semcursor_" + $PID + ".json")
$hadPerm = Test-Path $permFile
if ($hadPerm) { Copy-Item $permFile $permBakFile -Force }
Write-Host "NOTICE: backing up user runtime permissions.json -> $permBakFile (restored byte-identical in finally)"
function Restore-Perms {
    if ($hadPerm) { Copy-Item $permBakFile $permFile -Force }
    else { Remove-Item $permFile -ErrorAction SilentlyContinue }
}

# --- server lifecycle ------------------------------------------------------------
try {
Get-Process jkdesktop, jkwinserver, jkbridge, jkagentd -ErrorAction SilentlyContinue |
    Stop-Process -Force
Start-Sleep -Seconds 1
# Default permission environment: no file = cursor act parks (declared gate
# ask), move/read are none-allow. The live user file has no app_tool keys
# today, but removing it pins the contract against future live-file drift.
Remove-Item $permFile -ErrorAction SilentlyContinue
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
$up = $false
foreach ($i in 1..30) {
    Start-Sleep -Milliseconds 500
    if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up = $true; break }
}
Check "sc-up" $up "ping"
Write-Output "NOTICE: the user's live desktop server was stopped for the probe run - restart jkwinserver.exe (and jkbridge.exe) afterwards"

$agent = New-Pipe 1
$script:qid = [uint32]900

# --- 1: declaration -> catalog ----------------------------------------------------
Invoke-Agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}' | Out-Null
$win = $null
$winJson = ""
for ($i = 0; $i -lt 20; $i++) {
    Start-Sleep -Milliseconds 500
    $lw = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    $winJson = $lw
    if ($lw -match '"title":"Minesweeper"') { $win = $true; break }
}
Check "sc-launch-window" ($win -ne $null) $winJson

$cat = ""
$catOk = $false
for ($i = 0; $i -lt 20; $i++) {
    $cat = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
    if ($cat -match '"app":"minesweeper","name":"move"') { $catOk = $true; break }
    Start-Sleep -Milliseconds 500
}
Check "sc-catalog-move" ($cat -match '"app":"minesweeper","name":"move"') $cat
Check "sc-catalog-read" ($cat -match '"app":"minesweeper","name":"read"') ""
Check "sc-catalog-act" ($cat -match '"app":"minesweeper","name":"act"') ""
Check "sc-catalog-snapshot" ($cat -match '"app":"minesweeper","name":"snapshot"') ""
# The declared kinds drive the enum the LLM sees (server merge, spec §2).
# MINOR-2 (fix round 1): read the enum from PARSED catalog data, not a regex
# on raw text (docs/59 §12 lesson) - the synthesized act row's inputSchema is
# embedded raw JSON, so the parsed object exposes properties.kind.enum.
$enumParsed = ""
$enumOk = $false
try {
    $catObj = $cat | ConvertFrom-Json
    $actRow = $catObj.tools | Where-Object { $_.app -eq "minesweeper" -and $_.name -eq "act" } |
        Select-Object -First 1
    if ($actRow -ne $null -and $actRow.inputSchema -ne $null -and
        $actRow.inputSchema.properties -ne $null -and $actRow.inputSchema.properties.kind -ne $null) {
        $enumParsed = ($actRow.inputSchema.properties.kind.enum -join ",")
        $enumOk = ($enumParsed -eq "reveal,flag,question,clear,chord,reset")
    }
} catch { $enumParsed = "parse: " + $_.Exception.Message }
Check "sc-catalog-act-enum" $enumOk ("parsed enum=" + $enumParsed + " | " + $cat)

# --- 2: platform move --------------------------------------------------------------
function Move-Tool([string]$argsJson) {
    $script:qid++
    Send-AppTool $agent $script:qid "minesweeper" "move" $argsJson
    return (Read-Reply $agent $script:qid 8000)
}
# Declaration upsert resets the cursor to (0,0) - absolute move first.
# MINOR-2 (fix round 1): the echo contract is asserted from PARSED reply data
# (ok/row/col), not only a raw-text regex (docs/59 §12 lesson).
$script:qid++
Send-AppTool $agent $script:qid "minesweeper" "move" '{"to_row":4,"to_col":4}'
$mv0 = Read-Reply $agent $script:qid 8000
$absOk = $false
$absDetail = ""
if ($mv0 -ne $null) {
    try {
        $o = $mv0 | ConvertFrom-Json
        $absOk = ($o.ok -eq $true -and $o.row -eq 4 -and $o.col -eq 4)
    } catch { $absDetail = "parse: " + $_.Exception.Message }
} else { $absDetail = "no reply" }
Check "sc-move-init-abs" $absOk ("reply=" + $mv0 + " " + $absDetail)
# Relative: negative delta is legal movement and clamps at the boundary.
$r = Move-Tool '{"dr":0,"dc":-100}'
Check "sc-move-rel-clamp" ($r -match '"ok":true' -and $r -match '"row":4' -and $r -match '"col":0') $r
# Multi-step break semantics (MAJOR-1 fix round 1): the boundary-reaching step
# must come FIRST or break-at-first-boundary and break-at-end are
# indistinguishable. From (4,0): [{"dr":50},{"dc":3}] - dr+50 would pass the
# row-8 boundary, so break-at-first-boundary stops there = (8,0) (dc+3 NOT
# applied); break-at-end would land (8,3). Assert (8,0).
$r = Move-Tool '{"steps":[{"dr":50},{"dc":3}]}'
Check "sc-move-steps-boundary" ($r -match '"ok":true' -and $r -match '"row":8' -and $r -match '"col":0') $r
# Negative step deltas are legal movement (dr:-5 = up) - from (8,0) -> (3,0).
$r = Move-Tool '{"steps":[{"dr":-5}]}'
Check "sc-move-steps-negative" ($r -match '"ok":true' -and $r -match '"row":3' -and $r -match '"col":0') $r
# Only absurd magnitudes pre-validate (|delta| > 1<<20) -> bad_args with the
# pre-move echo, nothing applied. Single-step payload: this pins
# reject-before-apply for the pre-validated case, NOT multi-step atomicity -
# partial application WITHIN a multi-step run is intentional (the run stops at
# the first boundary and echoes the reached cell, JKSemanticCursor.cpp:65).
$r = Move-Tool '{"steps":[{"dr":-1048577}]}'
Check "sc-move-oversize-badargs" ($r -match '"ok":false' -and $r -match '"error":"bad_args"' -and $r -match '"row":3' -and $r -match '"col":0') $r
$r = Move-Tool '{}'
Check "sc-move-noargs-badargs" ($r -match '"ok":false' -and $r -match '"error":"bad_args"' -and $r -match '"row":3' -and $r -match '"col":0') $r
$r = Move-Tool '{"to_row":9,"to_col":0}'
Check "sc-move-outgrid-badgrid" ($r -match '"ok":false' -and $r -match '"error":"bad_grid"' -and $r -match '"row":3' -and $r -match '"col":0') $r

# --- 3: read serialization ----------------------------------------------------------
$script:qid++
$b = Read-Board $agent $script:qid
$readOk = ($b -ne $null -and $b.snap.status -eq "playing" -and
           $b.obj.rows -eq 9 -and $b.obj.cols -eq 9 -and $b.snap.lines.Count -eq 9)
Check "sc-read-ok" $readOk $(if ($b) { $b.raw } else { "no reply" })
Check "sc-read-cursor-header" ($readOk -and $b.raw -match '"cursor":\{"row":3,"col":0\}') ""
$glyphOk = $readOk
foreach ($l in $b.snap.lines) {
    if ($l.Length -ne 9 -or -not ($l -match '^[#F?0-9*]{9}$')) { $glyphOk = $false }
}
Check "sc-read-board-9x9" $glyphOk $(if ($b) { ($b.snap.lines -join "/") } else { "" })
Check "sc-read-fresh-board" ($readOk -and (Count-Chars $b.snap.board '#') -eq 81) ("closed=" + $(if ($b) { (Count-Chars $b.snap.board '#') } else { -1 }))
Check "sc-read-status-field" ($readOk -and $b.raw -match '"status":"playing"') ""

# --- 4: act park -> banner -> approve -> opened echo + board change ------------------
function Approve-Act([string]$kind, [int]$row, [int]$col) {
    $script:qid++
    $ev = Park-Act $agent $script:qid $kind $row $col
    # docs/64 §8: 위치 무의미한 kind(reset)는 배너에 좌표 표기가 없다.
    if ($kind -eq "reset") {
        $okName = ($ev -ne $null -and $ev.text -match '"name":"minesweeper\.reset"')
    } else {
        $okName = ($ev -ne $null -and $ev.text -match ('"name":"minesweeper\.' + $kind + ' at \(' + $row + ',' + $col + '\)"'))
    }
    Check ("sc-park-" + $kind + "-" + $row + "-" + $col) $okName $(if ($ev) { $ev.text } else { "no approval_request event" })
    # Phone-bridge surface: the parked event must carry the fields the bridge
    # relays to the phone (kind + banner name + request id + target identity).
    $reqId = Get-ReqId $ev
    $bridgeOk = ($ev -ne $null -and $ev.text -match '"kind":"app_tool"' -and
                 $reqId -gt 0 -and
                 $ev.text -match '"target":\{"app":"minesweeper","tool":"act"')
    Check "sc-park-bridge-fields" $bridgeOk $(if ($ev) { $ev.text } else { "no event" })
    if ($reqId -le 0) { return $null }
    $ap = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
    Check ("sc-approve-" + $kind + "-" + $row + "-" + $col) ($ap -match '"approved":true') $ap
    return (Read-Reply $agent $script:qid 12000)
}
$act = Approve-Act "reveal" 4 4
Check "sc-reveal-opened-echo" ($act -ne $null -and $act -match '"kind":"reveal"' -and
                               $act -match '"opened":([1-9][0-9]*)' -and
                               $act -match '"status":"playing"') $act
$script:qid++
$b2 = Read-Board $agent $script:qid
$closed1 = $(if ($b2) { (Count-Chars $b2.snap.board '#') } else { 81 })
Check "sc-reveal-board-changed" ($b2 -ne $null -and $closed1 -lt 81) ("closed=" + $closed1)
# Act does not move the cursor (spec §4 - the cursor state survives act).
Check "sc-cursor-after-act" ($b2 -ne $null -and $b2.raw -match '"cursor":\{"row":3,"col":0\}') $(if ($b2) { $b2.raw } else { "no reply" })

# --- 5: bad_state (invalid transitions echo) -----------------------------------------
$act2 = Approve-Act "flag" 4 4
Check "sc-badstate-flag-opened" ($act2 -ne $null -and $act2 -match '"error":"bad_state"' -and
                                 $act2 -match '"kind":"flag"' -and $act2 -match '"row":4' -and
                                 $act2 -match '"col":4' -and $act2 -match '"status":"playing"') $act2
$act2b = Approve-Act "reveal" 4 4
Check "sc-badstate-reopen" ($act2b -ne $null -and $act2b -match '"error":"bad_state"' -and
                            $act2b -match '"kind":"reveal"' -and $act2b -match '"row":4') $act2b

# --- 5b: flag serialization 'F' -------------------------------------------------------
$fr = -1; $fc = -1
if ($b2 -ne $null) {
    for ($r = 0; $r -lt 9; $r++) {
        $idx = $b2.snap.lines[$r].IndexOf('#')
        if ($idx -ge 0) { $fr = $r; $fc = $idx; break }
    }
}
Check "sc-flag-target-found" ($fr -ge 0) ("b2=" + $(if ($b2) { $b2.raw } else { "none" }))
$act3 = Approve-Act "flag" $fr $fc
Check "sc-flag-echo" ($act3 -ne $null -and $act3 -match '"kind":"flag"' -and $act3 -match '"opened":1' -and
                      $act3 -match '"status":"playing"') $act3
$script:qid++
$b3 = Read-Board $agent $script:qid
$flagOk = $false
if ($b3 -ne $null) {
    $flagOk = ($b3.snap.status -eq "playing" -and $b3.snap.flags -eq 1 -and
               $fr -ge 0 -and $b3.snap.lines[$fr][$fc] -eq 'F')
}
Check "sc-read-flag-cell" $flagOk $(if ($b3) { $b3.raw } else { "no reply" })

# --- 5c: question-mark cycle (question -> '?' -> clear -> '#') -------------------------
$qr = -1; $qc = -1
if ($b3 -ne $null) {
    for ($r = 0; $r -lt 9; $r++) {
        $idx = $b3.snap.lines[$r].IndexOf('#')
        if ($idx -ge 0 -and -not ($r -eq $fr -and $idx -eq $fc)) { $qr = $r; $qc = $idx; break }
    }
}
Check "sc-question-target-found" ($qr -ge 0) ("b3=" + $(if ($b3) { $b3.raw } else { "none" }))
$null = Approve-Act "question" $qr $qc
$script:qid++
$b3q = Read-Board $agent $script:qid
$qOk = $false
if ($b3q -ne $null) {
    $qOk = ($b3q.snap.status -eq "playing" -and $qr -ge 0 -and $b3q.snap.lines[$qr][$qc] -eq '?')
}
Check "sc-read-question-cell" $qOk $(if ($b3q) { $b3q.raw } else { "no reply" })
$null = Approve-Act "clear" $qr $qc
$script:qid++
$b3c = Read-Board $agent $script:qid
$cOk = $false
if ($b3c -ne $null) {
    $cOk = ($b3c.snap.status -eq "playing" -and $qr -ge 0 -and $b3c.snap.lines[$qr][$qc] -eq '#')
}
Check "sc-read-clear-cell" $cOk $(if ($b3c) { $b3c.raw } else { "no reply" })

# --- 6: boom - reveal loop until a mine -> lost + mines exposed ------------------------
$boom = $false
$won = $false
$boomEcho = ""
$lastBoard = $null
for ($i = 0; $i -lt 45; $i++) {
    $script:qid++
    $bl = Read-Board $agent $script:qid
    if ($bl -eq $null) { break }
    $lastBoard = $bl
    if ($bl.snap.status -eq "lost") { $boom = $true; break }
    if ($bl.snap.status -eq "won") { $won = $true; break }
    $br = -1; $bc = -1
    for ($r = 0; $r -lt 9; $r++) {
        $idx = $bl.snap.lines[$r].IndexOf('#')
        if ($idx -ge 0) { $br = $r; $bc = $idx; break }
    }
    if ($br -lt 0) { break }
    $script:qid++
    $ev = Park-Act $agent $script:qid "reveal" $br $bc
    $reqId = Get-ReqId $ev
    if ($reqId -le 0) { break }
    $null = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
    $rep = Read-Reply $agent $script:qid 12000
    if ($rep -ne $null) { $boomEcho = $rep }
    if ($rep -ne $null -and $rep -match '"status":"lost"') { $boom = $true; break }
    if ($rep -ne $null -and $rep -match '"status":"won"') { $won = $true; break }
}
Check "sc-boom-lost" ($boom -and -not $won) ("echo=" + $boomEcho)
$boomBoard = $null
$script:qid++
$bb = Read-Board $agent $script:qid
if ($bb -ne $null) { $boomBoard = $bb }
$minesOk = $false
$boardOk = $false
if ($boomBoard -ne $null) {
    $minesOk = ($boomBoard.snap.status -eq "lost" -and (Count-Chars $boomBoard.snap.board '*') -ge 9)
    $boardOk = ($boomBoard.snap.lines.Count -eq 9)
    foreach ($l in $boomBoard.snap.lines) {
        if ($l.Length -ne 9 -or -not ($l -match '^[#F?0-9*]{9}$')) { $boardOk = $false }
    }
}
Check "sc-boom-mines-exposed" $minesOk ("stars=" + $(if ($boomBoard) { (Count-Chars $boomBoard.snap.board '*') } else { -1 }) + " raw=" + $(if ($boomBoard) { $boomBoard.raw } else { "none" }))
Check "sc-boom-board-9x9" $boardOk ""

# --- 7: reset -> approve -> read -> cursor (0,0) + playing -----------------------------
$act4 = Approve-Act "reset" 0 0
Check "sc-reset-echo" ($act4 -ne $null -and $act4 -match '"kind":"reset"' -and $act4 -match '"status":"playing"') $act4
$script:qid++
$b4 = Read-Board $agent $script:qid
$resetOk = $false
if ($b4 -ne $null) {
    $allClosed = ($b4.snap.lines.Count -eq 9)
    foreach ($l in $b4.snap.lines) { if (-not ($l -match '^[#]{9}$')) { $allClosed = $false } }
    $resetOk = ($b4.snap.status -eq "playing" -and $allClosed -and
                $b4.raw -match '"cursor":\{"row":0,"col":0\}')
}
Check "sc-reset-cursor-origin" $resetOk $(if ($b4) { $b4.raw } else { "no reply" })

# --- 8: bad_args (server pre-app rejection) --------------------------------------------
$script:qid++
Send-AppTool $agent $script:qid "minesweeper" "act" '{"kind":"flag","col":5}'
$missRow = Read-Reply $agent $script:qid 5000
Check "sc-act-missing-row" ($missRow -ne $null -and $missRow -match '"ok":false' -and
                            $missRow -match '"error":"bad_args"') $missRow
$script:qid++
Send-AppTool $agent $script:qid "minesweeper" "act" '{"kind":"detonate","row":1,"col":1}'
$badKind = Read-Reply $agent $script:qid 4000
Check "sc-act-badkind" ($badKind -ne $null -and $badKind -match '"ok":false' -and
                        $badKind -match '"error":"bad_args"') $badKind
# The rejections above are server-side: a follow-up valid read must still
# return (the app pipe was never reached and nothing parked).
$script:qid++
$b5 = Read-Board $agent $script:qid
Check "sc-after-badargs-alive" ($b5 -ne $null -and $b5.snap.status -eq "playing") $(if ($b5) { $b5.raw } else { "no reply" })

# --- teardown ----------------------------------------------------------------------------
$agent.Dispose()
Get-Process jkdesktop, jkwinserver, jkbridge, jkagentd -ErrorAction SilentlyContinue |
    Stop-Process -Force
Write-Output "NOTICE: the desktop server is left stopped - restart jkwinserver.exe (and jkbridge.exe) to resume the live desktop"
Write-Host ("RESULT: " + ($(if ($script:fail -eq 0) { "ALL PASS" } else { "FAIL " + $script:fail })))
exit ($script:fail)
}
finally {
    Restore-Perms
    Remove-Item $permBakFile -ErrorAction SilentlyContinue
}
