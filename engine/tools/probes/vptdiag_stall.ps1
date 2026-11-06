# vptdiag: docs/50 s9.6 obs-2 UI-stall reproduction probe (diagnostic only).
# Wheel sessions on the jog knob + hover + foreground switching, with a TEXT
# (pixel-hash) liveness sampler between actions: a UI stall shows as a hash
# freeze window while the audio clock keeps running. Client stderr goes to
# -Log (instrumented [uistall] rows appear there when the UI thread blocks).
# ASCII only, PS 5.1.
param([string]$Mp4 = "I:/progwork/JKENGINE/tmp/vpt2_test.mp4",
      [string]$Log = "I:\progwork\JKENGINE\engine\build\vptdiag_ui.log",
      [int]$Rounds = 4,
      [int]$WheelDelta = -120)
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wd {
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
[Wd]::SetProcessDPIAware() | Out-Null

$build = "I:\progwork\JKENGINE\engine\build"
$exe   = "$build\jkdesktop.exe"
$sw = [System.Diagnostics.Stopwatch]::StartNew()

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
    $crect = New-Object Wd+RECT
    [Wd]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wd+POINT; $co.X = 0; $co.Y = 0
    [Wd]::ClientToScreen($script:srvHwnd, [ref]$co)
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
    [Wd]::SetCursorPos($px, $py) | Out-Null
}
# TEXT liveness sample: hash the vplayer layer region, print one line per call.
function Sample([string]$tag) {
    $bmp = New-Object System.Drawing.Bitmap $script:layerW, $script:layerH
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $sx = [int]([math]::Round($script:ox + $script:layerX * $script:scale))
    $sy = [int]([math]::Round($script:oy + $script:layerY * $script:scale))
    $g.CopyFromScreen($sx, $sy, 0, 0, (New-Object System.Drawing.Size ($script:layerW), ($script:layerH)))
    $g.Dispose()
    $rect = New-Object System.Drawing.Rectangle 0, 0, $script:layerW, $script:layerH
    $crop = $bmp.Clone($rect, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $bmp.Dispose()
    $bd = $crop.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, $crop.PixelFormat)
    $bytes = New-Object byte[] ($bd.Stride * $bd.Height)
    [System.Runtime.InteropServices.Marshal]::Copy($bd.Scan0, $bytes, 0, $bytes.Length)
    $crop.UnlockBits($bd)
    $crop.Dispose()
    # sample every 16th pixel of every 8th row for a cheap stable digest
    $h = [long]0
    for ($y = 0; $y -lt $bd.Height; $y += 8) {
        $row = $y * $bd.Stride
        for ($x = 0; $x -lt $script:layerW * 4; $x += 64) {
            $h = (([long]$h * 31) + $bytes[$row + $x]) -band 0xFFFFFF
        }
    }
    Write-Host ("smp t={0} {1} h={2:x8}" -f $sw.ElapsedMilliseconds, $tag, $h)
}

# setup: fresh server + vplayer client, stderr to $Log
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
Find-Server

for ($r = 1; $r -le $Rounds; $r++) {
    Write-Host ("round {0}" -f $r)
    [Wd]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 300
    Set-LayerCursor 888 582
    Start-Sleep -Milliseconds 400
    Sample ("r{0}-pre" -f $r)
    for ($i = 0; $i -lt 20; $i++) {
        [Wd]::mouse_event(0x0800, 0, 0, $WheelDelta, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 100
        if ($i % 2 -eq 1) { Sample ("r{0}-w{1}" -f $r, $i) }
    }
    # idle release (400 ms) fires during this window; release-seek + resume
    Start-Sleep -Milliseconds 500
    Sample ("r{0}-post" -f $r)
    Start-Sleep -Milliseconds 400
    Sample ("r{0}-post2" -f $r)
    Start-Sleep -Milliseconds 400
    Sample ("r{0}-post3" -f $r)
    Start-Sleep -Milliseconds 600
    Sample ("r{0}-post4" -f $r)
    Start-Sleep -Milliseconds 800
    Sample ("r{0}-post5" -f $r)
    # foreground switch storm + hover moves across the UI
    for ($k = 0; $k -lt 3; $k++) {
        [Wd]::SetForegroundWindow($script:srvHwnd) | Out-Null
        Set-LayerCursor 480 60
        Start-Sleep -Milliseconds 150
        Set-LayerCursor 888 582
        Start-Sleep -Milliseconds 150
    }
    Sample ("r{0}-end" -f $r)
    Start-Sleep -Seconds 3
    Sample ("r{0}-tail" -f $r)
}
Write-Host "DONE stall probe"