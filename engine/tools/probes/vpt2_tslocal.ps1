$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public class T3 {
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
$build = "I:\progwork\JKENGINE\engine\build"
$exe = "$build\jkdesktop.exe"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-14-vplayer-stability"
function Actl([string]$j) {
    $escaped = $j.Replace('"', [string][char]92 + '"')
    (& $exe agentctl $escaped 2>&1) -join "`n"
}
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build
Start-Sleep -Seconds 4
Actl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
Start-Sleep -Seconds 6
$h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1).MainWindowHandle
[T3]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 300
# click path input (layer 400,42 -> client offset measured: layer origin + (78,28))
$crect = New-Object T3+RECT
[T3]::GetClientRect($h, [ref]$crect) | Out-Null
$co = New-Object T3+POINT; $co.X = 0; $co.Y = 0
[T3]::ClientToScreen($h, [ref]$co) | Out-Null
$lx = 180; $ly = 40   # list_windows layer origin (client coords)
function Click([int]$cx, [int]$cy) {
    [T3]::SetCursorPos($co.X + $lx + 78 + $cx, $co.Y + $ly + 28 + $cy) | Out-Null
    Start-Sleep -Milliseconds 250
    [T3]::mouse_event(2,0,0,0,[UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [T3]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
    Start-Sleep -Milliseconds 300
}
Click 330 14
Start-Sleep -Milliseconds 300
Set-Clipboard -Value "I:/progwork/JKENGINE/tmp/vpt2_test.ts"
Start-Sleep -Milliseconds 200
[System.Windows.Forms.SendKeys]::SendWait("{END}")
Start-Sleep -Milliseconds 100
[System.Windows.Forms.SendKeys]::SendWait("{BS 300}")
Start-Sleep -Milliseconds 150
[System.Windows.Forms.SendKeys]::SendWait("^v")
Start-Sleep -Milliseconds 400
Click 730 14   # Open
Start-Sleep -Seconds 6
$bmp = New-Object System.Drawing.Bitmap ($crect.R-$crect.L), ($crect.B-$crect.T)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size)
$g.Dispose()
$bmp.Save("$shotDir\vpt2-ts-local.png")
$bmp.Dispose()
Write-Host "saved"