# Refocus-on-disconnect test: 2 tetris clients. #2 spawns last (has focus).
# Kill #2 (focused). Server should refocus topmost survivor (#1). Arrows then
# must move #1's piece (no clicks anywhere).
$ErrorActionPreference = 'Continue'
$exe = "I:\progwork\JKENGINE\prototype\sdl2_jkwindow\build\jkdesktop.exe"
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W3 {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern void keybd_event(byte k, byte s, uint f, UIntPtr e);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
"@
[W3]::SetProcessDPIAware() | Out-Null

# Spawn client #2.
$c2 = Start-Process -FilePath $exe -ArgumentList "--client","tetris" `
    -WorkingDirectory "I:\progwork\JKENGINE" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 4

# Kill #2 (it holds focus per focus-on-spawn).
taskkill /F /PID $c2.Id 2>&1 | Out-Null
Start-Sleep -Seconds 2

# Foreground server, arrows, compare left-pin vs right-pin of SURVIVOR (#1).
$srv = Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $srv) { Write-Host "FAIL: no server window"; exit 1 }
[W3]::SetForegroundWindow($srv.MainWindowHandle) | Out-Null
Start-Sleep -Milliseconds 500

function SendKeys($vk, $n) {
    for ($i=0; $i -lt $n; $i++) {
        [W3]::keybd_event($vk, 0, 0, [UIntPtr]::Zero)
        [W3]::keybd_event($vk, 0, 2, [UIntPtr]::Zero)
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
SendKeys 0x25 40
Shot "I:\progwork\JKENGINE\tmp\refocus_left.png"
SendKeys 0x27 40
Shot "I:\progwork\JKENGINE\tmp\refocus_right.png"

$imgA = [System.Drawing.Bitmap]::FromFile("I:\progwork\JKENGINE\tmp\refocus_left.png")
$imgB = [System.Drawing.Bitmap]::FromFile("I:\progwork\JKENGINE\tmp\refocus_right.png")
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
    Write-Host "changed px: $cnt  range: [$minX, $maxX]"
    if (($maxX - $minX) -gt 150) { Write-Host "PASS: survivor received arrows after focused client was killed" }
    else { Write-Host "FAIL: no horizontal traverse (span $($maxX - $minX)px)" }
} else { Write-Host "FAIL: zero change" }