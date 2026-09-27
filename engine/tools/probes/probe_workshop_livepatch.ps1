# probe_workshop_livepatch.ps1 - set_script live:1 e2e (spec
# docs/superpowers/specs/2026-09-27-workshop-livepatch-design.md section 6,
# gates 1-5). ASCII-only (PS5.1). Harness = probe_workshop.ps1 (client spawn,
# agentctl idiom, log asserts, probe-ws slot isolation, finally pointer
# restore) with the live-patch scenarios.
#
# Contracts consumed (Task 1 edb0690 + Task 2 2ee0b19, measured in
# adhoc_livepatch_trace before writing this probe):
#   live success        -> {"ok":true,"live":true,"slot","gen":N}
#                          + client log "[script] live patch: gen N"
#   compile-gate fail   -> {"ok":false,"live":true,"error",hint} - no file
#                          write, context unharmed
#   runtime-exception   -> {"ok":true,"live":false,"error",note:"recovered
#   fall                   by full reload"} - the app re-evals the FILE
#   (widget creation during patch = bad_patch TypeError -> the same fall,
#   with the "live patch" education text in the error echo)
#
# Scenario 4 notes (brief-vs-implementation, measured):
#  a) a patch whose source throws UNCONDITIONALLY ("throw new Error('patch-
#     time boom')") also throws at the fall reload (JKScriptHost::Start
#     re-evals the file -> evalFailed -> Stop -> dead host -> {"ok":false,
#     "live":true}). The clean fall contract spec section 4 defines (and
#     gate 4 asserts) needs an error that is live-context-only - so the
#     throw is guarded by the seed's liveMark (present in the live context,
#     absent in the fresh reload). The exception is real and fires at patch
#     time; the fall then recovers.
#  b) the throw comes BEFORE the source's createButton: official run 1
#     measured a createButton-first source tripping the CREATION gate first
#     (the response error was the gate text, not the boom) - gate 4 wants
#     its own runtime-exception echo. At the fresh reload the guarded
#     throw is skipped and the file's createButton runs (legal there).
#  c) onRestoreState receives the saved RAW STRING (jk.d.ts v7 - the host's
#     DispatchRestoreState passes JS_NewStringLen of the snapshot): the
#     hooks JSON.parse it. Run 1's implicit `n = s.n` read the string
#     property (undefined) - measured: restored3 s={"n":25} typeofN=undef.
#
# Scenarios:
#   s1  server up (probe-owned lifecycle: fresh --server)
#   s2  spawn workshop.jkx -> 7 tools + set_script schema has live:number
#       + the api tool carries the "patch" field (education surface)
#   s3  seed (full path, live:0) -> ok:true, live:false + timer warm (log n=)
#   c1  live patch success: onClick redefined only -> ok:true, live:true,
#       gen + "[script] live patch: gen" log + the timer counter CONTINUES
#       across the patch boundary (first post-patch n= > pre-patch n, no
#       reset) + a real mouse click runs the NEW definition (log pc=) with
#       the counter value intact (state survival, spec section 6 gate 1)
#   c2  syntax fail: live:1 with a syntax error -> ok:false, live:true +
#       error echo + hint; NO file write (get_script + the slot file still
#       hold the c1 source); the next click still logs pc= (old definition
#       still bound - context unharmed, spec section 6 gate 2)
#   c3  widget-creation violation: live:1 with createButton at top level ->
#       the bad_patch education text ("live patch") in the fall response +
#       onRestoreState transfer log + the reloaded app answers clicks
#       (spec section 6 gate 3; the no-duplicate-creation half is covered
#       by the workshop_slot_probe unit checks f1-f26 - e2e observes the
#       fall contract + the error echo)
#   c4  runtime-exception fall: live:1 throw (live-context-gated) -> ok:true,
#       live:false + "patch-time boom" error echo + note + the state
#       transfer log + the app alive (spec section 6 gate 4)
#   c5  phone path regression: the bridge WS relay (generic, no bridge
#       changes) forwards set_script live:1 -> ok:true, live:true + the
#       live patch log line + a click on the new behavior (spec section 6
#       gate 5)
#   c6  cleanup: close_window -> catalog rows 0; truth source restored;
#       probe slot + history wiped; .current pointer + bridge state files
#       byte-restored in finally
# SLOT ISOLATION: everything happens in the probe-owned probe-ws slot; the
# user's real slot and .current pointer are untouched (restored in
# finally). permissions.json is NEVER touched (docs/59 s16.1) - the relay
# needs no key (app_tool defaults to allow) and send_input rides the user's
# live all-allow file. state/jkbridge.json + state/chat.json ARE swapped for
# the c5 relay window (probe_phone_mirror.ps1 convention: byte backup +
# finally restore) - the live bridge process is dead by then (probe-owned
# lifecycle), so the swap never races a live bridge.
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root = Split-Path $exe
$jkx = Join-Path $root "apps\workshop.jkx"
$slotFile = Join-Path $root "state\scripts\probe-ws.js"          # probe-owned slot
$histDir  = Join-Path $root "state\scripts\.history\probe-ws"   # probe-owned history
$currentFile = Join-Path $root "state\scripts\.current_workshop" # last-slot pointer
$hadCur = Test-Path $currentFile
if ($hadCur) { $curBak = [System.IO.File]::ReadAllBytes($currentFile) }
$clientLog = Join-Path $root "workshop_client.log"
$stateDir = Join-Path $root "state"
$jbFile = Join-Path $stateDir "jkbridge.json"
$chatFile = Join-Path $stateDir "chat.json"
$hadJb = Test-Path $jbFile;  if ($hadJb) { $jbBak = [System.IO.File]::ReadAllBytes($jbFile) }
$hadChat = Test-Path $chatFile; if ($hadChat) { $chatBak = [System.IO.File]::ReadAllBytes($chatFile) }
$run = Join-Path ([System.IO.Path]::GetTempPath()) ("jkbridge_livepatch_" + $PID)
$script:fail = 0

