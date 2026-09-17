# vpt12 e2e: fullscreen layer state + bottom-hover OSD (docs/50 sec 11,
# spec 2026-09-17 vplayer-fullscreen-osd). Harness recipe = vpt11_reverse.ps1.
# TOOL-PATH DRIVEN - no SendKeys at all: media opens via the JK_VPLAYER_OPEN
# env hook (vpt8 affordance, OnInit), fullscreen via the window_fullscreen
# agentctl tool, restore via the OSD exit button and the tool's on:0.
#
# Scenarios:
#   S2 tool enter on:1 -> ok:true/fullscreen:true, geometry (0,0,desktop),
#      window.fullscreen event, server chrome X gone (compositor skip).
#   S3 OSD show: cursor into the bottom 22% band -> strip pixels change
#      (video PAUSED first, so the diff is the OSD overlay itself).
#   S4 OSD exit button click -> geometry restored + window.fullscreen_exit.
#   S5 re-enter -> OSD shows again -> cursor parked top-left, 3.4 s idle ->
#      OSD gone (2.5 s idle + 200 ms fade, spec 2.3; 3.4 s = timing margin).
#   S6 tool exit on:0 -> geometry restored + window.fullscreen_exit again.
# Pixel verdicts are machine-checked (paused video = stable frames); the
# executing agent additionally reads the shots with the Read tool (lesson 19).
#
# Test media: tmp/vpt2_test.mp4 (480x270@30, 30 s).
param()
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wt12 {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("kernel32.dll")] public static extern IntPtr GetConsoleWindow();
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, int d, UIntPtr e);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
[Wt12]::SetProcessDPIAware() | Out-Null
# Run-2 lesson: the probe console and arbitrary desktop popups overlap the
# server window and eat clicks / pollute pixel shots (the user's desktop is
# LIVE while this runs). Minimize our own console and pin the server TOPMOST
# (Find-Server re-applies after every geometry refresh).
[Wt12]::ShowWindow([Wt12]::GetConsoleWindow(), 6) | Out-Null   # SW_MINIMIZE

$build   = "I:\progwork\JKENGINE\engine\build"
$exe     = "$build\jkdesktop.exe"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-17-vplayer-fullscreen-osd\shots"
$mp4     = "I:/progwork/JKENGINE/tmp/vpt2_test.mp4"
$script:fails = 0
New-Item -ItemType Directory -Force -Path $shotDir | Out-Null

function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host ("PASS: " + $name) }
    else     { $script:fails++; Write-Host ("FAIL: " + $name + " -- " + $detail) }
    if ($detail) { Write-Host ("      " + $detail) }
}

function Invoke-Agentctl([string]$json) {
    $escaped = $json.Replace('"', [string][char]92 + '"')
    return (& $exe agentctl $escaped 2>&1) -join "`n"
}

$script:srvHwnd = [IntPtr]::Zero
$script:scale = 1.0; $script:ox = 0; $script:oy = 0
$script:layerX = 0; $script:layerY = 0; $script:layerW = 960; $script:layerH = 640
$script:vplayerId = 0

function Find-Server {
    $h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
        Select-Object -First 1).MainWindowHandle
    if (-not $h) { Write-Host "FAIL: NO SERVER WINDOW"; exit 1 }
    $script:srvHwnd = [IntPtr]$h
    [Wt12]::SetWindowPos($script:srvHwnd, [IntPtr](-1), 0, 0, 0, 0, 0x0003) | Out-Null  # TOPMOST, NOSIZE|NOMOVE
    $crect = New-Object Wt12+RECT
    [Wt12]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt12+POINT; $co.X = 0; $co.Y = 0
    [Wt12]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
    $script:scale = ($crect.R - $crect.L) / 1280.0
    $script:ox = $co.X; $script:oy = $co.Y
}

