# vpt11 e2e: reverse auto-play (docs/50 sec 10, spec 2026-09-15 section 7 v2).
# Harness recipe = vpt9_jogframescrub.ps1 (helpers copied verbatim).
# Scenarios: S0 button calibration, S1 reverse cadence (frame numbers
# DECREASE ~15/500ms at 30fps, clock frozen by auto-pause), S2 ring
# exhaustion fallback (decrease continues past the ring start, no stall),
# S3 auto-finish at 0 (clock ~0, playback restored), S4 toggle-off precision
# seek (clock snaps to the displayed frame), S5 drag takeover + step buttons.
# Frame-number verdicts are read OFF THE SHOTS by the executing agent with
# the Read tool (lesson 19); the probe itself only checks structure
# (aliveness, responsiveness).
#
# Geometry (measured, 1.25 scale layer crop; RE-MEASURED on the S0 shot):
# transport Pause/Play center ~(28, 68); seek slider row y~77; jog knob
# center ~(888, 582) (64 px disc, kmin = vidMax-(80,80)); reverse toggle "<<"
# ~(755, 582) (30x22 at kmin.x-116, c.y-11); step buttons "<" ~(793, 582),
# ">" ~(824, 582).
#
# Entry-state note: PlayerCore opens with paused{false} -- the media PLAYS
# right after open. So S1 enters "<<" from PLAYING with no transport click
# (a 28,68 click there would PAUSE); the auto-pause path is exercised
# precisely because the entry happens while playing.
#
# Test media: tmp/vpt2_test.mp4 (480x270@30, 30 s) burns "00:00:SS.mmm" +
# frame number into the top-left of the picture and "MM:SS / MM:SS" into the
# UI clock (top right).
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
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-15-vplayer-reverse-autoplay\shots"
$mp4     = "I:/progwork/JKENGINE/tmp/vpt2_test.mp4"
$script:fails = 0
New-Item -ItemType Directory -Force -Path $shotDir | Out-Null

# Measured layer coords (verify against vpt11-s0-base.png before trusting).
$revX   = 755.0   # "<<" reverse toggle center
$revY   = 582.0
$knobX  = 888.0   # jog knob center
$knobY  = 582.0
$stepBX = 793.0   # "<" = -1F
$stepFX = 824.0   # ">" = +1F

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
# Track measured in THIS layout (calibrated on the run-1 shots: f=0.4 landed
# 15.6 s at x=426, f=0.03 landed 5.6 s at x=171 -> x0=27, width=767, dur 30).
function Seek-Frac([double]$f) { Click-App (27.0 + 767.0 * $f) 77 }

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

# ---------- S0: base shot + "<<" button calibration ----------
Save-ServerShot "vpt11-s0-base.png"
Check "S0: alive (base)" (VPlayer-Alive) ""

# ---------- S1: reverse cadence from PLAYING (auto-pause) ----------
# Media is playing right after open (paused{false}). "<<" toggle-on: the
# session auto-pauses the clock and walks jogTarget_ backward at content fps
# (~15 frames per 500 ms at 30 fps). Shots a/b/c: frame numbers DECREASE.
Click-App $revX $revY
Start-Sleep -Milliseconds 120
Save-ServerShotFast "vpt11-s1-a.png"
Start-Sleep -Milliseconds 500
Save-ServerShotFast "vpt11-s1-b.png"
Start-Sleep -Milliseconds 500
Save-ServerShotFast "vpt11-s1-c.png"
Set-LayerCursor 200 300                  # park OFF the button: expose the tint
Start-Sleep -Milliseconds 250
Save-ServerShotFast "vpt11-s1-tint.png"  # session active -> tint WITHOUT hover masking
Click-App $revX $revY                    # toggle-off mid-walk: precision seek + resume
Start-Sleep -Milliseconds 800
Save-ServerShotFast "vpt11-s1-off.png"
$p = Ping-Ms
Check "S1: responsive during reverse walk" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S1: no crash" (VPlayer-Alive) ""

# ---------- S2: ring exhaustion -> fallback keeps decreasing ----------
# Seek (~12 s) clears the ring; ~2 s of playback rebuilds it to ~2 s; pause;
# "<<" and walk backward for ~6 s of shots: after the first ~2 s the target
# crosses the ring start and the debounced keyframe SeekScrub fallback takes
# over -- frame numbers must KEEP DECREASING (one GOP-boundary pop accepted,
# no stall).
Seek-Frac 0.4                            # ~12 s (playing: seek clears the ring)
Start-Sleep -Milliseconds 2000           # ring ~2 s
Click-App 28 68                          # Pause (was playing)
Start-Sleep -Milliseconds 500
Click-App $revX $revY                    # "<<" toggle-on from PAUSED
Start-Sleep -Milliseconds 120
Save-ServerShotFast "vpt11-s2-a.png"
foreach ($i in 1..9) {
    Start-Sleep -Milliseconds 600
    Save-ServerShotFast ("vpt11-s2-" + [char](97 + $i) + ".png")
}
Click-App $revX $revY                    # toggle-off mid-walk: clean exit
Start-Sleep -Milliseconds 800
$p = Ping-Ms
Check "S2: responsive after ring exhaustion" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S2: no crash" (VPlayer-Alive) ""

