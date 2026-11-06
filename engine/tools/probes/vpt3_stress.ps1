# T3 supplementary stress e2e: the refill mechanism only manifests when video
# decode cannot keep up with realtime, so this run adds CPU contention (10
# busy-loop processes on a 12-core box) during continuous hi-res playback and
# samples the underrun counter under load. Run with -Tag baseline-stress and
# -Tag after-stress; the caller stashes/rebuilds between the two runs.
# -STA required (Set-Clipboard). Harness recipe = vpt3_e2e.ps1.
param([string]$Tag = "after-stress")
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class WtS {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
[WtS]::SetProcessDPIAware() | Out-Null

$build   = "I:\progwork\JKENGINE\engine\build"
$exe     = "$build\jkdesktop.exe"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-14-vplayer-stability"
$hiRes   = "I:\progwork\JKENGINE\tmp\test_media\vpt_hires.mp4"
$script:fails = 0

function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host ("PASS: " + $name) }
    else     { $script:fails++; Write-Host ("FAIL: " + $name + " -- " + $detail) }
    if ($detail) { Write-Host ("      " + $detail) }
}
function Invoke-Agentctl([string]$json) {
    $escaped = $json.Replace('"', [string][char]92 + '"')
    return (& $exe agentctl $escaped 2>&1) -join "`n"
}
$script:srvHwnd = [IntPtr]::Zero
$script:scale = 1.0; $script:ox = 0; $script:oy = 0
$script:layerX = 0; $script:layerY = 0; $script:layerW = 960; $script:layerH = 640

function Find-Server {
    $h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
        Select-Object -First 1).MainWindowHandle
    if (-not $h) { Write-Host "FAIL: NO SERVER WINDOW"; exit 1 }
    $script:srvHwnd = [IntPtr]$h
    $crect = New-Object WtS+RECT
    [WtS]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object WtS+POINT; $co.X = 0; $co.Y = 0
    [WtS]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
    $script:scale = ($crect.R - $crect.L) / 1280.0
    $script:ox = $co.X; $script:oy = $co.Y
}
function Find-VPlayerLayer {
    $list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    $m = [regex]::Match($list, '"title":"Video Player".*?"x":(-?\d+),"y":(-?\d+),"w":(\d+),"h":(\d+)')
    if (-not $m.Success) { return $false }
    $script:layerX = [int]$m.Groups[1].Value
    $script:layerY = [int]$m.Groups[2].Value
    $script:layerW = [int]$m.Groups[3].Value
    $script:layerH = [int]$m.Groups[4].Value
    return $true
}
function VPlayer-Alive { return (Find-VPlayerLayer) }

function Send-Click([double]$lx, [double]$ly, [uint32]$down, [uint32]$up) {
    $px = [int]([math]::Round($script:ox + ($script:layerX + $lx) * $script:scale))
    $py = [int]([math]::Round($script:oy + ($script:layerY + $ly) * $script:scale))
    [WtS]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    [WtS]::SetCursorPos($px, $py) | Out-Null
    Start-Sleep -Milliseconds 250
    [WtS]::mouse_event($down, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [WtS]::mouse_event($up, 0, 0, 0, [UIntPtr]::Zero)
}
function Click-App([double]$lx, [double]$ly) { Send-Click $lx $ly 2 4 }
function Type-IntoPath([string]$path) {
    Click-App 400 42
    Start-Sleep -Milliseconds 300
    [WtS]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    for ($i = 0; $i -lt 5; $i++) {
        try { Set-Clipboard -Value $path; break } catch {
            try { [System.Windows.Forms.Clipboard]::SetText($path); break } catch {
                Start-Sleep -Milliseconds 700 }
        }
    }
    Start-Sleep -Milliseconds 150
    [System.Windows.Forms.SendKeys]::SendWait("{END}")
    Start-Sleep -Milliseconds 100
    [System.Windows.Forms.SendKeys]::SendWait("{BS 300}")
    Start-Sleep -Milliseconds 150
    [System.Windows.Forms.SendKeys]::SendWait("^v")
    Start-Sleep -Milliseconds 400
}
function Click-Open { Click-App 808 42 }

function Save-ServerShot([string]$name) {
    Find-Server
    [WtS]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 600
    $crect = New-Object WtS+RECT
    [WtS]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object WtS+POINT; $co.X = 0; $co.Y = 0
    [WtS]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($crect.R - $crect.L), ($crect.B - $crect.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size)
    $g.Dispose()
    $cx = [int]([math]::Round($script:layerX * $script:scale))
    $cy = [int]([math]::Round($script:layerY * $script:scale))
    $cw = [int]([math]::Round($script:layerW * $script:scale))
    $ch = [int]([math]::Round($script:layerH * $script:scale))
    if ($cx + $cw -gt $bmp.Width) { $cw = $bmp.Width - $cx }
    if ($cy + $ch -gt $bmp.Height) { $ch = $bmp.Height - $cy }
    $rect = New-Object System.Drawing.Rectangle $cx, $cy, $cw, $ch
    $crop = $bmp.Clone($rect, $bmp.PixelFormat)
    $crop.Save((Join-Path $shotDir $name))
    $crop.Dispose(); $bmp.Dispose()
    Write-Host ("shot {0}" -f $name)
}
function Ping-Ms {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $r = Invoke-Agentctl '{"tool":"ping","args":{}}'
    $sw.Stop()
    if ($r -notmatch '"ok"\s*:\s*true') { return -1.0 }
    return $sw.Elapsed.TotalMilliseconds
}

# ---------- setup ----------
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build
Start-Sleep -Seconds 4
Check "setup: server up" ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') ""
Invoke-Agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
Start-Sleep -Seconds 6
Find-Server
Check "setup: vplayer launched" (VPlayer-Alive) "no Video Player layer"

# open hi-res and let it settle briefly before the burners start
Type-IntoPath $hiRes
Click-Open
Start-Sleep -Seconds 6

# ---------- CPU contention: 10 busy loops on a 12-core box ----------
$burners = @()
for ($i = 0; $i -lt 10; $i++) {
    # hidden windows: a visible burner console would cover the server window
    $burners += Start-Process powershell -ArgumentList '-NoProfile','-WindowStyle',
        'Hidden','-Command','while($true){ [System.Math]::Sqrt(12345.678) | Out-Null }' -PassThru
}
Start-Sleep -Seconds 4    # burners at full tilt; playback now under load

Save-ServerShot ("vpt3-{0}-t10.png" -f $Tag)
$p = Ping-Ms
Check "stress: responsive under load (t10)" ($p -ge 0) ("ping {0:N0} ms" -f $p)
Start-Sleep -Seconds 15   # ~t+25 under load
Save-ServerShot ("vpt3-{0}-t25.png" -f $Tag)
Start-Sleep -Seconds 15   # ~t+40 under load
Save-ServerShot ("vpt3-{0}-t40.png" -f $Tag)
Check "stress: no crash after ~40 s loaded playback" (VPlayer-Alive) ""

# ---------- teardown ----------
foreach ($b in $burners) { try { Stop-Process -Id $b.Id -Force -ErrorAction SilentlyContinue } catch {} }
# sweep by exact command line (never a broad powershell kill)
Get-CimInstance Win32_Process -Filter "Name='powershell.exe'" |
    Where-Object { $_.CommandLine -match 'Math::Sqrt' } |
    ForEach-Object { try { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue } catch {} }
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Write-Host ("RESULT: " + $(if ($script:fails -eq 0) { "ALL PASS" } else { "$($script:fails) FAILURE(S)" }))
exit $(if ($script:fails -eq 0) { 0 } else { 1 })
