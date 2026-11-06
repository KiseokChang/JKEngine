# 클린 빌드 호버 실측: OUT → hover ON → capture, IN → hover OFF → capture,
# 버튼 rect(정확한 이미지 좌표 수학)에서 픽셀 차이 계수.
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class W3 {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
[W3]::SetProcessDPIAware() | Out-Null
$build = "I:\progwork\JKENGINE\engine\build"
$spawn = '{"tool":"launch_app","args":{"app":"minesweeper"}}'.Replace('"', '\"')
$null = (& "$build\jkdesktop.exe" agentctl $spawn)
Start-Sleep -Seconds 2
$hw = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
    Select-Object -First 1).MainWindowHandle
[void][W3]::SetForegroundWindow([IntPtr]$hw)
Start-Sleep -Milliseconds 400
$wr = New-Object W3+RECT
[W3]::GetWindowRect([IntPtr]$hw, [ref]$wr) | Out-Null
$cr = New-Object W3+RECT
[W3]::GetClientRect([IntPtr]$hw, [ref]$cr) | Out-Null
$pt = New-Object W3+POINT; $pt.X = 0; $pt.Y = 0
[W3]::ClientToScreen([IntPtr]$hw, [ref]$pt) | Out-Null
$sx = $cr.R / 1280.0; $sy = $cr.B / 720.0
Write-Output ("winRect=$($wr.L),$($wr.T) clientOrigin=$($pt.X),$($pt.Y) clientSize=$($cr.R),$($cr.B)")
function Shot([string]$path) {
    $bmp = New-Object System.Drawing.Bitmap ($wr.R - $wr.L), ($wr.B - $wr.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($wr.L, $wr.T, 0, 0, $bmp.Size)
    $g.Dispose()
    $bmp.Save($path)
    $bmp.Dispose()
}
function Diff-Btn($fa, $fb) {
    $a = [System.Drawing.Bitmap]::FromFile($fa)
    $b = [System.Drawing.Bitmap]::FromFile($fb)
    $hit = 0; $tot = 0
    for ($i = 0; $i -lt 13; $i++) {
        for ($j = 0; $j -lt 13; $j++) {
            $ix = 1007 + 2 + (-$wr.L - 160 + 160) + $i * 2   # btn image x = 1007.. -> grid
            $iy = 253 + 2 + $j * 2
            # image coords directly: btn image rect (1006..1031, 253..278)
            $ix = 1006 + $i * 2            # 1006..1030
            $iy = 253 + $j * 2             # 253..277
            $pa = $a.GetPixel($ix, $iy); $pb = $b.GetPixel($ix, $iy)
            $tot++
            if ([math]::Abs($pa.R - $pb.R) + [math]::Abs($pa.G - $pb.G) + [math]::Abs($pa.B - $pb.B) -gt 12) { $hit++ }
        }
    }
    $a.Dispose(); $b.Dispose()
    return "$hit/$tot"
}
# OUT: 커서를 보드 중앙으로 (logical 500+160, 170+190)
[void][W3]::SetCursorPos($pt.X + [int](660 * $sx), $pt.Y + [int](360 * $sy))
Start-Sleep -Milliseconds 700
Shot "I:\progwork\JKENGINE\engine\tools\probes\diag_ch2_out.png"
# ON: X 중심 logical (806, 182)
[void][W3]::SetCursorPos($pt.X + [int](806 * $sx), $pt.Y + [int](182 * $sy))
Start-Sleep -Milliseconds 700
Shot "I:\progwork\JKENGINE\engine\tools\probes\diag_ch2_on.png"
# OFF: 다시 보드 중앙
[void][W3]::SetCursorPos($pt.X + [int](660 * $sx), $pt.Y + [int](360 * $sy))
Start-Sleep -Milliseconds 700
Shot "I:\progwork\JKENGINE\engine\tools\probes\diag_ch2_off.png"
Write-Output ("DIFF out-vs-on : " + (Diff-Btn "I:\progwork\JKENGINE\engine\tools\probes\diag_ch2_out.png" "I:\progwork\JKENGINE\engine\tools\probes\diag_ch2_on.png"))
Write-Output ("DIFF on-vs-off : " + (Diff-Btn "I:\progwork\JKENGINE\engine\tools\probes\diag_ch2_on.png" "I:\progwork\JKENGINE\engine\tools\probes\diag_ch2_off.png"))
Write-Output "done"