# ---------- S3: auto-finish at 0 + playback restore ----------
# Independent scenario (S2 ended paused at ~10 s). Seek near the file start
# (~0.9 s), play, then "<<" from PLAYING: the walk reaches 0 in ~1.5 s,
# auto-finishScrub fires (precision seek to 0, jog off, playback restored) --
# s3-a: clock ~0-1 with PLAYING restored; s3-b: clock advanced, frames
# increasing again. s3-tint (cursor parked away): the active tint RELEASED.
Seek-Frac 0.027                          # ~0.8 s
Start-Sleep -Milliseconds 900
Click-App 28 68                          # Play
Start-Sleep -Milliseconds 400
Click-App $revX $revY                    # "<<" from PLAYING near 0
Start-Sleep -Milliseconds 2500           # walk ~1.5 s -> 0 -> auto-finish + resume
Save-ServerShotFast "vpt11-s3-a.png"
Start-Sleep -Milliseconds 1500
Save-ServerShotFast "vpt11-s3-b.png"
Set-LayerCursor 200 300                  # park OFF the button
Start-Sleep -Milliseconds 250
Save-ServerShotFast "vpt11-s3-tint.png"  # auto-finish -> tint released
$p = Ping-Ms
Check "S3: responsive after auto-finish" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S3: no crash" (VPlayer-Alive) ""

# ---------- S4: toggle-off precision seek (snap match, stays paused) ----------
# Independent scenario: pause wherever S3 left off, re-seed a known position
# (~9 s), then "<<" from PAUSED, walk 2 s, toggle-off -> precision seek to the
# snapped frame. Verdict (shots): s4-b burned frame == s4-b UI clock (snap)
# and s4-b frame is s4-a frame or +1; transport stays paused.
Click-App 28 68                          # Pause (playing after S3 resume)
Start-Sleep -Milliseconds 500
Seek-Frac 0.3                            # ~9 s headroom for the 2 s walk
Start-Sleep -Milliseconds 800
Click-App $revX $revY                    # "<<" toggle-on from PAUSED
Start-Sleep -Milliseconds 2000
Save-ServerShotFast "vpt11-s4-a.png"     # mid-walk: displayed frame F, clock = target
Click-App $revX $revY                    # toggle-off: precision seek to snap(F)
Start-Sleep -Milliseconds 800
Save-ServerShotFast "vpt11-s4-b.png"     # clock lands on F (or F+1), paused
$p = Ping-Ms
Check "S4: responsive after toggle-off" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S4: no crash" (VPlayer-Alive) ""

# ---------- S5: drag takeover + step button regression ----------
# Play, re-seed position (~3 s), "<<" enter (auto-pause, walk starts), then a
# knob drag takes the session over: reverse cadence stops, the drag target
# drives the display (s5-a mid-drag), release precision-seeks (s5-b). Then
# "<"/">" step +-1F (s5-c/s5-d) and playback resumes.
Click-App 28 68                          # Play (paused after S4)
Start-Sleep -Milliseconds 300
Seek-Frac 0.1                            # ~3 s headroom for the drag
Start-Sleep -Milliseconds 800
Click-App $revX $revY                    # "<<" enter from PLAYING
Start-Sleep -Milliseconds 300
# Drag takeover: press on the knob, arc the pointer (tangential, backward),
# shot mid-drag, release.
Refresh-Geom | Out-Null
[Wt9]::SetForegroundWindow($script:srvHwnd) | Out-Null
Start-Sleep -Milliseconds 200
Set-LayerCursor $knobX $knobY
Start-Sleep -Milliseconds 250
[Wt9]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)   # down
Start-Sleep -Milliseconds 120
Set-LayerCursor ($knobX + 40) ($knobY - 40)       # radial out (no rotation)
Start-Sleep -Milliseconds 120
Set-LayerCursor ($knobX + 10) ($knobY - 70)       # tangential (backward)
Start-Sleep -Milliseconds 150
Save-ServerShotFast "vpt11-s5-a.png"              # mid-drag
Set-LayerCursor ($knobX - 20) ($knobY - 60)       # keep arcing backward
Start-Sleep -Milliseconds 150
[Wt9]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)   # up = release finish
Start-Sleep -Milliseconds 700
Save-ServerShotFast "vpt11-s5-b.png"
# Steps are paused-friendly (stepFrame nudges WITHOUT pausing while playing,
# invisible under playback) - pause first so +/-1F is readable off the shots.
Click-App 28 68                          # Pause (release restored playback)
Start-Sleep -Milliseconds 500
Save-ServerShotFast "vpt11-s5-b2.png"    # paused baseline for the step reads
Click-App $stepBX $revY                  # "<" = -1F (paused)
Start-Sleep -Milliseconds 600
Save-ServerShotFast "vpt11-s5-c.png"
Click-App $stepFX $revY                  # ">" = +1F (back to baseline)
Start-Sleep -Milliseconds 600
Save-ServerShotFast "vpt11-s5-d.png"
Click-App 28 68                          # Play: resume for teardown
Start-Sleep -Milliseconds 1200
Save-ServerShotFast "vpt11-s5-resumed.png"
$p = Ping-Ms
Check "S5: responsive after takeover + steps" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S5: no crash" (VPlayer-Alive) ""

# ---------- teardown ----------
# Server left running: vpt9/vpt4/vpt5 regression probes restart it themselves;
# the caller restores the pre-probe server state.
Write-Host ("RESULT: " + $(if ($script:fails -eq 0) { "ALL PASS" } else { "$($script:fails) FAILURE(S)" }))
exit $(if ($script:fails -eq 0) { 0 } else { 1 })