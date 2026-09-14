# vpt9 e2e: jog dial = silent frame-scrub (docs/50 section 8, spec 2026-09-15).
# Harness recipe = vpt4_e2e.ps1 (DPI-aware, full-layer crop, measured geometry).
#
# Mechanism under test (commits 6a03dd1/eaf6e1f): the jog dial (knob drag +
# wheel scrub) is now a frame-scrub. Session start auto-pauses + SetJog(true)
# (audio decode skipped = fully silent); the video thread's clock gate decodes
# to the dial target (jogTargetPts); the UI displays the newest decoded frame
# <= target from the jog history ring (10 s / 1.5 GB caps). Crossing the ring
# start falls back to the old 40 ms-debounced keyframe SeekScrub; release does
# the precision seek (finishScrub) and restores the pre-scrub pause state.
#
# NOTE (documented-only): audio silence during a jog session is guaranteed by
# construction (jogging skips audio RingPush + paused device) and has no
# external observation point in the UI -- the spec's verification item 4 stays
# a user-measured item, asserted here by header comment only.
#
# Test media: tmp/vpt2_test.mp4 (480x270@30, 30 s) burns "00:00:SS.mmm" +
# frame number into the top-left of the picture and "MM:SS / MM:SS" into the
# UI clock (top right). Frame continuity verdicts are read OFF THE SHOTS by
# the executing agent with the Read tool (lesson 19: never OCR glyphs by
# regex; compare counters). The probe itself only checks structure
# (aliveness, responsiveness, state machine sanity).
#
# Geometry (measured, 1.25 scale layer crop): transport Pause/Play center
# ~(28, 68); seek slider row y~77; jog knob center ~(888, 582) (64 px disc,
# kmin = vidMax-(80,80)); step buttons "<" ~(793, 582), ">" ~(824, 582)
# (30x22 at kmin.x-78, c.y-11 + SameLine).
#
# Dial math: sPerRev = clamp(dur/8, 1, 30) = 3.75 s for the 30 s clip; one
# wheel tick = 1/16 rev = 0.234 s = ~7.03 frames at 30 fps. Wheel-up =
# forward, wheel-down = reverse.
#
# S0 calibration: knob hover -> rim brightens (verified on shot).
# S1 forward scrub from PAUSED: 1 tick shot (first-tick lag data point,
#    deferred minor a) then 8+8+8 ticks at 150 ms/tick -> 3 round shots.
#    Verdict (from shots): burned frame number advances ~7/tick (~56/round),
#    monotonically, no keyframe-scale random jumps.
# S2 reverse inside the ring is IMMEDIATE: 5 ticks down, shot taken with no
#    settle -> frame number already ~35 lower. A fallback seek would show a
#    landing delay instead (no settle = old frame).
# S3 ring-boundary fallback from a LONG forward scrub: paused seek to 15 s,
#    wheel up 60 ticks (~+14 s, ring trimmed to ~[19,29]) then 60 ticks down
#    (~-14 s -> target 15 s < ring start) -> fallback keyframe seek lands
#    near the target; playback state (paused) maintained after release.
# S4 release: wheel session from PLAYING -> mid-shot shows auto-pause, after
#    400 ms idle the release precision-seeks (clock = snap frame) and playing
#    resumes (clock advances ~2 s by the last shot).
# S5 regression: Play/Pause toggle, "<"/">" +-1F step buttons (frame number
#    -1 / +1 / +2 on shots), normal playback resumes.
param()
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wt9 {
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
[Wt9]::SetProcessDPIAware() | Out-Null

$build   = "I:\progwork\JKENGINE\engine\build"
$exe     = "$build\jkdesktop.exe"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-15-vplayer-jog-framescrub\shots"
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

function Find-Server {
    $h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
        Select-Object -First 1).MainWindowHandle
    if (-not $h) { Write-Host "FAIL: NO SERVER WINDOW"; exit 1 }
    $script:srvHwnd = [IntPtr]$h
    $crect = New-Object Wt9+RECT
    [Wt9]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt9+POINT; $co.X = 0; $co.Y = 0
    [Wt9]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
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
# observed) - re-sync server + layer geometry before every interaction.
function Refresh-Geom {
    Find-Server
    for ($i = 0; $i -lt 10; $i++) { if (Find-VPlayerLayer) { return $true }; Start-Sleep -Milliseconds 300 }
    return $false
}

# Cursor to layer-relative surface coords (physical pixels).
function Set-LayerCursor([double]$lx, [double]$ly) {
    $px = [int]([math]::Round($script:ox + ($script:layerX + $lx) * $script:scale))
    $py = [int]([math]::Round($script:oy + ($script:layerY + $ly) * $script:scale))
    [Wt9]::SetCursorPos($px, $py) | Out-Null
}

function Send-Click([double]$lx, [double]$ly, [uint32]$down, [uint32]$up) {
    Refresh-Geom | Out-Null
    [Wt9]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-LayerCursor $lx $ly
    Start-Sleep -Milliseconds 250
    [Wt9]::mouse_event($down, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [Wt9]::mouse_event($up, 0, 0, 0, [UIntPtr]::Zero)
}
function Click-App([double]$lx, [double]$ly) { Send-Click $lx $ly 2 4 }

function Type-IntoPath([string]$path) {
    Click-App 400 42
    Start-Sleep -Milliseconds 300
    [Wt9]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    # Direct typing (diag: clipboard ^v paste did not reach the InputText in
    # this session, SendKeys typing does; the media path has no SendKeys
    # special chars).
    [System.Windows.Forms.SendKeys]::SendWait("{END}")
    Start-Sleep -Milliseconds 100
    [System.Windows.Forms.SendKeys]::SendWait("{BS 300}")
    Start-Sleep -Milliseconds 150
    [System.Windows.Forms.SendKeys]::SendWait($path)
    Start-Sleep -Milliseconds 400
}
function Click-Open { Click-App 808 42 }

# Fast shot: cached server geometry (used mid-scrub where settle windows
# matter - the agentctl round trip would eat them).
function Save-ServerShotFast([string]$name) {
    $crect = New-Object Wt9+RECT
    [Wt9]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt9+POINT; $co.X = 0; $co.Y = 0
    [Wt9]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
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
# delayMs between ticks (25 = vpt4 burst, 150 = decode-friendly cadence).
function Send-Wheel([int]$ticks, [int]$delayMs) {
    $d = $(if ($ticks -ge 0) { 120 } else { -120 })
    for ($i = 0; $i -lt [math]::Abs($ticks); $i++) {
        [Wt9]::mouse_event(0x0800, 0, 0, $d, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds $delayMs
    }
}

# Hover the knob center (surface coords, measured).
function Hover-Knob {
    Refresh-Geom | Out-Null
    [Wt9]::SetForegroundWindow($script:srvHwnd) | Out-Null
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
Start-Sleep -Seconds 4
Check "setup: media open" (VPlayer-Alive) ""

# ---------- S0: knob hover calibration ----------
Save-ServerShot "vpt9-s0-base.png"
Hover-Knob
Save-ServerShot "vpt9-s0-knob-hover.png"
Check "S0: alive after hover" (VPlayer-Alive) ""

# ---------- S1: forward frame-scrub from PAUSED ----------
Seek-Frac 0.167            # ~5 s
Start-Sleep -Milliseconds 800
Click-App 28 68            # Pause
Start-Sleep -Milliseconds 500
Hover-Knob
Save-ServerShotFast "vpt9-s1-base.png"
Send-Wheel 1 150           # single first tick (first-tick lag data point)
Start-Sleep -Milliseconds 100
Save-ServerShotFast "vpt9-s1-t1.png"
Send-Wheel 7 150
Save-ServerShotFast "vpt9-s1-r1.png"
Send-Wheel 8 150
Save-ServerShotFast "vpt9-s1-r2.png"
Send-Wheel 8 150
Save-ServerShotFast "vpt9-s1-r3.png"
$p = Ping-Ms
Check "S1: responsive after forward scrub" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S1: no crash" (VPlayer-Alive) ""

# ---------- S2: reverse inside the ring is IMMEDIATE ----------
Save-ServerShotFast "vpt9-s2-before.png"
Send-Wheel -5 25
Save-ServerShotFast "vpt9-s2-after.png"   # NO settle: fallback would not have landed yet
$p = Ping-Ms
Check "S2: responsive after reverse ticks" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S2: no crash" (VPlayer-Alive) ""

# ---------- S3: ring-boundary fallback from a long forward scrub ----------
Click-App 28 68            # ensure paused
Start-Sleep -Milliseconds 400
Seek-Frac 0.5              # ~15 s: seek clears the ring, landing rebuilds it
Start-Sleep -Milliseconds 1500
Hover-Knob
Save-ServerShotFast "vpt9-s3-a.png"
Send-Wheel 60 25           # ~+14 s -> target ~29 s, ring trimmed to ~[19,29]
Start-Sleep -Milliseconds 400
Save-ServerShotFast "vpt9-s3-b.png"
Send-Wheel -60 25          # ~-14 s -> target ~15 s < ring start -> fallback seek
Start-Sleep -Milliseconds 1500
Save-ServerShotFast "vpt9-s3-c.png"
$p = Ping-Ms
Check "S3: responsive after ring-miss fallback" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S3: no crash" (VPlayer-Alive) ""

# ---------- S4: release = precision seek + playback restore ----------
Click-App 28 68            # Play
Start-Sleep -Milliseconds 500
Hover-Knob
Save-ServerShotFast "vpt9-s4-before.png"
Send-Wheel 10 25           # +2.34 s
Save-ServerShotFast "vpt9-s4-mid.png"     # inside session: auto-paused
Start-Sleep -Milliseconds 700             # 400 ms idle release fires here
Save-ServerShotFast "vpt9-s4-after.png"   # released: playing restored, snap frame
Start-Sleep -Seconds 2
Save-ServerShotFast "vpt9-s4-playing.png" # clock advanced ~2 s
$p = Ping-Ms
Check "S4: responsive after release" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S4: no crash" (VPlayer-Alive) ""

# ---------- S5: regression - play/pause + +-1F step buttons ----------
Click-App 28 68            # Pause
Start-Sleep -Milliseconds 600
Save-ServerShotFast "vpt9-s5-base.png"
Click-App 793 582          # "<" = -1F
Start-Sleep -Milliseconds 600
Save-ServerShotFast "vpt9-s5-minus.png"
Click-App 824 582          # ">" = +1F
Start-Sleep -Milliseconds 600
Save-ServerShotFast "vpt9-s5-plus1.png"
Click-App 824 582          # ">" = +1F again
Start-Sleep -Milliseconds 600
Save-ServerShotFast "vpt9-s5-plus2.png"
Click-App 28 68            # Play
Start-Sleep -Milliseconds 1500
Save-ServerShotFast "vpt9-s5-resumed.png"
$p = Ping-Ms
Check "S5: responsive after +-1F steps" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S5: no crash" (VPlayer-Alive) ""

# ---------- teardown ----------
# Server left running: vpt4/vpt5 regression probes restart it themselves;
# the caller restores the pre-probe server state.
Write-Host ("RESULT: " + $(if ($script:fails -eq 0) { "ALL PASS" } else { "$($script:fails) FAILURE(S)" }))
exit $(if ($script:fails -eq 0) { 0 } else { 1 })