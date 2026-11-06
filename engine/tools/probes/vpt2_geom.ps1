$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
$build = "I:\progwork\JKENGINE\engine\build"
$exe = "$build\jkdesktop.exe"
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build
Start-Sleep -Seconds 4
$escaped = '{"tool":"launch_app","args":{"app":"vplayer"}}'.Replace('"', [string][char]92 + '"')
& $exe agentctl $escaped | Out-Null
Start-Sleep -Seconds 6
$list = (& $exe agentctl ('{"tool":"list_windows","args":{}}'.Replace('"', [string][char]92 + '"'))) -join "`n"
Write-Host $list
$h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1).MainWindowHandle
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public class G2 {
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
$crect = New-Object G2+RECT
[G2]::GetClientRect($h, [ref]$crect) | Out-Null
$co = New-Object G2+POINT; $co.X = 0; $co.Y = 0
[G2]::ClientToScreen($h, [ref]$co) | Out-Null
Write-Host ("client {0}x{1} at {2},{3}" -f ($crect.R-$crect.L), ($crect.B-$crect.T), $co.X, $co.Y)
$bmp = New-Object System.Drawing.Bitmap ($crect.R-$crect.L), ($crect.B-$crect.T)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size)
$g.Dispose()
$bmp.Save("I:\progwork\JKENGINE\.superpowers\sdd\2026-09-14-vplayer-stability\vpt2-geom-fullclient.png")
$bmp.Dispose()
Write-Host "saved full-client shot"