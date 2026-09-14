# vplayer-stability T4 e2e: jog-knob mouse-wheel scrubbing (spec 1d, D5).
# -STA required (Set-Clipboard, docs/15 cp949 convention). Harness recipe =
# vpt3_e2e.ps1 (DPI-aware, full-layer crop, measured geometry).
#
# Test media: tmp/vpt2_test.mp4 (480x270@30, 30 s) — it burns a timecode
# ("00:00:SS.mmm" + frame no.) into the top-left of the picture, so frame
# snap after the 400 ms idle release is readable straight off the shots, and
# the UI clock ("MM:SS / MM:SS", top right) gives the scrub direction.
#
# Geometry (measured on the vpt3 crop, 1.25 scale): transport row Pause
# button center ~(28, 68); ##seek slider row y~77 (vpt2 formula kept);
# jog knob center ~(888, 582), radius 32 — hover calibration shot S0
# verifies the rim highlight before any wheel scenario runs.
#
# S0 calibration: knob hover -> rim brightens (hot state verified on shot).
# S1 wheel scrub from PLAYING: 40 ticks up (~+9.4 s at sPerRev=3.75,
#    1/16 rev/tick) -> mid-scrub shot shows auto-pause (button "Play"),
#    after 400 ms the release restores playing (button "Pause") and the
#    clock keeps advancing. Direction: wheel-up = forward.
# S2 wheel scrub from PAUSED: 40 ticks down from t~24 s -> time rewinds,
#    stays paused through the release and after.
# S3 drag + wheel mixed on the knob: wheel ticks mid-drag and drag after
#    wheel (latest wins) — no fighting, no crash.
# S4 regression: drag-only scrub (cw = forward, ccw = backward) + playback.
param()
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wt4 {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, int d, UIntPtr e);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
[Wt4]::SetProcessDPIAware() | Out-Null

$build   = "I:\progwork\JKENGINE\engine\build"
$exe     = "$build\jkdesktop.exe"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-14-vplayer-stability"
$mp4     = "I:/progwork/JKENGINE/tmp/vpt2_test.mp4"
$script:fails = 0

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

function Find-Server {
    $h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
        Select-Object -First 1).MainWindowHandle
    if (-not $h) { Write-Host "FAIL: NO SERVER WINDOW"; exit 1 }
    $script:srvHwnd = [IntPtr]$h
    $crect = New-Object Wt4+RECT
    [Wt4]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt4+POINT; $co.X = 0; $co.Y = 0
    [Wt4]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
    $script:scale = ($crect.R - $crect.L) / 1280.0
    $script:ox = $co.X; $script:oy = $co.Y
}

function Find-VPlayerLayer {
    $list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    $m = [regex]::Match($list, '"title":"Video Player".*?"x":(-?\d+),"y":(-?\d+),"w":(\d+),"h":(\d+)')
    if (-not $m.Success) { return $false }
    $script:layerX = [int]$m.Groups[1].Value
    $script:layerY = [int]$m.Groups[2].Value
    $script:layerW = [int]$m.Groups[3].Value
    $script:layerH = [int]$m.Groups[4].Value
    return $true
}
function VPlayer-Alive { return (Find-VPlayerLayer) }
# The shell relocates the window shortly after spawn ((0,0) -> (180,40)
# observed) — re-sync server + layer geometry before every interaction.
function Refresh-Geom {
    Find-Server
    for ($i = 0; $i -lt 10; $i++) { if (Find-VPlayerLayer) { return $true }; Start-Sleep -Milliseconds 300 }
    return $false
}

# Cursor to layer-relative surface coords (physical pixels).
function Set-LayerCursor([double]$lx, [double]$ly) {
    $px = [int]([math]::Round($script:ox + ($script:layerX + $lx) * $script:scale))
    $py = [int]([math]::Round($script:oy + ($script:layerY + $ly) * $script:scale))
    [Wt4]::SetCursorPos($px, $py) | Out-Null
}

