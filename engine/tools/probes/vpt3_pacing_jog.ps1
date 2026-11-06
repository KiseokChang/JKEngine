# vpt3diag: jog-session measurement for task-3 diagnosis (docs/50 s9 pacing cap).
# Reuses the vpt10 harness recipe: DPI-aware, layer-relative geometry, knob
# hover at (888,582) in the 960x640 layer, wheel ticks feed the jog target.
# The vplayer client process must already be running with stderr redirected to
# a file (passed via -Log) - this probe only injects wheel input and takes
# shots; the counters are read from the stderr log afterwards.
param([string]$Mp4 = "I:/progwork/JKENGINE/tmp/vpt2_test.mp4",
      [string]$Log = "I:\progwork\JKENGINE\engine\build\vpt3_jog.log",
      [string]$ShotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-15-vplayer-nv12-upload\shots")
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
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, int d, UIntPtr e);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
[Wt3]::SetProcessDPIAware() | Out-Null
New-Item -ItemType Directory -Force -Path $ShotDir | Out-Null

$build = "I:\progwork\JKENGINE\engine\build"
$exe   = "$build\jkdesktop.exe"

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
function Find-Layer {
    $list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    $m = [regex]::Match($list, '"title":"Video Player".*?"x":(-?\d+),"y":(-?\d+),"w":(\d+),"h":(\d+)')
    if (-not $m.Success) { return $false }
    $script:layerX = [int]$m.Groups[1].Value
    $script:layerY = [int]$m.Groups[2].Value
    $script:layerW = [int]$m.Groups[3].Value
    $script:layerH = [int]$m.Groups[4].Value
    return $true
}
function Set-LayerCursor([double]$lx, [double]$ly) {
    $px = [int]([math]::Round($script:ox + ($script:layerX + $lx) * $script:scale))
    $py = [int]([math]::Round($script:oy + ($script:layerY + $ly) * $script:scale))
    [Wt3]::SetCursorPos($px, $py) | Out-Null
}
function Save-ServerShot([string]$name) {
    Find-Layer | Out-Null
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
    $cw = [int]([math]::Round($script:layerW * $script:scale))
    $ch = [int]([math]::Round($script:layerH * $script:scale))
    if ($cx + $cw -gt $bmp.Width)  { $cw = $bmp.Width - $cx }
    if ($cy + $ch -gt $bmp.Height) { $ch = $bmp.Height - $cy }
    $rect = New-Object System.Drawing.Rectangle $cx, $cy, $cw, $ch
    $crop = $bmp.Clone($rect, $bmp.PixelFormat)
    $crop.Save((Join-Path $ShotDir $name))
    $crop.Dispose(); $bmp.Dispose()
    Write-Host ("shot {0}" -f $name)
}

# setup: fresh server + vplayer client (480p file), stderr to $Log
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build
Start-Sleep -Seconds 4
$env:JK_VPLAYER_OPEN = $Mp4
Start-Process -FilePath $exe -ArgumentList "--client","vplayer" -WorkingDirectory $build `
    -RedirectStandardError $Log
Start-Sleep -Seconds 10
if (-not (Find-Layer)) { Write-Host "FAIL: no Video Player layer"; exit 1 }
Write-Host "layer found"
Save-ServerShot "vpt3-jog-before.png"

# hover the knob and jog forward ~3 s of content at 150 ms cadence
Find-Server
[Wt3]::SetForegroundWindow($script:srvHwnd) | Out-Null
Start-Sleep -Milliseconds 300
Set-LayerCursor 888 582
Start-Sleep -Milliseconds 400
for ($i = 0; $i -lt 20; $i++) {
    [Wt3]::mouse_event(0x0800, 0, 0, 120, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 150
}
Save-ServerShot "vpt3-jog-session.png"
Start-Sleep -Milliseconds 500
Save-ServerShot "vpt3-jog2.png"
# let the 400 ms idle release fire, then let playback resume
Start-Sleep -Seconds 6
Save-ServerShot "vpt3-after-jog.png"
Write-Host "DONE jog session"