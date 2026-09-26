# probe_workshop_stage1.ps1 - workshop stage-1 live gate (docs/67 stage 1).
# ASCII-only (PS5.1). Lifecycle skeleton cloned from probe_workshop.ps1.
# 2026-09-26 hardening (user live-session collision): the probe NEVER writes
# the user's myapp.js and never deletes user-owned slot state. All writes go
# to probe-owned slots (ws1probe/ws1probe2); user files (.history/.current_)
# are backed up at start and restored in finally.
# Checks (setup + 7):
#   s1 server up (probe-owned lifecycle: fresh --server)
#   s2 spawn workshop.jkx -> 7 tools registered
#   c1 list_slots -> ok (current = whatever the last session persisted)
#   c2 state preservation (the stage-1 gate, live) on probe slot ws1probe:
#      v1 (one edit + label L1), v2 (same edit, label L2), v3 (label L3) ->
#      client log carries "[script] state restore: 1/1 edits"
#   c3 set_script {slot:ws1probe2} -> ok + auto-switch
#   c4 use_slot myapp -> back (probe slot state round trip is covered by the
#      unit probe; here the wire + persistence is the surface)
#   c5 script_history (ws1probe) -> gens 1,2 ascending (deterministic —
#      probe-owned slot, first write had no target file)
#   c6 restore_script {gen:2} -> ok + snapshotGen=3 + get_script carries the
#      restored v2 marker
#   c7 .current_workshop persisted with myapp
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root = Split-Path $exe
$jkx = Join-Path $root "apps\workshop.jkx"
$scripts = Join-Path $root "state\scripts"
$myapp = Join-Path $scripts "myapp.js"
$history = Join-Path $scripts ".history"
$current = Join-Path $scripts ".current_workshop"
$probeSlots = @("ws1probe", "ws1probe2")
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
    $psi.StandardOutputEncoding = [System.Text.Encoding]::UTF8
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

