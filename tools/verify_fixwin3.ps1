param(
    [string]$mode = "occ",
    [string]$exe = "i:\progwork\JKENGINE\prototype\sdl2_jkwindow\build\jkproto_sdl2_jkwindow.exe"
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# DPI-aware 검증 스크립트:
#  - SetProcessDPIAware()를 System.Windows.Forms 사용 전에 호출해 물리 픽셀
#    좌표계(1920x1080)에서 CopyFromScreen/지오메트리를 1:1로 얻는다.
#  - 이전 스크립트(probe_fixwin/verify_fixwin2)는 DPI-unaware 실행이라
#    캡처 비트맵이 화면의 논리(pt) 해상도가 아닌 '물리 좌상단 일부'로
#    해석되어 결과가 뒤틀렸다.
# ---------------------------------------------------------------------------
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class DpiUtil {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
}
public class Win32Geom {
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L; public int T; public int R; public int B; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X; public int Y; }
}
'@
[DpiUtil]::SetProcessDPIAware() | Out-Null

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

$screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$work   = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
Write-Host ("[env] physical screen: {0}x{1}, workarea: {2}x{3} @({4},{5})" -f `
    $screen.Width, $screen.Height, $work.Width, $work.Height, $work.X, $work.Y)

if (-not (Test-Path $exe)) { throw "exe not found: $exe" }

$proc = Start-Process -FilePath $exe -ArgumentList $mode -PassThru
try {
    Start-Sleep -Seconds 5
    $proc.Refresh()
    if ($proc.HasExited) { throw "app exited early (code $($proc.ExitCode))" }
    $hwnd = $proc.MainWindowHandle
    if ($hwnd -eq [IntPtr]::Zero) { throw "MainWindowHandle not found" }
    [DpiUtil]::SetForegroundWindow($hwnd) | Out-Null
    Start-Sleep -Milliseconds 600

    # ---- 창 지오메트리 (물리 픽셀) ----
    $wr = New-Object 'Win32Geom+RECT'
    [Win32Geom]::GetWindowRect($hwnd, [ref]$wr) | Out-Null
    $cr = New-Object 'Win32Geom+RECT'
    [Win32Geom]::GetClientRect($hwnd, [ref]$cr) | Out-Null
    $pt = New-Object 'Win32Geom+POINT'
    [Win32Geom]::ClientToScreen($hwnd, [ref]$pt) | Out-Null
    $cx = $pt.X; $cy = $pt.Y
    $cw = $cr.R - $cr.L; $ch = $cr.B - $cr.T
    Write-Host ("[geom] outer rect: ({0},{1})-({2},{3}) = {4}x{5}" -f `
        $wr.L, $wr.T, $wr.R, $wr.B, ($wr.R - $wr.L), ($wr.B - $wr.T))
    Write-Host ("[geom] client origin=({0},{1}) size={2}x{3}" -f $cx, $cy, $cw, $ch)

    # ---- 화면 캡처 (물리 픽셀 1:1) ----
    $bmp = New-Object System.Drawing.Bitmap $screen.Width, $screen.Height
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen(0, 0, 0, 0, $bmp.Size)
    $g.Dispose()

    function Px([int]$x, [int]$y) {
        $c = $bmp.GetPixel($x, $y)
        return @([int]$c.R, [int]$c.G, [int]$c.B)
    }
    function Near([object[]]$c, [int]$r, [int]$gg, [int]$b, [int]$tol) {
        return ([Math]::Abs($c[0] - $r) -le $tol) -and `
               ([Math]::Abs($c[1] - $gg) -le $tol) -and `
               ([Math]::Abs($c[2] - $b) -le $tol)
    }
    function Fmt([object[]]$c) { return "({0},{1},{2})" -f $c[0], $c[1], $c[2] }

    $script:pass = 0; $script:fail = 0
    function Check([string]$name, [bool]$ok, [string]$detail) {
        if ($ok) { Write-Host "  PASS  $name : $detail" -ForegroundColor Green; $script:pass++ }
        else     { Write-Host "  FAIL  $name : $detail" -ForegroundColor Red;  $script:fail++ }
    }

    # ---- 기대 레터박스 계산 (앱 논리 좌표계 1920x1080 등비) ----
    $appW = 1920; $appH = 1080
    $fit = [Math]::Min($cw / $appW, $ch / $appH)
    $contentW = [Math]::Floor($appW * $fit + 0.5)
    $contentH = [Math]::Floor($appH * $fit + 0.5)
    $lbx = [Math]::Floor(($cw - $appW * $fit) * 0.5 + 0.5)
    $lby = [Math]::Floor(($ch - $appH * $fit) * 0.5 + 0.5)
    Write-Host ("[calc] fit={0:F4} content={1}x{2} letterbox=({3},{4}) (client 기준)" -f `
        $fit, $contentW, $contentH, $lbx, $lby)
    $contentX = $cx + $lbx
    $contentY = $cy + $lby

    Write-Host "--- 창 지오메트리 검증 ---"
    Check "타이틀바 온스크린 (clientY >= 25px)" ($cy -ge 25) "clientY=$cy"
    Check "프레임 상단 온스크린 (outerTop >= 0)" ($wr.T -ge 0) "outerTop=$($wr.T)"
    $nativeBar = $cy - $wr.T
    Check "네이티브 타이틀바 높이 >= 20px" ($nativeBar -ge 20) "native bar=${nativeBar}px"
    Check "프레임이 작업영역 안 (outerBottom <= workBottom)" ($wr.B -le $work.Bottom) `
        "outerBottom=$($wr.B) workBottom=$($work.Bottom)"
    Check "프레임이 화면 안 (0 <= outerLeft, outerRight <= W)" `
        ($wr.L -ge 0 -and $wr.R -le $screen.Width) "outerL=$($wr.L) outerR=$($wr.R) screenW=$($screen.Width)"

    Write-Host "--- 레터박스 밴드 픽셀 검증 ---"
    $midY = $cy + [int]($ch / 2)
    $leftX  = $cx + [Math]::Max(1, [int]($lbx / 2))
    $rightX = $cx + $cw - 1 - [Math]::Max(1, [int]($lbx / 2))
    if ($lbx -gt 2) {
        $lc = Px $leftX $midY
        Check "좌측 밴드 = 데스크톱 배경색(192,192,192)" (Near $lc 192 192 192 12) `
            ("at({0},{1})={2}" -f $leftX, $midY, (Fmt $lc))
        $rc = Px $rightX $midY
        Check "우측 밴드 = 데스크톱 배경색(192,192,192)" (Near $rc 192 192 192 12) `
            ("at({0},{1})={2}" -f $rightX, $midY, (Fmt $rc))
    } else {
        Check "레터박스 존재 (lbx > 2)" $false "lbx=$lbx"
    }

    Write-Host "--- 콘텐츠 픽셀 검증 ---"
    $scanX = $contentX + [int]($contentW / 2)
    $tc = Px $scanX ($contentY + [int](12 * $fit))
    Check "인앱 타이틀바(파란) 표시" ($tc[2] -gt 80 -and ($tc[2] - $tc[0]) -gt 30) `
        ("at({0},{1})={2}" -f $scanX, ($contentY + [int](12 * $fit)), (Fmt $tc))
    $bc = Px $scanX ($contentY + $contentH - 6)
    Check "콘텐츠가 클라이언트 하단까지 (하단 여백 없음)" (-not (Near $bc 192 192 192 10)) `
        ("at({0},{1})={2}" -f $scanX, ($contentY + $contentH - 6), (Fmt $bc))
    $nsample = Px $scanX ([Math]::Max(1, $cy - [int]($nativeBar / 2)))
    Write-Host ("  [info] 네이티브 타이틀바 픽셀: {0}" -f (Fmt $nsample))

    Write-Host "--- 인앱 타이틀바 높이 측정 ---"
    $blueTop = -1; $blueBot = -1
    for ($y = $cy; $y -le ($cy + 80) -and $y -lt $screen.Height; $y++) {
        $c = Px $scanX $y
        if (($c[2] -gt 80) -and (($c[2] - $c[0]) -gt 30)) {
            if ($blueTop -lt 0) { $blueTop = $y }
            $blueBot = $y
        }
    }
    if ($blueTop -ge 0) {
        $barH = $blueBot - $blueTop + 1
        Write-Host ("  in-app title bar: y {0}..{1} h={2}px (최상단 1px는 회색 테두리)" -f $blueTop, $blueBot, $barH)
        Check "타이틀바 완전 표시 (클라이언트 내)" `
            ($blueTop -ge $cy -and $blueBot -le ($cy + $ch - 10)) `
            "bar ${blueTop}..${blueBot} in client ${cy}..$($cy + $ch)"
    } else {
        Check "인앱 타이틀바 감지" $false "no blue band in client top rows"
    }

    Write-Host "--- 콘텐츠 범위 스캔 (스케일/위치 검증) ---"
    # JKWindow는 콘텐츠 가장자리에 1px 회색 테두리(192,192,192)를 그린다.
    # 클라이언트 안에서 회색이 아닌 행/열의 첫/끝을 측정하고 테두리 2px를
    # 보정하면 콘텐츠의 물리 크기가 나온다 (측정 오차 ±1px = 0.1%).
    $firstNG = -1; $lastNG = -1
    for ($y = $cy; $y -lt ($cy + $ch); $y++) {
        if (-not (Near (Px $scanX $y) 192 192 192 12)) {
            if ($firstNG -lt 0) { $firstNG = $y }
            $lastNG = $y
        }
    }
    $firstNGx = -1; $lastNGx = -1
    for ($x = $cx; $x -lt ($cx + $cw); $x++) {
        if (-not (Near (Px $x $midY) 192 192 192 12)) {
            if ($firstNGx -lt 0) { $firstNGx = $x }
            $lastNGx = $x
        }
    }
    if ($firstNG -ge 0 -and $firstNGx -ge 0) {
        $measH = ($lastNG - $firstNG + 1) + 2
        $measW = ($lastNGx - $firstNGx + 1) + 2
        $scaleH = $measH / 1080.0
        $scaleW = $measW / 1920.0
        Write-Host ("  content rows: {0}..{1} (+2 border) = {2}px -> scaleH={3:F4}" -f $firstNG, $lastNG, $measH, $scaleH)
        Write-Host ("  content cols: {0}..{1} (+2 border) = {2}px -> scaleW={3:F4}" -f $firstNGx, $lastNGx, $measW, $scaleW)
        Check "세로 스케일 = fit (오차 0.005)" ([Math]::Abs($scaleH - $fit) -lt 0.005) `
            ("scaleH={0:F4} fit={1:F4}" -f $scaleH, $fit)
        Check "가로 스케일 = fit (오차 0.005)" ([Math]::Abs($scaleW - $fit) -lt 0.005) `
            ("scaleW={0:F4} fit={1:F4}" -f $scaleW, $fit)
        Check "콘텐츠 좌측 위치 = 예상 레터박스 시작" `
            ([Math]::Abs(($firstNGx - 1) - ($cx + $lbx)) -le 2) `
            ("firstContentCol={0} expected={1}" -f ($firstNGx - 1), ($cx + $lbx))
        Check "콘텐츠 우측 위치 = 예상 레터박스 끝" `
            ([Math]::Abs(($lastNGx + 1) - ($cx + $lbx + $contentW - 1)) -le 2) `
            ("lastContentCol={0} expected={1}" -f ($lastNGx + 1), ($cx + $lbx + $contentW - 1))
    } else {
        Check "콘텐츠 범위 감지" $false "no non-gray pixels found in client"
    }

    $out = "C:\temp_jkwin_verify\shot_fixwin3_${mode}.png"
    $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Host "[saved] $out"

    Write-Host ""
    Write-Host ("RESULT[{0}]: {1} pass, {2} fail" -f $mode, $script:pass, $script:fail)
    if ($script:fail -gt 0) { exit 1 }
    Write-Host "ALL CHECKS PASSED"
    exit 0
}
finally {
    if ($null -ne $proc -and -not $proc.HasExited) {
        $proc.CloseMainWindow() | Out-Null
        Start-Sleep -Milliseconds 800
        if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
    }
}