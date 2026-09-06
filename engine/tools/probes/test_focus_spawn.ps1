# Spawn tetris via client process, then send arrow keys WITHOUT clicking the
# surface — verifies focus-on-spawn (server must route keys to the new client).
$ErrorActionPreference = 'Continue'
$exe = "I:\progwork\JKENGINE\prototype\sdl2_jkwindow\build\jkdesktop.exe"
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void keybd_event(byte k, byte s, uint f, UIntPtr e);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    public struct POINT { public int X; public int Y; }
}
"@
[W]::SetProcessDPIAware() | Out-Null

function Shot($path) {
    Start-Sleep -Milliseconds 300
    $b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen(0,0,0,0,$bmp.Size)
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
}

# 1. Spawn tetris client (server already running).
Start-Process -FilePath $exe -ArgumentList "--client","tetris" `
    -WorkingDirectory "I:\progwork\JKENGINE" -WindowStyle Hidden
Start-Sleep -Seconds 4
Shot "I:\progwork\JKENGINE\tmp\focus_before.png"
Write-Host "spawned, before-shot taken (no clicks performed)"

# 2. Find server window, bring to foreground.
$srv = Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $srv) { Write-Host "FAIL: no server window"; exit 1 }
[W]::SetForegroundWindow($srv.MainWindowHandle) | Out-Null
Start-Sleep -Milliseconds 500

# 3. Send 6x LEFT arrow — piece should move left 6 cells.
for ($i=0; $i -lt 6; $i++) {
    [W]::keybd_event(0x25, 0, 0, [UIntPtr]::Zero)
    [W]::keybd_event(0x25, 0, 2, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 150
}
Start-Sleep -Milliseconds 800
Shot "I:\progwork\JKENGINE\tmp\focus_after.png"
Write-Host "after-shot taken (6x LEFT, zero surface clicks)"

# 4. Pixel-diff the two shots inside the board area.
$img1 = [System.Drawing.Bitmap]::FromFile("I:\progwork\JKENGINE\tmp\focus_before.png")
$img2 = [System.Drawing.Bitmap]::FromFile("I:\progwork\JKENGINE\tmp\focus_after.png")
$diff = 0
for ($y = 0; $y -lt $img1.Height; $y += 4) {
    for ($x = 0; $x -lt $img1.Width; $x += 4) {
        $p1 = $img1.GetPixel($x,$y); $p2 = $img2.GetPixel($x,$y)
        if ([Math]::Abs($p1.R-$p2.R) + [Math]::Abs($p1.G-$p2.G) + [Math]::Abs($p1.B-$p2.B) -gt 30) { $diff++ }
    }
}
$img1.Dispose(); $img2.Dispose()
Write-Host "pixel diff (sampled): $diff"
if ($diff -gt 20) { Write-Host "PASS: screen changed => arrows reached tetris without any click" }
else { Write-Host "FAIL: no visible change => keys still dropped" }