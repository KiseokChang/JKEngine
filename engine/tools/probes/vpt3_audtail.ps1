# Review Important-1 probe: audio stream ends before demuxer EOF — the
# underrun counter must STOP once the ring drains (dry-cap audioEof gate),
# not pump silence-passes until the demuxer itself hits EOF.
# Evidence: two shots ~8 s apart deep in the audio-less tail with EQUAL
# counts, then a seek back into the audio region resumes playback.
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public class Wt3 {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    public struct RECT { public int L, T, R, B; } public struct POINT { public int X, Y; }
}
"@
[Wt3]::SetProcessDPIAware() | Out-Null
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$mp4 = "I:/progwork/JKENGINE/tmp/vpt3_audtail.mp4"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-14-vplayer-stability"
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory "I:\progwork\JKENGINE\engine\build"
Start-Sleep -Seconds 4
function A([string]$json) {
    $escaped = $json.Replace('"', [string][char]92 + '"')
    (& $exe agentctl $escaped 2>&1) -join "`n"
}
A '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
Start-Sleep -Seconds 6
$h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1).MainWindowHandle
$crect = New-Object Wt3+RECT; [Wt3]::GetClientRect($h, [ref]$crect) | Out-Null
$co = New-Object Wt3+POINT; $co.X = 0; $co.Y = 0; [Wt3]::ClientToScreen($h, [ref]$co)
$scale = ($crect.R - $crect.L) / 1280.0
$list = A '{"tool":"list_windows","args":{}}'
$m = [regex]::Match($list, '"title":"Video Player".*?"x":(-?\d+),"y":(-?\d+),"w":(\d+),"h":(\d+)')
$lx = [int]$m.Groups[1].Value; $ly = [int]$m.Groups[2].Value
function Click([double]$x, [double]$y) {
    $px = [int]([math]::Round($co.X + ($lx + $x) * $scale))
    $py = [int]([math]::Round($co.Y + ($ly + $y) * $scale))
    [Wt3]::SetForegroundWindow($h) | Out-Null; Start-Sleep -Milliseconds 200
    [Wt3]::SetCursorPos($px, $py) | Out-Null; Start-Sleep -Milliseconds 250
    [Wt3]::mouse_event(2,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 80
    [Wt3]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
}
function Type-Into([string]$path) {
    Click 400 42; Start-Sleep -Milliseconds 400
    Set-Clipboard -Value $path; Start-Sleep -Milliseconds 300
    [System.Windows.Forms.SendKeys]::SendWait("{END}"); Start-Sleep -Milliseconds 100
    [System.Windows.Forms.SendKeys]::SendWait("{BS 300}"); Start-Sleep -Milliseconds 150
    [System.Windows.Forms.SendKeys]::SendWait("^v"); Start-Sleep -Milliseconds 400
    Click 808 42
}
function MemMB {
    $p = Get-Process jkdesktop | Select-Object -First 1
    return [int]($p.PrivateMemorySize64 / 1MB)
}
function Shot([string]$name) {
    $bmp = New-Object System.Drawing.Bitmap ($crect.R - $crect.L), ($crect.B - $crect.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size)
    $g.Dispose()
    $cx = [int]([math]::Round($lx * $scale)); $cy = [int]([math]::Round($ly * $scale))
    $cw = [int]([math]::Round(960 * $scale)); $ch = [int]([math]::Round(640 * $scale))
    $rect = New-Object System.Drawing.Rectangle $cx, $cy, $cw, $ch
    $crop = $bmp.Clone($rect, $bmp.PixelFormat)
    $crop.Save("$shotDir\$name")
    $crop.Dispose(); $bmp.Dispose()
}
Type-Into $mp4
Start-Sleep -Seconds 26    # audio ended at 20 s; ring drains ~24 s; dry cap fires
Write-Host ("MEM t26: {0} MB" -f (MemMB))
Shot "vpt3-audtail-t26.png"
Start-Sleep -Seconds 8     # deep in the audio-less tail
Write-Host ("MEM t34: {0} MB" -f (MemMB))
Shot "vpt3-audtail-t34.png"
# seek back into the audio region: playback must resume
Click (150.0 + 690.0 * 0.15) 77
Start-Sleep -Seconds 3
Shot "vpt3-audtail-afterseek.png"
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Write-Host "DONE audtail probe"