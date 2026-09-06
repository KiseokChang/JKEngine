# Server-mode mouse-capture smoke test (PMv2-aware).
# 1) find server window  2) click minesweeper icon  3) press inside the
# client surface, drag outside it, release  -> server log must show the
# forwarded MouseMove/MouseUp with out-of-bounds surface coords.
$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Win {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowW(string cls, string title);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@

[Win]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null

$h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
    Select-Object -First 1).MainWindowHandle
$h = [IntPtr]$h
if ($h -eq [IntPtr]::Zero) { Write-Host "FATAL: server window not found"; exit 1 }

$frame = New-Object Win+RECT
[Win]::GetWindowRect($h, [ref]$frame) | Out-Null
$crect = New-Object Win+RECT
[Win]::GetClientRect($h, [ref]$crect) | Out-Null
$co = New-Object Win+POINT
$co.X = 0; $co.Y = 0
[Win]::ClientToScreen($h, [ref]$co) | Out-Null

$clientW = $crect.R - $crect.L
$scale = $clientW / 1280.0
Write-Host ("frame=({0},{1}) clientW={2} scale={3:F3}" -f $frame.L, $frame.T, $clientW, $scale)

function Click([int]$lx, [int]$ly) {
    $px = [int]([math]::Round($co.X + $lx * $scale))
    $py = [int]([math]::Round($co.Y + $ly * $scale))
    [Win]::SetCursorPos($px, $py) | Out-Null
    Start-Sleep -Milliseconds 120
    [Win]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)   # LEFTDOWN
    Start-Sleep -Milliseconds 80
    [Win]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)   # LEFTUP
}

function CellPt([int]$lx, [int]$ly, [string]$tag) {
    $px = [int]([math]::Round($co.X + $lx * $scale))
    $py = [int]([math]::Round($co.Y + $ly * $scale))
    Write-Host ("{0}: client=({1},{2}) screen=({3},{4})" -f $tag, $lx, $ly, $px, $py)
}

# 1) launcher icon "minesweeper" at logical (82, 90) center
CellPt 82 90 "iconClick"
Click 82 90
Start-Sleep -Seconds 3   # allow spawn + surface creation

# 2) press inside surface (surface px (30,40) => logical (510,210)),
#    drag out to launcher bg (logical (200,120)), release there.
$inX = 480 + 30; $inY = 170 + 40
$outX = 200; $outY = 120
$inPxX = [int]([math]::Round($co.X + $inX * $scale))
$inPxY = [int]([math]::Round($co.Y + $inY * $scale))
$outPxX = [int]([math]::Round($co.X + $outX * $scale))
$outPxY = [int]([math]::Round($co.Y + $outY * $scale))
Write-Host ("drag inside=({0},{1}) outside=({2},{3})" -f $inPxX, $inPxY, $outPxX, $outPxY)

[Win]::SetCursorPos($inPxX, $inPxY) | Out-Null
Start-Sleep -Milliseconds 150
[Win]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)      # LEFTDOWN inside surface
Start-Sleep -Milliseconds 120
[Win]::SetCursorPos($outPxX, $outPxY) | Out-Null     # move cursor OUT of surface
Start-Sleep -Milliseconds 120
[Win]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)      # LEFTUP outside surface
Write-Host "DONE"