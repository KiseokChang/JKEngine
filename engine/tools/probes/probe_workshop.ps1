# probe_workshop.ps1 - workshop regression (docs/60 §4). ASCII-only (PS5.1).
# Checks (setup + 9):
#   s1 server up (probe-owned lifecycle: fresh --server)
#   s2 spawn workshop.jkx -> 7 tools registered (docs/67 stage 1: +list_slots/
#      use_slot/script_history/restore_script)
#   s3 template seeded: state/scripts/myapp.js exists + has createButton
#   1  get_script roundtrip: ok:true + source contains createButton
#   2  set_script good source -> ok:true + DISK file actually written +
#      get_script roundtrip matches
#   3  set_script broken source -> ok:false + error non-empty (the synchronous
#      reload closed loop - a deferred two-phase reload could not report an
#      error in the same response) + truth source on disk is the broken write
#   3b recover: set_script good again -> ok:true (agent self-fix loop)
#   4  args cap lifted (2026-09-21 phone-practical task-1, docs/57 §13):
#      9KiB source -> ok:true - the server front cap was 8KiB (args_too_large)
#      and is now 256KiB, aligned with the app's 256KiB backstop; a 9KiB
#      payload must pass through, NOT answer args_too_large
#   5  watch path: DISK edit (not via tools) -> client log
#      "[script] app.js changed - hot reload" within 3s
#   6  cleanup: close_window -> catalog rows 7 -> 0
#   7  truth source restored to the template (clean state for the eye check)
# permissions.json is NEVER touched by this probe (docs/59 §16.1 incident):
# app_tool defaults to allow without keys, and close_window allowance comes
# from the user's live file (9-key all-allow restore).
# Raw pipe helpers are NOT needed here - agentctl + agent tools suffice.
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root = Split-Path $exe
$jkx = Join-Path $root "apps\workshop.jkx"
$myapp = Join-Path $root "state\scripts\myapp.js"
$clientLog = Join-Path $root "workshop_client.log"
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
function AppTool([string]$app, [string]$tool, [string]$argsJson) {
    $a = '{"app":"' + $app + '","tool":"' + $tool + '"'
    if ($argsJson) { $a += ',"args":' + $argsJson }
    $a += '}'
    return (Invoke-Agentctl ('{"tool":"app_tool","args":' + $a + '}'))
}
# set_script payload: single-line JS (no newline escaping) with " escaped.
function SetScript([string]$js) {
    $esc = $js -replace '"', '\"'
    return (AppTool "workshop" "set_script" ('{"source":"' + $esc + '"}'))
}
function Catalog {
    return (Invoke-Agentctl '{"tool":"list_app_tools","args":{}}')
}

# --- server lifecycle ---------------------------------------------------------
Get-Process jkdesktop, jkwinserver -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process jkagentd, jkbridge -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
$up = $false
foreach ($i in 1..30) {
    Start-Sleep -Milliseconds 500
    if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up = $true; break }
}
Check "setup-server-up" $up ""

