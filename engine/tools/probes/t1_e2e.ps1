# Task 1 e2e: browser URL/title tracking + 2-row bar + g_viewPageY sync.
# ASCII-only on purpose (BOM-less ps1 with non-ASCII is read as cp949, docs/15).
# DPI context BEFORE Forms (else powershell stays virtualized; measured x1.25).
# The browser is composited into the SERVER desktop surface (its own hwnd is
# hidden), so clicks/screenshots target the server hwnd (probe_click
# convention: scale = serverClientW / 1280 logical) offset by the browser
# layer origin from list_windows (probe_agent_e2e escaped-text convention).
$ErrorActionPreference = 'Continue'
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Dpi0 {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
}
"@
[Dpi0]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class Wt1 {
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X; public int Y; }
}
"@

$build = "I:\progwork\JKENGINE\engine\build"
$exe   = "$build\jkdesktop.exe"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-14-browser-bookmarks"
$script:srvHwnd = [IntPtr]::Zero
$script:srvScale = 1.0
$script:srvOx = 0; $script:srvOy = 0
# browser layer origin in server-logical desktop coords (list_windows)
$script:layerX = 0; $script:layerY = 0; $script:layerW = 0; $script:layerH = 0

function Stop-Desktop {
    Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
        Where-Object { $_.CommandLine -match '--client' } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 1
}

function Find-ServerWindow {
    $h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
        Select-Object -First 1).MainWindowHandle
    if (-not $h) { Write-Host "NO SERVER WINDOW"; exit 1 }
    $script:srvHwnd = [IntPtr]$h
    $crect = New-Object Wt1+RECT
    [Wt1]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt1+POINT; $co.X = 0; $co.Y = 0
    [Wt1]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
    $script:srvScale = ($crect.R - $crect.L) / 1280.0
    $script:srvOx = $co.X; $script:srvOy = $co.Y
    Write-Host ("server client={0}x{1} origin=({2},{3}) scale={4}" -f `
        ($crect.R - $crect.L), ($crect.B - $crect.T), $co.X, $co.Y, $script:srvScale)
}

function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}

function Find-BrowserLayer {
    $list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    $m = [regex]::Match($list, '[\\"]+id[\\"]+:(\d+),[\\"]+title[\\"]+:[\\"]+Browser[\\"]+.*?[\\"]+x[\\"]+:(-?\d+),[\\"]+y[\\"]+:(-?\d+),[\\"]+w[\\"]+:(\d+),[\\"]+h[\\"]+:(\d+)')
    if (-not $m.Success) { Write-Host ("browser layer NOT FOUND: " + $list); exit 1 }
    $script:layerX = [int]$m.Groups[2].Value
    $script:layerY = [int]$m.Groups[3].Value
    $script:layerW = [int]$m.Groups[4].Value
    $script:layerH = [int]$m.Groups[5].Value
    Write-Host ("browser layer id={0} rect=({1},{2} {3}x{4})" -f `
        $m.Groups[1].Value, $script:layerX, $script:layerY, $script:layerW, $script:layerH)
}

# app-local logical (960x640 space) -> screen; clicks land through the server.
function Click-App([double]$lx, [double]$ly) {
    $slx = $script:layerX + $lx
    $sly = $script:layerY + $ly
    $px = [int]([math]::Round($script:srvOx + $slx * $script:srvScale))
    $py = [int]([math]::Round($script:srvOy + $sly * $script:srvScale))
    [Wt1]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    [Wt1]::SetCursorPos($px, $py) | Out-Null
    Start-Sleep -Milliseconds 250
    [Wt1]::mouse_event(2,0,0,0,[UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [Wt1]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
    Write-Host ("clicked app-local ({0},{1}) -> server-logical ({2},{3}) -> screen ({4},{5})" -f $lx, $ly, $slx, $sly, $px, $py)
}

function Save-ServerShot([string]$name) {
    [Wt1]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 300
    $crect = New-Object Wt1+RECT
    [Wt1]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt1+POINT; $co.X = 0; $co.Y = 0
    [Wt1]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
    $w = $crect.R - $crect.L; $hh = $crect.B - $crect.T
    $bmp = New-Object System.Drawing.Bitmap $w, $hh
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size)
    $path = Join-Path $shotDir $name
    $bmp.Save($path)
    $g.Dispose(); $bmp.Dispose()
    # crop to the browser layer for a readable shot
    $cx = [int]([math]::Round($script:layerX * $script:srvScale))
    $cy = [int]([math]::Round($script:layerY * $script:srvScale))
    $cw = [int]([math]::Round($script:layerW * $script:srvScale))
    $ch = [int]([math]::Round($script:layerH * $script:srvScale))
    $rect = New-Object System.Drawing.Rectangle $cx, $cy, $cw, $ch
    $crop = $bmp.Clone($rect, $bmp.PixelFormat)
    $crop.Save((Join-Path $shotDir ("crop_" + $name)))
    $crop.Dispose()
    $bmp.Dispose()
    Write-Host ("saved {0} (+crop {1}x{2} at layer ({3},{4}))" -f $path, $cw, $ch, $cx, $cy)
}

function Type-Keys([string]$keys) {
    [Wt1]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 300
    [System.Windows.Forms.SendKeys]::SendWait($keys)
    Write-Host ("typed: {0}" -f $keys)
}

if ($args.Count -eq 0 -or $args[0] -eq 'setup') {
    Stop-Desktop
    Start-Process -FilePath $exe -ArgumentList "--server" `
        -WorkingDirectory $build
    Start-Sleep -Seconds 4
    $launch = Invoke-Agentctl '{"tool":"launch_app","args":{"app":"browser"}}'
    Write-Host ("launch: {0}" -f $launch)
    Start-Sleep -Seconds 6
    Find-ServerWindow
    Find-BrowserLayer
    Save-ServerShot "t1_home.png"
}
elseif ($args[0] -eq 'click') {
    # click <lx> <ly> [shotName] [waitSec]
    Find-ServerWindow
    Find-BrowserLayer
    Click-App ([double]$args[1]) ([double]$args[2])
    $wait = 2; if ($args.Count -ge 5) { $wait = [int]$args[4] }
    Start-Sleep -Seconds $wait
    if ($args.Count -ge 4) { Save-ServerShot $args[3] }
}
elseif ($args[0] -eq 'type') {
    # type <keys> [shotName] [waitSec]
    Find-ServerWindow
    Find-BrowserLayer
    Type-Keys $args[1]
    $wait = 1; if ($args.Count -ge 4) { $wait = [int]$args[3] }
    Start-Sleep -Seconds $wait
    if ($args.Count -ge 3) { Save-ServerShot $args[2] }
}
elseif ($args[0] -eq 'shot') {
    Find-ServerWindow
    Find-BrowserLayer
    Save-ServerShot $args[1]
}
elseif ($args[0] -eq 'teardown') {
    Stop-Desktop
    Write-Host "stopped"
}