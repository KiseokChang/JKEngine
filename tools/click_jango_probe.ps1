# click_jango_probe.ps1 - jango 런처 버튼 클릭 E2E 검증 (PMv2 드라이버)
#
# 목적: jango "Equipment" 버튼에 실제 클릭을 주고 Equip 다이얼로그 인앱
# 타이틀바(파란 밴드)가 나타나는지 확인한 뒤, 모달 내부 "Close" 버튼 클릭으로
# 닫히는지 확인한다. 캡처 경유 마우스 좌표 회귀(2026-08-29 fix, 14번 문서 §12.7)
#
# 사용: powershell -NoProfile -ExecutionPolicy Bypass -File tools\click_jango_probe.ps1
param()
$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class Wcj {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L; public int T; public int R; public int B; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int x; public int y; }
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder sb, int max);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
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
if (-not [Wcj]::SetProcessDpiAwarenessContext([IntPtr](-4))) { [Wcj]::SetProcessDPIAware() | Out-Null }
Add-Type -AssemblyName System.Drawing

$build = "I:\progwork\JKENGINE\prototype\sdl2_jkwindow\build"
$exe = Join-Path $build "jkproto_sdl2_jkwindow.exe"
if (-not (Test-Path $exe)) { Write-Output "FAIL exe missing: $exe"; exit 1 }

Get-Process -Name "jkproto_sdl2_jkwindow" -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 400
$proc = Start-Process -FilePath $exe -ArgumentList "jango" -WorkingDirectory $build -WindowStyle Normal -PassThru

$h = [IntPtr]::Zero
for ($i = 0; $i -lt 40; $i++) {
  Start-Sleep -Milliseconds 250
  [Wcj]::TargetPid = [uint32]($proc.Id)
  $cb = [Wcj+EnumProc]{ param($hh, $l) [Wcj]::Callback($hh, $l) }
  [Wcj]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
  if ([Wcj]::Found -ne [IntPtr]::Zero) { $h = [Wcj]::Found; break }
}
if ($h -eq [IntPtr]::Zero) { Write-Output "FAIL app window not found"; exit 1 }
if (-not [Wcj]::IsWindowVisible($h)) { [Wcj]::ShowWindow($h, 5) | Out-Null; Start-Sleep -Milliseconds 800 }
[Wcj]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 1800

$cr = New-Object 'Wcj+RECT'
[Wcj]::GetClientRect($h, [ref]$cr) | Out-Null
$org = New-Object 'Wcj+POINT'
[Wcj]::ClientToScreen($h, [ref]$org) | Out-Null
$clientW = $cr.R - $cr.L; $clientH = $cr.B - $cr.T
$fit = [Math]::Min($clientW / 1920.0, $clientH / 1080.0)
$lbx = [Math]::Floor(($clientW - 1920*$fit)/2 + 0.5)
$lby = [Math]::Floor(($clientH - 1080*$fit)/2 + 0.5)
Write-Output ("GEOM client={0}x{1} org=({2},{3}) fit={4:F4} lb=({5},{6})" -f $clientW, $clientH, $org.x, $org.y, $fit, $lbx, $lby)

function AppPxToScreen {
  param([double]$ax, [double]$ay)
  return @([int]($org.x + $lbx + $ax * $fit + 0.5), [int]($org.y + $lby + $ay * $fit + 0.5))
}

function Get-Pixel {
  param([int]$sx, [int]$sy)
  $bmp = New-Object System.Drawing.Bitmap(1, 1)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($sx, $sy, 0, 0, (New-Object System.Drawing.Size(1, 1)))
  $c = $bmp.GetPixel(0, 0)
  $g.Dispose(); $bmp.Dispose()
  return $c
}

# 다이얼로그 타이틀 밴드 존재 검사: 다이얼로그 가로 중앙의 앵커 열을 세로로 스캔
# (탐색 범위는 다이얼로그 타이틀 밴드만 - 메인 창 자체 타이틀바/About 테두리 제외).
# 다이얼로그 rect = app (240,120)-(1680,960) → 타이틀 밴드 app y=120..144 ≈ 화면 144..167.
function Find-BlueBandRow {
  param([int]$sx)
  for ($sy = 130; $sy -le 210; $sy++) {
    $c = Get-Pixel $sx $sy
    if ($c.B -gt 80 -and ($c.B - $c.R) -gt 30 -and $c.G -lt 80) { return $sy }
  }
  return -1
}

$anchorX = (AppPxToScreen 960 132)[0]
Write-Output ("ANCHOR-COL x={0}" -f $anchorX)
$bandBefore = Find-BlueBandRow $anchorX
Write-Output ("BEFORE bandRow={0}" -f $bandBefore)

# Equipment 버튼 클릭 (JangoApp: ID_BTN_EQUIP, rect app=(50,260)-(300,330) => center ~(175,295))
$btn = AppPxToScreen 175 295
Write-Output ("CLICK Equipment app=(175,295) -> screen=({0},{1})" -f $btn[0], $btn[1])
[Wcj]::SetCursorPos($btn[0], $btn[1]) | Out-Null
Start-Sleep -Milliseconds 500
[Wcj]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 200
[Wcj]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 1500

$bandAfter = Find-BlueBandRow $anchorX
$openOk = ($bandBefore -lt 0 -and $bandAfter -ge 0)
$verdict1 = $(if($openOk){"OPEN-OK"}else{"OPEN-FAIL"})
Write-Output ("AFTER-OPEN bandRow={0} -> {1}" -f $bandAfter, $verdict1)

# 모달 다이얼로그 Close 버튼 클릭 (EquipDialog: window=(240,120)-(1680,960), Close rect local=(1290,715)-(1426,755), client offset (2,24) => global app center ~(1600,879))
$close = AppPxToScreen 1600 879
Write-Output ("CLICK Close app=(1600,879) -> screen=({0},{1})" -f $close[0], $close[1])
[Wcj]::SetCursorPos($close[0], $close[1]) | Out-Null
Start-Sleep -Milliseconds 500
[Wcj]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 200
[Wcj]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 1500

$bandEnd = Find-BlueBandRow $anchorX
$closeOk = ($bandAfter -ge 0 -and $bandEnd -lt 0)
$verdict2 = $(if($closeOk){"CLOSE-OK"}else{"CLOSE-FAIL"})
Write-Output ("AFTER-CLOSE bandRow={0} -> {1}" -f $bandEnd, $verdict2)

$rc = 0
if ($openOk -and $closeOk) { Write-Output "RESULT: PASS (open+close clicks captured correctly)" }
else { Write-Output "RESULT: FAIL"; $rc = 1 }
Get-Process -Name "jkproto_sdl2_jkwindow" -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Output "CLICK-JANGO-PROBE-DONE"
exit $rc