# probe_app_tools.ps1 - app tool hub stage 1 regression (spec 2026-09-19-app-tool-hub).
# ASCII-only (PS5.1). Checks (9 + setup):
#   1  catalog: vplayer spawn -> list_app_tools 6 rows (name/description/windowId>0)
#   2  seek e2e: open clip -> poll get_status opened -> seek 1 -> pos < 2.0
#   3  error surface: unknown app -> unknown_app_tool; seek w/o seconds -> app bad_args
#   3b open failure visibility: corrupt file -> get_status openFailed+error
#      (no silent decay to idle all-false - docs/59 §18)
#   4  multi-instance: 2nd vplayer -> 12 rows; no windowId -> ambiguous+candidates(2);
#      windowId of instance 1 -> success; close 2nd
#   5  auto-cleanup: subscriber first, close_window -> catalog cleared +
#      agent.app_tools_changed event
#   6  registration validation: bad_name / namespace_conflict / too_many_tools
#   7  gate deny: permissions.json {"app_tool.vplayer.seek":"deny"} -> denied
#   8  gate ask + parking: approval_request event (kind=app_tool) -> cross approve
#      (agentctl, different conn) -> parked result + paused actually toggled
#   9  tool_timeout: fake app never answers AgentToolCall -> tool_timeout
#      3s..15s (floor catches an instantly-emitted timeout), then close ->
#      manifest cleanup
#   10 bridge regression: probe_jkbridge.ps1 re-run (generic relay unchanged)
#   stage 2 (spec §6): c11/c12/c13 = brief checks 10-12 —
#   11 MCP tools/list synthesis: jkagentd stdio -> vplayer_seek + [vplayer]
#      prefix; 12 MCP tools/call: vplayer_seek ok + pos reflects + receipt +
#      unknown dynamic -> unknown_tool; 13 fallback: server down -> core only.
#   14 fix round 1: composite-name ambiguity (coll_a.b + coll.a_b ->
#      coll_a_b): tools/call -> ambiguous_tool+candidates(2), list skips it.
#   stage 3 (spec §5): c15/c16/c17 = brief checks 13-15 -
#   15 highlight pixel measurement: COORDINATOR RULING - do NOT duplicate
#      probe_app_tools_highlight.ps1's pixel logic here; the probe is folded
#      in as a nested regression (c10/probe_jkbridge precedent) and its ALL
#      PASS is an explicit check. It manages its own server lifecycle, so
#      c16/c17 restart what they need after it.
#   16 approval_request payload: target{app,tool,windowId,title} +
#      thumb absolute path + PNG magic first 8 bytes (thumb file deleted
#      after the check - it lives in state\screenshots).
#   17 approval strip wording: jkchat is launched BEFORE the ask parks, its
#      transcript edit (id 103) and strip prompt (id 106) are read via
#      WM_GETTEXT and must carry the target-present wording ("[vplayer
#      chang #<id>] play_pause sil-hal-kka-yo?" in romanized form) - Korean
#      needles are built from codepoints at runtime; this file stays
#      ASCII-only (PS5.1 encoding trap).
# Raw pipe helpers = mgr_t1_read/mgr_t8_cross_approve frame reader (one function
# consumes header+payload whole). agentctl args = probe_notes ProcessStartInfo
# raw command-line idiom (PS5.1 argv re-parsing trap). Tool args use INTEGERs
# (AgentJson GetInt coercion contract, task-5 note).
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root = Split-Path $exe
$clip = "I:/progwork/JKENGINE/tmp/vpt2_test.mp4"
$permFile = Join-Path $root "permissions.json"
$script:fail = 0

function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host ("PASS: " + $name) }
    else { $script:fail++; Write-Host ("FAIL: " + $name + " -- " + $detail) }
    if ($detail) { Write-Host ("      " + $detail) }
}

