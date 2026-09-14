# vpt9 diag: why did Type-IntoPath fail? Test clipboard set + paste + typing.
param()
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wd9 {
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
[Wd9]::SetProcessDPIAware() | Out-Null
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-15-vplayer-jog-framescrub\shots"

function Invoke-Agentctl([string]$json) {
    $escaped = $json.Replace('"', [string][char]92 + '"')
    return (& $exe agentctl $escaped 2>&1) -join "`n"
}
$h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
    Select-Object -First 1).MainWindowHandle
Write-Host ("server hwnd: " + $h)
if (-not $h) { Write-Host "NO SERVER"; exit 1 }
$srv = [IntPtr]$h
$list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
$m = [regex]::Match($list, '"title":"Video Player".*?"x":(-?\d+),"y":(-?\d+),"w":(\d+),"h":(\d+)')
if (-not $m.Success) { Write-Host "NO VPLAYER LAYER"; Write-Host $list; exit 1 }
$lx = [int]$m.Groups[1].Value; $ly = [int]$m.Groups[2].Value
$lw = [int]$m.Groups[3].Value; $lh = [int]$m.Groups[4].Value
$crect = New-Object Wd9+RECT
[Wd9]::GetClientRect($srv, [ref]$crect) | Out-Null
$co = New-Object Wd9+POINT; $co.X = 0; $co.Y = 0
[Wd9]::ClientToScreen($srv, [ref]$co) | Out-Null
$scale = ($crect.R - $crect.L) / 1280.0
Write-Host ("layer {0},{1} {2}x{3} scale {4} origin {5},{6}" -f $lx,$ly,$lw,$lh,$scale,$co.X,$co.Y)

function Set-LayerCursor([double]$cx, [double]$cy) {
    $px = [int]([math]::Round($co.X + ($lx + $cx) * $scale))
    $py = [int]([math]::Round($co.Y + ($ly + $cy) * $scale))
    [Wd9]::SetCursorPos($px, $py) | Out-Null
}
function Save-Shot([string]$name) {
    $crect2 = New-Object Wd9+RECT
    [Wd9]::GetClientRect($srv, [ref]$crect2) | Out-Null
    $co2 = New-Object Wd9+POINT; $co2.X = 0; $co2.Y = 0
    [Wd9]::ClientToScreen($srv, [ref]$co2) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($crect2.R - $crect2.L), ($crect2.B - $crect2.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($co2.X, $co2.Y, 0, 0, $bmp.Size)
    $g.Dispose()
    $cxi = [int]([math]::Round($lx * $scale)); $cyi = [int]([math]::Round($ly * $scale))
    $cwi = [int]([math]::Round($lw * $scale)); $chi = [int]([math]::Round($lh * $scale))
    $rect = New-Object System.Drawing.Rectangle $cxi, $cyi, $cwi, $chi
    $crop = $bmp.Clone($rect, $bmp.PixelFormat)
    $crop.Save((Join-Path $shotDir $name))
    $crop.Dispose(); $bmp.Dispose()
    Write-Host ("shot " + $name)
}

# Step 1: can we set the clipboard at all?
try {
    Set-Clipboard -Value "I:/progwork/JKENGINE/tmp/vpt2_test.mp4"
    $got = Get-Clipboard
    Write-Host ("clipboard set OK, readback: " + $got)
} catch { Write-Host ("clipboard FAILED: " + $_.Exception.Message) }

# Step 2: click the path row, paste, shot.
[Wd9]::SetForegroundWindow($srv) | Out-Null
Start-Sleep -Milliseconds 300
Set-LayerCursor 400 42
Start-Sleep -Milliseconds 400
[Wd9]::mouse_event(2,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 80
[Wd9]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
Start-Sleep -Milliseconds 300
[System.Windows.Forms.SendKeys]::SendWait("{END}")
Start-Sleep -Milliseconds 100
[System.Windows.Forms.SendKeys]::SendWait("{BS 300}")
Start-Sleep -Milliseconds 150
[System.Windows.Forms.SendKeys]::SendWait("^v")
Start-Sleep -Milliseconds 400
Save-Shot "vpt9-diag-paste.png"

# Step 3: fallback - direct typing into the field (clear first).
[System.Windows.Forms.SendKeys]::SendWait("{END}")
Start-Sleep -Milliseconds 100
[System.Windows.Forms.SendKeys]::SendWait("{BS 300}")
Start-Sleep -Milliseconds 150
[System.Windows.Forms.SendKeys]::SendWait("I:/progwork/JKENGINE/tmp/vpt2_test.mp4")
Start-Sleep -Milliseconds 300
Save-Shot "vpt9-diag-type.png"
Write-Host "diag done"