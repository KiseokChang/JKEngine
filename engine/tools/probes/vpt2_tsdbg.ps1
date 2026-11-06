# Focused S3 diagnostics: mpeg-ts over a named pipe (every avformat_seek_file
# fails: measured -1 EPERM), vplayer stderr captured for the [vplayer] lines.
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public class T4 {
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
$errLog = "I:\progwork\JKENGINE\tmp\vpt2_client_stderr.log"
function Actl([string]$j) {
    $escaped = $j.Replace('"', [string][char]92 + '"')
    (& $exe agentctl $escaped 2>&1) -join "`n"
}
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
$srv = Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build -RedirectStandardError $errLog -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 4
$holder = Start-Process powershell -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File','I:\progwork\JKENGINE\tmp\vpt2_serve.ps1','-Lifetime','120','-Name','vpt2_serve' -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 2
Actl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
Start-Sleep -Seconds 6
$h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1).MainWindowHandle
[T3]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 300
$crect = New-Object T3+RECT
[T3]::GetClientRect($h, [ref]$crect) | Out-Null
$co = New-Object T3+POINT; $co.X = 0; $co.Y = 0
[T3]::ClientToScreen($h, [ref]$co) | Out-Null
# vplayer layer at client (180,40); surface origin +78,+28 (measured)
$lx = 180; $ly = 40
function Click([int]$cx, [int]$cy) {
    [T3]::SetCursorPos($co.X + $lx + $cx, $co.Y + $ly + $cy) | Out-Null
    Start-Sleep -Milliseconds 250
    [T3]::mouse_event(2,0,0,0,[UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [T3]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
    Start-Sleep -Milliseconds 300
}
function Shot([string]$name) {
    $bmp = New-Object System.Drawing.Bitmap ($crect.R-$crect.L), ($crect.B-$crect.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size)
    $g.Dispose()
    $bmp.Save("$shotDir\$name")
    $bmp.Dispose()
    Write-Host "shot $name"
}
Click 478 42   # path input (layer 478,42)
Start-Sleep -Milliseconds 300
Set-Clipboard -Value "//./pipe/vpt2_serve"
Start-Sleep -Milliseconds 200
[System.Windows.Forms.SendKeys]::SendWait("{END}")
Start-Sleep -Milliseconds 100
[System.Windows.Forms.SendKeys]::SendWait("{BS 300}")
Start-Sleep -Milliseconds 150
[System.Windows.Forms.SendKeys]::SendWait("^v")
Start-Sleep -Milliseconds 400
Click 886 42   # Open
Start-Sleep -Seconds 6
Shot "vpt2-tsdbg-a-6s.png"
Click 210 77   # seek: click near slider start (backward)
Start-Sleep -Milliseconds 900
Shot "vpt2-tsdbg-b-after-fail.png"
Start-Sleep -Seconds 3
Shot "vpt2-tsdbg-c-3s-later.png"
Stop-Process -Id $holder.Id -Force -ErrorAction SilentlyContinue
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Write-Host "=== client stderr ==="
Get-Content $errLog