function Invoke-Agentctl([string]$json) {
    # PS5.1 native quoting trap (docs/55 lesson 3): values with quotes/spaces
    # get mangled when PS builds the child command line. Write the raw command
    # line ourselves: agentctl "{escaped-json}" - CRT turns \" into a literal ".
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
# windowId sits at the app_tool args level (alongside app/tool) - the app's
# own payload is the nested "args" passthrough (server reads windowId via
# GetObjInt("args","windowId"); nested passthrough args are opaque).
function AppTool([string]$app, [string]$tool, [string]$argsJson, [int]$windowId = 0) {
    $a = '{"app":"' + $app + '","tool":"' + $tool + '"'
    if ($windowId -gt 0) { $a += ',"windowId":' + $windowId }
    if ($argsJson) { $a += ',"args":' + $argsJson }
    $a += '}'
    return (Invoke-Agentctl ('{"tool":"app_tool","args":' + $a + '}'))
}

# --- permissions.json is probe-owned state: back up, remove for a clean
# baseline (absent file = allow for app_tool, deny for close_window), restore
# in finally (probe_files/probe_notes pattern - never burn user state).
# Stale-residue guard FIRST: a killed earlier run leaves its backup behind;
# backing up the current file on top of that could adopt polluted residue as
# user state (the exact incident class). Refuse to run - manual restore only.
# Backup name is per-process so a later run never overwrites a prior backup.
$stale = Get-ChildItem (Join-Path $env:TEMP "perm_pre_apptools_*.json") -ErrorAction SilentlyContinue
if ($stale) {
    Write-Host ("FAIL: setup-stale-residue -- " + (($stale | ForEach-Object { $_.Name }) -join ", "))
    Write-Host "      stale residue from a killed run - restore engine/build/permissions.json manually, delete the leftover(s) in TEMP, re-run"
    exit 1
}
$permBakFile = Join-Path $env:TEMP ("perm_pre_apptools_" + $PID + ".json")
$hadPerm = Test-Path $permFile
if ($hadPerm) { Copy-Item $permFile $permBakFile -Force }
function Set-Perms([string]$json) {
    [IO.File]::WriteAllText($permFile, $json, (New-Object System.Text.UTF8Encoding($false)))
}
function Clear-Perms { Remove-Item $permFile -ErrorAction SilentlyContinue }
Clear-Perms   # clean baseline from the start: c1-c3 run without user perms, not against the live file

# --- raw named pipe client (mgr_t8_cross_approve idiom + deadline reads) -----
function SendMsg([System.IO.Pipes.NamedPipeClientStream]$s, [int]$type, [byte[]]$payload) {
    $hdr = New-Object byte[] 12
    [BitConverter]::GetBytes([uint32]0x4A4B0001).CopyTo($hdr, 0)
    [BitConverter]::GetBytes([uint32]$type).CopyTo($hdr, 4)
    [BitConverter]::GetBytes([uint32]$payload.Length).CopyTo($hdr, 8)
    $s.Write($hdr, 0, 12)
    if ($payload.Length -gt 0) { $s.Write($payload, 0, $payload.Length) }
    $s.Flush()
}
# The acceptor reads Hello + a SECOND message: CreateSurface (window client)
# or AgentEventSubscribe (control-only; 1 = subscriber, 0 = plain agent decl,
# jkagentd/agentctl convention). A raw client whose second message is anything
# else is discarded server-side ("expected CreateSurface", pipe closed) - so
# fake-app connections must declare BEFORE registering.
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
function Subscribe-Pipe([System.IO.Pipes.NamedPipeClientStream]$s, [uint32]$flag) {
    $sub = New-Object byte[] 4
    [BitConverter]::GetBytes($flag).CopyTo($sub, 0)
    SendMsg $s 19 $sub
}
function SendQuery([System.IO.Pipes.NamedPipeClientStream]$s, [uint32]$qid, [string]$json) {
    $body = [Text.Encoding]::UTF8.GetBytes($json)
    $payload = New-Object byte[] (8 + $body.Length)
    [BitConverter]::GetBytes([uint32]$qid).CopyTo($payload, 0)
    [BitConverter]::GetBytes([uint32]$body.Length).CopyTo($payload, 4)
    [Array]::Copy($body, 0, $payload, 8, $body.Length)
    SendMsg $s 17 $payload
}
# AgentToolRegister (type 22): header {jsonLen} + JSON.
function Send-ToolRegister([System.IO.Pipes.NamedPipeClientStream]$s, [string]$json) {
    $body = [Text.Encoding]::UTF8.GetBytes($json)
    $payload = New-Object byte[] (4 + $body.Length)
    [BitConverter]::GetBytes([uint32]$body.Length).CopyTo($payload, 0)
    [Array]::Copy($body, 0, $payload, 4, $body.Length)
    SendMsg $s 22 $payload
}
# Bytes buffered in the pipe (PeekNamedPipe - PS5.1 NamedPipeClientStream has
# no IsDataAvailable). -1 = pipe closed/broken.
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class PipePeek {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool PeekNamedPipe(IntPtr hPipe, byte[] buf,
        int bufSize, out int read, out int avail, out int left);
}
"@
function Pipe-Avail([System.IO.Pipes.NamedPipeClientStream]$s) {
    $read = 0; $avail = 0; $left = 0
    try {
        $h = $s.SafePipeHandle.DangerousGetHandle()
        if (-not [PipePeek]::PeekNamedPipe($h, $null, 0, [ref]$read, [ref]$avail, [ref]$left)) { return -1 }
    } catch { return -1 }
    return $avail
}
# jkchat transcript/strip readers (c17): jkchat is a plain Win32 app - the
# transcript is a multiline EDIT (id IDC_LOG=103) and the approval strip a
# STATIC (id IDC_PROMPT=106). WM_GETTEXTLENGTH(0x000E)/WM_GETTEXT(0x000D) on
# standard controls is marshaled cross-process by the OS.
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class ChatUiR {
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
    [DllImport("user32.dll")] public static extern int SendMessageW(IntPtr h, int msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int SendMessageW(IntPtr h, int msg, IntPtr w, StringBuilder l);
}
"@
function Read-Ctrl([IntPtr]$hwnd, [int]$id) {
    $c = [ChatUiR]::GetDlgItem($hwnd, $id)
    if ($c -eq [IntPtr]::Zero) { return "" }
    $n = [ChatUiR]::SendMessageW($c, 0x000E, [IntPtr]::Zero, [IntPtr]::Zero)
    if ($n -le 0) { return "" }
    $sb = New-Object System.Text.StringBuilder ($n + 2)
    [void][ChatUiR]::SendMessageW($c, 0x000D, [IntPtr]($n + 1), $sb)
    return $sb.ToString()
}
# Deadline-bounded frame read: poll availability, then a SYNCHRONOUS Read only
# when the needed bytes are already buffered (Read returns without blocking).
# ReadAsync+Wait is unusable here - an abandoned pending read makes the NEXT
# ReadAsync block indefinitely (measured: 2nd frame read hung past its budget).
# One function consumes the entire frame (12-byte wire header + payload) and
# slices the type-specific message header off the payload: AgentQuery 8
# {queryId,jsonLen}, AgentReply/AgentToolResult 12 {queryId,ok,jsonLen},
# AgentEvent/AgentToolRegister 4 {jsonLen}, AgentToolCall 8 {reqId,jsonLen}.
# (mgr_t8's flat 12-byte skip happened to work for its event regexes because
# they matched mid-JSON - events actually carry a 4-byte header.)
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
    if ($type -eq 20 -or $type -eq 22) { $hs = 4 }
    elseif ($type -eq 17 -or $type -eq 23) { $hs = 8 }
    $text = ""
    if ($len -gt $hs) { $text = [Text.Encoding]::UTF8.GetString($pl, $hs, $len - $hs) }
    return @{ type = $type; len = $len; text = $text }
}

# --- server lifecycle --------------------------------------------------------
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process jkapp_vplayer -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
$up = $false
foreach ($i in 1..30) {
    Start-Sleep -Milliseconds 500
    if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up = $true; break }
}
Check "setup-server-up" $up ""

