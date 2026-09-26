# probe_workshop_stage1.ps1 - workshop stage-1 live gate (docs/67 stage 1).
# ASCII-only (PS5.1). Lifecycle skeleton cloned from probe_workshop.ps1;
# probe-owned additions cleaned in finally (second.js/.history/.current_).
# Checks (setup + 7):
#   s1 server up (probe-owned lifecycle: fresh --server)
#   s2 spawn workshop.jkx -> 7 tools registered
#   c1 list_slots -> ok + current=myapp
#   c2 state preservation (the stage-1 gate, live): set_script v1 (one edit,
#      one label) then v2 (same edit, changed label) -> client log carries
#      "[script] state restore: 1/1 edits" (capture+restore ran on the live
#      reload path)
#   c3 set_script {slot:second} -> ok + auto-switch (list_slots current=second)
#   c4 use_slot myapp -> back (state map round trip)
#   c5 script_history -> gens ascending (1,2)
#   c6 restore_script {gen:2} -> ok + snapshotGen=3 + get_script carries the
#      restored v1 marker
#   c7 .current_workshop persisted with myapp
# USER FILE: state/scripts/myapp.js backed up at start, template restored in
# finally (probe_workshop convention); probe slots + ribbon removed in finally.
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root = Split-Path $exe
$jkx = Join-Path $root "apps\workshop.jkx"
$myapp = Join-Path $root "state\scripts\myapp.js"
$second = Join-Path $root "state\scripts\second.js"
$current = Join-Path $root "state\scripts\.current_workshop"
$history = Join-Path $root "state\scripts\.history"
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
function Catalog {
    return (Invoke-Agentctl '{"tool":"list_app_tools","args":{}}')
}

# --- USER FILE guard ----------------------------------------------------------
$myappExisted = Test-Path $myapp
$myappBak = "$myapp.stage1_bak"
if ($myappExisted) {
    Copy-Item $myapp $myappBak -Force
    Write-Output "NOTICE: myapp.js backed up"
}

# --- server + client lifecycle (probe_workshop convention) ---------------------
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

