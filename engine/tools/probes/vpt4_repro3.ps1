# vpt4 freeze repro v3: exact failing sequence (S2 paused wheel-down scrub ->
# S3 slider seek 0.1 -> drag 1.5 with wheel mid), then discriminate:
#   D1 hover knob (rim highlight?)  D2 click Play (label flip?)  D3 wheel up
# Server stderr captured (looking for "[vplayer] seek failed" lines).
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wt6 {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, int d, UIntPtr e);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
[Wt6]::SetProcessDPIAware() | Out-Null

$build   = "I:\progwork\JKENGINE\engine\build"
$exe     = "$build\jkdesktop.exe"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-14-vplayer-stability"
$mp4     = "I:/progwork/JKENGINE/tmp/vpt2_test.mp4"

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
    $crect = New-Object Wt6+RECT
    [Wt6]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt6+POINT; $co.X = 0; $co.Y = 0
    [Wt6]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
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
function Refresh-Geom {
    Find-Server
    for ($i = 0; $i -lt 10; $i++) { if (Find-VPlayerLayer) { return $true }; Start-Sleep -Milliseconds 300 }
    return $false
}
function Set-LayerCursor([double]$lx, [double]$ly) {
    $px = [int]([math]::Round($script:ox + ($script:layerX + $lx) * $script:scale))
    $py = [int]([math]::Round($script:oy + ($script:layerY + $ly) * $script:scale))
    [Wt6]::SetCursorPos($px, $py) | Out-Null
}
function Click-App([double]$lx, [double]$ly) {
    Refresh-Geom | Out-Null
    [Wt6]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-LayerCursor $lx $ly
    Start-Sleep -Milliseconds 250
    [Wt6]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [Wt6]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
}
function Type-IntoPath([string]$path) {
    Click-App 400 42
    Start-Sleep -Milliseconds 300
    [Wt6]::SetForegroundWindow($script:srvHwnd) | Out-Null
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
    Refresh-Geom | Out-Null
    $crect = New-Object Wt6+RECT
    [Wt6]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt6+POINT; $co.X = 0; $co.Y = 0
    [Wt6]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
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
function Send-Wheel([int]$ticks) {
    $d = $(if ($ticks -ge 0) { 120 } else { -120 })
    for ($i = 0; $i -lt [math]::Abs($ticks); $i++) {
        [Wt6]::mouse_event(0x0800, 0, 0, $d, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 25
    }
}
function Hover-Knob {
    Refresh-Geom | Out-Null
    [Wt6]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-LayerCursor 888 582
    Start-Sleep -Milliseconds 250
}
function Drag-Knob([double]$turns, [switch]$WheelMid) {
    $cx = 888.0; $cy = 582.0; $r = 20.0
    Refresh-Geom | Out-Null
    [Wt6]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-LayerCursor ($cx + $r) $cy
    Start-Sleep -Milliseconds 250
    [Wt6]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 120
    $steps = 24
    $total = [math]::Abs($turns) * 2.0 * [math]::PI
    $dir = $(if ($turns -ge 0) { 1.0 } else { -1.0 })
    for ($i = 1; $i -le $steps; $i++) {
        $a = $total * $dir * $i / $steps
        Set-LayerCursor ($cx + $r * [math]::Cos($a)) ($cy + $r * [math]::Sin($a))
        Start-Sleep -Milliseconds 40
        if ($WheelMid -and $i -eq 12) { Send-Wheel 8 }
    }
    Start-Sleep -Milliseconds 120
    [Wt6]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
}

# ---------- setup ----------
$errLog = "I:\progwork\JKENGINE\tmp\vpt4_client_stderr.log"
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build -RedirectStandardError $errLog
Start-Sleep -Seconds 4
Invoke-Agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
Start-Sleep -Seconds 8
Type-IntoPath $mp4
Click-Open
Start-Sleep -Seconds 5
Save-ServerShot "vpt4-f0-opened.png"

# ---------- F1 = S2: paused wheel-down scrub ----------
Click-App (150.0 + 690.0 * 0.8) 77
Start-Sleep -Milliseconds 800
Click-App 28 68        # Pause
Start-Sleep -Milliseconds 500
Hover-Knob
Send-Wheel -40
Start-Sleep -Milliseconds 700
Save-ServerShot "vpt4-f1-after-s2.png"

# ---------- F2 = S3 trigger: slider seek 0.1 then drag+wheel ----------
Click-App (150.0 + 690.0 * 0.1) 77
Start-Sleep -Milliseconds 800
Save-ServerShot "vpt4-f2a-after-seek01.png"
Drag-Knob 1.5 -WheelMid
Start-Sleep -Milliseconds 700
Save-ServerShot "vpt4-f2b-after-dragwheel.png"

# ---------- D: is the UI alive? ----------
Hover-Knob
Save-ServerShot "vpt4-f3a-hover.png"          # rim highlight?
Click-App 28 68                                # Play/Resume click
Start-Sleep -Milliseconds 600
Save-ServerShot "vpt4-f3b-after-play.png"     # label flip / clock running?
Start-Sleep -Seconds 2
Save-ServerShot "vpt4-f3c-later.png"          # clock advancing?

Write-Host "stderr [vplayer] lines:"
Select-String -Path $errLog -SimpleMatch '[vplayer]' | ForEach-Object { Write-Host ("  " + $_.Line) }
Write-Host "DONE"