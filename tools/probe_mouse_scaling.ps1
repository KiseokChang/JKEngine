# probe_mouse_scaling.ps1 - 마우스/DPI 배율 실측 도구 (PMv2 드라이버)
#
# 목적: jango 앱을 띄우고 (옵션: 보조 모니터로 창 이동) 커서를 창 위 그리드로
# 스윕시켜 SDL 좌표 변환 사슬(SDL pt -> 물리 px -> 앱 논리)이 어긋나는지
# [DPISYNC] 로그로 관찰한다. 14_sdl2_window_dpi.md / 15_verification_playbook.md 참조.
#
# 사용: powershell -NoProfile -ExecutionPolicy Bypass -File tools\probe_mouse_scaling.ps1 [-Phase stay|move2nd|both]
#   -Phase stay    : 현재(주) 모니터에서만 그리드 스윕
#   -Phase move2nd : 보조 모니터로 창 이동 후 그리드 스윕
#   -Phase both    : 이동 전/후 그리드 각각 (기본)
param(
    [string]$Phase = "both"
)
$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class Wk {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L; public int T; public int R; public int B; }
  [StructLayout(LayoutKind.Sequential)] public struct MONITORINFO { public int cbSize; public RECT rcMonitor; public RECT rcWork; public uint dwFlags; }
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder sb, int max);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern IntPtr MonitorFromWindow(IntPtr h, uint f);
  [DllImport("user32.dll")] public static extern bool GetMonitorInfo(IntPtr h, ref MONITORINFO mi);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int w, int hh, uint flags);
  public static uint TargetPid;
  public static IntPtr Found;
  public static bool Callback(IntPtr h, IntPtr l) {
    uint pid; GetWindowThreadProcessId(h, out pid);
    if (pid == TargetPid) {
      StringBuilder cls = new StringBuilder(256);
      GetClassNameW(h, cls, 256);
      if (cls.ToString().Equals("SDL_app", StringComparison.OrdinalIgnoreCase)) { Found = h; return false; }
    }
    return true;
  }
}
"@
if (-not [Wk]::SetProcessDpiAwarenessContext([IntPtr](-4))) { [Wk]::SetProcessDPIAware() | Out-Null }

$root = "I:\progwork\JKENGINE\prototype\sdl2_jkwindow"
$build = Join-Path $root "build"
$exe = Join-Path $build "jkproto_sdl2_jkwindow.exe"
if (-not (Test-Path $exe)) { Write-Output "FAIL exe not found: $exe"; exit 1 }
$applog = Join-Path $env:TEMP "jk_probe_mouse_app.log"
if (Test-Path $applog) { Remove-Item $applog -Force }

Get-Process -Name "jkproto_sdl2_jkwindow" -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 400

# 앱 스폰(cwd=build, 숨김 콘솔). -WindowStyle Hidden은 첫 윈도우에 SW_HIDE를
# 적용할 수 있으므로 EnumWindows로 HWND를 찾아 ShowWindow로 복원한 뒤 스윕한다.
Start-Process -FilePath $exe -ArgumentList "jango" -WorkingDirectory $build -WindowStyle Hidden -RedirectStandardError $applog -RedirectStandardOutput (Join-Path $env:TEMP "jk_probe_mouse_out.log")

$h = [IntPtr]::Zero
for ($i = 0; $i -lt 40; $i++) {
  Start-Sleep -Milliseconds 250
  $pl = @(Get-Process -Name "jkproto_sdl2_jkwindow" -ErrorAction SilentlyContinue)
  if ($pl.Count -gt 0) {
    [Wk]::TargetPid = [uint32]($pl[0].Id)
    $cb = [Wk+EnumProc]{ param($hh, $l) [Wk]::Callback($hh, $l) }
    [Wk]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
    if ([Wk]::Found -ne [IntPtr]::Zero) { $h = [Wk]::Found; break }
  }
}
if ($h -eq [IntPtr]::Zero) { Write-Output "FAIL app window not found"; Get-Process -Name "jkproto_sdl2_jkwindow" -ErrorAction SilentlyContinue | Stop-Process -Force; exit 1 }
if (-not [Wk]::IsWindowVisible($h)) { [Wk]::ShowWindow($h, 5) | Out-Null; Start-Sleep -Milliseconds 800 }
[Wk]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 1800

function Grid-Sweep {
  param([string]$Tag, [IntPtr]$HH)
  $wr = New-Object 'Wk+RECT'
  [Wk]::GetWindowRect($HH, [ref]$wr) | Out-Null
  $w = $wr.R - $wr.L; $hhh = $wr.B - $wr.T
  Write-Output ("GRID {0} winRect=({1},{2}) {3}x{4}" -f $Tag, $wr.L, $wr.T, $w, $hhh)
  $xs = 0.15, 0.35, 0.50, 0.70, 0.92
  $ys = 0.20, 0.50, 0.85
  $i = 0
  foreach ($fy in $ys) {
    foreach ($fx in $xs) {
      $i++
      $x = [int]($wr.L + 25 + ($w - 60) * $fx)
      $y = [int]($wr.T + 55 + ($hhh - 80) * $fy)
      [Wk]::SetCursorPos($x, $y) | Out-Null
      Write-Output ("STEP {0} {1} px=({2},{3})" -f $Tag, $i, $x, $y)
      Start-Sleep -Milliseconds 450
    }
  }
}

$wr = New-Object 'Wk+RECT'
[Wk]::GetWindowRect($h, [ref]$wr) | Out-Null
$w0 = $wr.R - $wr.L; $h0 = $wr.B - $wr.T

function Move-And-Sweep {
  param([IntPtr]$HH)
  # SetWindowPos(PMv2 드라이버)로 창을 보조 모니터로 옮긴다. PMv1 시절과 달리
  # 혼합 배율 전환 중에도 창 크기가 보존되는지 MOVED 라인으로 바로 확인 가능.
  [Wk]::SetWindowPos($HH, [IntPtr]::Zero, 1962, 8, $script:w0, $script:h0, 0x0004) | Out-Null
  Start-Sleep -Milliseconds 2500
  $wr2 = New-Object 'Wk+RECT'
  [Wk]::GetWindowRect($HH, [ref]$wr2) | Out-Null
  $mi = New-Object 'Wk+MONITORINFO'
  $mi.cbSize = [System.Runtime.InteropServices.Marshal]::SizeOf([type]'Wk+MONITORINFO')
  $mh = [Wk]::MonitorFromWindow($HH, 2)
  [Wk]::GetMonitorInfo($mh, [ref]$mi) | Out-Null
  Write-Output ("MOVED winRect=({0},{1}) {2}x{3} monWork=({4},{5})-({6},{7})" -f `
    $wr2.L, $wr2.T, ($wr2.R-$wr2.L), ($wr2.B-$wr2.T), `
    $mi.rcWork.L, $mi.rcWork.T, $mi.rcWork.R, $mi.rcWork.B)
  Start-Sleep -Milliseconds 1200
  Grid-Sweep "secondary" $HH
}

if ($Phase -eq "stay") {
  Grid-Sweep "primary" $h
} elseif ($Phase -eq "move2nd") {
  Move-And-Sweep $h
} else {
  Grid-Sweep "primary" $h
  Move-And-Sweep $h
}

Start-Sleep -Milliseconds 500
Get-Process -Name "jkproto_sdl2_jkwindow" -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Output ("APPLOG {0}" -f $applog)
Write-Output "PROBE-DONE"