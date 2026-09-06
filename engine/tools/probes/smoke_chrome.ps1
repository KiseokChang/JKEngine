# Server-mode chrome smoke test: title-drag move, close button, border resize,
# multi-client spawn. All coordinates are server-window LOGICAL points
# (server logical size = 1280x720); converted to physical via clientW/1280.
#
# Client surface sizes: minesweeper 320x380, tetris 320x520.
# Spawn placement: centered + 20px cascade per existing client.
#
# Sequence (z-order tracked in comments):
#   spawn MS1 @ (480,170)   z: [MS1]
#   spawn TET @ (500,120)   z: [MS1, TET]
#   spawn MS2 @ (520,210)   z: [MS1, TET, MS2]
#   move TET  -> (950,88)   grab TET-local (100,12)
#   move MS2  -> (60,300)   grab MS2-local (150,12)
#   move MS1  -> (800,48)   grab MS1-local (100,12)
#   resize MS2 right edge (60,300 320x380): drag 377->470 => 410x380
#   resize MS2 right edge clamp: drag 467->100 => clamp 64x380
#   resize MS2 right edge back:  drag 121->380 => 320x380
#   resize MS2 BR corner:        drag (377,677)->(520,710) => 460x410
#   resize MS2 left edge:        drag 63->140 => 380x410 @ (140,300)
#   close MS2: close-btn center local (W-12,12) => logical (508,312)
#   spawn MS3 (acceptor re-arm check) => expected @ (520,210)
$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Win {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
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

function Get-ServerHwnd {
    for ($i = 0; $i -lt 50; ++$i) {
        $h = (Get-Process jkdesktop -ErrorAction SilentlyContinue |
            Where-Object { $_.MainWindowHandle -ne 0 } |
            Select-Object -First 1).MainWindowHandle
        if ($h -ne 0) { return [IntPtr]$h }
        Start-Sleep -Milliseconds 200
    }
    return [IntPtr]::Zero
}

$h = Get-ServerHwnd
if ($h -eq [IntPtr]::Zero) { Write-Host "FATAL: server window not found"; exit 1 }

$crect = New-Object Win+RECT
[Win]::GetClientRect($h, [ref]$crect) | Out-Null
$co = New-Object Win+POINT
$co.X = 0; $co.Y = 0
[Win]::ClientToScreen($h, [ref]$co) | Out-Null

$clientW = $crect.R - $crect.L
$scale = $clientW / 1280.0
Write-Host ("server clientW={0} scale={1:F3}" -f $clientW, $scale)

function Pt([int]$lx, [int]$ly) {
    return @([int]([math]::Round($co.X + $lx * $scale)),
             [int]([math]::Round($co.Y + $ly * $scale)))
}

function MoveCursor([int]$lx, [int]$ly) {
    $p = Pt $lx $ly
    [Win]::SetCursorPos($p[0], $p[1]) | Out-Null
    Start-Sleep -Milliseconds 40
}

function Click([int]$lx, [int]$ly) {
    MoveCursor $lx $ly
    Start-Sleep -Milliseconds 120
    [Win]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)   # LEFTDOWN
    Start-Sleep -Milliseconds 80
    [Win]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)   # LEFTUP
    Start-Sleep -Milliseconds 120
}

function Drag([int]$sx, [int]$sy, [int]$ex, [int]$ey, [string]$tag) {
    Write-Host ("drag {0}: ({1},{2}) -> ({3},{4})" -f $tag, $sx, $sy, $ex, $ey)
    MoveCursor $sx $sy
    Start-Sleep -Milliseconds 150
    [Win]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)   # LEFTDOWN
    Start-Sleep -Milliseconds 120
    $steps = 8
    for ($i = 1; $i -le $steps; ++$i) {
        $mx = $sx + [int]([math]::Round(($ex - $sx) * $i / $steps))
        $my = $sy + [int]([math]::Round(($ey - $sy) * $i / $steps))
        MoveCursor $mx $my
    }
    Start-Sleep -Milliseconds 150
    [Win]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)   # LEFTUP
    Start-Sleep -Milliseconds 300
}

Write-Host "=== spawn MS1 ==="
Click 82 90
Start-Sleep -Seconds 3
Write-Host "=== spawn TET ==="
Click 182 90
Start-Sleep -Seconds 3
Write-Host "=== spawn MS2 ==="
Click 82 90
Start-Sleep -Seconds 3

Write-Host "=== move TET (950,88) ==="
Drag 600 132 1050 100 "TET-move"
Write-Host "=== move MS2 (60,300) ==="
Drag 670 222 210 312 "MS2-move"
Write-Host "=== move MS1 (800,48) ==="
Drag 580 182 900 60 "MS1-move"

Write-Host "=== resize MS2 widen 410x380 ==="
Drag 377 500 470 500 "MS2-widen"
Write-Host "=== resize MS2 clamp 64x380 ==="
Drag 467 500 100 500 "MS2-clamp"
Write-Host "=== resize MS2 back 320x380 ==="
Drag 121 500 380 500 "MS2-restore"
Write-Host "=== resize MS2 BR corner 460x410 ==="
Drag 377 677 520 710 "MS2-BR"
Write-Host "=== resize MS2 left edge 380x410 @(140,300) ==="
Drag 63 500 140 500 "MS2-leftedge"

Write-Host "=== close MS2 ==="
Click 508 312
Start-Sleep -Seconds 2

Write-Host "=== spawn MS3 (acceptor re-arm) ==="
Click 82 90
Start-Sleep -Seconds 3

Write-Host "DONE"