$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class W4 {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
[W4]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
$h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
    Select-Object -First 1).MainWindowHandle
$h = [IntPtr]$h
$crect = New-Object W4+RECT
[W4]::GetClientRect($h, [ref]$crect) | Out-Null
$co = New-Object W4+POINT; $co.X = 0; $co.Y = 0
[W4]::ClientToScreen($h, [ref]$co) | Out-Null
$w = $crect.R - $crect.L; $ht = $crect.B - $crect.T
$bmp = New-Object System.Drawing.Bitmap($w, $ht)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size)
$bmp.Save("I:\progwork\JKENGINE\tmp\jango_fitscale.png", [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Host "saved jango_fitscale.png ($w x $ht)"