function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host ("PASS: " + $name) }
    else { $script:fail++; Write-Host ("FAIL: " + $name + " -- " + $detail) }
    if ($detail) { Write-Host ("      " + $detail) }
}

function Invoke-Agentctl([string]$json) {
    # PS5.1 native quoting trap (docs/55 lesson 3): build the raw command line.
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
function AppTool([string]$tool, [string]$argsJson) {
    $a = '{"tool":"app_tool","args":{"app":"workshop","tool":"' + $tool + '"'
    if ($argsJson) { $a += ',"args":' + $argsJson }
    $a += '}}'
    return (Invoke-Agentctl $a)
}
# set_script payload: single-line JS (no newline escaping) with " escaped.
function SetScript([string]$js) {
    $esc = $js -replace '"', '\"'
    return (AppTool "set_script" ('{"source":"' + $esc + '"}'))
}
function SetScriptLive([string]$js) {
    $esc = $js -replace '"', '\"'
    return (AppTool "set_script" ('{"source":"' + $esc + '","live":1}'))
}
# set_script into a named slot (creates it + auto-switches - docs/67 stage 1).
function SetScriptSlot([string]$slot, [string]$js) {
    $esc = $js -replace '"', '\"'
    return (AppTool "set_script" ('{"slot":"' + $slot + '","source":"' + $esc + '"}'))
}
function Catalog {
    return (Invoke-Agentctl '{"tool":"list_app_tools","args":{}}')
}
# Parsed app_tool result: the agentctl envelope carries the tool result in
# "result" - a raw '"ok":true' regex would hit the envelope, not the
# contract; parse, don't regex (docs/59 s12 lesson). A FAILING tool's result
# arrives under "error" instead (measured: {"ok":true,"windowId":4,
# "error":{"ok":false,"live":true,...}}) - unwrap it the same way.
function Parse-Result([string]$raw) {
    try {
        $o = $raw | ConvertFrom-Json
        if ($o.result -ne $null) { return $o.result }
        if ($o.error -ne $null -and $o.error -isnot [string]) { return $o.error }
    } catch { }
    return $null
}
# client log: rolling offset (the log is ASCII - chars equal bytes)
$script:logPos = 0
function Log-New {
    if (-not (Test-Path $clientLog)) { return "" }
    $raw = Get-Content $clientLog -Raw -ErrorAction SilentlyContinue
    if ($raw -eq $null) { return "" }
    if ($script:logPos -ge $raw.Length) { return "" }
    $t = $raw.Substring($script:logPos)
    $script:logPos = $raw.Length
    return $t
}
# poll Log-New for a needle within ms
function Wait-Log([string]$needle, [int]$ms) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $ms) {
        $t = Log-New
        if ($t -match $needle) { return $t }
        Start-Sleep -Milliseconds 200
    }
    return $null
}
# workshop window (main window - the panel is a child control, stable across
# fall reloads; measured windowId stays constant across falls); click target
# = the script's button SURFACE center with the measured panel offset
# (probe_conquest_workshop rung 6 diag capture: a widget declared at (20,60)
# renders at surface (22..172, 108..148) - so this probe's button declared at
# (10,10,140,26) renders at surface ~(12..152, 58..84), center (82,71); the
# declared center (80,23) hits the app chrome dead space - measured in
# adhoc2: (82,71) logs s0c= on the first try, (80,23) logs nothing).
# Mapped through the display ratio (probe_phone_mirror c9 identity: desktop
# point = window.x + fx*dw with fx the surface fraction). Retries up to 4
# clicks (a fall reload may still be rebuilding the panel).
function Click-Workshop([string]$needle, [int]$ms) {
    foreach ($attempt in 1..4) {
        $lw = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
        $wid = 0; $tapX = 0; $tapY = 0
        try {
            $o = $lw | ConvertFrom-Json
            foreach ($w in $o.windows) {
                if ($w.title -eq 'Workshop') {
                    $wid = $w.id
                    $tapX = $w.x + [int][Math]::Floor($w.dw * 82.0 / $w.w)
                    $tapY = $w.y + [int][Math]::Floor($w.dh * 71.0 / $w.h)
                }
            }
        } catch { }
        if ($wid -gt 0) {
            [void](Invoke-Agentctl ('{"tool":"send_input","args":{"id":' + $wid +
                ',"op":"click","x":' + $tapX + ',"y":' + $tapY + '}}'))
            $t = Wait-Log $needle $ms
            if ($t -ne $null) { return $t }
        }
        Start-Sleep -Milliseconds 500
    }
    return $null
}

