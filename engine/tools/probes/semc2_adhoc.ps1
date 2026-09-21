# semc2_adhoc.ps1 - semantic cursor Task 2 ad-hoc smoke (spec
# 2026-09-22-semantic-cursor §3/§8, act relay + approval banner cell label).
# ASCII-only (PS5.1). Fake cursor apps over raw pipes (semc1_adhoc.ps1 idiom,
# harness copied from probe_app_tools.ps1). Checks:
#   act pre-app server validation (Task 2 step 1): kind outside the declared
#      enum / missing row-col = bad_args, out-of-grid index = bad_grid with
#      the requested cell echo - all BEFORE the app is reached (the app pipe
#      must see no tool call);
#   parking-time cell rect fixation + banner label (step 2/3): parked act
#      approval_request name = "<app>.<kind> at (r,c)" (and target.tool stays
#      the plain app tool identity);
#   approval re-execution passes kind/row/col verbatim to the app (STRICT
#      parse of the app-side AgentToolCall);
#   approval-time re-declaration revalidation (step 4): same-geometry
#      redeclaration passes (relay happens), geometry change / declaration
#      dropped = bad_grid rejection while approval_resolved still broadcasts;
#   undeclared-app parking name unchanged: a plain (no cursor) app_tool parked
#      via an explicit permissions.json ask key keeps "<app>.<tool>"
#      (vplayer/workshop regression) - backup + finally-restore idiom.
# Run: powershell -File semc2_adhoc.ps1 "> log 2>&1" (file redirect, never a
# pipe - lesson 42). Requires a restartable desktop: kills jkdesktop, starts
# jkdesktop --server (probe-owned server instance).
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
        # Window client (Hello + CreateSurface) - cursor declarations need a
        # surface owner (NIT-8). SurfaceCreatePayload: {int32 w, int32 h, char title[128]}.
        $w = 64; $h = 64
        $pl = New-Object byte[] (8 + 128)
        [BitConverter]::GetBytes([int32]$w).CopyTo($pl, 0)
        [BitConverter]::GetBytes([int32]$h).CopyTo($pl, 4)
        $title = [Text.Encoding]::ASCII.GetBytes("fakegrid2")
        [Array]::Copy($title, 0, $pl, 8, $title.Length)
        SendMsg $p 3 $pl
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
public class PipePeekS2 {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool PeekNamedPipe(IntPtr hPipe, byte[] buf,
        int bufSize, out int read, out int avail, out int left);
}
"@
function Pipe-Avail([System.IO.Pipes.NamedPipeClientStream]$s) {
    $read = 0; $avail = 0; $left = 0
    try {
        $h = $s.SafePipeHandle.DangerousGetHandle()
        if (-not [PipePeekS2]::PeekNamedPipe($h, $null, 0, [ref]$read, [ref]$avail, [ref]$left)) { return -1 }
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
    $text = ""
    if ($len -gt $hs) { $text = [Text.Encoding]::UTF8.GetString($pl, $hs, $len - $hs) }
    return @{ type = $type; len = $len; text = $text; head0 = [uint32]$head0 }
}
# The fake app records every AgentToolCall it receives (verbatim args check)
# and answers act calls with the app-side no_act_impl error (the relay must
# pass through verbatim - the app owns the transition, errors surface as-is).
$script:lastAppCall = ""
function Read-Reply([System.IO.Pipes.NamedPipeClientStream]$agent,
                    [System.IO.Pipes.NamedPipeClientStream]$app,
                    [uint32]$qid, [int]$timeoutMs) {
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
                $script:lastAppCall = $g.text
                Send-ToolResult $app $g.head0 1 '{"ok":false,"error":"no_act_impl"}'
            }
        }
    }
    return $null
}
# Park an act (agent sends app_tool act) and return the approval_request
# event frame (drains app_tools_changed noise on the way).
function Park-Act([System.IO.Pipes.NamedPipeClientStream]$agent, [uint32]$qid,
                  [string]$argsJson) {
    SendQuery $agent $qid ('{"tool":"app_tool","args":{"app":"fakegrid","tool":"act","args":' + $argsJson + '}}')
    $ev = $null
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt 10000) {
        $f = Read-Frame $agent 300
        if ($f -ne $null -and $f.type -eq 20 -and
            $f.text -match '"topic":"agent.approval_request"') {
            $ev = $f
            break
        }
    }
    return $ev
}
# Wait for one agent event matching a topic (post-approve resolution check).
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
# Register ack read: the fake apps subscribe to agent events (semc1 idiom), so
# broadcast frames (approval_resolved / app_tools_changed) can sit in the queue
# ahead of the ack - skip non-ack frames until the AgentReply lands (lesson 30:
# a single-frame read on a subscribed pipe is a flake, not a product fault).
function Read-Ack([System.IO.Pipes.NamedPipeClientStream]$s, [int]$timeoutMs) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $timeoutMs) {
        $f = Read-Frame $s 300
        if ($f -ne $null -and $f.type -ne 20) { return $f }
    }
    return $null
}