try {
    # ---- spawn workshop client (stdout -> log file for the watch-path check) ---
    # A temp .cmd avoids PS5.1 Start-Process quoting hell entirely.
    if (Test-Path $clientLog) { Remove-Item $clientLog -Force }
    if (Test-Path $myapp) { Remove-Item $myapp -Force }   # template re-seed check
    $bat = Join-Path $env:TEMP ("ws_client_" + $PID + ".cmd")
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
    $head = $cat
    if ($head.Length -gt 300) { $head = $head.Substring(0, 300) }
    Check "s2-spawn-7-rows" ($rows -eq 7) ("rows=$rows / " + $head)
    $namesOk = ($cat -match '"app":"workshop","name":"get_script"') -and
               ($cat -match '"app":"workshop","name":"set_script"') -and
               ($cat -match '"app":"workshop","name":"api"') -and
               ($cat -match '"name":"list_slots"') -and
               ($cat -match '"name":"use_slot"') -and
               ($cat -match '"name":"script_history"') -and
               ($cat -match '"name":"restore_script"')
    Check "s2-catalog-names" $namesOk ""

    # ---- s3: template seeded on first boot --------------------------------------
    $seeded = $false
    foreach ($i in 1..20) {
        Start-Sleep -Milliseconds 300
        if (Test-Path $myapp) { $seeded = $true; break }
    }
    $body = ""
    if ($seeded) { $body = [IO.File]::ReadAllText($myapp) }
    Check "s3-template-seeded" ($seeded -and $body -match "createButton") ("myapp bytes=" + $body.Length)

    # ---- check 1: get_script roundtrip -------------------------------------------
    $g1 = (AppTool "workshop" "get_script" "")
    $g1s = $g1
    if ($g1s.Length -gt 300) { $g1s = $g1s.Substring(0, 300) }
    Check "c1-get-script-ok" ($g1 -match '"ok":true' -and $g1 -match "createButton") $g1s

    # ---- check 1b: api catalog — the LLM-facing digest (docs/60 §8) ---------------
    # The phone session burned 3 turns guessing at bindings (createListBox
    # 헛다리). The api tool must return the function list + the charset
    # contract. ASCII-only needle against the UTF-8 body.
    $api = (AppTool "workshop" "api" "")
    $apiOk = ($api -match '"ok":true' -and
              $api -match '"sig":"createButton\(rect, text\)"' -and
              $api -match '"sig":"setInterval' -and
              $api -match '"note":"createListBox')
    Check "c1b-api-catalog" $apiOk $api.Substring(0, [Math]::Min(200, $api.Length))

    # ---- check 2: set_script good source -> file written + roundtrip --------------
    # JS single-quote strings: a raw " in the source becomes \" in JSON and
    # the agentctl argv double-escape (CRT quoting, docs/55 lesson 3) truncates
    # the JSON -> server bad_request. Single quotes sidestep the layer.
    $src2 = "var btn = createButton({ x: 20, y: 20, w: 140, h: 34 }, 'V2 button'); function onClick(id) { if (id === btn) { setText(btn, 'clicked'); } }"
    $r2 = (SetScript $src2)
    Check "c2-set-ok" ($r2 -match '"ok":true') $r2
    $disk = ""
    if (Test-Path $myapp) { $disk = [IO.File]::ReadAllText($myapp) }
    Check "c2-disk-written" ($disk -match "V2 button") ("disk bytes=" + $disk.Length)
    $g2 = (AppTool "workshop" "get_script" "")
    $g2s = $g2
    if ($g2s.Length -gt 300) { $g2s = $g2s.Substring(0, 300) }
    Check "c2-roundtrip" ($g2 -match "V2 button") $g2s

    # ---- check 3: broken source -> synchronous error in the response --------------
    # Truncated call = QuickJS syntax error. If reload were deferred to the
    # watch tick, the response could not contain the error.
    $srcBad = 'var btn = createButton({ x: 20, y: 20, w: 140, h: 34'
    $r3 = (SetScript $srcBad)
    # unknown_app_tool also matches '"error":"[^"]+"' - exclude it explicitly
    # (its presence means the client never registered, not that reload failed).
    $errOk = $r3 -match '"ok":false' -and
             $r3 -match '"error":"(?!unknown_app_tool)[^"]+"' -and
             $r3 -notmatch 'unknown_app_tool'
    $r3s = $r3
    if ($r3s.Length -gt 400) { $r3s = $r3s.Substring(0, 400) }
    Check "c3-broken-error" $errOk $r3s
    # Truth source reflects the write (the agent must fix it - by design).
    $diskBad = ""
    if (Test-Path $myapp) { $diskBad = [IO.File]::ReadAllText($myapp) }
    Check "c3-truth-source-is-broken" ($diskBad -match "h: 34\s*$") ("disk tail=" + $diskBad.Substring([Math]::Max(0, $diskBad.Length - 60)))

    # ---- check 3b: recover (self-fix loop) ----------------------------------------
    $src3 = "var btn = createButton({ x: 20, y: 20, w: 140, h: 34 }, 'V3 button'); function onClick(id) { if (id === btn) { setText(btn, 'v3'); } }"
    $r4 = (SetScript $src3)
    Check "c3b-recovered" ($r4 -match '"ok":true') $r4

    # ---- check 4: args cap lifted to 256KiB -> 9KiB passes (docs/57 §13) ----------
    $big = "var x = 1;"
    while ($big.Length -lt 9216) { $big += " // padding for the lifted args cap" }
    $r5 = (SetScript $big)
    $r5s = $r5
    if ($r5s.Length -gt 300) { $r5s = $r5s.Substring(0, 300) }
    Check "c4-args-cap-256k-passes" (($r5 -match '"ok') -and ($r5 -notmatch 'args_too_large')) $r5s

    # ---- check 5: watch path - DISK edit reloads without tools ---------------------
    # AppendAllText + UTF8-no-BOM: PS5.1 Add-Content -Encoding UTF8 injects a
    # BOM mid-file (the known quirk) which would corrupt the JS.
    # Timestamp-alias guard (first live session, measured): the FAT-family
    # LastWriteTime on this exFAT volume aliases sub-second edits to the
    # previous stamp, so a 600ms gap after set_script's write is flaky while
    # 2s spacing reloads 3/3 (manual measurement). A human "edit again" is
    # the same pattern - retry up to 3 pushes 2s apart.
    $seen = $false
    for ($try = 1; $try -le 3 -and -not $seen; $try++) {
        Start-Sleep -Seconds 2
        [IO.File]::AppendAllText($myapp, "`r`n// WS-WATCH-MARK-$try",
            (New-Object System.Text.UTF8Encoding($false)))
        foreach ($i in 1..12) {
            Start-Sleep -Milliseconds 300
            if ((Test-Path $clientLog) -and
                ((Get-Content $clientLog -Raw -ErrorAction SilentlyContinue) -match "app\.js changed - hot reload")) {
                $seen = $true; break
            }
        }
    }
    Check "c5-watch-hot-reload" $seen ("log=" + (Test-Path $clientLog))

    # ---- check 6: cleanup - close window -> catalog rows 0 -------------------------
    $cat6 = Catalog
    $wid = 0
    $m = [regex]::Match($cat6, '"app":"workshop","name":"set_script","description":"[^"]*","inputSchema":[\s\S]*?"windowId":(\d+)')
    if ($m.Success) { $wid = [int]$m.Groups[1].Value }
    Check "c6-windowId>0" ($wid -gt 0) ("wid=$wid")
    $cl = (Invoke-Agentctl ('{"tool":"close_window","args":{"id":' + $wid + '}}'))
    $gone = $false
    foreach ($i in 1..20) {
        Start-Sleep -Milliseconds 500
        if (([regex]::Matches((Catalog), '"app":"workshop"').Count) -eq 0) { $gone = $true; break }
    }
    Check "c6-catalog-cleared" ($gone -and $cl -match '"ok":true') ("close=" + $cl + " gone=" + $gone)

    # ---- leave a clean truth source for the user's eye check ----------------------
    # The client is dead by now; copy the real template (Korean, UTF-8 no BOM)
    # over the probe-worn truth source. state/scripts/myapp.js is app-seeded
    # state - the next workshop boot keeps using it.
    $tpl = Join-Path $root "..\scripts\apps\workshop\app.js"
    $tplText = [IO.File]::ReadAllText($tpl)
    $tplText = ($tplText -replace '(?m)^//.*[\r\n]*', '' -replace '\r?\n', ' ').Trim()
    if (Test-Path $myapp) {
        [IO.File]::WriteAllText($myapp, $tplText + "`r`n", (New-Object System.Text.UTF8Encoding($false)))
    }
    Check "c7-truth-source-restored" (Test-Path $myapp) ""
} finally {
    # probe-owned server + the workshop client process (both are jkdesktop
    # processes) die here; the client log file stays for triage.
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $env:TEMP ("ws_client_" + $PID + ".cmd")) -Force -ErrorAction SilentlyContinue
}

if ($script:fail -eq 0) { Write-Host "RESULT: ALL PASS" }
else { Write-Host ("RESULT: " + $script:fail + " FAIL"); exit 1 }