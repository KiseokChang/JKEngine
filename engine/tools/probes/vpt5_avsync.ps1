# vpt5 follow-up: A/V sync slider retest. The gate run's click at (100,118)
# left the value at +0.00 — this retry clicks mid-right of the 160 px slider
# (surface x=150 -> fraction ~0.79 -> expected ~ +0.5 s) and screenshots.
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wt5b {
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
[Wt5b]::SetProcessDPIAware() | Out-Null
$build = "I:\progwork\JKENGINE\engine\build"
$exe = "$build\jkdesktop.exe"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-14-vplayer-stability"
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
    if (-not $h) { exit 1 }
    $script:srvHwnd = [IntPtr]$h
    $crect = New-Object Wt5b+RECT
    [Wt5b]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt5b+POINT; $co.X = 0; $co.Y = 0
    [Wt5b]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
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
function Click-App([double]$lx, [double]$ly) {
    Find-Server
    [Wt5b]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    $px = [int]([math]::Round($script:ox + ($script:layerX + $lx) * $script:scale))
    $py = [int]([math]::Round($script:oy + ($script:layerY + $ly) * $script:scale))
    [Wt5b]::SetCursorPos($px, $py) | Out-Null
    Start-Sleep -Milliseconds 250
    [Wt5b]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [Wt5b]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
}
function Type-IntoPath([string]$path) {
    Click-App 400 42
    Start-Sleep -Milliseconds 300
    [Wt5b]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-Clipboard -Value $path
    Start-Sleep -Milliseconds 150
    [System.Windows.Forms.SendKeys]::SendWait("{END}")
    Start-Sleep -Milliseconds 100
    [System.Windows.Forms.SendKeys]::SendWait("{BS 300}")
    Start-Sleep -Milliseconds 150
    [System.Windows.Forms.SendKeys]::SendWait("^v")
    Start-Sleep -Milliseconds 400
}
function Save-ServerShot([string]$name) {
    Find-Server
    $crect = New-Object Wt5b+RECT
    [Wt5b]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt5b+POINT; $co.X = 0; $co.Y = 0
    [Wt5b]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
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
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build
Start-Sleep -Seconds 4
Invoke-Agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
Start-Sleep -Seconds 6
# geometry settle: the shell relocates the window shortly after spawn —
# re-sync until two consecutive list_windows reads agree, then wait 2 s more
$prev = ""
for ($i = 0; $i -lt 15; $i++) {
    Find-Server | Out-Null
    if (Find-VPlayerLayer) {
        $cur = "$script:layerX,$script:layerY"
        if ($cur -eq $prev) { break }
        $prev = $cur
    }
    Start-Sleep -Milliseconds 500
}
Start-Sleep -Seconds 2
Find-Server | Out-Null
Find-VPlayerLayer | Out-Null
Type-IntoPath "I:/progwork/JKENGINE/tmp/vpt2_test.mp4"
Click-App 808 42
Start-Sleep -Seconds 4
Save-ServerShot "vpt5-s5g-before-avsync2.png"
# drag the A/V sync slider: down at mid, sweep right, up
Find-Server | Out-Null
[Wt5b]::SetForegroundWindow($script:srvHwnd) | Out-Null
Start-Sleep -Milliseconds 200
$px0 = [int]([math]::Round($script:ox + ($script:layerX + 100) * $script:scale))
$py0 = [int]([math]::Round($script:oy + ($script:layerY + 118) * $script:scale))
[Wt5b]::SetCursorPos($px0, $py0) | Out-Null
Start-Sleep -Milliseconds 250
[Wt5b]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 120
for ($i = 1; $i -le 10; $i++) {
    [Wt5b]::SetCursorPos($px0 + $i * 8, $py0) | Out-Null
    Start-Sleep -Milliseconds 40
}
Start-Sleep -Milliseconds 120
[Wt5b]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 500
Save-ServerShot "vpt5-s5h-after-avsync2.png"
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null