$cursorJson = '{"app":"fakegrid","tools":[{"name":"snapshot","description":"board snapshot","inputSchema":{"type":"object","properties":{}}}],' +
    '"cursor":{"type":"cell-grid","coordSpace":"client","origin":{"x":12,"y":44},"cellW":16,"cellH":16,' +
    '"rows":9,"cols":9,"cursorOwner":"platform","act":{"kinds":["reveal","flag","question"],"gate":"ask"}}}'
$cursorSame = $cursorJson
# Same app, geometry moved (origin.x 12 -> 100) - redeclaration revalidation
# must reject a parked act approved against the OLD geometry.
$cursorMoved = '{"app":"fakegrid","tools":[{"name":"snapshot","description":"board snapshot","inputSchema":{"type":"object","properties":{}}}],' +
    '"cursor":{"type":"cell-grid","coordSpace":"client","origin":{"x":100,"y":44},"cellW":16,"cellH":16,' +
    '"rows":9,"cols":9,"cursorOwner":"platform","act":{"kinds":["reveal","flag","question"],"gate":"ask"}}}'
# Same app, cursor block dropped - parked act must be rejected (contract gone).
$cursorDropped = '{"app":"fakegrid","tools":[{"name":"snapshot","description":"board snapshot","inputSchema":{"type":"object","properties":{}}}]}'

# --- permissions.json (undeclared-app ask parking): backup + finally-restore --
$permFile = Join-Path $root "permissions.json"
$stale = Get-ChildItem (Join-Path $env:TEMP "perm_pre_semc2_*.json") -ErrorAction SilentlyContinue
if ($stale) {
    Write-Host ("FAIL: setup-stale-residue -- " + (($stale | ForEach-Object { $_.Name }) -join ", "))
    Write-Host "      stale residue from a killed run - restore engine/build/permissions.json manually, delete the leftover(s) in TEMP, re-run"
    exit 1
}
$permBakFile = Join-Path $env:TEMP ("perm_pre_semc2_" + $PID + ".json")
$hadPerm = Test-Path $permFile
if ($hadPerm) { Copy-Item $permFile $permBakFile -Force }
Write-Host "NOTICE: backing up user runtime permissions.json -> $permBakFile (restored byte-identical in finally)"
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
Check "t2-up" $up "ping"

# --- register the fake cursor app (window client) -----------------------------
$app = New-Pipe 1 $true
Send-ToolRegister $app $cursorJson
$ack = Read-Ack $app 3000
Check "t2-register-ack" ($ack -ne $null -and $ack.text -match '"ok":true') $ack.text

$agent = New-Pipe 1 $false
$script:qid = 200

# --- act pre-app validation: bad kind -----------------------------------------
$script:lastAppCall = ""
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"fakegrid","tool":"act","args":{"kind":"chording","row":3,"col":5}}}'
$badKind = Read-Reply $agent $null $script:qid 4000
Check "t2-act-badkind" ($badKind -match '"ok":false' -and $badKind -match '"error":"bad_args"') $badKind
Check "t2-act-badkind-preapp" ($script:lastAppCall -eq "") "appCall=$($script:lastAppCall)"

# --- act pre-app validation: missing row/col -----------------------------------
$script:lastAppCall = ""
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"fakegrid","tool":"act","args":{"kind":"flag","col":5}}}'
$missRow = Read-Reply $agent $null $script:qid 4000
Check "t2-act-missing-row" ($missRow -match '"ok":false' -and $missRow -match '"error":"bad_args"') $missRow
Check "t2-act-missing-preapp" ($script:lastAppCall -eq "") "appCall=$($script:lastAppCall)"