$bat = Join-Path $env:TEMP ("ws1_client_" + $PID + ".cmd")
("@echo off`r`ncd /d `"" + $root + "`"`r`n`"" + $exe + "`" --jkx `"" + $jkx + "`" > `"" + $clientLog + "`" 2>&1`r`n") |
    Set-Content -Path $bat -Encoding ASCII
Start-Process -FilePath $bat -WindowStyle Hidden
$rows = 0
$cat = ""
foreach ($i in 1..30) {
    Start-Sleep -Milliseconds 500
    $cat = Catalog
    $rows = [regex]::Matches($cat, '"app":"workshop"').Count
    if ($rows -eq 7) { break }
}
Check "s2-spawn-7-rows" ($rows -eq 7) ("rows=$rows")

try {
    # ---- c1: list_slots -----------------------------------------------------------
    $r1 = (AppTool "workshop" "list_slots" "{}")
    Check "c1-list-slots" ($r1 -match '"ok":true' -and
                           $r1 -match '"current":"myapp"' -and
                           $r1 -match '"myapp"') $r1

    # ---- c2: state preservation on the live reload path (the stage-1 gate) --------
    # v1: one edit + one label. v2: same edit, different label -> reload must
    # capture+restore the edit; the client log line is the machine gate.
    $v1 = "var e = createEdit({x:10,y:50,w:140,h:24}, 'LIVESTATE'); var l = createLabel({x:10,y:90,w:200,h:20}, 'L1');"
    $rA = (AppTool "workshop" "set_script" ('{"source":"' + ($v1 -replace '"', '\"') + '"}'))
    Check "c2-set-v1" ($rA -match '"ok":true') $rA
    $v2 = "var e = createEdit({x:10,y:50,w:140,h:24}, 'LIVESTATE'); var l = createLabel({x:10,y:90,w:200,h:20}, 'L2');"
    $rB = (AppTool "workshop" "set_script" ('{"source":"' + ($v2 -replace '"', '\"') + '"}'))
    Check "c2b-set-v2" ($rB -match '"ok":true') $rB
    $seen = $false
    foreach ($i in 1..12) {
        Start-Sleep -Milliseconds 300
        if ((Test-Path $clientLog) -and
            ((Get-Content $clientLog -Raw -ErrorAction SilentlyContinue) -match "state restore: 1/1 edits")) {
            $seen = $true; break
        }
    }
    Check "c2c-state-restore-log" $seen ("log=" + (Test-Path $clientLog))

    # ---- c3: set_script on another slot auto-switches ------------------------------
    $vs = "createEdit({x:10,y:50,w:140,h:24}, 'SLOT2TEXT');"
    $rS = (AppTool "workshop" "set_script" ('{"source":"' + ($vs -replace '"', '\"') + '","slot":"second"}'))
    Check "c3-set-slot-second" ($rS -match '"ok":true' -and
                                $rS -match '"slot":"second"') $rS
    $rL = (AppTool "workshop" "list_slots" "{}")
    Check "c3b-current-second" ($rL -match '"current":"second"') $rL
    $rG = (AppTool "workshop" "get_script" '{"slot":"second"}')
    Check "c3c-second-source" ($rG -match 'SLOT2TEXT') ($rG.Substring(0, [Math]::Min(200, $rG.Length)))

    # ---- c4: use_slot back ----------------------------------------------------------
    $rU = (AppTool "workshop" "use_slot" '{"slot":"myapp"}')
    Check "c4-use-slot-back" ($rU -match '"ok":true') $rU
    $rL2 = (AppTool "workshop" "list_slots" "{}")
    Check "c4b-current-myapp" ($rL2 -match '"current":"myapp"') $rL2

    # ---- c5: script_history ascending ------------------------------------------------
    $rH = (AppTool "workshop" "script_history" "{}")
    $gen1 = $rH.IndexOf('"gen":1')
    $gen2 = $rH.IndexOf('"gen":2')
    Check "c5-history-ascending" ($rH -match '"ok":true' -and
                                  $gen1 -ge 0 -and $gen2 -gt $gen1) $rH

    # ---- c6: restore_script gen 2 (undoable: pre-restore becomes a new gen) ----------
    $rR = (AppTool "workshop" "restore_script" '{"gen":2}')
    Check "c6-restore-ok" ($rR -match '"ok":true' -and
                           $rR -match '"gen":2' -and
                           $rR -match '"snapshotGen":3') $rR
    $rG2 = (AppTool "workshop" "get_script" "")
    Check "c6b-restored-source" ($rG2 -match 'LIVESTATE' -and $rG2 -match 'L1') ($rG2.Substring(0, [Math]::Min(200, $rG2.Length)))

    # ---- c7: last-slot persistence ----------------------------------------------------
    $curTxt = ""
    if (Test-Path $current) { $curTxt = [IO.File]::ReadAllText($current).Trim() }
    Check "c7-current-persisted" ($curTxt -eq "myapp") ("content='" + $curTxt + "'")
} finally {
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    # probe-owned slot artifacts (docs/67 stage 1 ladder).
    Remove-Item $second -Force -ErrorAction SilentlyContinue
    Remove-Item $current -Force -ErrorAction SilentlyContinue
    if (Test-Path $history) { Remove-Item $history -Recurse -Force -ErrorAction SilentlyContinue }
    # USER FILE restore: the real template (Korean, UTF-8 no BOM) for the eye check.
    $tpl = Join-Path $root "..\scripts\apps\workshop\app.js"
    if (Test-Path $myappBak) {
        Copy-Item $myappBak $myapp -Force
        Remove-Item $myappBak -Force
        Write-Output "NOTICE: myapp.js restored"
    } elseif (-not $myappExisted) {
        Remove-Item $myapp -Force -ErrorAction SilentlyContinue
        $tplText = ""
        if (Test-Path $tpl) {
            $tplText = ([IO.File]::ReadAllText($tpl) -replace '(?m)^//.*[\r\n]*', '' -replace '\r?\n', ' ').Trim()
            [IO.File]::WriteAllText($myapp, $tplText + "`r`n", (New-Object System.Text.UTF8Encoding($false)))
        }
        Write-Output "NOTICE: probe-created myapp.js removed (template reseeded)"
    }
    Remove-Item $bat -Force -ErrorAction SilentlyContinue
}

if ($script:fail -eq 0) { Write-Host "RESULT: ALL PASS" }
else { Write-Host ("RESULT: " + $script:fail + " FAIL"); exit 1 }