# Launch a vplayer and poll until its 6 tools are registered (registration
# happens in OnInit, post-connect). Returns the list_app_tools reply.
function Spawn-VPlayer {
    Invoke-Agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
    foreach ($i in 1..30) {
        Start-Sleep -Milliseconds 500
        $l = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
        if ($l -match '"app":"vplayer"') { return $l }
    }
    return $l
}
# Poll get_status until opened (<=10s). Returns the get_status reply.
function Wait-Opened([string]$app) {
    foreach ($i in 1..25) {
        $st = (AppTool $app "get_status" "")
        if ($st -match '"opened":true') { return $st }
        Start-Sleep -Milliseconds 400
    }
    return $st
}

try {
    # ---- check 1: catalog ----------------------------------------------------
    $cat = Spawn-VPlayer
    $rows = [regex]::Matches($cat, '"app":"vplayer"').Count
    Check "c1-catalog-6-rows" ($cat -match '"ok":true' -and $rows -eq 6) ("rows=$rows")
    $namesOk = $true
    foreach ($n in @("open", "play_pause", "seek", "set_volume", "set_av_delay", "get_status")) {
        if ($cat -notmatch ('"app":"vplayer","name":"' + $n + '"')) { $namesOk = $false }
    }
    Check "c1-catalog-names" $namesOk "6 expected tool names present"
    $descOk = ($cat -match '"name":"seek","description":"[^"]+"') -and
              ($cat -match '"name":"get_status","description":"[^"]+"')
    Check "c1-catalog-descriptions" $descOk ""
    $wid = 0
    $m = [regex]::Match($cat, '"name":"seek","description":"[^"]*","inputSchema":[\s\S]*?"windowId":(\d+)')
    if ($m.Success) { $wid = [int]$m.Groups[1].Value }
    Check "c1-catalog-windowId>0" ($wid -gt 0) ("seek windowId=$wid")

    # ---- check 2: seek e2e ----------------------------------------------------
    $st = (AppTool "vplayer" "get_status" "")
    if ($st -notmatch '"opened":true') {
        $op = (AppTool "vplayer" "open" ('{"path":"' + $clip + '"}'))
        Check "c2-open-accepted" ($op -match '"accepted":true') $op
        $st = Wait-Opened "vplayer"
    }
    Check "c2-opened" ($st -match '"opened":true') $st
    $sk = (AppTool "vplayer" "seek" '{"seconds":1}')
    Check "c2-seek-ack" ($sk -match '"ok":true') $sk
    Start-Sleep -Milliseconds 300
    $st2 = (AppTool "vplayer" "get_status" "")
    $pos = -1.0
    $m = [regex]::Match($st2, '"pos":([0-9.]+)')
    if ($m.Success) { $pos = [double]$m.Groups[1].Value }
    Check "c2-pos<2.0" ($pos -ge 0 -and $pos -lt 2.0) ("pos=$pos")

    # ---- check 3: error surface ------------------------------------------------
    $e1 = (AppTool "nope" "x" "")
    Check "c3-unknown-app-tool" ($e1 -match '"error":"unknown_app_tool"') $e1
    $e2 = (AppTool "vplayer" "seek" "")
    Check "c3-bad-args-passthrough" ($e2 -match '"error":\{"error":"bad_args"') $e2

    # ---- check 3b: open failure visibility (docs/59 §18) ------------------------
    # A corrupt file (truncated no-moov mp4 fixture) must surface its classified
    # failure through get_status (openFailed + non-empty error). The defect:
    # BuildUi adopts the failure into openError_ and ClosePlayer's the core
    # within one frame, so get_status decayed to all-false + "" - the phone
    # bridge LLM watched accepted=true then silence forever.
    $bad = "I:/progwork/JKENGINE/tmp/vpt1_trunc_tail.mp4"
    $ob = (AppTool "vplayer" "open" ('{"path":"' + $bad + '"}'))
    Check "c3b-open-corrupt-accepted" ($ob -match '"accepted":true') $ob
    $fb = ""
    foreach ($i in 1..25) {
        Start-Sleep -Milliseconds 400
        $fb = (AppTool "vplayer" "get_status" "")
        if ($fb -match '"openFailed":true') { break }
    }
    Check "c3b-open-failure-surface" ($fb -match '"openFailed":true' -and
                                      $fb -match '"error":"[^"]+"') $fb

    # ---- check 4: multi-instance + ambiguity -----------------------------------
    Set-Perms '{"close_window":"allow"}'   # close_window defaults deny w/o file
    Invoke-Agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
    $cat2 = ""
    foreach ($i in 1..30) {
        Start-Sleep -Milliseconds 500
        $cat2 = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
        if (([regex]::Matches($cat2, '"app":"vplayer"').Count) -eq 12) { break }
    }
    $rows2 = [regex]::Matches($cat2, '"app":"vplayer"').Count
    Check "c4-12-rows" ($rows2 -eq 12) ("rows=$rows2")
    $ids = @()
    foreach ($mm in [regex]::Matches($cat2, '"name":"get_status","description":"[^"]*","inputSchema":[\s\S]*?"windowId":(\d+)')) {
        $ids += [int]$mm.Groups[1].Value
    }
    $amb = (AppTool "vplayer" "get_status" "")
    $cn = 0
    $m = [regex]::Match($amb, '"error":"ambiguous","candidates":\[([^\]]*)\]')
    if ($m.Success) { $cn = [regex]::Matches($m.Groups[1].Value, '"windowId":').Count }
    Check "c4-ambiguous-candidates" ($cn -eq 2 -and $ids.Count -eq 2) ("candidates=$cn ids=$($ids -join ',')")
    # windowId of instance 1 (the manifest registered first) -> direct hit.
    $w1 = $wid
    $d1 = (AppTool "vplayer" "get_status" "" $w1)
    Check "c4-windowId-direct" ($d1 -match '"ok":true' -and $d1 -match '"result":\{') $d1
    # close the second instance -> back to 6 rows
    $w2 = ($ids | Where-Object { $_ -ne $w1 } | Select-Object -First 1)
    $cl2 = (Invoke-Agentctl ('{"tool":"close_window","args":{"id":' + $w2 + '}}'))
    $cleared = $false
    foreach ($i in 1..20) {
        Start-Sleep -Milliseconds 500
        $cat3 = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
        if (([regex]::Matches($cat3, '"app":"vplayer"').Count) -eq 6) { $cleared = $true; break }
    }
    Check "c4-second-closed-back-to-6" ($cleared -and $cl2 -match '"ok":true') ("closed2=$cl2 rows=6:$cleared")

    # ---- check 5: auto-cleanup (subscriber FIRST - lesson 28) ------------------
    $connE = New-Pipe 1   # subscriber
    $cl1 = (Invoke-Agentctl ('{"tool":"close_window","args":{"id":' + $w1 + '}}'))
    $gone = $false
    foreach ($i in 1..20) {
        Start-Sleep -Milliseconds 500
        $cat4 = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
        if (([regex]::Matches($cat4, '"app":"vplayer"').Count) -eq 0) { $gone = $true; break }
    }
    Check "c5-catalog-cleared" ($gone -and $cl1 -match '"ok":true') "close=$cl1 rows0=$gone"
    $evt = ""
    foreach ($i in 1..50) {
        $f = Read-Frame $connE 400
        if ($f -and $f.type -eq 20 -and $f.text -match '"topic":"agent\.app_tools_changed"') { $evt = $f.text; break }
    }
    Check "c5-app-tools-changed-event" ($evt -ne "") $evt
    $connE.Dispose()

    # ---- check 6: registration validation (fake app over raw pipe) -------------
    $connR = New-Pipe 0   # control-only declaration, then register
    Send-ToolRegister $connR '{"app":"fakereg","tools":[{"name":"Bad-Name","description":"x"}]}'
    $ack1 = ""
    foreach ($i in 1..20) {
        $f = Read-Frame $connR 400
        if ($f -and $f.type -eq 18 -and $f.text -match '"error"') { $ack1 = $f.text; break }
    }
    Check "c6-bad-name" ($ack1 -match '"error":"bad_name"') $ack1
    Send-ToolRegister $connR '{"app":"list_windows","tools":[{"name":"ok_tool","description":"x"}]}'
    $ack2 = ""
    foreach ($i in 1..20) {
        $f = Read-Frame $connR 400
        if ($f -and $f.type -eq 18 -and $f.text -match '"error"') { $ack2 = $f.text; break }
    }
    Check "c6-namespace-conflict" ($ack2 -match '"error":"namespace_conflict"') $ack2
    # c6b (final review Important 1): event reserved-topic prefixes are also
    # namespace_conflict - app name equal to a server topic segment, and a
    # dotted tool name under a reserved prefix ("window.created" style).
    Send-ToolRegister $connR '{"app":"agent","tools":[{"name":"ok_tool","description":"x"}]}'
    $ack2b = ""
    foreach ($i in 1..20) {
        $f = Read-Frame $connR 400
        if ($f -and $f.type -eq 18 -and $f.text -match '"error"') { $ack2b = $f.text; break }
    }
    Check "c6b-reserved-app" ($ack2b -match '"error":"namespace_conflict"') $ack2b
    Send-ToolRegister $connR '{"app":"fakereg3","tools":[{"name":"window_created","description":"x"}]}'
    $ack2c = ""
    foreach ($i in 1..20) {
        $f = Read-Frame $connR 400
        if ($f -and $f.type -eq 18 -and $f.text -match '"ok"') { $ack2c = $f.text; break }
    }
    # window_created (underscore) must still be ACCEPTED - only dotted
    # topic-style names collide; bad_name would mean the token rule broke.
    Check "c6b-underscore-ok" ($ack2c -match '"ok":true') $ack2c
    $many = '{"app":"fakereg2","tools":['
    for ($i = 0; $i -lt 33; $i++) {
        if ($i) { $many += "," }
        $many += '{"name":"tool_' + ("{0:d2}" -f $i) + '","description":"x"}'
    }
    $many += "]}";
    Send-ToolRegister $connR $many
    $ack3 = ""
    foreach ($i in 1..20) {
        $f = Read-Frame $connR 400
        if ($f -and $f.type -eq 18 -and $f.text -match '"error"') { $ack3 = $f.text; break }
    }
    Check "c6-too-many-tools" ($ack3 -match '"error":"too_many_tools"') $ack3
    $connR.Dispose()

    # ---- respawn vplayer for the gate checks (7/8) + open the clip -------------
    Clear-Perms
    $cat5 = Spawn-VPlayer
    Check "setup-vplayer-reregistered" (([regex]::Matches($cat5, '"app":"vplayer"').Count) -eq 6) $cat5
    $op2 = (AppTool "vplayer" "open" ('{"path":"' + $clip + '"}'))
    $st3 = Wait-Opened "vplayer"
    Check "setup-clip-opened" ($st3 -match '"opened":true') "$op2 / $st3"

    # ---- check 7: gate deny -----------------------------------------------------
    Set-Perms '{"app_tool.vplayer.seek":"deny"}'
    $d = (AppTool "vplayer" "seek" '{"seconds":1}')
    Check "c7-deny" ($d -match '"error":"denied"') $d
    Clear-Perms

    # ---- check 8: gate ask + parking (cross approve) -----------------------------
    $pre = (AppTool "vplayer" "get_status" "")
    $pausedPre = $null
    $m = [regex]::Match($pre, '"paused":(true|false)')
    if ($m.Success) { $pausedPre = ($m.Groups[1].Value -eq "true") }
    Check "c8-pre-status" ($pausedPre -ne $null) $pre
    Set-Perms '{"app_tool.vplayer.play_pause":"ask"}'
    $connA = New-Pipe 1   # subscriber (parks the ask query)
    SendQuery $connA 901 '{"tool":"app_tool","args":{"app":"vplayer","tool":"play_pause","args":{}}}'
    $reqId = 0; $evt8 = ""   # init: a missed event must not echo a stale loop value (final review Minor)
    foreach ($i in 1..50) {
        $f = Read-Frame $connA 400
        if ($f -and $f.type -eq 20 -and $f.text -match '"kind":"app_tool"' -and
            $f.text -match '"name":"vplayer\.play_pause"') {
            $mm = [regex]::Match($f.text, '"request":(\d+)')
            if ($mm.Success) { $reqId = [int]$mm.Groups[1].Value; $evt8 = $f.text }
            break
        }
    }
    Check "c8-approval-request" ($reqId -gt 0) ($evt8 + " req=$reqId")
    $ap = (Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}'))
    Check "c8-approve-ack" ($ap -match '"approved":true') $ap
    $parked = ""
    foreach ($i in 1..50) {
        $f = Read-Frame $connA 400
        if ($f -and $f.type -eq 18 -and $f.text -match '"result"') { $parked = $f.text; break }
    }
    $pausedNew = $null
    $m = [regex]::Match($parked, '"paused":(true|false)')
    if ($m.Success) { $pausedNew = ($m.Groups[1].Value -eq "true") }
    Check "c8-parked-result" ($parked -match '"ok":true' -and $pausedNew -ne $pausedPre) ($parked + " pre=$pausedPre new=$pausedNew")
    $post = (AppTool "vplayer" "get_status" "")
    $pausedPost = $null
    $m = [regex]::Match($post, '"paused":(true|false)')
    if ($m.Success) { $pausedPost = ($m.Groups[1].Value -eq "true") }
    Check "c8-paused-toggled" ($pausedNew -ne $null -and $pausedPost -eq $pausedNew -and $pausedPost -ne $pausedPre) ("post=$post")
    $connA.Dispose()
    Clear-Perms

    # ---- check 9: tool_timeout (fake app registers, never answers) ---------------
    $connB = New-Pipe 0   # control-only declaration, then register
    Send-ToolRegister $connB '{"app":"slowpoke","tools":[{"name":"ping1","description":"never answers"}]}'
    $ackB = ""
    foreach ($i in 1..20) {
        $f = Read-Frame $connB 400
        if ($f -and $f.type -eq 18 -and $f.text -match '"ok"') { $ackB = $f.text; break }
    }
    Check "c9-register-ack" ($ackB -match '"ok":true') $ackB
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $to = (AppTool "slowpoke" "ping1" "")
    $ms = [int]$sw.ElapsedMilliseconds
    Check "c9-tool-timeout" ($to -match '"error":"tool_timeout"' -and $ms -ge 3000 -and $ms -le 15000) ("$to elapsed=${ms}ms")
    $connB.Dispose()   # close the client: manifest cleanup (inflight already consumed)
    $goneB = $false
    foreach ($i in 1..20) {
        Start-Sleep -Milliseconds 400
        $cat6 = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
        if (([regex]::Matches($cat6, '"app":"slowpoke"').Count) -eq 0) { $goneB = $true; break }
    }
    Check "c9-manifest-cleanup" $goneB ""

    # ---- check 10: bridge regression (probe-managed server, restores perms) ------
    # Clean slate first (dev-run lesson: the nested probe's fixed 2s jkbridge
    # startup window is tight while this probe's server + vplayer decode are
    # still live; a standalone-run-shaped slate keeps it deterministic).
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-Process jkapp_vplayer -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-Process jkbridge -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 2
    $bridgeLog = Join-Path $env:TEMP "probe_app_tools_bridge.log"
    $bo = (& powershell -NoProfile -ExecutionPolicy Bypass `
        -File "I:\progwork\JKENGINE\engine\tools\probes\probe_jkbridge.ps1") 2>&1
    $bo | Out-File -FilePath $bridgeLog -Encoding ASCII
    $brOk = (($bo | ForEach-Object { "$_" }) -join "`n") -match "PASS: jkbridge"
    Check "c10-jkbridge-regression" $brOk ("log=$bridgeLog")

    # ---- stage 2 (spec 2026-09-19-app-tool-hub §6): broker dynamic synthesis +
    # routing. Check ids c11/c12/c13 = brief stage-2 checks 10/11/12 (c10 is
    # taken by the bridge regression above). One jkagentd process per MCP
    # invocation (probe_agent_mcp idiom - the broker is stateless between
    # calls); judged on the JSON-RPC envelope. The broker gate reads the SAME
    # permissions.json this probe owns, so the Clear-Perms baseline = dynamic
    # tools allow (3-tier keys absent -> allow, broker lesson).
    Clear-Perms
    $agnt = Join-Path $root "jkagentd.exe"
    function Invoke-Mcp([string[]]$lines) {
        $out = $lines | & $agnt 2>$null
        return ($out -join "`n")
    }
    Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
    $up2 = $false
    foreach ($i in 1..30) {
        Start-Sleep -Milliseconds 500
        if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up2 = $true; break }
    }
    Check "setup-server-up-2" $up2 ""
    $cat7 = Spawn-VPlayer
    Check "setup-vplayer-3" (([regex]::Matches($cat7, '"app":"vplayer"').Count) -eq 6) $cat7

    # ---- c11 (stage-2 #10): MCP tools/list synthesis ----------------------------
    # initialize -> tools/list in ONE process (real MCP stdio handshake order).
    $lst = Invoke-Mcp @(
        '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}',
        '{"jsonrpc":"2.0","id":2,"method":"tools/list"}')
    Check "c11-mcp-dynamic-listed" ($lst -match '"name":"vplayer_seek"' -and
                                    $lst -match '\[vplayer\]') ($lst.Substring(0, [Math]::Min(400, $lst.Length)))
    Check "c11-mcp-core-tools" ($lst -match '"name":"list_app_tools"' -and
                                $lst -match '"name":"app_tool"') ""
    # Dynamic names must be the <app>_<tool> union (no dotted names).
    Check "c11-mcp-no-dotted-names" ($lst -notmatch '"name":"vplayer\.') ""

    # ---- c12 (stage-2 #11): MCP tools/call routing -------------------------------
    $op3 = (AppTool "vplayer" "open" ('{"path":"' + $clip + '"}'))
    $st4 = Wait-Opened "vplayer"
    Check "setup-clip-opened-2" ($st4 -match '"opened":true') "$op3 / $st4"
    $call = Invoke-Mcp '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"vplayer_seek","arguments":{"seconds":2}}}'
    Check "c12-mcp-call-ok" ($call -match 'ok\\":true') $call
    Start-Sleep -Milliseconds 400
    $stmcp = Invoke-Mcp '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"vplayer_get_status","arguments":{}}}'
    $pos2 = -1.0
    $m = [regex]::Match($stmcp, 'pos\\":([0-9.]+)')
    if ($m.Success) { $pos2 = [double]$m.Groups[1].Value }
    Check "c12-pos-reflects" ($pos2 -ge 1.0 -and $pos2 -lt 5.0) ("pos=$pos2")
    # Spec §6: the relay records receipts like other broker-path calls (the
    # receipt row carries the MCP name the caller used).
    $rec = (Get-Content (Join-Path $root "state\receipts.jsonl") -Tail 10 -ErrorAction SilentlyContinue) -join "`n"
    Check "c12-receipt" ($rec -match '"tool":"vplayer_seek"') ($rec.Substring(0, [Math]::Min(200, $rec.Length)))
    # Unknown dynamic name -> reverse-match miss -> unknown_tool (not a hang).
    $unk = Invoke-Mcp '{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"vplayer_nope","arguments":{}}}'
    Check "c12-unknown-dynamic" ($unk -match 'unknown_tool') $unk

    # ---- c14 (fix round 1): composite-name ambiguity -----------------------------
    # app "coll_a"+tool "b" and app "coll"+tool "a_b" both yield MCP name
    # "coll_a_b" (fix round 1 Important-1): tools/call must self-correct with
    # ambiguous_tool+candidates (never pick arbitrarily - the gate key
    # app_tool.<app>.<tool> would misapply), and tools/list must skip the
    # unroutable name. Server §4.2 mirrored. Fake apps over raw pipes (c6/c9
    # idiom); underscores are valid app tokens (ValidAppToolToken).
    $connX = New-Pipe 0
    $connY = New-Pipe 0
    Send-ToolRegister $connX '{"app":"coll_a","tools":[{"name":"b","description":"x"}]}'
    Send-ToolRegister $connY '{"app":"coll","tools":[{"name":"a_b","description":"x"}]}'
    $ackX = ""
    foreach ($i in 1..20) {
        $f = Read-Frame $connX 400
        if ($f -and $f.type -eq 18 -and $f.text -match '"ok"') { $ackX = $f.text; break }
    }
    $ackY = ""
    foreach ($i in 1..20) {
        $f = Read-Frame $connY 400
        if ($f -and $f.type -eq 18 -and $f.text -match '"ok"') { $ackY = $f.text; break }
    }
    Check "c14-setup-registered" ($ackX -match '"ok":true' -and $ackY -match '"ok":true') "$ackX / $ackY"
    $ambc = Invoke-Mcp '{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"coll_a_b","arguments":{}}}'
    # ambiguous_tool is a bare JSON-RPC result (early return, not content text)
    # - quotes are NOT escaped in the wire text.
    $cn2 = 0
    $m = [regex]::Match($ambc, '"error":"ambiguous_tool","candidates":\[([^\]]*)\]')
    if ($m.Success) { $cn2 = [regex]::Matches($m.Groups[1].Value, '"app":').Count }
    Check "c14-ambiguous-tool" ($cn2 -eq 2) ($ambc + " candidates=$cn2")
    $lst2 = Invoke-Mcp '{"jsonrpc":"2.0","id":8,"method":"tools/list"}'
    Check "c14-list-skips-composite" ($lst2 -notmatch '"name":"coll_a_b"') ""
    $connX.Dispose()
    $connY.Dispose()
    Start-Sleep -Milliseconds 800   # manifest cleanup lands

    # ---- c13 (stage-2 #12): fallback — server down -> static part only ----------
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-Process jkapp_vplayer -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 2
    $fb = Invoke-Mcp '{"jsonrpc":"2.0","id":6,"method":"tools/list"}'
    Check "c13-fallback-static" ($fb -match '"name":"list_windows"' -and
                                 $fb -notmatch '"name":"vplayer_seek"') ($fb.Substring(0, [Math]::Min(300, $fb.Length)))

    # ---- c15 (stage-3 #13): highlight pixel measurement, folded in -----------
    # Coordinator ruling: probe_app_tools_highlight.ps1 owns the pixel logic
    # (amber band/ring/glyph). Nested regression like c10 - the nested probe
    # kills and restarts the server VISIBLE (CopyFromScreen needs a real
    # window) and restores permissions itself, so c16/c17 rebuild after it.
    Get-Process jkbridge -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 2
    # Unique per parent PID: a fixed name let run2's ALL PASS overwrite
    # run1's failing log — the failure detail was unrecoverable (2026-09-20).
    $hlLog = Join-Path $env:TEMP "probe_app_tools_highlight_nested_$PID.log"
    $ho = (& powershell -NoProfile -ExecutionPolicy Bypass `
        -File "I:\progwork\JKENGINE\engine\tools\probes\probe_app_tools_highlight.ps1") 2>&1
    $ho | Out-File -FilePath $hlLog -Encoding ASCII
    $hlOk = (($ho | ForEach-Object { "$_" }) -join "`n") -match "RESULT: ALL PASS"
    Check "c15-highlight-probe" $hlOk ("log=$hlLog")

    # ---- c16/c17 (stage-3 #14/#15): approval payload + strip wording ---------
    # ONE parked ask feeds both: the raw-pipe subscriber inspects the event
    # JSON (c16), jkchat - launched BEFORE the park, it subscribes on its own
    # - renders the strip from the same event and is read via WM_GETTEXT (c17).
    Clear-Perms
    Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
    $up3 = $false
    foreach ($i in 1..30) {
        Start-Sleep -Milliseconds 500
        if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up3 = $true; break }
    }
    Check "setup-server-up-3" $up3 ""
    $cat9 = Spawn-VPlayer
    Check "setup-vplayer-4" (([regex]::Matches($cat9, '"app":"vplayer"').Count) -eq 6) $cat9
    $wcur = 0
    $m = [regex]::Match($cat9, '"name":"seek","description":"[^"]*","inputSchema":[\s\S]*?"windowId":(\d+)')
    if ($m.Success) { $wcur = [int]$m.Groups[1].Value }
    $op4 = (AppTool "vplayer" "open" ('{"path":"' + $clip + '"}'))
    $st5 = Wait-Opened "vplayer"
    Check "setup-clip-opened-3" ($st5 -match '"opened":true' -and $wcur -gt 0) "$op4 / $st5"
    [void](Invoke-Agentctl '{"tool":"launch_chat","args":{}}')
    $chatHwnd = [IntPtr]::Zero
    foreach ($i in 1..20) {
        Start-Sleep -Milliseconds 500
        $cp = Get-Process jkchat -ErrorAction SilentlyContinue |
            Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
        if ($cp) { $chatHwnd = [IntPtr]$cp.MainWindowHandle; break }
    }
    Check "setup-jkchat-up" ($chatHwnd -ne [IntPtr]::Zero) ""

    Set-Perms '{"app_tool.vplayer.play_pause":"ask"}'
    $connC = New-Pipe 1
    SendQuery $connC 911 '{"tool":"app_tool","args":{"app":"vplayer","tool":"play_pause","args":{}}}'
    $reqId16 = 0; $evt16 = ""
    foreach ($i in 1..50) {
        $f = Read-Frame $connC 400
        if ($f -and $f.type -eq 20 -and $f.text -match '"kind":"app_tool"' -and
            $f.text -match '"name":"vplayer\.play_pause"') {
            $mm = [regex]::Match($f.text, '"request":(\d+)')
            if ($mm.Success) { $reqId16 = [int]$mm.Groups[1].Value; $evt16 = $f.text }
            break
        }
    }
    Check "c16-setup-parked" ($reqId16 -gt 0) ($evt16 + " req=$reqId16")

    # c16a: target block {app,tool,windowId,title} - windowId must be THIS
    # vplayer layer (the catalog one), title non-empty.
    Check "c16-target-fields" ($evt16 -match ('"target":\{"app":"vplayer","tool":"play_pause","windowId":' + $wcur + ',"title":"[^"]+"')) ("wcur=$wcur " + $evt16)

    # c16b/c: thumb = absolute StateDir path, PNG magic first 8 bytes.
    $thumb = ""
    $m = [regex]::Match($evt16, '"thumb":"([^"]+)"')
    if ($m.Success) { $thumb = $m.Groups[1].Value }
    $thumbOk = ($thumb -ne "" -and [System.IO.Path]::IsPathRooted($thumb) -and (Test-Path $thumb))
    Check "c16-thumb-absolute" $thumbOk ("thumb=$thumb")
    $magic = @()
    if ($thumbOk) { $magic = [System.IO.File]::ReadAllBytes($thumb)[0..7] }
    $pngOk = ($magic.Count -eq 8 -and $magic[0] -eq 0x89 -and $magic[1] -eq 0x50 -and
              $magic[2] -eq 0x4E -and $magic[3] -eq 0x47 -and $magic[4] -eq 0x0D -and
              $magic[5] -eq 0x0A -and $magic[6] -eq 0x1A -and $magic[7] -eq 0x0A)
    Check "c16-thumb-png-magic" $pngOk (($magic | ForEach-Object { $_.ToString("X2") }) -join " ")

    # c17: strip wording on the real jkchat UI. Strip prompt (106) must be the
    # exact target-present string; the transcript line (103) must carry the
    # app-tool label + window tag. Korean needles from codepoints (ASCII-only
    # file): chang=window, "aap do"=the [..] label, "sil-hal-kka-yo"=exec ask.
    function U([int[]]$c) { return (-join ($c | ForEach-Object { [char]$_ })) }
    $krChang = U @(0xCC3D)                                    # chang (window)
    $krAapDo = U @(0xC571, 0x0020, 0xB3C4, 0xAD6C)            # aep do-gu label
    $krExe   = U @(0xC2E4, 0xD589, 0xD560, 0xAE4C, 0xC694)    # sil-hal-kka-yo
    $stripWant = "[vplayer " + $krChang + " #" + $wcur + "] play_pause " + $krExe + "?"
    $strip = ""
    foreach ($i in 1..12) {
        Start-Sleep -Milliseconds 400
        $strip = Read-Ctrl $chatHwnd 106
        if ($strip -match "play_pause") { break }
    }
    Check "c17-strip-target-wording" ($strip -eq $stripWant) ("strip=[$strip] want=[$stripWant]")
    $logTxt = Read-Ctrl $chatHwnd 103
    $logTail = ""
    if ($logTxt.Length -gt 300) { $logTail = $logTxt.Substring($logTxt.Length - 300) } else { $logTail = $logTxt }
    # .Contains, not -match: the needles contain literal "[" (regex class open).
    $lineWant = "[" + $krAapDo + "] vplayer.play_pause"
    Check "c17-transcript-line" ($logTxt.Contains($lineWant) -and
                                 $logTxt.Contains($krChang + " #" + $wcur)) ("log=[$logTail]")

    # resolve the parked ask (cross approve) + thumb cleanup
    $ap16 = (Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId16 + ',"decision":"allow"}}'))
    Check "c16-approve-ack" ($ap16 -match '"approved":true') $ap16
    $parked16 = ""
    foreach ($i in 1..50) {
        $f = Read-Frame $connC 400
        if ($f -and $f.type -eq 18 -and $f.text -match '"result"') { $parked16 = $f.text; break }
    }
    Check "c16-parked-result" ($parked16 -match '"ok":true') $parked16
    $connC.Dispose()
    if ($thumb -ne "") { Remove-Item $thumb -Force -ErrorAction SilentlyContinue }
    Clear-Perms

} finally {
    Clear-Perms
    if ($hadPerm) { Copy-Item $permBakFile $permFile -Force; Remove-Item $permBakFile -ErrorAction SilentlyContinue }
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-Process jkapp_vplayer -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-Process jkbridge -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-Process jkchat -ErrorAction SilentlyContinue | Stop-Process -Force
}

Write-Host ("RESULT: " + $(if ($script:fail -eq 0) { "ALL PASS" } else { "$($script:fail) FAILURE(S)" }))
exit $(if ($script:fail -eq 0) { 0 } else { 1 })