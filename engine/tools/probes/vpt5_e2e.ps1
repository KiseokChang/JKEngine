# vplayer-stability T5 INTEGRATED GATE e2e (spec section 2, all scenarios).
# -STA required (Set-Clipboard, docs/15 cp949 convention). Harness recipe =
# vpt4_e2e.ps1 (DPI-aware, full-layer crop, measured geometry).
#
# S1 corrupt files x3 (vpt1 media): trunc-tail mp4 (no moov) / 0-byte /
#     header-corrupted -> classified openError (red), no crash. Then a silent
#     named pipe -> "opening" phase + cancel affordance, no crash.
# S2 hi-res underrun: 1920x1080 mp4, ~45 s continuous playback, "underrun: N"
#     sampled at t8/t25/t45; then 3 slider seeks -> audio recovery proven by
#     the clock ADVANCING between two shots 4 s apart (audio-master clock).
# S3 seek correctness: local mp4 (has timecode burn-in) repeated seeks incl a
#     rapid scrub burst; raw H.264 elementary stream (no index, seek
#     unsupported) as the indexless-source proxy for the failed-seek path.
# S4 wheel scrub (vpt4 recipe): knob hover wheel from PLAYING (mid-scrub
#     auto-pause inside the 400 ms window, release restores playing) and from
#     PAUSED (stays paused through release).
# S5 regression: pause/play, volume slider, A/V sync slider, jog drag cw+ccw.
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wt5 {
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
[Wt5]::SetProcessDPIAware() | Out-Null

$build   = "I:\progwork\JKENGINE\engine\build"
$exe     = "$build\jkdesktop.exe"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-14-vplayer-stability"
$hiRes   = "I:\progwork\JKENGINE\tmp\test_media\vpt_hires.mp4"
$mp4     = "I:/progwork/JKENGINE/tmp/vpt2_test.mp4"
$raw     = "I:/progwork/JKENGINE/tmp/vpt2_raw.h264"
$pipeArg = '//./pipe/vpt5_hang'
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
    $crect = New-Object Wt5+RECT
    [Wt5]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt5+POINT; $co.X = 0; $co.Y = 0
    [Wt5]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
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
    [Wt5]::SetCursorPos($px, $py) | Out-Null
}

