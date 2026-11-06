# T3 e2e: audio-first ring refill + underrun counter + device-failure text.
# -STA required (Set-Clipboard, docs/15 cp949 convention). Harness recipe =
# vpt2_e2e.ps1 (same server/layer/click/crop mechanics, measured geometry).
#
# Run with -Tag baseline (pre-T3 build) and -Tag after (T3 build); the
# caller stashes/rebuilds between the two runs. Shots are named
# vpt3-<tag>-*.png; the "underrun: N" text sits in the layer's bottom-right
# corner and is read off the shots.
#
# S1 high-res mp4 (engine-generated synthetic, tmp/test_media/vpt_hires.mp4,
#    1920x1080 h264 High + AAC 44.1k stereo, 60 s): continuous playback —
#    underrun counter sampled at ~t+8,
#    t+25, t+45 s of playback. Counter is cumulative since file open.
# S2 local mp4 (tmp/vpt2_test.mp4, 30 s): slider seeks x5 including a rapid
#    3-seek burst ??after the last seek the clock must ADVANCE (the A/V
#    clock is delivered audio samples, so an advancing clock is audible
#    audio by construction), no permanent silence, no crash.
# S3 device-failure text: honest limitation ??SDL_OpenAudioDevice failure
#    cannot be induced live on this machine (no code-free trigger); the
#    code path is verified by inspection in the report instead.
param([string]$Tag = "after")
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wt3 {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
# DPI-aware: this box runs at 125% (dpi 120). Without this, CopyFromScreen
# grabs raw physical pixels at virtualized coords and the shot clips the
# surface's right/bottom ??the "underrun: N" line was invisible for exactly
# that reason (measured: client 1280x720 virtual = 1600x900 physical).
[Wt3]::SetProcessDPIAware() | Out-Null

$build   = "I:\progwork\JKENGINE\engine\build"
$exe     = "$build\jkdesktop.exe"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-14-vplayer-stability"
$hiRes   = "I:\progwork\JKENGINE\tmp\test_media\vpt_hires.mp4"
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
    $crect = New-Object Wt3+RECT
    [Wt3]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt3+POINT; $co.X = 0; $co.Y = 0
    [Wt3]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
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

function Send-Click([double]$lx, [double]$ly, [uint32]$down, [uint32]$up) {
    $px = [int]([math]::Round($script:ox + ($script:layerX + $lx) * $script:scale))
    $py = [int]([math]::Round($script:oy + ($script:layerY + $ly) * $script:scale))
    [Wt3]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    [Wt3]::SetCursorPos($px, $py) | Out-Null
    Start-Sleep -Milliseconds 250
    [Wt3]::mouse_event($down, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [Wt3]::mouse_event($up, 0, 0, 0, [UIntPtr]::Zero)
}
function Click-App([double]$lx, [double]$ly) { Send-Click $lx $ly 2 4 }

function Type-IntoPath([string]$path) {
    Click-App 400 42
    Start-Sleep -Milliseconds 300
    [Wt3]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    for ($i = 0; $i -lt 5; $i++) {
        try { Set-Clipboard -Value $path; break } catch {
            try { [System.Windows.Forms.Clipboard]::SetText($path); break } catch {
                Start-Sleep -Milliseconds 700 }
        }
    }
    Start-Sleep -Milliseconds 150
    [System.Windows.Forms.SendKeys]::SendWait("{END}")
    Start-Sleep -Milliseconds 100
    [System.Windows.Forms.SendKeys]::SendWait("{BS 300}")
    Start-Sleep -Milliseconds 150
    [System.Windows.Forms.SendKeys]::SendWait("^v")
    Start-Sleep -Milliseconds 400
}
function Click-Open { Click-App 808 42 }

function Save-ServerShot([string]$name) {
    Find-Server
    $crect = New-Object Wt3+RECT
    [Wt3]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt3+POINT; $co.X = 0; $co.Y = 0
    [Wt3]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($crect.R - $crect.L), ($crect.B - $crect.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size)
    $g.Dispose()
    $cx = [int]([math]::Round($script:layerX * $script:scale))
    $cy = [int]([math]::Round($script:layerY * $script:scale))
    # The layer rect IS the surface window (list_windows logical * scale =
    # physical); a full-layer crop keeps the bottom-right "underrun: N" line
    # (T3) and the jog knob both in frame.
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

function Ping-Ms {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $r = Invoke-Agentctl '{"tool":"ping","args":{}}'
    $sw.Stop()
    if ($r -notmatch '"ok"\s*:\s*true') { return -1.0 }
    return $sw.Elapsed.TotalMilliseconds
}

# Slider seek: commit a click at parameter f of the ##seek slider track.
function Seek-Frac([double]$f) {
    Click-App (150.0 + 690.0 * $f) 77
}

# ---------- setup ----------
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build
Start-Sleep -Seconds 4
Check "setup: server up" ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') ""
Invoke-Agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
Start-Sleep -Seconds 6
Find-Server
Check "setup: vplayer launched" (VPlayer-Alive) "no Video Player layer"

# ---------- S1: high-res playback, underrun counter sampled ----------
Type-IntoPath $hiRes
Click-Open
Start-Sleep -Seconds 8     # async open (local disk) + first frames
Save-ServerShot ("vpt3-{0}-s1-t8.png" -f $Tag)
Start-Sleep -Seconds 17    # ~t+25
Save-ServerShot ("vpt3-{0}-s1-t25.png" -f $Tag)
$p = Ping-Ms
Check "S1: responsive during high-res playback" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Start-Sleep -Seconds 20    # ~t+45
Save-ServerShot ("vpt3-{0}-s1-t45.png" -f $Tag)
Check "S1: no crash after ~45 s high-res playback" (VPlayer-Alive) ""

# ---------- S2: seek-then-audio-recovery on the local mp4 ----------
Type-IntoPath $mp4
Click-Open
Start-Sleep -Seconds 4
$marks = @(0.5, 0.05, 0.8, 0.35, 0.95)
foreach ($f in $marks) {
    Seek-Frac $f
    Start-Sleep -Milliseconds 700
}
# Rapid scrub burst (3 seeks ~2 s of intent, each click ~530 ms harness cost).
foreach ($f in @(0.1, 0.7, 0.45)) {
    Seek-Frac $f
    Start-Sleep -Milliseconds 300
}
Start-Sleep -Seconds 2
Save-ServerShot ("vpt3-{0}-s2-after-seeks.png" -f $Tag)
$p = Ping-Ms
Check "S2: responsive after seeks" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Start-Sleep -Seconds 3
Save-ServerShot ("vpt3-{0}-s2-clock-later.png" -f $Tag)
Check "S2: no crash" (VPlayer-Alive) ""

# ---------- teardown ----------
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Write-Host ("RESULT: " + $(if ($script:fails -eq 0) { "ALL PASS" } else { "$($script:fails) FAILURE(S)" }))
exit $(if ($script:fails -eq 0) { 0 } else { 1 })
