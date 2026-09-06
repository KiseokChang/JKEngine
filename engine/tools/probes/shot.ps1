$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class W3 {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    public struct RECT { public int L, T, R, B; }
}
"@
[W3]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
$h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
    Select-Object -First 1).MainWindowHandle
$r = New-Object W3+RECT
[W3]::GetWindowRect([IntPtr]$h, [ref]$r) | Out-Null
$w = $r.R - $r.L
$hh = $r.B - $r.T
$bmp = New-Object System.Drawing.Bitmap $w, $hh
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
$bmp.Save("I:\progwork\JKENGINE\tmp\smoke_now.png")
Write-Host ("saved {0}x{1} at ({2},{3})" -f $w, $hh, $r.L, $r.T)