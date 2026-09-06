Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class PT {
    public struct POINT { public int X, Y; }
    [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT p);
    [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, System.Text.StringBuilder sb, int max);
    [DllImport("user32.dll")] public static extern bool IsWindowUnicode(IntPtr h);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
}
"@

$pt = New-Object PT+POINT
$pt.X = 798; $pt.Y = 352
$w = [PT]::WindowFromPoint($pt)
$pid2 = 0
[PT]::GetWindowThreadProcessId($w, [ref]$pid2) | Out-Null
$proc = Get-Process -Id $pid2 -ErrorAction SilentlyContinue
$sb = New-Object System.Text.StringBuilder 256
[PT]::GetWindowTextW($w, $sb, 256) | Out-Null
Write-Host ("under(798,352): hwnd={0} pid={1} proc={2} enabled={3} title=[{4}]" -f $w, $pid2, $proc.ProcessName, ([PT]::IsWindowEnabled($w)), $sb)

$fg = [PT]::GetForegroundWindow()
$pid3 = 0
[PT]::GetWindowThreadProcessId($fg, [ref]$pid3) | Out-Null
$procf = Get-Process -Id $pid3 -ErrorAction SilentlyContinue
$sb2 = New-Object System.Text.StringBuilder 256
[PT]::GetWindowTextW($fg, $sb2, 256) | Out-Null
Write-Host ("foreground: hwnd={0} pid={1} proc={2} title=[{3}]" -f $fg, $pid3, $procf.ProcessName, $sb2)