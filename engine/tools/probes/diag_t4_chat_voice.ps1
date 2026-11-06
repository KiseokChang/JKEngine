$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class W5 {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    public struct RECT { public int L, T, R, B; }
}
"@
[W5]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
$h = (Get-Process jkdesktop | Where-Object { $_.ProcessName -eq 'jkdesktop' -and $_.MainWindowTitle -like '*Window Server*' } |
    Select-Object -First 1).MainWindowHandle
$h = [IntPtr]$h
[W5]::ShowWindow($h, 9) | Out-Null     # SW_RESTORE
[W5]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 600

# capture start: F6 down (push-to-talk) — hold $holdMs then release
$holdMs = 3500
[W5]::keybd_event(0x75, 0, 0, [UIntPtr]::Zero)        # F6 down
Start-Sleep -Milliseconds $holdMs                      # hold = recording
[W5]::keybd_event(0x75, 0, 2, [UIntPtr]::Zero)        # F6 up (= stop → transcribe)
Write-Host "F6 pushed (hold ${holdMs}ms) — transcribing"

Start-Sleep -Seconds 16
$r = New-Object W5+RECT
[W5]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.R - $r.L
$hh = $r.B - $r.T
$bmp = New-Object System.Drawing.Bitmap $w, $hh
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
$bmp.Save("I:\progwork\JKENGINE\engine\tools\probes\diag_t4_chat_voice.png")
Write-Host ("saved {0}x{1} at ({2},{3})" -f $w, $hh, $r.L, $r.T)