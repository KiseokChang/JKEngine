# Click the compositor close overlay on the fit-scaled jango layer.
# Layer pos (20,20), scale 0.667, display 1280x720 -> close overlay at
# surface (1898..1918, 2..22) -> logical ~(1292, 28).
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class W6 {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
[W6]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
$h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
    Select-Object -First 1).MainWindowHandle
$h = [IntPtr]$h
$crect = New-Object W6+RECT
[W6]::GetClientRect($h, [ref]$crect) | Out-Null
$co = New-Object W6+POINT; $co.X = 0; $co.Y = 0
[W6]::ClientToScreen($h, [ref]$co) | Out-Null
$scale = ($crect.R - $crect.L) / 1280.0

$lx = 1292; $ly = 28
$px = [int]([math]::Round($co.X + $lx * $scale))
$py = [int]([math]::Round($co.Y + $ly * $scale))
[W6]::SetCursorPos($px, $py) | Out-Null
Start-Sleep -Milliseconds 250
[W6]::mouse_event(2,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 60
[W6]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
Start-Sleep -Milliseconds 800

$w = $crect.R - $crect.L; $ht = $crect.B - $crect.T
$bmp = New-Object System.Drawing.Bitmap($w, $ht)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size)
$bmp.Save("I:\progwork\JKENGINE\tmp\jango_closed.png", [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Host "clicked close at logical ($lx,$ly); saved jango_closed.png"