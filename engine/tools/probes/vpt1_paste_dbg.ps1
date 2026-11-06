. { } # debug: clipboard paste into vplayer path field
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class DpiD { [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx); }
"@ 
[DpiD]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Set-Clipboard -Value "I:/progwork/JKENGINE/tmp/vpt1_faststart.mp4"
Write-Host ("clipboard now: " + (Get-Clipboard))
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory "I:\progwork\JKENGINE\engine\build"
Start-Sleep -Seconds 4
& $exe agentctl '{\"tool\":\"launch_app\",\"args\":{\"app\":\"vplayer\"}}' | Out-Null
Start-Sleep -Seconds 6
$h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1).MainWindowHandle
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class WtD {
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
$crect = New-Object WtD+RECT
[WtD]::GetClientRect($h, [ref]$crect) | Out-Null
$co = New-Object WtD+POINT; $co.X=0; $co.Y=0
[WtD]::ClientToScreen($h, [ref]$co) | Out-Null
$scale = ($crect.R - $crect.L) / 1280.0
# click path input: layer 180,40 + (400,42) app-local
$px = [int]($co.X + (180 + 400) * $scale); $py = [int]($co.Y + (40 + 42) * $scale)
[WtD]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 300
[WtD]::SetCursorPos($px, $py) | Out-Null
Start-Sleep -Milliseconds 250
[WtD]::mouse_event(2,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 80; [WtD]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
Start-Sleep -Milliseconds 400
[System.Windows.Forms.SendKeys]::SendWait("^a")
Start-Sleep -Milliseconds 150
[System.Windows.Forms.SendKeys]::SendWait("^v")
Start-Sleep -Milliseconds 500
# click Open
$px2 = [int]($co.X + (180 + 808) * $scale); $py2 = [int]($co.Y + (40 + 42) * $scale)
[WtD]::SetCursorPos($px2, $py2) | Out-Null
Start-Sleep -Milliseconds 250
[WtD]::mouse_event(2,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 80; [WtD]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
Start-Sleep -Seconds 4
$bmp = New-Object System.Drawing.Bitmap ($crect.R - $crect.L), ($crect.B - $crect.T)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size); $g.Dispose()
$rect = New-Object System.Drawing.Rectangle ([int](180*$scale)), ([int](40*$scale)), ([int](960*$scale)), ([int](640*$scale))
$crop = $bmp.Clone($rect, $bmp.PixelFormat)
$crop.Save("I:\progwork\JKENGINE\.superpowers\sdd\2026-09-14-vplayer-stability\vpt1-paste-dbg.png")
Write-Host "saved vpt1-paste-dbg.png"
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
