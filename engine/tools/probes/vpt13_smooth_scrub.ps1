# vpt13 e2e: smooth-scrub FLOW GATE (plan 2026-09-24 task 5, spec 2026-09-24).
# Harness recipe = vpt9_jogframescrub.ps1 (DPI-aware, full-layer crop, measured
# geometry) + vpt5_e2e.ps1 client-direct stderr capture (docs/60 13.2 lesson:
# server-side Start-Process redirects are DEAD -- MirrorLogToFiles dup2's the
# server's fds and spawned-client stderr is nondeterministically aliased, so
# the probe spawns the vplayer client itself and $errLog owns its stderr).
#
# Consumes (Task 4 producers, ClientVPlayerApp.cpp):
#   [vpt13] D=%.3f T=%.3f lo=%.3f          100 ms throttle, emitted ONLY inside
#                                          a live frame-scrub session (drag /
#                                          wheel / reverse all feed the pump)
#   [vpt11] rev pos=%.3f D=%.3f fps=%.1f   1 s throttle while reverseActive_
# D = display clock (JKScrubClock::Chase), T = input target, lo = jog-ring
# oldest pts (-1 = empty ring).
#
# FLOW CRITERION (plan task 5): between consecutive 100 ms samples the display
# clock may move at most kFlowMax * 0.1 s + 1 frame + slack
# (8.0 * 0.1 + 1/30 + 0.2 ~= 1.03 s). D is a pure clock (JKScrubClock::Chase) --
# any larger single-sample jump means a seek-snap leaked into the display path,
# the regression this gate exists to catch. S2's "no keyframe jump > 3 frames"
# is folded into the same bound: a snap pop is orders of magnitude above it,
# while sub-bound sample deltas ride the input quantum (one wheel tick =
# sPerRev/16 = 0.469 s on a 60 s clip) and cannot be finer.
#
# Mechanism under test: inputs own T; D chases T (JogTo per moved D, decode
# runs to it -- no seeks in the hot path). The 16 s ring + backward GOP-chain
# refill supply the frames; the keyframe SeekScrub fires ONLY as the 250 ms
# stall fallback when the chain cannot serve the position (ring front frozen,
# D beyond it) -- "stall, then resume" (spec sec 5 chain-exhaustion UX).
#
# Media (absolute paths into the MAIN repo tmp are correct for media; $build,
# the binary under test, resolves to the WORKTREE build):
#   tmp/test_media/vpt_hires.mp4    1080p 60 s 30 fps -g 30  -> S1/S2: the GOP
#       chain refill KEEPS UP with an in-ring -5 s pull (lo steps down, no
#       fallback) and forward decode fills the ring behind a +6 s drag.
#   tmp/test_media/vpt13_longgop.mp4 1080p 60 s 30 fps -g 300 -> S3: one chain
#       attempt decodes a 10 s GOP (~1 s wall at 1080p), so the ring front
#       freezes far longer than the 250 ms stall window and the fallback
#       SeekScrub fires DETERMINISTICALLY (-g 30 measured 2026-09-24: refill
#       follows a -30 s burst, lo never freezes 250 ms, fallback never fires).
#       Measured/shot verdicts do not read glyphs, so burned testsrc2 art is fine.
#
# Geometry (measured, 1.25 scale layer crop; re-verify on vpt13-s0-base.png):
# transport Pause/Play center ~(28, 68); seek slider row y~77 (vpt9 calibration
# x = 150 + 690*f -- only roughly right here, every position claim is read off
# the T stream / a 1-tick position probe, never off the calibration); jog knob
# center ~(888, 582) (64 px disc); reverse toggle "<<" ~(755, 582).
#
# Dial math (60 s clip): sPerRev = clamp(dur/8, 1, 30) = 7.5 s; one wheel tick
# = 1/16 rev = 0.469 s. Drag: dth = cross(r, dMouse)/|r|^2 -- a tangential arc
# at radius r rotates dth = arcLen/r, and a RADIAL move has cross ~= 0 (used to
# exit the disc without rotating the dial). Screen-clockwise (y down) = forward.
#
# S0 calibration: base shot + knob-hover shot + Seed-Pos feedback (slider frac
# corrected from the probed position until within 4 s of the target).
# S1 forward drag scrub (PAUSED, hires): press the knob, radial out to r=60,
#    arc clockwise 288 deg (0.8 rev = +6.0 s) in 4 deg/22 ms steps. Verdicts
#    off the [vpt13] stream: >=8 samples, every |dD| <= flow bound, D never
#    moves backward, T advanced >= 3 s, last sample D within 0.5 s of T.
# S2 reverse pull inside the ring (PAUSED, hires, ~-5 s = 11 wheel ticks at
#    250 ms): D recedes continuously (every dD <= +0.05, |dD| <= flow bound),
#    chain refill evidence = at least one lo-decreasing step >= 0.05 s AND
#    min(lo) <= first lo - 1.0, ends with |D - T| <= 0.5.
# S3 reverse OUTSIDE the ring (PAUSED, longgop, -30 s = 64 wheel ticks at
#    15 ms + a 2-tick slow tail so the chase converges before the release):
#    D outruns the chain -> at least one sample with D beyond the ring front
#    (ringMiss, source margin d < lo - 0.05), then the 250 ms fallback
#    SeekScrub tears the ring down/rebuilds it below the front D ran past
#    (lo drop >= 2 s after the miss, or the ring empties) and D re-tracks
#    (last |D - T| <= 1.0). The per-sample flow bound is deliberately NOT
#    applied here: D legitimately chases at up to kFlowMax per UI frame while
#    the ring is behind; the stall/resume structure is the acceptance (spec
#    sec 5 "stall, then resume"). Run-A lesson: a single-sample lo drop of
#    7.6 s IS the rebuild -- the frozen window can resolve within one sample,
#    so the acceptance keys on miss + rebuild, not on a frozen-run length.
# S4 release = finishScrub contract (vpt9 S4): (a) from PAUSED, wheel +8 ticks,
#    400 ms idle release, then a 1-tick position probe -- its first T minus one
#    tick quantum must equal the released target within 0.2 s (precision
#    landing, machine-checked); (b) from PLAYING, wheel +5 ticks, mid shot
#    (auto-pause), release shot, +2 s shot (clock advanced). Shot verdicts are
#    read by the executing agent (lesson 19); the probe checks structure.
# S5 "<<" reverse auto-play: [vpt11] rev pos= lines -- pos monotone decreasing
#    (rate 0.3..1.5 pos-s/s) and D tracking pos within 0.25 s.
param()
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wt13 {
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
[Wt13]::SetProcessDPIAware() | Out-Null

$build    = "I:\progwork\JKENGINE\.claude\worktrees\vplayer-scrub\engine\build"
$exe      = "$build\jkdesktop.exe"
$shotDir  = "I:\progwork\JKENGINE\.claude\worktrees\vplayer-scrub\.superpowers\sdd\2026-09-24-vplayer-smooth-scrub\shots"
$mp4      = "I:/progwork/JKENGINE/tmp/test_media/vpt_hires.mp4"
$mp4Long  = "I:/progwork/JKENGINE/tmp/test_media/vpt13_longgop.mp4"
$script:fails = 0
New-Item -ItemType Directory -Force -Path $shotDir | Out-Null

# ---- flow-gate constants (mirrored from ClientVPlayerApp.cpp / plan task 5)
$kFlowMax = 8.0     # Chase cap, frames per UI frame (source constant)
$fps      = 30.0    # both clips are 30 fps
$kTickSec = 7.5 / 16.0   # sPerRev/16 = 0.469 s per wheel notch (60 s clip)
$maxJump  = $kFlowMax * 0.1 + (1.0 / $fps) + 0.2   # ~= 1.03 s per 100 ms sample

$knobX = 888.0; $knobY = 582.0
$revX  = 755.0; $revY  = 582.0

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
$script:playing = $false   # transport verdict tracked for Pause/Play clicks

function Find-Server {
    $h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
        Select-Object -First 1).MainWindowHandle
    if (-not $h) { Write-Host "FAIL: NO SERVER WINDOW"; exit 1 }
    $script:srvHwnd = [IntPtr]$h
    $crect = New-Object Wt13+RECT
    [Wt13]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt13+POINT; $co.X = 0; $co.Y = 0
    [Wt13]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
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
function Refresh-Geom {
    Find-Server
    for ($i = 0; $i -lt 10; $i++) { if (Find-VPlayerLayer) { return $true }; Start-Sleep -Milliseconds 300 }
    return $false
}

function Set-LayerCursor([double]$lx, [double]$ly) {
    $px = [int]([math]::Round($script:ox + ($script:layerX + $lx) * $script:scale))
    $py = [int]([math]::Round($script:oy + ($script:layerY + $ly) * $script:scale))
    [Wt13]::SetCursorPos($px, $py) | Out-Null
}

function Send-Click([double]$lx, [double]$ly, [uint32]$down, [uint32]$up) {
    Refresh-Geom | Out-Null
    [Wt13]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-LayerCursor $lx $ly
    Start-Sleep -Milliseconds 250
    [Wt13]::mouse_event($down, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [Wt13]::mouse_event($up, 0, 0, 0, [UIntPtr]::Zero)
}
function Click-App([double]$lx, [double]$ly) { Send-Click $lx $ly 2 4 }

function Click-Pause([bool]$pause) {   # transport toggle with verdict tracking
    Click-App 28 68
    $script:playing = (-not $pause)
}
function Type-IntoPath([string]$path) {
    Click-App 400 42
    Start-Sleep -Milliseconds 300
    [Wt13]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    [System.Windows.Forms.SendKeys]::SendWait("{END}")
    Start-Sleep -Milliseconds 100
    [System.Windows.Forms.SendKeys]::SendWait("{BS 300}")
    Start-Sleep -Milliseconds 150
    [System.Windows.Forms.SendKeys]::SendWait($path)
    Start-Sleep -Milliseconds 400
}
function Click-Open { Click-App 808 42 }

# Red "empty path" label check (vpt11 recipe) -- the SendKeys typing step is
# the one flaky setup action; retry the open rather than pass vacuously.
function Test-OpenFailed {
    Refresh-Geom | Out-Null
    $crect = New-Object Wt13+RECT
    [Wt13]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt13+POINT; $co.X = 0; $co.Y = 0
    [Wt13]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($crect.R - $crect.L), ($crect.B - $crect.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size)
    $g.Dispose()
    $x0 = [int]([math]::Round(($script:layerX + 0) * $script:scale))
    $y0 = [int]([math]::Round(($script:layerY + 75) * $script:scale))
    $w  = [int]([math]::Round(240 * $script:scale))
    $h  = [int]([math]::Round(25 * $script:scale))
    $red = 0
    for ($y = $y0; $y -lt $y0 + $h -and $y -lt $bmp.Height; $y++) {
        for ($x = $x0; $x -lt $x0 + $w -and $x -lt $bmp.Width; $x++) {
            $c = $bmp.GetPixel($x, $y)
            if ($c.R -gt 140 -and ($c.R - $c.G) -gt 60 -and ($c.R - $c.B) -gt 60) { $red++ }
        }
    }
    $bmp.Dispose()
    return ($red -gt 20)
}

function Open-Media([string]$path) {
    Type-IntoPath $path
    Click-Open
    Start-Sleep -Seconds 6
    $ok = $false
    for ($attempt = 1; $attempt -le 3; $attempt++) {
        if (-not (Test-OpenFailed)) { return $true }
        Write-Host ("setup: open attempt {0} showed empty-path label, retrying" -f $attempt)
        Type-IntoPath $path
        Click-Open
        Start-Sleep -Seconds 6
    }
    return $false
}

function Save-ServerShotFast([string]$name) {
    $crect = New-Object Wt13+RECT
    [Wt13]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt13+POINT; $co.X = 0; $co.Y = 0
    [Wt13]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
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

function Seek-Frac([double]$f) { Click-App (150.0 + 690.0 * $f) 77 }

function Send-Wheel([int]$ticks, [int]$delayMs) {
    $d = $(if ($ticks -ge 0) { 120 } else { -120 })
    for ($i = 0; $i -lt [math]::Abs($ticks); $i++) {
        [Wt13]::mouse_event(0x0800, 0, 0, $d, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds $delayMs
    }
}

function Hover-Knob {
    Refresh-Geom | Out-Null
    [Wt13]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-LayerCursor $knobX $knobY
    Start-Sleep -Milliseconds 250
}

# ---------- stderr log access ([vpt13]/[vpt11] producers live here) ----------
$script:errLog = "$build\vpt13_client_stderr.log"

# Get-Content (NOT [System.IO.File]::ReadAllLines): the Start-Process redirect
# writer holds the file in a share mode that blocks ReadAllLines outright
# (measured run 1: every read threw "in use by another process" -> 0 samples
# parsed while the file grew to 19 KB). Get-Content opens with read-share and
# is the idiom the vpt5/vpt11 gates already read their logs with.
function Read-LogLines {
    for ($i = 0; $i -lt 30; $i++) {
        try { return @(Get-Content -Path $script:errLog) } catch { Start-Sleep -Milliseconds 150 }
    }
    return @()
}
function Log-Len { return (Read-LogLines).Count }
function Log-Tail([int]$off) {
    $l = Read-LogLines
    if ($l.Count -le $off) { return @() }
    return $l[$off..($l.Count - 1)]
}

function Parse-Vpt13([string[]]$lines) {
    $out = @()
    foreach ($ln in $lines) {
        $m = [regex]::Match($ln, '\[vpt13\] D=(-?[0-9.]+) T=(-?[0-9.]+) lo=(-?[0-9.]+)')
        if ($m.Success) {
            $out += New-Object psobject -Property @{
                d  = [double]$m.Groups[1].Value
                t  = [double]$m.Groups[2].Value
                lo = [double]$m.Groups[3].Value
            }
        }
    }
    return $out
}
function Parse-Vpt11([string[]]$lines) {
    $out = @()
    foreach ($ln in $lines) {
        $m = [regex]::Match($ln, '\[vpt11\] rev pos=([0-9.]+) D=(-?[0-9.]+)')
        if ($m.Success) {
            $out += New-Object psobject -Property @{
                pos = [double]$m.Groups[1].Value
                d   = [double]$m.Groups[2].Value
            }
        }
    }
    return $out
}

# Open a 1-tick wheel session purely to read the live position: the first
# sample seeds D at st.pos and T one tick ahead, so landed pos = T_first -
# kTickSec (exact input quantum, chase-independent). Transport state survives
# (release restores whatever was there).
function Probe-Pos {
    $off = Log-Len
    Hover-Knob
    Send-Wheel 1 100
    Start-Sleep -Milliseconds 800
    $s = Parse-Vpt13 (Log-Tail $off)
    if ($s.Count -eq 0) { return $null }
    return ($s[0].t - $kTickSec)
}

# Seek + verify by feedback: the slider calibration is only roughly known, so
# correct the frac from the probed position until the landing is within 4 s of
# the target (or the tries run out -- scenario verdicts read real T values).
function Seed-Pos([double]$target) {
    $f = $target / 60.0
    $pos = $null
    for ($try = 0; $try -lt 4; $try++) {
        $f = [math]::Min(0.97, [math]::Max(0.02, $f))
        Seek-Frac $f
        Start-Sleep -Milliseconds 1500
        $pos = Probe-Pos
        if ($null -eq $pos) { return $null }
        if ([math]::Abs($pos - $target) -le 4.0) { return $pos }
        $f = $f * ($target / [math]::Max($pos, 1.0))
    }
    return $pos
}

# ---------- setup ----------
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
if (Test-Path $script:errLog) { Remove-Item -Force $script:errLog }
# Server: stdout split only (docs/60 13.2 -- NEVER expect client diagnostics
# through a server-side redirect).
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build -RedirectStandardOutput "I:\progwork\JKENGINE\tmp\vpt13_server_stdout.log"
Start-Sleep -Seconds 4
Check "setup: server up" ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') ""
# vplayer client spawned DIRECTLY by the probe: $errLog owns its stderr.
Start-Process -FilePath $exe -ArgumentList "--client","vplayer" `
    -WorkingDirectory $build -RedirectStandardError $script:errLog `
    -RedirectStandardOutput "I:\progwork\JKENGINE\tmp\vpt13_client_stdout.log"
Start-Sleep -Seconds 6
Check "setup: vplayer launched" (Refresh-Geom) "no Video Player layer"
Check "setup: media open (hires)" (Open-Media $mp4) "empty-path label persisted"
$script:playing = $true   # PlayerCore opens unpaused

# ---------- S0: base shot + knob hover + seed ----------
Save-ServerShot "vpt13-s0-base.png"
Hover-Knob
Save-ServerShot "vpt13-s0-knob-hover.png"
Check "S0: alive (base)" (VPlayer-Alive) ""
$pos = Seed-Pos 15.0
Check "S0: vpt13 stream live (position probe)" ($null -ne $pos) "no [vpt13] samples on a 1-tick session"
Write-Host ("      seeded pos ~{0:N2} s" -f $(if ($null -ne $pos) { $pos } else { -1 }))

# ---------- S1: forward drag scrub (PAUSED, hires) ----------
Click-Pause $true
Start-Sleep -Milliseconds 500
$off = Log-Len
# Press the knob, radial out to r=60 (cross ~= 0 -> no rotation), then arc
# clockwise 288 deg = 0.8 rev = +6.0 s, 4 deg per 22 ms.
Refresh-Geom | Out-Null
[Wt13]::SetForegroundWindow($script:srvHwnd) | Out-Null
Start-Sleep -Milliseconds 200
Set-LayerCursor $knobX $knobY
Start-Sleep -Milliseconds 250
[Wt13]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)   # down
Start-Sleep -Milliseconds 120
$rad = 60.0
Set-LayerCursor ($knobX + $rad) $knobY              # radial out (no rotation)
Start-Sleep -Milliseconds 150
for ($phi = 4.0; $phi -le 288.0; $phi += 4.0) {
    $a = $phi * [math]::PI / 180.0
    Set-LayerCursor ($knobX + $rad * [math]::Cos($a)) ($knobY + $rad * [math]::Sin($a))
    Start-Sleep -Milliseconds 22
}
Start-Sleep -Milliseconds 120
[Wt13]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)   # up = finishScrub
Start-Sleep -Milliseconds 900
$s1 = Parse-Vpt13 (Log-Tail $off)
Check "S1: samples captured" ($s1.Count -ge 8) ("got {0} [vpt13] samples" -f $s1.Count)
$s1ok = $true; $s1why = ""
for ($i = 1; $i -lt $s1.Count; $i++) {
    $dd = [math]::Abs($s1[$i].d - $s1[$i - 1].d)
    if ($dd -gt $maxJump) { $s1ok = $false; $s1why = "sample {0}: |dD|={1:N3} > {2:N3}" -f $i, $dd, $maxJump; break }
    if ($s1[$i].d -lt $s1[$i - 1].d - 0.02) { $s1ok = $false; $s1why = "sample {0}: D moved backward {1:N3}" -f $i, ($s1[$i - 1].d - $s1[$i].d); break }
}
$tAdv = 0.0
if ($s1.Count -ge 2) { $tAdv = ($s1 | Measure-Object -Property t -Maximum).Maximum - ($s1 | Measure-Object -Property t -Minimum).Minimum }
$landS1 = 99.0
if ($s1.Count -ge 1) { $landS1 = [math]::Abs($s1[-1].d - $s1[-1].t) }
Check "S1: flow continuity (|dD| <= 1.033/sample, forward only)" $s1ok $s1why
Check "S1: target advanced (>= 3 s)" ($tAdv -ge 3.0) ("dT={0:N2} over {1} samples" -f $tAdv, $s1.Count)
Check "S1: D tracks T at last sample (<= 0.5 s)" ($landS1 -le 0.5) ("|D-T|={0:N3}" -f $landS1)
$p = Ping-Ms
Check "S1: responsive after drag scrub" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S1: no crash" (VPlayer-Alive) ""

# ---------- S2: reverse pull inside the ring (~-5 s) — chain refill evidence ----------
$pos2 = Seed-Pos 40.0
Write-Host ("      S2 seeded pos ~{0:N2} s" -f $(if ($null -ne $pos2) { $pos2 } else { -1 }))
$off = Log-Len
Hover-Knob
Send-Wheel -11 250          # 11 * -0.469 = -5.16 s
Start-Sleep -Milliseconds 900
$s2 = Parse-Vpt13 (Log-Tail $off)
Check "S2: samples captured" ($s2.Count -ge 8) ("got {0} [vpt13] samples" -f $s2.Count)
$s2ok = $true; $s2why = ""
for ($i = 1; $i -lt $s2.Count; $i++) {
    $dd = [math]::Abs($s2[$i].d - $s2[$i - 1].d)
    if ($dd -gt $maxJump) { $s2ok = $false; $s2why = "sample {0}: |dD|={1:N3} > {2:N3} (snap pop)" -f $i, $dd, $maxJump; break }
    if ($s2[$i].d -gt $s2[$i - 1].d + 0.05) { $s2ok = $false; $s2why = "sample {0}: D jumped forward {1:N3} during reverse" -f $i, ($s2[$i].d - $s2[$i - 1].d); break }
}
$tBack = 0.0
if ($s2.Count -ge 2) { $tBack = ($s2 | Measure-Object -Property t -Maximum).Maximum - ($s2 | Measure-Object -Property t -Minimum).Minimum }
$loDrops = 0; $loMin = 99.0; $loFirst = 99.0
if ($s2.Count -ge 1) {
    $loFirst = $s2[0].lo
    $loMin = ($s2 | Measure-Object -Property lo -Minimum).Minimum
    for ($i = 1; $i -lt $s2.Count; $i++) { if ($s2[$i - 1].lo - $s2[$i].lo -ge 0.05) { $loDrops++ } }
}
$landS2 = 99.0
if ($s2.Count -ge 1) { $landS2 = [math]::Abs($s2[-1].d - $s2[-1].t) }
Check "S2: D recedes continuously (no snap, no forward jump)" $s2ok $s2why
Check "S2: target receded (3..8 s)" ($tBack -ge 3.0 -and $tBack -le 8.0) ("dT={0:N2}" -f $tBack)
Check "S2: chain refill evidence (lo decreasing)" ($loDrops -ge 1 -and ($loFirst - $loMin) -ge 1.0) ("drops={0} net lo {1:N2} -> {2:N2}" -f $loDrops, $loFirst, $loMin)
Check "S2: D tracks T at last sample (<= 0.5 s)" ($landS2 -le 0.5) ("|D-T|={0:N3}" -f $landS2)
$p = Ping-Ms
Check "S2: responsive after in-ring reverse" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S2: no crash" (VPlayer-Alive) ""

# ---------- S3: reverse OUTSIDE the ring (-30 s, longgop) — stall, then resume ----------
Check "setup: media open (longgop)" (Open-Media $mp4Long) "empty-path label persisted"
$script:playing = $true     # PlayerCore opens unpaused
$pos3 = Seed-Pos 40.0
Write-Host ("      S3 seeded pos ~{0:N2} s" -f $(if ($null -ne $pos3) { $pos3 } else { -1 }))
$off = Log-Len
Hover-Knob
Send-Wheel -64 15           # 64 * -0.469 = -30 s in ~1 s: D outruns the chain
Send-Wheel -2 700           # slow tail: re-arms the 400 ms idle release so the
                            # chase gets ~2 s to converge BEFORE the release
                            # (run 2 lesson: the release otherwise cuts D off
                            # 3.3 s behind T -- landing still correct via the
                            # precision seek, but the in-session retrack evidence
                            # never reaches the log)
Start-Sleep -Milliseconds 2500
$s3 = Parse-Vpt13 (Log-Tail $off)
Check "S3: samples captured" ($s3.Count -ge 8) ("got {0} [vpt13] samples" -f $s3.Count)
$stallRun = 0; $bestRun = 0
$missIdx = -1; $loAtMiss = -1.0
for ($i = 1; $i -lt $s3.Count; $i++) {
    # ringMiss (source margin): D beyond the ring front -- the chain cannot
    # serve the display position right now.
    if ($s3[$i].lo -ge 0.0 -and $s3[$i].d -lt $s3[$i].lo - 0.05) {
        if ($missIdx -lt 0) { $missIdx = $i; $loAtMiss = $s3[$i].lo }
        if ($i -gt 0 -and [math]::Abs($s3[$i].lo - $s3[$i - 1].lo) -lt 0.005) {
            $stallRun++
            if ($stallRun -gt $bestRun) { $bestRun = $stallRun }
        } else { $stallRun = 0 }
    } else { $stallRun = 0 }
}
# Resume evidence: after the miss the ring is torn down (lo = -1 while the
# fallback seek lands) or rebuilt >= 2 s below the front D ran past.
$loAfter = $loAtMiss; $ringCleared = $false
if ($missIdx -ge 0 -and $missIdx -lt $s3.Count - 1) {
    foreach ($s in $s3[($missIdx + 1)..($s3.Count - 1)]) {
        if ($s.lo -lt 0.0) { $ringCleared = $true; continue }
        if ($s.lo -lt $loAfter) { $loAfter = $s.lo }
    }
}
if ($ringCleared) { $loAfter = $loAtMiss - 10.0 }
$landS3 = 99.0
if ($s3.Count -ge 1) { $landS3 = [math]::Abs($s3[-1].d - $s3[-1].t) }
$tBack3 = 0.0
if ($s3.Count -ge 2) { $tBack3 = ($s3 | Measure-Object -Property t -Maximum).Maximum - ($s3 | Measure-Object -Property t -Minimum).Minimum }
Check "S3: target receded >= 20 s" ($tBack3 -ge 20.0) ("dT={0:N2}" -f $tBack3)
Check "S3: stall observed (D beyond ring front)" ($missIdx -ge 0) ("ringMiss samples={0}, longest frozen run={1}" -f (($s3 | Where-Object { $_.lo -ge 0.0 -and $_.d -lt $_.lo - 0.05 }).Count), $bestRun)
Check "S3: fallback resumed (ring torn down / rebuilt >= 2 s below the front)" (($loAtMiss - $loAfter) -ge 2.0) ("lo {0:N2} -> {1:N2} cleared={2}" -f $loAtMiss, $loAfter, $ringCleared)
Check "S3: D re-tracked after fallback (|D-T| <= 1.0)" ($landS3 -le 1.0) ("|D-T|={0:N3}" -f $landS3)
$p = Ping-Ms
Check "S3: responsive after out-of-ring fallback" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S3: no crash" (VPlayer-Alive) ""
Save-ServerShotFast "vpt13-s3-landed.png"

# ---------- S4a: release = precision landing from PAUSED (machine-checked) ----------
Click-Pause $true           # S3's release restored playing; park it
Start-Sleep -Milliseconds 500
$off = Log-Len
Hover-Knob
Send-Wheel 8 150            # +3.75 s from wherever S3 landed
Start-Sleep -Milliseconds 900   # 400 ms idle release fires here
$s4 = Parse-Vpt13 (Log-Tail $off)
Check "S4a: session samples captured" ($s4.Count -ge 4) ("got {0}" -f $s4.Count)
$tRel = -1.0
if ($s4.Count -ge 1) { $tRel = $s4[-1].t }
$landed = Probe-Pos          # paused: pos static since release
$s4ok = $false; $s4why = "no probe sample"
if ($tRel -ge 0 -and $null -ne $landed) {
    $err = [math]::Abs($landed - $tRel)
    $s4ok = ($err -le 0.2)
    $s4why = "released T={0:N3}, probed pos={1:N3}, err={2:N3}" -f $tRel, $landed, $err
}
Check "S4a: precision landing (probed pos == released T, <= 0.2 s)" $s4ok $s4why

# ---------- S4b: release from PLAYING restores playback (vpt9 S4 shots) ----------
Click-Pause $false          # Play
Start-Sleep -Milliseconds 500
$off = Log-Len
Hover-Knob
Send-Wheel 5 100            # +2.34 s from playing: auto-pause inside the session
Save-ServerShotFast "vpt13-s4-mid.png"
Start-Sleep -Milliseconds 900
Save-ServerShotFast "vpt13-s4-after.png"
$s4b = Parse-Vpt13 (Log-Tail $off)
Check "S4b: session samples captured" ($s4b.Count -ge 3) ("got {0}" -f $s4b.Count)
Start-Sleep -Seconds 2
Save-ServerShotFast "vpt13-s4-playing.png"
$p = Ping-Ms
Check "S4b: responsive after release" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S4b: no crash" (VPlayer-Alive) ""

# ---------- S5: "<<" reverse auto-play ([vpt11] rev pos= stream) ----------
$pos5 = Seed-Pos 30.0       # playing: headroom for a 5 s reverse walk
Write-Host ("      S5 seeded pos ~{0:N2} s" -f $(if ($null -ne $pos5) { $pos5 } else { -1 }))
$off = Log-Len
Click-App $revX $revY       # "<<" toggle-on from PLAYING
Start-Sleep -Milliseconds 2500
Save-ServerShotFast "vpt13-s5-rev1.png"
Start-Sleep -Milliseconds 2500
Save-ServerShotFast "vpt13-s5-rev2.png"
Click-App $revX $revY       # toggle-off
Start-Sleep -Milliseconds 1000
$rev = Parse-Vpt11 (Log-Tail $off)
Check "S5: pacing lines captured" ($rev.Count -ge 4) ("got {0} [vpt11] lines" -f $rev.Count)
$mono = $false; $rate = 0.0; $s5why = "insufficient lines"
if ($rev.Count -ge 4) {
    $mono = $true
    for ($i = 1; $i -lt $rev.Count; $i++) {
        if ($rev[$i].pos -gt $rev[$i - 1].pos + 0.05) { $mono = $false; $s5why = "pos rose {0:N3} -> {1:N3}" -f $rev[$i - 1].pos, $rev[$i].pos; break }
        if ([math]::Abs($rev[$i].d - $rev[$i].pos) -gt 0.25) { $mono = $false; $s5why = "sample {0}: |D-pos|={1:N3} > 0.25" -f $i, ([math]::Abs($rev[$i].d - $rev[$i].pos)); break }
    }
    $deltas = @()
    for ($i = 1; $i -lt $rev.Count; $i++) { $deltas += ($rev[$i - 1].pos - $rev[$i].pos) }
    $rate = ($deltas | Measure-Object -Average).Average
    if ($mono) { $s5why = ("mono ok over {0} lines" -f $rev.Count) }
}
Check "S5: pos monotone decreasing + D tracks pos (<= 0.25 s)" $mono $s5why
Check "S5: reverse cadence rate (0.3..1.5 pos-s/s)" ($rate -ge 0.3 -and $rate -le 1.5) ("avg rate={0:N3}" -f $rate)
$p = Ping-Ms
Check "S5: responsive after reverse playback" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S5: no crash" (VPlayer-Alive) ""

# ---------- teardown ----------
# Server left running: the regression probes restart it themselves; the caller
# restores the pre-probe server state.
Write-Host ("RESULT: " + $(if ($script:fails -eq 0) { "ALL PASS" } else { "$($script:fails) FAILURE(S)" }))
exit $(if ($script:fails -eq 0) { 0 } else { 1 })