# --- server lifecycle ---------------------------------------------------------
Write-Output ("NOTICE: probe start " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss") +
              " - the live desktop stack is stopped for the probe run")
Get-Process jkdesktop, jkwinserver, jkagentd, jkbridge -ErrorAction SilentlyContinue |
    Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
$up = $false
foreach ($i in 1..30) {
    Start-Sleep -Milliseconds 500
    if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up = $true; break }
}
Check "s1-server-up" $up ""

try {
    # ---- spawn workshop client (stdout -> log file for log assertions) ------
    if (Test-Path $clientLog) { Remove-Item $clientLog -Force }
    # log staleness guard, docs/61 - half 1: the log from an earlier run is
    # GONE before the spawn (asserted; any log read later is this run's).
    Check "s2-log-absent-before-spawn" (-not (Test-Path $clientLog)) "client log survived the pre-spawn delete"
    $probeStart = Get-Date
    $bat = Join-Path $env:TEMP ("wslp_" + $PID + ".cmd")
    ("@echo off`r`ncd /d `"" + $root + "`"`r`n`"" + $exe + "`" --jkx `"" + $jkx + "`" > `"" + $clientLog + "`" 2>&1`r`n") |
        Set-Content -Path $bat -Encoding ASCII
    Start-Process -FilePath $bat -WindowStyle Hidden
    $cat = ""
    $rows = 0
    foreach ($i in 1..30) {
        Start-Sleep -Milliseconds 500
        $cat = Catalog
        $rows = [regex]::Matches($cat, '"app":"workshop"').Count
        if ($rows -eq 7) { break }
    }
    Check "s2-spawn-7-rows" ($rows -eq 7) ("rows=$rows")
    $namesOk = ($cat -match '"app":"workshop","name":"get_script"') -and
               ($cat -match '"app":"workshop","name":"set_script"') -and
               ($cat -match '"app":"workshop","name":"api"')
    Check "s2-catalog-names" $namesOk ""
    # set_script schema gained the live:number arg (Task 2, additive). Row
    # extraction by IndexOf (shape-independent - no ConvertFrom-Json guess):
    # slice from the set_script row to the next "app":" row, match inside.
    $schemaOk = $false
    $schemaDetail = "set_script row not found"
    $ssStart = $cat.IndexOf('"name":"set_script"')
    if ($ssStart -ge 0) {
        $ssEnd = $cat.IndexOf('"app":"', $ssStart)
        if ($ssEnd -lt 0) { $ssEnd = $cat.Length }
        if ($ssEnd -gt $ssStart) {
            $row = $cat.Substring($ssStart, $ssEnd - $ssStart)
            if ($row -match '"live":\{"type":"number"\}') {
                $schemaOk = $true; $schemaDetail = "live:number present"
            } else { $schemaDetail = $row.Substring(0, [Math]::Min(240, $row.Length)) }
        }
    }
    Check "s2-set-script-schema-live" $schemaOk ("live.type=" + $schemaDetail)
    # api tool: the patch education field (the closed-loop surface, docs/67)
    $api = (AppTool "api" "")
    Check "s2-api-patch-field" ($api -match '"patch"') ""

    # ---- s3: seed the probe-owned slot (full path, live:0) ---------------------
    # n = the timer counter (the state-survival witness); liveMark = a live
    # context marker (set by the seed, absent after a fresh file reload - the
    # c4 throw is guarded by it, see the header note); onSaveState/
    # onRestoreState = the state-transfer observers for the fall paths.
    if (Test-Path $slotFile) { Remove-Item $slotFile -Force }
    if (Test-Path $histDir) { Remove-Item -Recurse -Force $histDir }
    # onRestoreState contract (jk.d.ts v7): saved = the SAVED RAW STRING (the
    # host's DispatchRestoreState passes JS_NewStringLen of the snapshot) -
    # JSON.parse inside the hook; `n = s.n` on the string reads undefined
    # (measured in adhoc2: restored3 s={"n":25} typeofN=undefined).
    $seed = "globalThis.n = 0; globalThis.liveMark = 1; setInterval(function(){ n = n + 1; log('n=' + n); }, 250); function onSaveState(){ return { n: (typeof n === 'number' ? n : 0) }; } function onRestoreState(s){ var st = JSON.parse(s); n = st.n; log('restored=' + JSON.stringify(st)); } function onClick(id){ setText(id, 'count=' + n); log('s0c=' + n); } var b = createButton({ x: 10, y: 10, w: 140, h: 26 }, 'c0');"
    $r0 = (SetScriptSlot "probe-ws" $seed)
    $r0res = Parse-Result $r0
    $r0Ok = ($r0res -ne $null -and $r0res.ok -eq $true -and
             $r0res.'live' -eq $false -and ($r0res.gen -is [int]))
    $r0s = $r0
    if ($r0s.Length -gt 300) { $r0s = $r0s.Substring(0, 300) }
    Check "s3-probe-slot-seed" $r0Ok $r0s

    # timer warm-up: the counter must be ticking (lastPre >= 4) before patching
    $lastPre = 0
    foreach ($i in 1..50) {
        Start-Sleep -Milliseconds 300
        $raw = ""
        if (Test-Path $clientLog) { $raw = Get-Content $clientLog -Raw -ErrorAction SilentlyContinue }
        $vals = [regex]::Matches($raw, '\[script\] n=([0-9]+)')
        if ($vals.Count -gt 0) { $lastPre = [int]$vals[$vals.Count - 1].Groups[1].Value }
        if ($lastPre -ge 4) { break }
    }
    Check "s3-timer-warm" ($lastPre -ge 4) ("lastPre=" + $lastPre)
    $script:logPos = 0
    if (Test-Path $clientLog) { $script:logPos = (Get-Content $clientLog -Raw).Length }
    # log staleness guard, docs/61: the log was DELETED pre-spawn (asserted
    # at s2-log-absent) - existence + non-empty content here IS the
    # freshness stamp. (Measured run 2: a timestamp comparison is NOT
    # usable - NTFS same-name tunneling gives the recreated file the OLD
    # file's CreationTime (2026-09-20 14:29:43 held through a delete +
    # recreate a week later), and LastWriteTime under the client's open
    # handle reads the stale committed value; stat post-close shows the
    # true 21:14:38.)
    $logFresh = $false
    $logDetail = "log missing after the spawn"
    if (Test-Path $clientLog) {
        $logFresh = ((Get-Item $clientLog).Length -gt 0)
        $logDetail = ("recreated by this run's spawn; length=" + (Get-Item $clientLog).Length)
    }
    Check "s3-log-fresh" $logFresh $logDetail

    # ---- c1: live patch success (timer counter survives) -----------------------
    # The patch replaces onClick ONLY (no creation, no re-arm, no n reset) -
    # the seed's timer keeps firing and the global n keeps counting.
    $p1 = "function onClick(id){ setText(id, 'patched:' + n); log('pc=' + n); }"
    $r1 = (SetScriptLive $p1)
    $r1res = Parse-Result $r1
    $r1Ok = ($r1res -ne $null -and $r1res.ok -eq $true -and
             $r1res.'live' -eq $true -and ($r1res.gen -is [int]) -and
             $r1res.slot -eq 'probe-ws')
    $r1s = $r1
    if ($r1s.Length -gt 300) { $r1s = $r1s.Substring(0, 300) }
    Check "c1-live-response" $r1Ok $r1s
    $lpLog = Wait-Log '\[script\] live patch: gen [0-9]+' 5000
    Check "c1-live-patch-log" ($lpLog -ne $null) ("matched=" + $lpLog)
    # counter continues: the FIRST post-patch n= must exceed the pre-patch
    # value (a full reload would restart at 1 <= lastPre)
    $c1v = 0
    foreach ($i in 1..25) {
        $t = Log-New
        foreach ($m in [regex]::Matches($t, 'n=([0-9]+)')) { $c1v = [int]$m.Groups[1].Value }
        if ($c1v -gt $lastPre) { break }
        Start-Sleep -Milliseconds 300
    }
    Check "c1-counter-continues" ($c1v -gt $lastPre) ("first post-patch n=" + $c1v + " pre=" + $lastPre)
    # a real mouse click runs the NEW definition (pc= line) with the counter
    # value carried (the OLD definition logged s0c=)
    $pcLog = Click-Workshop 'pc=([0-9]+)' 1500
    $pcOk = $false; $pcV = 0
    if ($pcLog -ne $null) {
        $m = [regex]::Match($pcLog, 'pc=([0-9]+)')
        if ($m.Success) { $pcV = [int]$m.Groups[1].Value; $pcOk = ($pcV -gt $lastPre) }
    }
    Check "c1-click-new-behavior" $pcOk ("pc=" + $pcV + " pre=" + $lastPre + " tail=" + $pcLog)

    # ---- c2: syntax fail = no damage -------------------------------------------
    # Truncated paren = QuickJS syntax error at the COMPILE gate. The gate
    # fails BEFORE the snapshot/write - file + context unharmed, and the
    # response must carry the error echo + the retry hint (the closed loop).
    $p2 = "function onClick(id { setText(id, 'count=' + n); }"
    $r2 = (SetScriptLive $p2)
    $r2res = Parse-Result $r2
    $r2Ok = ($r2res -ne $null -and $r2res.ok -eq $false -and
             $r2res.'live' -eq $true -and
             ($r2res.error -ne $null -and $r2res.error.Length -gt 0) -and
             ($r2res.hint -ne $null -and $r2res.hint -match 'retry live:1'))
    $r2s = $r2
    if ($r2s.Length -gt 400) { $r2s = $r2s.Substring(0, 400) }
    Check "c2-gate-fail-response" $r2Ok $r2s
    # truth source untouched: get_script AND the slot file still hold c1's source
    $g2 = (AppTool "get_script" "")
    $g2res = Parse-Result $g2
    $g2Ok = ($g2res -ne $null -and $g2res.ok -eq $true -and
             $g2res.source -match 'patched:')
    $g2s = $g2
    if ($g2s.Length -gt 200) { $g2s = $g2s.Substring(0, 200) }
    Check "c2-file-untouched-get" $g2Ok $g2s
    $disk2 = ""
    if (Test-Path $slotFile) { $disk2 = [IO.File]::ReadAllText($slotFile) }
    Check "c2-file-untouched-disk" ($disk2 -match 'patched:') ("disk bytes=" + $disk2.Length)
    # context alive: the click path still runs the OLD (c1) definition
    $pc2 = Click-Workshop 'pc=([0-9]+)' 1500
    $pc2Ok = $false; $pc2V = 0
    if ($pc2 -ne $null) {
        $m2 = [regex]::Match($pc2, 'pc=([0-9]+)')
        if ($m2.Success) { $pc2V = [int]$m2.Groups[1].Value; $pc2Ok = ($pc2V -gt $lastPre) }
    }
    Check "c2-click-still-old" $pc2Ok ("pc=" + $pc2V + " pre=" + $lastPre + " tail=" + $pc2)

    # ---- c3: widget creation during patch = bad_patch fall ----------------------
    # The patch source carries the state-transfer hooks so the fall reload
    # (which re-evals the FILE) keeps the counter: onSaveState is captured at
    # teardown, onRestoreState re-binds n in the reloaded context. The file's
    # createButton is LEGAL at the full reload (creation is only gated during
    # a patch) - so the reloaded app has its own button at the same rect.
    $p3 = "globalThis.liveMark = 2; function onSaveState(){ return { n: (typeof n === 'number' ? n : 0) }; } function onRestoreState(s){ var st = JSON.parse(s); n = st.n; log('restored=' + JSON.stringify(st)); } function onClick(id){ setText(id, 'count=' + n); log('s3c=' + n); } createButton({ x: 10, y: 10, w: 140, h: 26 }, 'c3b');"
    [void](Log-New)   # drain the log tail - only post-call lines are asserted
    $r3 = (SetScriptLive $p3)
    $r3res = Parse-Result $r3
    $r3Ok = ($r3res -ne $null -and $r3res.ok -eq $true -and
             $r3res.'live' -eq $false -and
             ($r3res.error -match 'live patch') -and
             ($r3res.note -eq 'recovered by full reload') -and
             ($r3res.gen -is [int]))
    $r3s = $r3
    if ($r3s.Length -gt 400) { $r3s = $r3s.Substring(0, 400) }
    Check "c3-badpatch-fall-response" $r3Ok $r3s
    # the fall is a real reload: onRestoreState ran (state carried to the file)
    $rest3 = Wait-Log 'restored=' 5000
    Check "c3-fall-restored" ($rest3 -ne $null) ("matched=" + $rest3)
    # the reloaded app answers clicks (the file's own widget set, 1 button)
    $s3log = Click-Workshop 's3c=([0-9]+)' 1500
    $s3Ok = $false; $s3v = 0
    if ($s3log -ne $null) {
        $m3 = [regex]::Match($s3log, 's3c=([0-9]+)')
        if ($m3.Success) { $s3v = [int]$m3.Groups[1].Value; $s3Ok = ($s3v -ge $lastPre) }
    }
    Check "c3-click-alive" $s3Ok ("s3c=" + $s3v + " pre=" + $lastPre + " tail=" + $s3log)

    # ---- c4: runtime exception at patch time -> fall contract -------------------
    # The throw fires ONLY in the live context (liveMark exists) - the fall
    # reload re-evals the same file fresh, where liveMark is undefined, so
    # the file loads clean and the app recovers (see the header note).
    # The throw comes BEFORE createButton: measured in adhoc2, a
    # createButton-first source trips the CREATION gate first (the response
    # error is the gate text, not the boom) - rung 1 official run defect.
    # Throwing first gives gate 4 its own error echo; at the fresh reload
    # the guarded throw is skipped (liveMark absent) and the file's own
    # createButton runs (legal outside a patch).
    $p4 = "function onSaveState(){ return { n: (typeof n === 'number' ? n : 0) }; } function onRestoreState(s){ var st = JSON.parse(s); n = st.n; log('restored=' + JSON.stringify(st)); } function onClick(id){ setText(id, 'ok'); log('s4c=' + n); } if (typeof liveMark !== 'undefined') { throw new Error('patch-time boom'); } createButton({ x: 10, y: 10, w: 140, h: 26 }, 'c4b');"
    $r4 = (SetScriptLive $p4)
    $r4res = Parse-Result $r4
    $r4Ok = ($r4res -ne $null -and $r4res.ok -eq $true -and
             $r4res.'live' -eq $false -and
             ($r4res.error -match 'patch-time boom') -and
             ($r4res.note -eq 'recovered by full reload') -and
             ($r4res.gen -is [int]))
    $r4s = $r4
    if ($r4s.Length -gt 400) { $r4s = $r4s.Substring(0, 400) }
    Check "c4-boom-fall-response" $r4Ok $r4s
    $rest4 = Wait-Log 'restored=' 5000
    Check "c4-fall-restored" ($rest4 -ne $null) ("matched=" + $rest4)
    $s4log = Click-Workshop 's4c=([0-9]+)' 1500
    $s4Ok = $false; $s4V = 0
    if ($s4log -ne $null) {
        $m4 = [regex]::Match($s4log, 's4c=([0-9]+)')
        if ($m4.Success) { $s4V = [int]$m4.Groups[1].Value; $s4Ok = ($s4V -ge $lastPre) }
    }
    Check "c4-click-alive" $s4Ok ("s4c=" + $s4V + " pre=" + $lastPre + " tail=" + $s4log)

    # ---- c5: phone path regression (bridge WS relay) -----------------------------
    # The bridge is a generic relay (zero bridge changes in this task): the
    # probe swaps the bridge state to its own token/port (byte-restore in
    # finally), starts jkbridge.exe, connects as the phone web UI does, and
    # sends one set_script live:1 through the SAME tool frame the phone
    # sends. The WS session is closed right after - it holds the agent slot,
    # so the follow-up agentctl checks must come after the close
    # (probe_phone_mirror c10 lesson).
    '{"token":"probetoken0123456789abcdef","port":8899}' |
        Set-Content -Path $jbFile -Encoding ASCII
    '{"engine":"stub"}' | Set-Content -Path $chatFile -Encoding ASCII
    if (Test-Path $run) { Remove-Item -Recurse -Force $run }
    New-Item -ItemType Directory -Path $run -Force | Out-Null
    Start-Process -FilePath (Join-Path $root "jkbridge.exe") `
        -WorkingDirectory $root -RedirectStandardOutput (Join-Path $run "bridge.log") `
        -RedirectStandardError (Join-Path $run "bridge_err.log") `
        -WindowStyle Hidden
    Start-Sleep -Seconds 2
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
    $c1 = New-Sock "127.0.0.1" 8899
    $hs = WsHandshake $c1 "probetoken0123456789abcdef"
    Check "c5-ws-handshake" ($hs -match "101") ""
    WsSend $c1 '{"type":"hello"}'
    $hello = $null
    foreach ($i in 1..10) { $r = WsRecv $c1 3000; if ($r -match '"type":"hello"') { $hello = $r; break } }
    Check "c5-hello-ok" ($hello -ne $null -and $hello -match '"ok"\s*:\s*(1|true)') "$hello"
    # one set_script live:1 through the generic relay (the phone's exact frame)
    $p5 = "function onClick(id){ setText(id, 'phone:' + n); log('ph=' + n); }"
    $esc5 = $p5 -replace '"', '\"'
    WsSend $c1 ('{"type":"tool","tool":"app_tool","args":{"app":"workshop","tool":"set_script","args":{"source":"' +
        $esc5 + '","live":1}},"label":"lpset"}')
    $lp = $null
    foreach ($i in 1..20) { $r = WsRecv $c1 3000; if ($r -match '"label":"lpset"') { $lp = $r; break } }
    $lpOk = $false
    if ($lp -ne $null) {
        try {
            # the relay's "json" = the agentctl envelope - the tool contract
            # rides its "result" (measured run 1: {"ts":...,"type":"reply",
            # "label":"lpset","json":{"ok":true,"windowId":4,"result":{
            # "ok":true,"live":true,...}}} - parsing .json alone hit the
            # envelope's ok, a tautology)
            $lo = ($lp | ConvertFrom-Json).json
            if ($lo.result -ne $null) { $lo = $lo.result }
            $lpOk = ($lo -ne $null -and $lo.ok -eq $true -and $lo.'live' -eq $true)
        } catch { $lpOk = $false }
    }
    $lps = ""
    if ($lp -ne $null) { $lps = $lp.Substring(0, [Math]::Min(300, $lp.Length)) }
    Check "c5-ws-relay-live-patch" $lpOk $lps
    $c1.Close()
    # the relay executed the live branch in the client (log assert), and the
    # phone-patched behavior answers a real mouse click
    $lpLog5 = Wait-Log '\[script\] live patch: gen [0-9]+' 5000
    Check "c5-ws-relay-log" ($lpLog5 -ne $null) ("matched=" + $lpLog5)
    $phLog = Click-Workshop 'ph=([0-9]+)' 1500
    Check "c5-click-phone-behavior" ($phLog -ne $null) ("matched=" + $phLog)

    # ---- c6: cleanup - close window -> catalog rows 0 ---------------------------
    $cat6 = Catalog
    $wid = 0
    $c6Start = $cat6.IndexOf('"name":"set_script"')
    if ($c6Start -ge 0) {
        $c6End = $cat6.IndexOf('"app":"', $c6Start)
        if ($c6End -lt 0) { $c6End = $cat6.Length }
        if ($c6End -gt $c6Start) {
            $row6 = $cat6.Substring($c6Start, $c6End - $c6Start)
            $m = [regex]::Match($row6, '"windowId":(\d+)')
            if ($m.Success) { $wid = [int]$m.Groups[1].Value }
        }
    }
    Check "c6-windowId>0" ($wid -gt 0) ("wid=$wid")
    $cl = (Invoke-Agentctl ('{"tool":"close_window","args":{"id":' + $wid + '}}'))
    $gone = $false
    foreach ($i in 1..20) {
        Start-Sleep -Milliseconds 500
        if (([regex]::Matches((Catalog), '"app":"workshop"').Count) -eq 0) { $gone = $true; break }
    }
    Check "c6-catalog-cleared" ($gone -and $cl -match '"ok":true') ("close=" + $cl + " gone=" + $gone)

    # ---- c7: probe-owned slot wiped (the user's slot list stays clean) ----------
    # SLOT ISOLATION: the probe wrote ONLY probe-ws (myapp.js was never
    # touched - the seed used the slot arg, so every live patch + fall
    # reloaded inside probe-ws). Wipe the probe-owned slot + history here;
    # finally repeats it defensively.
    if (Test-Path $slotFile) { Remove-Item $slotFile -Force -ErrorAction SilentlyContinue }
    if (Test-Path $histDir) { Remove-Item -Recurse -Force $histDir -ErrorAction SilentlyContinue }
    Check "c7-probe-slot-wiped" (-not (Test-Path $slotFile)) "probe-ws.js still present"
} finally {
    # probe-owned server + the workshop client + the probe bridge all die
    # here; the client log file stays for triage.
    Get-Process jkdesktop, jkwinserver, jkagentd, jkbridge -ErrorAction SilentlyContinue |
        Stop-Process -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $env:TEMP ("wslp_" + $PID + ".cmd")) -Force -ErrorAction SilentlyContinue
    # USER FILE guard (restored in finally only):
    # .current_workshop pointer -> the pre-probe value
    if ($hadCur) { [System.IO.File]::WriteAllBytes($currentFile, $curBak) }
    elseif (Test-Path $currentFile) { Remove-Item $currentFile -Force -ErrorAction SilentlyContinue }
    # bridge state (probe_phone_mirror convention): byte-identical restore
    if ($hadJb) { [System.IO.File]::WriteAllBytes($jbFile, $jbBak) }
    elseif (Test-Path $jbFile) { Remove-Item $jbFile -Force -ErrorAction SilentlyContinue }
    if ($hadChat) { [System.IO.File]::WriteAllBytes($chatFile, $chatBak) }
    elseif (Test-Path $chatFile) { Remove-Item $chatFile -Force -ErrorAction SilentlyContinue }
    if (Test-Path $slotFile) { Remove-Item $slotFile -Force -ErrorAction SilentlyContinue }
    if (Test-Path $histDir) { Remove-Item -Recurse -Force $histDir -ErrorAction SilentlyContinue }
    if ($run -and (Test-Path $run)) { Remove-Item -Recurse -Force $run -ErrorAction SilentlyContinue }
    Write-Output "NOTICE: the desktop server is left stopped - restart jkdesktop.exe --server + taskbar client + jkagentd + jkbridge.exe to resume the live desktop (it loads the repacked workshop.jkx with the live branch)"
}

if ($script:fail -eq 0) { Write-Host "RESULT: ALL PASS" }
else { Write-Host ("RESULT: " + $script:fail + " FAIL"); exit 1 }