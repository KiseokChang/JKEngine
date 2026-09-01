param(
    [string]$exe = "i:\progwork\JKENGINE\prototype\sdl2_jkwindow\build\jkproto_sdl2_jkwindow.exe",
    [string]$outPath = "i:\progwork\JKENGINE\tools\tempp\launcher_click_mine.png"
)

$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class DpiUtil {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, IntPtr info);
}
public class Win32Geom {
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L; public int T; public int R; public int B; }
}
'@
[DpiUtil]::SetProcessDPIAware() | Out-Null

Add-Type -AssemblyName System.Drawing

if (-not (Test-Path $exe)) { throw "exe not found: $exe" }

$proc = Start-Process -FilePath $exe -ArgumentList "minesweeper" -PassThru
try {
    Start-Sleep -Seconds 4
    $proc.Refresh()
    if ($proc.HasExited) { throw "app exited early (code $($proc.ExitCode))" }
    $hwnd = $proc.MainWindowHandle
    if ($hwnd -eq [IntPtr]::Zero) { throw "MainWindowHandle not found" }
    [DpiUtil]::SetForegroundWindow($hwnd) | Out-Null
    Start-Sleep -Milliseconds 500

    $wr = New-Object 'Win32Geom+RECT'
    [Win32Geom]::GetWindowRect($hwnd, [ref]$wr) | Out-Null

    # Click inside the Minesweeper launcher icon (first icon, roughly centered).
    $clickX = $wr.L + 100
    $clickY = $wr.T + 125
    [DpiUtil]::SetCursorPos($clickX, $clickY) | Out-Null
    Start-Sleep -Milliseconds 100
    [DpiUtil]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero) # LBUTTONDOWN
    Start-Sleep -Milliseconds 50
    [DpiUtil]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero) # LBUTTONUP

    # Wait for the floating Mine window to open.
    Start-Sleep -Seconds 2

    $proc.Refresh()
    if ($proc.HasExited) { throw "app exited after click (code $($proc.ExitCode))" }
    [Win32Geom]::GetWindowRect($hwnd, [ref]$wr) | Out-Null
    $w = $wr.R - $wr.L
    $h = $wr.B - $wr.T

    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($wr.L, $wr.T, 0, 0, $bmp.Size)
    $g.Dispose()

    $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host ("saved capture to {0} ({1}x{2})" -f $outPath, $w, $h)
} finally {
    if (-not $proc.HasExited) { $proc.Kill() | Out-Null }
}
