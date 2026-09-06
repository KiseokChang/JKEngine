# Directional key test: tetris client still running from previous test.
# LEFT x40 -> shot A (piece pinned at left wall), RIGHT x40 -> shot B (pinned right).
# If arrows are being routed, the changed-pixel centroid must move RIGHT.
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W2 {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern void keybd_event(byte k, byte s, uint f, UIntPtr e);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
"@
[W2]::SetProcessDPIAware() | Out-Null

$srv = Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $srv) { Write-Host "FAIL: no server window"; exit 1 }
[W2]::SetForegroundWindow($srv.MainWindowHandle) | Out-Null
Start-Sleep -Milliseconds 500

function SendKeys($vk, $n) {
    for ($i=0; $i -lt $n; $i++) {
        [W2]::keybd_event($vk, 0, 0, [UIntPtr]::Zero)
        [W2]::keybd_event($vk, 0, 2, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 60
    }
}
function Shot($path) {
    Start-Sleep -Milliseconds 400
    $b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen(0,0,0,0,$bmp.Size)
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
}

SendKeys 0x25 40   # LEFT x40 -> left wall
Shot "I:\progwork\JKENGINE\tmp\dir_left.png"
SendKeys 0x27 40   # RIGHT x40 -> right wall
Shot "I:\progwork\JKENGINE\tmp\dir_right.png"

$imgA = [System.Drawing.Bitmap]::FromFile("I:\progwork\JKENGINE\tmp\dir_left.png")
$imgB = [System.Drawing.Bitmap]::FromFile("I:\progwork\JKENGINE\tmp\dir_right.png")
$sumX = 0.0; $cnt = 0; $minX = -1; $maxX = -1
for ($y = 0; $y -lt $imgA.Height; $y += 2) {
    for ($x = 0; $x -lt $imgA.Width; $x += 2) {
        $p1 = $imgA.GetPixel($x,$y); $p2 = $imgB.GetPixel($x,$y)
        if ([Math]::Abs($p1.R-$p2.R) + [Math]::Abs($p1.G-$p2.G) + [Math]::Abs($p1.B-$p2.B) -gt 30) {
            $sumX += $x; $cnt++
            if ($minX -lt 0 -or $x -lt $minX) { $minX = $x }
            if ($maxX -lt 0 -or $x -gt $maxX) { $maxX = $x }
        }
    }
}
$imgA.Dispose(); $imgB.Dispose()
if ($cnt -gt 0) {
    $cx = [Math]::Round($sumX / $cnt)
    Write-Host "changed px: $cnt  centroid-x: $cx  range: [$minX, $maxX]"
    if ($cnt -gt 100) { Write-Host "PASS: piece moved horizontally => arrows routed without any surface click" }
    else { Write-Host "FAIL: too few changed pixels" }
} else { Write-Host "FAIL: zero change" }