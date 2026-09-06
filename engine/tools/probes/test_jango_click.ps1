# Click jango Personnel button through the fit-scale mapping.
# Layer: pos (20,20) logical, scale 0.667. Personnel center surface (175,85).
# -> logical (137, 77) -> physical x1.25
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class W5 {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
[W5]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
$h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
    Select-Object -First 1).MainWindowHandle
$h = [IntPtr]$h
$crect = New-Object W5+RECT
[W5]::GetClientRect($h, [ref]$crect) | Out-Null
$co = New-Object W5+POINT; $co.X = 0; $co.Y = 0
[W5]::ClientToScreen($h, [ref]$co) | Out-Null
$scale = ($crect.R - $crect.L) / 1280.0

$lx = 137; $ly = 77
$px = [int]([math]::Round($co.X + $lx * $scale))
$py = [int]([math]::Round($co.Y + $ly * $scale))
[W5]::SetCursorPos($px, $py) | Out-Null
Start-Sleep -Milliseconds 250
[W5]::mouse_event(2,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 60
[W5]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
Start-Sleep -Milliseconds 600

$w = $crect.R - $crect.L; $ht = $crect.B - $crect.T
$bmp = New-Object System.Drawing.Bitmap($w, $ht)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size)
$bmp.Save("I:\progwork\JKENGINE\tmp\jango_password.png", [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Host "clicked logical ($lx,$ly); saved jango_password.png"