# --- act pre-app validation: out-of-grid index -> bad_grid with echo -----------
$script:lastAppCall = ""
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"fakegrid","tool":"act","args":{"kind":"flag","row":9,"col":2}}}'
$oor = Read-Reply $agent $null $script:qid 4000
Check "t2-act-outgrid" ($oor -match '"ok":false' -and $oor -match '"error":"bad_grid"' -and $oor -match '"row":9' -and $oor -match '"col":2') $oor
Check "t2-act-outgrid-preapp" ($script:lastAppCall -eq "") "appCall=$($script:lastAppCall)"

# --- parking: banner name "<app>.<kind> at (r,c)" ------------------------------
$script:qid++
$ev = Park-Act $agent $script:qid '{"kind":"flag","row":3,"col":5}'
$okPark = ($ev -ne $null -and $ev.text -match '"kind":"app_tool"' -and
           $ev.text -match '"name":"fakegrid.flag at \(3,5\)"')
Check "t2-park-banner-name" $okPark ($(if ($ev) { $ev.text } else { "no event" }))
# Structured identity stays the plain app tool (consumers that need the raw
# tool/name keep working).
$okTarget = ($ev -ne $null -and $ev.text -match '"target":\{"app":"fakegrid","tool":"act"' -and
             $ev.text -match '"target_id":\d+')
Check "t2-park-target-identity" $okTarget ($(if ($ev) { $ev.text } else { "no event" }))
$reqId = 0
if ($ev -ne $null -and $ev.text -match '"request":(\d+)') { $reqId = [int]$Matches[1] }

# --- approval re-executes: app receives kind/row/col verbatim -------------------
$script:lastAppCall = ""
$ap = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
Check "t2-approve-ack" ($ap -match '"approved":true') $ap
$actReply = Read-Reply $agent $app $script:qid 8000
Check "t2-relay-verbatim" ($actReply -match '"error":"no_act_impl"' -and
                           $script:lastAppCall -match '"tool":"act"' -and
                           $script:lastAppCall -match '"kind":"flag"' -and
                           $script:lastAppCall -match '"row":3' -and
                           $script:lastAppCall -match '"col":5') ("appCall=" + $script:lastAppCall + " | reply=" + $actReply)

# --- revalidation case A: same-geometry redeclaration -> approval proceeds ------
$script:lastAppCall = ""
Send-ToolRegister $app $cursorSame
$ackSame = Read-Ack $app 3000
Check "t2-redeclare-ack" ($ackSame -ne $null -and $ackSame.text -match '"ok":true') $ackSame.text
$script:qid++
$ev = Park-Act $agent $script:qid '{"kind":"reveal","row":1,"col":1}'
$okName = ($ev -ne $null -and $ev.text -match '"name":"fakegrid.reveal at \(1,1\)"')
$reqId = 0
if ($ev -ne $null -and $ev.text -match '"request":(\d+)') { $reqId = [int]$Matches[1] }
Check "t2-redecl-park" ($okName -and $reqId -gt 0) ($(if ($ev) { $ev.text } else { "no event" }))
$script:lastAppCall = ""
$ap = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
$actReply = Read-Reply $agent $app $script:qid 8000
Check "t2-redecl-same-geometry-relay" ($actReply -match '"error":"no_act_impl"' -and
                                       $script:lastAppCall -match '"kind":"reveal"' -and
                                       $script:lastAppCall -match '"row":1') ("appCall=" + $script:lastAppCall + " | reply=" + $actReply)

