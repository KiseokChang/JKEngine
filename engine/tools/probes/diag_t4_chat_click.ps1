$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class W6 {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
[W6]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
$h = (Get-Process jkdesktop | Where-Object { $_.ProcessName -eq 'jkdesktop' -and $_.MainWindowTitle -like '*Window Server*' } |
    Select-Object -First 1).MainWindowHandle
$h = [IntPtr]$h
$cr = New-Object W6+RECT
[W6]::GetClientRect($h, [ref]$cr) | Out-Null
$co = New-Object W6+POINT; $co.X = 0; $co.Y = 0
[W6]::ClientToScreen($h, [ref]$co) | Out-Null
$scale = ($cr.R - $cr.L) / 1280.0
Write-Host ("client w={0} scale={1:F4}" -f ($cr.R - $cr.L), $scale)

# [구두] button estimate — chat window logical (300,90) 720x540 on the server
# canvas; button = chat-local (688, 490) → server-logical (988, 580).
$lx = 988.0 * $scale
$ly = 580.0 * $scale
$px = [int][math]::Round($co.X + $lx)
$py = [int][math]::Round($co.Y + $ly)
Write-Host ("click physical ({0},{1})" -f $px, $py)
[W6]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 400
[W6]::SetCursorPos($px, $py) | Out-Null
Start-Sleep -Milliseconds 200
[W6]::mouse_event(2,0,0,0,[UIntPtr]::Zero)
Start-Sleep -Milliseconds 80
[W6]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
Start-Sleep -Milliseconds 1500   # during capture — first shot
$r = New-Object W6+RECT
[W6]::GetWindowRect($h, [ref]$r) | Out-Null
function Snap($name) {
    $rr = New-Object W6+RECT
    [W6]::GetWindowRect($h, [ref]$rr) | Out-Null
    $ww = $rr.R - $rr.L; $whh = $rr.B - $rr.T
    $bmp = New-Object System.Drawing.Bitmap $ww, $whh
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($rr.L, $rr.T, 0, 0, $bmp.Size)
    ("I:\progwork\JKENGINE\engine\tools\probes\" + $name) | ForEach-Object { $bmp.Save($_) }
    Write-Host ("saved {0} ({1}x{2})" -f $name, $ww, $whh)
}
Snap "diag_t4_chat_click_rec.png"
# click again = 마침 (stop → transcribe)
[W6]::mouse_event(2,0,0,0,[UIntPtr]::Zero)
Start-Sleep -Milliseconds 80
[W6]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
Write-Host "clicked again = stop"
Start-Sleep -Milliseconds 1200
Snap "diag_t4_chat_click_transcribing.png"
Start-Sleep -Seconds 16
Snap "diag_t4_chat_click_done.png"