function Send-Click([double]$lx, [double]$ly, [uint32]$down, [uint32]$up) {
    Refresh-Geom | Out-Null
    [Wt4]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-LayerCursor $lx $ly
    Start-Sleep -Milliseconds 250
    [Wt4]::mouse_event($down, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [Wt4]::mouse_event($up, 0, 0, 0, [UIntPtr]::Zero)
}
function Click-App([double]$lx, [double]$ly) { Send-Click $lx $ly 2 4 }

function Type-IntoPath([string]$path) {
    Click-App 400 42
    Start-Sleep -Milliseconds 300
    [Wt4]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    # Direct typing (vpt9 diag 2026-09-15: clipboard ^v paste stopped reaching
    # the InputText in this environment; SendKeys typing works and the media
    # paths contain no SendKeys special chars).
    [System.Windows.Forms.SendKeys]::SendWait("{END}")
    Start-Sleep -Milliseconds 100
    [System.Windows.Forms.SendKeys]::SendWait("{BS 300}")
    Start-Sleep -Milliseconds 150
    [System.Windows.Forms.SendKeys]::SendWait($path)
    Start-Sleep -Milliseconds 400
}
function Click-Open { Click-App 808 42 }

# Fast shot: cached server geometry (used mid-scrub where the 400 ms window
# matters — Find-Server's agentctl round trip would eat the window).
function Save-ServerShotFast([string]$name) {
    $crect = New-Object Wt4+RECT
    [Wt4]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt4+POINT; $co.X = 0; $co.Y = 0
    [Wt4]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
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

function Ping-Ms {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $r = Invoke-Agentctl '{"tool":"ping","args":{}}'
    $sw.Stop()
    if ($r -notmatch '"ok"\s*:\s*true') { return -1.0 }
    return $sw.Elapsed.TotalMilliseconds
}

# Slider seek: commit a click at parameter f of the ##seek slider track.
function Seek-Frac([double]$f) { Click-App (150.0 + 690.0 * $f) 77 }

# Wheel ticks over the current cursor position: one notch = +-120.
function Send-Wheel([int]$ticks) {
    $d = $(if ($ticks -ge 0) { 120 } else { -120 })
    for ($i = 0; $i -lt [math]::Abs($ticks); $i++) {
        [Wt4]::mouse_event(0x0800, 0, 0, $d, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 25
    }
}

# Hover the knob center (surface coords, measured).
function Hover-Knob {
    Refresh-Geom | Out-Null
    [Wt4]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-LayerCursor 888 582
    Start-Sleep -Milliseconds 250
}

# Drag the knob: press at the right of center, sweep |turns| revolutions
# (positive = clockwise on screen), release. -WheelMid injects 8 wheel ticks
# at the half-way point while the button is held.
function Drag-Knob([double]$turns, [switch]$WheelMid) {
    $cx = 888.0; $cy = 582.0; $r = 20.0
    Refresh-Geom | Out-Null
    [Wt4]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-LayerCursor ($cx + $r) $cy
    Start-Sleep -Milliseconds 250
    [Wt4]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 120
    $steps = 24
    $total = [math]::Abs($turns) * 2.0 * [math]::PI
    $dir = $(if ($turns -ge 0) { 1.0 } else { -1.0 })
    for ($i = 1; $i -le $steps; $i++) {
        $a = $total * $dir * $i / $steps
        Set-LayerCursor ($cx + $r * [math]::Cos($a)) ($cy + $r * [math]::Sin($a))
        Start-Sleep -Milliseconds 40
        if ($WheelMid -and $i -eq 12) { Send-Wheel 8 }
    }
    Start-Sleep -Milliseconds 120
    [Wt4]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
}

# ---------- setup ----------
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build
Start-Sleep -Seconds 4
Check "setup: server up" ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') ""
Invoke-Agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
Start-Sleep -Seconds 6
Check "setup: vplayer launched" (Refresh-Geom) "no Video Player layer"
Type-IntoPath $mp4
Click-Open
Start-Sleep -Seconds 4

# ---------- S0: knob hover calibration ----------
Save-ServerShot "vpt4-s0-playing.png"
Hover-Knob
Save-ServerShot "vpt4-s0-knob-hover.png"
Check "S0: alive after hover" (VPlayer-Alive) ""

# ---------- S1: wheel scrub from playing (40 ticks up = ~+9.4 s) ----------
Hover-Knob
Start-Sleep -Milliseconds 600
Save-ServerShotFast "vpt4-s1a-before.png"
Send-Wheel 40
Save-ServerShotFast "vpt4-s1b-mid-scrub.png"   # must land inside the 400 ms window
Start-Sleep -Milliseconds 700                  # idle release fires here
Save-ServerShotFast "vpt4-s1c-after-release.png"
Start-Sleep -Seconds 2
Save-ServerShotFast "vpt4-s1d-playing-on.png"
$p = Ping-Ms
Check "S1: responsive after wheel scrub" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S1: no crash" (VPlayer-Alive) ""

# ---------- S2: wheel scrub from paused (40 ticks down = ~-9.4 s) ----------
Seek-Frac 0.8
Start-Sleep -Milliseconds 800
Click-App 28 68        # Pause
Start-Sleep -Milliseconds 500
Hover-Knob
Save-ServerShotFast "vpt4-s2a-paused-before.png"
Send-Wheel -40
Save-ServerShotFast "vpt4-s2b-mid-scrub.png"
Start-Sleep -Milliseconds 700
Save-ServerShotFast "vpt4-s2c-after-release.png"
Start-Sleep -Seconds 2
Save-ServerShotFast "vpt4-s2d-still-paused.png"
$p = Ping-Ms
Check "S2: responsive after paused scrub" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S2: no crash" (VPlayer-Alive) ""

# ---------- S3: drag + wheel mixed (latest wins) ----------
Seek-Frac 0.1
Start-Sleep -Milliseconds 800
Drag-Knob 1.5 -WheelMid          # wheel ticks injected mid-drag
Start-Sleep -Milliseconds 700
Save-ServerShotFast "vpt4-s3a-mixed.png"
Hover-Knob
Send-Wheel 20                    # wheel burst...
Drag-Knob 0.5                     # ...immediately overtaken by a drag
Start-Sleep -Milliseconds 700
Save-ServerShotFast "vpt4-s3b-latest-wins.png"
$p = Ping-Ms
Check "S3: responsive after drag+wheel mix" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S3: no crash" (VPlayer-Alive) ""

# ---------- S4: regression — drag-only scrub + normal playback ----------
Seek-Frac 0.5
Start-Sleep -Milliseconds 800
Drag-Knob 2.0                    # clockwise = forward
Start-Sleep -Milliseconds 700
Save-ServerShotFast "vpt4-s4a-drag-cw.png"
Drag-Knob -2.0                   # counter-clockwise = backward
Start-Sleep -Milliseconds 700
Save-ServerShotFast "vpt4-s4b-drag-ccw.png"
Start-Sleep -Seconds 2
Save-ServerShotFast "vpt4-s4c-playing-on.png"
$p = Ping-Ms
Check "S4: responsive after drag scrub" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S4: no crash" (VPlayer-Alive) ""

# ---------- teardown ----------
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Write-Host ("RESULT: " + $(if ($script:fails -eq 0) { "ALL PASS" } else { "$($script:fails) FAILURE(S)" }))
exit $(if ($script:fails -eq 0) { 0 } else { 1 })