function Find-VPlayerLayer {
    $list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    $mid = [regex]::Match($list, '"id":(\d+),"title":"Video Player"')
    if (-not $mid.Success) { return $false }
    $script:vplayerId = [int]$mid.Groups[1].Value
    $m = [regex]::Match($list, '"title":"Video Player".*?"x":(-?\d+),"y":(-?\d+),"w":(\d+),"h":(\d+)')
    if (-not $m.Success) { return $false }
    $script:layerX = [int]$m.Groups[1].Value
    $script:layerY = [int]$m.Groups[2].Value
    $script:layerW = [int]$m.Groups[3].Value
    $script:layerH = [int]$m.Groups[4].Value
    return $true
}
function VPlayer-Alive { return (Find-VPlayerLayer) }
function Refresh-Geom {
    Find-Server
    for ($i = 0; $i -lt 10; $i++) { if (Find-VPlayerLayer) { return $true }; Start-Sleep -Milliseconds 300 }
    return $false
}

function Set-LayerCursor([double]$lx, [double]$ly) {
    $px = [int]([math]::Round($script:ox + ($script:layerX + $lx) * $script:scale))
    $py = [int]([math]::Round($script:oy + ($script:layerY + $ly) * $script:scale))
    [Wt12]::SetCursorPos($px, $py) | Out-Null
}

function Send-Click([double]$lx, [double]$ly) {
    Refresh-Geom | Out-Null
    [Wt12]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-LayerCursor $lx $ly
    Start-Sleep -Milliseconds 250
    [Wt12]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [Wt12]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
}

# Crop the CURRENT layer rect from the server client area (recipe = vpt11).
function Save-ServerShotFast([string]$name) {
    $crect = New-Object Wt12+RECT
    [Wt12]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt12+POINT; $co.X = 0; $co.Y = 0
    [Wt12]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($crect.R - $crect.L), ($crect.B - $crect.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size)
    $g.Dispose()
    $cx = [int]([math]::Round($script:layerX * $script:scale))
    $cy = [int]([math]::Round($script:layerY * $script:scale))
    $cw = [int]([math]::Round($script:layerW * $script:scale))
    $ch = [int]([math]::Round($script:layerH * $script:scale))
    if ($cx + $cw -gt $bmp.Width) { $cw = $bmp.Width - $cx }
    if ($cy + $ch -gt $bmp.Height) { $ch = $bmp.Height - $cy }
    $rect = New-Object System.Drawing.Rectangle $cx, $cy, $cw, $ch
    $crop = $bmp.Clone($rect, $bmp.PixelFormat)
    $crop.Save((Join-Path $shotDir $name))
    $crop.Dispose(); $bmp.Dispose()
    Write-Host ("shot {0}" -f $name)
}
function Save-ServerShot([string]$name) { Refresh-Geom | Out-Null; Save-ServerShotFast $name }

# Pixel-diff count between two saved shots over a crop-relative region
# (region in PHYSICAL crop pixels; counts pixels with any-channel delta > 25).
function Count-Diff([string]$a, [string]$b, [double]$rx, [double]$ry, [double]$rw, [double]$rh) {
    $pa = Join-Path $shotDir $a; $pb = Join-Path $shotDir $b
    if (-not (Test-Path $pa) -or -not (Test-Path $pb)) { return -1 }
    $ba = New-Object System.Drawing.Bitmap $pa
    $bb = New-Object System.Drawing.Bitmap $pb
    $x0 = [int]$rx; $y0 = [int]$ry
    $x1 = [math]::Min([int]($rx + $rw), [math]::Min($ba.Width, $bb.Width))
    $y1 = [math]::Min([int]($ry + $rh), [math]::Min($ba.Height, $bb.Height))
    $n = 0
    for ($y = $y0; $y -lt $y1; $y++) {
        for ($x = $x0; $x -lt $x1; $x++) {
            $ca = $ba.GetPixel($x, $y); $cb = $bb.GetPixel($x, $y)
            if ([math]::Abs($ca.R - $cb.R) -gt 25 -or
                [math]::Abs($ca.G - $cb.G) -gt 25 -or
                [math]::Abs($ca.B - $cb.B) -gt 25) { $n++ }
        }
    }
    $ba.Dispose(); $bb.Dispose()
    return $n
}

