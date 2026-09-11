# Desktop Agent screenshot probe (docs/35): capture_window -> PNG on disk,
# capture_region -> physical-size check (logical x OutputScale), rubber-band
# overlay spawn (server resizes it to the whole desktop), viewer spawn.
# PASS/FAIL via exit code.
# NOTE (lesson 34): ImGui apps are not WM_GETTEXT-readable — judge from
# list_windows titles and files only.
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

# Clean baseline: no stale screenshots from earlier runs.
$stateDir = Join-Path (Split-Path $exe) "state"
Remove-Item (Join-Path $stateDir "screenshots") -Recurse -ErrorAction SilentlyContinue

Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 3

function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}
$list = '{"tool":"list_windows","args":{}}'

# --- 1. capture_window: launch minesweeper, shoot it by id ---
$r0 = Invoke-Agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}'
Start-Sleep -Seconds 3
$l1 = Invoke-Agentctl $list
$mine = $l1 | ConvertFrom-Json
$mineWin = $mine.windows | Where-Object { $_.title -match "Mine" } | Select-Object -First 1
$capPath = ""
$r1ok = $false
$magicOk = $false
if ($mineWin) {
    $r1 = Invoke-Agentctl ('{"tool":"capture_window","args":{"id":' + $mineWin.id + '}}')
    if ($r1 -match '"path\\?":\\?"([^"]*)"') {
        $capPath = $Matches[1] -replace '\\\\', '\'
    }
    $r1ok = ($r1 -match 'ok\\?":true') -and ($capPath -ne "")
    if ($r1ok -and (Test-Path $capPath)) {
        $bytes = [System.IO.File]::ReadAllBytes($capPath)[0..3]
        $magicOk = ($bytes[0] -eq 0x89 -and $bytes[1] -eq 0x50 -and
                    $bytes[2] -eq 0x4E -and $bytes[3] -eq 0x47)
    }
}

# --- 2. capture_region: 120x90 logical at (10,10) ---
$r2 = Invoke-Agentctl '{"tool":"capture_region","args":{"x":10,"y":10,"w":120,"h":90}}'
$regionPath = ""
$r2ok = $false
if ($r2 -match '"path\\?":\\?"([^"]*)"') {
    $regionPath = $Matches[1] -replace '\\\\', '\'
}
$r2ok = ($r2 -match 'ok\\?":true') -and ($regionPath -ne "") -and (Test-Path $regionPath)

# --- 3. region PNG size: logical 120x90 scaled by the output scale ---
Add-Type -AssemblyName System.Drawing
$rw = 0; $rh = 0
if ($r2ok) {
    $img = [System.Drawing.Image]::FromFile($regionPath)
    $rw = $img.Width; $rh = $img.Height
    $img.Dispose()
}
# Physical = logical x scale (1.0 or 1.25 on the test rigs) — accept a
# physical size at or above the logical rect with the same 4:3 aspect.
$sizeOk = ($rw -ge 120 -and $rh -ge 90 -and
           [math]::Abs(($rw / [double]$rh) - (120 / 90.0)) -lt 0.05)

# --- 4. overlay spawn: server resizes it to the whole desktop ---
$r4 = Invoke-Agentctl '{"tool":"launch_app","args":{"app":"snap"}}'
Start-Sleep -Seconds 2
$l4 = Invoke-Agentctl $list
$snapWin = ($l4 | ConvertFrom-Json).windows |
    Where-Object { $_.title -eq "Region Capture" } | Select-Object -First 1
# The server default desktop is 1280x720 logical; the overlay must sit at
# (0,0) covering all of it (docs/35 Task 3 spawn hook).
$overlayOk = ($null -ne $snapWin -and $snapWin.x -eq 0 -and $snapWin.y -eq 0 -and
              $snapWin.w -ge 1280 -and $snapWin.h -ge 720)

# --- 5+6. viewer spawn: one "Screenshots" window, no duplicates ---
$r5 = Invoke-Agentctl '{"tool":"launch_app","args":{"app":"shot"}}'
Start-Sleep -Seconds 2
$l5 = Invoke-Agentctl $list
$viewers = @(($l5 | ConvertFrom-Json).windows |
    Where-Object { $_.title -eq "Screenshots" })
$viewerOk = ($null -ne $viewers -and $viewers.Count -ge 1)
$countOk = ($viewers.Count -eq 1)

# --- cleanup (after all captures — dead handles read as empty) ---
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Remove-Item (Join-Path $stateDir "screenshots") -Recurse -ErrorAction SilentlyContinue

# --- judge ---
$ok = $true
if ($r0 -match 'ok\\?":true' -and $r1ok -and $magicOk) { Write-Host "capture-window: PASS ($capPath)" } else { $ok = $false; Write-Host "capture-window: FAIL r1ok=$r1ok magic=$magicOk $r1" }
if ($r2ok) { Write-Host "capture-region: PASS ($regionPath)" } else { $ok = $false; Write-Host "capture-region: FAIL $r2" }
if ($sizeOk) { Write-Host "region-size: PASS (${rw}x${rh})" } else { $ok = $false; Write-Host "region-size: FAIL (${rw}x${rh}, want 4:3 >= 120x90)" }
if ($r4 -match 'ok\\?":true' -and $overlayOk) { Write-Host "overlay-spawn: PASS ($($snapWin.w)x$($snapWin.h) at 0,0)" } else { $ok = $false; Write-Host "overlay-spawn: FAIL $l4" }
if ($r5 -match 'ok\\?":true' -and $viewerOk) { Write-Host "viewer-spawn: PASS" } else { $ok = $false; Write-Host "viewer-spawn: FAIL $l5" }
if ($countOk) { Write-Host "viewer-count: PASS (1 window)" } else { $ok = $false; Write-Host "viewer-count: FAIL ($($viewers.Count) windows)" }

if ($ok) { Write-Host "PASS: agent shot"; exit 0 } else { Write-Host "FAIL: agent shot"; exit 1 }