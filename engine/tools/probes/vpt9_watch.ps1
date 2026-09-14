# vpt9 live-watch: is the frozen display self-healing at clock ~25.07?
param()
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Ww9 {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
[Ww9]::SetProcessDPIAware() | Out-Null
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-15-vplayer-jog-framescrub\shots"
function Invoke-Agentctl([string]$json) {
    $escaped = $json.Replace('"', [string][char]92 + '"')
    return (& $exe agentctl $escaped 2>&1) -join "`n"
}
$h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
    Select-Object -First 1).MainWindowHandle
if (-not $h) { Write-Host "NO SERVER"; exit 1 }
$srv = [IntPtr]$h
$list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
$m = [regex]::Match($list, '"title":"Video Player".*?"x":(-?\d+),"y":(-?\d+),"w":(\d+),"h":(\d+)')
if (-not $m.Success) { Write-Host "NO VPLAYER"; exit 1 }
$lx = [int]$m.Groups[1].Value; $ly = [int]$m.Groups[2].Value
$lw = [int]$m.Groups[3].Value; $lh = [int]$m.Groups[4].Value
$crect = New-Object Ww9+RECT
[Ww9]::GetClientRect($srv, [ref]$crect) | Out-Null
$co = New-Object Ww9+POINT; $co.X = 0; $co.Y = 0
[Ww9]::ClientToScreen($srv, [ref]$co) | Out-Null
$scale = ($crect.R - $crect.L) / 1280.0
for ($i = 1; $i -le 4; $i++) {
    $bmp = New-Object System.Drawing.Bitmap ($crect.R - $crect.L), ($crect.B - $crect.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size)
    $g.Dispose()
    $cxi = [int]([math]::Round($lx * $scale)); $cyi = [int]([math]::Round($ly * $scale))
    $cwi = [int]([math]::Round($lw * $scale)); $chi = [int]([math]::Round($lh * $scale))
    $rect = New-Object System.Drawing.Rectangle $cxi, $cyi, $cwi, $chi
    $crop = $bmp.Clone($rect, $bmp.PixelFormat)
    $name = "vpt9-watch-$i.png"
    $crop.Save((Join-Path $shotDir $name))
    $crop.Dispose(); $bmp.Dispose()
    Write-Host ("shot " + $name)
    Start-Sleep -Seconds 2
}
Write-Host "watch done"