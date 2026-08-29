# app_mouse_test3.ps1 - drag test; app via launch_app_traced.cmd (stderr trace)
$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W6 {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L; public int T; public int R; public int B; }
  [StructLayout(LayoutKind.Sequential)] public struct MONITORINFO { public int cbSize; public RECT rcMonitor; public RECT rcWork; public uint dwFlags; }
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr value);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern IntPtr MonitorFromWindow(IntPtr h, uint flags);
  [DllImport("user32.dll")] public static extern bool GetMonitorInfo(IntPtr h, ref MONITORINFO mi);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint flags, int dx, int dy, uint data, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
}
"@
# PMv2 인식 실패(구형 OS)면 시스템 DPI 인식으로 폴백
if (-not [W6]::SetProcessDpiAwarenessContext([IntPtr](-4))) {
  [W6]::SetProcessDPIAware() | Out-Null
}
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$wdir = "i:\progwork\JKENGINE\prototype\sdl2_jkwindow\build"

Get-Process -Name "jkproto_sdl2_jkwindow" -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 300

$trace = "C:\temp_jkwin_verify\dpi_trace.log"
if (Test-Path $trace) { Remove-Item $trace -Force }

Start-Process -FilePath "cmd.exe" -ArgumentList "/c", "C:\temp_jkwin_verify\launch_app_traced.cmd" -WorkingDirectory $wdir -WindowStyle Hidden

$h = [IntPtr]::Zero
for ($i = 0; $i -lt 40; $i++) {
  Start-Sleep -Milliseconds 250
  $pl = @(Get-Process -Name "jkproto_sdl2_jkwindow" -ErrorAction SilentlyContinue)
  if ($pl.Count -gt 0 -and $pl[0].MainWindowHandle -ne [IntPtr]::Zero) { $h = $pl[0].MainWindowHandle; break }
}
if ($h -eq [IntPtr]::Zero) { Write-Output "FAIL app window not found"; Get-Process -Name "jkproto_sdl2_jkwindow" -ErrorAction SilentlyContinue | Stop-Process -Force; exit 1 }
Start-Sleep -Milliseconds 1500

function Shot-Window {
  param([string]$Tag, [IntPtr]$HH)
  $wr = New-Object 'W6+RECT'
  [W6]::GetWindowRect($HH, [ref]$wr) | Out-Null
  $bw = $wr.R - $wr.L; $bh = $wr.B - $wr.T
  if ($bw -le 0 -or $bh -le 0) { return }
  $bmp = New-Object System.Drawing.Bitmap($bw, $bh)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($wr.L, $wr.T, 0, 0, (New-Object System.Drawing.Size($bw, $bh)))
  $g.Dispose()
  $png = "C:\temp_jkwin_verify\shot2_{0}.png" -f $Tag
  $bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  Write-Output ("SHOT {0} file={1} winRect=({2},{3},{4}x{5})" -f $Tag, $png, $wr.L, $wr.T, $bw, $bh)
}

function Shot-Region {
  param([string]$Tag, [int]$X, [int]$Y, [int]$W, [int]$H)
  if ($W -le 0 -or $H -le 0) { return }
  $bmp = New-Object System.Drawing.Bitmap($W, $H)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($X, $Y, 0, 0, (New-Object System.Drawing.Size($W, $H)))
  $g.Dispose()
  $png = "C:\temp_jkwin_verify\shot2_{0}.png" -f $Tag
  $bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  Write-Output ("SHOT {0} file={1} rect=({2},{3},{4}x{5})" -f $Tag, $png, $X, $Y, $W, $H)
}

function Dump-Monitor {
  param([IntPtr]$HH, [string]$Tag)
  $mh = [W6]::MonitorFromWindow($HH, 2)
  $mi = New-Object 'W6+MONITORINFO'
  $mi.cbSize = [System.Runtime.InteropServices.Marshal]::SizeOf([type]'W6+MONITORINFO')
  [W6]::GetMonitorInfo($mh, [ref]$mi) | Out-Null
  # Write-Output은 $var = Dump-Monitor 형태로 호출하면 변수에 흡수되어 콘솔에 안 찍힌다 — Write-Host 필수
  Write-Host ("MONITOR[{0}] rcMon=({1},{2})-({3},{4}) rcWork=({5},{6})-({7},{8})" -f `
    $Tag, $mi.rcMonitor.L, $mi.rcMonitor.T, $mi.rcMonitor.R, $mi.rcMonitor.B, `
    $mi.rcWork.L, $mi.rcWork.T, $mi.rcWork.R, $mi.rcWork.B)
  return $mi
}

function Drag-WindowTo {
  param([IntPtr]$HH, [int]$TargetX, [int]$TargetY)
  $wr = New-Object 'W6+RECT'
  [W6]::GetWindowRect($HH, [ref]$wr) | Out-Null
  $grabX = [int](($wr.L + $wr.R) / 2)
  $grabY = $wr.T + 15
  [W6]::SetForegroundWindow($HH) | Out-Null
  Start-Sleep -Milliseconds 250
  [W6]::SetCursorPos($grabX, $grabY) | Out-Null
  Start-Sleep -Milliseconds 300
  [W6]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero) | Out-Null
  Start-Sleep -Milliseconds 300
  for ($i = 1; $i -le 50; $i++) {
    $x = [int]($grabX + ($TargetX - $grabX) * $i / 50)
    $y = [int]($grabY + ($TargetY - $grabY) * $i / 50)
    [W6]::SetCursorPos($x, $y) | Out-Null
    Start-Sleep -Milliseconds 40
  }
  [W6]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero) | Out-Null
  Start-Sleep -Milliseconds 3000
}

[W6]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 800
$wr0 = New-Object 'W6+RECT'
[W6]::GetWindowRect($h, [ref]$wr0) | Out-Null
[W6]::SetCursorPos(($wr0.R + 60), ($wr0.T - 60)) | Out-Null
Start-Sleep -Milliseconds 800
$miP = Dump-Monitor $h "primary"
Shot-Window "primary" $h

Drag-WindowTo $h 3200 300
$mi2 = Dump-Monitor $h "display2"
Shot-Window "display2" $h
Start-Sleep -Milliseconds 1500
Shot-Window "display2b" $h
Shot-Region "display2_full" $mi2.rcMonitor.L $mi2.rcMonitor.T ($mi2.rcMonitor.R - $mi2.rcMonitor.L) ($mi2.rcMonitor.B - $mi2.rcMonitor.T)

Drag-WindowTo $h 700 300
$mi3 = Dump-Monitor $h "back_primary"
Shot-Window "back_primary" $h
Start-Sleep -Milliseconds 1500
Shot-Window "back_primary_b" $h

Get-Process -Name "jkproto_sdl2_jkwindow" -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Output "DONE"