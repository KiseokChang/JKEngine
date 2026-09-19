# probe_filedlg_voice.ps1 - filedlg voice navigation e2e (spec
# 2026-09-19-filedlg-voice-nav section 9, 12 checks). ASCII-only (PS5.1).
# Server pattern = probe_app_tools.ps1 (own server + teardown, raw pipe frames,
# ProcessStartInfo agentctl idiom, PID-unique permissions backup + stale-residue
# guard, restore in finally).
#
# RUN PREREQUISITES (the probe FAILS FAST instead of colliding):
#   1. No live jkdesktop.exe / jkbridge.exe / jkwinserver.exe. The wire pipe
#      (JKWindowServerPipe) is a compile-time constant with no override - a
#      second server splits instances (multi-instance lesson), and this probe
#      kills only processes it spawned itself, so it refuses to run beside a
#      foreign desktop. A live desktop also file-locks jkdesktop.exe, which is
#      how the stale binary below happened in the first place.
#   2. Fresh jkdesktop.exe: libjkserver.a must not be newer than jkdesktop.exe
#      (jkdesktop --server embeds the server; a stale binary silently lacks
#      Tasks 1/2 - modal guard, wait:event, file.open_result. Caught live
#      2026-09-19: jkdesktop.exe 17:32 predated the Task 1 modal field).
#      Rebuild first: cmake --build build --target jkdesktop
#
# Checks (spec section 9, 12 checks; letters are sub-assertions; c5a/c5b run
# before c4 because c4 selects and the fresh-dialog boundary needs -1):
#   c1  file_open reply mode (raw pipe) -> dialog process spawns; c1b: the
#       parked query gets NO early reply (reply mode really parks)
#   c2  list_app_tools -> 3 filedlg rows, each "modal":true, names
#       navigate/list/choose, schemas embedded, title echo
#   c3  app_tool filedlg list -> entries/total/dir/selected(/file) + order
#   c5a fresh dialog, no selection: key:down lands on 0; c5b key:up clamps 0;
#       c5c down clamps at last index; c5d {} -> bad_args; c5e 99 -> bad_index
#   c4  navigate index:2/index:0 -> selected reflects + fileBuf mirror +
#       selectedName/selectedIsDir snapshot
#   c6  navigate key:parent -> dir ascends; choose <dir> -> descended:true
#   c7  choose a.txt -> app {"resolved":true,"path":..} AND the parked
#       reply-mode file_open resolves {"ok":true,"path":..} (reply-mode e2e);
#       dialog process exits; catalog rows back to 0
#   c8  c8a re-open wait:"event" -> IMMEDIATE parked ack; c8b choose b.txt ->
#       resolved + file.open_result event on the subscriber (ok+path) + ~1s
#       drain asserting NO late AgentReply for the parked qid (single
#       delivery, spec section 6); c8c broker e2e in ONE jkagentd stdio
#       session: tools/call file_open (broker injects wait:event) ->
#       tools/call read_events BEFORE choose (subscribes this connection -
#       the server pushes only to subscribers at push time and the broker
#       declares subscribe=0 until its first read_events - and drains the
#       queue) -> filedlg_choose -> tools/call read_events AGAIN and the
#       file.open_result event is asserted in THAT RESPONSE LINE (jkagentd
#       writes one stdout line per request and never streams events to
#       stdout); every broker ReadLine is deadline-bounded so a lost
#       response degrades to FAIL instead of wedging the probe
#   c9  c9a fresh dialog, tools live; c9b requester connection disposed while
#       the dialog lives -> app_tool answers tool_gone (modal guard, slot
#       truth source pendingFileDialog_.dialogConnId, spec section 5);
#       c9c kill the dialog process -> rows vanish -> unknown_app_tool
#   c10 manual `--filedlg "{}"` with no parked slot -> file_dialog_params
#       fails (no_pending_dialog) -> tools never registered (0 rows) +
#       unknown_app_tool (spec section 0 decision 2 side effect IS the guard)
#   c11 publish_event topic "file.open_result" -> reserved_topic;
#       c11b events_list catalog carries file.open_result (source server)
#   c12 jkagentd --selftest + jkdesktop test smoke + nested regression probes:
#       probe_app_tools.ps1 x2, probe_files.ps1, probe_agent_chat.ps1 (each
#       manages its own server lifecycle and permissions; run last - they
#       kill this probe's server too, same exclusive pipe by design). An
#       inventory re-check runs immediately BEFORE each nested run: the
#       nested probes kill jkdesktop/jkchat/jkbridge BY IMAGE, so a foreign
#       desktop/bridge that re-appeared mid-run would die indirectly - the
#       nested run is skipped + FAILed (fail fast) in that case.
#
# Wire facts asserted against (Tasks 1-4 as-built):
#   - file_open reply mode parks; AgentReply (type 18) {"ok":true,"path":..}
#   - wait:event acks {"ok":true,"parked":true} at once; resolution is ONE
#     file.open_result broadcast {"topic":"file.open_result","ok":true[,"path":..]}
#   - list_app_tools row: {"app","name","description","inputSchema","windowId",
#     "title","connId","modal":true|false}
#   - app_tool relay reply: {"ok":true,"windowId":N,"result":{<app json>}}
#     (filedlg always returns true - app errors ride INSIDE result); guard
#     failures are bare {"ok":false,"error":"tool_gone"|"unknown_app_tool"}
#   - Paths: the dialog NavigateTo lexically_normal()s the start dir, so dir/
#     path fields may come back with backslashes even when filed with slashes -
#     PathRx builds a separator-flexible regex everywhere.
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$agnt = "I:\progwork\JKENGINE\engine\build\jkagentd.exe"
$root = Split-Path $exe
$permFile = Join-Path $root "permissions.json"
$script:fail = 0
$script:myPids = @()     # every process this probe spawned (kill ONLY these)
$script:t0 = Get-Date

