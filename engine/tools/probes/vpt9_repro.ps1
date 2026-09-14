# vpt9 repro: isolate the S3 wedge (long forward jog to EOF clamp, then
# reverse across the ring start -> fallback seek -> display frozen).
# Deterministic rerun of the vpt9 S3 sequence on a fresh player, with
# longer observation and a +-1F decode-aliveness probe at the end.
param()
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wr9 {
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
[Wr9]::SetProcessDPIAware() | Out-Null
$build   = "I:\progwork\JKENGINE\engine\build"
$exe     = "$build\jkdesktop.exe"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-15-vplayer-jog-framescrub\shots"
$mp4     = "I:/progwork/JKENGINE/tmp/vpt2_test.mp4"

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
    $crect = New-Object Wr9+RECT
    [Wr9]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wr9+POINT; $co.X = 0; $co.Y = 0
    [Wr9]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
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
function Refresh-Geom {
    Find-Server
    for ($i = 0; $i -lt 10; $i++) { if (Find-VPlayerLayer) { return $true }; Start-Sleep -Milliseconds 300 }
    return $false
}
function Set-LayerCursor([double]$lx, [double]$ly) {
    $px = [int]([math]::Round($script:ox + ($script:layerX + $lx) * $script:scale))
    $py = [int]([math]::Round($script:oy + ($script:layerY + $ly) * $script:scale))
    [Wr9]::SetCursorPos($px, $py) | Out-Null
}
function Send-Click([double]$lx, [double]$ly) {
    Refresh-Geom | Out-Null
    [Wr9]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-LayerCursor $lx $ly
    Start-Sleep -Milliseconds 250
    [Wr9]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [Wr9]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
}
function Type-IntoPath([string]$path) {
    Send-Click 400 42
    Start-Sleep -Milliseconds 300
    [Wr9]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    [System.Windows.Forms.SendKeys]::SendWait("{END}")
    Start-Sleep -Milliseconds 100
    [System.Windows.Forms.SendKeys]::SendWait("{BS 300}")
    Start-Sleep -Milliseconds 150
    [System.Windows.Forms.SendKeys]::SendWait($path)
    Start-Sleep -Milliseconds 400
}
function Save-Shot([string]$name) {
    Refresh-Geom | Out-Null
    $crect = New-Object Wr9+RECT
    [Wr9]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wr9+POINT; $co.X = 0; $co.Y = 0
    [Wr9]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
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
function Seek-Frac([double]$f) { Send-Click (150.0 + 690.0 * $f) 77 }
function Send-Wheel([int]$ticks, [int]$delayMs) {
    $d = $(if ($ticks -ge 0) { 120 } else { -120 })
    for ($i = 0; $i -lt [math]::Abs($ticks); $i++) {
        [Wr9]::mouse_event(0x0800, 0, 0, $d, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds $delayMs
    }
}
function Hover-Knob {
    Refresh-Geom | Out-Null
    [Wr9]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-LayerCursor 888 582
    Start-Sleep -Milliseconds 250
}

# ---------- fresh server + player ----------
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build
Start-Sleep -Seconds 4
Invoke-Agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
Start-Sleep -Seconds 6
if (-not (Refresh-Geom)) { Write-Host "FAIL: no vplayer"; exit 1 }
Type-IntoPath $mp4
Send-Click 808 42
Start-Sleep -Seconds 4
Save-Shot "vpt9-r0-open.png"

# ---------- R1: paused at 15 s, jog forward to the EOF clamp ----------
Send-Click 28 68          # Pause (fresh open = playing)
Start-Sleep -Milliseconds 500
Seek-Frac 0.5             # ~15 s (paused seek)
Start-Sleep -Milliseconds 1500
Hover-Knob
Send-Wheel 60 25          # +14 s -> target clamps at ~29.97 (EOF)
Start-Sleep -Milliseconds 1200
Save-Shot "vpt9-r1-fwd-eof.png"

# ---------- R2: reverse across the ring start (fallback) ----------
Send-Wheel -60 25         # -14 s -> target ~15.9 < ring start -> fallback
Start-Sleep -Milliseconds 1200
Save-Shot "vpt9-r2-fallback.png"
Start-Sleep -Seconds 5
Save-Shot "vpt9-r3-plus5s.png"
Start-Sleep -Seconds 5
Save-Shot "vpt9-r4-plus10s.png"

# ---------- R4: is decode alive? +-1F precision seek must repaint ----------
Send-Click 824 582        # ">" = +1F
Start-Sleep -Milliseconds 800
Save-Shot "vpt9-r5-step1f.png"
Write-Host "repro done"