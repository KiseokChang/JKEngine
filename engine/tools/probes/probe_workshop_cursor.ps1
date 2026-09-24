# probe_workshop_cursor.ps1 - script-app semantic cursor (docs/60 s5 backlog
# burn-down: workshop declareCursor). ASCII-only (PS5.1). Harness = the
# probe_semantic_cursor.ps1 official probe (raw pipe idiom, PS5.1 argv trap
# workaround, park/approve flow) with the workshop .jkx spawn of
# probe_workshop_canvas.ps1.
#
# Scenarios:
#   1  declaration: workshop script calls declareCursor (global code) ->
#      list_app_tools exposes workshop.move/read/act; the act kind enum is
#      the script's declared kinds (server InjectKindsEnum merge into the
#      app's own act schema)
#   2  platform move: absolute echo (server-owned, no approval, app untouched)
#   3  read: a script app declares no snapshot tool -> NIT-7 immediate
#      unknown_app_tool (no 10s tool_timeout hang) - v1 limit, documented
#   4  act: park -> approval_request banner "workshop.<kind> at (r,c)" ->
#      approve -> the relay reaches the script's global onAgentAct, whose
#      return JSON becomes the tool result (kind/row/col echo)
#   5  pre-app rejections: act kind outside the declared enum / missing col /
#      out-of-grid row on the REDECLARED grid (bad_grid) - the app pipe must
#      see none of these
#   6  re-declaration: set_script a second script with a different grid ->
#      move to the old grid's far corner fails bad_grid, inside the new one
#      succeeds (latest declaration wins, cursor reset)
#   c1 cleanup: workshop window closed; truth source + permissions restored
#
# Run: powershell -File probe_workshop_cursor.ps1 > log 2>&1 (file redirect -
# lesson 42). Kills the live desktop stack and starts a probe-owned server;
# leaves it STOPPED at teardown (NOTICE) - restart jkwinserver.exe +
# jkbridge.exe to resume the live desktop. permissions.json is probe-owned
# state (stale guard + backup + byte-identical finally restore, docs/59
# s16.1). myapp.js IS touched by set_script: backup + NOTICE + restore.
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root = Split-Path $exe
$jkx = Join-Path $root "apps\workshop.jkx"
$myapp = Join-Path $root "state\scripts\myapp.js"
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
# app_tool via agentctl (set_script only - the relay calls go through the raw
# pipe so act park/approve timing is observable).
function AppTool-Agentctl([string]$tool, [string]$argsJson) {
    $a = '{"tool":"app_tool","args":{"app":"workshop","tool":"' + $tool + '"'
    if ($argsJson) { $a += ',"args":' + $argsJson }
    $a += '}}'
    return (Invoke-Agentctl $a)
}
function SetScript([string]$js) {
    $esc = $js -replace '"', '\"'
    return (AppTool-Agentctl "set_script" ('{"source":"' + $esc + '"}'))
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
    # Control-only agent pipe (the probe only queries and reads events; the
    # workshop client registers its own window connection).
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
public class PipePeekWC {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool PeekNamedPipe(IntPtr hPipe, byte[] buf,
        int bufSize, out int read, out int avail, out int left);
}
"@
function Pipe-Avail([System.IO.Pipes.NamedPipeClientStream]$s) {
    $read = 0; $avail = 0; $left = 0
    try {
        $h = $s.SafePipeHandle.DangerousGetHandle()
        if (-not [PipePeekWC]::PeekNamedPipe($h, $null, 0, [ref]$read, [ref]$avail, [ref]$left)) { return -1 }
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

# --- permissions.json: probe-owned state (stale guard + backup + restore) ------
$permFile = Join-Path $root "permissions.json"
$stale = Get-ChildItem (Join-Path $env:TEMP "perm_pre_wscur_*.json") -ErrorAction SilentlyContinue
if ($stale) {
    Write-Host ("FAIL: setup-stale-residue -- " + (($stale | ForEach-Object { $_.Name }) -join ", "))
    Write-Host "      stale residue from a killed run - restore engine/build/permissions.json manually, delete the leftover(s) in TEMP, re-run"
    exit 1
}
$permBakFile = Join-Path $env:TEMP ("perm_pre_wscur_" + $PID + ".json")
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
Remove-Item $permFile -ErrorAction SilentlyContinue
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
$up = $false
foreach ($i in 1..30) {
    Start-Sleep -Milliseconds 500
    if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up = $true; break }
}
Check "wc-up" $up "ping"
Write-Output "NOTICE: the user's live desktop server was stopped for the probe run - restart jkwinserver.exe (and jkbridge.exe) afterwards"

# truth source backup: myapp.js is a user runtime file (docs/60 s2.1) -
# backup at start, restore in finally, NOTICE both ways.
$myappBak = "$myapp.probe_wscur_bak"
$hadMyapp = (Test-Path $myapp)
if ($hadMyapp) {
    Copy-Item $myapp $myappBak -Force
    Write-Host "NOTICE: backed up myapp.js -> probe_wscur_bak"
}

# --- 1: spawn workshop -> boot register (no cursor yet, 3 hub rows) ---------------
$bat = Join-Path $env:TEMP ("wscur_" + $PID + ".cmd")
("@echo off`r`ncd /d `"" + $root + "`"`r`n`"" + $exe + "`" --jkx `"" + $jkx + "`"`r`n") |
    Set-Content -Path $bat -Encoding ASCII
Start-Process -FilePath $bat -WindowStyle Hidden
$rows = 0
foreach ($i in 1..30) {
    Start-Sleep -Milliseconds 500
    $rows = [regex]::Matches((Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'), '"app":"workshop"').Count
    if ($rows -eq 3) { break }
}
Check "wc-spawn-3-rows" ($rows -eq 3) ("rows=$rows")
if ($rows -ne 3) { throw "workshop client did not come up" }
# The boot script declares nothing - move/read/act must be ABSENT until a
# declareCursor script is installed (fail-closed default = old behavior).
$cat0 = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
Check "wc-boot-no-cursor" ($cat0 -notmatch '"app":"workshop","name":"(move|read|act)"') $cat0

$agent = New-Pipe 1
$script:qid = [uint32]700

# --- 2: install the cursor script --------------------------------------------------
# declareCursor in GLOBAL code (before onCreate) + onAgentAct returning an
# OBJECT (the object path of the return contract: JSON.stringify becomes the
# tool result). No double quotes anywhere in the JS - the PS5.1 argv layer
# stays single-escape-clean (docs/55 lesson 3).
$src1 = "var cv = createCanvas({x:10,y:10,w:300,h:200});" +
        "declareCursor({origin:{x:10,y:10}, cellW:30, cellH:25, rows:8, cols:10," +
        " kinds:['paint']});" +
        "function onCreate() { canvasClear(cv,'#202030');" +
        "canvasRect(cv,0,0,300,200,'#404040',true); }" +
        "function onAgentAct(kind,row,col) { log('act '+kind+' '+row+','+col);" +
        "return {ok:true, kind:kind, row:row, col:col, painted:true}; }"
$r1 = (SetScript $src1)
Check "wc-script-set" ($r1 -match '"ok":true') $r1
# Registration is asynchronous (declareCursor fires during eval; the register
# rides the surface pipe) - poll the catalog until move shows up.
$cat = ""
$catOk = $false
for ($i = 0; $i -lt 20; $i++) {
    Start-Sleep -Milliseconds 500
    $cat = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
    if ($cat -match '"app":"workshop","name":"move"') { $catOk = $true; break }
}
Check "wc-catalog-move" ($cat -match '"app":"workshop","name":"move"') $cat
Check "wc-catalog-read" ($cat -match '"app":"workshop","name":"read"') ""
Check "wc-catalog-act" ($cat -match '"app":"workshop","name":"act"') ""
Check "wc-hub-rows-kept" ($cat -match '"name":"get_script"' -and
                          $cat -match '"name":"set_script"') ""
# Declared kinds drive the act enum (parsed, not regex - docs/59 s12 lesson).
$enumOk = $false
$enumParsed = ""
try {
    $catObj = $cat | ConvertFrom-Json
    $actRow = $catObj.tools | Where-Object { $_.app -eq "workshop" -and $_.name -eq "act" } |
        Select-Object -First 1
    if ($actRow -ne $null -and $actRow.inputSchema.properties.kind -ne $null) {
        $enumParsed = ($actRow.inputSchema.properties.kind.enum -join ",")
        $enumOk = ($enumParsed -eq "paint")
    }
} catch { $enumParsed = "parse: " + $_.Exception.Message }
Check "wc-catalog-act-enum" $enumOk ("parsed enum=" + $enumParsed)

# --- 3: platform move (server-owned, no approval, app untouched) --------------------
$script:qid++
Send-AppTool $agent $script:qid "workshop" "move" '{"to_row":2,"to_col":3}'
$mv = Read-Reply $agent $script:qid 8000
$mvOk = $false
if ($mv -ne $null) {
    try { $o = $mv | ConvertFrom-Json; $mvOk = ($o.ok -eq $true -and $o.row -eq 2 -and $o.col -eq 3) }
    catch { $mv = "parse: " + $_.Exception.Message }
}
Check "wc-move-echo" $mvOk $mv

# --- 4: read - script apps declare no snapshot tool (NIT-7 immediate answer) --------
$script:qid++
Send-AppTool $agent $script:qid "workshop" "read" "{}"
$rd = Read-Reply $agent $script:qid 5000
Check "wc-read-no-snapshot" ($rd -ne $null -and $rd -match '"ok":false' -and
                             $rd -match '"error":"unknown_app_tool"') $rd

# --- 5: act park -> approve -> onAgentAct relay --------------------------------------
$script:qid++
Send-AppTool $agent $script:qid "workshop" "act" '{"kind":"paint","row":2,"col":3}'
$ev = Wait-Event $agent '"topic":"agent\.approval_request"' 10000
$parkOk = ($ev -ne $null -and $ev.text -match '"name":"workshop\.paint at \(2,3\)"')
Check "wc-park-banner" $parkOk $(if ($ev) { $ev.text } else { "no approval_request event" })
$reqId = Get-ReqId $ev
$bridgeOk = ($ev -ne $null -and $ev.text -match '"kind":"app_tool"' -and
             $reqId -gt 0 -and
             $ev.text -match '"target":\{"app":"workshop","tool":"act"')
Check "wc-park-bridge-fields" $bridgeOk $(if ($ev) { $ev.text } else { "no event" })
$act = $null
if ($reqId -gt 0) {
    $ap = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
    Check "wc-approve" ($ap -match '"approved":true') $ap
    $act = Read-Reply $agent $script:qid 12000
}
$relayOk = ($act -ne $null -and $act -match '"painted":true' -and
            $act -match '"kind":"paint"' -and $act -match '"row":2' -and
            $act -match '"col":3')
Check "wc-act-onAgentAct-relay" $relayOk $act

# --- 6: pre-app rejections (server-side; the app pipe must see none) ------------------
$script:qid++
Send-AppTool $agent $script:qid "workshop" "act" '{"kind":"detonate","row":1,"col":1}'
$badKind = Read-Reply $agent $script:qid 5000
Check "wc-act-badkind" ($badKind -ne $null -and $badKind -match '"ok":false' -and
                        $badKind -match '"error":"bad_args"') $badKind
$script:qid++
Send-AppTool $agent $script:qid "workshop" "act" '{"kind":"paint","row":1}'
$missCol = Read-Reply $agent $script:qid 5000
Check "wc-act-missing-col" ($missCol -ne $null -and $missCol -match '"ok":false' -and
                            $missCol -match '"error":"bad_args"') $missCol
$script:qid++
Send-AppTool $agent $script:qid "workshop" "act" '{"kind":"paint","row":8,"col":9}'
$badGrid = Read-Reply $agent $script:qid 5000
Check "wc-act-badgrid" ($badGrid -ne $null -and $badGrid -match '"ok":false' -and
                        $badGrid -match '"error":"bad_grid"' -and
                        $badGrid -match '"row":8' -and $badGrid -match '"col":9') $badGrid

# --- 7: re-declaration (set_script a different grid - latest wins, reset) -------------
# src2 also defines onSnapshot (docs/60 s13 follow-up): the read relay gains
# the app snapshot path - ComposeCursorRead assembles the script's
# serialization with the cursor header (v1 kept read = explicit error).
$src2 = "declareCursor({origin:{x:0,y:0}, cellW:10, cellH:10, rows:4, cols:4," +
        " kinds:['paint','erase']});" +
        "function onAgentAct(kind,row,col) { return {ok:true, kind:kind}; }" +
        "function onSnapshot() { return {board:'................', at:16," +
        " rows:4, cols:4}; }"
$r2 = (SetScript $src2)
Check "wc-redeclare-set" ($r2 -match '"ok":true') $r2
$cat2Ok = $false
$cat2 = ""
for ($i = 0; $i -lt 20; $i++) {
    Start-Sleep -Milliseconds 500
    $cat2 = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
    if ($cat2 -match '"app":"workshop","name":"act"') {
        try {
            $o2 = $cat2 | ConvertFrom-Json
            $a2 = $o2.tools | Where-Object { $_.app -eq "workshop" -and $_.name -eq "act" } |
                Select-Object -First 1
            $e2 = ($a2.inputSchema.properties.kind.enum -join ",")
            if ($e2 -eq "paint,erase") { $cat2Ok = $true; break }
        } catch { }
    }
}
Check "wc-redeclare-enum-merged" $cat2Ok $cat2
# onSnapshot registered the app snapshot tool -> the catalog must show it
# (the read relay candidate).
Check "wc-redeclare-catalog-snapshot" ($cat2 -match '"app":"workshop","name":"snapshot"') $cat2
# Old grid was 8x10 - row 7 is inside the OLD grid, out of the NEW 4x4 one:
# a stale-cursor world would reveal it; the reset world rejects it (bad_grid).
$script:qid++
Send-AppTool $agent $script:qid "workshop" "move" '{"to_row":7,"to_col":0}'
$mvOld = Read-Reply $agent $script:qid 8000
Check "wc-redeclare-reset-move" ($mvOld -ne $null -and $mvOld -match '"ok":false' -and
                                 $mvOld -match '"error":"bad_grid"') $mvOld
$script:qid++
Send-AppTool $agent $script:qid "workshop" "move" '{"to_row":3,"to_col":3}'
$mvNew = Read-Reply $agent $script:qid 8000
$mvNewOk = $false
if ($mvNew -ne $null) {
    try { $o3 = $mvNew | ConvertFrom-Json; $mvNewOk = ($o3.ok -eq $true -and $o3.row -eq 3 -and $o3.col -eq 3) }
    catch { $mvNew = "parse: " + $_.Exception.Message }
}
Check "wc-redeclare-move-newgrid" $mvNewOk $mvNew
# onSnapshot read relay: cursor at (3,3) on the 4x4 grid - ComposeCursorRead
# assembles ok + cursor header + the script's serialization (parsed, docs/59
# s12 lesson).
$script:qid++
Send-AppTool $agent $script:qid "workshop" "read" "{}"
$rdSnap = Read-Reply $agent $script:qid 10000
$snapOk = $false
$snapDetail = ""
if ($rdSnap -ne $null) {
    try {
        $so = $rdSnap | ConvertFrom-Json
        $snapOk = ($so.ok -eq $true -and $so.cursor.row -eq 3 -and
                   $so.cursor.col -eq 3 -and $so.rows -eq 4 -and $so.cols -eq 4 -and
                   $so.snapshot.board -eq '................' -and $so.snapshot.at -eq 16)
    } catch { $snapDetail = "parse: " + $_.Exception.Message }
} else { $snapDetail = "no reply" }
Check "wc-snapshot-read-relay" $snapOk ($rdSnap + " " + $snapDetail)

# --- 8: non-cursor script clears the declaration (fail-closed hygiene) ----------------
$r3 = (SetScript "var L = createLabel({x:10,y:10,w:120,h:20},'no cursor here');")
Check "wc-clear-script-set" ($r3 -match '"ok":true') $r3
$cat3Ok = $false
$cat3 = ""
for ($i = 0; $i -lt 20; $i++) {
    Start-Sleep -Milliseconds 500
    $cat3 = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
    if ($cat3 -notmatch '"app":"workshop","name":"(move|read|act)"') { $cat3Ok = $true; break }
}
Check "wc-clear-removes-cursor-tools" $cat3Ok $cat3

# --- teardown ----------------------------------------------------------------------------
$agent.Dispose()
$lw = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
$mw = [regex]::Match($lw, '\{"id":(\d+)[^}]*"title":"Workshop"')
if ($mw.Success) {
    [void](Invoke-Agentctl ('{"tool":"close_window","args":{"id":' + $mw.Groups[1].Value + '}}'))
    Write-Output "NOTICE: workshop window closed"
}
Get-Process jkdesktop, jkwinserver, jkbridge, jkagentd -ErrorAction SilentlyContinue |
    Stop-Process -Force
Write-Output "NOTICE: the desktop server is left stopped - restart jkwinserver.exe (and jkbridge.exe) to resume the live desktop"
Write-Host ("RESULT: " + ($(if ($script:fail -eq 0) { "ALL PASS" } else { "FAIL " + $script:fail })))
exit ($script:fail)
}
finally {
    Restore-Perms
    Remove-Item $permBakFile -ErrorAction SilentlyContinue
    if (Test-Path $myappBak) {
        Copy-Item $myappBak $myapp -Force
        Remove-Item $myappBak -Force
        Write-Host "NOTICE: myapp.js restored from backup"
    } else {
        Write-Host "NOTICE: no myapp.js backup existed at start - leaving the seeded template"
    }
    Remove-Item (Join-Path $env:TEMP ("wscur_" + $PID + ".cmd")) -Force -ErrorAction SilentlyContinue
}