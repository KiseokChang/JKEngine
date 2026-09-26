# probe_workshop_canvas.ps1 - canvas drawing + keyboard/mouse event API
# (docs/60 s10, backlog item 1: the game/toy layer). ASCII-only (PS5.1).
# Scenario: install a canvas+event script via set_script, then let the REAL
# input routing prove the event path - the script schedules its own
# injectMouse/injectKey (v2 automation bindings -> window RespondMessage ->
# HitTest -> canvas -> sink -> onMouse/onKey), and each event draws. Verify is
# capture_window hash deltas, the conquest ladder's verify primitive.
#   s1 server up (probe-owned lifecycle: fresh --server, docs/60 s4 template)
#   s2 spawn workshop.jkx -> 7 catalog rows (docs/67 stage 1)
#   c1 canvas script set_script -> ok:true (sync reload; parse+boot proves
#      createCanvas + all canvas* bindings + color strings exist)
#   c2 capture hash changes after install (canvas drew the onCreate scene)
#   c3 hash changes again when the scheduled injectMouse lands
#      (down -> canvasPixel + label - the full event path, no send_input,
#      so permissions.json is NEVER touched - the 9/19 incident rule)
#   c4 hash changes again when the scheduled injectKey lands (onKey line)
#   c5 api catalog lists the canvas bindings (kApiCatalog sync - the LLM
#      contract and jk.d.ts must move together, additive policy)
#   c6 cleanup: close_window -> catalog rows 0; truth source restored
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
function Invoke-Agentctl([string]$json) {
    # StandardOutputEncoding=UTF8: the engine prints UTF-8; the default
    # redirected decode is ANSI and mojibakes Korean (docs/60, 2026-09-24).
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = 'agentctl "' + ($json -replace '"', '\"') + '"'
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.StandardOutputEncoding = [Text.Encoding]::UTF8
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
function SetScript([string]$js) {
    $esc = $js -replace '"', '\"'
    return (AppTool "workshop" "set_script" ('{"source":"' + $esc + '"}'))
}
function Catalog {
    return (Invoke-Agentctl '{"tool":"list_app_tools","args":{}}')
}
function Get-WorkshopWindowId {
    $cat = Catalog
    $m = [regex]::Match($cat, '"app":"workshop","name":"set_script","description":"[^"]*","inputSchema":[\s\S]*?"windowId":(\d+)')
    if ($m.Success) { return [int]$m.Groups[1].Value }
    return 0
}
function Capture-Hash([int]$wid) {
    $r = Invoke-Agentctl ('{"tool":"capture_window","args":{"id":' + $wid + '}}')
    $pm = [regex]::Match($r, '"path\\?":\\?"([^"]*)"')
    if (-not $pm.Success) { return "" }
    $p = $pm.Groups[1].Value -replace '\\\\', '\'
    if (-not (Test-Path $p)) { return "" }
    return (Get-FileHash $p -Algorithm SHA256).Hash
}
# Wait until the capture hash changes away from $baseline and settles (two
# consecutive equal captures) - a single capture can grab a mid-render frame
# (the batch browser flake measured 2026-09-24). Empty string on timeout.
function Wait-ChangedStable([int]$wid, [string]$baseline, [int]$deadlineSec) {
    $c = ""
    $deadline = [System.Diagnostics.Stopwatch]::StartNew()
    while ($deadline.ElapsedMilliseconds -lt ($deadlineSec * 1000)) {
        Start-Sleep -Milliseconds 400
        $c = Capture-Hash $wid
        if ($c -ne "" -and $c -ne $baseline) { break }
    }
    if ($c -eq "" -or $c -eq $baseline) { return "" }
    for ($i = 0; $i -lt 10; $i++) {
        $c2 = Capture-Hash $wid
        if ($c2 -ne "" -and $c2 -eq $c) { return $c }
        if ($c2 -eq "") { return $c }
        $c = $c2
    }
    return $c
}

# --- server lifecycle (probe-owned, docs/60 s4 template) ----------------------
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

# permissions.json is NEVER touched (docs/59 s16.1): the drive path is the
# script's own injectMouse/injectKey, not send_input.

# truth source backup: myapp.js is a user runtime file (docs/60 s2.1) -
# backup at start, restore in finally, NOTICE both ways.
$permBak = "$myapp.probe_canvas_bak"
$hadMyapp = (Test-Path $myapp)
if ($hadMyapp) {
    Copy-Item $myapp $permBak -Force
    Write-Host "NOTICE: backed up myapp.js -> probe_canvas_bak"
}
$workshopRan = $false

try {
    # ---- spawn the workshop client ------------------------------------------------
    $bat = Join-Path $env:TEMP ("wsc_canvas_" + $PID + ".cmd")
    ("@echo off`r`ncd /d `"" + $root + "`"`r`n`"" + $exe + "`" --jkx `"" + $jkx + "`"`r`n") |
        Set-Content -Path $bat -Encoding ASCII
    Start-Process -FilePath $bat -WindowStyle Hidden
    $rows = 0
    foreach ($i in 1..30) {
        Start-Sleep -Milliseconds 500
        $rows = [regex]::Matches((Catalog), '"app":"workshop"').Count
        if ($rows -eq 7) { break }
    }
    Check "s2-spawn-7-rows" ($rows -eq 7) ("rows=$rows")
    if ($rows -ne 7) { throw "workshop client did not come up" }
    $workshopRan = $true

    # ---- c1: install the canvas+event script ---------------------------------------
    # One line, single quotes (the agentctl argv double-escape trap, docs/55
    # lesson 3). The script draws a shape scene, then at t+2.5s injects a
    # canvas click and at t+4.5s a key - each event handler draws on top.
    $src = "var cv = createCanvas({x:10,y:10,w:220,h:120});" +
           "var lg = createLabel({x:10,y:140,w:220,h:20},'idle');" +
           "function draw() { canvasClear(cv,'#101020');" +
           "canvasRect(cv,5,5,80,40,'#ff4040',true);" +
           "canvasLine(cv,10,60,200,110,'#40ff40');" +
           "canvasCircle(cv,150,30,20,'#4080ff',true);" +
           "canvasText(cv,10,116,'CANVAS OK','#ffffff'); }" +
           "function onCreate() { draw();" +
           "var t1 = setInterval(function(){ clearInterval(t1); injectMouse(60,70); }, 2500);" +
           "var t2 = setInterval(function(){ clearInterval(t2); injectKey(65); }, 4500); }" +
           "function onMouse(type,x,y,cid,btn) { if (type==='down' && cid===cv) { setText(lg,'down:'+x+','+y); canvasPixel(cv,x,y,'#ffff00'); } }" +
           "function onKey(key,down) { if (down) { setText(lg,'key:'+key); canvasLine(cv,(key%200),0,(key%200),119,'#ffff00'); } }"
    $r1 = (SetScript $src)
    Check "c1-canvas-script-set" ($r1 -match '"ok":true') $r1

    # ---- c2: the onCreate scene rendered (hash differs from the template's) ----
    $wid = Get-WorkshopWindowId
    Check "s-windowId>0" ($wid -gt 0) ("wid=$wid")
    if ($wid -le 0) { throw "no workshop window id" }
    $h0 = ""
    foreach ($i in 1..25) {
        Start-Sleep -Milliseconds 400
        $h0 = Capture-Hash $wid
        if ($h0 -ne "") { break }
    }
    Check "c2-capture-baseline" ($h0 -ne "") "capture_window"
    if ($h0 -eq "") { throw "no capture" }
    # The t1 mouse event is 2.5s after install; h0 must be captured BEFORE it.
    # The first change (h1) is the mouse drawing, the second (h2) the key.
    $h1 = Wait-ChangedStable $wid $h0 9
    Check "c3-mouse-event-drew" ($h1 -ne "" -and $h1 -ne $h0) "injectMouse at t+2.5s"
    $h2 = Wait-ChangedStable $wid $h1 9
    Check "c4-key-event-drew" ($h2 -ne "" -and $h2 -ne $h1) "injectKey at t+4.5s"

    # ---- c5: api catalog lists the canvas bindings (kApiCatalog sync) ----------
    $api = (AppTool "workshop" "api" "")
    $apiOk = ($api -match '"sig":"createCanvas\(rect\)"') -and
             ($api -match '"sig":"canvasClear\(id, color\?\)"') -and
             ($api -match '"sig":"canvasText') -and
             ($api -match 'onMouse\(type,x,y,canvasId,button\)')
    Check "c5-api-catalog-canvas" $apiOk ($api.Substring(0, [Math]::Min(160, $api.Length)))
} finally {
    if ($workshopRan) {
        $wid2 = Get-WorkshopWindowId
        if ($wid2 -gt 0) {
            [void](Invoke-Agentctl ('{"tool":"close_window","args":{"id":' + $wid2 + '}}'))
        }
    }
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $env:TEMP ("wsc_canvas_" + $PID + ".cmd")) -Force -ErrorAction SilentlyContinue
    # truth source restore (user runtime file rule)
    if (Test-Path $permBak) {
        Copy-Item $permBak $myapp -Force
        Remove-Item $permBak -Force
        Write-Host "NOTICE: myapp.js restored from backup"
    } else {
        Write-Host "NOTICE: no myapp.js backup existed at start - leaving the seeded template"
    }
}

if ($script:fail -eq 0) { Write-Host "RESULT: ALL PASS" }
else { Write-Host ("RESULT: " + $script:fail + " FAIL"); exit 1 }