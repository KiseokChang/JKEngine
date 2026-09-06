$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class W2 {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
[W2]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
$h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
    Select-Object -First 1).MainWindowHandle
$h = [IntPtr]$h
$crect = New-Object W2+RECT
[W2]::GetClientRect($h, [ref]$crect) | Out-Null
$co = New-Object W2+POINT; $co.X = 0; $co.Y = 0
[W2]::ClientToScreen($h, [ref]$co) | Out-Null
$scale = ($crect.R - $crect.L) / 1280.0
$px = [int]([math]::Round($co.X + 200 * $scale))
$py = [int]([math]::Round($co.Y + 90 * $scale))
Write-Host ("probe click at logical (200,90) physical client=({0},{1})" -f 200, 90)
[W2]::SetCursorPos($px, $py) | Out-Null
Start-Sleep -Milliseconds 200
[W2]::mouse_event(2,0,0,0,[UIntPtr]::Zero)
Start-Sleep -Milliseconds 80
[W2]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
Write-Host "clicked"