# --- revalidation case B: geometry moved -> reject, resolution still visible ----
$script:qid++
$ev = Park-Act $agent $script:qid '{"kind":"flag","row":2,"col":2}'
$reqId = 0
if ($ev -ne $null -and $ev.text -match '"request":(\d+)') { $reqId = [int]$Matches[1] }
Check "t2-park-moved" ($ev -ne $null -and $ev.text -match '"name":"fakegrid.flag at \(2,2\)"' -and $reqId -gt 0) ($(if ($ev) { $ev.text } else { "no event" }))
# Re-declare with the geometry moved BEFORE the approval - the parked record
# keeps the old rect (parking-time fixation); approval must recompute and reject.
Send-ToolRegister $app $cursorMoved
$ackMoved = Read-Ack $app 3000
Check "t2-moved-ack" ($ackMoved -ne $null -and $ackMoved.text -match '"ok":true') $ackMoved.text
$script:lastAppCall = ""
$ap = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
$rejReply = Read-Reply $agent $null $script:qid 5000
Check "t2-moved-reject" ($rejReply -match '"ok":false' -and $rejReply -match '"error":"bad_grid"') $rejReply
Check "t2-moved-no-relay" ($script:lastAppCall -eq "") "appCall=$($script:lastAppCall)"
$ev = Wait-Event $agent '"topic":"agent.approval_resolved"' 5000
Check "t2-moved-resolved-event" ($ev -ne $null -and $ev.text -match '"decision":"allow"') $(if ($ev) { $ev.text } else { "no event" })

# --- revalidation case C: cursor declaration dropped -> reject ------------------
$script:qid++
$ev = Park-Act $agent $script:qid '{"kind":"reveal","row":4,"col":4}'
# NOTE: Park-Act happens while the MOVED declaration is live - (4,4) is inside
# the 9x9 grid, the rect differs from case B's parked record but is consistent
# with the moved declaration, so parking succeeds.
$reqId = 0
if ($ev -ne $null -and $ev.text -match '"request":(\d+)') { $reqId = [int]$Matches[1] }
$okName = ($ev -ne $null -and $ev.text -match '"name":"fakegrid.reveal at \(4,4\)"')
Check "t2-park-dropped-prep" ($okName -and $reqId -gt 0) ($(if ($ev) { $ev.text } else { "no event" }))
Send-ToolRegister $app $cursorDropped
$ackDrop = Read-Ack $app 3000
Check "t2-dropped-ack" ($ackDrop -ne $null -and $ackDrop.text -match '"ok":true') $ackDrop.text
$script:lastAppCall = ""
$ap = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
$rejReply = Read-Reply $agent $null $script:qid 5000
Check "t2-dropped-reject" ($rejReply -match '"ok":false' -and $rejReply -match '"error":"bad_grid"') $rejReply
Check "t2-dropped-no-relay" ($script:lastAppCall -eq "") "appCall=$($script:lastAppCall)"

# --- undeclared app unchanged: parked plain app_tool keeps "<app>.<tool>" -------
# Explicit ask key (semc1 idiom) - plain app_tool has no cursor, so the banner
# name must stay the pre-Task-2 form (vplayer/workshop regression).
Send-ToolRegister $app '{"app":"plainapp","tools":[{"name":"bump","description":"x","inputSchema":{"type":"object","properties":{}}}]}'
$ackPlain = Read-Ack $app 3000
Check "t2-plain-register" ($ackPlain -ne $null -and $ackPlain.text -match '"ok":true') $ackPlain.text
Set-Perms '{"app_tool.plainapp.bump":"ask"}'
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"plainapp","tool":"bump","args":{}}}'
$ev = Wait-Event $agent '"topic":"agent.approval_request"' 8000
$okName = ($ev -ne $null -and $ev.text -match '"name":"plainapp\.bump"' -and
           $ev.text -notmatch ' at \(')
Check "t2-plain-name-unchanged" $okName $(if ($ev) { $ev.text } else { "no event" })
$reqId = 0
if ($ev -ne $null -and $ev.text -match '"request":(\d+)') { $reqId = [int]$Matches[1] }
if ($reqId -gt 0) {
    $ap = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
    $plainReply = Read-Reply $agent $app $script:qid 5000
    Check "t2-plain-relay" ($plainReply -match '"result":') ("reply=" + $plainReply)
}

# --- cleanup ------------------------------------------------------------------
$app.Dispose()
$agent.Dispose()
Write-Host ("RESULT: " + ($(if ($script:fail -eq 0) { "ALL PASS" } else { "FAIL " + $script:fail })))
exit ($script:fail)
}
finally {
    Restore-Perms
    Remove-Item $permBakFile -ErrorAction SilentlyContinue
}