# Glyph-white pixel count in the crop-relative top-right corner
# [cw-60..cw-6] x [2..30]: the chrome close button's white X glyph
# (240,240,240 on face (38,38,38)) only exists in window mode. Run-2
# lesson: dominant-color uniformity was marginal (baseline 0.960 vs the
# 0.95 gate) and face-color counting is useless (the video's background IS
# (38,38,38)) - white-glyph counting is binary: window mode > 10, theater
# ~0 (the corner video block is a saturated bar, never near-white).
function Glyph-White([string]$shot) {
    $p = Join-Path $shotDir $shot
    if (-not (Test-Path $p)) { return -1 }
    $bmp = New-Object System.Drawing.Bitmap $p
    $x1 = $bmp.Width - 6; $x0 = [math]::Max(0, $bmp.Width - 60)
    $n = 0
    for ($y = 2; $y -lt 31 -and $y -lt $bmp.Height; $y++) {
        for ($x = $x0; $x -lt $x1; $x++) {
            $c = $bmp.GetPixel($x, $y)
            if ($c.R -ge 200 -and $c.G -ge 200 -and $c.B -ge 200) { $n++ }
        }
    }
    $bmp.Dispose()
    return $n
}

# ---------- setup ----------
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build
Start-Sleep -Seconds 4
Check "setup: server up" ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') ""
# Media opens via the env hook (no SendKeys): OnInit reads JK_VPLAYER_OPEN.
$env:JK_VPLAYER_OPEN = $mp4
$cliErr = "$build\vpt12_client_err.txt"
Start-Process -FilePath $exe -ArgumentList "--client", "vplayer" -WorkingDirectory $build -RedirectStandardError $cliErr
Start-Sleep -Seconds 6
Check "setup: vplayer launched" (Refresh-Geom) "no Video Player layer"
Start-Sleep -Seconds 2   # decode settle (async open, D1)
# Pause for stable pixel verdicts: transport Pause at measured (28,68).
# Run-1 lesson: raw clicks without SetForegroundWindow land in whatever
# window overlaps (the console ate the click, video kept playing, every
# pixel diff was playback noise). Send-Click foregrounds the server first.
Send-Click 28 68
Start-Sleep -Milliseconds 500
# Window-mode baseline: geometry + shot (chrome X present).
if (-not (Refresh-Geom)) { Write-Host "FAIL: baseline geometry"; exit 1 }
$preX = $script:layerX; $preY = $script:layerY
$preW = $script:layerW; $preH = $script:layerH
Check "S1: baseline geometry captured" ($preW -gt 0 -and $preH -gt 0) ("{0}x{1} at ({2},{3})" -f $preW, $preH, $preX, $preY)
Save-ServerShot "vpt12-s1-baseline.png"
# Park the cursor top-left (off the controls) before the fullscreen entry.
Set-LayerCursor 12 12
Start-Sleep -Milliseconds 300

# ---------- S2: tool enter on:1 ----------
# Event delivery check rides the same toggle: the server does NOT print
# agent events to jkdesktop_run.log (PushAgentEventJson is subscribe-only),
# so capture the stream with the agent-events CLI probe (subscribe + print
# for N seconds), fire the tool inside the window, then parse the capture.
$ev1 = "$build\vpt12_events1.txt"
Start-Process -FilePath $exe -ArgumentList "agent-events","4" -WorkingDirectory $build -RedirectStandardOutput $ev1
Start-Sleep -Milliseconds 600
$r = Invoke-Agentctl ('{"tool":"window_fullscreen","args":{"id":' + $script:vplayerId + ',"on":1}}')
Check "S2: fullscreen on:1 reply ok" ($r -match '"ok"\s*:\s*true' -and $r -match '"fullscreen"\s*:\s*true') ($r.Substring(0, [math]::Min(120, $r.Length)))
Start-Sleep -Milliseconds 800
if (-not (Refresh-Geom)) { Write-Host "FAIL: theater geometry"; exit 1 }
Check "S2: geometry fullscreen" ($script:layerX -eq 0 -and $script:layerY -eq 0 -and
    $script:layerW -gt $preW -and $script:layerH -gt $preH) ("{0}x{1} at ({2},{3})" -f $script:layerW, $script:layerH, $script:layerX, $script:layerY)