function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host ("PASS: " + $name) }
    else { $script:fail++; Write-Host ("FAIL: " + $name + " -- " + $detail) }
    if ($detail) { Write-Host ("      " + $detail) }
}

function Invoke-Agentctl([string]$json) {
    # PS5.1 native quoting trap (docs/55 lesson 3): build the raw command line
    # ourselves - CRT turns \" into a literal " for the agentctl argv.
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
# app_tool wrapper: windowId sits at the args level; the app payload is the
# nested "args" passthrough (server reads windowId at the args level).
function AppTool([string]$app, [string]$tool, [string]$argsJson, [int]$windowId = 0) {
    $a = '{"app":"' + $app + '","tool":"' + $tool + '"'
    if ($windowId -gt 0) { $a += ',"windowId":' + $windowId }
    if ($argsJson) { $a += ',"args":' + $argsJson }
    $a += '}'
    return (Invoke-Agentctl ('{"tool":"app_tool","args":' + $a + '}'))
}
# Separator-flexible regex for a fixture path (the dialog lexically_normal()s).
function PathRx([string]$p) {
    return ([regex]::Escape($p) -replace '/', '[/\\\\]')
}
# Deadline-bounded ReadLine for the c8c broker stdio session. PS5.1's
# synchronous ReadLine would block forever if jkagentd fails to answer a
# request (jkagentd writes one line per request and nothing else); polling
# the pending ReadLineAsync task keeps THIS thread free to check HasExited
# and the deadline, so a lost response degrades to a FAIL instead of
# wedging the probe (finally never reached, state left dirty).
function Read-BrokerLine([int]$timeoutMs) {
    if ($script:procM.HasExited) { return $null }
    $t = $script:procM.StandardOutput.ReadLineAsync()
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while (-not $t.IsCompleted) {
        if ($script:procM.HasExited) { return $null }
        if ($sw.ElapsedMilliseconds -gt $timeoutMs) {
            # Session is toast: kill the broker so no later ReadLineAsync
            # stacks onto the still-pending task (that would throw and
            # bypass finally).
            Stop-Process -Id $script:procM.Id -Force -ErrorAction SilentlyContinue
            return $null
        }
        Start-Sleep -Milliseconds 50
    }
    return $t.Result
}

# --- permissions.json is probe-owned state: stale-residue guard first (a
# killed earlier run must not have its backup adopted as user state), then a
# PID-unique backup, restored byte-identical in finally. Absent file = allow
# defaults for every tool this probe exercises (file_open gate is allow).
$stale = Get-ChildItem (Join-Path $env:TEMP "perm_pre_filedlgvoice_*.json") -ErrorAction SilentlyContinue
if ($stale) {
    Write-Host ("FAIL: setup-stale-residue -- " + (($stale | ForEach-Object { $_.Name }) -join ", "))
    Write-Host "      stale residue from a killed run - restore engine/build/permissions.json manually, delete the leftover(s) in TEMP, re-run"
    exit 1
}
$permBakFile = Join-Path $env:TEMP ("perm_pre_filedlgvoice_" + $PID + ".json")
$hadPerm = Test-Path $permFile
if ($hadPerm) { Copy-Item $permFile $permBakFile -Force }
function Set-Perms([string]$json) {
    [IO.File]::WriteAllText($permFile, $json, (New-Object System.Text.UTF8Encoding($false)))
}
function Clear-Perms { Remove-Item $permFile -ErrorAction SilentlyContinue }
Clear-Perms

# --- raw named pipe client (probe_app_tools idiom + deadline reads) ----------
function SendMsg([System.IO.Pipes.NamedPipeClientStream]$s, [int]$type, [byte[]]$payload) {
    $hdr = New-Object byte[] 12
    [BitConverter]::GetBytes([uint32]0x4A4B0001).CopyTo($hdr, 0)
    [BitConverter]::GetBytes([uint32]$type).CopyTo($hdr, 4)
    [BitConverter]::GetBytes([uint32]$payload.Length).CopyTo($hdr, 8)
    $s.Write($hdr, 0, 12)
    if ($payload.Length -gt 0) { $s.Write($payload, 0, $payload.Length) }
    $s.Flush()
}
# Second message must be AgentEventSubscribe (1 = subscriber / 0 = plain agent)
# or the server discards the connection ("expected CreateSurface").
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
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class PipePeekV {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool PeekNamedPipe(IntPtr hPipe, byte[] buf,
        int bufSize, out int read, out int avail, out int left);
}
"@
function Pipe-Avail([System.IO.Pipes.NamedPipeClientStream]$s) {
    $read = 0; $avail = 0; $left = 0
    try {
        $h = $s.SafePipeHandle.DangerousGetHandle()
        if (-not [PipePeekV]::PeekNamedPipe($h, $null, 0, [ref]$read, [ref]$avail, [ref]$left)) { return -1 }
    } catch { return -1 }
    return $avail
}
# Deadline-bounded frame read. Consumes header+payload whole and slices the
# type-specific header: AgentEvent 4 {jsonLen}, AgentQuery/AgentToolCall 8,
# AgentReply/AgentToolResult 12 {queryId, ok, jsonLen}. qid/ok are exposed so
# the caller can correlate parked replies (probe_app_tools slices them off).
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
    $qid = 0; $okf = 0
    if ($type -eq 18 -and $len -ge 12) {
        $qid = [int][BitConverter]::ToUInt32($pl, 0)
        $okf = [int][BitConverter]::ToUInt32($pl, 4)
    }
    $text = ""
    if ($len -gt $hs) { $text = [Text.Encoding]::UTF8.GetString($pl, $hs, $len - $hs) }
    return @{ type = $type; len = $len; qid = $qid; ok = $okf; text = $text }
}

