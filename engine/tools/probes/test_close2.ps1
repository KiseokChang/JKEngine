# Click the close overlay of the clamped fit layer at (0,0):
# surface (1898..1918, 2..22) x scale 0.667 -> logical (1272, 8)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class W7 {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
[W7]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
$h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
    Select-Object -First 1).MainWindowHandle
$h = [IntPtr]$h
$crect = New-Object W7+RECT
[W7]::GetClientRect($h, [ref]$crect) | Out-Null
$co = New-Object W7+POINT; $co.X = 0; $co.Y = 0
[W7]::ClientToScreen($h, [ref]$co) | Out-Null
$scale = ($crect.R - $crect.L) / 1280.0

$lx = 1272; $ly = 8
$px = [int]([math]::Round($co.X + $lx * $scale))
$py = [int]([math]::Round($co.Y + $ly * $scale))
[W7]::SetCursorPos($px, $py) | Out-Null
Start-Sleep -Milliseconds 250
[W7]::mouse_event(2,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 60
[W7]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
Start-Sleep -Milliseconds 800
Write-Host "clicked close at logical ($lx,$ly)"