$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class W7 {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
    public struct RECT { public int L, T, R, B; }
}
"@
[W7]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
$h = (Get-Process jkdesktop | Where-Object { $_.ProcessName -eq 'jkdesktop' -and $_.MainWindowTitle -like '*Window Server*' } |
    Select-Object -First 1).MainWindowHandle
$h = [IntPtr]$h
$r = New-Object W7+RECT
[W7]::GetWindowRect($h, [ref]$r) | Out-Null
Write-Host ("win rect ({0},{1},{2},{3})" -f $r.L, $r.T, $r.R, $r.B)
function Snap($name) {
    $rr = New-Object W7+RECT
    [W7]::GetWindowRect($h, [ref]$rr) | Out-Null
    $ww = $rr.R - $rr.L; $whh = $rr.B - $rr.T
    $bmp = New-Object System.Drawing.Bitmap $ww, $whh
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($rr.L, $rr.T, 0, 0, $bmp.Size)
    ("I:\progwork\JKENGINE\engine\tools\probes\" + $name) | ForEach-Object { $bmp.Save($_) }
    Write-Host ("saved {0}" -f $name)
}
$px = $r.L + 1213
$py = $r.T + 760
[W7]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 400
[W7]::SetCursorPos($px, $py) | Out-Null
Start-Sleep -Milliseconds 200
[W7]::mouse_event(2,0,0,0,[UIntPtr]::Zero)
Start-Sleep -Milliseconds 80
[W7]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
Write-Host ("clicked at ({0},{1}) — capture start expected" -f $px, $py)
Start-Sleep -Milliseconds 1800
Snap "diag_t4_chat_click2_rec.png"
[W7]::SetCursorPos($px, $py) | Out-Null
[W7]::mouse_event(2,0,0,0,[UIntPtr]::Zero)
Start-Sleep -Milliseconds 80
[W7]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
Write-Host "clicked again = stop -> transcribe"
Start-Sleep -Milliseconds 1200
Snap "diag_t4_chat_click2_transcribing.png"
Start-Sleep -Seconds 16
Snap "diag_t4_chat_click2_done.png"