Start-Sleep -Seconds 3
$evText = (Get-Content $ev1 -ErrorAction SilentlyContinue) -join "`n"
Check "S2: window.fullscreen event" ($evText -match '"topic":"window[.]fullscreen"') ""
Save-ServerShot "vpt12-s2-theater.png"
$glyphWin = Glyph-White "vpt12-s1-baseline.png"
$glyphFs  = Glyph-White "vpt12-s2-theater.png"
Check "S2: chrome X gone in theater" ($glyphWin -gt 10 -and $glyphFs -lt 5) ("baseline-glyph-white={0} theater={1}" -f $glyphWin, $glyphFs)

# ---------- S3: OSD show (bottom 22% band hover) ----------
# Video is PAUSED (S1) - the strip-region pixel diff is the OSD overlay only.
$stripY = [int]($script:layerH * 0.78) * $script:scale
Set-LayerCursor ($script:layerW * 0.5) ($script:layerH * 0.9)
Start-Sleep -Milliseconds 400
Save-ServerShotFast "vpt12-s3-osd-shown.png"
$osdPix = [int](500 * $script:scale * $script:scale)
$d = Count-Diff "vpt12-s3-osd-shown.png" "vpt12-s2-theater.png" 0 $stripY ([math]::Round($script:layerW * $script:scale)) ([math]::Round($script:layerH * $script:scale) - $stripY)
Check "S3: OSD appears in band" ($d -ge $osdPix) ("strip diff={0} (thr {1})" -f $d, $osdPix)
Write-Host "--- client osd trace ---"
Get-Content "$build\vpt12_client_err.txt" -ErrorAction SilentlyContinue | Select-String '\[vpt12\]' | Select-Object -Last 12

# ---------- S4: OSD exit button -> restore ----------
# Exit button center measured from the alpha=1.0 shot (run 5 lesson): the
# strip is 64pt at (0, h-64); the button's physical center maps to layer
# (1200, 673) at 1280x720 — i.e. (w-80, h-47). The old (w-68, h-42) landed
# on the TASKBAR (a server layer above the fullscreen client, top edge
# ~physical 847), which ate the click.
$ev2 = "$build\vpt12_events2.txt"
Start-Process -FilePath $exe -ArgumentList "agent-events","4" -WorkingDirectory $build -RedirectStandardOutput $ev2
Start-Sleep -Milliseconds 600
Refresh-Geom | Out-Null
[Wt12]::SetForegroundWindow($script:srvHwnd) | Out-Null
Start-Sleep -Milliseconds 200
Set-LayerCursor ($script:layerW - 80) ($script:layerH - 47)
Start-Sleep -Milliseconds 300
[Wt12]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 80
[Wt12]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 800
$geomOk = $false; $gdetail = "no layer"
if (Refresh-Geom) {
    $geomOk = ([math]::Abs($script:layerX - $preX) -le 3 -and [math]::Abs($script:layerY - $preY) -le 3 -and
               [math]::Abs($script:layerW - $preW) -le 3 -and [math]::Abs($script:layerH - $preH) -le 3)
}
Check "S4: exit button restores geometry" $geomOk ("restored {0}x{1} at ({2},{3})" -f $script:layerW, $script:layerH, $script:layerX, $script:layerY)
Start-Sleep -Seconds 3
$evText = (Get-Content $ev2 -ErrorAction SilentlyContinue) -join "`n"
Check "S4: window.fullscreen_exit event" ($evText -match '"topic":"window[.]fullscreen_exit"') ""
Save-ServerShot "vpt12-s4-restored.png"

