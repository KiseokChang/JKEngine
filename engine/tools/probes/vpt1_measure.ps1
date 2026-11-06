# Measure vplayer UI coords: fresh server, launch vplayer, baseline shot.
$ErrorActionPreference = 'Continue'
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class DpiM {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
}
"@
[DpiM]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class WtM {
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
$build = "I:\progwork\JKENGINE\engine\build"
$exe = "$build\jkdesktop.exe"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-14-vplayer-stability"
function Invoke-Agentctl([string]$json) {
    # PS 5.1 native argv quirk: the server needs the JSON's quotes to arrive
    # as backslash-quote — build it with [char]92 to dodge escaping hell.
    $escaped = $json.Replace('"', [string][char]92 + '"')
    return (& $exe agentctl $escaped 2>&1) -join "`n"
}
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build
Start-Sleep -Seconds 4
Write-Host ("ping: " + (Invoke-Agentctl '{"tool":"ping","args":{}}'))
Invoke-Agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
Start-Sleep -Seconds 6
$list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
Write-Host ("windows: " + $list)
$h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1).MainWindowHandle
$crect = New-Object WtM+RECT
[WtM]::GetClientRect($h, [ref]$crect) | Out-Null
$co = New-Object WtM+POINT; $co.X = 0; $co.Y = 0
[WtM]::ClientToScreen($h, [ref]$co) | Out-Null
$scale = ($crect.R - $crect.L) / 1280.0
$m = [regex]::Match($list, '"title":"Video Player".*?"x":(-?\d+),"y":(-?\d+),"w":(\d+),"h":(\d+)')
Write-Host ("layer: x={0} y={1} w={2} h={3} scale={4}" -f $m.Groups[1].Value, $m.Groups[2].Value, $m.Groups[3].Value, $m.Groups[4].Value, $scale)
$lx = [int]$m.Groups[1].Value; $ly = [int]$m.Groups[2].Value
$lw = [int]$m.Groups[3].Value; $lh = [int]$m.Groups[4].Value
$cx = [int]($lx * $scale); $cy = [int]($ly * $scale)
$cw = [int]($lw * $scale); $ch = [int]($lh * $scale)
$bmp = New-Object System.Drawing.Bitmap ($crect.R - $crect.L), ($crect.B - $crect.T)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size)
$g.Dispose()
$rect = New-Object System.Drawing.Rectangle $cx, $cy, $cw, $ch
$crop = $bmp.Clone($rect, $bmp.PixelFormat)
$crop.Save((Join-Path $shotDir "vpt1-baseline.png"))
$crop.Dispose(); $bmp.Dispose()
Write-Host "saved vpt1-baseline.png"