function Send-Click([double]$lx, [double]$ly, [uint32]$down, [uint32]$up) {
    Refresh-Geom | Out-Null
    [Wt5]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-LayerCursor $lx $ly
    Start-Sleep -Milliseconds 250
    [Wt5]::mouse_event($down, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [Wt5]::mouse_event($up, 0, 0, 0, [UIntPtr]::Zero)
}
function Click-App([double]$lx, [double]$ly) { Send-Click $lx $ly 2 4 }

function Type-IntoPath([string]$path) {
    Click-App 400 42
    Start-Sleep -Milliseconds 300
    [Wt5]::SetForegroundWindow($script:srvHwnd) | Out-Null
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

function Save-ServerShotFast([string]$name) {
    $crect = New-Object Wt5+RECT
    [Wt5]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt5+POINT; $co.X = 0; $co.Y = 0
    [Wt5]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
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

function Send-Wheel([int]$ticks) {
    $d = $(if ($ticks -ge 0) { 120 } else { -120 })
    for ($i = 0; $i -lt [math]::Abs($ticks); $i++) {
        [Wt5]::mouse_event(0x0800, 0, 0, $d, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 25
    }
}

function Hover-Knob {
    Refresh-Geom | Out-Null
    [Wt5]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-LayerCursor 888 582
    Start-Sleep -Milliseconds 250
}

function Drag-Knob([double]$turns) {
    $cx = 888.0; $cy = 582.0; $r = 20.0
    Refresh-Geom | Out-Null
    [Wt5]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-LayerCursor ($cx + $r) $cy
    Start-Sleep -Milliseconds 250
    [Wt5]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 120
    $steps = 24
    $total = [math]::Abs($turns) * 2.0 * [math]::PI
    $dir = $(if ($turns -ge 0) { 1.0 } else { -1.0 })
    for ($i = 1; $i -le $steps; $i++) {
        $a = $total * $dir * $i / $steps
        Set-LayerCursor ($cx + $r * [math]::Cos($a)) ($cy + $r * [math]::Sin($a))
        Start-Sleep -Milliseconds 40
    }
    Start-Sleep -Milliseconds 120
    [Wt5]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
}

function Start-PipeHolder([string]$name = 'vpt5_hang') {
    return (Start-Process powershell -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass',
        '-File','I:\progwork\JKENGINE\tmp\vpt1_pipehold.ps1','-Mode','Hang','-Name',$name -PassThru -WindowStyle Hidden)
}
function Stop-PipeHolder($p) {
    if ($p) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
    Start-Sleep -Milliseconds 800
}

# ---------- setup ----------
$errLog = "I:\progwork\JKENGINE\tmp\vpt5_client_stderr.log"
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
# NOTE (2026-09-24): the server must NOT share $errLog with the client —
# MirrorLogToFiles (docs/60 §8-9) dup2's the server's stdout/stderr into a
# pipe at fd level, orphaning any inherited redirect handle, and the
# console-child std-handle slot reuse makes server-spawned client stderr
# land in state/logs/server_*.log only nondeterministically (measured:
# run1 0 lines / run2 1 line). Probe spawns the vplayer client directly
# (probe_theme_swap.ps1 recipe) so $errLog owns the client's stderr.
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build -RedirectStandardOutput "I:\progwork\JKENGINE\tmp\vpt5_server_stdout.log"
Start-Sleep -Seconds 4
Check "setup: server up" ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') ""
Start-Process -FilePath $exe -ArgumentList "--client","vplayer" `
    -WorkingDirectory $build -RedirectStandardError $errLog `
    -RedirectStandardOutput "I:\progwork\JKENGINE\tmp\vpt5_client_stdout.log"
Start-Sleep -Seconds 6
Find-Server
Check "setup: vplayer launched" (Refresh-Geom) "no Video Player layer"

# ---------- S1: corrupt files x3 -> classified openError, no crash ----------
Type-IntoPath "I:/progwork/JKENGINE/tmp/vpt1_trunc_tail.mp4"
Click-Open
Start-Sleep -Seconds 3
Save-ServerShot "vpt5-s1a-openerror-trunc.png"
Check "S1a: no crash after truncated-tail mp4" ((VPlayer-Alive) -and ((Ping-Ms) -ge 0)) ""

Type-IntoPath "I:/progwork/JKENGINE/tmp/vpt1_zero.mp4"
Click-Open
Start-Sleep -Seconds 3
Save-ServerShot "vpt5-s1b-zero-byte.png"
Check "S1b: no crash after 0-byte file" ((VPlayer-Alive) -and ((Ping-Ms) -ge 0)) ""

Type-IntoPath "I:/progwork/JKENGINE/tmp/vpt1_hdr.mp4"
Click-Open
Start-Sleep -Seconds 3
Save-ServerShot "vpt5-s1c-header-corrupt.png"
Check "S1c: no crash after header-corrupted file" ((VPlayer-Alive) -and ((Ping-Ms) -ge 0)) ""

# ---------- S1d: silent pipe -> "opening" phase + cancel ----------
$holder = Start-PipeHolder 'vpt5_hang'
Start-Sleep -Seconds 1
Type-IntoPath $pipeArg
Click-Open
Start-Sleep -Seconds 2
Save-ServerShot "vpt5-s1d-opening.png"
Check "S1d: no crash 2s into hung open (opening phase)" ((VPlayer-Alive) -and ((Ping-Ms) -ge 0)) ""
Click-App 115 42    # cancel button (right of the opening text, vpt1 measured)
Start-Sleep -Seconds 3
Save-ServerShot "vpt5-s1e-cancelled.png"
Check "S1e: no crash after open cancel" ((VPlayer-Alive) -and ((Ping-Ms) -ge 0)) ""
Stop-PipeHolder $holder

# ---------- S2: hi-res playback, underrun counter + seek audio recovery ----------
Type-IntoPath $hiRes
Click-Open
Start-Sleep -Seconds 8
Save-ServerShot "vpt5-s2-t8.png"
Start-Sleep -Seconds 17
Save-ServerShot "vpt5-s2-t25.png"
$p = Ping-Ms
Check "S2: responsive during high-res playback" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Start-Sleep -Seconds 20
Save-ServerShot "vpt5-s2-t45.png"
Check "S2: no crash after ~45 s high-res playback" (VPlayer-Alive) ""

# seeks on the hi-res file -> audio recovery (clock must advance)
foreach ($f in @(0.3, 0.7, 0.5)) { Seek-Frac $f; Start-Sleep -Milliseconds 800 }
Start-Sleep -Seconds 2
Save-ServerShot "vpt5-s2-after-seeks.png"
$p = Ping-Ms
Check "S2: responsive after hi-res seeks" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Start-Sleep -Seconds 4
Save-ServerShot "vpt5-s2-clock-later.png"
Check "S2: no crash after hi-res seeks" (VPlayer-Alive) ""

# ---------- S3: seek correctness on the local mp4 + indexless-source failure ----------
Type-IntoPath $mp4
Click-Open
Start-Sleep -Seconds 4
$marks = @(0.8, 0.2, 0.6, 0.35, 0.9)
$maxPing = 0.0
foreach ($f in $marks) {
    Seek-Frac $f
    $pp = Ping-Ms
    if ($pp -gt $maxPing) { $maxPing = $pp }
    Start-Sleep -Milliseconds 700
}
Save-ServerShot "vpt5-s3a-after-seeks.png"
Check "S3: 5 slider seeks, UI stays responsive" ($maxPing -ge 0 -and $maxPing -lt 500) ("max ping {0:N0} ms" -f $maxPing)
foreach ($f in @(0.1, 0.7, 0.25, 0.85)) { Seek-Frac $f; Start-Sleep -Milliseconds 300 }
Start-Sleep -Milliseconds 500
Save-ServerShot "vpt5-s3b-after-scrub-burst.png"
$p = Ping-Ms
Check "S3: responsive after scrub burst" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S3: no crash" (VPlayer-Alive) ""

# indexless-source proxy: raw H.264 elementary stream (avformat_seek_file fails)
Type-IntoPath $raw
Click-Open
Start-Sleep -Seconds 8
Seek-Frac 0.05
Start-Sleep -Milliseconds 1200
# open-latency flake absorber (2026-09-24, runs 1/3): if the raw open hadn't
# finished when the first seek was attempted, no SeekCommon ran and no
# diagnostic could fire. Retry once on a different fraction — the source is
# still indexless, so the failed-seek contract is unchanged.
$diag = @(Select-String -Path $errLog -SimpleMatch '[vplayer] seek failed').Count
if ($diag -lt 1) {
    Seek-Frac 0.15
    Start-Sleep -Milliseconds 1200
    $diag = @(Select-String -Path $errLog -SimpleMatch '[vplayer] seek failed').Count
}
Save-ServerShot "vpt5-s3c-raw-seek-fail.png"
$p = Ping-Ms
Check "S3: UI responsive right after failed seek (raw h264)" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S3: no crash after failed seek" (VPlayer-Alive) ""
Start-Sleep -Milliseconds 500
Check "S3: failed-seek diagnostic fired" ($diag -ge 1) ("[vplayer] seek-failed diagnostics: $diag")

# ---------- S4: wheel scrub (vpt4 recipe) ----------
# back to the mp4: it has the timecode burn-in + 30 s duration
Type-IntoPath $mp4
Click-Open
Start-Sleep -Seconds 4
Save-ServerShot "vpt5-s4a-playing.png"
Hover-Knob
Start-Sleep -Milliseconds 600
Send-Wheel 40
Save-ServerShotFast "vpt5-s4b-mid-scrub.png"    # inside the 400 ms window
Start-Sleep -Milliseconds 700
Save-ServerShotFast "vpt5-s4c-after-release.png"
Start-Sleep -Seconds 2
Save-ServerShotFast "vpt5-s4d-playing-on.png"
$p = Ping-Ms
Check "S4: responsive after wheel scrub from playing" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S4: no crash" (VPlayer-Alive) ""

Seek-Frac 0.8
Start-Sleep -Milliseconds 800
Click-App 28 68        # Pause
Start-Sleep -Milliseconds 500
Hover-Knob
Save-ServerShotFast "vpt5-s4e-paused-before.png"
Send-Wheel -40
Save-ServerShotFast "vpt5-s4f-paused-mid-scrub.png"
Start-Sleep -Milliseconds 700
Save-ServerShotFast "vpt5-s4g-paused-after-release.png"
Start-Sleep -Seconds 2
Save-ServerShotFast "vpt5-s4h-still-paused.png"
$p = Ping-Ms
Check "S4: responsive after paused scrub" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S4: no crash" (VPlayer-Alive) ""

# ---------- S5: regression ??pause/play, volume, avDelay, jog drag ----------
Click-App 28 68        # Pause
Start-Sleep -Milliseconds 500
Save-ServerShotFast "vpt5-s5a-paused.png"
Click-App 28 68        # Play
Start-Sleep -Milliseconds 500
Click-App 930 70       # volume slider (transport row right, measured)
Start-Sleep -Milliseconds 400
Save-ServerShotFast "vpt5-s5b-volume-clicked.png"
Click-App 100 118      # A/V sync slider (second row, measured)
Start-Sleep -Milliseconds 400
Save-ServerShotFast "vpt5-s5c-avsync-clicked.png"
Drag-Knob 2.0
Start-Sleep -Milliseconds 700
Save-ServerShotFast "vpt5-s5d-jog-cw.png"
Drag-Knob -2.0
Start-Sleep -Milliseconds 700
Save-ServerShotFast "vpt5-s5e-jog-ccw.png"
Start-Sleep -Seconds 2
Save-ServerShotFast "vpt5-s5f-playing-on.png"
$p = Ping-Ms
Check "S5: responsive after regression controls" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S5: no crash" (VPlayer-Alive) ""

# ---------- teardown ----------
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Write-Host ("RESULT: " + $(if ($script:fails -eq 0) { "ALL PASS" } else { "$($script:fails) FAILURE(S)" }))
exit $(if ($script:fails -eq 0) { 0 } else { 1 })
