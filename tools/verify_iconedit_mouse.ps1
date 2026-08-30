# verify_iconedit_mouse.ps1 - IconEdit 마우스 클릭 정확도 검증 (PMv2)
#
# 목적: iconedit 픽셀 보드에서 특정 그리드 셀을 클릭했을 때
# 실제로 해당 셀의 색상이 바뀌는지 확인한다.
# 이 프로브는 SDL2 논리 좌표 -> 물리 픽셀 변환과
# JKControl 내 마우스 좌표 라우팅이 정확한지 검증한다.
param(
    [string]$exe = "i:\progwork\JKENGINE\prototype\sdl2_jkwindow\build\jkproto_sdl2_jkwindow.exe"
)
$ErrorActionPreference = 'Stop'

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class Wci {
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
if (-not [Wci]::SetProcessDpiAwarenessContext([IntPtr](-4))) { [Wci]::SetProcessDPIAware() | Out-Null }
Add-Type -AssemblyName System.Drawing

if (-not (Test-Path $exe)) { throw "exe not found: $exe" }

Get-Process -Name "jkproto_sdl2_jkwindow" -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 400

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $exe
$psi.Arguments = "iconedit"
$psi.UseShellExecute = $false
$proc = [System.Diagnostics.Process]::Start($psi)

$h = [IntPtr]::Zero
for ($i = 0; $i -lt 40; $i++) {
    Start-Sleep -Milliseconds 250
    [Wci]::TargetPid = [uint32]($proc.Id)
    $cb = [Wci+EnumProc]{ param($hh, $l) [Wci]::Callback($hh, $l) }
    [Wci]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
    if ([Wci]::Found -ne [IntPtr]::Zero) { $h = [Wci]::Found; break }
}
if ($h -eq [IntPtr]::Zero) { throw "app window not found" }
[Wci]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 1500


function Get-Pixel {
    param([int]$sx, [int]$sy)
    # Re-capture the full client area into the shared bitmap and read the pixel
    # at the requested screen coordinate. This avoids GDI+ 1x1 bitmap/DPI
    # quirks that can return the wrong color on HiDPI displays.
    $tmpG = [System.Drawing.Graphics]::FromImage($bmp)
    $tmpG.CopyFromScreen($org.x, $org.y, 0, 0, (New-Object System.Drawing.Size($clientW, $clientH)))
    $tmpG.Dispose()
    $cx = $sx - $org.x
    $cy = $sy - $org.y
    if ($cx -lt 0 -or $cy -lt 0 -or $cx -ge $clientW -or $cy -ge $clientH) {
        return [System.Drawing.Color]::FromArgb(0, 0, 0)
    }
    return $bmp.GetPixel($cx, $cy)
}

function Click {
    param([int]$sx, [int]$sy)
    [Wci]::SetCursorPos($sx, $sy) | Out-Null
    Start-Sleep -Milliseconds 400
    [Wci]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 200
    [Wci]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 600
}

function RightClick {
    param([int]$sx, [int]$sy)
    [Wci]::SetCursorPos($sx, $sy) | Out-Null
    Start-Sleep -Milliseconds 400
    [Wci]::mouse_event(0x0008, 0, 0, 0, [UIntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 200
    [Wci]::mouse_event(0x0010, 0, 0, 0, [UIntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 800
}

# Capture the client area to find the actual pixel board grid.
$cr = New-Object 'Wci+RECT'
[Wci]::GetClientRect($h, [ref]$cr) | Out-Null
$org = New-Object 'Wci+POINT'
[Wci]::ClientToScreen($h, [ref]$org) | Out-Null
$clientW = $cr.R - $cr.L
$clientH = $cr.B - $cr.T
$bmp = New-Object System.Drawing.Bitmap $clientW, $clientH
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($org.x, $org.y, 0, 0, (New-Object System.Drawing.Size($clientW, $clientH)))
$g.Dispose()

function Px([int]$x, [int]$y) { return $bmp.GetPixel($x, $y) }
function FmtColor { param($c) return "({0},{1},{2})" -f $c.R, $c.G, $c.B }

# Compute letterbox fit and margins used by the app.
$fit = [Math]::Min($clientW / 1920.0, $clientH / 1080.0)
$lbx = [Math]::Floor(($clientW - 1920 * $fit) / 2 + 0.5)
$lby = [Math]::Floor(($clientH - 1080 * $fit) / 2 + 0.5)

# The pixel board is the large dark-gray area. Detect its physical edges and
# convert them back to app-logical units so we can compute the centered 24x24
# grid origin exactly the same way PixelBoard does internally.
function IsDark { param($c) return ($c.R -lt 100 -and $c.G -lt 100 -and $c.B -lt 100) }

$midX = [int]($clientW / 2)
$boardTop = -1
for ($y = 60; $y -lt $clientH; $y++) {
    if (IsDark (Px $midX $y)) { $boardTop = $y; break }
}
if ($boardTop -lt 0) { throw "could not locate pixel board top" }

$boardLeft = -1
for ($x = 0; $x -lt $clientW; $x++) {
    if (IsDark (Px $x $boardTop)) { $boardLeft = $x; break }
}
if ($boardLeft -lt 0) { throw "could not locate pixel board left" }

$boardBottom = -1
for ($y = $clientH - 1; $y -gt $boardTop; $y--) {
    if (IsDark (Px $midX $y)) { $boardBottom = $y; break }
}
if ($boardBottom -lt 0) { throw "could not locate pixel board bottom" }

$midY = [int](($boardTop + $boardBottom) / 2)
$boardRight = -1
for ($x = $clientW - 1; $x -gt $boardLeft; $x--) {
    if (IsDark (Px $x $midY)) { $boardRight = $x; break }
}
if ($boardRight -lt 0) { throw "could not locate pixel board right" }

$physBoardW = $boardRight - $boardLeft + 1
$physBoardH = $boardBottom - $boardTop + 1

$logBoardLeft = ($boardLeft - $org.x - $lbx) / $fit
$logBoardTop = ($boardTop - $org.y - $lby) / $fit
$logBoardW = $physBoardW / $fit
$logBoardH = $physBoardH / $fit

$zoom = [int]([Math]::Min($logBoardW / 24, $logBoardH / 24))
if ($zoom -le 0) { $zoom = 1 }
$originX = [int]($logBoardLeft + ($logBoardW - 24 * $zoom) / 2 + 0.5)
$originY = [int]($logBoardTop + ($logBoardH - 24 * $zoom) / 2 + 0.5)

Write-Host ("[board] detected phys=({0},{1} {2}x{3}) logical=({4},{5} {6}x{7}) zoom={8}" -f $boardLeft, $boardTop, $physBoardW, $physBoardH, $logBoardLeft, $logBoardTop, $logBoardW, $logBoardH, $zoom)
Write-Host ("[board] client origin=({0},{1}) fit={2:F4} letterbox=({3},{4})" -f $org.x, $org.y, $fit, $lbx, $lby)

function GetCellCenter {
    param([int]$px, [int]$py)
    $cx = $originX + $px * $zoom + [int]($zoom / 2)
    $cy = $originY + $py * $zoom + [int]($zoom / 2)
    # The app expects mouse coordinates in app logical units; convert from app
    # logical cell center to physical screen point via the same fit/letterbox
    # transform the renderer uses.
    $sx = [int]($org.x + $lbx + $cx * $fit + 0.5)
    $sy = [int]($org.y + $lby + $cy * $fit + 0.5)
    return @($sx, $sy)
}

$script:pass = 0; $script:fail = 0
function Check {
    param([string]$name, [bool]$ok, [string]$detail)
    if ($ok) { Write-Host "  PASS  $name : $detail" -ForegroundColor Green; $script:pass++ }
    else { Write-Host "  FAIL  $name : $detail" -ForegroundColor Red; $script:fail++ }
}

# Test 1: click cell (2,2) and verify it turns white.
$cellA = GetCellCenter 2 2
$beforeA = Get-Pixel $cellA[0] $cellA[1]
Write-Host ("[test1] click cell(2,2) screen=({0},{1}) before={2}" -f $cellA[0], $cellA[1], (FmtColor $beforeA))
Click $cellA[0] $cellA[1]
$afterA = Get-Pixel $cellA[0] $cellA[1]
Check "cell(2,2) turns white" (($afterA.R -gt 200) -and ($afterA.G -gt 200) -and ($afterA.B -gt 200)) ("after=" + (FmtColor $afterA))

# Test 2: open color picker from cell (4,4), select red, paint cell (5,5).
$cellB = GetCellCenter 4 4
RightClick $cellB[0] $cellB[1]
Start-Sleep -Milliseconds 500

# ColorDialog is created at app-logical (200,200,500,500) with a 2pt border and
# 24pt title bar. Its JKListBox is at dialog-client (10,30,280,280), items are
# 16px tall, and the listbox has a 2px inner inset. Convert the Red item
# (index 5) center from app-logical coordinates to the same screen space used by
# GetCellCenter.
$dlgClientX = 200 + 2
$dlgClientY = 200 + 24
$listboxX = $dlgClientX + 10
$listboxY = $dlgClientY + 30
$itemHeight = 16
$redIdx = 5
$redItemX = $listboxX + 2 + [int]((280 - 20) / 2)
$redItemY = $listboxY + 2 + ($redIdx * $itemHeight) + [int]($itemHeight / 2)
$redScreenX = [int]($org.x + $lbx + $redItemX * $fit + 0.5)
$redScreenY = [int]($org.y + $lby + $redItemY * $fit + 0.5)
Write-Host ("[test2] Red item app=({0},{1}) screen=({2},{3})" -f $redItemX, $redItemY, $redScreenX, $redScreenY)
Click $redScreenX $redScreenY
Start-Sleep -Milliseconds 500

$cellC = GetCellCenter 5 5
$beforeC = Get-Pixel $cellC[0] $cellC[1]
Click $cellC[0] $cellC[1]
$afterC = Get-Pixel $cellC[0] $cellC[1]
Write-Host ("[test2] click cell(5,5) before={0} after={1}" -f (FmtColor $beforeC), (FmtColor $afterC))
Check "cell(5,5) turns red" (($afterC.R -gt 150) -and ($afterC.G -lt 80) -and ($afterC.B -lt 80)) ("after=" + (FmtColor $afterC))

# Cleanup
if (-not $proc.HasExited) {
    $proc.CloseMainWindow() | Out-Null
    Start-Sleep -Milliseconds 800
    if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
}
$bmp.Dispose()

Write-Host ""
Write-Host ("RESULT: {0} pass, {1} fail" -f $pass, $fail)
if ($fail -gt 0) { exit 1 }
Write-Host "ALL CHECKS PASSED"
exit 0
