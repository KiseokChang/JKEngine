# vpt10 e2e: NV12 upload path 4K verdict (docs/50 section 7.4-1 as-built,
# spec/plan 2026-09-15). Harness recipe = vpt9_jogframescrub.ps1 (DPI-aware,
# full-layer crop, measured geometry).
#
# Test media (tmp/ is git-ignored; regenerate with):
#   ffmpeg -f lavfi -i "testsrc2=size=3840x2160:rate=50,format=yuv420p" ^
#          -f lavfi -i "sine=frequency=440:sample_rate=48000" ^
#          -t 20 -c:v libx264 -preset veryfast -crf 30 -pix_fmt yuv420p ^
#          -c:a aac tmp/vpt10_4k.mp4
#   ffmpeg -f lavfi -i "testsrc2=size=481x271:rate=30" -t 5 tmp/vpt10_odd.mp4
#
# The 4K clip burns "00:00:SS.mmm" + frame number (50 fps) into the top-left
# of the picture; the odd clip is testsrc2 at odd dimensions (481x271 -> the
# dimension gate must pick the RGBA fallback).
#
# Verdicts are read OFF THE SHOTS by the executing agent with the Read tool
# (lesson 19: never OCR glyphs by regex; transcribe counters/gauges by eye).
# The probe itself only checks structure (aliveness, responsiveness, state).
# For the small 4K burned glyphs the executor crop-zooms the saved PNGs with
# ffmpeg before reading.
#
# Scenarios (vplayer AUTOPLAYS on open - state anchor, no state-reading in
# script; every transport click below is placed against that anchor):
# S0 setup: 4K file open -> autoplaying (async open, T1).
# S1 render gauge: NO interaction; 3 shots 2 s apart while PLAYING (the gauge
#    keeps its last posted value while paused - known, brief-mandated).
#    Executor reads the "render NNHz" gauge (Korean label) off the status row.
#    After the PopVideoFrame pacing fix (docs/50 section 9.4, task-4: drops
#    only truly-expired frames + videoQ cap 3 -> 6) the measured 4K playback
#    gauge is ~32-39 Hz (counted ok 29-43/s). A reading in ~28-42 Hz is
#    in-family. The brief's 45-50 Hz bar is NOT reached: the residual cap is
#    SUPPLY-side (the decode clock gate emits frames just-in-time, so each
#    burst's tail arrives already expired; avg push lateness +5..+16 ms at
#    4K, +22..+30 ms at 480p) - not the display drop rule. The jog path
#    (47-59 Hz, no clock gate) remains the chain's upper bound. Do NOT treat
#    a sub-40 reading as a probe failure.
#    (pre-fix baseline: 16-31 Hz over 3 runs - latest-wins drop consumed each
#    decode burst as 1 display + N-1 discards).
# S2 color bars: testsrc2 full-frame saturated bars (R/G/Y/B/M/C) on the
#    NV12 YUV->RGB shader path - no chroma distortion, checked against an
#    ffmpeg-extracted reference frame (tmp/ref4k_5s.png).
# S3 jog ring hit: click Pause (now deterministically paused), wheel UP 5
#    ticks (ring fills forward), short settle (250 ms - under the 400 ms
#    release idle), then wheel DOWN 5 ticks with NO settle - shot must
#    already show the burned frame back at the start point (ring hit =
#    instant display; a keyframe fallback seek would land late). Release
#    restores the pre-scrub PAUSED state; then Play -> clock advances.
#    Dial math: dur 20 s -> sPerRev = clamp(20/8,1,30) = 2.5 s; one tick =
#    1/16 rev = 0.156 s = ~7.8 frames at 50 fps; 5 ticks ~ 0.78 s (~39 f).
# S4 play/pause toggle + restore (one click each way, anchor still known).
# S5 odd-dimension fallback: open tmp/vpt10_odd.mp4 -> picture renders
#    correctly (RGBA fallback path).
# S6 control: open the small 480x270@30 clip (vpt2_test.mp4) and read the
#    gauge while PLAYING - isolates "4K-specific ceiling" from a global
#    playback pacing cap (30 fps content, so ~30 = content-limited).
param()
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wt10 {
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
[Wt10]::SetProcessDPIAware() | Out-Null

$build   = "I:\progwork\JKENGINE\engine\build"
$exe     = "$build\jkdesktop.exe"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-15-vplayer-nv12-upload\shots"
$mp4     = "I:/progwork/JKENGINE/tmp/vpt10_4k.mp4"
$mp4odd  = "I:/progwork/JKENGINE/tmp/vpt10_odd.mp4"
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

function Find-Server {
    $h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
        Select-Object -First 1).MainWindowHandle
    if (-not $h) { Write-Host "FAIL: NO SERVER WINDOW"; exit 1 }
    $script:srvHwnd = [IntPtr]$h
    $crect = New-Object Wt10+RECT
    [Wt10]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt10+POINT; $co.X = 0; $co.Y = 0
    [Wt10]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
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
# The shell relocates the window shortly after spawn - re-sync server + layer
# geometry before every interaction.
function Refresh-Geom {
    Find-Server
    for ($i = 0; $i -lt 10; $i++) { if (Find-VPlayerLayer) { return $true }; Start-Sleep -Milliseconds 300 }
    return $false
}

function Set-LayerCursor([double]$lx, [double]$ly) {
    $px = [int]([math]::Round($script:ox + ($script:layerX + $lx) * $script:scale))
    $py = [int]([math]::Round($script:oy + ($script:layerY + $ly) * $script:scale))
    [Wt10]::SetCursorPos($px, $py) | Out-Null
}

function Send-Click([double]$lx, [double]$ly, [uint32]$down, [uint32]$up) {
    Refresh-Geom | Out-Null
    [Wt10]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-LayerCursor $lx $ly
    Start-Sleep -Milliseconds 250
    [Wt10]::mouse_event($down, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [Wt10]::mouse_event($up, 0, 0, 0, [UIntPtr]::Zero)
}
function Click-App([double]$lx, [double]$ly) { Send-Click $lx $ly 2 4 }

function Type-IntoPath([string]$path) {
    Click-App 400 42
    Start-Sleep -Milliseconds 300
    [Wt10]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    # Direct typing (clipboard paste does not reach the InputText in this
    # environment - vpt9 diag; the media path has no SendKeys special chars).
    [System.Windows.Forms.SendKeys]::SendWait("{END}")
    Start-Sleep -Milliseconds 100
    [System.Windows.Forms.SendKeys]::SendWait("{BS 300}")
    Start-Sleep -Milliseconds 150
    [System.Windows.Forms.SendKeys]::SendWait($path)
    Start-Sleep -Milliseconds 400
}
function Click-Open { Click-App 808 42 }

function Save-ServerShotFast([string]$name) {
    $crect = New-Object Wt10+RECT
    [Wt10]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt10+POINT; $co.X = 0; $co.Y = 0
    [Wt10]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
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

function Send-Wheel([int]$ticks, [int]$delayMs) {
    $d = $(if ($ticks -ge 0) { 120 } else { -120 })
    for ($i = 0; $i -lt [math]::Abs($ticks); $i++) {
        [Wt10]::mouse_event(0x0800, 0, 0, $d, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds $delayMs
    }
}

function Hover-Knob {
    Refresh-Geom | Out-Null
    [Wt10]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-LayerCursor 888 582
    Start-Sleep -Milliseconds 250
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
# 4K50 async open + first decode is heavier than the 480p clips - give it time.
Start-Sleep -Seconds 10
Check "S0: alive after 4K open" (VPlayer-Alive) ""
Save-ServerShot "vpt10-s0-open.png"

# ---------- S1: render gauge while PLAYING (no interaction) ----------
Start-Sleep -Seconds 4     # settle into steady state past the open burst
Save-ServerShot "vpt10-s1-renderA.png"   # playing (anchor: autoplay)
Start-Sleep -Seconds 2
Save-ServerShot "vpt10-s1-renderB.png"   # clock advanced, gauge re-sampled
Start-Sleep -Seconds 2
Save-ServerShot "vpt10-s1-renderC.png"   # third sample
$p = Ping-Ms
Check "S1: responsive during 4K playback" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S1: no crash" (VPlayer-Alive) ""

# ---------- S2: color bars (NV12 YUV->RGB shader path) ----------
Save-ServerShot "vpt10-s2-color.png"    # still playing; bars are static
$p = Ping-Ms
Check "S2: responsive after color shot" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)

# ---------- S3: jog ring hit (up 5 / down 5, instant reverse display) ----------
# By S3 the 20 s clip has played to ~19 s (EOF) - seek back to mid-clip first
# so the jog session runs mid-clip, not EOF-clamped.
Click-App (150.0 + 690.0 * 0.25) 77   # ~5 s
Start-Sleep -Milliseconds 1800        # seek landing rebuilds the ring
Click-App 28 68            # anchor is PLAYING -> this PAUSES
Start-Sleep -Milliseconds 600
Hover-Knob
Save-ServerShotFast "vpt10-s3-base.png"
Send-Wheel 5 150           # +0.78 s: ring fills forward from here
Start-Sleep -Milliseconds 250              # stay INSIDE the session (<400 ms idle)
Save-ServerShotFast "vpt10-s3-up5.png"
Send-Wheel -5 25           # back through the ring, 25 ms cadence
Save-ServerShotFast "vpt10-s3-down5.png"   # NO settle: ring hit must be instant
Start-Sleep -Milliseconds 700              # 400 ms idle release fires here
Save-ServerShotFast "vpt10-s3-released.png"   # pre-scrub PAUSED restored
Click-App 28 68            # Play
Start-Sleep -Milliseconds 1500
Save-ServerShotFast "vpt10-s3-playing.png" # clock advanced
$p = Ping-Ms
Check "S3: responsive after jog ring hit" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S3: no crash" (VPlayer-Alive) ""

# ---------- S4: play/pause toggle + restore ----------
Click-App 28 68            # Pause
Start-Sleep -Milliseconds 600
Save-ServerShotFast "vpt10-s4-paused.png"
Click-App 28 68            # Play
Start-Sleep -Milliseconds 1500
Save-ServerShotFast "vpt10-s4-resumed.png"
$p = Ping-Ms
Check "S4: responsive after toggle" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S4: no crash" (VPlayer-Alive) ""

# ---------- S5: odd-dimension RGBA fallback ----------
Type-IntoPath $mp4odd
Click-Open
Start-Sleep -Seconds 5
Check "S5: alive after odd-dim open" (VPlayer-Alive) ""
Save-ServerShot "vpt10-s5-odd.png"
$p = Ping-Ms
Check "S5: responsive after odd-dim open" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)

# ---------- S6: control - small clip gauge while PLAYING ----------
Type-IntoPath "I:/progwork/JKENGINE/tmp/vpt2_test.mp4"
Click-Open
Start-Sleep -Seconds 6
Check "S6: alive after small-clip open" (VPlayer-Alive) ""
Save-ServerShot "vpt10-s6-smallA.png"    # autoplaying
Start-Sleep -Seconds 2
Save-ServerShot "vpt10-s6-smallB.png"
$p = Ping-Ms
Check "S6: responsive after small-clip open" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)

# ---------- teardown ----------
Write-Host ("RESULT: " + $(if ($script:fails -eq 0) { "ALL PASS" } else { "$($script:fails) FAILURE(S)" }))
exit $(if ($script:fails -eq 0) { 0 } else { 1 })