# --- USER FILE guard (docs/60 §11 사고 레슨 — 수동 검증에도 백업 의무) ---------
$myappExisted = Test-Path $myapp
$myappBak = "$myapp.stage1_bak"
if ($myappExisted) {
    Copy-Item $myapp $myappBak -Force
    Write-Output "NOTICE: myapp.js backed up"
}
$curExisted = Test-Path $current
$curBak = "$current.stage1_bak"
if ($curExisted) { Copy-Item $current $curBak -Force }
$histExisted = Test-Path $history
$histBak = "$history.stage1_bak"
if ($histExisted) { Move-Item $history $histBak -Force }
Write-Output "NOTICE: user slot state guarded (.history/.current moved aside)"

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
    # 사용자 세션이 마지막 슬롯을 바꿔뒀을 수 있다 — ok + 목록 존재만 단정.
    $r1 = (AppTool "workshop" "list_slots" "{}")
    Check "c1-list-slots" ($r1 -match '"ok":true' -and
                           $r1 -match '"current":"' -and
                           $r1 -match '"slots":\[') $r1

    # ---- c2: state preservation on the live reload path (the stage-1 gate) --------
    # All on probe-owned slot ws1probe: v1 (edit+label L1), v2 (label L2),
    # v3 (label L3). Each reload after the first captures/restores the edit.
    $v1 = "var e = createEdit({x:10,y:50,w:140,h:24}, 'LIVESTATE'); var l = createLabel({x:10,y:90,w:200,h:20}, 'L1');"
    $v2 = $v1 -replace "'L1'", "'L2'"
    $v3 = $v1 -replace "'L1'", "'L3'"
    $rA = (AppTool "workshop" "set_script" ('{"source":"' + ($v1 -replace '"', '\"') + '","slot":"ws1probe"}'))
    Check "c2-set-v1" ($rA -match '"ok":true' -and $rA -match '"slot":"ws1probe"') $rA
    $rB = (AppTool "workshop" "set_script" ('{"source":"' + ($v2 -replace '"', '\"') + '"}'))
    Check "c2b-set-v2" ($rB -match '"ok":true') $rB
    $rC = (AppTool "workshop" "set_script" ('{"source":"' + ($v3 -replace '"', '\"') + '"}'))
    Check "c2c-set-v3" ($rC -match '"ok":true') $rC
    $seen = $false
    foreach ($i in 1..12) {
        Start-Sleep -Milliseconds 300
        if ((Test-Path $clientLog) -and
            ((Get-Content $clientLog -Raw -ErrorAction SilentlyContinue) -match "state restore: 1/1 edits")) {
            $seen = $true; break
        }
    }
    Check "c2d-state-restore-log" $seen ("log=" + (Test-Path $clientLog))

    # ---- c3: set_script on another slot auto-switches ------------------------------
    $vs = "createEdit({x:10,y:50,w:140,h:24}, 'SLOT2TEXT');"
    $rS = (AppTool "workshop" "set_script" ('{"source":"' + ($vs -replace '"', '\"') + '","slot":"ws1probe2"}'))
    Check "c3-set-slot-second" ($rS -match '"ok":true' -and
                                $rS -match '"slot":"ws1probe2"') $rS
    $rL = (AppTool "workshop" "list_slots" "{}")
    Check "c3b-current-second" ($rL -match '"current":"ws1probe2"') $rL
    $rG = (AppTool "workshop" "get_script" '{"slot":"ws1probe2"}')
    Check "c3c-second-source" ($rG -match 'SLOT2TEXT') ($rG.Substring(0, [Math]::Min(200, $rG.Length)))

    # ---- c5: script_history ascending (probe-owned slot — deterministic) ------------
    $rH = (AppTool "workshop" "script_history" '{"slot":"ws1probe"}')
    $gen1 = $rH.IndexOf('"gen":1')
    $gen2 = $rH.IndexOf('"gen":2')
    Check "c5-history-ascending" ($rH -match '"ok":true' -and
                                  $gen1 -ge 0 -and $gen2 -gt $gen1) $rH

    # ---- c6: restore_script gen 2 (undoable: pre-restore becomes a new gen) ----------
    # 복원은 대상 슬롯으로 자동 전환하는 제품 동작 — c4 복귀 체크는 그 뒤에 와야
    # .current의 마지막 기록이 myapp이 된다.
    $rR = (AppTool "workshop" "restore_script" '{"slot":"ws1probe","gen":2}')
    Check "c6-restore-ok" ($rR -match '"ok":true' -and
                           $rR -match '"gen":2' -and
                           $rR -match '"snapshotGen":3') $rR
    $rG2 = (AppTool "workshop" "get_script" '{"slot":"ws1probe"}')
    Check "c6b-restored-source" ($rG2 -match 'LIVESTATE' -and $rG2 -match 'L2') ($rG2.Substring(0, [Math]::Min(200, $rG2.Length)))

    # ---- c4: use_slot back (last — .current must end on myapp) ----------------------
    $rU = (AppTool "workshop" "use_slot" '{"slot":"myapp"}')
    Check "c4-use-slot-back" ($rU -match '"ok":true') $rU
    $rL2 = (AppTool "workshop" "list_slots" "{}")
    Check "c4b-current-myapp" ($rL2 -match '"current":"myapp"') $rL2

    # ---- c7: last-slot persistence ----------------------------------------------------
    $curTxt = ""
    if (Test-Path $current) { $curTxt = [IO.File]::ReadAllText($current).Trim() }
    Check "c7-current-persisted" ($curTxt -eq "myapp") ("content='" + $curTxt + "'")
} finally {
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    # probe-owned slot artifacts only (ws1probe*) — user slots untouched.
    foreach ($s in $probeSlots) {
        Remove-Item (Join-Path $scripts ($s + ".js")) -Force -ErrorAction SilentlyContinue
        Remove-Item (Join-Path $history $s) -Recurse -Force -ErrorAction SilentlyContinue
    }
    # USER FILE restore — everything the probe moved aside goes back byte-identical.
    if (Test-Path $myappBak) {
        Copy-Item $myappBak $myapp -Force
        Remove-Item $myappBak -Force
        Write-Output "NOTICE: myapp.js restored"
    } elseif (-not $myappExisted) {
        Remove-Item $myapp -Force -ErrorAction SilentlyContinue
        $tpl = Join-Path $root "..\scripts\apps\workshop\app.js"
        if (Test-Path $tpl) {
            $tplText = ([IO.File]::ReadAllText($tpl) -replace '(?m)^//.*[\r\n]*', '' -replace '\r?\n', ' ').Trim()
            [IO.File]::WriteAllText($myapp, $tplText + "`r`n", (New-Object System.Text.UTF8Encoding($false)))
        }
        Write-Output "NOTICE: probe-created myapp.js removed (template reseeded)"
    }
    if ($curExisted) {
        Copy-Item $curBak $current -Force
        Remove-Item $curBak -Force
    } else {
        Remove-Item $current -Force -ErrorAction SilentlyContinue
    }
    if ($histExisted) {
        if (Test-Path $history) { Remove-Item $history -Recurse -Force -ErrorAction SilentlyContinue }
        Move-Item $histBak $history -Force
        Write-Output "NOTICE: user .history restored"
    }
    Remove-Item $bat -Force -ErrorAction SilentlyContinue
}

if ($script:fail -eq 0) { Write-Host "RESULT: ALL PASS" }
else { Write-Host ("RESULT: " + $script:fail + " FAIL"); exit 1 }