# ---------- S5: re-enter + OSD idle hide ----------
$r = Invoke-Agentctl ('{"tool":"window_fullscreen","args":{"id":' + $script:vplayerId + ',"on":1}}')
Check "S5: re-enter fullscreen" ($r -match '"ok"\s*:\s*true' -and $r -match '"fullscreen"\s*:\s*true') ""
Start-Sleep -Milliseconds 800
Refresh-Geom | Out-Null
Set-LayerCursor ($script:layerW * 0.5) ($script:layerH * 0.9)
Start-Sleep -Milliseconds 400
Save-ServerShotFast "vpt12-s5-osd-shown.png"
Set-LayerCursor 12 12   # park OUT of the band: fade-out starts (2.5 s idle + 200 ms fade)
Start-Sleep -Milliseconds 3400
Save-ServerShotFast "vpt12-s5-osd-hidden.png"
$cw = [math]::Round($script:layerW * $script:scale)
$chh = [math]::Round($script:layerH * $script:scale)
$dShown = Count-Diff "vpt12-s5-osd-shown.png" "vpt12-s2-theater.png" 0 $stripY $cw ($chh - $stripY)
$dGone  = Count-Diff "vpt12-s5-osd-hidden.png" "vpt12-s5-osd-shown.png" 0 $stripY $cw ($chh - $stripY)
$dBack  = Count-Diff "vpt12-s5-osd-hidden.png" "vpt12-s2-theater.png" 0 $stripY $cw ($chh - $stripY)
$hidePix = [int](200 * $script:scale * $script:scale)
Check "S5: OSD shown on re-enter" ($dShown -ge $osdPix) ("diff={0} (thr {1})" -f $dShown, $osdPix)
Check "S5: OSD hidden after 3.4 s idle" ($dGone -ge $osdPix -and $dBack -le $hidePix) ("shown-diff={0} gone-diff={1} back-diff={2} (hide thr {3})" -f $dShown, $dGone, $dBack, $hidePix)

# ---------- S6: tool exit on:0 ----------
$ev3 = "$build\vpt12_events3.txt"
Start-Process -FilePath $exe -ArgumentList "agent-events","4" -WorkingDirectory $build -RedirectStandardOutput $ev3
Start-Sleep -Milliseconds 600
$r = Invoke-Agentctl ('{"tool":"window_fullscreen","args":{"id":' + $script:vplayerId + ',"on":0}}')
Check "S6: fullscreen on:0 reply ok" ($r -match '"ok"\s*:\s*true' -and $r -match '"fullscreen"\s*:\s*false') ""
Start-Sleep -Milliseconds 800
$geomOk2 = $false
if (Refresh-Geom) {
    $geomOk2 = ([math]::Abs($script:layerX - $preX) -le 3 -and [math]::Abs($script:layerY - $preY) -le 3 -and
                [math]::Abs($script:layerW - $preW) -le 3 -and [math]::Abs($script:layerH - $preH) -le 3)
}
Check "S6: geometry restored" $geomOk2 ("{0}x{1} at ({2},{3})" -f $script:layerW, $script:layerH, $script:layerX, $script:layerY)
Start-Sleep -Seconds 3
$evText = (Get-Content $ev3 -ErrorAction SilentlyContinue) -join "`n"
Check "S6: second exit event" ($evText -match '"topic":"window[.]fullscreen_exit"') ""

# ---------- teardown ----------
Write-Host ("RESULT: " + $(if ($script:fails -eq 0) { "ALL PASS" } else { "$($script:fails) FAILURE(S)" }))
exit $(if ($script:fails -eq 0) { 0 } else { 1 })