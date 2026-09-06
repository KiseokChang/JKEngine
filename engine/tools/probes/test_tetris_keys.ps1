# Tetris key verification: focus the tetris surface, send arrow keys,
# screenshot before/after for visual comparison.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class W3 {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
[W3]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null

$h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
    Select-Object -First 1).MainWindowHandle
$h = [IntPtr]$h
[W3]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 300

$crect = New-Object W3+RECT
[W3]::GetClientRect($h, [ref]$crect) | Out-Null
$co = New-Object W3+POINT; $co.X = 0; $co.Y = 0
[W3]::ClientToScreen($h, [ref]$co) | Out-Null
$scale = ($crect.R - $crect.L) / 1280.0

# Tetris surface 320x520, first client centered: logical (480,100) size 320x520.
# Click its center to focus: logical (640, 360)
$cx = [int]([math]::Round($co.X + 640 * $scale))
$cy = [int]([math]::Round($co.Y + 360 * $scale))
[W3]::SetCursorPos($cx, $cy) | Out-Null
Start-Sleep -Milliseconds 150
[W3]::mouse_event(2,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 60
[W3]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
Start-Sleep -Milliseconds 300
Write-Host "focused tetris surface at logical (640,360)"

function Shot($path) {
    $w = $crect.R - $crect.L; $ht = $crect.B - $crect.T
    $bmp = New-Object System.Drawing.Bitmap($w, $ht)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size)
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
}

$out = "I:\progwork\JKENGINE\tmp"
Shot "$out\tetris_before.png"

# Send LEFT arrow x5 (VK_LEFT=0x25), then rotate (VK_UP=0x26)
for ($i = 0; $i -lt 5; $i++) {
    [W3]::keybd_event(0x25, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 40
    [W3]::keybd_event(0x25, 0, 2, [UIntPtr]::Zero)  # KEYEVENTF_KEYUP
    Start-Sleep -Milliseconds 120
}
Start-Sleep -Milliseconds 400
Shot "$out\tetris_after.png"
Write-Host "sent 5x LEFT; screenshots saved"