# --- PID-scoped process helpers ----------------------------------------------
# The probe kills ONLY processes it spawned (or their descendants - the server
# spawns taskbar/filedlg). A foreign jkdesktop/jkbridge is refused at setup,
# never killed.
function Add-TreePids([int]$parentPid) {
    foreach ($c in (Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" -ErrorAction SilentlyContinue)) {
        if ($c.ParentProcessId -eq $parentPid -and $script:myPids -notcontains $c.ProcessId) {
            $script:myPids += $c.ProcessId
            Add-TreePids $c.ProcessId
        }
    }
}
# Poll for a file dialog child of the probe's server. Returns its PID (0 = none).
function Wait-DialogPid([int]$serverPid, [int]$loops) {
    foreach ($i in 1..$loops) {
        Start-Sleep -Milliseconds 400
        foreach ($c in (Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" -ErrorAction SilentlyContinue)) {
            if ($c.ParentProcessId -eq $serverPid -and $c.CommandLine -match '--filedlg') {
                if ($script:myPids -notcontains $c.ProcessId) { $script:myPids += $c.ProcessId }
                return $c.ProcessId
            }
        }
    }
    return 0
}
function Get-FileDlgRows {
    return (([regex]::Matches((Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'), '"app":"filedlg"')).Count)
}
function Wait-Rows([int]$want, [int]$loops) {
    $n = -1
    foreach ($i in 1..$loops) {
        $n = Get-FileDlgRows
        if ($n -eq $want) { return $true }
        Start-Sleep -Milliseconds 400
    }
    return ($n -eq $want)
}
# Kill every PID the probe recorded (server tree + dialogs + broker child).
function Stop-MyPids {
    foreach ($p2 in $script:myPids) {
        if ($p2 -gt 0) { Stop-Process -Id $p2 -Force -ErrorAction SilentlyContinue }
    }
}
# Mid-run foreign re-check (c12 nested runs): the nested probes kill
# jkdesktop/jkchat/jkbridge BY IMAGE, so a foreign desktop/bridge that
# re-appeared after the pre-spawn inventory would be killed indirectly. Own
# PIDs (this probe's server tree) are excluded. Returns $false and FAILs
# (fail fast) when anything foreign is alive - the caller skips the nested run.
function Test-ForeignFree([string]$tag) {
    $inv = @()
    foreach ($img in @("jkdesktop", "jkbridge", "jkwinserver")) {
        $inv += (Get-Process $img -ErrorAction SilentlyContinue |
                 Where-Object { $script:myPids -notcontains $_.Id } |
                 ForEach-Object { "$img(pid=$($_.Id))" })
    }
    if ($inv) {
        Check ("setup-foreign-midrun-" + $tag) $false ("foreign re-appeared before the nested run: " + ($inv -join ", ") + " - nested run SKIPPED")
        return $false
    }
    return $true
}

# --- pre-spawn inventory + foreign-server fail fast (task constraints) -------
$preInv = @()
foreach ($img in @("jkdesktop", "jkbridge", "jkwinserver")) {
    $preInv += (Get-Process $img -ErrorAction SilentlyContinue | ForEach-Object { "$img(pid=$($_.Id))" })
}
Write-Host ("PRE-SPAWN INVENTORY: " + $(if ($preInv) { $preInv -join ", " } else { "(none)" }))
if ($preInv) {
    Write-Host "FAIL: setup-foreign-instance -- live jkdesktop/jkbridge present (see inventory above)"
    Write-Host "      the wire pipe JKWindowServerPipe has no per-instance override - a second server splits instances (multi-instance lesson),"
    Write-Host "      and this probe must never kill processes it did not spawn. Close the live desktop/bridge session, then re-run."
    if ($hadPerm) { Copy-Item $permBakFile $permFile -Force; Remove-Item $permBakFile -ErrorAction SilentlyContinue }
    exit 1
}
# Behavioral double-check: nothing may answer the pipe before we spawn.
$probe = New-Object System.IO.Pipes.NamedPipeClientStream(".", "JKWindowServerPipe",
    [System.IO.Pipes.PipeDirection]::InOut)
$answered = $true
try { $probe.Connect(2000) } catch { $answered = $false }
$probe.Dispose()
if ($answered) {
    Write-Host "FAIL: setup-unknown-server -- something answers JKWindowServerPipe but no jkdesktop/jkbridge image is alive"
    Write-Host "      fail fast instead of colliding (task constraint 4) - identify the listener, then re-run"
    if ($hadPerm) { Copy-Item $permBakFile $permFile -Force; Remove-Item $permBakFile -ErrorAction SilentlyContinue }
    exit 1
}
# Stale-binary guard: jkdesktop --server embeds the server (libjkserver.a).
# Extended to the other Task 3/4 artifacts: jkagentd.exe links ONLY jkcore
# (no libjkagentd.a exists - engine/CMakeLists.txt target jkagentd ->
# target_link_libraries jkcore) and jkapp_filedlg.dll links jkclient
# (engine/CMakeLists.txt:548-554); a newer static lib means the binary was
# never relinked and silently lacks the new code.
if (-not (Test-Path (Join-Path $root "libjkserver.a")) -or -not (Test-Path $exe) -or -not (Test-Path $agnt) -or
    -not (Test-Path (Join-Path $root "jkapp_filedlg.dll")) -or
    -not (Test-Path (Join-Path $root "libjkcore.a")) -or
    -not (Test-Path (Join-Path $root "libjkclient.a"))) {
    Write-Host "FAIL: setup-missing-artifacts -- engine/build/jkdesktop.exe, jkagentd.exe, jkapp_filedlg.dll, libjkserver.a, libjkcore.a or libjkclient.a missing"
    if ($hadPerm) { Copy-Item $permBakFile $permFile -Force; Remove-Item $permBakFile -ErrorAction SilentlyContinue }
    exit 1
}
$stalePairs = @(
    @{ bin = "jkdesktop.exe";     lib = "libjkserver.a"; tgt = "jkdesktop" },
    @{ bin = "jkagentd.exe";      lib = "libjkcore.a";   tgt = "jkagentd" },
    @{ bin = "jkapp_filedlg.dll"; lib = "libjkclient.a"; tgt = "jkapp_filedlg" }
)
foreach ($sp in $stalePairs) {
    $binAge = (Get-Item (Join-Path $root $sp.bin)).LastWriteTime
    $libAge = (Get-Item (Join-Path $root $sp.lib)).LastWriteTime
    if ($libAge -gt $binAge) {
        Write-Host ("FAIL: setup-stale-binary -- " + $sp.lib + " (" + $libAge.ToString("HH:mm:ss") +
                    ") is newer than " + $sp.bin + " (" + $binAge.ToString("HH:mm:ss") + ")")
        Write-Host ("      the binary was not relinked after the lib was rebuilt; rebuild first: cmake --build build --target " + $sp.tgt)
        if ($hadPerm) { Copy-Item $permBakFile $permFile -Force; Remove-Item $permBakFile -ErrorAction SilentlyContinue }
        exit 1
    }
}

# --- fixture dir: entries resolve to [.., sub, a.txt, b.txt, c.txt] ----------
$base = "I:\progwork\JKENGINE\tmp\pfdv_$PID"
$startFwd = ("I:/progwork/JKENGINE/tmp/pfdv_" + $PID + "/start")
$startRx = PathRx $startFwd          # matches / or \ separators
New-Item -ItemType Directory -Path (Join-Path $base "start\sub") -Force | Out-Null
foreach ($f in @("a.txt", "b.txt", "c.txt")) {
    [IO.File]::WriteAllText((Join-Path $base ("start\" + $f)), "probe fixture $f",
        (New-Object System.Text.UTF8Encoding($false)))
}

# --- server lifecycle (own server only) ---------------------------------------
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden | Out-Null
Start-Sleep -Milliseconds 800
$serverPid = 0
foreach ($c in (Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" -ErrorAction SilentlyContinue)) {
    if ($c.CommandLine -match '--server') { $serverPid = $c.ProcessId; break }
}
$script:myPids += $serverPid
Add-TreePids $serverPid   # taskbar child (and any early spawns) are mine too
$up = $false
foreach ($i in 1..30) {
    Start-Sleep -Milliseconds 500
    if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up = $true; break }
}
Check "setup-server-up" ($up -and $serverPid -gt 0) ("serverPid=$serverPid")

try {
    # ---- c1: file_open reply mode (default) -> dialog spawn ------------------
    $connS = New-Pipe 1   # subscriber first (lesson 28) - carries parked queries + events
    SendQuery $connS 401 ('{"tool":"file_open","args":{"start":"' + $startFwd + '","title":"voice nav probe"}}')
    $dlgA = Wait-DialogPid $serverPid 30
    Check "c1-dialog-spawned" ($dlgA -gt 0) ("dialogPid=$dlgA")
    # c1b: reply mode parks - no AgentReply for qid 401 within ~2s (the dialog
    # registration may emit app_tools_changed events meanwhile - type 20, skipped).
    $early = $false
    foreach ($i in 1..10) {
        $f = Read-Frame $connS 200
        if ($f -and $f.type -eq 18 -and $f.qid -eq 401) { $early = $true; break }
    }
    Check "c1b-reply-mode-parks" (-not $early) "no early AgentReply for the parked query"

    # ---- c2: catalog - 3 filedlg rows, modal:true ----------------------------
    $rows3 = 0; $cat = ""
    foreach ($i in 1..30) {
        Start-Sleep -Milliseconds 400
        $cat = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
        $rows3 = ([regex]::Matches($cat, '"app":"filedlg"')).Count
        if ($rows3 -eq 3) { break }
    }
    Check "c2-three-rows" ($rows3 -eq 3) ("rows=$rows3")
    $fdRows = @($cat -split '\{"app":"' | Where-Object { $_ -match '^filedlg"' })
    $namesOk = ($fdRows.Count -eq 3 -and
                ($fdRows | Where-Object { $_ -match '"name":"navigate"' }).Count -eq 1 -and
                ($fdRows | Where-Object { $_ -match '"name":"list"' }).Count -eq 1 -and
                ($fdRows | Where-Object { $_ -match '"name":"choose"' }).Count -eq 1)
    Check "c2-row-names" $namesOk "navigate/list/choose rows"
    $modalOk = ($fdRows.Count -eq 3)
    foreach ($r in $fdRows) { if ($r -notmatch '"modal":true') { $modalOk = $false } }
    Check "c2-modal-true" $modalOk ($fdRows -join " | ")
    Check "c2-title-echo" ($cat -match '"app":"filedlg"[\s\S]*?"title":"voice nav probe"') ""
    $schemaOk = ($fdRows.Count -eq 3)
    foreach ($r in $fdRows) { if ($r -notmatch '"inputSchema":\{"type":"object"') { $schemaOk = $false } }
    Check "c2-schemas-embedded" $schemaOk ""

    # ---- c3: list -> entries/total/dir/selected ------------------------------
    $ls = (AppTool "filedlg" "list" '{"offset":0,"limit":50}')
    $lsOk = ($ls -match '"result":\{' -and $ls -match '"total":5' -and
             $ls -match '"entries":\[' -and $ls -match ('"dir":"' + $startRx + '"') -and
             $ls -match '"selected":-1' -and $ls -match '"file":""')
    Check "c3-list-fields" $lsOk $ls
    $names3 = [regex]::Matches($ls, '"name":"([^"]+)"') | ForEach-Object { $_.Groups[1].Value }
    $exp3 = @("..", "sub", "a.txt", "b.txt", "c.txt")
    $match3 = ($names3.Count -eq 5)
    if ($match3) { for ($i = 0; $i -lt 5; $i++) { if ($names3[$i] -ne $exp3[$i]) { $match3 = $false } } }
    Check "c3-entries-order" $match3 ($names3 -join ",")

    # ---- c5a: fresh dialog, no selection -> first down lands on 0 ------------
    $d1 = (AppTool "filedlg" "navigate" '{"key":"down"}')
    Check "c5a-down-from-none-lands-0" ($d1 -match '"index":0' -and $d1 -match '"selected":0' -and
                                        $d1 -match '"file":"\.\."' -and $d1 -match '"selectedName":"\.\."') $d1
    # ---- c5b: up clamps at 0 ---------------------------------------------------
    $d2 = (AppTool "filedlg" "navigate" '{"key":"up"}')
    Check "c5b-up-clamps-0" ($d2 -match '"index":0' -and $d2 -match '"file":"\.\."') $d2

    # ---- c4: navigate index -> selected reflects + file mirror ---------------
    $d3 = (AppTool "filedlg" "navigate" '{"index":2}')
    Check "c4-index2-selected" ($d3 -match '"index":2' -and $d3 -match '"selected":2' -and
                                $d3 -match '"file":"a.txt"' -and $d3 -match '"selectedName":"a.txt"' -and
                                $d3 -match '"selectedIsDir":false' -and $d3 -match ('"dir":"' + $startRx + '"')) $d3
    $d4 = (AppTool "filedlg" "navigate" '{"index":0}')
    Check "c4-index0-mirror" ($d4 -match '"selected":0' -and $d4 -match '"file":"\.\."') $d4
    [void](AppTool "filedlg" "navigate" '{"index":2}')   # park on a.txt for later

    # ---- c5c/d/e: bottom clamp + arg errors ----------------------------------
    $d5 = (AppTool "filedlg" "navigate" '{"index":4}')
    $d6 = (AppTool "filedlg" "navigate" '{"key":"down"}')
    Check "c5c-down-clamps-last" ($d5 -match '"selected":4' -and $d6 -match '"index":4' -and $d6 -match '"file":"c.txt"') "$d5 / $d6"
    $d7 = (AppTool "filedlg" "navigate" '{}')
    Check "c5d-bad-args" ($d7 -match '"result":\{"ok":true,"error":"bad_args"\}') $d7
    $d8 = (AppTool "filedlg" "navigate" '{"index":99}')
    Check "c5e-bad-index-count" ($d8 -match '"error":"bad_index"' -and $d8 -match '"count":5') $d8

    # ---- c6: parent ascends, choose of the dir descends back ------------------
    $p1 = (AppTool "filedlg" "navigate" '{"key":"parent"}')
    $p1Dir = ""
    $m = [regex]::Match($p1, '"dir":"([^"]*)"')
    if ($m.Success) { $p1Dir = $m.Groups[1].Value }
    $parentOk = ($p1Dir -match ('pfdv_' + $PID)) -and ($p1Dir -notmatch '[/\\]start$')
    Check "c6-parent-ascends" ($p1 -match '"ok":true' -and $parentOk) "dir=$p1Dir"
    $p2 = (AppTool "filedlg" "choose" '{"name":"start"}')
    Check "c6-choose-dir-descends" ($p2 -match '"descended":true' -and $p2 -match ('"dir":"' + $startRx + '"')) $p2

    # ---- c7: choose a file -> parked reply-mode file_open resolves e2e --------
    $ch = (AppTool "filedlg" "choose" '{"name":"a.txt"}')
    Check "c7-app-resolved" ($ch -match '"result":\{"ok":true,"resolved":true,"path":"[^"]*[/\\]start[/\\]a\.txt"\}') $ch
    $parked = ""
    foreach ($i in 1..100) {
        $f = Read-Frame $connS 200
        if ($f -and $f.type -eq 18 -and $f.qid -eq 401) { $parked = $f.text; break }
    }
    Check "c7-parked-reply-path" ($parked -match '"ok":true' -and $parked -match '"path":"[^"]*[/\\]start[/\\]a\.txt"') $parked
    $gone = $false
    foreach ($i in 1..20) {
        Start-Sleep -Milliseconds 400
        if (Get-Process -Id $dlgA -ErrorAction SilentlyContinue) { continue }
        $gone = $true; break
    }
    Check "c7-dialog-exited" $gone ("dialogPid=$dlgA")
    Check "c7-rows-vanish" (Wait-Rows 0 20) "filedlg rows back to 0 after resolve"

    # ---- c8a: wait:"event" -> immediate parked ack ----------------------------
    $sw8 = [System.Diagnostics.Stopwatch]::StartNew()
    SendQuery $connS 402 ('{"tool":"file_open","args":{"start":"' + $startFwd + '","wait":"event"}}')
    $ack = ""; $ackMs = -1
    foreach ($i in 1..50) {
        $f = Read-Frame $connS 200
        if ($f -and $f.type -eq 18 -and $f.qid -eq 402) { $ack = $f.text; $ackMs = [int]$sw8.ElapsedMilliseconds; break }
    }
    Check "c8a-immediate-parked-ack" ($ack -match '"ok":true,"parked":true' -and $ackMs -ge 0 -and $ackMs -lt 5000) ("$ack elapsed=${ackMs}ms")
    $dlgB = Wait-DialogPid $serverPid 30
    Check "c8a-dialog-B-spawned" ($dlgB -gt 0 -and $dlgB -ne $dlgA) ("dialogPid=$dlgB")
    Check "c8a-rows-3" (Wait-Rows 3 30) ""
    # ---- c8b: choose b.txt -> resolved + file.open_result event ---------------
    $ch8 = (AppTool "filedlg" "choose" '{"name":"b.txt"}')
    Check "c8b-choose-resolved" ($ch8 -match '"result":\{"ok":true,"resolved":true,"path":"[^"]*[/\\]start[/\\]b\.txt"\}') $ch8
    $evt = ""
    foreach ($i in 1..100) {
        $f = Read-Frame $connS 200
        if ($f -and $f.type -eq 20 -and $f.text -match '"topic":"file\.open_result"') { $evt = $f.text; break }
    }
    Check "c8b-file-open-result-event" ($evt -match '"ok":true' -and $evt -match '"path":"[^"]*[/\\]start[/\\]b\.txt"') $evt
    # Single-delivery invariant (spec section 6): the resolution is ONE
    # file.open_result broadcast - the parked wait:event query (qid 402) must
    # NOT also receive an AgentReply. Drain ~1s and assert absence.
    $dbl = $false
    $drainSw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($drainSw.ElapsedMilliseconds -lt 1100) {
        $f = Read-Frame $connS 150
        if ($f -and $f.type -eq 18 -and $f.qid -eq 402) { $dbl = $true; break }
    }
    Check "c8b-no-reply-double-delivery" (-not $dbl) "qid 402 must stay parked - event-only resolution"
    Check "c8b-dialog-B-gone" (-not (Get-Process -Id $dlgB -ErrorAction SilentlyContinue)) ("dialogPid=$dlgB")

    # ---- c8c: broker e2e in ONE jkagentd stdio session -------------------------
    # The broker injects wait:"event" into MCP file_open (Task 4), so the MCP
    # call returns parked at once. jkagentd stdout is ONE response line per
    # request (main.cpp getline -> HandleLine -> fputs, nothing else) and
    # events NEVER stream to stdout - they queue in the broker connection and
    # are drained ONLY by the read_events tool. The server broadcasts to
    # whoever is subscribed AT PUSH TIME (no server-side queueing for
    # non-subscribers) and the broker declares subscribe=0 at startup,
    # subscribing on its FIRST read_events - so read_events must be called
    # BEFORE choose (subscription preemption + queue drain), then AGAIN after
    # choose, and the file.open_result event is asserted in that RESPONSE
    # LINE's events array. No fixed sleeps; the dialog is polled via agentctl
    # between writes. Every ReadLine goes through Read-BrokerLine (deadline
    # bounded) so a missing answer can never wedge the probe.
    Clear-Perms   # broker gate reads the same file; absent = allow
    $psiM = New-Object System.Diagnostics.ProcessStartInfo
    $psiM.FileName = $agnt
    $psiM.UseShellExecute = $false
    $psiM.RedirectStandardInput = $true
    $psiM.RedirectStandardOutput = $true
    $psiM.CreateNoWindow = $true
    $procM = [System.Diagnostics.Process]::Start($psiM)
    $script:procM = $procM
    $script:myPids += $procM.Id
    $sin = $procM.StandardInput
    $sin.WriteLine('{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}')
    $initR = Read-BrokerLine 30000
    Check "c8c-broker-init" ($null -ne $initR -and $initR -match '"protocolVersion"') "$initR"
    $sin.WriteLine('{"jsonrpc":"2.0","method":"notifications/initialized"}')
    $sin.WriteLine('{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"file_open","arguments":{"start":"' + $startFwd + '"}}}')
    $foR = Read-BrokerLine 30000
    Check "c8c-mcp-file-open-parked" ($null -ne $foR -and $foR -match 'parked\\":true' -and $foR -match 'ok\\":true') "$foR"
    $dlgC = Wait-DialogPid $serverPid 30
    Check "c8c-dialog-C-spawned" ($dlgC -gt 0) ("dialogPid=$dlgC")
    Check "c8c-rows-3" (Wait-Rows 3 30) ""
    # (1) read_events BEFORE choose - subscribes this broker connection (the
    # server pushes only to push-time subscribers; subscribe=0 at startup,
    # first read_events subscribes) and drains whatever queued meanwhile.
    $sin.WriteLine('{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"read_events","arguments":{}}}')
    $subR = Read-BrokerLine 30000
    Check "c8c-read-events-subscribed" ($null -ne $subR -and $subR -match 'ok\\":true') "$subR"
    $sin.WriteLine('{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"filedlg_choose","arguments":{"name":"c.txt"}}}')
    $choR = Read-BrokerLine 30000
    Check "c8c-mcp-choose-resolved" ($null -ne $choR -and $choR -match 'resolved\\":true' -and $choR -match 'c\.txt') "$choR"
    # (2) read_events AFTER choose - file.open_result arrives in THIS response
    # line's events array, never on stdout by itself.
    $sin.WriteLine('{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"read_events","arguments":{}}}')
    $evR = Read-BrokerLine 30000
    Check "c8c-read-events-delivers" ($null -ne $evR -and $evR -match 'file\.open_result' -and $evR -match 'c\.txt') "$evR"
    $sin.Close()
    if (-not $procM.WaitForExit(30000)) { Stop-Process -Id $procM.Id -Force -ErrorAction SilentlyContinue }
    Check "c8c-dialog-C-gone" ($dlgC -gt 0 -and -not (Get-Process -Id $dlgC -ErrorAction SilentlyContinue)) ("dialogPid=$dlgC")
    Check "c8c-rows-vanish" (Wait-Rows 0 20) ""

    # ---- c9: modal guard (slot truth source, spec section 5) -------------------
    SendQuery $connS 403 ('{"tool":"file_open","args":{"start":"' + $startFwd + '","wait":"event"}}')
    $ack9 = ""
    foreach ($i in 1..50) {
        $f = Read-Frame $connS 200
        if ($f -and $f.type -eq 18 -and $f.qid -eq 403) { $ack9 = $f.text; break }
    }
    Check "c9a-parked-ack" ($ack9 -match '"ok":true,"parked":true') $ack9
    $dlgD = Wait-DialogPid $serverPid 30
    Check "c9a-dialog-D-spawned" ($dlgD -gt 0 -and $dlgD -ne $dlgC) ("dialogPid=$dlgD")
    Check "c9a-rows-3" (Wait-Rows 3 30) ""
    $live = (AppTool "filedlg" "list" '{"offset":0}')
    Check "c9a-tools-live" ($live -match '"result":\{"ok":true' -and $live -match '"total":5') $live
    # c9b: dispose the REQUESTER connection -> slot recycled (requesterConnId=0,
    # dialogConnId=0) while the dialog lives -> manifest connId no longer owned
    # -> modal guard answers tool_gone BEFORE any relay.
    $connS.Dispose()
    Start-Sleep -Milliseconds 1500
    $g = (AppTool "filedlg" "list" '{"offset":0}')
    Check "c9b-modal-guard-tool-gone" ($g -match '"ok":false,"error":"tool_gone"') $g
    # c9c: kill the dialog process -> manifest cleanup -> tools vanish.
    Stop-Process -Id $dlgD -Force -ErrorAction SilentlyContinue
    Check "c9c-rows-vanish" (Wait-Rows 0 20) ""
    $g2 = (AppTool "filedlg" "list" '{"offset":0}')
    Check "c9c-unknown-app-tool" ($g2 -match '"ok":false,"error":"unknown_app_tool"') $g2

    # ---- c10: orphan guard - manual spawn with no parked slot ------------------
    # Manual `--filedlg "{}"` never gets file_dialog_params (no pending slot ->
    # no_pending_dialog) -> toolsRegistered_ never fires -> 0 rows forever.
    $pMn = Start-Process -FilePath $exe -ArgumentList '--filedlg', '"{}"' -WorkingDirectory $root -WindowStyle Hidden -PassThru
    $script:myPids += $pMn.Id
    Start-Sleep -Seconds 6   # generous: params query + (never-firing) registration window
    Check "c10-rows-stay-0" ((Get-FileDlgRows) -eq 0) ("rows=" + (Get-FileDlgRows))
    $g3 = (AppTool "filedlg" "list" '{"offset":0}')
    Check "c10-unknown-app-tool" ($g3 -match '"ok":false,"error":"unknown_app_tool"') $g3
    Stop-Process -Id $pMn.Id -Force -ErrorAction SilentlyContinue

    # ---- c11: reserved topic gate + catalog ------------------------------------
    $pe = (Invoke-Agentctl '{"tool":"publish_event","args":{"topic":"file.open_result","data":{"ok":true,"path":"spoof.txt"}}}')
    Check "c11-reserved-topic" ($pe -match '"ok":false,"error":"reserved_topic"') $pe
    $el = (Invoke-Agentctl '{"tool":"events_list","args":{}}')
    Check "c11b-catalog-entry" ($el -match '"file\.open_result"' -and $el -match '"source":"server"') ($el.Substring(0, [Math]::Min(240, $el.Length)))

    # ---- c12: regressions -------------------------------------------------------
    Clear-Perms
    $selfOut = (& $agnt --selftest) 2>&1
    $selfTxt = (($selfOut | ForEach-Object { "$_" }) -join "`n")
    Check "c12a-jkagentd-selftest" ($LASTEXITCODE -eq 0 -and $selfTxt -match '(?m)^selftest: 0 failures\s*$') ($selfTxt.Substring(0, [Math]::Min(200, $selfTxt.Length)))
    $pT = Start-Process -FilePath $exe -ArgumentList "test" -WorkingDirectory $root -WindowStyle Hidden -PassThru
    $script:myPids += $pT.Id
    Start-Sleep -Seconds 6
    Check "c12b-jkdesktop-test-alive" (-not $pT.HasExited) $(if ($pT.HasExited) { "exit=" + $pT.ExitCode } else { "alive" })
    Stop-Process -Id $pT.Id -Force -ErrorAction SilentlyContinue
    # Nested regression probes - each manages its own server lifecycle, kills
    # its own tree, and restores its own permissions baseline. Run LAST: they
    # tear this probe's server down too (same exclusive pipe - by design).
    # Test-ForeignFree runs immediately before each run (see helper).
    foreach ($run in 1..2) {
        if (-not (Test-ForeignFree ("c12c-run" + $run))) { continue }
        $nestedLog = Join-Path $env:TEMP ("probe_filedlg_voice_nested_run" + $run + "_" + $PID + ".log")
        $no = (& powershell -NoProfile -ExecutionPolicy Bypass `
            -File "I:\progwork\JKENGINE\engine\tools\probes\probe_app_tools.ps1") 2>&1
        $no | Out-File -FilePath $nestedLog -Encoding ASCII
        $nOk = (($no | ForEach-Object { "$_" }) -join "`n") -match "RESULT: ALL PASS"
        Check ("c12c-probe-app-tools-run" + $run) $nOk ("log=$nestedLog")
    }
    # c12d: file hub regression (probe_files.ps1, own server + state backups).
    if (Test-ForeignFree "c12d") {
        $filesLog = Join-Path $env:TEMP ("probe_filedlg_voice_files_run_" + $PID + ".log")
        $no = (& powershell -NoProfile -ExecutionPolicy Bypass `
            -File "I:\progwork\JKENGINE\engine\tools\probes\probe_files.ps1") 2>&1
        $no | Out-File -FilePath $filesLog -Encoding ASCII
        $nOk = (($no | ForEach-Object { "$_" }) -join "`n") -match "PASS: file hub"
        Check "c12d-probe-files" $nOk ("log=$filesLog")
    }
    # c12e: chat approval pipeline regression (probe_agent_chat.ps1, own
    # server + jkchat/minesweeper windows spawned and reaped by the probe).
    if (Test-ForeignFree "c12e") {
        $chatLog = Join-Path $env:TEMP ("probe_filedlg_voice_chat_run_" + $PID + ".log")
        $no = (& powershell -NoProfile -ExecutionPolicy Bypass `
            -File "I:\progwork\JKENGINE\engine\tools\probes\probe_agent_chat.ps1") 2>&1
        $no | Out-File -FilePath $chatLog -Encoding ASCII
        $nOk = (($no | ForEach-Object { "$_" }) -join "`n") -match "PASS: agent chat"
        Check "c12e-probe-agent-chat" $nOk ("log=$chatLog")
    }

} finally {
    Clear-Perms
    if ($hadPerm) { Copy-Item $permBakFile $permFile -Force; Remove-Item $permBakFile -ErrorAction SilentlyContinue }
    Stop-MyPids
    # Safety net for probe-spawned strays ONLY - the StartTime filter keeps any
    # pre-existing (foreign) process untouched even if it appeared mid-run.
    foreach ($img in @("jkbridge", "jkchat", "jkapp_vplayer")) {
        Get-Process $img -ErrorAction SilentlyContinue |
            Where-Object { $_.StartTime -ge $script:t0 } | Stop-Process -Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Milliseconds 500
    Remove-Item $base -Recurse -Force -ErrorAction SilentlyContinue
    if ($connS) { $connS.Dispose() }
}

Write-Host ("RESULT: " + $(if ($script:fail -eq 0) { "ALL PASS" } else { "$($script:fail) FAILURE(S)" }))
exit $(if ($script:fail -eq 0) { 0 } else { 1 })
